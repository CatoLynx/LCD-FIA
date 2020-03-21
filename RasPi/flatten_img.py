import argparse
from PIL import Image


def flatten_img(img, row_height = None):
    # if row_height is given, split the image into rows of said height
    # and flatten each row separately
    pixels = img.load()
    width, height = img.size

    if row_height:
        num_rows = height // row_height
    else:
        num_rows = 1
        row_height = height
    
    num_hp_w = width // 96
    num_hp_h = row_height // 32
    flattened_imgs = []
    flattened_row_width = num_hp_w * num_hp_h * 96
    for row_index in range(num_rows):
        y_base = row_index * row_height
        row_img = img.crop((0, y_base, width-1, y_base+row_height))
        
        out = Image.new('L', (flattened_row_width, 32))
        
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
                    out.paste(row_img.crop((x, y, x+96, y+32)).rotate(180), (x_out, 0))
                else:
                    out.paste(row_img.crop((x, y, x+96, y+32)), (x_out, 0))
                x_out -= 96
        flattened_imgs.append(out)
    
    out = Image.new('L', (flattened_row_width * len(flattened_imgs), 32))
    for i, img in enumerate(flattened_imgs):
        out.paste(img, (flattened_row_width * i, 0))
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--file', '-f', required=True, type=str)
    parser.add_argument('--output', '-o', required=False, default=None, type=str)
    parser.add_argument('--row-height', '-rh', required=False, default=None, type=int)
    args = parser.parse_args()
    img = Image.open(args.file).convert("L")
    out = flatten_img(img, args.row_height)
    
    if args.output:
        out.save(args.output)
    else:
        out.show()


if __name__ == "__main__":
    main()
