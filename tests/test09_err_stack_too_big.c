/*
 * Test9 - Error checking (stack too big)
 *
 * Creating a task that requires more space than what is available in the
 * stack region returns RTX_ERR.
 *
 * Usage: paste this entire file into Core/Src/main.c, build, flash.
 * Expected serial:
 *   __AUTOTEST__$START$test9_err_stack_too_big
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

	autotest_start("test9_err_stack_too_big");

	osKernelInit();

	/* A stack far larger than any task slot can provide must be rejected */
	TCB huge = {
		.ptask = task_main,
		.stack_high = 0,
		.stack_size = 0x1000,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&huge), RTX_ERR);

	/* Sanity: the largest supported stack still works */
	TCB ok = {
		.ptask = task_main,
		.stack_high = 0,
		.stack_size = 0x800,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&ok), RTX_OK);
	autotest_assert_neq(ok.tid, TID_NULL);

	printf("Oversized stack rejected, 0x800 stack accepted (tid=%u)\r\n", ok.tid);

	autotest_pass();

	while (1) {
	}
}
