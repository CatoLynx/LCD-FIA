import argparse
import random
import time
import traceback

from fia_control import FIA
from PIL import Image, ImageDraw

from local_settings import *


class GameOfLife:
    def __init__(self, width, height, cell_size):
        self.width = width
        self.height = height
        self.cell_size = cell_size
        self.img = Image.new('L', (width, height), 0)
    
    def seed(self, img):
        if isinstance(img, Image.Image):
            self.img = img.convert('L')
        else:
            self.img = Image.open(img).convert('L')
    
    def get_neighbours(self, img, x, y):
        # Get the 8 neighbouring cells in an image
        # in the order:
        # 1 2 3
        # 4 X 5
        # 6 7 8
        w, h = img.size
        px = img.load()
        nb = [None] * 8
        cs = self.cell_size
        if x < 0 or y < 0:
            return None
        if y > 0:
            if x > 0:
                nb[0] = px[x-cs, y-cs] > 127
            nb[1] = px[x, y-cs] > 127
            if x < w - cs:
                nb[2] = px[x+cs, y-cs] > 127
        if x > 0:
            nb[3] = px[x-cs, y] > 127
            if y < h - cs:
                nb[5] = px[x-cs, y+cs] > 127
        if x < w - cs:
            nb[4] = px[x+cs, y] > 127
            if y < h - cs:
                nb[7] = px[x+cs, y+cs] > 127
        if y < h - cs:
            nb[6] = px[x, y+cs] > 127
        return nb.count(True), nb

    def evolve(self):
        # Evolves according to the rules of Conway's Game of Life
        img_out = self.img.copy()
        px_in = self.img.load()
        px_out = img_out.load()
        draw = ImageDraw.Draw(img_out)
        for x in range(0, self.width, self.cell_size):
            for y in range(0, self.height, self.cell_size):
                alive = px_in[x, y] > 127
                num_alive_nb, nb = self.get_neighbours(self.img, x, y)
                if alive and 2 <= num_alive_nb <= 3:
                    if self.cell_size == 1:
                        px_out[x, y] = 255
                    else:
                        draw.rectangle((x, y, x+self.cell_size-1, y+self.cell_size-1), fill=255)
                elif not alive and num_alive_nb == 3:
                    if self.cell_size == 1:
                        px_out[x, y] = 255
                    else:
                        draw.rectangle((x, y, x+self.cell_size-1, y+self.cell_size-1), fill=255)
                else:
                    if self.cell_size == 1:
                        px_out[x, y] = 0
                    else:
                        draw.rectangle((x, y, x+self.cell_size-1, y+self.cell_size-1), fill=0)
        self.img = img_out


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--width', '-w', required=False, type=int, default=480)
    parser.add_argument('--height', '-h', required=False, type=int, default=128)
    parser.add_argument('--cell-size', '-cs', required=False, type=int, default=1)
    parser.add_argument('--interval', '-i', required=False, type=int, default=1000)
    parser.add_argument('--seed', '-s', required=False, type=str)
    args = parser.parse_args()

    fia = FIA("/dev/ttyAMA1", (3, 0), width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)
    gol = GameOfLife(args.width, args.height, args.cell_size)
    
    if args.seed:
        gol.seed(args.seed)
    
    while True:
        fia.send_image(gol.img)
        gol.evolve()
        time.sleep(args.interval / 1000)


if __name__ == "__main__":
    main()
