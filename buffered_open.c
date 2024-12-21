#include "buffered_open.h"
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

// initialize the buffered_file_t structure with the file descriptor
// and allocate memory for both read and write buffers.
buffered_file_t *buffered_open(const char *pathname, int flags, ...) {
    // allocate memory for the buffered_file_t.
    buffered_file_t *bf = malloc(sizeof(buffered_file_t));
    // if allocation failed, print an error and return NULL.
    if (!bf) {
        perror("Error allocating memory for buffered_file_t");
        return NULL;
    }

    // allocate memory for read_buffer.
    bf->read_buffer = malloc(BUFFER_SIZE);
    // if allocation failed, print an error, free previously allocated memory, and return NULL.
    if (!bf->read_buffer) {
        free(bf);
        perror("Error allocating memory for read_buffer");
        return NULL;
    }

    // allocate memory for write_buffer.
    bf->write_buffer = malloc(BUFFER_SIZE);
    // if allocation failed, print an error, free previously allocated memory, and return NULL.
    if (!bf->write_buffer) {
        free(bf->read_buffer);
        free(bf);
        perror("Error allocating memory for write_buffer");
        return NULL;
    }

    // remove the O_PREAPPEND flag from the flags before calling the original open function
    // to avoid conflicts with standard file operations.
    if (flags & O_PREAPPEND) {
        bf->preappend = 1;
        flags &= ~O_PREAPPEND;
    } else {
        bf->preappend = 0;
    }

    // handle mode argument if provided.
    va_list args;
    mode_t mode;
    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    // open the file with the regular open function.
    bf->fd = open(pathname, flags, mode);
    // if open failed, print an error, free allocated memory, and return NULL.
    if (bf->fd == -1) { 
        perror("Error opening file");
        free(bf->read_buffer);
        free(bf->write_buffer);
        free(bf);
        return NULL;
    }

    // initialize additional fields in the buffered_file_t structure.
    bf->read_buffer_size = 0;
    bf->write_buffer_size = 0;
    bf->read_buffer_pos = 0;
    bf->write_buffer_pos = 0;
    bf->flags = flags;
    bf->last_operation = -1; // no operation yet.
    return bf;
}

// write data to the write buffer.
ssize_t buffered_write(buffered_file_t *bf, const void *buf, size_t count) {
    // if the last operation was a read, we need to seek to the end for appending.
    if (bf->last_operation == 0) {
        // Seek to the end of the file for appending.
        if (lseek(bf->fd, 0, SEEK_END) == -1) {
            perror("Error seeking in file for appending");
            return -1;
        }
    }
    size_t bytes_written = 0;
    if (bf->preappend) {
        // set the beginning of the file for reading the existing content.
        if (lseek(bf->fd, 0, SEEK_SET) == -1) {
            perror("Error seeking in file");
            return -1;
        }
        // allocate memory for temporary buffer to store the existing file content.
        char *temp_buf = malloc(BUFFER_SIZE);
        // if allocation failed, print an error and return -1.
        if (!temp_buf) {
            perror("Error allocating memory for temporary buffer");
            return -1;
        }

        // read existing content into temp_buf.
        ssize_t bytes_read = read(bf->fd, temp_buf, BUFFER_SIZE);
        // if read failed, print an error, free the temporary buffer, and return -1.
        if (bytes_read == -1) {
            perror("Error reading file");
            free(temp_buf);
            return -1;
        }

        // reset the file offset to the beginning for writing the new data.
        if (lseek(bf->fd, 0, SEEK_SET) == -1) {
            perror("Error seeking in file");
            free(temp_buf);
            return -1;
        }
        while (bytes_written < count) {
            size_t available_bytes_to_write = BUFFER_SIZE - bf->write_buffer_pos;
            // flush the write buffer only when it is full.
            if (available_bytes_to_write == 0) {
                if (buffered_flush(bf) == -1) {
                    free(temp_buf);
                    return -1;
                }
                available_bytes_to_write = BUFFER_SIZE;
            }
            // update bytes left to write.
            size_t to_write;
            if ((count - bytes_written) < available_bytes_to_write) {
                to_write = count - bytes_written;
            } else {
                to_write = available_bytes_to_write;
            }
            memcpy(bf->write_buffer + bf->write_buffer_pos, buf + bytes_written, to_write);
            // update the positions and counters.
            bf->write_buffer_pos += to_write;
            bytes_written += to_write;
    }

    // flush the buffer to ensure all new data is written to the file.
    if (buffered_flush(bf) == -1) {
        free(temp_buf);
        return -1;
    }

    // append the original content.
    if (bytes_read > 0) {
        if (write(bf->fd, temp_buf, bytes_read) == -1) {
            perror("Error appending original content");
            free(temp_buf);
            return -1;
        }
    }
    free(temp_buf);
    // mark last operation as write.
    bf->last_operation = 1; 
    return bytes_written;
    } else {
        // no need to preappend.
        size_t bytes_written = 0;
        while (bytes_written < count) {
            size_t available_bytes_to_write = BUFFER_SIZE - bf->write_buffer_pos;
            // flush the write buffer only when it is full.
            if (available_bytes_to_write == 0) {
                if (buffered_flush(bf) == -1) {
                    return -1;
                }
                available_bytes_to_write = BUFFER_SIZE;
            }
            // update bytes left to write.
            size_t to_write;
            if ((count - bytes_written) < available_bytes_to_write) {
                // sets to_write to the number of remaining bytes that need to be written.
                to_write = count - bytes_written; 
            } else {
                // maximum amount of data that can fit into the buffer.
                to_write = available_bytes_to_write;
            }
            memcpy(bf->write_buffer + bf->write_buffer_pos, buf + bytes_written, to_write);
            // update the positions and counters.
            bf->write_buffer_pos += to_write;
            bytes_written += to_write;
        }
        // mark last operation as write.
        bf->last_operation = 1;
        return bytes_written;
    }
}

// read data from the read buffer.
ssize_t buffered_read(buffered_file_t *bf, void *buf, size_t count) {
    // if the last operation was a write, flush the buffer.
    if (buffered_flush(bf) == -1) {
            return -1;
    }
    size_t bytes_read = 0; 
    while (bytes_read < count) {
        // if the read buffer is empty, fill it by reading from the file descriptor.
        if (bf->read_buffer_pos >= bf->read_buffer_size) {
            ssize_t bytes_read_fill = read(bf->fd, bf->read_buffer, BUFFER_SIZE);
            // failed or EOF.
            if (bytes_read_fill <= 0) {
                break;
            }
            bf->read_buffer_size = bytes_read_fill;
            bf->read_buffer_pos = 0;
        }

        // calculate how much data is available in the buffer.
        size_t available_data = bf->read_buffer_size - bf->read_buffer_pos;

        // determine how much to read this iteration.
        size_t to_read;
        if ((count - bytes_read) < available_data) {
            to_read = count - bytes_read;
        } else {
            to_read = available_data;
        }

        // copy data from the buffer to the user's buffer.
        memcpy((char*)buf + bytes_read, bf->read_buffer + bf->read_buffer_pos, to_read);

        // update positions and counters.
        bf->read_buffer_pos += to_read;
        bytes_read += to_read;
    }
    // mark last operation as read.
    bf->last_operation = 0; 
    // check if number of bytes that were actually read from the file less than count.
    // if true assign null to end.
    if (bytes_read < count) {
        ((char*)buf)[bytes_read] = '\0';
    }
    return bytes_read; 
}

int buffered_flush(buffered_file_t *bf) {
    // check if there is data to flush in the write buffer.
    if (bf->write_buffer_pos > 0) { 
        // write any pending data in the write buffer to the file.
        ssize_t bytes_written = write(bf->fd, bf->write_buffer, bf->write_buffer_pos);
        // return -1 if write failed or didn't write all data.
        if (bytes_written != bf->write_buffer_pos) {
            perror("Error flushing write buffer");
            return -1;
        }
        // reset the write buffer position.
        bf->write_buffer_pos = 0;
    }
    // return success.
    return 0;
}

void free_before_exit(buffered_file_t *bf) {
    free(bf->read_buffer);
    free(bf->write_buffer);
    free(bf);
}

int buffered_close(buffered_file_t *bf) {
    // flushing the write buffer to the file before closing the file descriptor.
    if (buffered_flush(bf) == -1) {
        free_before_exit(bf);
        return -1;
    }

    // close the file descriptor.
    int result = close(bf->fd);
    // check if close failed.
    if (result == -1) {
        perror("Error closing file");
    }

    // free allocated memory for buffers and the buffered_file_t structure.
    free_before_exit(bf);

    // return the result of close.
    return result;
}
