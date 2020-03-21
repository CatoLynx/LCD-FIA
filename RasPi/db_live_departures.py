import argparse
import datetime
import json
import requests
import spidev
import time
import traceback

from render_layout import LayoutRenderer
from send_image_spi import send_image


def load_trains(station):
    resp = requests.get("https://dbf.finalrewind.org/{}.json".format(station))
    data = resp.json()
    return data['departures']

def calc_real_times(trains):
    output = []
    for train in trains:
        if train['scheduledDeparture']:
            departure = datetime.datetime.strptime(train['scheduledDeparture'], "%H:%M")
            departure += datetime.timedelta(minutes=train['delayDeparture'])
            train['actualDeparture'] = departure.strftime("%H:%M")
        else:
            train['actualDeparture'] = None
        
        if train['scheduledArrival']:
            departure = datetime.datetime.strptime(train['scheduledArrival'], "%H:%M")
            departure += datetime.timedelta(minutes=train['delayArrival'])
            train['actualArrival'] = departure.strftime("%H:%M")
        else:
            train['actualArrival'] = None
        
        output.append(train)
    return output

def get_info_long(train):
    messages = []
    if train['isCancelled']:
        messages.append("fällt heute aus")
    if train['platform'] != train['scheduledPlatform']:
        messages.append("heute von Gleis {}".format(train['platform']))
    if train['delayDeparture']:
        messages.append("heute ca. {} Minuten später".format(train['delayDeparture']))
    if messages:
        return " " + "  ---  ".join(messages)
    else:
        return ""

def get_info_short(train):
    if train['isCancelled']:
        return "fällt aus"
    elif train['platform'] != train['scheduledPlatform']:
        return "Gleis {}".format(train['platform'])
    return ""

def time_sort(train):
    now = datetime.datetime.now()
    now_date = now.date()
    now_time = now.time()
    departure_time = datetime.datetime.strptime(train['actualDeparture'], "%H:%M").time()
    departure = datetime.datetime.combine(now_date, departure_time)
    if now_time >= datetime.time(12, 0) and departure_time < datetime.time(12, 0):
        departure += datetime.timedelta(days=1)
    return departure


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--layout', '-l', required=True, type=str)
    parser.add_argument('--font-dir', '-fd', required=True, type=str)
    parser.add_argument('--station', '-s', required=True, type=str)
    args = parser.parse_args()

    spi = spidev.SpiDev()
    spi.open(3, 0)
    spi.max_speed_hz = 5000000
    
    renderer = LayoutRenderer(args.font_dir)
    
    with open(args.layout, 'r', encoding='utf-8') as f:
        layout = json.load(f)
    
    while True:
        try:
            trains = [train for train in calc_real_times(load_trains(args.station)) if train['actualDeparture']]
            trains.sort(key=time_sort)
            
            data = {
                'placeholders': {
                    'platform': str(trains[0]['scheduledPlatform']),
                    'departure': trains[0]['scheduledDeparture'],
                    'info': get_info_long(trains[0]),
                    'train': trains[0]['train'],
                    'via': "  -  ".join(trains[0]['via'][:2]),
                    'destination': trains[0]['destination'],
                    'next_train_1_departure': trains[1]['scheduledDeparture'],
                    'next_train_1_delay': "+{}".format(trains[1]['delayDeparture']) if trains[1]['delayDeparture'] else "",
                    'next_train_2_departure': trains[2]['scheduledDeparture'],
                    'next_train_2_delay': "+{}".format(trains[2]['delayDeparture']) if trains[2]['delayDeparture'] else "",
                    'next_train_1_train': trains[1]['train'],
                    'next_train_2_train': trains[2]['train'],
                    'next_train_1_destination': trains[1]['destination'],
                    'next_train_1_info': get_info_short(trains[1]),
                    'next_train_2_destination': trains[2]['destination'],
                    'next_train_2_info': get_info_short(trains[2])
                }
            }
            
            img = renderer.render(layout, data)
            send_image(img, spi, 64)
            time.sleep(30)
        except KeyboardInterrupt:
            break
        except:
            traceback.print_exc()
            time.sleep(10)

if __name__ == "__main__":
    main()
