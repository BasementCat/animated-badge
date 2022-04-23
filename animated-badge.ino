#include <Arduino.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "Adafruit_ZeroDMA.h"
#include <SD.h>

#include "FileList_impl.h"
#include "prefs.h"
#include "constants.h"
#include "bootscreen_impl.h"
#include "colors.h"
#include "Anim_impl.h"
#include "QOIF2_impl.h"


Adafruit_ILI9341 tft(tft8bitbus, TFT_D0, TFT_WR, TFT_DC, TFT_CS, TFT_RESET, TFT_RD);

FileList files = FileList(FILE_DIRECTORY);
Prefs prefs;


void setup() {
	Serial.begin(SERIAL_SPEED);

	pinMode(TFT_BACKLIGHT, OUTPUT);
  	digitalWrite(TFT_BACKLIGHT, HIGH);

  	// TODO: set tft_cs to output/high? (prob. not)
  	// TODO: set sd_cs to output/high?
  	// TODO: spi config?
  	SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
   //  SPI.setClockDivider(2);

  	tft.begin();
  	tft.setRotation(4);

  	// TODO: re-enable me for prod
  	// bootscreen(&tft);
  	// still need a delay for serial
  	delay(500);

  	start_sd:
    tft.fillScreen(COLOR_BLUE);

    Serial.print("Initializing SD card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("failed!");
        tft.fillScreen(COLOR_RED);
        // die("Failed to initialize SD card", false);
        delay(1000);
        goto start_sd;
    }
    Serial.println("OK!");

    read_prefs(&prefs);
    files.init(&prefs);

    // TODO: set backlight
    // ledcWrite(TFT_BL_CHAN, prefs.brightness);
}


void loop() {
	// TODO: zero screen in between images
    File fp;
    int next_time;
    // next_time = millis() + (prefs.display_time_s * 1000);
    next_time = millis() + 4000;

    Serial.println("Open file");
    fp = SD.open(files.get_cur_file());
    if (!fp) {
    	Serial.println("Can't open file, moving to next");
    	Serial.flush();
        files.next_file(&prefs);
        return;
    }

    if (files.is_anim) {
    	Serial.println("Rendering ANIM");
    	render_anim(&fp, next_time);
    	files.next_file(&prefs);
    } else if (files.is_qoif2) {
        render_qoif2(&fp, next_time);
        files.next_file(&prefs);
    } else {
    	Serial.println("Bad file type");
    	fp.close();
    	files.next_file(&prefs);
    }
}

void render_anim(File *fp, int next_time) {
	Anim img = Anim(fp);
	Serial.println("Opening anim");
    int res = img.open();
    if (res != 0) {
        Serial.println("invalid file");
        files.next_file(&prefs);
        fp->close();
        return;
    }

    Serial.println("Seeking to frames");
    res = img.read_frames();
    if (res != 0) {
        Serial.println("failed to seek to frames");
        files.next_file(&prefs);
        fp->close();
        return;
    }

    Serial.println("Start anim loop");
    uint16_t buf[5000 * 2];
    int pixels;
    double frames=0, framesstart=millis();
    while (next_time > millis()) {
    	// Serial.println("Read frame");
        FrameHeader* fh = img.read_frame();
        // tft.dmaWait();
        // tft.endWrite();
        // tft.startWrite();
        // tft.setAddrWindow(fh->x, fh->y, fh->w, fh->h);
        // Serial.print("Frame @");Serial.print(fh->x);Serial.print("x");Serial.print(fh->y);
        // Serial.print(", dims ");Serial.print(fh->w);Serial.print("x");Serial.println(fh->h);
        while (true) {
            pixels = img.read_pixels(buf);
            if (pixels < 0) {
                Serial.println("error reading pixels");
                files.next_file(&prefs);
                fp->close();
                return;
            }
            if (pixels < 1) {
            	// tft.dmaWait();
             //    tft.endWrite();
            	frames++;
                // img.wait_next();
                break;
            }
            // tft.dmaWait();
            // tft.writePixels(buf, pixels, false);
        }
        // TODO: break loop after x ms
    }
    Serial.print("FPS: ");Serial.println(frames / (double)((millis() - framesstart) / 1000));
    fp->close();
}

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

#define QOIF2_F_THUMB 1
#define QOIF2_F_START 2
#define QOIF2_F_END 4
#define QOIF2_F_BIG 8

// #define QOIF2_MAGIC 0x716f6946
#define QOIF2_MAGIC 0x46696f71
#define QOIF2_VERSION 2
// #define QOIF2_TRAILER b'\x00\x00\x00\x00\x00\x00\x00\x01'
#define READ_BUF_SZ 25000

void render_qoif2(File *fp, int next_time) {
    QOIF2FileHeader fh;
    QOIF2BlockHeader1 bh1;
    QOIF2BlockHeader2 bh2;
    QOIF2BlockHeader2Big bh2b;
    unsigned int width, height, x, y;
    int frame_count = 0;
    int8_t dr, dg, db;
    uint8_t r, g, b, run;
    long blocks_start, block_start, frame_start, delay_ms;
    bool file_ok = true;
    uint32_t trailer_temp;
    uint16_t cache[64], cur_px, last_px = 0, buffer[2][READ_BUF_SZ], wbufpos = 0, rbufpos = 0;
    uint8_t wbuf = 0, rbuf = 0, tag, arg1, arg2;

    Serial.println("Opening qoif2");
    fp->read((uint8_t*)&fh, sizeof(fh));
    if (fh.magic != QOIF2_MAGIC) {
        file_ok = false;
        Serial.println("Invalid magic");
    }
    if (fh.width != 240 || fh.height != 320) {
        // TODO: figure out scaling/centering/or just rendering what fits
        file_ok = false;
        Serial.println("Invalid dimensions");
    }
    if (fh.channels != 2) {
        // TODO: support at least rgb
        file_ok = false;
        Serial.println("Invalid channels");
    }
    if (fh.version != QOIF2_VERSION) {
        file_ok = false;
        Serial.println("Invalid version");
    }

    if (!file_ok) {
        files.next_file(&prefs);
        fp->close();
        return;
    }

    blocks_start = fp->position();

    // Serial.println("Reading blocks");
    while (next_time > millis()) {
        wbuf = 0;
        rbuf = 0;
        wbufpos = 0;
        rbufpos = 0;

        // Serial.println("Read bh1");
        fp->read((uint8_t*)&bh1, sizeof(bh1));

        if (bh1.flags & QOIF2_F_START) {
            frame_start = millis();
            frame_count++;
        }

        if (bh1.flags & QOIF2_F_BIG) {
            // Serial.println("Read bh2b");
            fp->read((uint8_t*)&bh2b, sizeof(bh2b));
            width = bh2b.width;
            height = bh2b.height;
            x = bh2b.x;
            y = bh2b.y;
        } else {
            // Serial.println("Read bh2");
            fp->read((uint8_t*)&bh2, sizeof(bh2));
            width = bh2.width;
            height = bh2.height;
            x = bh2.x;
            y = bh2.y;
        }

        tft.dmaWait();
        tft.endWrite();
        tft.startWrite();
        tft.setAddrWindow(x, y, width, height);

        // Serial.println("Read img data");
        block_start = fp->position();
        while (fp->position() - block_start < bh1.datalen) {
            run = 1;
            fp->read(&tag, 1);
            switch (tag) {
                case 0xff:
                    // RGBA - not supported
                    fp->seek(fp->position() + 4);
                    continue;
                    break;
                case 0xfe:
                    // RGB - already verified 16b
                    // Serial.println("tag: rgb");
                    fp->read((uint8_t*)&cur_px, sizeof(cur_px));
                    break;
                default:
                    arg1 = tag & 0b00111111;
                    tag = tag >> 6;
                    switch (tag) {
                        case 0:
                            // index
                            // Serial.println("tag: index");
                            cur_px = cache[arg1];
                            break;
                        case 1:
                            // diff
                            // Serial.println("tag: diff");
                            dr = (arg1 >> 4) - 2;
                            dg = ((arg1 >> 2) & 0b11) - 2;
                            db = (arg1 & 0b11) - 2;
                            r = last_px >> 11;
                            g = last_px >> 5 & 0b111111;
                            b = last_px & 0b11111;
                            r += dr;
                            g += dg;
                            b += db;
                            cur_px = (r << 11) | (g << 5) | b;
                            break;
                        case 2:
                            // luma
                            // Serial.println("tag: luma");
                            fp->read((uint8_t*)&arg2, sizeof(arg2));
                            cur_px = last_px;
                            dg = arg1 - 32;
                            dr = ((arg2 >> 4) - 8) + dg;
                            db = ((arg2 & 0b1111) - 8) + dg;
                            r = last_px >> 11;
                            g = last_px >> 5 & 0b111111;
                            b = last_px & 0b11111;
                            r += dr;
                            g += dg;
                            b += db;
                            cur_px = (r << 11) | (g << 5) | b;
                            break;
                        case 3:
                            // run
                            // Serial.println("tag: run");
                            cur_px = last_px;
                            run = arg1 + 1;
                            break;
                    }
            }

            if (rbufpos + run > READ_BUF_SZ) {
                // Serial.println("Write to screen - buffer full");
                tft.dmaWait();
                wbuf = rbuf;
                wbufpos = rbufpos;
                tft.writePixels(buffer[wbuf], wbufpos, false);
                rbuf = rbuf ? 0 : 1;
                rbufpos = 0;
            }
            for (int i = 0; i < run; i++) {
                buffer[rbuf][i + rbufpos] = cur_px;
            }
            rbufpos += run;
            last_px = cur_px;
            cache[(cur_px * 6311) % 64] = cur_px;
        }

        // Serial.println("End of block data");

        if (rbufpos) {
            // Serial.println("Write to screen - data still in buffer");
            tft.dmaWait();
            wbuf = rbuf;
            wbufpos = rbufpos;
            tft.writePixels(buffer[wbuf], wbufpos, false);
        }

        if (bh1.flags & QOIF2_F_END) {
            tft.dmaWait();
            tft.endWrite();
            if (bh1.duration) {
                delay_ms = bh1.duration - (millis() - frame_start);
                if (delay_ms > 0)
                    delay(delay_ms);
            }
        }

        // TODO: should do this before delay, however need to break out of loop possibly but after delay
        fp->read((uint8_t*)&trailer_temp, sizeof(trailer_temp));
        if (trailer_temp == 0) {
            fp->read((uint8_t*)&trailer_temp, sizeof(trailer_temp));
            if (trailer_temp == 0x01000000) {
                // reached eof, if multi frame, seek to start & rerun
                if (frame_count > 1) {
                    fp->seek(blocks_start);
                } else {
                    break;
                }
            } else {
                fp->seek(fp->position() - sizeof(trailer_temp) * 2);
            }
        } else {
            fp->seek(fp->position() - sizeof(trailer_temp));
        }
    }
    Serial.println("End of loop");

    if (frame_count == 1) {
        delay_ms = next_time - millis();
        if (delay_ms > 0)
            delay(delay_ms);
    }

    fp->close();
}

// void render_qoif2(File *fp, int next_time) {
//     Serial.println("Instantiate qoif2");
//     QOIF2 img(fp);
//     Serial.println("read header");
//     switch (img.open()) {
//         case 0:
//             break;
//         case QOIF2_E_MAGIC:
//         case QOIF2_E_DIM:
//         case QOIF2_E_BPP:
//         case QOIF2_E_VER:
//         default:
//             // TODO: log
//             Serial.println("bad header");
//             files.next_file(&prefs);
//             fp->close();
//             return;
//     }

//     Serial.println("Reading blocks");
//     int rb_res, frame_count = 0;
//     bool rp_res;
//     long frame_start, delay_ms;
//     while (next_time > millis()) {
//         rb_res = img.readBlock();
//         if (rb_res == QOIF2_E_EOF) {
//             // TODO: log
//             files.next_file(&prefs);
//             fp->close();
//             return;
//         } else if (rb_res == QOIF2_EOF && frame_count < 2) {
//             break;
//         }

//         if (img.block_info.flags & QOIF2_F_START) {
//             frame_start = millis();
//             frame_count++;
//         }

//         tft.dmaWait();
//         tft.endWrite();
//         tft.startWrite();
//         tft.setAddrWindow(img.block_x, img.block_y, img.block_width, img.block_height);

//         while (true) {
//             rp_res = img.readPixels();
//             // TODO: maybe fill buffer a little before dmawait?
//             tft.dmaWait();
//             tft.writePixels(img.pixels, img.pixel_count, false);
//             if (!rp_res) break;
//         }


//         if (img.block_info.flags & QOIF2_F_END) {
//             tft.dmaWait();
//             tft.endWrite();
//             img.fillBuffer();
//             if (img.block_info.duration) {
//                 delay_ms = img.block_info.duration - (millis() - frame_start);
//                 if (delay_ms > 0)
//                     delay(delay_ms);
//             }
//         }
//     }
//     Serial.println("End of loop");

//     if (frame_count == 1) {
//         Serial.println("Only 1 frame, delaying until next_time");
//         delay_ms = next_time - millis();
//         if (delay_ms > 0)
//             delay(delay_ms);
//     }

//     Serial.println("End of file display, closing");
//     fp->close();
// }


void die(const char *message) {
    die(message, false);
}


void die(const char *message, bool dont_die) {
    tft.fillScreen(COLOR_BLACK);

    tft.setTextWrap(true);

    tft.setCursor(10, 15);
    tft.setTextColor(tft.color565(255, 0, 0));
    tft.setTextSize(3);
    tft.println("ERROR");

    tft.setCursor(0, 50);
    tft.setTextColor(0xffff);
    tft.setTextSize(1);
    tft.println(message);
    while (!dont_die) delay(1000);
}