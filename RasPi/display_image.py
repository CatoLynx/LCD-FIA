import argparse
import datetime
import math
import sys
import time
from PIL import Image, ImageSequence

from fia_control import FIA, FIAEmulator

from local_settings import *


def pos_nonzero_int(value):
    try:
        ivalue = int(value)
    except ValueError:
        raise argparse.ArgumentTypeError("{} must be a positive non-zero integer".format(value))
    if ivalue <= 0:
        raise argparse.ArgumentTypeError("{} must be a positive non-zero integer".format(value))
    return ivalue

def pos_nonzero_int_or_neg1(value):
    try:
        ivalue = int(value)
    except ValueError:
        raise argparse.ArgumentTypeError("{} must be a positive non-zero integer or -1".format(value))
    if ivalue <= 0 and ivalue != -1:
        raise argparse.ArgumentTypeError("{} must be a positive non-zero integer or -1".format(value))
    return ivalue


def display_image(fia, filename, width = None, height = None, interval = None, countdown = False, loop_count = 1, output = False):
    def _print(*args, **kwargs):
        if output:
            print(*args, **kwargs)

    _print("Loading image...")
    img = Image.open(filename)
    in_width, in_height = img.size
    if interval is not None:
        frame_interval = interval
    else:
        frame_interval = img.info.get('duration', 1000)
    frame_interval /= 1000
    
    _print("Loading frames...")
    frames = []
    if width and height:
        for frame in ImageSequence.Iterator(img):
            temp = Image.new('L', (width, height))
            temp.paste(frame, (0, 0))
            frames.append(temp)
    else:
        for frame in ImageSequence.Iterator(img):
            frames.append(frame.convert('L'))
    frame_count = len(frames)
    
    duration = datetime.timedelta(seconds=math.ceil(frame_count * frame_interval))
    _print("Input: {width}x{height}, {fps:.2f} fps, {count} frames, duration {duration}".format(width=in_width, height=in_height, fps=1/frame_interval, count=frame_count, duration=duration))
    
    if countdown:
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
    loop_dropped = 0
    total_dropped = 0
    loop_desync = 0.0
    total_desync = 0.0
    next_frame_interval = frame_interval
    repeat = True
    i_loop = 0
    while repeat:
        now = time.time()
        diff = now - last_frame_time
        if diff >= next_frame_interval:
            timestamp = datetime.timedelta(milliseconds=cur_frame * frame_interval * 1000)
            timestamp -= datetime.timedelta(microseconds=timestamp.microseconds)
            if output:
                sys.stdout.write("\rFrame {frame:>6} of {count:>6} ({timestamp}), took {diff:>5.3f}s, target {interval:>5.3f}s, loop desync {loop_desync:>8.3f}s (dropped: {loop_dropped:>6}), total desync {total_desync:>8.3f}s (dropped: {total_dropped:>6})".format(frame=cur_frame+1, timestamp=timestamp, count=frame_count, diff=diff, interval=next_frame_interval, loop_desync=loop_desync, loop_dropped=loop_dropped, total_desync=total_desync, total_dropped=total_dropped))
                sys.stdout.flush()
            fia.send_image(frames[cur_frame])
            last_frame_time = now
            cur_frame += 1
            loop_desync += diff - frame_interval
            total_desync += diff - frame_interval
            if loop_desync >= frame_interval:
                # Drop a frame when we are desynced by more than one frame
                cur_frame += 1
                loop_desync -= frame_interval
                total_desync -= frame_interval
                loop_dropped += 1
                total_dropped += 1
            next_frame_interval = max(0, frame_interval - loop_desync)
            if cur_frame >= len(frames):
                i_loop += 1
                cur_frame = 0
                loop_desync = 0.0
                loop_dropped = 0
                if i_loop >= loop_count and loop_count != -1:
                    repeat = False


def main():
    parser = argparse.ArgumentParser(description="This script can show an image or animation on the display.", add_help=False)
    parser.add_argument('--file', '-f', required=True, type=str, help="Input file to display (static image or GIF)")
    parser.add_argument('--width', '-w', required=False, default=None, type=pos_nonzero_int, help="Width to crop input to")
    parser.add_argument('--height', '-h', required=False, default=None, type=pos_nonzero_int, help="Height to crop input to")
    parser.add_argument('--interval', '-i', required=False, default=None, type=pos_nonzero_int, help="Interval between frames in ms (defaults to GIF's setting or one second)")
    parser.add_argument('--countdown', '-c', action='store_true', help="If set, do an interactive countdown before starting to help with manually syncing video and audio")
    parser.add_argument('--loop-count', '-lc', required=False, default=1, type=pos_nonzero_int_or_neg1, help="Number of loops to run. Defaults to 1. Positive integer or -1 for infinite loop.")
    parser.add_argument('--emulate', '-e', action='store_true', help="Run in emulation mode")
    parser.add_argument('--help', action='help', help="Display this help message")
    args = parser.parse_args()
    
    if args.emulate:
        fia = FIAEmulator(width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)
    else:
        fia = FIA("/dev/ttyAMA1", (3, 0), width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)

    try:
        display_image(fia, args.file, width=args.width, height=args.height, interval=args.interval, countdown=args.countdown, loop_count=args.loop_count, output=True)
    except KeyboardInterrupt:
        print("")
        fia.exit()


if __name__ == "__main__":
    main()
