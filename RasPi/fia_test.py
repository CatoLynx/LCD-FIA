import time

from fia_control import FIA


def main():
    fia = FIA("/dev/ttyAMA1", (3, 0))
    
    fia.set_heaters_state(0)
    fia.set_circulation_fans_state(0)
    fia.set_heat_exchanger_fan_state(0)
    fia.set_backlight_ballast_fans_state(0)
    
    fia.set_backlight_base_brightness(8192, 8192)
    
    while True:
        print(fia.get_temperatures())
        time.sleep(5)


if __name__ == "__main__":
    main()
