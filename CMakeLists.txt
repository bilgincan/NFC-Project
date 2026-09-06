cmake_minimum_required(VERSION 3.20)

# Must be set before project() when using the toolchain file below.
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)

project(nfc_door_opener C ASM)

set(TARGET ${PROJECT_NAME}.elf)
set(MCU_FAMILY STM32F0xx)
set(MCU_MODEL  STM32F072xB)

# --- HAL driver / CMSIS locations ------------------------------------------
set(HAL_DRIVER_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F0xx_HAL_Driver)
set(CMSIS_DEVICE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F0xx)
set(CMSIS_CORE_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include)

# --- Application sources -----------------------------------------------------
set(APP_SOURCES
    Core/Src/main.c
    Core/Src/stm32f0xx_it.c
    Core/Src/system_stm32f0xx.c
    Core/Src/mfrc522.c
    Core/Startup/startup_stm32f072xb.s
)

# --- HAL driver sources -------------------------------------------------------
# Kept in sync with the modules enabled in Core/Inc/stm32f0xx_hal_conf.h.
# Add more <peripheral>.c files here (and #define HAL_xxx_MODULE_ENABLED in
# stm32f0xx_hal_conf.h) as the NFC reader drivers grow (e.g. SPI/I2C already
# included below for MFRC522/PN532-style readers).
set(HAL_SOURCES
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_cortex.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_rcc.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_rcc_ex.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_gpio.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_dma.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_exti.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_flash.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_flash_ex.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_pwr.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_pwr_ex.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_i2c.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_i2c_ex.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_spi.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_spi_ex.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_uart.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_uart_ex.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_tim.c
    ${HAL_DRIVER_DIR}/Src/stm32f0xx_hal_tim_ex.c
)

add_executable(${TARGET} ${APP_SOURCES} ${HAL_SOURCES})

target_include_directories(${TARGET} PRIVATE
    Core/Inc
    ${HAL_DRIVER_DIR}/Inc
    ${CMSIS_DEVICE_DIR}/Include
    ${CMSIS_CORE_DIR}
)

target_compile_definitions(${TARGET} PRIVATE
    ${MCU_MODEL}
    USE_HAL_DRIVER
)

target_compile_options(${TARGET} PRIVATE
    -Os
    -g3
    -Wall
    -fdata-sections
    -ffunction-sections
)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32F072RBTX_FLASH.ld)

target_link_options(${TARGET} PRIVATE
    -T${LINKER_SCRIPT}
    -Wl,-Map=${PROJECT_NAME}.map
)

set_target_properties(${TARGET} PROPERTIES
    LINK_DEPENDS ${LINKER_SCRIPT}
)

# --- Post-build: .hex / .bin + size report -----------------------------------
add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET}>
    COMMENT "Generating ${PROJECT_NAME}.hex / .bin"
)

# --- Flash via st-flash (from the stlink-tools package) ----------------------
add_custom_target(flash
    COMMAND st-flash --reset write ${PROJECT_NAME}.bin 0x8000000
    DEPENDS ${TARGET}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Flashing ${PROJECT_NAME}.bin to the board over ST-LINK"
)
