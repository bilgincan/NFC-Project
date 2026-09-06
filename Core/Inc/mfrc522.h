/**
  ******************************************************************************
  * @file    mfrc522.h
  * @brief   Minimal register-level driver for the MFRC522 NFC/RFID reader
  *          (ISO14443A), talking over SPI.
  *
  *          Wiring (NUCLEO-F072RB <-> MFRC522) - see also README.md:
  *
  *            MFRC522 pin   Nucleo pin   Arduino label
  *            -----------   ----------   --------------
  *            3.3V          3V3          -
  *            RST           PA9          D8
  *            GND           GND          -
  *            MISO          PB4          D5
  *            MOSI          PB5          D4
  *            SCK           PB3          D3
  *            SDA (= CS)    PB6          D10
  *            IRQ           not connected
  *
  *          SPI1 is remapped off its default PA5/PA6/PA7 pins onto
  *          PB3/PB4/PB5 so PA5 stays free for LD2 (the status LED).
  ******************************************************************************
  */

#ifndef __MFRC522_H
#define __MFRC522_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* ===== Board wiring, as #defines ========================================= */

/* SPI bus */
#define MFRC522_SPI                    SPI1
#define MFRC522_SPI_CLK_ENABLE()       __HAL_RCC_SPI1_CLK_ENABLE()

#define MFRC522_SPI_GPIO_PORT          GPIOB
#define MFRC522_SPI_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOB_CLK_ENABLE()
#define MFRC522_SCK_PIN                GPIO_PIN_3   /* D3 */
#define MFRC522_MISO_PIN               GPIO_PIN_4   /* D5 */
#define MFRC522_MOSI_PIN               GPIO_PIN_5   /* D4 */
#define MFRC522_SPI_AF                 GPIO_AF0_SPI1

/* Chip-select (labelled "SDA" on the MFRC522 board - it's SPI NSS, not I2C) */
#define MFRC522_CS_PORT                GPIOB
#define MFRC522_CS_PIN                 GPIO_PIN_6   /* D10 */
#define MFRC522_CS_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOB_CLK_ENABLE()

/* Reset (active low) */
#define MFRC522_RST_PORT               GPIOA
#define MFRC522_RST_PIN                GPIO_PIN_9   /* D8 */
#define MFRC522_RST_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOA_CLK_ENABLE()

/* ===== Driver API ========================================================= */

#define MFRC522_UID_MAX_LEN 10

typedef struct
{
  uint8_t size;                        /* number of valid bytes in uidByte[] */
  uint8_t uidByte[MFRC522_UID_MAX_LEN];
  uint8_t sak;                         /* Select ACKnowledge, from the last select */
} MFRC522_UID;

/**
  * @brief  Configure the CS/RST GPIOs and bring the MFRC522 up (reset +
  *         register init + antenna on). Call after the SPI peripheral
  *         itself has already been initialized (HAL_SPI_Init).
  */
void MFRC522_Init(SPI_HandleTypeDef *hspi);

/**
  * @brief  Read the chip's VersionReg (0x37). Genuine/clone MFRC522s
  *         typically return 0x91 or 0x92; 0x00 or 0xFF means the SPI link
  *         isn't actually reaching the chip (wiring/power problem) rather
  *         than "no card" - use this to tell the two apart.
  */
uint8_t MFRC522_GetVersion(void);

/**
  * @brief  Non-blocking check: is a PICC (card/tag) currently in the field
  *         and in IDLE state (i.e. answers REQA)?
  */
bool MFRC522_IsNewCardPresent(void);

/**
  * @brief  Run anticollision + select on a card that answered REQA, and
  *         retrieve its UID. Only 4-byte (single cascade level) UIDs are
  *         supported, which covers the common Mifare Classic 1K/4K tags and
  *         most key fobs.
  * @retval true on success, uid is filled in.
  */
bool MFRC522_ReadCardSerial(MFRC522_UID *uid);

/**
  * @brief  Send HLTA (halt) so the currently-selected card drops back to
  *         IDLE/HALT and can be detected again on a future tap.
  */
void MFRC522_HaltA(void);

#ifdef __cplusplus
}
#endif

#endif /* __MFRC522_H */
