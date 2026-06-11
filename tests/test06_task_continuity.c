/*
 * Test6 - Task continuity
 *
 * Two tasks print and yield to one another. Each task increments and prints
 * a counter that is declared as a local variable. Verifies the counter
 * value is intact after yielding and resuming.
 *
 * Usage: paste this entire file into Core/Src/main.c, build, flash.
 * Expected serial:
 *   __AUTOTEST__$START$test6_task_continuity
 *   Task 1 counter 1
 *   Task 2 counter 1
 *   ... up to counter 16 each ...
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
#define NUM_ROUNDS 16

/* Global mirror of each task's run count, used to cross-check the local one */
static int global_counts[2];

static void task_main(void *args)
{
	(void)args;

	task_t t = osGetTID();

	/* Local counter — its value must survive every yield/resume */
	int counter = 0;

	while (1) {
		counter++;
		global_counts[t - 1]++;

		/* If the local counter were corrupted by a context switch,
		 * it would disagree with the global mirror. */
		autotest_assert_eq(counter, global_counts[t - 1]);
		printf("Task %u counter %d\r\n", t, counter);

		if (t == 2 && counter == NUM_ROUNDS) {
			autotest_assert_eq(global_counts[0], NUM_ROUNDS);
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

	autotest_start("test6_task_continuity");

	osKernelInit();

	for (int i = 0; i < 2; i++) {
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
