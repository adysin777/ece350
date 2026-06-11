/*
 * Test3 - Round robin scheduling
 *
 * Three tasks print and yield to one another. Verifies tasks are running in
 * the correct order and alternate continuously.
 *
 * Usage: paste this entire file into Core/Src/main.c, build, flash.
 * Expected serial:
 *   __AUTOTEST__$START$test3_round_robin
 *   Task 1 turn 1
 *   Task 2 turn 2
 *   Task 3 turn 3
 *   ... (16 full cycles) ...
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
#define NUM_TASKS 3
#define NUM_CYCLES 16

static int turn;

static void task_main(void *args)
{
	(void)args;

	while (1) {
		task_t t = osGetTID();

		/* Strict round-robin: turn k must be taken by tid (k % 3) + 1 */
		autotest_assert_eq(t, (task_t)((turn % NUM_TASKS) + 1));
		turn++;
		printf("Task %u turn %d\r\n", t, turn);

		if (turn >= NUM_TASKS * NUM_CYCLES) {
			/* turn is a multiple of 3 here, so this is task 3 */
			autotest_assert_eq(t, (task_t)NUM_TASKS);
			autotest_pass();
		}

		osYield();
	}
}

int main(void)
{
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_USART2_UART_Init();

	autotest_start("test3_round_robin");

	osKernelInit();

	for (int i = 0; i < NUM_TASKS; i++) {
		TCB tcb = {
			.ptask = task_main,
			.stack_high = 0,
			.stack_size = TEST_STACK_SIZE,
			.state = DORMANT,
			.tid = TID_NULL,
		};
		autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
		autotest_assert_eq(tcb.tid, (task_t)(i + 1));
	}

	osKernelStart();
	autotest_fail("kernel start returned");

	while (1) {
	}
}
