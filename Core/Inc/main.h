/**
  ******************************************************************************
  * @file    main.h
  * @brief   Board pin definitions and public prototypes for the
  *          nfc-door-opener project (NUCLEO-F072RB).
  ******************************************************************************
  */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f0xx_hal.h"

/* Board pin definitions ------------------------------------------------- */

/* LD2 - the green user LED on the Nucleo board */
#define LD2_Pin                         GPIO_PIN_5
#define LD2_GPIO_Port                   GPIOA
#define LD2_GPIO_CLK_ENABLE()           __HAL_RCC_GPIOA_CLK_ENABLE()

/* B1 - the blue user button on the Nucleo board */
#define B1_Pin                          GPIO_PIN_13
#define B1_GPIO_Port                    GPIOC
#define B1_GPIO_CLK_ENABLE()            __HAL_RCC_GPIOC_CLK_ENABLE()

/* Debug UART - USART2 on PA2(TX)/PA3(RX), already routed to the ST-LINK's
 * virtual COM port on the Nucleo board (no extra wiring needed). Open it at
 * 115200 8N1 on the host to see printf() output. */
#define DEBUG_USART                     USART2
#define DEBUG_USART_CLK_ENABLE()        __HAL_RCC_USART2_CLK_ENABLE()
#define DEBUG_USART_GPIO_PORT           GPIOA
#define DEBUG_USART_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOA_CLK_ENABLE()
#define DEBUG_USART_TX_PIN              GPIO_PIN_2
#define DEBUG_USART_RX_PIN              GPIO_PIN_3
#define DEBUG_USART_AF                  GPIO_AF1_USART2
#define DEBUG_USART_BAUDRATE            115200U

extern UART_HandleTypeDef huart2;

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
