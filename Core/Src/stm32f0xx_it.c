/**
  ******************************************************************************
  * @file    stm32f0xx_it.c
  * @brief   Cortex-M0 interrupt/exception handlers.
  ******************************************************************************
  */

#include "main.h"
#include "stm32f0xx_it.h"

/* Cortex-M0 core handlers ------------------------------------------------- */

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
  while (1)
  {
  }
}

void SVC_Handler(void)
{
}

void PendSV_Handler(void)
{
}

/**
  * @brief SysTick interrupt handler - drives HAL_GetTick()/HAL_Delay().
  */
void SysTick_Handler(void)
{
  HAL_IncTick();
}
