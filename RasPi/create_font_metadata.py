import argparse
import json
import os

from PIL import Image


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--font-dir', '-fd', required=True, type=str)
    args = parser.parse_args()

    # Find all font dirs
    print("Locating font directories")
    dirs = []
    for path in os.listdir(args.font_dir):
        path = os.path.join(args.font_dir, path)
        if not os.path.isdir(path):
            continue
        size_dirs = os.listdir(path)
        for size_dir in size_dirs:
            size_dir = os.path.join(path, size_dir)
            dirs.append(size_dir)
    print(f"Found {len(dirs)} directories")

    for path in dirs:
        metadata = {
            'char_sizes': {}
        }
        print(f"Creating metadata for {path}")
        files = os.listdir(path)
        for fn in files:
            full_name = os.path.join(path, fn)
            filename, ext = os.path.splitext(fn)
            if ext != ".bmp":
                continue
            try:
                char_code = int(filename, base=16)
            except ValueError:
                continue
            with Image.open(full_name) as img:
                metadata['char_sizes'][char_code] = img.size
        with open(os.path.join(path, "metadata.json"), 'w') as f:
            json.dump(metadata, f, indent=2, sort_keys=True)


if __name__ == "__main__":
    main()
