#ifndef NRF_BOOTLOADER_AVS_QSPI_H
#define NRF_BOOTLOADER_AVS_QSPI_H
#include "mbed.h"

namespace avs {

#if TARGET_NRF52840_DK || TARGET_DISCO_L496AG
// Both boards use MX25R6435F Macronix memory chip
enum { QSPI_FLASH_SIZE = 0x800000u, QSPI_WRITE_PAGE_SIZE = 0x100u, QSPI_ERASE_PAGE_SIZE = 0x1000u };
#else
#error "Unsupported target"
#endif

/**
 * Reads exactly @p size bytes from specified @p address.
 *
 * NOTE: this function handles unaligned addresses.
 *
 * @returns 0 on success, negative value on error.
 */
int qspi_read(QSPI &spi, size_t address, void *out_buffer, size_t size);

/**
 * Writes exactly @p size bytes at specified @p address.
 *
 * WARNING: @p address MUST be 4-byte aligned. There are no requirements on the
 * @p size though.
 *
 * @returns 0 on success, negative value on error.
 */
int qspi_write(QSPI &spi, size_t address, const void *buffer, size_t size);

/**
 * Erases a single 4KB page starting from specified @p address.
 *
 * WARNING: @p address MUST be page aligned.
 *
 * @returns 0 on success, negative value otherwise.
 */
int qspi_erase_page(QSPI &spi, size_t address);

/**
 * Erases the minimum number of pages starting from the @p address so that at
 * least @p size bytes are erased.
 *
 * WARNING: @p address MUST be page aligned.
 *
 * @returns 0 on success, negative value othwise.
 */
int qspi_erase_at_least(QSPI &spi, size_t address, size_t size);

} // namespace avs

#endif // NRF_BOOTLOADER_AVS_QSPI_H
