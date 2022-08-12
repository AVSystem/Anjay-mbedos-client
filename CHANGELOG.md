# Changelog

## 22.08-rc1 (Fri 12th, 2022)

### Known issues
- Memory leak in integration layer (heap usage steadily increasing)

### Features
- Added support for Firmware Update object w/ mbed-cloud-client bootloader
- Added support for NUCLEO_F429ZI target
- Added support for NRF52840_DK target
- Added support for persistence of Access Control, Server and Security object
- Added support for EST (commercial feature)
- Added support for SMS binding (commercial feature)

### Improvements
- Updated Anjay-mbedos to version X.XX.X
- Added support for Mbed OS 6 and Mbed CLI 2
- More sensible model number for unsupported boards is reported in Device object
  now
- Migrated to KVStore on internal flash; removed dependency on QSPIF

## 22.03 (Mar 04th, 2022)

### Improvements
- Updated Anjay-mbedos to version 2.14.1.1

### Bugfixes
- Implemented `AvsTcpSocket::recv_with_buffer_hack()` that works around
  problems with polling on TCP sockets on certain network stacks

## 22.02 (Feb 11th, 2022)

### Improvements
- Updated to Anjay-mbedos 2.14.1
- Updated to Mbed OS 5.15.8
- Added "Load default configuration" option to the configuration menu
- Migrated socket and notification handling to Anjay event loop and
  scheduler jobs
- Updated sensors object to use more standard units
- Incorporated Janne Kiiskilä's improvements, see #8, #10, #11 and #12

### Bugfixes
- Removed explicit UDP listen port setting, which caused problems with
  IPv6 implementation in the BG96 driver

## 21.02 (Feb 1st, 2021)

### Features
- Implemented Reboot resource in Device object

### Improvements
- Updated Anjay-mbedos to version 2.9.0
- Removed unimplemented stubs from Device object implementation

## 20.05.1 (May 29th, 2020)

### Features
- Connection management using NetworkService class

### Improvements
- Refactor of QSPI handling code

## 20.05 (May 25th, 2020)

### Features
- Initial release
