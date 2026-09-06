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

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
