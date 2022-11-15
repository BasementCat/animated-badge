# ---

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

import cv2
import numpy as np

from lib import ImageParser
from lib.formats import (
    ImageFormat,
    anim,
    qoif,
)


logger = logging.getLogger('convert')
logging.basicConfig(level='DEBUG')

SIZES = {
    'small': ((128, 128), 64),
    'medium': ((240, 320), 80),
    'large': ((320, 480), 80),
}
FORMATS = ImageFormat.get_formats()


def parse_args():
    def _parse_color(v):
        if v:
            if v.lower() in ('common', 'edge'):
                return v.lower()
            else:
                if v.lower().startswith('0x'):
                    v = v[2:]
                elif v.startswith('#'):
                    v = v[1:]

                if len(v) != 6:
                    raise ValueError("Wrong length for color")

                return (
                    int(v[:2], 16),
                    int(v[2:4], 16),
                    int(v[4:], 16)
                )
        return None

    parser = argparse.ArgumentParser(description="Convert images to a format suitable for microcontrollers")
    parser.add_argument('format', help="Output image format", choices=list(FORMATS.keys()))
    parser.add_argument('-i', '--input-dir', default='.', help="Process images in this directory")
    parser.add_argument('-o', '--output-dir', default='.', help="Output directory, existing files may be overwritten")
    parser.add_argument('-b', '--bpp', type=int, choices=(16, 24), default=16, help="Bits per pixel, should match capabilities of target display")
    parser.add_argument('-s', '--size', choices=list(SIZES.keys()), default='medium', help="Target image size - " + str(SIZES))
    parser.add_argument('-S', '--custom-size', type=int, nargs=3, metavar='WIDTH HEIGHT THUMB', help="Specify a custom width, height, and thumbnail size, overrides --size")
    parser.add_argument('-T', '--no-thumbnail', action='store_false', dest='do_thumbnail', help="Don't generate thumbnails")
    parser.add_argument('-B', '--background-color', default='000000', type=_parse_color, help="Background color - a 24 bit hex color (6 digits, optionally starting with '0x' or '#'), or 'common' to use the most common color in the image, or 'edge' to use the most common edge color in the image")
    parser.add_argument('-f', '--filenames', nargs='*', help="Image/GIF filenames to extract")
    parser.add_argument('-F', '--format-args', nargs=2, action='append', help="Additional key/value arguments per format, probably for debugging")
    args = parser.parse_args()

    if args.custom_size:
        if any(*(v < 1 for v in args.custom_size)):
            die("All arguments to --custom-size must be >0")

    return args


def get_filenames(args):
    def _f_is_img(warn=True):
        def _f_is_img_impl(fn):
            type_, _ = mimetypes.guess_type(fn, strict=False)
            if not type_ or not type_.startswith('image/'):
                if warn:
                    logger.warning("%s does not appear to be an image, skipping", fn)
                return False
            return True
        return _f_is_img_impl

    if args.filenames:
        filenames = list(filter(_f_is_img(), args.filenames))
    else:
        filenames = list(filter(_f_is_img(False), glob.glob(os.path.join(args.input_dir, '*'))))


    if not filenames:
        die("No files to convert")

    return filenames


def die(msg, code=1):
    sys.stderr.write(msg + '\n')
    sys.exit(code)


if __name__ == '__main__':
    args = parse_args()
    filenames = get_filenames(args)
    conv_cls = FORMATS[args.format]['writer']
    if args.custom_size:
        w, h, t = args.custom_size
    else:
        (w, h), t = SIZES[args.size]

    for filename in filenames:
        img = ImageParser(args, filename, (w, h, t))
        converter = conv_cls(args, img)

        fn = os.path.join(args.output_dir, os.path.splitext(os.path.basename(filename))[0] + '.' + conv_cls.EXT)
        try:
            with open(fn, 'wb') as fp:
                for chunk in converter:
                    written = 0
                    while written < len(chunk):
                        written += fp.write(chunk[written:])
        except:
            logger.error("Failed to convert %s", filename, exc_info=True)
