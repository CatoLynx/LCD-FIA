import argparse
import datetime
import json
import random
import re
import requests
import time
import traceback

from deutschebahn import DBInfoscreen
from layout_renderer import LayoutRenderer
from fia_control import FIA

from db_common import *
from local_settings import *


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--font-dir', '-fd', required=True, type=str)
    parser.add_argument('--station', '-s', required=True, type=str)
    parser.add_argument('--platforms', '-p', required=True, type=str,
        help="Example string: 1a,1-24,101-104")
    args = parser.parse_args()
    
    platforms = []
    raw_platform_list_parts = args.platforms.split(",")
    for part in raw_platform_list_parts:
        if "-" in part:
            p_min, p_max = map(int, part.split("-"))
            platforms += map(str, range(p_min, p_max + 1))
        else:
            platforms.append(part)

    fia = FIA("/dev/ttyAMA1", (3, 0), width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)
    
    renderer = LayoutRenderer(args.font_dir)
    
    dbi = DBInfoscreen("dbf.finalrewind.org")
    
    platform_layout = [
        {
            'name': "platform_0",
            'type': 'text',
            'x': 1,
            'y': 0,
            'width': 19,
            'height': 9,
            'pad_left': 0,
            'pad_top': 0,
            'inverted': False,
            'font': "7_DBLCD",
            'size': 0,
            'align': 'right',
            'spacing': 1
        },
        {
            'name': "departure_0",
            'type': 'text',
            'x': 25,
            'y': 0,
            'width': 30,
            'height': 9,
            'pad_left': 0,
            'pad_top': 0,
            'inverted': False,
            'font': "7_DBLCD",
            'size': 0,
            'align': 'left',
            'spacing': 1
        },
        {
            'name': "train_0",
            'type': 'text',
            'x': 59,
            'y': 0,
            'width': 43,
            'height': 9,
            'pad_left': 0,
            'pad_top': 0,
            'inverted': False,
            'font': "7_DBLCD",
            'size': 0,
            'align': 'left',
            'spacing': 1
        },
        {
            'name': "destination_0",
            'type': 'text',
            'x': 106,
            'y': 0,
            'width': 132,
            'height': 9,
            'pad_left': 0,
            'pad_top': 0,
            'inverted': False,
            'font': "7_DBLCD",
            'size': 0,
            'align': 'left',
            'spacing': 1
        }
    ]
    
    layout = {
        'width': 480,
        'height': 128,
        'placeholders': [
            {
                'name': "divider_1",
                'type': 'line',
                'x': 22,
                'y': 0,
                'x2': 22,
                'y2': 127,
                'inverted': False,
                'line_width': 1
            },
            {
                'name': "divider_2",
                'type': 'line',
                'x': 56,
                'y': 0,
                'x2': 56,
                'y2': 127,
                'inverted': False,
                'line_width': 1
            },
            {
                'name': "divider_3",
                'type': 'line',
                'x': 239,
                'y': 0,
                'x2': 239,
                'y2': 127,
                'inverted': False,
                'line_width': 2
            },
            {
                'name': "divider_4",
                'type': 'line',
                'x': 263,
                'y': 0,
                'x2': 263,
                'y2': 127,
                'inverted': False,
                'line_width': 1
            },
            {
                'name': "divider_5",
                'type': 'line',
                'x': 297,
                'y': 0,
                'x2': 297,
                'y2': 127,
                'inverted': False,
                'line_width': 1
            }
        ]
    }
    
    data = {
        'placeholders': {}
    }
    
    while True:
        try:
            station_code = args.station
            trains = dbi.calc_real_times(dbi.get_trains(args.station))
            trains = filter(lambda t: t['actualDeparture'] is not None, trains)
            trains = filter(lambda t: not t['train'].startswith("Bus SEV"), trains)
            trains = list(trains)
            trains = sorted(trains, key=dbi.time_sort)
            
            x, y = 0, 0
            for i, platform in enumerate(platforms):
                placeholders = [p.copy() for p in platform_layout]
                placeholders[0]['name'] = "platform_" + platform
                placeholders[1]['name'] = "departure_" + platform
                placeholders[2]['name'] = "train_" + platform
                placeholders[3]['name'] = "destination_" + platform
                placeholders[0]['y'] = placeholders[1]['y'] = placeholders[2]['y'] = placeholders[3]['y'] = y
                placeholders[0]['x'] += x
                placeholders[1]['x'] += x
                placeholders[2]['x'] += x
                placeholders[3]['x'] += x
                layout['placeholders'] += placeholders
                
                data['placeholders']['platform_' + platform] = platform
                p_trains = list(filter(lambda t: t['platform'] == platform, trains))
                if p_trains:
                    t = p_trains[0]
                    data['placeholders']['departure_' + platform] = t['actualDeparture']
                    data['placeholders']['train_' + platform] = get_train_number(t['train']).replace(" ", "")
                    data['placeholders']['destination_' + platform] = get_destination_name(t['destination'])
                
                y += 9
                if 56 < y < 64:
                    y = 64
                if y > 120:
                    y = 0
                    x += 241
            
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
