import argparse
import datetime
import json
import random
import requests
import time
import traceback

from deutschebahn import DBInfoscreen, DS100
from layout_renderer import LayoutRenderer
from fia_control import FIA, FIAEmulator
from utils import TimeoutError, timeout

from db_common import *
from local_settings import *

if BL_AUX_IN_1_ENABLED:
    import threading
    import RPi.GPIO as gpio


LAST_DATA_HASH = 0
COACH_ORDER_CACHE = {}


def bl_timer(fia):
    if fia.get_backlight_state() == 1:
        print("Backlight is already on, aborting")
        return
    print("Enabling backlight")
    fia.set_backlight_state(1)
    time.sleep(BL_AUX_IN_1_TIME)
    print("Disabling backlight")
    fia.set_backlight_state(0)


def on_bl_button_pressed(pin, fia):
    print("Backlight button pressed")
    bl_thread = threading.Thread(target=bl_timer, kwargs={'fia': fia})
    bl_thread.start()


def train_type_filter(train, train_types):
    cur_type = train['train'].split()[0]
    for tt in train_types:
        if cur_type == tt:
            return True
    return False


def show_departures(dbi, ds100, fia, renderer, config, auto_clear_scroll_buf = False):
    global LAST_DATA_HASH
    global COACH_ORDER_CACHE
    
    @timeout(10)
    def _get_trains(station_code):
        return dbi.get_trains(station_code)
    
    @timeout(10)
    def _get_coach_order(train_number, departure):
        return dbi.get_coach_order(train_number, departure)
    
    station = config.get('station')
    display_station = config.get('display_station')
    train_types = config.get('train_types')
    platform = config.get('platform', 'all')
    layout_single = config.get('layout_single', {})
    layout_double = config.get('layout_double', layout_single)
    replacement_map = config.get('replacement_map')
    duration = config.get('duration', 10)
    
    if type(layout_single) is str:
        with open(layout_single, 'r', encoding='utf-8') as f:
            layout_single = json.load(f)
    
    if type(layout_double) is str:
        with open(layout_double, 'r', encoding='utf-8') as f:
            layout_double = json.load(f)
    
    if type(replacement_map) is str:
        with open(replacement_map, 'r', encoding='utf-8') as f:
            replacement_map = json.load(f)
    
    
    random_station = station == "RANDOM"
    num_tries = 0
    while num_tries < 5:
        num_tries += 1
        if random_station:
            filter_func = lambda s: s['type'] == "Bf" and not s['code'][0] in ("X", "Z")
            station_code = random.choice([code for code, data in ds100.filter(filter_func).items()])
            print("Trying station {}".format(station_code))
            trains = dbi.calc_real_times(_get_trains(station_code))
        else:
            station_code = station
            trains = dbi.calc_real_times(_get_trains(station))
        trains = filter(lambda t: t['actualDeparture'] is not None, trains)
        trains = filter(lambda t: not t['train'].startswith("Bus SEV"), trains)
        if train_types:
            trains = filter(lambda t: train_type_filter(t, train_types), trains)
        if platform != "all":
            trains = filter(lambda t: t['scheduledPlatform'] == platform, trains)
        trains = list(trains)
        if trains or not random_station:
            break
    trains = sorted(trains, key=dbi.time_sort)
    
    # Filter out double trains (same platform, time and destination)
    uids = set()
    new_trains = []
    for train in trains:
        uid = "{platform} {time} {destination}".format(platform=train.get('platform', ""), time=train.get('scheduledDeparture', ""), destination=train.get('destination', ""))
        if uid not in uids:
            uids.add(uid)
            new_trains.append(train)
    trains = new_trains
    
    # If there are at least two trains, check if there are two trains that are coupled
    double_pair = None
    if len(trains) >= 2:
        search_length = min(len(trains), 5)
        x = 0
        for y in range(search_length):
            if x == y:
                continue
            if trains[x]['scheduledDeparture'] == trains[y]['scheduledDeparture'] and trains[x]['scheduledPlatform'] == trains[y]['scheduledPlatform']:
                double_pair = (x, y)
                break
    
    double = False
    if double_pair:
        double = True
        x, y = double_pair
        main_trains = [trains[x], trains[y]]
        next_trains_set = set([i for i, t in enumerate(trains)])
        next_trains_set -= set(double_pair)
        next_trains_idx = list(next_trains_set)
        next_trains = []
        if len(next_trains_idx) >= 1:
            next_trains.append(trains[next_trains_idx[0]])
        if len(next_trains_idx) >= 2:
            next_trains.append(trains[next_trains_idx[1]])
    elif len(trains) >= 1:
        main_trains = [trains[0]]
        next_trains = []
        if len(trains) >= 2:
            next_trains.append(trains[1])
        if len(trains) >= 3:
            next_trains.append(trains[2])
    else:
        main_trains = []
        next_trains = []
    
    has_main_trains = bool(main_trains)
    has_next_trains = bool(next_trains)
    
    main_trains += [{}] * (2 - len(main_trains))
    next_trains += [{}] * (2 - len(next_trains))
    
    if not has_main_trains:
        data = {
            'placeholders': {
                'station': display_station,
                'destination': "Kein Zugverkehr",
                'next_trains': "",
            }
        }
        if random_station:
            data['placeholders']['station'] = ds100.get(station_code).get('short_name')
        layout = layout_single
    else:
        delay_next_1 = dbi.round_delay(next_trains[0].get('delayDeparture', 0))
        delay_next_2 = dbi.round_delay(next_trains[1].get('delayDeparture', 0))
        
        if delay_next_1 == -1:
            delay_next_1 = ">210"
        if delay_next_2 == -1:
            delay_next_2 = ">210"
        
        if double:
            train1 = main_trains[0]
            train2 = main_trains[1]
            
            data = {
                'placeholders': {
                    'station': display_station,
                    'platform': str(train1.get('scheduledPlatform')),
                    'departure_1': train1.get('scheduledDeparture'),
                    'info': get_info_long(train1),
                    'train_1': get_train_number(train1.get('train', ""), replacement_map),
                    'via_1': " \xb4 ".join(get_via(train1, replacement_map)),
                    'destination_1': get_destination_name(train1.get('destination', ""), replacement_map),
                    'departure_2': train2.get('scheduledDeparture'),
                    'train_2': get_train_number(train2.get('train', ""), replacement_map),
                    'via_2': " \xb4 ".join(get_via(train2, replacement_map)),
                    'destination_2': get_destination_name(train2.get('destination', ""), replacement_map),
                    'next_train_1_departure': next_trains[0].get('scheduledDeparture'),
                    'next_train_1_delay': "+{}".format(delay_next_1) if delay_next_1 else "",
                    'next_train_2_departure': next_trains[1].get('scheduledDeparture'),
                    'next_train_2_delay': "+{}".format(delay_next_2) if delay_next_2 else "",
                    'next_train_1_train': get_train_number(next_trains[0].get('train', ""), replacement_map),
                    'next_train_2_train': get_train_number(next_trains[1].get('train', ""), replacement_map),
                    'next_train_1_destination': get_destination_name(next_trains[0].get('destination', ""), replacement_map),
                    'next_train_1_info': get_info_short(next_trains[0]),
                    'next_train_2_destination': get_destination_name(next_trains[1].get('destination', ""), replacement_map),
                    'next_train_2_info': get_info_short(next_trains[1])
                }
            }
            
            if random_station:
                data['placeholders']['station'] = ds100.get(station_code).get('short_name')
            if not has_next_trains:
                data['placeholders']['next_trains'] = ""
            layout = layout_double
        else:
            train = main_trains[0]
            if "F" in train.get('trainClasses', []):
                if train.get('train') in COACH_ORDER_CACHE:
                    order_sections, order_coaches = COACH_ORDER_CACHE[train.get('train')]
                else:
                    train_number = int(train.get('train').split()[1])
                    departure_time = datetime.datetime.strptime(train.get('scheduledDeparture'), "%H:%M").time()
                    if departure_time < datetime.datetime.now().time():
                        departure_date = datetime.date.today() + datetime.timedelta(days=1)
                    else:
                        departure_date = datetime.date.today()
                    departure_dt = datetime.datetime.combine(departure_date, departure_time)
                    departure = departure_dt.strftime("%Y%m%d%H%M")
                    order_data = _get_coach_order(train_number, departure)
                    #print(order_data)
                    order_sections, order_coaches = get_coach_order_strings(order_data)
                    #print(order_sections, order_coaches)
                    COACH_ORDER_CACHE[train.get('train')] = (order_sections, order_coaches)
            else:
                order_sections = None
                order_coaches = None
            
            data = {
                'placeholders': {
                    'station': display_station,
                    'platform': str(train.get('scheduledPlatform')),
                    'departure': train.get('scheduledDeparture'),
                    'info': get_info_long(train),
                    'train': get_train_number(train.get('train', ""), replacement_map),
                    'order_sections': "\x25\x26\x27\x28\x29\x2a\x2b" if order_sections else None,
                    'order_coaches': order_coaches,
                    'via': " \xb4 ".join(get_via(train, replacement_map)),
                    'destination': get_destination_name(train.get('destination', ""), replacement_map),
                    'next_train_1_departure': next_trains[0].get('scheduledDeparture'),
                    'next_train_1_delay': "+{}".format(delay_next_1) if delay_next_1 else "",
                    'next_train_2_departure': next_trains[1].get('scheduledDeparture'),
                    'next_train_2_delay': "+{}".format(delay_next_2) if delay_next_2 else "",
                    'next_train_1_train': get_train_number(next_trains[0].get('train', ""), replacement_map),
                    'next_train_2_train': get_train_number(next_trains[1].get('train', ""), replacement_map),
                    'next_train_1_destination': get_destination_name(next_trains[0].get('destination', ""), replacement_map),
                    'next_train_1_info': get_info_short(next_trains[0]),
                    'next_train_2_destination': get_destination_name(next_trains[1].get('destination', ""), replacement_map),
                    'next_train_2_info': get_info_short(next_trains[1])
                }
            }
            if random_station:
                data['placeholders']['station'] = ds100.get(station_code).get('short_name')
            if not has_next_trains:
                data['placeholders']['next_trains'] = ""
            layout = layout_single
    
    data_hash = hash(json.dumps(data, sort_keys=True))
    if data_hash != LAST_DATA_HASH:
        renderer.free_scroll_buffers()
        renderer.display(layout, data)
        LAST_DATA_HASH = data_hash
    time.sleep(duration)
    if auto_clear_scroll_buf:
        renderer.free_scroll_buffers()


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--layout-single', '-ls', required=True, type=str)
    parser.add_argument('--layout-double', '-ld', required=False, type=str)
    parser.add_argument('--font-dir', '-fd', required=True, type=str)
    parser.add_argument('--station', '-s', required=True, type=str)
    parser.add_argument('--train-types', '-tt', required=False, type=str)
    parser.add_argument('--platform', '-p', required=False, type=str, default="all")
    parser.add_argument('--replacement-map', '-rm', required=False, type=str)
    parser.add_argument('--emulate', '-e', action='store_true', help="Run in emulation mode")
    parser.add_argument('--dbi-host', required=False, type=str, default="dbf.finalrewind.org")
    args = parser.parse_args()
    
    if args.emulate:
        fia = FIAEmulator(width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)
    else:
        fia = FIA("/dev/ttyAMA1", (3, 0), width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)

    if BL_AUX_IN_1_ENABLED:
        time.sleep(3)
        gpio.setmode(gpio.BCM)
        gpio.setup(FIA.PIN_CTRL_AUX1_OUT, gpio.IN)
        gpio.add_event_detect(FIA.PIN_CTRL_AUX1_OUT, gpio.RISING, callback=lambda pin: on_bl_button_pressed(pin, fia))
        fia.set_backlight_state(0)
    
    renderer = LayoutRenderer(args.font_dir, fia=fia)
    
    dbi = DBInfoscreen(args.dbi_host)
    ds100 = DS100()
    
    with open(args.layout_single, 'r', encoding='utf-8') as f:
        layout_single = json.load(f)
    
    if args.layout_double:
        with open(args.layout_double, 'r', encoding='utf-8') as f:
            layout_double = json.load(f)
    else:
        layout_double = layout_single

    if args.train_types:
        train_types = args.train_types.split(",")
    else:
        train_types = []
    
    if args.replacement_map:
        with open(args.replacement_map, 'r') as f:
            replacement_map = json.load(f)
    else:
        replacement_map = None
    
    config = {
        'station': args.station,
        'train_types': args.train_types,
        'platform': args.platform,
        'layout_single': layout_single,
        'layout_double': layout_double,
        'replacement_map': replacement_map,
        'duration': 30
    }
    
    while True:
        try:
            show_departures(dbi, ds100, fia, renderer, config)
        except KeyboardInterrupt:
            renderer.free_scroll_buffers()
            fia.exit()
            break
        except:
            traceback.print_exc()
            time.sleep(10)

if __name__ == "__main__":
    main()
