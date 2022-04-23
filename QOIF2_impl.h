// #ifndef _QOIF2_IMPL_H_
// #define _QOIF2_IMPL_H_

// #include <SD.h>
// #include <RingBuf.h>

// // File read buffer, in bytes
// #define QOIF2_READ_BUF_SZ 50000

// /*
// class RingBuffer {
// public:
//     constructor(File* fp) {
//         this->fp = fp;
//         this->fillBuffer();
//     }

//     void setResetPos(long new_pos) {
//         this->reset_pos = new_pos;
//     }

//     void resetPos() {
//         if (this->reset_pos > -1)
//             this->fp->seek(this->reset_pos);
//     }

//     void fillBuffer() {
//         unsigned int free = QOIF2_READ_BUF_SZ - this->size, to_read = 0, read = 0;
//         if (this->head >= this->tail) {
//             // Fill the end of the buffer
//             to_read = QOIF2_READ_BUF_SZ - this->head;
//             read = this->fp->read(this->buf + this->head, to_read);
//             this->size = (this->size + read);
//             this->head = (this->head + read) % QOIF2_READ_BUF_SZ;
//         }
//         if (this->tail - this->head > 1) {
//             // Fill the beginning of the buffer, if there's room
//             to_read = (this->tail - this->head) - 1;
//             read = this->fp->read(this->buf + this->head, to_read);
//             this->size += read;
//             this->head += read;
//         }
//         if (read < to_read) {
//             // We didn't read as many bytes as requested, so we hit EOF
//             // Up to the caller to determine if there's not enough data, & retry
//             this->resetPos();
//         }
//     }

//     bool read(uint8_t* dest, unsigned int sz) {
//         if (this->size < sz) {
//             // Don't have enough bytes to fill dest, so try to read more data
//             this->fillBuffer();
//             if (this->size < sz) {
//                 // Still don't have enough bytes - could have hit EOF, but even so, if we re-read we'd be getting some bytes from the beginning of reset_pos & that's wrong, so fail
//                 return false;
//             }
//         }

//         unsigned int to_read = 0, read = 0;

//         if (this->head >= this->tail) {
//             to_read = MIN(QOIF2_READ_BUF_SZ - this->tail, sz);
//             memcpy(dest + read, this->buf + this->tail, to_read);
//             read += to_read;
//             this->size = (this->size - read);
//             this->tail = (this->tail + read) % QOIF2_READ_BUF_SZ;
//         }
//         if (read < sz && this->tail - this->head - 1)
//             // memcpy from this->buf + this->tail to dest, sz bytes up to buf sz
//             // adjust tail to 0
//             // memcpy again, sz - read
//         }
//         // adjust tail
//         // reduce size
//         return true;
//     }

// private:
//     File* fp;
//     long reset_pos = -1;
//     unsigned int size = 0, head = 0, tail = 0;
//     uint8_t buf[QOIF2_READ_BUF_SZ];
// }
// */


// #define QOIF2_MAGIC 0x46696f71
// #define QOIF2_VERSION 2
// // Pixel buffer, in pixels (2 buffers, 2 bytes per pixel, 25k size is 100k bytes)
// #define QOIF2_PX_BUF_SZ 25000

// #define QOIF2_F_THUMB 1
// #define QOIF2_F_START 2
// #define QOIF2_F_END 4
// #define QOIF2_F_BIG 8

// #define QOIF2_E_MAGIC 1
// #define QOIF2_E_DIM 2
// #define QOIF2_E_BPP 3
// #define QOIF2_E_VER 4
// #define QOIF2_E_EOF 5  // Unexpected EOF
// #define QOIF2_EOF 6  // Expected EOF


// typedef struct __attribute__ ((packed)) {
//     uint32_t magic;
//     uint32_t width;
//     uint32_t height;
//     uint8_t channels;
//     uint8_t colorspace;
//     uint8_t version;
// } QOIF2FileHeader;

// typedef struct __attribute__ ((packed)) {
//     uint8_t flags;
//     uint16_t duration;
//     uint32_t datalen;
// } QOIF2BlockHeader1;

// typedef struct __attribute__ ((packed)) {
//     uint16_t width;
//     uint16_t height;
//     uint16_t x;
//     uint16_t y;
// } QOIF2BlockHeader2;

// typedef struct __attribute__ ((packed)) {
//     uint32_t width;
//     uint32_t height;
//     uint32_t x;
//     uint32_t y;
// } QOIF2BlockHeader2Big;


// class QOIF2 {
// public:
//     QOIF2BlockHeader1 block_info;
//     unsigned int block_width, block_height, block_x, block_y;
//     uint16_t *pixels;
//     unsigned int pixel_count = 0;

//     QOIF2(File* fp) {
//         this->fp = fp;
//     }

//     int open() {
//         // Serial.println("Opening qoif2");
//         this->fp->read((uint8_t*)&this->fh, sizeof(this->fh));
//         if (this->fh.magic != QOIF2_MAGIC) {
//             // Serial.println("Invalid magic");
//             return QOIF2_E_MAGIC;
//         }
//         if (this->fh.width != 240 || this->fh.height != 320) {
//             // TODO: figure out scaling/centering/or just rendering what fits
//             // Serial.println("Invalid dimensions");
//             return QOIF2_E_DIM;
//         }
//         if (this->fh.channels != 2) {
//             // TODO: support at least rgb
//             // Serial.println("Invalid channels");
//             return QOIF2_E_BPP;
//         }
//         if (this->fh.version != QOIF2_VERSION) {
//             // Serial.println("Invalid version");
//             return QOIF2_E_VER;
//         }

//         this->all_blocks_start = this->fp->position();
//         this->fillBuffer();

//         return 0;
//     }

//     void fillBuffer() {
//         int max_read, read_bytes;
//         uint8_t buf[10000];
//         while (!this->read_buf.isFull()) {
//             max_read = this->read_buf.maxSize() - this->read_buf.size();
//             read_bytes = 0;
//             if (max_read > 10000)
//                 max_read = 10000;
//             Serial.print("Try to read ");Serial.print(max_read);Serial.println("b into ring buffer");
//             read_bytes = this->fp->read(buf, max_read);
//             Serial.print("Read ");Serial.print(read_bytes);Serial.println("b into ring buffer");
//             for (int i = 0; i < read_bytes; i++) {
//                 this->read_buf.push(buf[i]);
//             }
//             if (read_bytes < max_read) {
//                 Serial.println("Hit EOF - resetting FP");
//                 this->fp->seek(this->all_blocks_start);
//                 return;
//             }
//         }
//     }

//     int readBlock() {
//         QOIF2BlockHeader2 bh2;
//         QOIF2BlockHeader2Big bh2b;

//         this->wbuf = 0;
//         this->rbuf = 0;
//         this->wbufpos = 0;
//         this->rbufpos = 0;
//         this->read_block_bytes = 0;

//         // Serial.println("Read block_info");
//         this->read((uint8_t*)&this->block_info, sizeof(this->block_info));
//         // Trailer is 0x00000000 0x00000001, 8b
//         // bh1 is 7b - if all 0 (datalen should never be 0), next byte must be 1
//         // if not, fail
//         if (this->block_info.flags == 0 && this->block_info.duration == 0 && this->block_info.datalen == 0) {
//             if (this->readByte() == 1) {
//                 return QOIF2_EOF;
//             } else {
//                 return QOIF2_E_EOF;
//             }
//         }

//         if (this->block_info.flags & QOIF2_F_BIG) {
//             // Serial.println("Read bh2b");
//             this->read((uint8_t*)&bh2b, sizeof(bh2b));
//             this->block_width = bh2b.width;
//             this->block_height = bh2b.height;
//             this->block_x = bh2b.x;
//             this->block_y = bh2b.y;
//         } else {
//             // Serial.println("Read bh2");
//             this->read((uint8_t*)&bh2, sizeof(bh2));
//             this->block_width = bh2.width;
//             this->block_height = bh2.height;
//             this->block_x = bh2.x;
//             this->block_y = bh2.y;
//         }

//         Serial.print("Read block of ");Serial.print(this->block_width);Serial.print("x");Serial.print(this->block_height);
//         Serial.print(" @");Serial.print(this->block_x);Serial.print("x");Serial.println(this->block_y);

//         return 0;
//     }

//     bool readPixels() {
//         // Serial.println("Read img data");
//         uint8_t tag, arg1, arg2;
//         uint16_t cur_px;
//         int8_t dr, dg, db;
//         uint8_t r, g, b, run;
//         bool buf_ovf = false;
//         while (this->read_block_bytes < this->block_info.datalen) {
//             run = 1;
//             this->read(&tag, 1);
//             this->read_block_bytes += sizeof(tag);
//             switch (tag) {
//                 case 0xff:
//                     // RGBA - not supported
//                     this->skip(4);
//                     this->read_block_bytes += 4;
//                     continue;
//                     break;
//                 case 0xfe:
//                     // RGB - already verified 16b
//                     // Serial.println("tag: rgb");
//                     this->read((uint8_t*)&cur_px, sizeof(cur_px));
//                     this->read_block_bytes += sizeof(cur_px);
//                     break;
//                 default:
//                     arg1 = tag & 0b00111111;
//                     tag = tag >> 6;
//                     switch (tag) {
//                         case 0:
//                             // index
//                             // Serial.println("tag: index");
//                             cur_px = this->cache[arg1];
//                             break;
//                         case 1:
//                             // diff
//                             // Serial.println("tag: diff");
//                             dr = (arg1 >> 4) - 2;
//                             dg = ((arg1 >> 2) & 0b11) - 2;
//                             db = (arg1 & 0b11) - 2;
//                             r = this->last_px >> 11;
//                             g = this->last_px >> 5 & 0b111111;
//                             b = this->last_px & 0b11111;
//                             r += dr;
//                             g += dg;
//                             b += db;
//                             cur_px = (r << 11) | (g << 5) | b;
//                             break;
//                         case 2:
//                             // luma
//                             // Serial.println("tag: luma");
//                             this->fp->read((uint8_t*)&arg2, sizeof(arg2));
//                             this->read_block_bytes += sizeof(arg2);
//                             cur_px = this->last_px;
//                             dg = arg1 - 32;
//                             dr = ((arg2 >> 4) - 8) + dg;
//                             db = ((arg2 & 0b1111) - 8) + dg;
//                             r = this->last_px >> 11;
//                             g = this->last_px >> 5 & 0b111111;
//                             b = this->last_px & 0b11111;
//                             r += dr;
//                             g += dg;
//                             b += db;
//                             cur_px = (r << 11) | (g << 5) | b;
//                             break;
//                         case 3:
//                             // run
//                             // Serial.println("tag: run");
//                             cur_px = this->last_px;
//                             run = arg1 + 1;
//                             break;
//                     }
//             }

//             if (this->rbufpos + run > QOIF2_PX_BUF_SZ) {
//                 // Serial.println("Write to screen - buffer full");
//                 buf_ovf = true;
//                 this->wbuf = this->rbuf;
//                 this->wbufpos = this->rbufpos;
//                 this->rbuf = this->rbuf ? 0 : 1;
//                 this->rbufpos = 0;
//                 this->pixels = this->buffer[this->wbuf];
//                 this->pixel_count = this->wbufpos;
//             }
//             for (int i = 0; i < run; i++) {
//                 this->buffer[this->rbuf][i + this->rbufpos] = cur_px;
//             }
//             this->rbufpos += run;
//             this->last_px = cur_px;
//             this->cache[(cur_px * 6311) % 64] = cur_px;

//             if (buf_ovf) return true;
//         }
//         // Serial.println("End of block data"); if not rv
//         this->wbuf = this->rbuf;
//         this->wbufpos = this->rbufpos;
//         this->rbuf = this->rbuf ? 0 : 1;
//         this->rbufpos = 0;
//         this->pixels = this->buffer[this->wbuf];
//         this->pixel_count = this->wbufpos;
//         return false;
//     }

// private:
//     File* fp;
//     QOIF2FileHeader fh;
//     long all_blocks_start = -1, read_block_bytes = 0, frame_start;
//     uint16_t buffer[2][QOIF2_PX_BUF_SZ], wbufpos = 0, rbufpos = 0, cache[64], last_px = 0;
//     uint8_t wbuf = 0, rbuf = 0;

//     RingBuf<uint8_t, QOIF2_READ_BUF_SZ> read_buf;

//     bool read(uint8_t* dest, unsigned int sz) {
//         if (sz > this->read_buf.size()) {
//             this->fillBuffer();
//             if (sz > this->read_buf.size()) {
//                 return false;
//             }
//         }
//         for (unsigned int i = 0; i < sz; i++) {
//             this->read_buf.pop(dest[i]);
//         }
//         return true;
//     }

//     void skip(unsigned int sz) {
//         uint8_t b;
//         for (unsigned int i = 0; i < sz; i++) {
//             this->read(&b, 1);
//         }
//     }

//     uint8_t readByte() {
//         uint8_t b;
//         this->read(&b, 1);
//         return b;
//     }
// };

// #endif