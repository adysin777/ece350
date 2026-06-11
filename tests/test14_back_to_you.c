/*
 * Test14 - Yield / exit / recreate ("Back to you N")
 *
 * Task1 increments a counter, yields, recreates itself, then exits.
 * Task2 prints the counter each round.
 *
 * Usage: paste this entire file into Core/Src/main.c, build, flash.
 * Expected serial (repeating):
 *   Back to you 1
 *   Back to you 2
 *   ...
 */

#include "common.h"
#include "k_task.h"
#include "main.h"

#include <stdio.h>

static int i_test;

static void Task1(void *args)
{
	(void)args;

	i_test++;
	osYield();

	TCB mytask = {
		.ptask = Task1,
		.stack_size = 0x400,
		.stack_high = 0,
		.tid = TID_NULL,
		.state = DORMANT,
	};
	osCreateTask(&mytask);
	osTaskExit();

	while (1) {
	}
}

static void Task2(void *args)
{
	(void)args;

	while (1) {
		printf("Back to you %d\r\n", i_test);
		osYield();
	}
}

int main(void)
{
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_USART2_UART_Init();

	osKernelInit();

	TCB mytask = {
		.stack_size = 0x400,
		.tid = 0xff,
		.stack_high = 0,
		.state = DORMANT,
	};

	mytask.ptask = Task1;
	osCreateTask(&mytask);

	mytask.ptask = Task2;
	osCreateTask(&mytask);

	osKernelStart();

	while (1) {
	}
}
