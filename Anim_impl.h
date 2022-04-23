#ifndef _ANIM_H_
#define _ANIM_H_

#include <SD.h>

/*
Image format: (little endian)

Magic header:
    4b magic - 0x676d4941
    2b version
    2b header offset (start of frames)

V3 header:
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
data is either 3b RGB, or 2b 5-6-5 encoded
Chunk header:
    1b command
    1b length

    Commands:
        1: RAW - raw pixel data
        2: RLE - followed by 1 "pixel", length determines how many pixels to render
        # 3: SKIP - Don't render <length> pixels, leaving them as their previous value
        255: END - End of frame
*/

typedef struct __attribute__ ((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t h_offset;
} FileHeader;

typedef struct __attribute__ ((packed)) {
    uint16_t w;
    uint16_t h;
    uint8_t bpp;
    uint8_t _res;
    uint16_t flags;
} AnimHeader;

typedef struct __attribute__ ((packed)) {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint16_t duration;
    uint8_t flags;
    uint32_t d_len;
} FrameHeader;

typedef struct __attribute__ ((packed)) {
    uint8_t command;
    uint16_t len;
} ChunkHeader;

#define ANIM_FH_MAGIC 0x676d4941
#define ANIM_VERSION 4

#define ANIM_IF_IS_ANIM 1
#define ANIM_IF_HAS_THUMB 2

#define ANIM_FF_BEGIN 1
#define ANIM_FF_END 128

#define ANIM_C_RAW 1
#define ANIM_C_RLE 2
#define ANIM_C_END 255

#define ANIM_E_BAD_MAGIC 1
#define ANIM_E_BAD_VERSION 2
#define ANIM_E_NO_THUMB 3
#define ANIM_E_BAD_BPP 4

class Anim {
public:
    bool is_anim, has_thumb;
    int w, h;
    long frame_start_time;

    Anim(File* fp_) {
        fp = fp_;
    }

    int open() {
        fp->read((uint8_t *)&fileheader, sizeof(fileheader));
        if (fileheader.magic != ANIM_FH_MAGIC) {
            Serial.println("Bad magic");
            return ANIM_E_BAD_MAGIC;
        }
        if (fileheader.version != ANIM_VERSION) {
            Serial.println("Bad version");
            return ANIM_E_BAD_VERSION;
        }

        fp->read((uint8_t *)&animheader, sizeof(animheader));
        // TODO: should validate header here
        // TODO: 24bpp support
        if (animheader.bpp != 16) {
            Serial.println("Bad BPP");
            return ANIM_E_BAD_BPP;
        }

        is_anim = animheader.flags & ANIM_IF_IS_ANIM;
        has_thumb = animheader.flags & ANIM_IF_HAS_THUMB;
        w = animheader.w;
        h = animheader.h;

        return 0;
    }

    int read_thumb() {
        if (!has_thumb) {
            Serial.println("No thumbnail to read");
            return ANIM_E_NO_THUMB;
        }
        fp->seek(fileheader.h_offset);
        return 0;
    }

    int read_frames() {
        fp->seek(fileheader.h_offset);
        if (has_thumb) {
            FrameHeader fh;
            fp->read((uint8_t *) &fh, sizeof(fh));
            fp->seek(fp->position() + fh.d_len);
        }
        frame_start = fp->position();
        return 0;
    }

    FrameHeader* read_frame() {
        fp->read((uint8_t *) &frameheader, sizeof(frameheader));
        frame_start_time = millis();
        return &frameheader;
    }

    int read_pixels(uint16_t* buf) {
        ChunkHeader ch;
        fp->read((uint8_t*) &ch, sizeof(ch));

        switch (ch.command) {
            // TODO: 24bpp support
            case ANIM_C_RAW:
                fp->read((uint8_t*) buf, ch.len * 2);
                return ch.len;
                break;
            case ANIM_C_RLE:
                uint16_t px;
                fp->read((uint8_t*)&px, 2);
                for (int i = 0; i < ch.len; i++) {
                    buf[i] = px;
                }
                return ch.len;
                break;
            case ANIM_C_END:
                if (is_anim && !fp->available()) {
                    fp->seek(frame_start);
                }
                return 0;
                break;
        }

        return -1;
    }

    void wait_next() {
        int wait_time = frameheader.duration - (millis() - frame_start_time);
        if (wait_time > 0) {
            delay(wait_time);
        }
    }

private:
    File* fp;
    FileHeader fileheader;
    AnimHeader animheader;
    FrameHeader frameheader;
    long frame_start;
};

#endif