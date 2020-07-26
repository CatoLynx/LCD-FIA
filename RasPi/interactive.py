import time
from PIL import Image

from fia_control import FIA

from local_settings import *

fia = FIA("/dev/ttyAMA1", (3, 0), width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)

white = Image.new('L', (DISPLAY_WIDTH, DISPLAY_HEIGHT), 'white')
black = Image.new('L', (DISPLAY_WIDTH, DISPLAY_HEIGHT), 'black')