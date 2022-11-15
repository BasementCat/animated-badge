#ifndef FILEBUFFER_IMPL_H
#define FILEBUFFER_IMPL_H

class FileBuffer {
public:
    uint8_t *buf;
    int max_size = 0, head = 0, tail = 0, size = 0;
    File *fp;
    long reset_pos = 0;

    FileBuffer(File *fp, int size) {
        this->max_size = size;
        this->buf = (uint8_t*) malloc(size);
        this->fp = fp;
        this->reset_pos = this->fp->position();
        this->fill();
    }

    ~FileBuffer() {
        free(this->buf);
    }

    void fill() {
        int max_read, read_b;
        while (this->size < this->max_size) {
            if (this->tail <= this->head) {
                // read to end of buffer
                max_read = min(this->max_size - this->size, this->max_size - this->head);
                read_b = this->fp->read(this->buf + this->head, max_read);
            } else {
                // read from head -> tail
                max_read = this->tail - this->head;
                read_b = this->fp->read(this->buf + this->head, max_read);
            }
            this->size += read_b;
            this->head = (this->head + read_b) % this->max_size;
            if (read_b < max_read) {
                // If we hit EOF, reset to starting pos & bail - we may not need data going forward
                this->fp->seek(this->reset_pos);
                return;
            }
        }
    }

    int read(uint8_t* dest, int sz) {
        if (this->size < sz) {
            this->fill();
            if (this->size < sz) {
                return -1;
            }
        }
        for (int offset = 0; offset < sz; offset++, this->size--, this->tail = (this->tail + 1) % this->max_size) {
            dest[offset] = this->buf[this->tail];
        }
        return sz;
    }

    uint8_t readByte() {
        uint8_t b;
        this->read(&b, 1);
        return b;
    }

    int skip(int sz) {
        if (this->size < sz) {
            this->fill();
            if (this->size < sz) {
                return -1;
            }
        }
        this->tail = (this->tail + sz) % this->max_size;
        return sz;
    }
};

#endif