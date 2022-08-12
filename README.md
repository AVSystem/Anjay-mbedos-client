# Anjay-mbedos-client [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">](http://www.avsystem.com/)

## Supported hardware

This example project mainly targets the STM32L496AG-DISCOVERY development kit
[P-L496G-CELL02](https://www.st.com/en/evaluation-tools/p-l496g-cell02.html).

However, the code should run with basic functionality on any
[officially supported board by Mbed OS](https://os.mbed.com/platforms), having
at least 512K flash and 64K of memory, with the exception that the network setup
will need to be implemented (see NetworkService class in main.cpp).

### Sensor board X-NUCLEO-IKS01A2

The application also supports **optional** [X-NUCLEO-IKS01A2](https://www.st.com/en/ecosystems/x-nucleo-iks01a2.html) sensor board. This board can be attaached on top of most STM32 boards. You can enable it via configuration `"SENSORS_IKS01A2=1"` in [`mbed_app.json`](https://github.com/AVSystem/Anjay-mbedos-client/blob/master/mbed_app.json#L3).

## Overview

Application uses [mbed OS](https://os.mbed.com/mbed-os/) as the base operating system.

**NOTE:** The application currently depends on Mbed OS 6. However, please note that the [Anjay-mbedos](https://github.com/AVSystem/Anjay-mbedos) library that is declared as a dependency, supports both Mbed OS 5 and Mbed OS 6. See its README.md file for details.

The following LwM2M Objects are supported in this application:

- Security (/0),
- Server (/1),
- Device (/3),
- Connectivity Monitoring (/4),
- Firmware Update (/5).

Following objects are optional depending on HW choice:

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
4. You can now compile and flash project through UI.

NOTE: if you're using a built-in serial monitor, please make sure to use `115200` as a baud rate.

### With Mbed CLI 1

#### Ubuntu 18.04 or later

1. Download and unpack the [GCC ARM Compiler](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads).
2. Install dependencies: `sudo apt install python3-pip git mercurial`
3. Install mbed-cli tool: `python3 -m pip install mbed-cli`
4. Configure the project:
    - initialize the Mbed CLI configuration: `mbed config root .`
    - set the toolchain: `mbed toolchain GCC_ARM`
    - set path to the compiler unpacked in the first step, e.g. `mbed config GCC_ARM_PATH ~/Downloads/gcc-arm-none-eabi-9-2019-q4-major/bin`
    - set the target board: `mbed target DISCO_L496AG`
    - fetch dependencies: `mbed deploy`
5. Compile the project: `mbed compile`. The resulting binary is then `./BUILD/DISCO_L496AG/GCC_ARM/Anjay-mbedos-client.bin`.

#### Windows 10

1. Download and install [mbed-cli](https://github.com/ARMmbed/mbed-cli-windows-installer/releases/latest).
2. Open project's directory in terminal and fetch dependencies: `mbed deploy`.
3. Compile the project: `mbed compile`. The resulting binary is then `./BUILD/DISCO_L496AG/GCC_ARM/anjay-mbedos-client.bin`.

### With Mbed CLI 2

**NOTE:** Support for Mbed CLI 2 is preliminary and incomplete at this point. Using Mbed CLI 1 is recommended.

The application supports building using the `mbed-tools compile` command from [Mbed CLI 2](https://os.mbed.com/docs/mbed-os/v6.15/build-tools/mbed-cli-2.html) - please refer to its documentation for details.

However, please note that the `X_NUCLEO_IKS01A2` library that is included as a dependency, is hosted in Mercurial repositories, which are not supported by Mbed CLI 2. For this reason, the `mbed-tools deploy` command will not work. You should download the dependencies using Mbed CLI 1 tools (`mbed deploy`) or by manually cloning the dependency repositories instead.

## Flashing the STM32 board

1. Connect the USB STLINK micro-USB port on the STM32 board to your computer through a USB cable.
2. The device should show up as a mass-storage device named `DIS_L496ZG` (or similar).
3. Copy the binary `anjay-mbedos-client.bin` to the device. This triggers flashing procedure.
4. After everything finishes, the STM32 board will reboot.
5. You may now access the serial port interface e.g. through picocom on Linux: `picocom -b 115200 /dev/ttyACM0` or PuTTY on Windows.

## Connecting to the LwM2M Server

To connect to [Coiote IoT Device
Management](https://www.avsystem.com/products/coiote-iot-device-management-platform/) LwM2M Server,
please register at https://eu.iot.avsystem.cloud/. Then have a look at the Configuration menu
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

## Firmware Update support

This project supports Firmware Update using the [mbed-bootloader](https://github.com/PelionIoT/mbed-bootloader/).
This feature is intended to be usable as a drop-in replacement for the [Pelion Device Management Client](https://github.com/PelionIoT/mbed-cloud-client),
using the same bootloader, on-flash metadata and similar firmware image format.

An example configuration for this feature is provided for the
[NUCLEO-F429ZI](https://www.st.com/en/evaluation-tools/nucleo-f429zi.html) board.

### Compilation guide

**NOTE:** Building the project with support for the bootloader and firmware update is only supported with Mbed CLI 1.

mbed-bootloader needs to be compiled first. The `bootstrap.py` script automates this process.

1. As a prerequisite, please install [manifest-tool](https://github.com/PelionIoT/manifest-tool): `python3 -m pip install manifest-tool`
2. Run the bootstrap script: `./bootstrap.py --target NUCLEO_F429ZI`
    - NOTE: This will create a development configuration for `manifest-dev-tool` and modify `mbed_app.json` so that it includes the generated certificate and IDs.
3. Compile the application normally: `mbed compile --profile=release`

### Generating firmware images for FOTA

The LwM2M Firmware Update object implementation expects images that are a concatenation of the manifest,
as generated by the aforementioned `manifest-tool`, and the corresponding raw firmware update image.

This process is automated by the `firmwarize.py` script. Running it without arguments after compiling the
project will create an appropriate file and save it with the `*.pkg` extension in the per-target build
directory (e.g. `BUILD/NUCLEO_F429ZI/GCC_ARM-RELEASE`).

This flow can also be replicated manually (commands are presented for Unix-like operating systems):

1. Create the manifest file: `manifest-dev-tool create -u ' ' -p BUILD/NUCLEO_F429ZI/GCC_ARM-RELEASE/anjay-mbedos-client_update.bin --sign-image -o manifest.bin`
    - Make sure to always use the `*_update.bin` file. The build directory also other images, but they may not be compatible with the update process
      (e.g. may be merged with the bootloader).
    - NOTE: Since the manifest is always concatenated with the image in Anjay-mbedos-client, the "Payload URL" field of the manifest is ignored.
      It is thus advised to set it to a dummy value - a single space is used in this example.
2. Concatenate it with the image: `cat manifest.bin BUILD/NUCLEO_F429ZI/GCC_ARM-RELEASE/anjay-mbedos-client_update.bin > update.pkg`
3. The `update.pkg` is ready to be used as payload in the LwM2M Firmware Update process.

For production use, you may want to use `manifest-tool` instead of `manifest-dev-tool`. Please refer to the documentaiton of these tools for details.

## Persistence

This application supports persistence of Access Control, Server and Security objects. It is useful for preserving
the configuration of these objects set by either Bootstrap or Management servers across power cycles.

The persistence configuration may be adjusted in `Persistence configuration` menu.
To enable it, use `Enable / Disable` toggle option. From this point, the application,
if possible, will use the preserved configuration instead one set in `BOOTSTRAP/REGULAR SERVER`
configuration menus. `Purge persistence` option completely clears previously saved state.

## Enrollment over Secure Transport (EST)

**NOTE:** EST is a commercial feature of Anjay - it cannot be enabled and compiled against
public release of [Anjay-mbedos](https://github.com/AVSystem/Anjay-mbedos) from GitHub, which
is a dependency of this project. You can find more information on commercial features
[here](https://avsystem.github.io/Anjay-doc/CommercialFeatures.html).

This application supports Enrollment over Secure Transport feature which enhances security
by generating and managing certificates without private key leaving the device. You can find
a detailed description on using EST in Anjay [here](https://avsystem.github.io/Anjay-doc/CommercialFeatures/CF-EST.html).

To enable it, in `mbed_app.json`:

1. configure the endpoint name by setting `config/endpoint_name` field
2. provide a set of keys and certificates:
    - initial client certificate and private key should be generated using some kind of external
      tool such as OpenSSL, by calling for example:
      ```
      openssl genrsa -out private.pem 2048
      openssl req -key private.pem -new -out device.csr
      openssl x509 -signkey private.pem -in device.csr -req -days 365 -out device.crt
      ```
      note that certificate's Common Name (CN) must be equal to the endpoint name,
    - contents of `device.crt` and `private.pem` files should be set in `config/est_client_pub_cert`
      and `config/est_client_priv_key` fields respectively, contents of these files should be encoded
      as base64-encoded DER format strings,
    - server certificate should be set in `config/est_server_pub_cert` field, in the same format as above,
3. configure server's URI by setting the `config/bs_dtls_addr` field,
4. set `config/with_est` option to `true`,

**TIP:** PEM-encoded certificates can be converted to DER-encoded base64 strings by calling (on Linux)
`openssl x509 -inform pem -in certificate.pem -outform der | base64`. For RSA private keys, replace
`x509` with `rsa`.

In the runtime, please enable persistence in `Persistence configuration` menu.
From this point, the application should automatically bootstrap using the configured
server and then perform the EST process. Future reconnections will use credentials
exchanged in bootstrap and EST phase.
