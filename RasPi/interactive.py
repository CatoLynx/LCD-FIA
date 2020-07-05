import time
from PIL import Image

from fia_control import FIA

fia = FIA("/dev/ttyAMA1", (3, 0))

white = Image.new('L', (480, 128), 'white')
black = Image.new('L', (480, 128), 'black')