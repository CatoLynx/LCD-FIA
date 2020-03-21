import argparse
import serial
from PIL import Image, ImageGrab

from send_image import send_image


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--port', '-p', required=True, type=str)
    parser.add_argument('--baud', '-b', required=False, default=115200, type=int)
    parser.add_argument('--width', '-w', required=False, default=None, type=int)
    parser.add_argument('--height', '-h', required=False, default=None, type=int)
    args = parser.parse_args()
    
    port = serial.Serial(args.port, baudrate=args.baud)

    x = 0
    y = 500
    w = 128
    h = 288

    while True:
        img = ImageGrab.grab((x, y, x+w, y+h))
        if w > 128 or h > 288:
        	img = img.resize((128, 288), Image.LANCZOS)
        img = img.rotate(-90, expand=True)
        send_image(img, port)


if __name__ == "__main__":
    main()
