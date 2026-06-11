/*
 * Test11 - Robustness
 *
 * One task that continuously recreates itself and exits. Expected to
 * repeat 500 times.
 *
 * Usage: paste this entire file into Core/Src/main.c, build, flash.
 * Expected serial:
 *   __AUTOTEST__$START$test11_robustness
 *   life 50
 *   life 100
 *   ... up to life 500 ...
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
#define NUM_LIVES 500

static int lives;

static void phoenix_main(void *args)
{
	(void)args;

	lives++;
	if (lives % 50 == 0) {
		printf("life %d\r\n", lives);
	}

	if (lives >= NUM_LIVES) {
		autotest_pass();
	}

	/* Recreate myself, then exit — the replacement carries on */
	TCB tcb = {
		.ptask = phoenix_main,
		.stack_high = 0,
		.stack_size = TEST_STACK_SIZE,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	if (osCreateTask(&tcb) != RTX_OK) {
		autotest_fail("recreate failed");
	}

	osTaskExit();
	autotest_fail("task exit returned");
}

int main(void)
{
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_USART2_UART_Init();

	autotest_start("test11_robustness");

	osKernelInit();

	TCB tcb = {
		.ptask = phoenix_main,
		.stack_high = 0,
		.stack_size = TEST_STACK_SIZE,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&tcb), RTX_OK);

	osKernelStart();
	autotest_fail("kernel start returned");

	while (1) {
	}
}
