# nfc-door-opener

Firmware for an STM32F072 Nucleo (NUCLEO-F072RB) driving an MFRC522 NFC card
reader to control a door lock. Right now it detects a card, prints its UID
over a debug serial console, and switches on LD2 (the on-board LED) as a
stand-in for the real lock actuator — the UID whitelist and the actual lock
output still need to be added.

## Hardware

- **MCU board:** NUCLEO-F072RB (STM32F072RBT6, Cortex-M0, 48 MHz, 128 KB
  Flash, 16 KB RAM)
- **NFC reader:** MFRC522 (SPI) — see [Wiring](#wiring) below
- **Lock actuator:** TBD (relay/MOSFET driving an electric strike or
  solenoid)

## Directory layout

```
Core/
  Inc/                  Application headers (main.h, stm32f0xx_it.h,
                         stm32f0xx_hal_conf.h, mfrc522.h)
  Src/                  Application sources:
                         main.c          - clock/GPIO/SPI/UART init, main loop
                         mfrc522.c       - MFRC522 SPI driver
                         syscalls.c      - printf() -> USART2 retargeting
                         stm32f0xx_it.c  - interrupt handlers
                         system_stm32f0xx.c
  Startup/
    startup_stm32f072xb.s   Reset/vector table for the STM32F072xB

Drivers/
  CMSIS/                 ARM CMSIS core headers + ST's STM32F0xx device
                          headers/system file (from STMicroelectronics'
                          cmsis_device_f0 repo)
  STM32F0xx_HAL_Driver/   ST's HAL/LL peripheral drivers (from
                          STMicroelectronics' stm32f0xx_hal_driver repo)

cmake/gcc-arm-none-eabi.cmake   CMake toolchain file for arm-none-eabi-gcc
STM32F072RBTX_FLASH.ld          Linker script (128 KB flash / 16 KB RAM)
CMakeLists.txt                  Build definition (what Makefile wraps)
Makefile                        `make build` / `make flash` / `make clean`
```

The `Drivers/` sources are vendored copies (not submodules) so the project
builds standalone without extra `git submodule` steps. They come from ST's
official repos:
- https://github.com/STMicroelectronics/stm32f0xx_hal_driver
- https://github.com/STMicroelectronics/cmsis_device_f0

## Wiring

| MFRC522 pin | Nucleo pin | Arduino label | Notes |
|---|---|---|---|
| 3.3V         | 3V3 | —   | **Not 5V** — MFRC522 is 3.3V only |
| RST          | PA9 | D8  | Active-low reset |
| GND          | GND | —   | |
| MISO         | PB4 | D5  | SPI1_MISO (remapped off its default pin) |
| MOSI         | PB5 | D4  | SPI1_MOSI (remapped off its default pin) |
| SCK          | PB3 | D3  | SPI1_SCK (remapped off its default pin) |
| SDA (= SS/CS)| PB6 | D10 | Chip-select, driven manually as GPIO |
| IRQ          | not connected | — | Not used |

SPI1 is deliberately remapped from its default PA5/PA6/PA7 pins onto
PB3/PB4/PB5 so that PA5 stays free for LD2. For wiring more than one reader,
share SCK/MISO/MOSI across all of them and give each its own CS GPIO — see
`Core/Inc/mfrc522.h` for the full pin `#define`s.

## Prerequisites

```bash
# Compiler + build tools
sudo apt install cmake ninja-build

# GNU Arm Embedded toolchain (arm-none-eabi-gcc) - if not already installed,
# download from https://developer.arm.com/downloads/-/gnu-rm and add its
# bin/ directory to PATH.

# Flashing/debugging over the Nucleo's on-board ST-LINK
sudo apt install stlink-tools    # provides st-flash, st-info, st-util
# and/or
sudo apt install openocd
```

This environment already has `arm-none-eabi-gcc` 10.3 at
`/opt/gcc-arm-none-eabi/gcc-arm-none-eabi-10.3-2021.10/bin` — make sure that's
on your `PATH`.

## Quick start (Makefile)

```bash
make build   # configure (first run) + compile into build/
make flash   # build, then flash build/nfc_door_opener.bin via st-flash
make clean   # remove the build/ directory entirely
```

`make flash` depends on `build`, so it always flashes a freshly built image —
just run `make flash` on its own each time. `make clean` is a full wipe (not
an incremental clean); the next `make build` reconfigures from scratch.

These are thin wrappers around the CMake commands below, kept for
convenience — use the CMake commands directly if you want more control
(e.g. a Debug build, or a different generator).

## Build (CMake directly)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This produces `build/nfc_door_opener.elf`, `.hex`, `.bin` and a `.map` file,
and prints a Flash/RAM usage summary.

## Flash (CMake directly)

With the board connected over USB (its on-board ST-LINK):

```bash
# via stlink-tools
st-flash --reset write build/nfc_door_opener.bin 0x8000000
# or, using the CMake helper target (same thing):
cmake --build build --target flash

# via openocd
openocd -f interface/stlink.cfg -f target/stm32f0x.cfg \
  -c "program build/nfc_door_opener.elf verify reset exit"
```

> **WSL2 note:** if `st-flash`/`make flash` fails immediately with something
> like "Could not find chip id!" or "Failed to connect to target", that's
> usually just USB-passthrough flakiness (`usbipd`), not a real problem —
> retry the command. If it instead reports a USB permission error, see
> [Debug logging](#debug-logging-printf-over-usart2) below for the same class
> of fix (add yourself to a group / `chmod` the device node).

## Debug logging (printf over USART2)

`printf()` is retargeted (via `_write()` in
[Core/Src/syscalls.c](Core/Src/syscalls.c)) onto **USART2**, which is wired
straight to the ST-LINK's virtual COM port — no extra wiring needed, it uses
the same USB cable as flashing. Settings: **115200 baud, 8N1**.

On boot, and while running, you'll see things like:

```
--- nfc-door-opener booting ---
MFRC522 VersionReg = 0x92
Ready - scan a card.
Card detected, UID: DE AD BE EF  (SAK=0x08)
```

`VersionReg` doubles as a wiring sanity check: a real chip answers with
something like `0x91`/`0x92`; `0x00` or `0xFF` means the SPI link isn't
actually reaching the reader (wiring/power problem), and the firmware fails
loud (LD2 fast-blinks forever) rather than silently never detecting cards.

To watch the output on Linux/WSL2, the port shows up as `/dev/ttyACM0`:

```bash
# one-time: let your user read the serial device without sudo each time
sudo usermod -aG dialout $USER   # then log out/in for it to take effect

# or, without logging out, just for the current device:
sudo chmod 666 /dev/ttyACM0

# then watch it (Ctrl+C to stop):
stty -F /dev/ttyACM0 115200 raw -echo
cat /dev/ttyACM0
```

(No extra terminal program like `picocom`/`minicom` is required — plain
`cat` works fine on a USB-CDC serial port.)

## Adding more to the NFC side

The HAL config ([Core/Inc/stm32f0xx_hal_conf.h](Core/Inc/stm32f0xx_hal_conf.h))
also enables the **I2C** and **TIM** HAL modules (SPI and UART are already in
use), so no build-system changes should be needed for e.g. a PN532 over I2C
alongside the MFRC522 — just add its driver source and extend `main.c`.

## Door lock output

Drive the lock actuator through a relay or MOSFET from a spare GPIO (do not
power a solenoid/strike directly from a GPIO pin) — configure it as
`GPIO_MODE_OUTPUT_PP` similar to `LD2_GPIO_Init()` in `main.c`.

## Known issues

- The reader can stop responding after continuous operation (freezes rather
  than just missing cards) until the board is reset/reflashed. Traced to
  blocking SPI/UART HAL calls with no timeout; a fix was tried and reverted
  because it didn't fully resolve it — still open.
