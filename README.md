# ENCX24J600 Linux Kernel Driver

An out-of-tree Linux kernel driver for the **Microchip ENCX24J600** stand-alone 10/100 Ethernet controller, supporting both **SPI** and **SMI** (BCM2835 Secondary Memory Interface) bus interfaces. This driver is primarily intended for use with Raspberry Pi boards.

## Features

- Full 10/100 Mbps Ethernet support (MAC + PHY)
- Two bus interface backends:
  - **SPI** — standard SPI-attached mode (up to 14 MHz, ~8.9 Mbps throughput)
  - **SMI** — Broadcom BCM2835 Secondary Memory Interface (PSP mode 5), for Raspberry Pi
- Autonegotiation with fallback to parallel detection
- Configurable duplex and speed settings
- RX filter modes: normal, multicast, and promiscuous
- Flow control support
- DMA operations (copy, checksum)
- Interrupt-driven packet handling via kthread workers
- ethtool support (link settings, register dump, driver info)
- Device Tree overlay support for Raspberry Pi

## Prerequisites

- Linux kernel headers for your running kernel
- Device Tree Compiler (`dtc`) for building overlays
- For SMI mode: a kernel with BCM2835 SMI support (e.g., Raspberry Pi kernel)

## Building

### Build kernel modules and device tree overlays

```sh
make
```

### Build only kernel modules

```sh
make modules
```

### Build only device tree overlays

```sh
make overlays
```

### Clean build artifacts

```sh
make clean
```

## Installation

### Load the modules

For **SPI** mode:

```sh
sudo insmod encx24j600.ko
sudo insmod encx24j600-spi.ko
```

For **SMI** mode:

```sh
sudo insmod encx24j600.ko
sudo insmod encx24j600-smi.ko
```

### Apply the Device Tree overlay

Copy the appropriate `.dtbo` file to your overlays directory (e.g., `/boot/overlays/` on Raspberry Pi) and enable it in your boot configuration.

**SPI mode** (`encx24j600.dtbo`):

Add to `/boot/config.txt`:

```
dtoverlay=encx24j600
```

**SMI mode** (`encx24j600-smi.dtbo`):

Add to `/boot/config.txt`:

```
dtoverlay=encx24j600-smi
```

## Device Tree Overlay Parameters

### SPI overlay (`encx24j600-overlay.dts`)

| Parameter | Description | Default |
|-----------|-------------|---------|
| `int_pin` | GPIO pin used for the interrupt line | `25` |
| `speed`   | SPI bus clock frequency (Hz)         | `14000000` |

Example with custom parameters:

```
dtoverlay=encx24j600,int_pin=24,speed=12000000
```

### SMI overlay (`encx24j600-smi-overlay.dts`)

The SMI overlay uses GPIO pins 4–15 for the SMI data/control bus and GPIO 19 for the interrupt line. These are configured in the overlay and are not exposed as override parameters.

## Hardware Connections

### SPI Mode

| ENCX24J600 Pin | Raspberry Pi Pin | Function |
|----------------|------------------|----------|
| SCK            | GPIO 11 (SPI0 SCLK) | SPI Clock |
| SI (MOSI)      | GPIO 10 (SPI0 MOSI) | SPI Data In |
| SO (MISO)      | GPIO 9 (SPI0 MISO)  | SPI Data Out |
| CS             | GPIO 8 (SPI0 CE0)   | Chip Select |
| INT            | GPIO 25              | Interrupt (active low) |

### SMI Mode (PSP Mode 5)

The SMI mode uses the Broadcom BCM2835 Secondary Memory Interface and requires GPIO pins 4–15 for the parallel data/control bus, plus GPIO 19 for the interrupt line.

## Module Parameters

The `encx24j600` module supports the following parameter:

| Parameter | Type | Description |
|-----------|------|-------------|
| `debug`   | int  | Debug level (0=none, …, 16=all). Default: `-1` (use system default) |

## File Structure

| File | Description |
|------|-------------|
| `encx24j600.c` | Core driver: MAC/PHY initialization, TX/RX, interrupt handling, ethtool |
| `encx24j600.h` | Driver data structures and function prototypes |
| `encx24j600_hw.h` | Hardware register definitions, SRAM layout, constants |
| `encx24j600-spi.c` | SPI bus interface backend |
| `encx24j600-smi.c` | SMI (BCM2835) bus interface backend |
| `encx24j600-overlay.dts` | Device Tree overlay for SPI mode |
| `encx24j600-smi-overlay.dts` | Device Tree overlay for SMI mode |
| `Makefile` | Build system for modules and overlays |

## License

This project is licensed under the **GNU General Public License v2** (or later).

## Authors

- Jon Ringle (Gridpoint) — original SPI driver
- Sascha Ittner (modusoft GmbH) — SMI bus backend
