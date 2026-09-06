/**
  ******************************************************************************
  * @file    main.c
  * @brief   NFC door opener.
  *
  *          Polls an MFRC522 NFC reader over SPI1. Whenever a card is
  *          presented, LD2 (the user LED on the NUCLEO-F072RB, PA5) is
  *          switched on for a visible "unlocked" pulse, then the card is
  *          halted so the next tap is detected as a fresh read.
  *
  *          See Core/Inc/mfrc522.h for the reader wiring (SPI1 remapped to
  *          PB3/PB4/PB5, CS on PB6, RST on PA9).
  ******************************************************************************
  */

#include "main.h"
#include "mfrc522.h"

/* How long to hold LD2 (and, later, a real lock actuator) on after a
 * successful read. */
#define UNLOCK_PULSE_MS 1500U

static SPI_HandleTypeDef hspi1;

static void SystemClock_Config(void);
static void LD2_GPIO_Init(void);
static void SPI1_Init(void);

int main(void)
{
  /* Reset of all peripherals, and initialize the Flash interface and SysTick. */
  HAL_Init();

  /* Configure the system clock (HSI -> PLL -> 48 MHz SYSCLK). */
  SystemClock_Config();

  /* Initialize the LED GPIO and the MFRC522 reader. */
  LD2_GPIO_Init();
  SPI1_Init();
  MFRC522_Init(&hspi1);

  /* Sanity-check the SPI link before ever polling for cards: 0x00/0xFF is
   * what you read back when MISO is stuck (disconnected, wrong pin, or the
   * reader has no/unstable power) rather than a genuine chip response. If
   * this fails, something is wrong with wiring/power, not with a missing
   * card - fail loud (fast blink) instead of just sitting there silently
   * never detecting anything. */
  uint8_t version = MFRC522_GetVersion();
  if (version == 0x00 || version == 0xFF)
  {
    Error_Handler();
  }

  while (1)
  {
    MFRC522_UID uid;

    if (MFRC522_IsNewCardPresent() && MFRC522_ReadCardSerial(&uid))
    {
      /* Card read: switch the light on. */
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
      HAL_Delay(UNLOCK_PULSE_MS);
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

      /* Send the card back to HALT so it can be detected again on the next
       * tap, instead of staying ACTIVE (which would otherwise stop it from
       * answering REQA). */
      MFRC522_HaltA();
    }
  }
}

/**
  * @brief Enable the GPIOA clock and configure PA5 (LD2) as a push-pull output.
  */
static void LD2_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  LD2_GPIO_CLK_ENABLE();

  GPIO_InitStruct.Pin   = LD2_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
}

/**
  * @brief Configure SPI1 (remapped to PB3/PB4/PB5) for the MFRC522.
  *        Mode 0 (CPOL=0, CPHA=0), MSB first, software NSS - chip select is
  *        driven manually by the MFRC522 driver. 48 MHz / 32 = 1.5 MHz -
  *        well under the MFRC522's 10 MHz limit, chosen deliberately
  *        conservative for reliability over jumper-wire/breadboard
  *        connections (raise it, e.g. to _8 for 6 MHz, once the wiring is
  *        solid - a soldered/short connection can safely run faster).
  */
static void SPI1_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  MFRC522_SPI_CLK_ENABLE();
  MFRC522_SPI_GPIO_CLK_ENABLE();

  GPIO_InitStruct.Pin       = MFRC522_SCK_PIN | MFRC522_MISO_PIN | MFRC522_MOSI_PIN;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = MFRC522_SPI_AF;
  HAL_GPIO_Init(MFRC522_SPI_GPIO_PORT, &GPIO_InitStruct);

  hspi1.Instance               = MFRC522_SPI;
  hspi1.Init.Mode              = SPI_MODE_MASTER;
  hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
  hspi1.Init.NSS               = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial     = 7;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief System clock configuration.
  *        HSI (8 MHz) -> /2 -> PLL x12 -> SYSCLK = 48 MHz, HCLK = 48 MHz,
  *        PCLK1 = 48 MHz. The Nucleo-F072RB does not populate an HSE crystal
  *        by default, so HSI is used as the PLL source.
  */
static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState       = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSI; /* HSI/2 internally */
  RCC_OscInitStruct.PLL.PLLMUL     = RCC_PLL_MUL12;     /* 4 MHz * 12 = 48 MHz */
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                               | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  This is executed in case of error occurrence.
  *         Blink LD2 fast forever so a hardware/clock init failure is visible.
  */
void Error_Handler(void)
{
  __disable_irq();
  LD2_GPIO_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin   = LD2_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  while (1)
  {
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    for (volatile uint32_t i = 0; i < 200000; i++)
    {
    }
  }
}
