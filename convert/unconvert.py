import argparse
import logging
import os
import sys
import shutil
import struct
import itertools
import glob
import mimetypes
import math

from PIL import Image, ImageDraw, ImageFont

from lib.formats import (
    BadFileTypeForReader,
    ImageFormat,
    anim,
    qoif,
)


logger = logging.getLogger('unconvert')
logging.basicConfig(level='DEBUG')


def parse_args():
    parser = argparse.ArgumentParser(description="Unconvert images for debugging")
    parser.add_argument('filename')
    return parser.parse_args()


def die(msg, code=1):
    sys.stderr.write(msg + '\n')
    sys.exit(code)


if __name__ == '__main__':
    args = parse_args()

    with open(args.filename, 'rb') as fp:
        for type_, clses in ImageFormat.get_formats().items():
            fp.seek(0)
            try:
                img = clses['reader'](args, args.filename, fp)
            except BadFileTypeForReader:
                continue
            else:
                img.render()
                sys.exit(0)

    die("No reader found for " + args.filename)
