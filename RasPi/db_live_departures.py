import argparse
import datetime
import json
import requests
import time
import traceback

from db_infoscreen import DBInfoscreen
from layout_renderer import LayoutRenderer
from fia_control import FIA

def get_info_long(train):
    messages = []
    if train['isCancelled']:
        messages.append("fällt heute aus")
    if train['platform'] != train['scheduledPlatform']:
        messages.append("heute von Gleis {}".format(train['platform']))
    delay = DBInfoscreen.round_delay(train['delayDeparture'])
    if delay:
        if delay > 0:
            messages.append("heute ca. {} Minuten später".format(delay))
        else:
            messages.append("heute unbestimmt verspätet")
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


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--layout', '-l', required=True, type=str)
    parser.add_argument('--font-dir', '-fd', required=True, type=str)
    parser.add_argument('--station', '-s', required=True, type=str)
    args = parser.parse_args()

    fia = FIA("/dev/ttyAMA1", (3, 0))
    
    renderer = LayoutRenderer(args.font_dir)
    
    dbi = DBInfoscreen("dbf.finalrewind.org")
    
    with open(args.layout, 'r', encoding='utf-8') as f:
        layout = json.load(f)
    
    while True:
        try:
            trains = [train for train in dbi.calc_real_times(dbi.get_trains(args.station)) if train['actualDeparture']]
            trains.sort(key=dbi.time_sort)
            
            delay_next_1 = dbi.round_delay(trains[1]['delayDeparture'])
            delay_next_2 = dbi.round_delay(trains[2]['delayDeparture'])
            
            if delay_next_1 == -1:
                delay_next_1 = ">210"
            if delay_next_2 == -1:
                delay_next_2 = ">210"
            
            data = {
                'placeholders': {
                    'platform': str(trains[0]['scheduledPlatform']),
                    'departure': trains[0]['scheduledDeparture'],
                    'info': get_info_long(trains[0]),
                    'train': trains[0]['train'],
                    'via': "  -  ".join(trains[0]['via'][:2]),
                    'destination': trains[0]['destination'],
                    'next_train_1_departure': trains[1]['scheduledDeparture'],
                    'next_train_1_delay': "+{}".format(delay_next_1) if delay_next_1 else "",
                    'next_train_2_departure': trains[2]['scheduledDeparture'],
                    'next_train_2_delay': "+{}".format(delay_next_2) if delay_next_2 else "",
                    'next_train_1_train': trains[1]['train'],
                    'next_train_2_train': trains[2]['train'],
                    'next_train_1_destination': trains[1]['destination'],
                    'next_train_1_info': get_info_short(trains[1]),
                    'next_train_2_destination': trains[2]['destination'],
                    'next_train_2_info': get_info_short(trains[2])
                }
            }
            
            img = renderer.render(layout, data)
            fia.send_image(img)
            time.sleep(30)
        except KeyboardInterrupt:
            break
        except:
            traceback.print_exc()
            time.sleep(10)

if __name__ == "__main__":
    main()
