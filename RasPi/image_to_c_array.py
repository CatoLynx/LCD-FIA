import argparse
import math
import os
import time

from PIL import Image
from fia_control import FIA
from local_settings import *


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--image', type=str, required=True, help="The image to convert")
    parser.add_argument('-o', '--output', type=str, required=False, help="The name of the output file. Default: same as image filename")
    parser.add_argument('-n', '--name', type=str, required=False, help="The name of the image buffer in the generated C file. Default: <image_filename> with only ASCII characters")
    args = parser.parse_args()
    
    input_basename = os.path.splitext(os.path.basename(args.image))[0]
    
    if args.output:
        output_name = args.output
    else:
        output_name = input_basename
    
    header_name = output_name + ".h"
    source_name = output_name + ".c"
    
    allowed_characters = list(map(chr, range(ord('0'), ord('9')+1)))
    allowed_characters += list(map(chr, range(ord('a'), ord('z')+1)))
    allowed_characters += list(map(chr, range(ord('A'), ord('Z')+1)))
    allowed_characters += ["_"]
    if args.name:
        buffer_name = ''.join([i if i in allowed_characters else '_' for i in args.name])
    else:
        buffer_name = ''.join([i if i in allowed_characters else '_' for i in input_basename])
    
    fia = FIA("/dev/ttyAMA1", (3, 0), width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)
    img = Image.open(args.image).convert('L')
    array, width, height = fia.img_to_array(img)
    buffer_size = len(array)
    
    buffer_data = ""
    num_cols = math.ceil(height / 8)
    
    for i in range(0, buffer_size, num_cols):
        buffer_data += "    "
        buffer_data += ", ".join([f"0x{byte:02x}" for byte in array[i:i+num_cols]])
        if i + num_cols < buffer_size:
            buffer_data += ",\n"
    
    buffer_size_def_name = buffer_name.upper() + "_SIZE"
    header_template = f"""#pragma once\n\n#include <stdint.h>\n\n#define {buffer_size_def_name} {buffer_size}\nconst uint8_t {buffer_name}[{buffer_size_def_name}];\n"""
    source_template = f"""#include "{header_name}"\n\nconst uint8_t {buffer_name}[{buffer_size_def_name}] = {{\n{buffer_data}\n}};\n"""
    
    with open(header_name, 'w') as f:
        f.write(header_template)
    
    with open(source_name, 'w') as f:
        f.write(source_template)


if __name__ == "__main__":
    main()


