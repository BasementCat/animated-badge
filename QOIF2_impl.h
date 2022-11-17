#ifndef _QOIF2_IMPL_H_
#define _QOIF2_IMPL_H_

#include <Arduino.h>
#include "Adafruit_ILI9341.h"

#include "FileBuffer_impl.h"

// QOIF2
typedef struct __attribute__ ((packed)) {
    uint32_t magic;
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    uint8_t colorspace;
    uint8_t version;
} QOIF2FileHeader;

typedef struct __attribute__ ((packed)) {
    uint8_t flags;
    uint16_t duration;
    uint32_t datalen;
} QOIF2BlockHeader1;

typedef struct __attribute__ ((packed)) {
    uint16_t width;
    uint16_t height;
    uint16_t x;
    uint16_t y;
} QOIF2BlockHeader2;

typedef struct __attribute__ ((packed)) {
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
} QOIF2BlockHeader2Big;

#define QOIF2_E_MAGIC 1
#define QOIF2_E_DIMENSIONS 2
#define QOIF2_E_CHANNELS 3
#define QOIF2_E_VERSION 4
#define QOIF2_E_TRAILER 5

#define QOIF2_F_THUMB 1
#define QOIF2_F_START 2
#define QOIF2_F_END 4
#define QOIF2_F_BIG 8

#define QOIF2_B_ONE_FRAME 101
#define QOIF2_B_END 102
#define QOIF2_B_DELAY 103
#define QOIF2_B_CONTINUE 104

#define QOIF2_MAGIC 0x46696f71
#define QOIF2_VERSION 2
// #define QOIF2_TRAILER b'\x00\x00\x00\x00\x00\x00\x00\x01'
// #define QOIF2_READ_BUF_SZ 30000
#define QOIF2_READ_BUF_SZ 10000


class QOIF2 {
private:
    Adafruit_ILI9341* tft;
    File* fp;
    QOIF2FileHeader fh;
    QOIF2BlockHeader1 bh1;
    QOIF2BlockHeader2 bh2;
    QOIF2BlockHeader2Big bh2b;
    unsigned int width, height, x, y;
    int frame_count = 0;
    int8_t dr, dg, db;
    uint8_t r, g, b, run;
    long blocks_start, frame_start;
    uint32_t trailer_temp;
    uint16_t cache[64], cur_px, last_px = 0, buffer[2][QOIF2_READ_BUF_SZ], wbufpos = 0, rbufpos = 0;
    uint8_t wbuf = 0, rbuf = 0, tag, arg1, arg2;
    FileBuffer *read_buf;

public:
    long delay_ms;
    float delay_diff;

    QOIF2(Adafruit_ILI9341* tft, File* fp) {
        this->tft = tft;
        this->fp = fp;
    }

    ~QOIF2() {
        if (this->read_buf)
            delete this->read_buf;
    }

    int open() {
        Serial.println("Opening qoif2");
        this->fp->read((uint8_t*)&this->fh, sizeof(this->fh));
        if (this->fh.magic != QOIF2_MAGIC) {
            return QOIF2_E_MAGIC;
        }
        if (this->fh.width != SCREEN_WIDTH || this->fh.height != SCREEN_HEIGHT) {
            // TODO: figure out scaling/centering/or just rendering what fits
            return QOIF2_E_DIMENSIONS;
        }
        if (this->fh.channels != 2) {
            // TODO: support at least rgb
            return QOIF2_E_CHANNELS;
        }
        if (this->fh.version != QOIF2_VERSION) {
            return QOIF2_E_VERSION;
        }

        blocks_start = this->fp->position();
        this->read_buf = new FileBuffer(this->fp, QOIF2_READ_BUF_SZ);

        return 0;
    }

    int read_and_render_block() {
        // Serial.println("Reading blocks");
        this->wbuf = 0;
        this->rbuf = 0;
        this->wbufpos = 0;
        this->rbufpos = 0;

        // Serial.println("Read bh1");
        this->read_buf->read((uint8_t*)&this->bh1, sizeof(this->bh1));
        if (this->bh1.flags == 0 && this->bh1.duration == 0 && this->bh1.datalen == 0) {
            // bh1 is 7b, trailer is 8b ending in 0x01 - datalen should never be 0 so we should be in the trailer
            if (this->read_buf->readByte() == 1) {
                // if only 1 frame, break & delay, otherwise continue the loop
                if (this->frame_count < 2)
                    return QOIF2_B_ONE_FRAME;
                return QOIF2_B_END;
            } else {
                // error condition
                return QOIF2_E_TRAILER;
            }
        }

        if (this->bh1.flags & QOIF2_F_START) {
            this->frame_start = millis();
            this->frame_count++;
        }

        if (this->bh1.flags & QOIF2_F_BIG) {
            // Serial.println("Read bh2b");
            this->read_buf->read((uint8_t*)&this->bh2b, sizeof(this->bh2b));
            this->width = this->bh2b.width;
            this->height = this->bh2b.height;
            this->x = this->bh2b.x;
            this->y = this->bh2b.y;
        } else {
            // Serial.println("Read bh2");
            this->read_buf->read((uint8_t*)&this->bh2, sizeof(this->bh2));
            this->width = this->bh2.width;
            this->height = this->bh2.height;
            this->x = this->bh2.x;
            this->y = this->bh2.y;
        }

        this->tft->dmaWait();
        this->tft->endWrite();
        this->tft->startWrite();
        this->tft->setAddrWindow(this->x, this->y, this->width, this->height);

        // Serial.println("Read img data");
        int read_b = 0;
        while (read_b < this->bh1.datalen) {
            this->run = 1;
            read_b += this->read_buf->read(&this->tag, 1);
            switch (this->tag) {
                case 0xff:
                    // RGBA - not supported
                    this->read_buf->skip(4);
                    continue;
                    break;
                case 0xfe:
                    // RGB - already verified 16b
                    // Serial.println("tag: rgb");
                    read_b += this->read_buf->read((uint8_t*)&this->cur_px, sizeof(this->cur_px));
                    break;
                default:
                    this->arg1 = this->tag & 0b00111111;
                    this->tag = this->tag >> 6;
                    switch (this->tag) {
                        case 0:
                            // index
                            // Serial.println("tag: index");
                            this->cur_px = this->cache[this->arg1];
                            break;
                        case 1:
                            // diff
                            // Serial.println("tag: diff");
                            this->dr = (this->arg1 >> 4) - 2;
                            this->dg = ((this->arg1 >> 2) & 0b11) - 2;
                            this->db = (this->arg1 & 0b11) - 2;
                            this->r = this->last_px >> 11;
                            this->g = this->last_px >> 5 & 0b111111;
                            this->b = this->last_px & 0b11111;
                            this->r += this->dr;
                            this->g += this->dg;
                            this->b += this->db;
                            this->cur_px = (this->r << 11) | (this->g << 5) | this->b;
                            break;
                        case 2:
                            // luma
                            // Serial.println("tag: luma");
                            read_b += this->read_buf->read((uint8_t*)&this->arg2, sizeof(this->arg2));
                            this->cur_px = this->last_px;
                            this->dg = this->arg1 - 32;
                            this->dr = ((this->arg2 >> 4) - 8) + this->dg;
                            this->db = ((this->arg2 & 0b1111) - 8) + this->dg;
                            this->r = this->last_px >> 11;
                            this->g = this->last_px >> 5 & 0b111111;
                            this->b = this->last_px & 0b11111;
                            this->r += this->dr;
                            this->g += this->dg;
                            this->b += this->db;
                            this->cur_px = (this->r << 11) | (this->g << 5) | this->b;
                            break;
                        case 3:
                            // run
                            // Serial.println("tag: run");
                            this->cur_px = this->last_px;
                            this->run = this->arg1 + 1;
                            break;
                    }
            }

            if (this->run > 1 || this->rbufpos + this->run > QOIF2_READ_BUF_SZ) {
                // Dump the buffer to the screen if there's a run of pixels, or it's full
                // Serial.println("Write to screen - buffer full");
                this->tft->dmaWait();
                this->wbuf = this->rbuf;
                this->wbufpos = this->rbufpos;
                this->tft->writePixels(this->buffer[this->wbuf], this->wbufpos, false);
                this->rbuf = this->rbuf ? 0 : 1;
                this->rbufpos = 0;
            }
            if (this->run > 1) {
                // write the run of pixels
                this->tft->dmaWait();
                tft->writeColor(this->cur_px, this->run);
            } else {
                // otherwise, put the pixel into the buffer
                this->buffer[this->rbuf][this->rbufpos++] = this->cur_px;
            }
            this->last_px = this->cur_px;
            this->cache[(this->cur_px * 6311) % 64] = this->cur_px;
        }

        // Serial.println("End of block data");

        if (this->rbufpos) {
            // Serial.println("Write to screen - data still in buffer");
            this->tft->dmaWait();
            this->wbuf = this->rbuf;
            this->wbufpos = this->rbufpos;
            this->tft->writePixels(this->buffer[this->wbuf], this->wbufpos, false);
        }

        if (this->bh1.flags & QOIF2_F_END) {
            this->tft->dmaWait();
            this->tft->endWrite();
            if (this->bh1.duration) {
                this->read_buf->fill();
                this->delay_ms = this->bh1.duration - (millis() - this->frame_start);
                this->delay_diff = (float) this->delay_ms / (float) this->bh1.duration;
                return QOIF2_B_DELAY;
            }
        }

        // Serial.println("End of loop");
        return QOIF2_B_CONTINUE;
    }
};

#endif