import argparse
import json
import os

from PIL import Image, ImageOps


class LayoutRenderer:
    CHAR_MAP = {
        "ä": 0xc3a4,
        "ö": 0xc3b6,
        "ü": 0xc3bc,
        "Ü": 0xc39c,
        "ß": 0xc39f,
        "Ä": 0xc384,
        "Ö": 0xc396
    }
    
    def __init__(self, font_dir):
        self.font_dir = font_dir
        self.img_mode = 'L'
        self.img_bg = 255
    
    def get_char_filename(self, font, size, code):
        return os.path.join(self.font_dir, font, "size_{}".format(size), "{:x}.bmp".format(code))
    
    def render_character(self, img, x, y, filename):
        try:
            char_img = Image.open(filename)
        except FileNotFoundError:
            return (False, x, y)
        char_width, char_height = char_img.size
        img.paste(char_img, (x, y))
        return (True, x+char_width, y)

    def render_text(self, width, height, pad_left, pad_top, font, size, inverted, text):
        text_img = Image.new(self.img_mode, (width, height), color=self.img_bg)
        x = pad_left
        y = pad_top
        for char in text:
            if char in self.CHAR_MAP:
                code = self.CHAR_MAP[char]
            else:
                code = ord(char)
            success, x, y = self.render_character(text_img, x, y, self.get_char_filename(font, size, code))
        if inverted:
            text_img = ImageOps.invert(text_img)
        return text_img

    def render_image(self, width, height, pad_left, pad_top, value_img):
        new_img = Image.new(self.img_mode, (width, height), color=self.img_bg)
        new_img.paste(value_img, (pad_left, pad_top))
        if inverted:
            new_img = ImageOps.invert(new_img)
        return new_img

    def render_placeholder(self, img, placeholder, value):
        x = placeholder['x']
        y = placeholder['y']
        width = placeholder['width']
        height = placeholder['height']
        pad_left = placeholder['pad_left']
        pad_top = placeholder['pad_top']
        p_type = placeholder['type']
        inverted = placeholder['inverted']
        default = placeholder['default']
        
        if p_type == 'text':
            font = placeholder['font']
            size = placeholder['size']
            img.paste(self.render_text(width, height, pad_left, pad_top, font, size, inverted, value), (x, y))
        elif p_type == 'image':
            try:
                value_img = Image.open(value)
            except FileNotFoundError:
                return
            img.paste(self.render_image(width, height, pad_left, pad_top, inverted, value_img), (x, y))
    
    def render(self, layout, data):
        img = Image.new(self.img_mode, (layout['width'], layout['height']), color=self.img_bg)
        for placeholder in layout['placeholders']:
            value = data['placeholders'].get(placeholder['name'], placeholder['default'])
            self.render_placeholder(img, placeholder, value)
        return ImageOps.invert(img)


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--output', '-o', required=False, type=str)
    parser.add_argument('--layout', '-l', required=True, type=str)
    parser.add_argument('--font-dir', '-fd', required=True, type=str)
    parser.add_argument('--data', '-d', required=True, type=str)
    parser.add_argument('--rotation', '-r', required=False, type=int)
    args = parser.parse_args()
    
    renderer = LayoutRenderer(args.font_dir)
    
    with open(args.layout, 'r', encoding='utf-8') as f:
        layout = json.load(f)
    
    with open(args.data, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    img = renderer.render(layout, data)
    
    if args.rotation:
        img = img.rotate(args.rotation, expand=True)
    
    if args.output:
        img.save(args.output)
    else:
        img.show()


if __name__ == "__main__":
    main()
