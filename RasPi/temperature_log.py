import time

from PIL import Image

from fia_control import FIA


def main():
    fia = FIA("/dev/ttyAMA1", (3, 0))
    
    fia.set_backlight_state(1)
    fia.set_heaters_state(0)
    fia.set_circulation_fans_state(2)
    fia.set_heat_exchanger_fan_state(1)
    fia.set_backlight_ballast_fans_state(1)
    
    fia.set_backlight_base_brightness(2048, 2048)
    
    fia.send_image(Image.new('L', (480, 128), 'white'))
    
    with open("temperatures.csv", 'a') as f:
        while True:
            temps = list(fia.get_temperatures())
            humidity = fia.get_humidity()
            data = [time.time()] + temps + [humidity]
            print(data)
            f.write(",".join(map(str, data)) + "\n")
            time.sleep(5)


if __name__ == "__main__":
    main()
