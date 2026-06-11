/*
 * Test4 - Exiting a task
 *
 * Three tasks print and yield to one another. The second task calls
 * osTaskExit after five iterations. Verifies correct task ordering, and
 * that the remaining tasks continue after the task exit.
 *
 * Usage: paste this entire file into Core/Src/main.c, build, flash.
 * Expected serial:
 *   __AUTOTEST__$START$test4_task_exit
 *   ... task prints, "Task 2 exiting" after its 5th run ...
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
#define EXIT_AFTER 5
#define FINISH_AT 20

static int c1, c2, c3;

static void task_main(void *args)
{
	(void)args;

	while (1) {
		task_t t = osGetTID();

		if (t == 1) {
			c1++;
			printf("Task 1 run %d\r\n", c1);
		} else if (t == 2) {
			/* Task 1 runs immediately before task 2 in every cycle */
			autotest_assert_eq(c1, c2 + 1);
			c2++;
			printf("Task 2 run %d\r\n", c2);
			if (c2 == EXIT_AFTER) {
				printf("Task 2 exiting\r\n");
				osTaskExit();
				autotest_fail("task exit returned");
			}
		} else if (t == 3) {
			c3++;
			/* Task 1 runs once per cycle, before task 3 */
			autotest_assert_eq(c1, c3);
			/* Task 2 ran each cycle until it exited at EXIT_AFTER */
			autotest_assert_eq(c2, (c3 <= EXIT_AFTER) ? c3 : EXIT_AFTER);
			printf("Task 3 run %d\r\n", c3);

			if (c3 == FINISH_AT) {
				/* Remaining tasks kept running after the exit */
				autotest_assert_eq(c1, FINISH_AT);
				autotest_assert_eq(c2, EXIT_AFTER);
				autotest_pass();
			}
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

	autotest_start("test4_task_exit");

	osKernelInit();

	for (int i = 0; i < 3; i++) {
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
