import argparse
import datetime
import math
import spidev
import sys
import time
from PIL import Image, ImageSequence

from fia_control import FIA


def pos_nonzero_int(value):
    try:
        ivalue = int(value)
    except ValueError:
        raise argparse.ArgumentTypeError("{} must be a positive non-zero integer".format(value))
    if ivalue <= 0:
        raise argparse.ArgumentTypeError("{} must be a positive non-zero integer".format(value))
    return ivalue


def main():
    parser = argparse.ArgumentParser(description="This script can show an image or animation on the display.", add_help=False)
    parser.add_argument('--file', '-f', required=True, type=str, help="Input file to display (static image or GIF)")
    parser.add_argument('--width', '-w', required=False, default=None, type=pos_nonzero_int, help="Width to crop input to")
    parser.add_argument('--height', '-h', required=False, default=None, type=pos_nonzero_int, help="Height to crop input to")
    parser.add_argument('--interval', '-i', required=False, default=None, type=pos_nonzero_int, help="Interval between frames in ms (defaults to GIF's setting or one second)")
    parser.add_argument('--countdown', '-c', action='store_true', help="If set, do an interactive countdown before starting to help with manually syncing video and audio")
    parser.add_argument('--loop', '-l', action='store_true', help="If set, loop animation forever")
    parser.add_argument('--help', action='help', help="Display this help message")
    args = parser.parse_args()
    
    fia = FIA("/dev/ttyAMA1", (3, 0))
    
    print("Loading image...")
    img = Image.open(args.file)
    in_width, in_height = img.size
    if args.interval is not None:
        frame_interval = args.interval
    else:
        frame_interval = img.info.get('duration', 1000)
    frame_interval /= 1000
    
    print("Loading frames...")
    frames = []
    if args.width and args.height:
        for frame in ImageSequence.Iterator(img):
            temp = Image.new('L', (args.width, args.height))
            temp.paste(frame, (0, 0))
            frames.append(temp)
    else:
        for frame in ImageSequence.Iterator(img):
            frames.append(frame.convert('L'))
    frame_count = len(frames)
    
    duration = datetime.timedelta(seconds=math.ceil(frame_count * frame_interval))
    print("Input: {width}x{height}, {fps:.2f} fps, {count} frames, duration {duration}".format(width=in_width, height=in_height, fps=1/frame_interval, count=frame_count, duration=duration))
    
    if args.countdown:
        input("Ready! Press Enter to start.")
        sys.stdout.write("3... ")
        sys.stdout.flush()
        time.sleep(1)
        sys.stdout.write("2... ")
        sys.stdout.flush()
        time.sleep(1)
        sys.stdout.write("1... ")
        sys.stdout.flush()
        time.sleep(1)
        sys.stdout.write("Starting!\n")
        sys.stdout.flush()
    
    last_frame_time = time.time() - frame_interval
    cur_frame = 0
    loop_desync = 0.0
    total_desync = 0.0
    next_frame_interval = frame_interval
    repeat = True
    try:
        while repeat:
            now = time.time()
            diff = now - last_frame_time
            if diff >= next_frame_interval:
                sys.stdout.write("\rFrame {frame:>6} of {count:>6}, took {diff:>5.3f}s, target {interval:>5.3f}s, loop desync {loop_desync:>8.3f}s, total desync {total_desync:>8.3f}s".format(frame=cur_frame+1, count=frame_count, diff=diff, interval=next_frame_interval, loop_desync=loop_desync, total_desync=total_desync))
                sys.stdout.flush()
                fia.send_image(frames[cur_frame])
                last_frame_time = now
                cur_frame += 1
                loop_desync += diff - frame_interval
                total_desync += diff - frame_interval
                next_frame_interval = max(0, frame_interval - loop_desync)
                if cur_frame >= len(frames):
                    cur_frame = 0
                    loop_desync = 0.0
                    if not args.loop:
                        repeat = False
    except KeyboardInterrupt:
        print("")


if __name__ == "__main__":
    main()
