/*
 * Test2 - Starting the kernel
 *
 * Creates two tasks, calls osKernelStart to launch them, and verifies the
 * first task ran first.
 *
 * Usage: paste this entire file into Core/Src/main.c, build, flash.
 * Expected serial:
 *   __AUTOTEST__$START$test2_kernel_start
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

static int task1_runs;
static int task2_runs;

static void task1_main(void *args)
{
	(void)args;

	/* Task 1 was created first, so it must run before task 2 */
	autotest_assert_eq(task2_runs, 0);
	task1_runs++;

	while (1) {
		osYield();
	}
}

static void task2_main(void *args)
{
	(void)args;

	task2_runs++;
	/* By the time task 2 first runs, task 1 must already have run */
	autotest_assert_eq(task1_runs, 1);
	autotest_pass();

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

	autotest_start("test2_kernel_start");

	osKernelInit();

	TCB tcb1 = {
		.ptask = task1_main,
		.stack_high = 0,
		.stack_size = TEST_STACK_SIZE,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&tcb1), RTX_OK);

	TCB tcb2 = {
		.ptask = task2_main,
		.stack_high = 0,
		.stack_size = TEST_STACK_SIZE,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&tcb2), RTX_OK);

	osKernelStart();
	autotest_fail("kernel start returned");

	while (1) {
	}
}
