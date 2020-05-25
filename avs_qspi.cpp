#include "avs_qspi.h"

#include <assert.h>

#include <algorithm>

namespace avs {

namespace impl {

enum {
    QSPI_READ_STATUS_REGISTER_SIZE = 2,
    QSPI_BLOCK_MULTIPLE = 4,
    // NOTE: We would really like to use simple overloads of
    // QSPI::read()/QSPI::write() methods, but unfortunately they do some
    // strange things and any read/write ALWAYS results in a failure.
    QSPI_WRITE_OPCODE = 0x02,
    // 0x0B is FAST_READ, 0x03 is READ.
    // Don't use FAST variant unless explicitly enabled. Its implementation on
    // MX25R6435F has a documented "dummy cycle" before actual data is sent.
    // NRF implementation of QSPI interface handles that byte gracefully,
    // but STM one does not, making us receive a byte of garbage before actual
    // data.
#if TARGET_NRF52840_DK
    QSPI_READ_OPCODE = 0x0B,
#else
    QSPI_READ_OPCODE = 0x03,
#endif
    QSPI_READ_STATUS_REGISTER_OPCODE = 0x05,
    QSPI_WRITE_ENABLE_OPCODE = 0x06,
    QSPI_ERASE_SECTOR_OPCODE = 0x20
};

enum class StatusFlag {
    // MX25R6435F: WIP
    WriteInProgress = 0x01,
    // MX25R6435F: WEL
    WriteLatchEnabled = 0x02,
};

static int read_aligned_spi(QSPI &spi,
                            size_t address,
                            void *out_buffer,
                            size_t requested_size) {
    assert(address % QSPI_BLOCK_MULTIPLE == 0);
    assert(requested_size % QSPI_BLOCK_MULTIPLE == 0);
    size_t bytes_read = requested_size;
    if (spi.read(QSPI_READ_OPCODE, -1, address, (char *) out_buffer,
                 &bytes_read)
                    != QSPI_STATUS_OK
            || bytes_read != requested_size) {
        return -1;
    }
    return 0;
}

static int read_status_flags(QSPI &spi) {
    char status_value[QSPI_READ_STATUS_REGISTER_SIZE] = { 0xFF };
    if (spi.command_transfer(QSPI_READ_STATUS_REGISTER_OPCODE, -1,
                             NULL, 0, status_value, sizeof(status_value))
            != QSPI_STATUS_OK) {
        return -1;
    }
    return (uint8_t) status_value[0];
}

static int write_enable(QSPI &spi) {
    // based on
    // https://github.com/ARMmbed/mbed-os-example-qspi/blob/3cb19fe592cb40761fb1cdcea10e30a09838bf3e/main.cpp
    if (spi.command_transfer(impl::QSPI_WRITE_ENABLE_OPCODE, -1, NULL, 0, NULL,
                             0)
                    != QSPI_STATUS_OK) {
        return -1;
    }

    return 0;
}

static int start_write_or_erase(QSPI &spi) {
    // TODO: timeout
    for (;;) {
        if (write_enable(spi)) {
            return -1;
        }

        int result = read_status_flags(spi);
        if (result < 0) {
            return result;
        } else if ((result & (int) StatusFlag::WriteLatchEnabled) != 0) {
            return 0;
        }
    }
}

static int end_write_or_erase(QSPI &spi) {
    // TODO: timeout
    for (;;) {
        int result = read_status_flags(spi);
        if (result < 0) {
            return result;
        } else if ((result & (int) StatusFlag::WriteInProgress) == 0) {
            return 0;
        }
    }
}

static int verify_data(QSPI &spi,
                       size_t address,
                       const void *buffer,
                       size_t size) {
    constexpr size_t CHUNK_SIZE = 256;
    char read_buf[CHUNK_SIZE];
    const char *expected = (const char *) buffer;

    for (size_t off = 0; off < size; off += CHUNK_SIZE) {
        size_t to_read = std::min(size - off, (size_t) CHUNK_SIZE);
        if (qspi_read(spi, address, read_buf, to_read)
                || memcmp(read_buf, expected, to_read) != 0) {
            return -1;
        }
    }

    return 0;
}

static int
write_aligned_spi(QSPI &spi, size_t address, const void *buffer, size_t size) {
    assert(address % QSPI_BLOCK_MULTIPLE == 0);
    assert(size % QSPI_BLOCK_MULTIPLE == 0);

    // Assume we can't write more than QSPI_WRITE_PAGE_SIZE at a time.
    // For example, DISCO_L496AG QSPI flash (MX25R6435F) cannot handle more
    // than 256 bytes at a time, and happily discards anything but the last 256
    // bytes sent to it.
    for (size_t off = 0; off < size; off += QSPI_WRITE_PAGE_SIZE) {
        const size_t to_write = std::min(size - off, (size_t) QSPI_WRITE_PAGE_SIZE);
        size_t bytes_written = to_write;

        if (start_write_or_erase(spi)
                || spi.write(QSPI_WRITE_OPCODE, -1, address, (const char *) buffer,
                    &bytes_written)
                != QSPI_STATUS_OK
                || bytes_written != to_write
                || end_write_or_erase(spi)
                || verify_data(spi, address, buffer, to_write)) {
            return -1;
        }
    }
    return 0;
}

} // namespace impl

int qspi_read(QSPI &spi,
              size_t address,
              void *out_buffer,
              size_t requested_size) {
    //
    //  `before_size` - number of bytes before `address`. If non-zero, we'd
    //  always ignore exactly before_size-1 bytes when reading the first chunk
    //  requiring alignment.
    //  /\
    //  vv
    //  0123 4567 ...
    // [#### #### #### #### ####]
    //  ^ ^
    //  | |
    //  | addr
    //  aligned addr
    size_t before_size = address % impl::QSPI_BLOCK_MULTIPLE;
    size_t aligned_address = address - before_size;
    char *bufptr = (char *) out_buffer;

    if (before_size) {
        char aligned[impl::QSPI_BLOCK_MULTIPLE];
        if (impl::read_aligned_spi(spi, aligned_address, aligned,
                                   sizeof(aligned))) {
            return -1;
        }
        const size_t bytes_misaligned = impl::QSPI_BLOCK_MULTIPLE - before_size;
        memcpy(bufptr, &aligned[before_size], bytes_misaligned);
        bufptr += bytes_misaligned;
        // We successfully read first nonaligned chunk, our aligned address
        // can be bumped and the number of requested bytes can be decreased.
        aligned_address += impl::QSPI_BLOCK_MULTIPLE;
        requested_size -= min(requested_size, bytes_misaligned);
    }

    // The address is aligned now, and requested_bytes are the number of bytes
    // yet to be read. They still may point outside the 4-byte boundary. We'd
    // first read as much 4-byte chunks as possible, and in the end read that
    // additional (if any) 4-byte block.
    size_t after_size = requested_size % impl::QSPI_BLOCK_MULTIPLE;
    size_t aligned_size = requested_size - after_size;
    if (aligned_size) {
        if (impl::read_aligned_spi(spi, aligned_address, bufptr,
                                   aligned_size)) {
            return -1;
        }
        bufptr += aligned_size;
        aligned_address += aligned_size;
    }
    if (after_size) {
        char aligned[impl::QSPI_BLOCK_MULTIPLE];
        if (impl::read_aligned_spi(spi, aligned_address, aligned,
                                   sizeof(aligned))) {
            return -1;
        }
        memcpy(bufptr, aligned, after_size);
    }
    return 0;
}

int qspi_write(QSPI &spi, size_t address, const void *buffer, size_t size) {
    if (address % impl::QSPI_BLOCK_MULTIPLE) {
        return -1;
    }
    size_t aligned_size = size - (size % impl::QSPI_BLOCK_MULTIPLE);
    size_t bytes_left = size % impl::QSPI_BLOCK_MULTIPLE;

    if (impl::write_aligned_spi(spi, address, buffer, aligned_size)) {
        return -1;
    }
    if (bytes_left) {
        char aligned[impl::QSPI_BLOCK_MULTIPLE];
        memset(aligned, 0xFF, sizeof(aligned));
        memcpy(aligned, (const char *) buffer + aligned_size, bytes_left);
        return impl::write_aligned_spi(spi, address + aligned_size, aligned,
                                       sizeof(aligned));
    }
    return 0;
}

int qspi_erase_page(QSPI &spi, size_t address) {
    if (address % avs::QSPI_ERASE_PAGE_SIZE) {
        return -1;
    }

    if (impl::start_write_or_erase(spi)
            || spi.command_transfer(impl::QSPI_ERASE_SECTOR_OPCODE, address, NULL, 0,
                             NULL, 0)
                    != QSPI_STATUS_OK
            || impl::end_write_or_erase(spi)) {
        return -1;
    }

    return 0;
}

int qspi_erase_at_least(QSPI &spi, size_t address, size_t size) {
    if (size == 0) {
        return 0;
    }
    const size_t pages_to_erase =
            (size + avs::QSPI_ERASE_PAGE_SIZE - 1) / avs::QSPI_ERASE_PAGE_SIZE;
    int result = 0;
    for (size_t page = 0; page < pages_to_erase; ++page) {
        if ((result = avs::qspi_erase_page(
                     spi, address + avs::QSPI_ERASE_PAGE_SIZE * page))) {
            break;
        }
    }
    return result;
}

} // namespace avs
