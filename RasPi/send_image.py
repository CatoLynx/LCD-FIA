import argparse
import serial
import time
from PIL import Image, ImageSequence

from flatten_img import flatten_img
from img_to_array import img_to_array


def send_array(array, port):
    length = len(array)
    array = [length >> 8, length & 0xFF] + array
    port.write(bytearray(array))

def send_image(img, port):
    flattened = flatten_img(img)
    array, width, height = img_to_array(flattened)
    send_array(array, port)


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--file', '-f', required=True, type=str)
    parser.add_argument('--port', '-p', required=True, type=str)
    parser.add_argument('--baud', '-b', required=False, default=115200, type=int)
    parser.add_argument('--width', '-w', required=False, default=None, type=int)
    parser.add_argument('--height', '-h', required=False, default=None, type=int)
    parser.add_argument('--interval', '-i', required=False, default=None, type=int)
    args = parser.parse_args()
    
    port = serial.Serial(args.port, baudrate=args.baud)
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
            send_image(frames[cur_frame], port)
            last_frame_time = now
            cur_frame += 1
        if cur_frame >= len(frames):
            cur_frame = 0


if __name__ == "__main__":
    main()
