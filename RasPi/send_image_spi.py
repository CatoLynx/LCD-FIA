import argparse
import spidev
import time
from PIL import Image, ImageSequence

from flatten_img import flatten_img
from img_to_array import img_to_array


def send_array(array, port):
    length = len(array)
    port.writebytes2(bytearray(array))

def send_image(img, port, row_height):
    flattened = flatten_img(img, row_height)
    array, width, height = img_to_array(flattened)
    send_array(array, port)


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--file', '-f', required=True, type=str)
    parser.add_argument('--width', '-w', required=False, default=None, type=int)
    parser.add_argument('--height', '-h', required=False, default=None, type=int)
    parser.add_argument('--interval', '-i', required=False, default=None, type=int)
    parser.add_argument('--row-height', '-rh', required=False, default=None, type=int)
    args = parser.parse_args()
    
    spi = spidev.SpiDev()
    spi.open(3, 0)
    spi.max_speed_hz = 5000000
    
    img = Image.open(args.file)
    if args.interval:
        duration = args.interval
    else:
        duration = img.info.get('duration', 1000)
    
    frames = []
    if args.width and args.height:
        for frame in ImageSequence.Iterator(img):
            temp = Image.new('L', (args.width, args.height))
            temp.paste(frame, (0, 0))
            frames.append(temp)
    else:
        for frame in ImageSequence.Iterator(img):
            frames.append(frame.convert('L'))
    
    last_frame_time = 0
    cur_frame = 0
    while True:
        now = time.time()
        if now - last_frame_time >= duration / 1000:
            send_image(frames[cur_frame], spi, args.row_height)
            last_frame_time = now
            cur_frame += 1
        if cur_frame >= len(frames):
            cur_frame = 0


if __name__ == "__main__":
    main()
