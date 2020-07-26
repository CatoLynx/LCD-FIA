import argparse
import datetime
import json
import random
import requests
import time
import traceback

from deutschebahn import DBInfoscreen, DS100
from layout_renderer import LayoutRenderer
from fia_control import FIA

from db_common import *
from local_settings import *

if BL_AUX_IN_1_ENABLED:
    import threading
    import RPi.GPIO as gpio


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


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--layout-single', '-ls', required=True, type=str)
    parser.add_argument('--layout-double', '-ld', required=False, type=str)
    parser.add_argument('--font-dir', '-fd', required=True, type=str)
    parser.add_argument('--station', '-s', required=True, type=str)
    parser.add_argument('--train-types', '-tt', required=False, type=str)
    parser.add_argument('--platform', '-p', required=False, type=str, default="all")
    args = parser.parse_args()

    fia = FIA("/dev/ttyAMA1", (3, 0), width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)

    if BL_AUX_IN_1_ENABLED:
        gpio.setmode(gpio.BCM)
        gpio.setup(FIA.PIN_CTRL_AUX1_OUT, gpio.IN)
        gpio.add_event_detect(FIA.PIN_CTRL_AUX1_OUT, gpio.RISING, callback=lambda pin: on_bl_button_pressed(pin, fia))
        fia.set_backlight_state(0)
    
    renderer = LayoutRenderer(args.font_dir, fia=fia)
    
    dbi = DBInfoscreen("dbf.finalrewind.org")
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
    
    last_data_hash = 0
    
    COACH_ORDER_CACHE = {}
    while True:
        try:
            random_station = args.station == "RANDOM"
            num_tries = 0
            while num_tries < 5:
                num_tries += 1
                if random_station:
                    filter_func = lambda s: s['type'] == "Bf" and not s['code'][0] in ("X", "Z")
                    station_code = random.choice([code for code, data in ds100.filter(filter_func).items()])
                    print("Trying station {}".format(station_code))
                    trains = dbi.calc_real_times(dbi.get_trains(station_code))
                else:
                    station_code = args.station
                    trains = dbi.calc_real_times(dbi.get_trains(args.station))
                trains = filter(lambda t: t['actualDeparture'] is not None, trains)
                trains = filter(lambda t: not t['train'].startswith("Bus SEV"), trains)
                if args.train_types:
                    trains = filter(lambda t: train_type_filter(t, train_types), trains)
                if args.platform != "all":
                    trains = filter(lambda t: t['scheduledPlatform'] == args.platform, trains)
                trains = list(trains)
                if trains or not random_station:
                    break
            trains = sorted(trains, key=dbi.time_sort)
            
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
                            'platform': str(train1.get('scheduledPlatform')),
                            'departure_1': train1.get('scheduledDeparture'),
                            'info': get_info_long(train1),
                            'train_1': get_train_number(train1.get('train', "")),
                            'via_1': " \xb4 ".join(map(get_via_name, train1.get('via', [])[:2])),
                            'destination_1': get_destination_name(train1.get('destination', "")),
                            'departure_2': train2.get('scheduledDeparture'),
                            'train_2': get_train_number(train2.get('train', "")),
                            'via_2': " \xb4 ".join(map(get_via_name, train2.get('via', [])[:2])),
                            'destination_2': get_destination_name(train2.get('destination', "")),
                            'next_train_1_departure': next_trains[0].get('scheduledDeparture'),
                            'next_train_1_delay': "+{}".format(delay_next_1) if delay_next_1 else "",
                            'next_train_2_departure': next_trains[1].get('scheduledDeparture'),
                            'next_train_2_delay': "+{}".format(delay_next_2) if delay_next_2 else "",
                            'next_train_1_train': get_train_number(next_trains[0].get('train', "")),
                            'next_train_2_train': get_train_number(next_trains[1].get('train', "")),
                            'next_train_1_destination': get_destination_name(next_trains[0].get('destination', "")),
                            'next_train_1_info': get_info_short(next_trains[0]),
                            'next_train_2_destination': get_destination_name(next_trains[1].get('destination', "")),
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
                            order_data = dbi.get_coach_order(train_number, departure)
                            #print(order_data)
                            order_sections, order_coaches = get_coach_order_strings(order_data)
                            #print(order_sections, order_coaches)
                            COACH_ORDER_CACHE[train.get('train')] = (order_sections, order_coaches)
                    else:
                        order_sections = None
                        order_coaches = None
                    
                    data = {
                        'placeholders': {
                            'platform': str(train.get('scheduledPlatform')),
                            'departure': train.get('scheduledDeparture'),
                            'info': get_info_long(train),
                            'train': get_train_number(train.get('train', "")),
                            'order_sections': "\x25\x26\x27\x28\x29\x2a\x2b" if order_sections else None,
                            'order_coaches': order_coaches,
                            'via': " \xb4 ".join(map(get_via_name, train.get('via', [])[:2])),
                            'destination': get_destination_name(train.get('destination', "")),
                            'next_train_1_departure': next_trains[0].get('scheduledDeparture'),
                            'next_train_1_delay': "+{}".format(delay_next_1) if delay_next_1 else "",
                            'next_train_2_departure': next_trains[1].get('scheduledDeparture'),
                            'next_train_2_delay': "+{}".format(delay_next_2) if delay_next_2 else "",
                            'next_train_1_train': get_train_number(next_trains[0].get('train', "")),
                            'next_train_2_train': get_train_number(next_trains[1].get('train', "")),
                            'next_train_1_destination': get_destination_name(next_trains[0].get('destination', "")),
                            'next_train_1_info': get_info_short(next_trains[0]),
                            'next_train_2_destination': get_destination_name(next_trains[1].get('destination', "")),
                            'next_train_2_info': get_info_short(next_trains[1])
                        }
                    }
                    if random_station:
                        data['placeholders']['station'] = ds100.get(station_code).get('short_name')
                    if not has_next_trains:
                        data['placeholders']['next_trains'] = ""
                    layout = layout_single
            
            data_hash = hash(json.dumps(data, sort_keys=True))
            if data_hash != last_data_hash:
                renderer.free_scroll_buffers()
                renderer.display(layout, data)
                last_data_hash = data_hash
            time.sleep(60 if random_station else 30)
        except KeyboardInterrupt:
            renderer.free_scroll_buffers()
            break
        except:
            traceback.print_exc()
            time.sleep(10)

if __name__ == "__main__":
    main()
