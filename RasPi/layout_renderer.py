import argparse
import json
import os

from PIL import Image, ImageOps, ImageDraw


class LayoutRenderer:
    CHAR_MAP = {
        
    }
    
    def __init__(self, font_dir):
        self.font_dir = font_dir
        self.img_mode = 'L'
        self.img_bg = 255
        self.img_fg = 0
    
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

    def render_text(self, width, height, pad_left, pad_top, font, size, align, inverted, spacing, text):
        text_img = Image.new(self.img_mode, (width, height), color=self.img_bg)
        x = pad_left
        y = pad_top
        for char in text:
            if char in self.CHAR_MAP:
                code = self.CHAR_MAP[char]
            else:
                code = ord(char)
            success, x, y = self.render_character(text_img, x, y, self.get_char_filename(font, size, code))
            x += spacing
        if align in ('center', 'right'):
            bbox = ImageOps.invert(text_img).getbbox()
            if bbox is not None:
                cropped = text_img.crop((bbox[0], 0, bbox[2], height-1))
                cropped_width = cropped.size[0]
                text_img = Image.new(self.img_mode, (width, height), color=self.img_bg)
                if align == 'center':
                    x_offset = (width - cropped_width) // 2
                elif align == 'right':
                    x_offset = width - cropped_width
                text_img.paste(cropped, (x_offset, 0))
        if inverted:
            text_img = ImageOps.invert(text_img)
        return text_img

    def render_image(self, width, height, pad_left, pad_top, value_img):
        new_img = Image.new(self.img_mode, (width, height), color=self.img_bg)
        new_img.paste(value_img, (pad_left, pad_top))
        if inverted:
            new_img = ImageOps.invert(new_img)
        return new_img

    def render_placeholder(self, img, placeholder, value, render_boxes, render_content):
        x = placeholder.get('x')
        y = placeholder.get('y')
        width = placeholder.get('width')
        height = placeholder.get('height')
        pad_left = placeholder.get('pad_left')
        pad_top = placeholder.get('pad_top')
        p_type = placeholder.get('type')
        inverted = placeholder.get('inverted')
        default = placeholder.get('default')
        
        if render_content and p_type == 'text':
            if not value:
                return
            font = placeholder.get('font')
            size = placeholder.get('size')
            align = placeholder.get('align')
            spacing = placeholder.get('spacing', 0)
            img.paste(self.render_text(width, height, pad_left, pad_top, font, size, align, inverted, spacing, value), (x, y))
        elif render_content and p_type == 'image':
            if not value:
                return
            try:
                value_img = Image.open(value)
            except FileNotFoundError:
                return
            img.paste(self.render_image(width, height, pad_left, pad_top, inverted, value_img), (x, y))
        elif p_type == 'line':
            x2 = placeholder.get('x2', 0)
            y2 = placeholder.get('y2', 0)
            line_width = placeholder.get('line_width', 1)
            draw = ImageDraw.Draw(img)
            color = self.img_bg if inverted else self.img_fg
            draw.line((x, y, x2, y2), fill=color, width=line_width)
        elif p_type == 'rectangle':
            fill = placeholder.get('fill')
            draw = ImageDraw.Draw(img)
            color = self.img_bg if inverted else self.img_fg
            draw.rectangle((x, y, x+width-1, y+height-1), outline=color, fill=color if fill else None)
        
        if render_boxes:
            draw = ImageDraw.Draw(img)
            draw.rectangle((x, y, x+width-1, y+height-1), outline=self.img_fg)
    
    def render(self, layout, data, render_boxes = False, render_content = True):
        img = Image.new(self.img_mode, (layout['width'], layout['height']), color=self.img_bg)
        if render_content:
            for placeholder in layout['placeholders']:
                value = data['placeholders'].get(placeholder['name'], placeholder.get('default'))
                self.render_placeholder(img, placeholder, value, render_boxes=False, render_content=True)
        if render_boxes:
            for placeholder in layout['placeholders']:
                self.render_placeholder(img, placeholder, value=None, render_boxes=True, render_content=False)
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
