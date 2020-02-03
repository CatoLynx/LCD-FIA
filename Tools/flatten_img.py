import argparse
from PIL import Image


def flatten_img(img):
    pixels = img.load()
    width, height = img.size
    
    num_hp_w = width // 96
    num_hp_h = height // 32
    
    out = Image.new('L', (num_hp_w * num_hp_h * 96, 32))
    
    x_out = out.size[0] - 96
    for yp in range(num_hp_h):
        if not yp % 2:
            r = range(num_hp_w)
            rotate = False
        else:
            r = range(num_hp_w-1, -1, -1)
            rotate = True
        
        for xp in r:
            x = width - xp * 96 - 96
            y = yp * 32
            if rotate:
                out.paste(img.crop((x, y, x+96, y+32)).rotate(180), (x_out, 0))
            else:
                out.paste(img.crop((x, y, x+96, y+32)), (x_out, 0))
            x_out -= 96
    
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--file', '-f', required=True, type=str)
    parser.add_argument('--output', '-o', required=False, default=None, type=str)
    args = parser.parse_args()
    img = Image.open(args.file).convert("L")
    out = flatten_img(img)
    
    if args.output:
        out.save(args.output)
    else:
        out.show()


if __name__ == "__main__":
    main()
