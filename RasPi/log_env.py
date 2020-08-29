import time
from fia_control import FIA
from local_settings import *

fia = FIA("/dev/ttyAMA1", (3, 0), width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)

env_brt_a, env_brt_b = fia.get_env_brightness()
bl_brt_a, bl_brt_b = fia.get_backlight_brightness()
bl_base_brt_a, bl_base_brt_b = fia.get_backlight_base_brightness()
bl_state = fia.get_backlight_state()
bl_temp, air_temp, board_temp, mcu_temp = fia.get_temperatures()
hum = fia.get_humidity()
bl_fan = fia.get_backlight_ballast_fans_state()
circ_fan = fia.get_circulation_fans_state()
exch_fan = fia.get_heat_exchanger_fan_state()
heaters = fia.get_heaters_state()
doors = fia.get_door_states()
contrast_a, contrast_b = fia.get_lcd_contrast()

values = ",".join(map(str, (env_brt_a, env_brt_b, bl_brt_a, bl_brt_b, bl_base_brt_a, bl_base_brt_b, bl_state, bl_temp, air_temp, board_temp, mcu_temp, hum, bl_fan, circ_fan, exch_fan, heaters, doors, contrast_a, contrast_b)))

with open("env_log.csv", 'a') as f:
    f.write(f"{time.time():.0f},{values}\n")
