/**
  ******************************************************************************
  * @file    syscalls.c
  * @brief   Minimal newlib syscall stubs to retarget printf() onto USART2
  *          (see main.c / main.h) and to give malloc a working heap via
  *          _sbrk(), using the heap region carved out by the linker script
  *          (STM32F072RBTX_FLASH.ld: symbol `end` up to the stack).
  *
  *          Linked with -specs=nano.specs -specs=nosys.specs: nosys.specs
  *          already provides default (mostly no-op/error) versions of these
  *          syscalls, but our own definitions here take priority at link
  *          time since they're pulled in before the nosys archive is
  *          searched for the same symbols.
  ******************************************************************************
  */

#include "main.h"
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Provided by the linker script. */
extern uint8_t _estack;
extern uint32_t _Min_Stack_Size;
extern uint8_t end; /* first free byte after .bss, from ._user_heap_stack */

int _write(int file, char *ptr, int len)
{
  (void)file;
  HAL_UART_Transmit(&huart2, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
  return len;
}

void *_sbrk(int incr)
{
  static uint8_t *heap_end = NULL;
  uint8_t *prev_heap_end;
  uint8_t *stack_limit = (uint8_t *)&_estack - (uint32_t)&_Min_Stack_Size;

  if (heap_end == NULL)
  {
    heap_end = &end;
  }
  prev_heap_end = heap_end;

  if (heap_end + incr > stack_limit)
  {
    errno = ENOMEM;
    return (void *)-1;
  }

  heap_end += incr;
  return prev_heap_end;
}
