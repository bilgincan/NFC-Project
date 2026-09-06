# nfc-door-opener

Firmware for an STM32F072 Nucleo (NUCLEO-F072RB) driving one or more NFC card
readers to control a door lock. This currently contains just the toolchain,
vendored HAL/CMSIS drivers, and a blink sanity-check — the NFC reader and lock
control logic still need to be added.

## Hardware

- **MCU board:** NUCLEO-F072RB (STM32F072RBT6, Cortex-M0, 48 MHz, 128 KB
  Flash, 16 KB RAM)
- **NFC reader(s):** TBD (e.g. MFRC522 over SPI, or PN532 over I2C/SPI/UART)
- **Lock actuator:** TBD (relay/MOSFET driving an electric strike or
  solenoid)

## Directory layout

```
Core/
  Inc/                  Application headers (main.h, stm32f0xx_it.h,
                         stm32f0xx_hal_conf.h)
  Src/                  Application sources (main.c, stm32f0xx_it.c,
                         system_stm32f0xx.c)
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
CMakeLists.txt                  Build definition
```

The `Drivers/` sources are vendored copies (not submodules) so the project
builds standalone without extra `git submodule` steps. They come from ST's
official repos:
- https://github.com/STMicroelectronics/stm32f0xx_hal_driver
- https://github.com/STMicroelectronics/cmsis_device_f0

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

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This produces `build/nfc_door_opener.elf`, `.hex`, `.bin` and a `.map` file,
and prints a Flash/RAM usage summary.

## Flash

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

## What the sample does

[main.c](Core/Src/main.c) configures the system clock (HSI → PLL → 48 MHz)
and toggles **LD2**, the green user LED on PA5, every 500 ms — a 1 Hz blink —
using `HAL_Delay()` driven by the SysTick interrupt. It's meant purely to
confirm the toolchain, HAL drivers, linker script and flashing setup all work
before wiring up any NFC hardware.

## Adding an NFC reader

The HAL config ([Core/Inc/stm32f0xx_hal_conf.h](Core/Inc/stm32f0xx_hal_conf.h))
already enables the **SPI**, **I2C**, **UART** and **TIM** HAL modules, and
`CMakeLists.txt` compiles their driver sources, so no build-system changes
should be needed to talk to a reader — just add its driver source files and
extend `main.c`. Some options and their default Arduino-header pins on the
Nucleo-F072RB:

| Reader   | Interface        | Nucleo pins (Arduino header)                                    |
|----------|-------------------|-------------------------------------------------------------------|
| MFRC522  | SPI1              | SCK=PA5(D13), MISO=PA6(D12), MOSI=PA7(D11), SS=any GPIO (e.g. PB6/D10), RST=any GPIO |
| PN532    | I2C1              | SCL=PB8(D15), SDA=PB9(D14)                                        |
| PN532    | UART1             | TX=PA9(D8)/RX=PA10(D2) (or USART2 on the ST-LINK VCP pins, but those are used for debug printf) |

> Note: MFRC522 over SPI1 conflicts with LD2 (PA5) if you use SPI1's default
> SCK pin — either remap SPI1 to PB3/PB4/PB5, or move the LED check to a
> different pin/remove it once the reader is wired up.

For multiple readers on one SPI bus, wire each reader's SS/NSS to its own
GPIO and drive chip-select manually with `HAL_GPIO_WritePin()` around each
transaction.

## Door lock output

Drive the lock actuator through a relay or MOSFET from a spare GPIO (do not
power a solenoid/strike directly from a GPIO pin) — configure it as
`GPIO_MODE_OUTPUT_PP` similar to `LD2_GPIO_Init()` in `main.c`.
