/*
 * Test12 - SVC test
 *
 * Interrupts are disabled before calling osCreateTask and osKernelStart, to
 * verify kernel operations are placed behind SVC calls.
 *
 * NOTE: BASEPRI is used instead of __disable_irq()/PRIMASK. PRIMASK raises
 * the execution priority to 0, which would make the synchronous SVC
 * instruction escalate to HardFault. BASEPRI masks every configurable
 * interrupt with priority >= 1 (including PendSV at the lowest priority)
 * while still allowing SVC (priority 0) to execute. If kernel operations
 * were NOT behind SVC and relied on an immediate PendSV switch, this test
 * would hang or fault.
 *
 * Usage: paste this entire file into Core/Src/main.c, build, flash.
 * Expected serial:
 *   __AUTOTEST__$START$test12_svc
 *   create + start issued with interrupts masked
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

	/* Make sure nothing is masked once tasks are running */
	__set_BASEPRI(0);

	printf("task running after masked create/start\r\n");
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

	autotest_start("test12_svc");

	osKernelInit();

	/* Mask all configurable interrupts of priority >= 1.
	 * SVC (priority 0) still works; PendSV (lowest priority) is held. */
	__set_BASEPRI(1 << (8 - __NVIC_PRIO_BITS));

	TCB tcb = {
		.ptask = task_main,
		.stack_high = 0,
		.stack_size = TEST_STACK_SIZE,
		.state = DORMANT,
		.tid = TID_NULL,
	};

	/* These only work under the mask if they are implemented as SVC calls */
	autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
	autotest_assert_neq(tcb.tid, TID_NULL);
	autotest_assert_eq(osKernelStart(), RTX_OK);

	printf("create + start issued with interrupts masked\r\n");

	/* Unmask: the pending context switch fires and the task runs */
	__set_BASEPRI(0);

	autotest_fail("task never ran");

	while (1) {
	}
}
