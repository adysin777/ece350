/*
 * Test13 - Kernel keeps its own TCB copy
 *
 * Verifies osCreateTask writeback, osTaskInfo readback, and that reusing the
 * caller's TCB struct does not overwrite an already-created task.
 *
 * Usage: paste this entire file into Core/Src/main.c, build, flash.
 * Expected serial:
 *   PASS: osCreateTask updated the input TCB with a valid TID=...
 *   PASS: TCB populated correctly
 *   PASS: kernel kept its own copy of TCB
 */

#include "common.h"
#include "k_task.h"
#include "main.h"

#include <stdio.h>

#define EXPECTED_MAX_TASKS 16
#define TEST_STACK_SIZE 0x400

static void Task1(void *args)
{
	(void)args;
	printf("PASS: kernel kept its own copy of TCB\r\n");
	while (1) {
		/* does not yield */
	}
}

static void TaskFail(void *args)
{
	(void)args;
	printf("FAIL: Task1 configuration was lost\r\n");
	while (1) {
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
		.ptask = Task1,
		.stack_size = TEST_STACK_SIZE,
		.tid = 0xff,
		.stack_high = 0,
	};

	if (osCreateTask(&mytask) != RTX_OK) {
		printf("FAIL: osCreateTask failed\r\n");
		while (1) {
		}
	}

	if (mytask.tid >= EXPECTED_MAX_TASKS || mytask.tid == 0) {
		printf("FAIL: osCreateTask did not update the input TCB with a valid TID\r\n\r\n");
		while (1) {
		}
	}

	printf("PASS: osCreateTask updated the input TCB with a valid TID=%u\r\n\r\n",
	       mytask.tid);

	TCB task_readback = {
		.ptask = NULL,
		.stack_size = 0,
		.tid = 0xff,
		.stack_high = 0,
	};

	if (osTaskInfo(mytask.tid, &task_readback) != RTX_OK) {
		printf("FAIL: osTaskInfo failed\r\n");
		while (1) {
		}
	}

	printf("Values retrieved by osTaskInfo:\r\n");
	printf("ptask=%p\r\n", task_readback.ptask);
	printf("stack_high=0x%x\r\n", task_readback.stack_high);
	printf("tid=%u\r\n", task_readback.tid);
	printf("state=0x%x\r\n", task_readback.state);
	printf("stack_size=0x%x\r\n", task_readback.stack_size);

	if (task_readback.tid == mytask.tid && task_readback.stack_high != 0 &&
	    task_readback.ptask == Task1 && task_readback.stack_size == TEST_STACK_SIZE &&
	    task_readback.state == READY) {
		printf("PASS: TCB populated correctly\r\n\r\n");
	} else {
		printf("FAIL: TCB not populated correctly\r\n\r\n");
		while (1) {
		}
	}

	mytask.ptask = TaskFail;
	if (osCreateTask(&mytask) != RTX_OK) {
		printf("FAIL: osCreateTask failed\r\n");
		while (1) {
		}
	}

	osKernelStart();

	while (1) {
	}
}
