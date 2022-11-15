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

from PIL import Image

from . import (
    EndOfFile,
    ImageFormat,
    ImageFormatWriter,
    ImageFormatReader,
    BadFileTypeForReader,
)


logger = logging.getLogger(__name__)


class QOIFBase:
    """\
    Documentation: https://qoiformat.org/ - in particular, https://qoiformat.org/qoi-specification.pdf
    """

    KEY = 'qoif'
    EXT = 'qoi'

    FM_HEADER = ('<IIIBB', ('magic', 'width', 'height', 'channels', 'colorspace'))

    MAGIC = struct.unpack('<I', b'qoif')[0]

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.prev_px = (0, 0, 0, 255)
        self.prev_cache = [(0, 0, 0, 0) for _ in range(64)]

    @staticmethod
    def _mk_cache_key(px):
        return (
            (px[0] * 3)
            + (px[1] * 5)
            + (px[2] * 7)
            + (px[3] *11)
        ) % 64

    def _set_cache(self, px):
        self.prev_cache[self._mk_cache_key(px)] = px

    def _get_cache(self, px):
        k = self._mk_cache_key(px)
        if self.prev_cache[k] == px:
            return k
        return False

    def _get_cache_idx(self, idx):
        return self.prev_cache[idx]


class QOIFWriter(QOIFBase, ImageFormatWriter):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def __iter__(self):
        yield self.pack_fmt_keys(
            self.FM_HEADER,
            magic=self.MAGIC,
            width=self.image.width,
            height=self.image.height,
            channels=3,
            colorspace=1,
        )

        for diff, frame in self.image:
            yield from self.process_frame(frame)
            break  # Animation is not supported, ignore later frames

        # trailer
        yield b'\x00\x00\x00\x00\x00\x00\x00\x01'

    def _calc_op_diff(self, px):
        diff = tuple((px[i] - self.prev_px[i] for i in range(4)))
        if diff[3] or any((v < -2 or v > 1 for v in diff)):
            return False
        return diff

    def _calc_op_luma(self, px):
        diff = tuple((px[i] - self.prev_px[i] for i in range(4)))
        if diff[3] or (diff[1] < -32 or diff[1] > 31):
            return False
        diff_g = diff[1]
        diff_r = diff[0] - diff_g
        diff_b = diff[2] - diff_g
        if diff_r < -8 or diff_r > 7 or diff_b < -8 or diff_b > 7:
            return False
        return (diff_g, diff_r, diff_b)

    def _get_op(self, px):
        idx = self._get_cache(px)
        if idx is not False:
            return struct.pack('<B', idx)

        chan_diff = self._calc_op_diff(px)
        if chan_diff is not False:
            return struct.pack(
                '<B',
                0b01000000
                | ((chan_diff[0] + 2) << 4)
                | ((chan_diff[1] + 2) << 2)
                | (chan_diff[2] + 2)
            )

        luma_diff = self._calc_op_luma(px)
        if luma_diff is not False:
            return struct.pack(
                '<BB',
                0b10000000 | (luma_diff[0] + 32),
                ((luma_diff[1] + 8) << 4)
                | (luma_diff[2] + 8)
            )

        # Alpha is not supported, only using RGB op
        return struct.pack(
            '<BBBB',
            0b11111110,
            px[0],
            px[1],
            px[2]
        )

    def process_frame(self, frame):
        for rle_len, pixels in frame.get_pixels_rle(63, only_chunk_rle=True):
            if rle_len > 1:
                px = tuple(list(pixels[0]) + [255])
                yield self._get_op(px)
                rle_len -= 1
                yield struct.pack(
                    '<B',
                    0b11000000
                    | (rle_len - 1)
                )
                self._set_cache(px)
                self.prev_px = px
            else:
                for px in pixels:
                    px = tuple(list(px) + [255])
                    yield self._get_op(px)
                    self._set_cache(px)
                    self.prev_px = px


class QOIFReader(QOIFBase, ImageFormatReader):
    def __init__(self, args, filename, fp):
        super().__init__(args, filename, fp)

        self.header = self.read_fmt(self.FM_HEADER, self.fp)
        if self.header['magic'] != self.MAGIC:
            raise BadFileTypeForReader("Magic does not match")

        self.bpp = self.header['channels'] * 8

    def read_header(self):
        return (self.header['width'], self.header['height'], self.bpp, {})

    def _read_pixels_from_frame(self):
        while True:
            px = None
            run = 1
            tag = struct.unpack('<B', self.fp.read(1))[0]
            if tag == 0b11111110:
                px = tuple(list(struct.unpack('<BBB', self.fp.read(3))) + [255])
            elif tag == 0b11111111:
                px = tuple(struct.unpack('<BBBB'), self.fp.read(4))
            else:
                arg = tag & 0b00111111
                tag = tag >> 6
                if tag == 0:
                    px = self._get_cache_idx(arg)
                elif tag == 1:
                    diff = (
                        (arg >> 4) - 2,
                        ((arg >> 2) & 3) - 2,
                        (arg & 3) - 2,
                    )
                    px = (
                        self.prev_px[0] + diff[0],
                        self.prev_px[1] + diff[1],
                        self.prev_px[2] + diff[2],
                        self.prev_px[3]
                    )
                elif tag == 2:
                    diff_g = arg - 32
                    diff_rb = struct.unpack('<B', self.fp.read(1))[0]
                    diff_r = diff_g + ((diff_rb >> 4) - 8)
                    diff_b = diff_g + ((diff_rb & 15) - 8)
                    px = (
                        self.prev_px[0] + diff_r,
                        self.prev_px[1] + diff_g,
                        self.prev_px[2] + diff_b,
                        self.prev_px[3]
                    )
                elif tag == 3:
                    px = self.prev_px
                    run = arg + 1

            if px:
                for _ in range(run):
                    yield px
                self._set_cache(px)
                self.prev_px = px

    def read_frames(self):
        frame = Image.new('RGBA', (self.header['width'], self.header['height']), (0, 0, 0, 0))
        pixels = iter(self._read_pixels_from_frame())
        for y in range(self.header['height']):
            for x in range(self.header['width']):
                frame.putpixel((x, y), next(pixels))

        return [((0, 0), [(self.header['width'], self.header['height'], 0, frame)])]


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

from PIL import Image

from . import (
    EndOfFile,
    ImageFormat,
    ImageFormatWriter,
    ImageFormatReader,
    BadFileTypeForReader,
)


logger = logging.getLogger(__name__)


class QOIF2Base:
    """\
    QOIF, but with animation!

    Differences from QOIF:

      * Magic string is "qoiF"
      * Header has an additional field at the end:
        * 1b version (must be 2)
      * Channels may be 2, for 16 bit 5-6-5 RGB encoding (in this case, alpha is not supported, so the rgba op tag must not be present)
        * In the case of 2 "channels", the rgb op tag is followed by 2b of rgb565
        * To calculate the index in the cache, multiply the 16 bit color by 6311 (0b0001100010100111) & modulo 64
      * Each block of image data is preceded by 2 headers:
        * Header 1 - common
          * 1b flags:
            * F_THUMB: 1 - This block is a thumbnail, for normal rendering this block is skipped (implies F_START and F_END, and should not have a duration)
            * F_START: 2 - This block is the start of a displayed frame
            * F_END: 4 - This block is the end of a displayed frame, this is the only case when a duration should be set
            * F_BIG: 8 - The second header is the "big" version, supporting larger pixel sizes
          * 2b duration, in ms
          * 4b datalen, length of block data (excluding headers)
        * Header 2 - differs depending on the dimensions of the block (a larger version is required to support the maximum dimensions as per the header, the smaller version is suitable for small images)
            * This header has 4 fields - width, height, x, and y positions
            * If F_BIG is not set, all fields are 2b
            * Otherwise, all fields are 4b (required if any dimension or position is >65535 px)

    Image data is otherwise stored identically to QOIF, except as described above for 16 bit color
    """

    KEY = 'qoif2'
    EXT = 'qox'

    FM_HEADER = ('<IIIBBB', ('magic', 'width', 'height', 'channels', 'colorspace', 'version'))
    FM_BLOCK1 = ('<BHI', ('flags', 'duration', 'datalen'))
    FM_BLOCK2 = ('<HHHH', ('width', 'height', 'x', 'y'))
    FM_BLOCK2_BIG = ('<IIII', ('width', 'height', 'x', 'y'))

    F_THUMB = 1
    F_START = 2
    F_END = 4
    F_BIG = 8

    MAGIC = struct.unpack('<I', b'qoiF')[0]
    VERSION = 2
    TRAILER = b'\x00\x00\x00\x00\x00\x00\x00\x01'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def setup(self):
        if self.bpp < 24:
            self.prev_px = 0
            self.prev_cache = [0 for _ in range(64)]
        else:
            self.prev_px = (0, 0, 0, 255)
            self.prev_cache = [(0, 0, 0, 0) for _ in range(64)]

    def _mk_cache_key(self, px):
        if self.bpp < 24:
            return (px * 6311) % 64
        return ((px[0] * 3) + (px[1] * 5) + (px[2] * 7) + (px[3] *11)) % 64

    def _set_cache(self, px):
        self.prev_cache[self._mk_cache_key(px)] = px

    def _get_cache(self, px):
        k = self._mk_cache_key(px)
        if self.prev_cache[k] == px:
            return k
        return False

    def _get_cache_idx(self, idx):
        return self.prev_cache[idx]


class QOIF2Writer(QOIF2Base, ImageFormatWriter):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.bpp = self.args.bpp
        self.exclude_tags = []
        if self.args.format_args:
            for k, v in self.args.format_args:
                if k == 'notags':
                    self.exclude_tags = v.split(',')
        self.setup()

    def __iter__(self):
        yield self.pack_fmt_keys(
            self.FM_HEADER,
            magic=self.MAGIC,
            width=self.image.width,
            height=self.image.height,
            channels=int(self.args.bpp / 8),
            colorspace=1,
            version=self.VERSION,
        )

        for diff, frame in self.image:
            yield from self.process_frame(diff, frame)

        # trailer
        yield self.TRAILER

    def _unpack_px(self, px):
        if self.bpp < 24:
            return (
                px >> 11,
                (px >> 5) & 0b111111,
                px & 0b11111,
                255
            )
        return px

    def _calc_op_diff(self, px):
        px = self._unpack_px(px)
        pv = self._unpack_px(self.prev_px)
        diff = tuple((px[i] - pv[i] for i in range(4)))
        if diff[3] or any((v < -2 or v > 1 for v in diff)):
            return False
        return diff

    def _calc_op_luma(self, px):
        px = self._unpack_px(px)
        pv = self._unpack_px(self.prev_px)
        diff = tuple((px[i] - pv[i] for i in range(4)))
        if diff[3] or (diff[1] < -32 or diff[1] > 31):
            return False
        diff_g = diff[1]
        diff_r = diff[0] - diff_g
        diff_b = diff[2] - diff_g
        if diff_r < -8 or diff_r > 7 or diff_b < -8 or diff_b > 7:
            return False
        return (diff_g, diff_r, diff_b)

    def _get_op(self, px):
        if 'index' not in self.exclude_tags:
            idx = self._get_cache(px)
            if idx is not False:
                return struct.pack('<B', idx)

        if 'diff' not in self.exclude_tags:
            chan_diff = self._calc_op_diff(px)
            if chan_diff is not False:
                return struct.pack(
                    '<B',
                    0b01000000
                    | ((chan_diff[0] + 2) << 4)
                    | ((chan_diff[1] + 2) << 2)
                    | (chan_diff[2] + 2)
                )

        if 'luma' not in self.exclude_tags:
            luma_diff = self._calc_op_luma(px)
            if luma_diff is not False:
                return struct.pack(
                    '<BB',
                    0b10000000 | (luma_diff[0] + 32),
                    ((luma_diff[1] + 8) << 4)
                    | (luma_diff[2] + 8)
                )

        if self.args.bpp == 32:
            return struct.pack(
                '<BBBBB',
                0b11111111,
                *px
            )
        elif self.args.bpp == 24:
            return struct.pack(
                '<BBBB',
                0b11111110,
                *px[:3]
            )
        elif self.args.bpp == 16:
            return struct.pack(
                '<BH',
                0b11111110,
                px
            )

    def process_frame_data(self, frame, x=None, y=None, w=None, h=None):
        for rle_len, pixels in frame.get_pixels_rle(63, x, y, w, h, only_chunk_rle=True):
            if rle_len > 1 and 'run' in self.exclude_tags:
                pixels = [pixels[0] for _ in range(rle_len)]
                rle_len = 1

            if rle_len > 1:
                px = pixels[0]
                if self.args.bpp < 24:
                    px = self.color_565(px)
                else:
                    px = tuple(list(px) + [255])
                yield self._get_op(px)
                rle_len -= 1
                yield struct.pack(
                    '<B',
                    0b11000000
                    | (rle_len - 1)
                )
                self._set_cache(px)
                self.prev_px = px
            else:
                for px in pixels:
                    if self.args.bpp < 24:
                        px = self.color_565(px)
                    else:
                        px = tuple(list(px) + [255])
                    yield self._get_op(px)
                    self._set_cache(px)
                    self.prev_px = px

    def process_frame(self, diff, frame):
        diff = diff or [(None, None, None, None)]
        for i, (x, y, w, h) in enumerate(diff):
            pixel_data = b''.join(self.process_frame_data(frame, x, y, w, h))

            duration = 0
            flags = 0
            if i == 0:
                # First chunk
                flags |= self.F_START
            if i + 1 == len(diff):
                # Last chunk
                flags |= self.F_END
                duration = frame.duration

            bh2 = self.FM_BLOCK2
            for v in (x or 0, y or 0, w or self.image.width, h or self.image.height):
                if v > 65535:
                    bh2 = self.FM_BLOCK2_BIG
                    flags |= self.F_BIG
                    break

            yield self.pack_fmt_keys(
                self.FM_BLOCK1,
                flags=flags,
                duration=duration,
                datalen=len(pixel_data),
            )

            yield self.pack_fmt_keys(
                bh2,
                width=self.image.width if w is None else w,
                height=self.image.height if h is None else h,
                x=0 if x is None else x,
                y=0 if y is None else y,
            )

            yield pixel_data


class QOIF2Reader(QOIF2Base, ImageFormatReader):
    def __init__(self, args, filename, fp):
        super().__init__(args, filename, fp)

        self.header = self.read_fmt(self.FM_HEADER, self.fp)
        if self.header['magic'] != self.MAGIC:
            raise BadFileTypeForReader("Magic does not match")

        if self.header['version'] != self.VERSION:
            raise BadFileTypeForReader("Version does not match")

        self.bpp = self.header['channels'] * 8
        self.setup()

    def read_header(self):
        return (self.header['width'], self.header['height'], self.bpp, {})

    def _read_pixels_from_frame(self, datalen):
        read = 0
        while read < datalen:
            px = None
            run = 1
            tag = struct.unpack('<B', self.fp.read(1))[0]
            read += 1
            if tag == 0b11111110:
                if self.bpp < 24:
                    px = struct.unpack('<H', self.fp.read(2))[0]
                    read += 2
                else:
                    px = tuple(list(struct.unpack('<BBB', self.fp.read(3))) + [255])
                    read += 3
            elif tag == 0b11111111:
                px = tuple(struct.unpack('<BBBB'), self.fp.read(4))
                read += 4
            else:
                arg = tag & 0b00111111
                tag = tag >> 6
                if tag == 0:
                    # index
                    px = self._get_cache_idx(arg)
                elif tag == 1:
                    # diff
                    diff = (
                        (arg >> 4) - 2,
                        ((arg >> 2) & 3) - 2,
                        (arg & 3) - 2,
                    )
                    if self.bpp < 24:
                        px = self.prev_px
                        px += diff[0] << 11
                        px += diff[1] << 5
                        px += diff[2]
                    else:
                        px = (
                            self.prev_px[0] + diff[0],
                            self.prev_px[1] + diff[1],
                            self.prev_px[2] + diff[2],
                            self.prev_px[3]
                        )
                elif tag == 2:
                    # luma
                    diff_g = arg - 32
                    diff_rb = struct.unpack('<B', self.fp.read(1))[0]
                    read += 1
                    diff_r = diff_g + ((diff_rb >> 4) - 8)
                    diff_b = diff_g + ((diff_rb & 15) - 8)
                    if self.bpp < 24:
                        px = self.prev_px
                        px += diff_r << 11
                        px += diff_g << 5
                        px += diff_b
                    else:
                        px = (
                            self.prev_px[0] + diff_r,
                            self.prev_px[1] + diff_g,
                            self.prev_px[2] + diff_b,
                            self.prev_px[3]
                        )
                elif tag == 3:
                    # run
                    px = self.prev_px
                    run = arg + 1

            if px is not None:
                for _ in range(run):
                    yield px
                self._set_cache(px)
                self.prev_px = px

    def read_block(self):
        bh = self.read_fmt(self.FM_BLOCK1, self.fp)
        bh['flags'] = {f: bool(bh['flags'] & getattr(self, f)) for f in ('F_START', 'F_END', 'F_THUMB', 'F_BIG')}
        logger.debug("Read bh1: %s", bh)
        bh.update(self.read_fmt(self.FM_BLOCK2_BIG if bh['flags']['F_BIG'] else self.FM_BLOCK2, self.fp))
        logger.debug("Read bh2: %s", bh)
        block = Image.new('RGBA', (self.header['width'], self.header['height']), (0, 0, 0, 0))
        pixels = iter(self._read_pixels_from_frame(bh['datalen']))
        for y in range(bh['height']):
            for x in range(bh['width']):
                px = next(pixels)
                if self.bpp < 24:
                    px = self.color_565_to_888(px)
                block.putpixel((x + bh['x'], y + bh['y']), px)
        return bh, block

    def read_frames(self):
        # frame = Image.new('RGBA', (self.header['width'], self.header['height']), (0, 0, 0, 0))
        # pixels = iter(self._read_pixels_from_frame())
        # for y in range(self.header['height']):
        #     for x in range(self.header['width']):
        #         frame.putpixel((x, y), next(pixels))

        # return [((0, 0), [(self.header['width'], self.header['height'], 0, frame)])]
        raw_frames = []
        logger.debug("Read frames")
        while True:
            if self.fp.read(8) == self.TRAILER:
                logger.debug("Read trailer, end read frames")
                break
            self.fp.seek(self.fp.tell() - 8)
            raw_frames.append(self.read_block())

        frames = []
        # if self.flags['IF_HAS_THUMB']:
        #     fh, thumb = raw_frames.pop(0)
        #     frames.append(((-1, -1), [(fh['w'], fh['h'], 0, thumb)]))

        # Group frames
        frame_set_start = None
        frame_set = []
        for idx, (bh, block) in enumerate(raw_frames):
            if bh['flags']['F_START']:
                frame_set_start = idx
                frame_set = []
            frame_set.append((bh['width'], bh['height'], bh['duration'], block))
            if bh['flags']['F_END']:
                frames.append(((frame_set_start, idx), frame_set))

        return frames
