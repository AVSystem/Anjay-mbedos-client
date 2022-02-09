
# Anjay-mbedos-client [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">](http://www.avsystem.com/)

## Supported hardware and overview

This example project mainly targets the STM32L496AG-DISCOVERY development kit
[P-L496G-CELL02](https://www.st.com/en/evaluation-tools/p-l496g-cell02.html)
along with the optional
[X-NUCLEO-IKS01A2](https://www.st.com/en/ecosystems/x-nucleo-iks01a2.html#overview)
sensor board.
* Note! There is a very similar [IKS02A1](https://www.st.com/en/ecosystems/x-nucleo-iks02a1.html#overview), but it has mostly different chips.
* If you get that - please note that only the magnetometer will work as the current driver set does not support the other chips.

However, the code should run with basic functionality on any officially supported board by mbedOS:
https://os.mbed.com/platforms, having at least 512K flash and 32K of memory and additional SPI
chip for configuration persistence, with the exception that the network setup will need to be
implemented (see NetworkService class in main.cpp).

It uses [mbedOS](https://www.mbed.com/en/platform/mbed-os/) as the base
operating system.

The following LwM2M Objects are supported in this application:

- Security (/0),
- Server (/1),
- Access Control (/2),
- Device (/3),
- Connectivity Monitoring (/4),
- Humidity (/3304),
- Accelerometer (/3313),
- Magnetometer (/3314),
- Barometer (/3315),
- Joystick (/3345).

## Compilation guide

### With Mbed Studio (NOTE: requires registration on mbed.com)

1. Download and install [Mbed Studio](https://os.mbed.com/studio/)
2. Start Mbed Studio, click File -> Open Workspace and pick the folder with cloned repository.
3. In the Libraries view, and then on `(!)` button, and then Fix all, to check out all dependencies (it may take a while).
4. You can now compile&flash project through UI.

NOTE: if you're using a built-in serial monitor, please make sure to use `115200` as a baud rate.

### With mbed-cli

#### Ubuntu 18.04 or later

1. Download and unpack the [GCC ARM Compiler](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads).
2. Install dependencies: `sudo apt install python-pip git mercurial`
3. Install mbed-cli tool: `pip install mbed-cli`
4. Configure the project:
    - set path to the compiler unpacked in the first step, e.g. `mbed config GCC_ARM_PATH ~/Downloads/gcc-arm-none-eabi-9-2019-q4-major/bin`
    - fetch dependencies: `mbed deploy`
5. Compile the project: `mbed compile`. The resulting binary is then `./BUILD/DISCO_L496AG/GCC_ARM/Anjay-mbedos-client.bin`.

#### Windows 10

1. Download and install [mbed-cli](https://github.com/ARMmbed/mbed-cli-windows-installer/releases/latest).
2. Open project's directory in terminal and fetch dependencies: `mbed deploy`.
3. Compile the project: `mbed compile`. The resulting binary is then `./BUILD/DISCO_L496AG/GCC_ARM/anjay-mbedos-client.bin`.

## Flashing the STM32 board

1. Connect the USB STLINK micro-USB port on the STM32 board to your computer through a USB cable.
2. The device should show up as a mass-storage device named `DIS_L496ZG` (or similar).
3. Copy the binary `anjay-mbedos-client.bin` to the device. This triggers flashing procedure.
4. After everything finishes, the STM32 board will reboot.
5. You may now access the serial port interface e.g. through picocom on Linux: `picocom -b 115200 /dev/ttyACM0` or PuTTY on Windows.

## Connecting to the LwM2M Server

To connect to [Coiote IoT Device
Management](https://www.avsystem.com/products/coiote-iot-device-management-platform/) LwM2M Server,
please register at https://www.avsystem.com/try-anjay/. Then have a look at the Configuration menu
to configure security credentials and other necessary settings (like modem APNs, etc.).

NOTE: You may use any LwM2M Server compliant with LwM2M 1.0 TS. The server URI can be changed
in the Configuration menu.

## Configuration menu

While connected to a serial port interface, and during bootup, the device shows:

```
Press any key in 3 seconds to enter device configuration menu...
```

You can then press any key on your keyboard to enter the configuration menu. After that you'll
see a few configuration options that can be altered and persisted within the flash memory for
future bootups.
