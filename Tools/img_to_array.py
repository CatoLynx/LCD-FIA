import argparse
from PIL import Image


def img_to_array(img):
    pixels = img.load()
    width, height = img.size

    output = []

    for x in range(width):
        for y_byte in range(0, height, 8):
            byte = 0x00
            for y_bit in range(8):
                y = y_byte + y_bit
                state = pixels[x, y] > 64
                if state:
                    byte |= (1 << y_bit)
            output.append(byte)
    
    return output, width, height


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--file', '-f', required=True, type=str)
    args = parser.parse_args()
    img = Image.open(args.file).convert("L")
    array, width, height = img_to_array(img)

    for i in range(0, width*height//8, 16):
        print(", ".join(map(lambda v: "0x{:02X}".format(v), array[i:i+16])) + ",")


if __name__ == "__main__":
    main()
