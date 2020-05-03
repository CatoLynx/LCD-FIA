import time
from PIL import Image

from fia_control import FIA

fia = FIA("/dev/ttyAMA1", (3, 0))