/*
 * Test10 - Stack reuse
 *
 * Creates tasks with large stacks (0x800) until stack space becomes
 * exhausted (osCreateTask eventually returns RTX_ERR). Arbitrarily chooses
 * a TID for testing, uses osTaskInfo to record stack_high of this TID.
 *
 * Starts the kernel to run the successfully created tasks. The task with
 * the chosen TID calls osTaskExit. The subsequent task calls osCreateTask,
 * which should return successfully. Calling osTaskInfo on this new task
 * should show the same stack_high as the recorded stack_high of the
 * exited task.
 *
 * Usage: paste this entire file into Core/Src/main.c, build, flash.
 * Expected serial:
 *   __AUTOTEST__$START$test10_stack_reuse
 *   Created N tasks before exhaustion
 *   Task 4 exiting
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

#define TEST_STACK_SIZE 0x800

/* Chosen by fair dice roll */
static const task_t chosen_tid = 4;
static U32 recorded_stack_hi;

static void dummy_task_main(void *args)
{
	(void)args;
	while (1) {
		osYield();
	}
}

static void task_main(void *args)
{
	(void)args;

	while (1) {
		task_t t = osGetTID();

		if (t == chosen_tid) {
			printf("Task %u exiting\r\n", t);
			osTaskExit();
			autotest_fail("task exit returned");
		}

		if (t == chosen_tid + 1) {
			/* Runs right after the chosen task exited; its slot
			 * (TID and stack) must be reused by this create. */
			TCB tcb = {
				.ptask = dummy_task_main,
				.stack_high = 0,
				.stack_size = TEST_STACK_SIZE,
				.state = DORMANT,
				.tid = TID_NULL,
			};
			autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
			autotest_assert_eq(tcb.tid, chosen_tid);

			TCB info = {0};
			autotest_assert_eq(osTaskInfo(tcb.tid, &info), RTX_OK);
			autotest_assert_eq(info.stack_high, recorded_stack_hi);

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

	autotest_start("test10_stack_reuse");

	osKernelInit();

	/* Create large-stack tasks until creation fails */
	int created = 0;
	while (1) {
		TCB tcb = {
			.ptask = task_main,
			.stack_high = 0,
			.stack_size = TEST_STACK_SIZE,
			.state = DORMANT,
			.tid = TID_NULL,
		};
		if (osCreateTask(&tcb) != RTX_OK) {
			break;
		}
		created++;
		if (created > MAX_TASKS) {
			autotest_fail("create never returned RTX_ERR");
		}
	}
	printf("Created %d tasks before exhaustion\r\n", created);

	/* Need at least the chosen task and its successor */
	autotest_assert(created >= (int)chosen_tid + 1);

	/* Record where the chosen task's stack lives */
	TCB info = {0};
	autotest_assert_eq(osTaskInfo(chosen_tid, &info), RTX_OK);
	autotest_assert(info.stack_high != 0);
	recorded_stack_hi = info.stack_high;

	osKernelStart();
	autotest_fail("kernel start returned");

	while (1) {
	}
}
