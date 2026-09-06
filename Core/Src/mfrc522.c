/**
  ******************************************************************************
  * @file    mfrc522.c
  * @brief   Minimal register-level driver for the MFRC522 NFC/RFID reader.
  *          Implements just enough of the ISO14443-3A / NXP MFRC522 protocol
  *          to detect a card and read its UID: REQA, single cascade-level
  *          (4-byte UID) anticollision + select, and HLTA.
  ******************************************************************************
  */

#include "mfrc522.h"
#include <string.h>

/* ===== MFRC522 register map (address only, R/W bit added on access) ====== */
#define REG_COMMAND        0x01
#define REG_COM_IRQ        0x04
#define REG_DIV_IRQ        0x05
#define REG_ERROR          0x06
#define REG_FIFO_DATA      0x09
#define REG_FIFO_LEVEL     0x0A
#define REG_CONTROL        0x0C
#define REG_BIT_FRAMING    0x0D
#define REG_COLL           0x0E
#define REG_MODE           0x11
#define REG_TX_CONTROL     0x14
#define REG_TX_ASK         0x15
#define REG_CRC_RESULT_H   0x21
#define REG_CRC_RESULT_L   0x22
#define REG_T_MODE         0x2A
#define REG_T_PRESCALER    0x2B
#define REG_T_RELOAD_H     0x2C
#define REG_T_RELOAD_L     0x2D
#define REG_VERSION        0x37

/* PCD (reader chip) commands, written to REG_COMMAND */
#define PCD_CMD_IDLE        0x00
#define PCD_CMD_CALC_CRC    0x03
#define PCD_CMD_TRANSCEIVE  0x0C
#define PCD_CMD_SOFT_RESET  0x0F

/* PICC (card) commands */
#define PICC_CMD_REQA       0x26
#define PICC_CMD_SEL_CL1    0x93
#define PICC_CMD_HLTA       0x50

/* ComIrqReg bits we wait on for a Transceive to finish */
#define IRQ_RX              0x20
#define IRQ_IDLE            0x10
#define IRQ_TIMER           0x01

typedef enum
{
  MFRC522_STATUS_OK = 0,
  MFRC522_STATUS_TIMEOUT,
  MFRC522_STATUS_ERROR,
  MFRC522_STATUS_COLLISION,
  MFRC522_STATUS_NO_ROOM,
} MFRC522_Status;

static SPI_HandleTypeDef *s_hspi;

/* ===== Low level: chip select + single/multi register access ============= */

static inline void CS_Select(void)
{
  HAL_GPIO_WritePin(MFRC522_CS_PORT, MFRC522_CS_PIN, GPIO_PIN_RESET);
}

static inline void CS_Deselect(void)
{
  HAL_GPIO_WritePin(MFRC522_CS_PORT, MFRC522_CS_PIN, GPIO_PIN_SET);
}

static void PCD_WriteRegister(uint8_t reg, uint8_t value)
{
  uint8_t addr = (uint8_t)((reg << 1) & 0x7E); /* bit7=0 selects write */
  CS_Select();
  HAL_SPI_Transmit(s_hspi, &addr, 1, HAL_MAX_DELAY);
  HAL_SPI_Transmit(s_hspi, &value, 1, HAL_MAX_DELAY);
  CS_Deselect();
}

static void PCD_WriteRegisterN(uint8_t reg, uint8_t count, const uint8_t *values)
{
  uint8_t addr = (uint8_t)((reg << 1) & 0x7E);
  CS_Select();
  HAL_SPI_Transmit(s_hspi, &addr, 1, HAL_MAX_DELAY);
  if (count > 0)
  {
    HAL_SPI_Transmit(s_hspi, (uint8_t *)values, count, HAL_MAX_DELAY);
  }
  CS_Deselect();
}

static uint8_t PCD_ReadRegister(uint8_t reg)
{
  uint8_t addr = (uint8_t)(0x80 | ((reg << 1) & 0x7E)); /* bit7=1 selects read */
  uint8_t value = 0;
  CS_Select();
  HAL_SPI_Transmit(s_hspi, &addr, 1, HAL_MAX_DELAY);
  HAL_SPI_Receive(s_hspi, &value, 1, HAL_MAX_DELAY);
  CS_Deselect();
  return value;
}

/* Multi-byte read (used for draining the FIFO). Per the MFRC522 SPI protocol,
 * the address must be re-sent before each byte except the last, where 0x00
 * is sent instead - the returned byte always lags one transfer behind. */
static void PCD_ReadRegisterN(uint8_t reg, uint8_t count, uint8_t *values)
{
  if (count == 0)
  {
    return;
  }
  uint8_t addr = (uint8_t)(0x80 | ((reg << 1) & 0x7E));
  uint8_t dummy;
  CS_Select();
  HAL_SPI_TransmitReceive(s_hspi, &addr, &dummy, 1, HAL_MAX_DELAY);
  for (uint8_t i = 0; i < (uint8_t)(count - 1); i++)
  {
    HAL_SPI_TransmitReceive(s_hspi, &addr, &values[i], 1, HAL_MAX_DELAY);
  }
  uint8_t zero = 0x00;
  HAL_SPI_TransmitReceive(s_hspi, &zero, &values[count - 1], 1, HAL_MAX_DELAY);
  CS_Deselect();
}

static void PCD_SetRegisterBitMask(uint8_t reg, uint8_t mask)
{
  uint8_t tmp = PCD_ReadRegister(reg);
  PCD_WriteRegister(reg, (uint8_t)(tmp | mask));
}

static void PCD_ClearRegisterBitMask(uint8_t reg, uint8_t mask)
{
  uint8_t tmp = PCD_ReadRegister(reg);
  PCD_WriteRegister(reg, (uint8_t)(tmp & (uint8_t)(~mask)));
}

static void PCD_AntennaOn(void)
{
  uint8_t value = PCD_ReadRegister(REG_TX_CONTROL);
  if ((value & 0x03) != 0x03)
  {
    PCD_WriteRegister(REG_TX_CONTROL, (uint8_t)(value | 0x03));
  }
}

/* Use the chip's own CRC coprocessor - avoids needing a software CRC16 table. */
static void PCD_CalculateCRC(const uint8_t *data, uint8_t length, uint8_t *result)
{
  PCD_WriteRegister(REG_COMMAND, PCD_CMD_IDLE);
  PCD_WriteRegister(REG_DIV_IRQ, 0x04);    /* clear CRCIRq */
  PCD_WriteRegister(REG_FIFO_LEVEL, 0x80); /* flush FIFO */
  PCD_WriteRegisterN(REG_FIFO_DATA, length, data);
  PCD_WriteRegister(REG_COMMAND, PCD_CMD_CALC_CRC);

  for (uint16_t i = 5000; i > 0; i--)
  {
    if (PCD_ReadRegister(REG_DIV_IRQ) & 0x04)
    {
      break;
    }
  }

  result[0] = PCD_ReadRegister(REG_CRC_RESULT_L);
  result[1] = PCD_ReadRegister(REG_CRC_RESULT_H);
}

/**
  * @brief  Send a frame to the PICC and collect its reply via the
  *         Transceive command. txLastBits selects a short frame (e.g. 7 for
  *         REQA); pass 0 for a normal whole-byte frame.
  */
static MFRC522_Status PCD_TransceiveData(const uint8_t *sendData, uint8_t sendLen,
                                          uint8_t *backData, uint8_t *backLen,
                                          uint8_t *validBits, uint8_t txLastBits)
{
  PCD_WriteRegister(REG_COMMAND, PCD_CMD_IDLE);
  PCD_WriteRegister(REG_COM_IRQ, 0x7F);      /* clear all IRQ flags */
  PCD_WriteRegister(REG_FIFO_LEVEL, 0x80);   /* flush FIFO */
  PCD_WriteRegisterN(REG_FIFO_DATA, sendLen, sendData);
  PCD_WriteRegister(REG_BIT_FRAMING, txLastBits);
  PCD_WriteRegister(REG_COMMAND, PCD_CMD_TRANSCEIVE);
  PCD_SetRegisterBitMask(REG_BIT_FRAMING, 0x80); /* StartSend = 1 */

  bool completed = false;
  for (uint16_t i = 2000; i > 0; i--)
  {
    uint8_t irq = PCD_ReadRegister(REG_COM_IRQ);
    if (irq & (IRQ_RX | IRQ_IDLE))
    {
      completed = true;
      break;
    }
    if (irq & IRQ_TIMER)
    {
      break; /* hardware timeout, no PICC in the field */
    }
  }

  PCD_ClearRegisterBitMask(REG_BIT_FRAMING, 0x80); /* StartSend = 0 */

  if (!completed)
  {
    return MFRC522_STATUS_TIMEOUT;
  }

  uint8_t errorReg = PCD_ReadRegister(REG_ERROR);
  if (errorReg & 0x13) /* BufferOvfl | ParityErr | ProtocolErr */
  {
    return MFRC522_STATUS_ERROR;
  }

  if (backData != NULL && backLen != NULL)
  {
    uint8_t n = PCD_ReadRegister(REG_FIFO_LEVEL);
    if (n > *backLen)
    {
      return MFRC522_STATUS_NO_ROOM;
    }
    *backLen = n;
    PCD_ReadRegisterN(REG_FIFO_DATA, n, backData);
    if (validBits != NULL)
    {
      *validBits = PCD_ReadRegister(REG_CONTROL) & 0x07;
    }
  }

  if (errorReg & 0x08) /* CollErr */
  {
    return MFRC522_STATUS_COLLISION;
  }

  return MFRC522_STATUS_OK;
}

/* ===== ISO14443A / PICC level ============================================= */

static MFRC522_Status PICC_RequestA(uint8_t *bufferATQA, uint8_t *bufferSize)
{
  if (bufferATQA == NULL || *bufferSize < 2)
  {
    return MFRC522_STATUS_NO_ROOM;
  }

  PCD_ClearRegisterBitMask(REG_COLL, 0x80); /* ValuesAfterColl: clear leftover collision bits */

  uint8_t validBits = 7; /* REQA is a 7-bit short frame */
  uint8_t cmd = PICC_CMD_REQA;
  MFRC522_Status status = PCD_TransceiveData(&cmd, 1, bufferATQA, bufferSize, &validBits, 7);
  if (status != MFRC522_STATUS_OK)
  {
    return status;
  }
  if (*bufferSize != 2 || validBits != 0)
  {
    return MFRC522_STATUS_ERROR; /* ATQA must be exactly 2 whole bytes */
  }
  return MFRC522_STATUS_OK;
}

/* ===== Public API ========================================================= */

void MFRC522_Init(SPI_HandleTypeDef *hspi)
{
  s_hspi = hspi;

  MFRC522_CS_GPIO_CLK_ENABLE();
  MFRC522_RST_GPIO_CLK_ENABLE();

  GPIO_InitTypeDef gpio = {0};

  gpio.Pin   = MFRC522_CS_PIN;
  gpio.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio.Pull  = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(MFRC522_CS_PORT, &gpio);
  CS_Deselect();

  gpio.Pin = MFRC522_RST_PIN;
  HAL_GPIO_Init(MFRC522_RST_PORT, &gpio);

  /* Hardware reset pulse (active low). */
  HAL_GPIO_WritePin(MFRC522_RST_PORT, MFRC522_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(2);
  HAL_GPIO_WritePin(MFRC522_RST_PORT, MFRC522_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(50); /* let the oscillator start up */

  /* Soft reset as well, for a known-good register state either way. */
  PCD_WriteRegister(REG_COMMAND, PCD_CMD_SOFT_RESET);
  HAL_Delay(50);

  /* Timer: 25 ms timeout for Transceive waits (used internally by the chip,
   * separate from our own SW polling loops above). */
  PCD_WriteRegister(REG_T_MODE, 0x80);
  PCD_WriteRegister(REG_T_PRESCALER, 0xA9);
  PCD_WriteRegister(REG_T_RELOAD_H, 0x03);
  PCD_WriteRegister(REG_T_RELOAD_L, 0xE8);

  PCD_WriteRegister(REG_TX_ASK, 0x40); /* force 100% ASK modulation */
  PCD_WriteRegister(REG_MODE, 0x3D);   /* CRC preset 0x6363 per ISO14443-3 */

  PCD_AntennaOn();
}

uint8_t MFRC522_GetVersion(void)
{
  return PCD_ReadRegister(REG_VERSION);
}

bool MFRC522_IsNewCardPresent(void)
{
  uint8_t atqa[2];
  uint8_t atqaLen = sizeof(atqa);
  return PICC_RequestA(atqa, &atqaLen) == MFRC522_STATUS_OK;
}

bool MFRC522_ReadCardSerial(MFRC522_UID *uid)
{
  uint8_t cmdBuffer[9];
  uint8_t backBuffer[5]; /* 4 UID bytes + BCC */
  uint8_t backLen;

  /* Anticollision, cascade level 1, NVB=0x20 (no bits of the UID known yet). */
  cmdBuffer[0] = PICC_CMD_SEL_CL1;
  cmdBuffer[1] = 0x20;
  backLen = sizeof(backBuffer);
  if (PCD_TransceiveData(cmdBuffer, 2, backBuffer, &backLen, NULL, 0) != MFRC522_STATUS_OK
      || backLen != 5)
  {
    return false;
  }

  uint8_t bcc = (uint8_t)(backBuffer[0] ^ backBuffer[1] ^ backBuffer[2] ^ backBuffer[3]);
  if (bcc != backBuffer[4])
  {
    return false; /* corrupted read / collision between multiple cards */
  }

  memcpy(uid->uidByte, backBuffer, 4);
  uid->size = 4;

  /* Select: NVB=0x70 (all 5 bytes known), UID+BCC + CRC_A computed on-chip. */
  cmdBuffer[0] = PICC_CMD_SEL_CL1;
  cmdBuffer[1] = 0x70;
  memcpy(&cmdBuffer[2], backBuffer, 5);
  uint8_t crc[2];
  PCD_CalculateCRC(cmdBuffer, 7, crc);
  cmdBuffer[7] = crc[0];
  cmdBuffer[8] = crc[1];

  uint8_t sakBuffer[3]; /* SAK + CRC_A */
  uint8_t sakLen = sizeof(sakBuffer);
  if (PCD_TransceiveData(cmdBuffer, 9, sakBuffer, &sakLen, NULL, 0) != MFRC522_STATUS_OK
      || sakLen < 1)
  {
    return false;
  }

  uid->sak = sakBuffer[0];
  return true;
}

void MFRC522_HaltA(void)
{
  uint8_t buffer[4];
  buffer[0] = PICC_CMD_HLTA;
  buffer[1] = 0x00;
  uint8_t crc[2];
  PCD_CalculateCRC(buffer, 2, crc);
  buffer[2] = crc[0];
  buffer[3] = crc[1];

  /* The PICC does not acknowledge HLTA, so a timeout here is the expected
   * (successful) outcome - nothing to check. */
  uint8_t backLen = 0;
  PCD_TransceiveData(buffer, 4, NULL, &backLen, NULL, 0);
}
