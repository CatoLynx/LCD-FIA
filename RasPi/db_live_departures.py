import argparse
import datetime
import json
import re
import requests
import time
import traceback

from db_infoscreen import DBInfoscreen
from layout_renderer import LayoutRenderer
from fia_control import FIA


GENERAL_STATION_REPLACEMENTS = [
    (r"Frankfurt\(Main\)", "Frankfurt "),
    (r"Offenbach\(Main\)", "Offenbach "),
    (r"\(Hess\)", ""),
    (r"\(Taunus\)", ""),
    (r" Bahnhof", " Bhf"),
    (r"Frankfurt\(M\) Flughafen(.+)", "F-Flugh.\g<1> \xb0"),
    (r"Gesundbrunnen", "Gesundbr."),
    (r"(?<=\S)\(", " ("),
]

DESTINATION_REPLACEMENTS = [
    (r"Frankfurt \(M\)", "Frankfurt "),
]

VIA_REPLACEMENTS = [
    (r"Frankfurt-", "F-"),
    (r"Frankfurt \(M\)", "F-"),
    (r"Frankfurt am Main - ", "F-"),
    (r"Frankfurt (?!am Main)", "F-"),
]

TRAIN_NUMBER_REPLACEMENTS = [
    (r"VIA R", "R"),
]


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

def get_general_station_name(name):
    for regex, replacement in GENERAL_STATION_REPLACEMENTS:
        name = re.sub(regex, replacement, name)
    return name

def get_destination_name(name):
    name = get_general_station_name(name)
    for regex, replacement in DESTINATION_REPLACEMENTS:
        name = re.sub(regex, replacement, name)
    return name

def get_via_name(name):
    name = get_general_station_name(name)
    for regex, replacement in VIA_REPLACEMENTS:
        name = re.sub(regex, replacement, name)
    return name

def get_train_number(line):
    for regex, replacement in TRAIN_NUMBER_REPLACEMENTS:
        line = re.sub(regex, replacement, line)
    return line

def get_coach_order_strings(data):
    SEC_LBL_MAP = {
        "A": "\x25",
        "B": "\x26",
        "C": "\x27",
        "D": "\x28",
        "E": "\x29",
        "F": "\x2a",
        "G": "\x2b"
    }
    
    CAR_RIGHT = "\x90"
    CAR_LEFT = "\x91"
    CAR = "\x92"
    CAR_1CL = "\x93"
    CAR_2CL = "\x94"
    CAR_RIGHT_1CL = "\xa6"
    CAR_LEFT_1CL = "\xa7"
    CAR_RIGHT_2CL = "\xa8"
    CAR_LEFT_2CL = "\xa9"
    CAR_BIKE = "\x31"
    CAR_WHEELCHAIR = "\x32"
    CAR_RESTAURANT = "\x33"
    CAR_BISTRO = "\x34"
    CAR_BED_1 = "\x35"
    CAR_BED_2 = "\x36"
    
    CAR_INV = "\x2e"
    CAR_1CL_INV = "\x2f"
    CAR_2CL_INV = "\x30"
    CAR_BIKE_INV = "\x95"
    CAR_WHEELCHAIR_INV = "\x96"
    CAR_RESTAURANT_INV = "\x97"
    CAR_BISTRO_INV = "\x98"
    CAR_BED_1_INV = "\x99"
    CAR_BED_2_INV = "\x9a"
    
    END_CAR_LEFT_WIDE_1CL = "\x9b"
    END_CAR_LEFT_WIDE_2CL = "\x9c"
    END_CAR_RIGHT_WIDE_1CL = "\x9d"
    END_CAR_RIGHT_WIDE_2CL = "\x9e"
    END_CAR_LEFT_WIDE = "\x9f"
    END_CAR_RIGHT_WIDE = "\xa0"
    END_CAR_LEFT_WIDE_BIKE = "\x3d"
    END_CAR_RIGHT_WIDE_BIKE = "\x3e"
    END_CAR_RIGHT_NARROW_1CL = "\xac"
    END_CAR_LEFT_NARROW_1CL = "\xad"
    
    END_CAR_LEFT_WIDE_1CL_INV = "\x37"
    END_CAR_LEFT_WIDE_2CL_INV = "\x38"
    END_CAR_RIGHT_WIDE_1CL_INV = "\x39"
    END_CAR_RIGHT_WIDE_2CL_INV = "\x3a"
    END_CAR_LEFT_WIDE_INV = "\x3b"
    END_CAR_RIGHT_WIDE_INV = "\x3c"
    END_CAR_LEFT_WIDE_BIKE_INV = "\xa1"
    END_CAR_RIGHT_WIDE_BIKE_INV = "\xa2"
    END_CAR_RIGHT_NARROW_1CL_INV = "\x44"
    END_CAR_LEFT_NARROW_1CL_INV = "\x45"
    
    CPL_TIP_TIP = "\xa5"
    CPL_CAR_TIP_LEFT = "\xae"
    CPL_TIP_RIGHT_CAR = "\xaf"
    
    CPL_TIP_TIP_INV = "\x3f"
    CPL_CAR_TIP_LEFT_INV = "\x46"
    CPL_TIP_RIGHT_CAR_INV = "\x47"
    
    TIP_LEFT_SMALL = "\xa3"
    TIP_RIGHT_SMALL = "\xa4"
    TIP_LEFT_LARGE = "\xaa"
    TIP_RIGHT_LARGE = "\xab"
    
    TIP_LEFT_SMALL_INV = "\x40"
    TIP_RIGHT_SMALL_INV = "\x41"
    TIP_LEFT_LARGE_INV = "\x42"
    TIP_RIGHT_LARGE_INV = "\x43"
    
    with open("wr.json", 'w') as f:
        json.dump(data, f)
    
    if not data:
        return (None, None)
    try:
        units = data['data']['istformation']['allFahrzeuggruppe']
    except KeyError:
        return (None, None)
    if not units:
        return (None, None)
    
    coaches = []
    for unit in units:
        for coach in unit['allFahrzeug']:
            coaches.append((unit['fahrzeuggruppebezeichnung'], coach))
    
    # Reshape data into a dict mapping sections to coaches and
    # their unit's train number
    section_unit_coaches = {}
    for coach in coaches:
        section = coach[1]['fahrzeugsektor']
        if section not in section_unit_coaches:
            section_unit_coaches[section] = {}
        unit = coach[0]
        if unit not in section_unit_coaches[section]:
            section_unit_coaches[section][unit] = []
        section_unit_coaches[section][unit].append(coach)
    
    # Reshape once more into a dict mapping sections to dicts
    # mapping the unit's train number to the list of
    # coach types and their respective features
    section_coach_details = {}
    for section, units in section_unit_coaches.items():
        section_coach_details[section] = {}
        for unit, coaches in units.items():
            if unit not in section_coach_details:
                section_coach_details[section][unit] = []
            section_coach_details[section][unit] += [(coach[1]['kategorie'], [a['ausstattungsart'] for a in coach[1]['allFahrzeugausstattung']]) for coach in coaches]
    
    # Ensure we are going through the sections alphabetically
    section_coach_details_sorted = sorted(section_coach_details.items(), key=lambda scd: scd[0])
    
    # Filter the data to remove invalid sections
    # and coach data containing only irrelevant coach types
    # like no-seat control cars and locomotives.
    # Also, we are assuming that the train is contiguous
    # (i.e. there are no empty sections in the middle of a train)
    section_coach_details_sorted_filtered = []
    for section, units in section_coach_details_sorted:
        if section not in SEC_LBL_MAP:
            continue
        coach_details = [cd for cds in units.values() for cd in cds]
        coach_types = set([cd[0] for cd in coach_details])
        coach_features = set([ft for fts in [cd[1] for cd in coach_details] for ft in fts])
        if coach_types <= {"TRIEBKOPF", "LOK"}:
            continue
        section_coach_details_sorted_filtered.append((section, coach_types, coach_features))
    
    # Apply some logic to determine the most important coach type
    # per section (i.e. the one that will be displayed)
    sections_string = ""
    coaches_string = ""
    unit_started = False # To remember if the next control car will be < or >
    length = len(section_coach_details_sorted_filtered)
    
    first_section = section_coach_details_sorted[0][0]
    for section in "ABCDEFG":
        if section == first_section:
            break
        coaches_string += " "
    
    for i, data in enumerate(section_coach_details_sorted_filtered):
        section, coach_types, coach_features = data
        first_coach = i == 0
        last_coach = i == length - 1
        
        sections_string += SEC_LBL_MAP.get(section, " ")
        #print("Section {}".format(section))
        #print(i, first_coach, last_coach)
        #print(coach_types)
        #print(coach_features)
        #print("")
        if coach_types <= {"TRIEBKOPF", "LOK"}:
            coaches_string += " "
        elif "STEUERWAGENERSTEKLASSE" in coach_types or "DOPPELSTOCKSTEUERWAGENERSTEKLASSE" in coach_types:
            if unit_started:
                coaches_string += END_CAR_RIGHT_WIDE_1CL_INV
            else:
                coaches_string += END_CAR_LEFT_WIDE_1CL_INV
            unit_started = not unit_started
        elif "STEUERWAGENZWEITEKLASSE" in coach_types or "DOPPELSTOCKSTEUERWAGENZWEITEKLASSE" in coach_types:
            if "PLAETZEFAHRRAD" in coach_features:
                if unit_started:
                    coaches_string += END_CAR_RIGHT_WIDE_BIKE_INV
                else:
                    coaches_string += END_CAR_LEFT_WIDE_BIKE_INV
            else:
                if unit_started:
                    coaches_string += END_CAR_RIGHT_WIDE_2CL
                else:
                    coaches_string += END_CAR_LEFT_WIDE_2CL
            unit_started = not unit_started
        elif "SPEISEWAGEN" in coach_types or "HALBSPEISEWAGENZWEITEKLASSE" in coach_types or "HALBSPEISEWAGENERSTEKLASSE" in coach_types:
            coaches_string += CAR_RESTAURANT_INV
        elif "BISTRO" in coach_types:
            coaches_string += CAR_BISTRO_INV
        elif "DOPPELSTOCKWAGENERSTEKLASSE" in coach_types:
            # To prevent double-decker coaches from showing up as bike spaces
            # since they generally all have bike spaces
            coaches_string += CAR_1CL_INV
        elif "DOPPELSTOCKWAGENZWEITEKLASSE" in coach_types:
            # see previous
            if first_coach:
                coaches_string += CAR_LEFT_2CL
            elif last_coach:
                coaches_string += CAR_RIGHT_2CL
            else:
                coaches_string += CAR_2CL
        elif "PLAETZEFAHRRAD" in coach_features:
            coaches_string += CAR_BIKE_INV
        elif "PLAETZEROLLSTUHL" in coach_features:
            coaches_string += CAR_WHEELCHAIR_INV
        elif "REISEZUGWAGENERSTEKLASSE" in coach_types:
            coaches_string += CAR_1CL_INV
        elif "REISEZUGWAGENZWEITEKLASSE" in coach_types:
            if first_coach:
                coaches_string += CAR_LEFT_2CL
            elif last_coach:
                coaches_string += CAR_RIGHT_2CL
            else:
                coaches_string += CAR_2CL
        else:
            if first_coach:
                coaches_string += CAR_LEFT
            elif last_coach:
                coaches_string += CAR_RIGHT
            else:
                coaches_string += CAR
        
        if first_coach:
            unit_started = True
    
    return (sections_string, coaches_string)


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
    
    COACH_ORDER_CACHE = {}
    while True:
        try:
            trains = [train for train in dbi.calc_real_times(dbi.get_trains(args.station)) if train['actualDeparture'] is not None and not train['train'].startswith("Bus SEV")]
            trains.sort(key=dbi.time_sort)
            
            delay_next_1 = dbi.round_delay(trains[1]['delayDeparture'])
            delay_next_2 = dbi.round_delay(trains[2]['delayDeparture'])
            
            if delay_next_1 == -1:
                delay_next_1 = ">210"
            if delay_next_2 == -1:
                delay_next_2 = ">210"
            
            if "F" in trains[0]['trainClasses']:
                if trains[0]['train'] in COACH_ORDER_CACHE:
                    order_sections, order_coaches = COACH_ORDER_CACHE[trains[0]['train']]
                else:
                    train_number = int(trains[0]['train'].split()[1])
                    departure_time = datetime.datetime.strptime(trains[0]['scheduledDeparture'], "%H:%M").time()
                    if departure_time < datetime.datetime.now().time():
                        departure_date = datetime.date.today() + datetime.timedelta(days=1)
                    else:
                        departure_date = datetime.date.today()
                    departure_dt = datetime.datetime.combine(departure_date, departure_time)
                    departure = departure_dt.strftime("%Y%m%d%H%M")
                    order_sections, order_coaches = get_coach_order_strings(dbi.get_coach_order(train_number, departure))
                    COACH_ORDER_CACHE[trains[0]['train']] = (order_sections, order_coaches)
            else:
                order_sections = None
                order_coaches = None
            
            data = {
                'placeholders': {
                    'platform': str(trains[0]['scheduledPlatform']),
                    'departure': trains[0]['scheduledDeparture'],
                    'info': get_info_long(trains[0]),
                    'train': get_train_number(trains[0]['train']),
                    'order_sections': "\x25\x26\x27\x28\x29\x2a" if order_sections else None,
                    'order_coaches': order_coaches,
                    'via': " \xb4 ".join(map(get_via_name, trains[0]['via'][:2])),
                    'destination': get_destination_name(trains[0]['destination']),
                    'next_train_1_departure': trains[1]['scheduledDeparture'],
                    'next_train_1_delay': "+{}".format(delay_next_1) if delay_next_1 else "",
                    'next_train_2_departure': trains[2]['scheduledDeparture'],
                    'next_train_2_delay': "+{}".format(delay_next_2) if delay_next_2 else "",
                    'next_train_1_train': get_train_number(trains[1]['train']),
                    'next_train_2_train': get_train_number(trains[2]['train']),
                    'next_train_1_destination': get_destination_name(trains[1]['destination']),
                    'next_train_1_info': get_info_short(trains[1]),
                    'next_train_2_destination': get_destination_name(trains[2]['destination']),
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
