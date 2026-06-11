/*
 * Test5 - Stack data preservation
 *
 * Two tasks print and yield to one another. Each task prints a string that
 * is declared as a local character array. Verifies the string is intact
 * after yielding and resuming.
 *
 * Usage: paste this entire file into Core/Src/main.c, build, flash.
 * Expected serial:
 *   __AUTOTEST__$START$test5_stack_preservation
 *   ... 16 rounds of each task printing its string ...
 *   __AUTOTEST__$PASS$
 */

#include "common.h"
#include "k_task.h"
#include "helper.h"
#include "main.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

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
#define SENTINEL_LEN 96

/* Fixed-size arrays so the full SENTINEL_LEN bytes are valid to copy/compare */
static const char sentinel1[SENTINEL_LEN] =
	"Task one keeps this string on its own stack across yields.";
static const char sentinel2[SENTINEL_LEN] =
	"Task two also parks a different string on its private stack.";

static int r1, r2;

static void task_main(void *args)
{
	(void)args;

	task_t t = osGetTID();
	const char *src = (t == 1) ? sentinel1 : sentinel2;

	/* Local (stack) copy of the string — must survive every context switch */
	char local[SENTINEL_LEN];
	memcpy(local, src, SENTINEL_LEN);
	int my_runs = 0;

	while (1) {
		autotest_assert_eq(memcmp(local, src, SENTINEL_LEN), 0);
		my_runs++;
		if (t == 1) {
			r1 = my_runs;
		} else {
			r2 = my_runs;
		}
		printf("Task %u (run %d): %s\r\n", t, my_runs, local);

		if (t == 2 && my_runs == NUM_ROUNDS) {
			autotest_assert_eq(r1, NUM_ROUNDS);
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

	autotest_start("test5_stack_preservation");

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
