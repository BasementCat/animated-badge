import struct
import math

from PIL import Image, ImageDraw, ImageFont


class Error(Exception):
    def __init__(self, msg, *args, **kwargs):
        super().__init__(msg.format(*args, **kwargs))
class EndOfFile(Error):pass
class ShortRead(Error):pass
class FileError(Error):pass
class BadFileTypeForReader(Error):pass


class ImageFormat:
    KEY = None
    TYPE = None

    @classmethod
    def get_formats(cls):
        queue = [cls]
        out = {}
        while queue:
            c = queue.pop()
            queue += c.__subclasses__()
            if c.KEY and c.TYPE:
                out.setdefault(c.KEY, {})[c.TYPE] = c
        return out

    def __init__(self, args):
        self.args = args

    def __iter__(self):
        raise NotImplementedError()

    @staticmethod
    def pack_fmt_keys(fmt, **data):
        pack_data = [data[k] for k in fmt[1]]
        return struct.pack(fmt[0], *pack_data)

    @staticmethod
    def pack_fmt(fmt, *data):
        return struct.pack(fmt, *data)

    @staticmethod
    def color_565(pixel):
        red, green, blue = pixel
        return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3)

    @staticmethod
    def color_565_raw(pixel):
        red, green, blue = pixel
        return (
            (red & 0xF8) >> 3,
            (green & 0xFC) >> 2,
            (blue & 0xF8) >> 3,
        )

    @staticmethod
    def color_565_to_888(color):
        r = ((color >> 11) * 527 + 23) >> 6
        g = (((color >> 5) & 0b111111) * 259 + 33) >> 6
        b = ((color & 0b11111) * 527 + 23) >> 6
        return (r, g, b)

    @staticmethod
    def color_565_raw_to_888(pixel):
        r = (pixel[0] * 527 + 23) >> 6
        g = (pixel[1] * 259 + 33) >> 6
        b = (pixel[2] * 527 + 23) >> 6
        return (r, g, b)

    @staticmethod
    def read_fmt(fmt_def, fp, single=False):
        if isinstance(fmt_def, str):
            fmt = fmt_def
            keys = None
        else:
            fmt, keys = fmt_def
        slen = fp.read(struct.calcsize(fmt))
        if slen == b'':
            raise EndOfFile("Reached EOF trying to read struct {} ({}b)", repr(fmt), struct.calcsize(fmt))
        if len(slen) < struct.calcsize(fmt):
            raise ShortRead("Read {}b/{}b trying to read struct {}", len(slen), struct.calcsize(fmt), repr(fmt))
        res = struct.unpack(fmt, slen)
        if keys:
            return dict(zip(keys, res))
        if single:
            return res[0]
        return res


class ImageFormatWriter(ImageFormat):
    TYPE = 'writer'

    def __init__(self, args, image):
        super().__init__(args)
        self.image = image

    def __iter__(self):
        raise NotImplementedError("Writers must yield chunks of data to be written")


class ImageFormatReader(ImageFormat):
    TYPE = 'reader'

    def __init__(self, args, filename, fp):
        super().__init__(args)
        self.filename = filename
        self.fp = fp

    def read_header(self):
        raise NotImplementedError("Readers must produce a tuple of (width, height, bpp, flags)")

    def read_frames(self):
        raise NotImplementedError("Readers must produce an iterable of frame sets, each frame set being an iterable of (index, frames) - index is (-1, -1) for thumbnail and (s, e) for other frames where s,e are the start & end frame indexes, frames are an iterable of (w, h, duration, image)")

    def render(self):
        width, height, bpp, flags = self.read_header()
        frames = self.read_frames()
        dims = math.ceil(math.sqrt(len(frames)))

        """Calculate final image size:
            frame w/h, +1px bofder, +5px pad (all sides) for image area
            top + font h + 5px pad (top only) for meta
            font shit again x2 for name + meta
            """
        font = ImageFont.load_default()
        img_border = 1
        img_pad = 5
        text_top = 5
        _, font_h = font.getsize('ABCDEFG')

        text_line = font_h + text_top
        im_meta = text_line * 2
        block_w = width + ((img_border + img_pad) * 2)
        block_h = height + ((img_border + img_pad) * 2) + text_line

        img = Image.new('RGBA', (block_w * dims, (block_h * dims) + im_meta), (255, 255, 255, 255))
        draw = ImageDraw.Draw(img)

        # Write image meta
        draw.text((img_pad, text_top), self.filename, fill='black')
        draw.text((img_pad, text_line + text_top), f"{width} x {height} @ {bpp}bpp, flags: {flags}", fill='black')

        try:
            frames = iter(frames)
            for y in range(im_meta, img.size[1], block_h):
                for x in range(0, img.size[0], block_w):
                    idx, frame_set = next(frames)
                    frame_set = list(frame_set)
                    if idx[0] == -1:
                        w, h, d, frame = frame_set[0]
                        draw.text((x + img_pad, y + text_top), f"THUMB: {frame.size[0]} x {frame.size[1]}", fill='black')
                    else:
                        text = str(idx[0]) if idx[0] == idx[1] else f"{idx[0]}-{idx[1]}"
                        text += ': '
                        text += ', '.join((f"{w}x{h}" for w, h, _, _ in frame_set))
                        text += ', ' + str(frame_set[-1][2]) + 'ms'
                        draw.text((x + img_pad, y + text_top), text, fill='black')
                    draw.rectangle(
                        (
                            (x + img_pad, y + img_pad + text_line),
                            (x + img_pad + width, y + img_pad + text_line + height)
                        ),
                        outline=(0, 0, 0, 255),
                        width=img_border,
                        fill=(0, 0, 0, 255)
                    )
                    for w, h, dur, frame in frame_set:
                        img.paste(frame, (x + img_pad + 1, y + img_pad + text_line + 1), mask=frame)
        except StopIteration:
            pass

        img.show()
