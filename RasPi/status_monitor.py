import json
import time
import traceback
from PIL import Image

from fia_control import FIA
from layout_renderer import LayoutRenderer

from local_settings import *

POWER_STATES = ["OFF", "ON"]
MULTI_POWER_STATES = ["OFF", "HALF", "FULL"]
DOOR_STATES = ["CLOSED", "OPEN"]

def main():
    fia = FIA("/dev/ttyAMA1", (3, 0), width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)
    renderer = LayoutRenderer("fonts")
    
    with open("layouts/temp_monitor_5x2.json", 'r', encoding='utf-8') as f:
        layout = json.load(f)
    
    while True:
        try:
            temps = fia.get_temperatures()
            humidity = fia.get_humidity()
            backlight_state = fia.get_backlight_state()
            bl_base_brightness_a, bl_base_brightness_b = fia.get_backlight_base_brightness()
            bl_cur_brightness_a, bl_cur_brightness_b = fia.get_backlight_brightness()
            env_brightness_a, env_brightness_b = fia.get_env_brightness()
            door_states = fia.get_door_states()
            heaters_state = fia.get_heaters_state()
            circ_fans_state = fia.get_circulation_fans_state()
            heat_exc_fan_state = fia.get_heat_exchanger_fan_state()
            bl_ballast_fans_state = fia.get_backlight_ballast_fans_state()
            
            data = {'placeholders': {
                'temp_ballasts': "{:.1f}".format(temps[0]),
                'temp_airflow': "{:.1f}".format(temps[1]),
                'temp_board': "{:.1f}".format(temps[2]),
                'temp_mcu': "{:.1f}".format(temps[3]),
                'humidity': "{:.1f}".format(humidity),
                'backlight_state': POWER_STATES[backlight_state],
                'backlight_base_brt_a': str(bl_base_brightness_a),
                'backlight_base_brt_b': str(bl_base_brightness_b),
                'backlight_cur_brt_a': str(bl_cur_brightness_a),
                'backlight_cur_brt_b': str(bl_cur_brightness_b),
                'env_brt_a': str(env_brightness_a),
                'env_brt_b': str(env_brightness_b),
                'door_state_a': DOOR_STATES[door_states & 1 != 0],
                'door_state_b': DOOR_STATES[door_states & 2 != 0],
                'heaters_state': MULTI_POWER_STATES[heaters_state],
                'circ_fans_state': MULTI_POWER_STATES[circ_fans_state],
                'heat_exc_fan_state': POWER_STATES[heat_exc_fan_state],
                'bl_ballast_fans_state': POWER_STATES[bl_ballast_fans_state],
            }}
            img = renderer.render(layout, data)
            fia.send_image(img)
            time.sleep(1)
        except KeyboardInterrupt:
            break
        except:
            traceback.print_exc()
            time.sleep(1)


if __name__ == "__main__":
    main()
