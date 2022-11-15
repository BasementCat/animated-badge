import logging
import itertools
import math

import cv2
import numpy as np
from PIL import Image


logger = logging.getLogger(__name__)


def diff_images(f1, f2):
    # https://stackoverflow.com/a/53652807
    scale = 0.25
    scale_up = 1 / scale

    # Function to fill all the bounding box
    def fill_rects(image, stats):

        for i,stat in enumerate(stats):
            if i > 0:
                p1 = (stat[0],stat[1])
                p2 = (stat[0] + stat[2],stat[1] + stat[3])
                cv2.rectangle(image,p1,p2,255,-1)


    # Load image file
    img1 = cv2.cvtColor(np.array(f1), cv2.COLOR_BGR2GRAY)
    img2 = cv2.cvtColor(np.array(f2), cv2.COLOR_BGR2GRAY)

    # Subtract the 2 image to get the difference region
    img3 = cv2.subtract(img1,img2)

    # Make it smaller to speed up everything and easier to cluster
    small_img = cv2.resize(img3,(0,0),fx = scale, fy = scale)


    # Morphological close process to cluster nearby objects
    fat_img = cv2.dilate(small_img, None,iterations = 3)
    fat_img = cv2.erode(fat_img, None,iterations = 3)

    fat_img = cv2.dilate(fat_img, None,iterations = 3)
    fat_img = cv2.erode(fat_img, None,iterations = 3)

    # Threshold strong signals
    # _, bin_img = cv2.threshold(fat_img,20,255,cv2.THRESH_BINARY)
    _, bin_img = cv2.threshold(fat_img,0,255,cv2.THRESH_BINARY)

    # Analyse connected components
    num_labels, labels, stats, centroids = cv2.connectedComponentsWithStats(bin_img)

    # Cluster all the intersected bounding box together
    rsmall, csmall = np.shape(small_img)
    new_img1 = np.zeros((rsmall, csmall), dtype=np.uint8)

    fill_rects(new_img1,stats)


    # Analyse New connected components to get final regions
    num_labels_new, labels_new, stats_new, centroids_new = cv2.connectedComponentsWithStats(new_img1)


    # labels_disp = np.uint8(200*labels/np.max(labels)) + 50
    # labels_disp2 = np.uint8(200*labels_new/np.max(labels_new)) + 50
    # cv2.imshow('diff',img3)
    # cv2.imshow('small_img',small_img)
    # cv2.imshow('fat_img',fat_img)
    # cv2.imshow('bin_img',bin_img)
    # cv2.imshow("labels",labels_disp)
    # cv2.imshow("labels_disp2",labels_disp2)
    # cv2.waitKey(0)

    stats = [(False, v) for i, v in enumerate(stats_new) if i > 0]
    while stats:
        scaled, (x, y, w, h, _) = stats.pop(0)
        if not scaled:
            x = int(math.floor(x * scale_up))
            y = int(math.floor(y * scale_up))
            w = int(math.ceil(w * scale_up))
            h = int(math.ceil(h * scale_up))

        if w > 255:
            half = int(w / 2)
            stats.append((True, (
                x,
                y,
                half,
                h,
                None
            )))
            stats.append((True, (
                x + half,
                y,
                w - half,
                h,
                None
            )))
            continue
        if h > 255:
            half = int(h / 2)
            stats.append((True, (
                x,
                y,
                w,
                half,
                None
            )))
            stats.append((True, (
                x,
                y + half,
                w,
                h - half,
                None
            )))
            continue

        yield x, y, w, h


class ImageParser:
    def __init__(self, args, filename, size):
        self.args = args
        self.filename = filename
        self.width, self.height, self.thumb_size = size
        self.bgcolor = None if self.args.background_color in ('common', 'edge') else self.args.background_color

        logger.debug("Open %s", self.filename)
        try:
            self.img = Image.open(self.filename)
        except Exception as e:
            raise RuntimeError("Failed to open image", self.filename) from e

        self.is_animated = getattr(self.img, 'is_animated', False)
        self.frames = self.img.n_frames if self.is_animated else 1
        self._get_bgcolor(self.img.convert('RGB'))
        self.thumb = ImageFrame(self.args, -1, self.img.convert('RGB'), 0, self.bgcolor, self.thumb_size, self.thumb_size)

    def _get_bgcolor(self, fr):
        w, h = fr.size

        pxset = {}
        if self.args.background_color == 'common':
            for y in range(h):
                for x in range(w):
                    px = fr.getpixel((x, y))
                    pxset.setdefault(px, 0)
                    pxset[px] += 1
        elif self.args.background_color == 'edge':
            pixels = [(x, 0) for x in range(w)]
            pixels += [(x, h - 1) for x in range(w)]
            pixels += [(0, y) for y in range(1, h - 1)]
            pixels += [(w - 1, y) for y in range(1, h - 1)]
            for x, y in pixels:
                px = fr.getpixel((x, y))
                pxset.setdefault(px, 0)
                pxset[px] += 1
        elif isinstance(self.args.background_color, tuple) and len(self.args.background_color) == 3:
            self.bgcolor = self.args.background_color
            return
        else:
            raise ValueError("Bad bgcolor")

        self.bgcolor = list(sorted(pxset.items(), key=lambda v: v[1]))[-1][0]

    def __iter__(self):
        last_frame = None
        for frame_num in range(self.frames):
            self.img.seek(frame_num)
            frame = ImageFrame(self.args, frame_num, self.img.convert('RGB'), self.img.info.get('duration', 0), self.bgcolor, self.width, self.height)
            diff = list(diff_images(last_frame.frame, frame.frame)) if last_frame else None
            yield diff, frame
            last_frame = frame


class ImageFrame:
    def __init__(self, args, frame_num, frame, duration, bgcolor, width, height):
        self.args = args
        self.frame_num = frame_num
        self.frame = frame
        assert self.frame.mode == 'RGB'
        self.duration = duration
        self.bgcolor = bgcolor
        self.width = width
        self.height = height

        self._resize()

    def _resize(self):
        # Resize the image
        # TODO: some images may benefit from BOX/NEAREST resampling when a pixel art look is desired
        # TODO: fit mode (fill - img fills screen, no crop, cover - img fills screen, cropped so ther's no bg)
        out_ratio = self.width / self.height
        in_ratio = self.frame.size[0] / self.frame.size[1]
        if out_ratio >= 1:
            if in_ratio <= out_ratio:
                new_size = int(self.height * in_ratio), self.height
            else:
                new_size = self.width, int(self.width / in_ratio)
        else:
            if in_ratio >= out_ratio:
                new_size = self.width, int(self.width / in_ratio)
            else:
                new_size = int(self.height * in_ratio), self.height
        self.frame = self.frame.resize(new_size)

        if self.frame.size != (self.width, self.height):
            # Center the image onto a background
            new_frame = Image.new('RGB', (self.width, self.height), self.bgcolor)
            new_frame.paste(self.frame, (int((self.width - self.frame.size[0]) / 2), int((self.height - self.frame.size[1]) / 2)))
            self.frame = new_frame

    def get_pixels(self, x=None, y=None, w=None, h=None):
        if not all((v is not None for v in (x, y, w, h))):
            x = y = 0
            w, h = self.frame.size
        for yv in range(y, y + h):
            for xv in range(x, x + w):
                yield self.frame.getpixel((xv, yv))

    def get_pixels_rle(self, max_chunk_size, x=None, y=None, w=None, h=None, only_chunk_rle=False):
        def chunk_list(data, is_rle=False):
            out = []
            for v in data:
                out.append(v)
                if max_chunk_size and (not only_chunk_rle or is_rle) and len(out) == max_chunk_size:
                    yield out
                    out = []
            if out:
                yield out

        expected_size = (w or self.frame.size[0]) * (h or self.frame.size[1])

        all_pixels = list(self.get_pixels(x, y, w, h))
        assert len(all_pixels) == expected_size

        total_px = 0
        out = list(map(lambda v: list(v[1]), itertools.groupby(all_pixels)))
        # out is a list of lists of pixel values that are all the same
        raw_px = []
        for group in out:
            if len(group) > 3:
                # The group is a long enough run of pixels to RLE-encode
                if raw_px:
                    for v in chunk_list(raw_px):
                        total_px += len(v)
                        yield 0, v
                    raw_px = []
                for v in chunk_list(group, True):
                    if v:
                        total_px += len(v)
                        yield len(v), [v[0]]
            else:
                raw_px += group
        if raw_px:
            for v in chunk_list(raw_px):
                if v:
                    total_px += len(v)
                    yield 0, v

        assert total_px == expected_size
