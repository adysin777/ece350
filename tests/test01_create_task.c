/*
 * Test1 - Creating a task
 *
 * Creates one task using osCreateTask, checks for RTX_OK, checks that a
 * valid TID was written to the input TCB. Uses osTaskInfo to read the
 * kernel TCB for this TID and verifies required fields have valid data.
 *
 * Usage: paste this entire file into Core/Src/main.c, build, flash.
 * Expected serial:
 *   __AUTOTEST__$START$test1_create_task
 *   __AUTOTEST__$PASS$
 */

#include "common.h"
#include "k_task.h"
#include "main.h"

#include <stdio.h>
#include <stdint.h>

/* ---- autotest harness (inlined so this file is self-contained) ---- */
__attribute__((unused)) static void autotest_start(const char *test_name)
{
	printf("\r\n\r\n\r\n");
	printf("__AUTOTEST__$START$%s\r\n", test_name);
}

__attribute__((unused)) static void autotest_pass(void)
{
	printf("__AUTOTEST__$PASS$\r\n");
	while (1) {
	}
}

__attribute__((unused)) static void autotest_fail(const char *reason)
{
	printf("__AUTOTEST__$FAIL$%s\r\n", reason);
	while (1) {
	}
}

__attribute__((unused)) static void autotest_assert_eq_impl(intmax_t a, const char *a_str,
							     intmax_t b, const char *b_str)
{
	if (a != b) {
		printf("__AUTOTEST__$FAIL$Assertion failed: %s == %s\r\n", a_str, b_str);
		while (1) {
		}
	}
}

__attribute__((unused)) static void autotest_assert_neq_impl(intmax_t a, const char *a_str,
							      intmax_t b, const char *b_str)
{
	if (a == b) {
		printf("__AUTOTEST__$FAIL$Assertion failed: %s != %s\r\n", a_str, b_str);
		while (1) {
		}
	}
}

#define autotest_assert(cond) autotest_assert_neq_impl((cond), #cond, 0, "0")
#define autotest_assert_eq(a, b) autotest_assert_eq_impl((a), #a, (b), #b)
#define autotest_assert_neq(a, b) autotest_assert_neq_impl((a), #a, (b), #b)
/* ---- end autotest harness ---- */

#define TEST_STACK_SIZE 0x400

static void task_main(void *args)
{
	(void)args;
	while (1) {
		osYield();
	}
}

int main(void)
{
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_USART2_UART_Init();

	autotest_start("test1_create_task");

	osKernelInit();

	TCB tcb = {
		.ptask = task_main,
		.stack_high = 0,
		.stack_size = TEST_STACK_SIZE,
		.state = DORMANT,
		.tid = TID_NULL,
	};

	/* Create returns RTX_OK */
	autotest_assert_eq(osCreateTask(&tcb), RTX_OK);

	/* A valid TID was written back into the input TCB */
	autotest_assert_neq(tcb.tid, TID_NULL);
	autotest_assert(tcb.tid < MAX_TASKS);

	/* Kernel TCB readback has valid data in every required field */
	TCB info = {0};
	autotest_assert_eq(osTaskInfo(tcb.tid, &info), RTX_OK);
	autotest_assert_eq(info.tid, tcb.tid);
	autotest_assert_eq((U32)info.ptask, (U32)task_main);
	autotest_assert(info.stack_high != 0);
	autotest_assert_eq(info.stack_size, TEST_STACK_SIZE);
	autotest_assert_eq(info.state, READY);

	printf("tid=%u ptask=%p stack_high=0x%x stack_size=0x%x state=%u\r\n",
	       info.tid, info.ptask, info.stack_high, info.stack_size, info.state);

	autotest_pass();

	while (1) {
	}
}
