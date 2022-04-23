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


class AnimBase:
    EXT = 'sda'

    FM_MAGIC = ('<IHH', ('magic', 'version', 'offset'))
    FM_HEADER = ('<HHBBH', ('width', 'height', 'bpp', 'reserved', 'flags'))
    FM_RGB = '<BBB'
    FM_565 = '<H'

    MAGIC = 0x676d4941

    IF_IS_ANIM = 1
    IF_HAS_THUMB = 2

    FF_BEGIN = 1
    FF_END = 128

    C_RAW = 1
    C_RLE = 2
    # C_SKIP = 3
    C_END = 255

    # def __init__(self, args, filename):
    #     self.args = args
    #     self.filename = filename


class AnimWriterBase(AnimBase, ImageFormatWriter):
    TYPE = 'writer'

    def __iter__(self):
        # File header
        yield self.pack_fmt_keys(
            self.FM_MAGIC,
            magic=self.MAGIC,
            version=self.VERSION,
            offset=struct.calcsize(self.FM_MAGIC[0]) + struct.calcsize(self.FM_HEADER[0])
        )

        # Figure out image header, produce that
        flags = 0
        if self.image.is_animated:
            flags |= self.IF_IS_ANIM
        if self.args.do_thumbnail:
            flags |= self.IF_HAS_THUMB

        yield self.pack_fmt_keys(
            self.FM_HEADER,
            width=self.image.width,
            height=self.image.height,
            bpp=self.args.bpp,
            reserved=0,
            flags=flags
        )

        if self.args.do_thumbnail:
            yield from self.process_frame(self.image.thumb)

        for diff, frame in self.image:
            yield from self.process_frame(frame, diff)

    def process_frame(self, frame, diff=None):
        if diff:
            for i, (x, y, w, h) in enumerate(diff):
                pixel_data = b''.join(self.process_frame_data(frame, x, y, w, h))

                duration = 0
                flags = 0
                if i == 0:
                    # First chunk
                    flags |= self.FF_BEGIN
                if i + 1 == len(diff):
                    # Last chunk
                    flags |= self.FF_END
                    duration = self.mk_duration(frame.duration)

                yield self.pack_fmt_keys(
                    self.FM_FRAME,
                    x=x,
                    y=y,
                    w=w,
                    h=h,
                    duration=duration,
                    flags=flags,
                    datalen=len(pixel_data)
                )

                yield pixel_data
        else:
            pixel_data = b''.join(self.process_frame_data(frame))

            yield self.pack_fmt_keys(
                self.FM_FRAME,
                x=0,
                y=0,
                w=frame.width,
                h=frame.height,
                duration=self.mk_duration(frame.duration),
                flags=0 if frame.frame_num is None else (self.FF_BEGIN | self.FF_END),
                datalen=len(pixel_data)
            )

            yield pixel_data

    def convert_pixels(self, pixels):
        for px in pixels:
            if self.args.bpp == 16:
                yield self.pack_fmt(self.FM_565, self.color_565(px))
            elif self.args.bpp == 24:
                yield self.pack_fmt(self.FM_RGB, *px)
            else:
                raise ValueError("Bad BPP", self.args.bpp)

    def process_frame_data(self, frame, x=None, y=None, w=None, h=None):
        for rle_len, chunk in frame.get_pixels_rle(self.MAX_CHUNK_SIZE, x, y, w, h):
            if rle_len:
                # RLE
                yield self.pack_fmt_keys(self.FM_CHUNK, command=self.C_RLE, datalen=rle_len)
            else:
                # Raw
                yield self.pack_fmt_keys(self.FM_CHUNK, command=self.C_RAW, datalen=len(chunk))
            yield from self.convert_pixels(chunk)

        yield self.pack_fmt_keys(self.FM_CHUNK, command=self.C_END, datalen=0)


    def mk_duration(self, duration):
        return int(duration)


class AnimReaderBase(AnimBase, ImageFormatReader):
    TYPE = 'reader'

    def __init__(self, args, filename, fp):
        super().__init__(args, filename, fp)

        logger.debug("Reading magic")
        magic = self.read_fmt(self.FM_MAGIC, self.fp)

        if magic['magic'] != self.MAGIC:
            raise BadFileTypeForReader("Magic does not match")

        if magic['version'] != self.VERSION:
            raise BadFileTypeForReader("Version does not match")

        hsize = (struct.calcsize(self.FM_MAGIC[0]) + struct.calcsize(self.FM_HEADER[0]))
        if magic['offset'] != hsize:
            raise FileError("Bad offset in header, got {}, expected {}", magic['offset'], hsize)

    def read_header(self):
        logger.debug("Reading header")
        self.header = self.read_fmt(self.FM_HEADER, self.fp)

        if not all((self.header['width'], self.header['height'])):
            raise FileError("File is missing width or height")

        if self.header['bpp'] not in (16, 24):
            raise FileError("BPP is {}, not 16 or 24", self.header['bpp'])

        if self.header['reserved']:
            raise FileError("Reserved is {}, not 0", self.header['reserved'])

        self.flags = {f: bool(getattr(self, f) & self.header['flags']) for f in ('IF_IS_ANIM', 'IF_HAS_THUMB')}
        logger.info("%s: %dx%d@%dbpp, flags: %s", self.filename, self.header['width'], self.header['height'], self.header['bpp'], self.flags)

        return (self.header['width'], self.header['height'], self.header['bpp'], self.flags)

    def _read_pixels_from_frame(self):
        # def extract_pixels(version, filename, fp, const, header, flags):
        def read_px():
            if self.header['bpp'] == 16:
                return self.color_565_to_888(self.read_fmt(self.FM_565, self.fp, single=True))
            elif self.header['bpp'] == 24:
                return self.read_fmt(self.FM_RGB, self.fp)

        while True:
            # logger.debug("Read chunk of frame")
            res = self.read_fmt(self.FM_CHUNK, self.fp)
            if res['command'] == self.C_RAW:
                # logger.debug("Reading %d raw pixels", res['datalen'])
                for _ in range(res['datalen']):
                    yield read_px()
            elif res['command'] == self.C_RLE:
                # logger.debug("Reading %d RLE pixels", res['datalen'])
                px = read_px()
                for _ in range(res['datalen']):
                    yield px
            elif res['command'] == self.C_END:
                # logger.debug("Reached end of frame")
                if res['datalen']:
                    raise FileError("Nonzero arg {} to END", res['datalen'])
                break
            else:
                raise FileError("Bad command: {}", res['command'])

    def _read_frames_from_file(self):
        # def load_frames(version, filename, fp, const, header, flags):
        while True:
            try:
                logger.debug("Read frame header")
                fheader = self.read_fmt(self.FM_FRAME, self.fp)
                if not (0 <= fheader['x'] < self.header['width']): raise FileError("Bad frame x {}", fheader['x'])
                if not (0 <= fheader['y'] < self.header['height']): raise FileError("Bad frame y {}", fheader['y'])
                if self.VERSION < 4:
                    if not (fheader['w'] == 0 and fheader['h'] == 0) or (fheader['w'] and fheader['h']):
                        raise FileError("Bad frame w/h (must both be set, or 0)")
                else:
                    if not (fheader['w'] and fheader['h']):
                        raise FileError("Bad frame w/h (must both be set)")
                if not ((0 if self.VERSION < 4 else 1) <= fheader['w'] <= self.header['width']): raise FileError("Bad frame w {}", fheader['w'])
                if not ((0 if self.VERSION < 4 else 1) <= fheader['h'] <= self.header['height']): raise FileError("Bad frame h {}", fheader['h'])
                # assert fheader['flags'] == 0, "flags set"
                initpos = self.fp.tell()

                if self.VERSION < 4:
                    # if w/h are 0, set to header w/h
                    fheader['w'] = fheader['w'] or self.header['width']
                    fheader['h'] = fheader['h'] or self.header['height']
            except EndOfFile:
                logger.debug("Reached end of frames")
                break
            else:
                logger.debug("Extracting pixels from frame")
                frame = Image.new('RGBA', (self.header['width'], self.header['height']), (0, 0, 0, 0))
                x = y = 0
                for px in self._read_pixels_from_frame():
                    frame.putpixel((x + fheader['x'], y + fheader['y']), tuple(list(px) + [255]))
                    x += 1
                    if x >= (fheader['w'] or self.header['width']):
                        x = 0
                        y += 1
                dlen = self.fp.tell() - initpos
                if dlen != fheader['datalen']:
                    raise FileError("Frame has bad datalen, expected {}, got {}", fheader['datalen'], dlen)
                yield fheader, frame

    def read_frames(self):
        logger.debug("Loading frames")
        raw_frames = list(self._read_frames_from_file())
        frames = []
        if self.flags['IF_HAS_THUMB']:
            fh, thumb = raw_frames.pop(0)
            frames.append(((-1, -1), [(fh['w'], fh['h'], 0, thumb)]))

        # Group frames
        frame_set_start = None
        frame_set = []
        for idx, (fheader, frame) in enumerate(raw_frames):
            if self.flags.get('IF_IS_ANIM'):
                if fheader['flags'] & self.FF_BEGIN:
                    frame_set_start = idx
                    frame_set = []
                frame_set.append((fheader['w'], fheader['h'], fheader['duration'], frame))
                if fheader['flags'] & self.FF_END:
                    frames.append(((frame_set_start, idx), frame_set))
            else:
                frames.append(((idx, idx), [(fheader['w'], fheader['h'], fheader['duration'], frame)]))

        return frames


class AnimV3Base:
    """
    Image format: (little endian)

    Magic header:
        4b magic - 0x676d4941
        2b version
        2b header offset (start of frames)

    Header:
        2b pixel w
        2b pixel h
        1b bpp - no palette, only 16 or 24 bpp allowed (since target is likely only 16bpp)
        1b reserved
        2b flags

        Flags:
            1: IS_ANIM, set to 1 if more than one frame exists
            2: HAS_THUMB, first frame is a thumbnail

    Frame header: appears before each frame
        2b x, 2b y - position
        1b w, 1b h - size of window - if 0, assume full image size
            if a frame updates a larger window, must be split into multiple frames
        1b duration in 10ms increments
        1b flags
        4b frame data length (excluding this header)

        Flags:
            1: BEGIN - Beginning of a "rendered" frame (as each changed block is written as an independent frame)
            128: END - End of a "rendered" frame

            BEGIN and END are only applicable when IS_ANIM is set on the image

            Frames that are themselves one "rendered" frame must set both "BEGIN" and "END".  Only the "END" frame should
            have a duration.

    Frame data:
    Image data is stored as a series of chunks, a header followed by data
    data is either 3b RGB, or 2b 5-6-5 encoded, depending on BPP

    Chunk header:
        1b command
        1b length

        Commands:
            1: RAW - raw pixel data
            2: RLE - followed by 1 "pixel", length determines how many pixels to render
            # 3: SKIP - Don't render <length> pixels, leaving them as their previous value
            255: END - End of frame
    """

    VERSION = 3

    FM_FRAME = ('<HHBBBBI', ('x', 'y', 'w', 'h', 'duration', 'flags', 'datalen'))
    FM_CHUNK = ('<BB', ('command', 'datalen'))

    MAX_CHUNK_SIZE = 255

    def mk_duration(self, duration):
        return int(duration / 10)


class AnimV4Base:
    """
    Image format: (little endian)

    Magic header:
        4b magic - 0x676d4941
        2b version
        2b header offset (start of frames)

    Header:
        2b pixel w
        2b pixel h
        1b bpp - no palette, only 16 or 24 bpp allowed (since target is likely only 16bpp)
        1b reserved
        2b flags

        Flags:
            1: IS_ANIM, set to 1 if more than one frame exists
            2: HAS_THUMB, first frame is a thumbnail

    V4 frame header:
        2b x, 2b y - position
        2b w, 2b h - size of window, must not be 0
            if a frame updates a larger window, must be split into multiple frames
        2b duration in ms
        1b flags
        4b frame data length (excluding this header)

        Flags:
            1: BEGIN - Beginning of a "rendered" frame (as each changed block is written as an independent frame)
            128: END - End of a "rendered" frame

            BEGIN and END are only applicable when IS_ANIM is set on the image

            Frames that are themselves one "rendered" frame must set both "BEGIN" and "END".  Only the "END" frame should
            have a duration.

    Frame data:
    Image data is stored as a series of chunks, a header followed by data
    data is either 3b RGB, or 2b 5-6-5 encoded, depending on BPP

    Chunk header:
        1b command
        2b length

        Commands:
            1: RAW - raw pixel data
            2: RLE - followed by 1 "pixel", length determines how many pixels to render
            # 3: SKIP - Don't render <length> pixels, leaving them as their previous value
            255: END - End of frame
    """

    VERSION = 4

    FM_FRAME = ('<HHHHHBI', ('x', 'y', 'w', 'h', 'duration', 'flags', 'datalen'))
    FM_CHUNK = ('<BH', ('command', 'datalen'))

    # This requires a lot of memory
    # MAX_CHUNK_SIZE = 65535
    MAX_CHUNK_SIZE = 5000

    def mk_duration(self, duration):
        return int(duration)


class AnimV3Writer(AnimV3Base, AnimWriterBase):
    KEY = 'anim3'


class AnimV3Reader(AnimV3Base, AnimReaderBase):
    KEY = 'anim3'


class AnimV4Writer(AnimV4Base, AnimWriterBase):
    KEY = 'anim4'


class AnimV4Reader(AnimV4Base, AnimReaderBase):
    KEY = 'anim4'
