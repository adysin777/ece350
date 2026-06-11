#include "common.h"
#include "k_task.h"
#include "helper.h"
#include "main.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static void autotest_start(const char *test_name)
{
	printf("\r\n\r\n\r\n");
	printf("__AUTOTEST__$START$%s\r\n", test_name);
}

static void autotest_pass(void)
{
	printf("__AUTOTEST__$PASS$\r\n");
	while (1) {
	}
}

static void autotest_fail(const char *reason)
{
	printf("__AUTOTEST__$FAIL$%s\r\n", reason);
	while (1) {
	}
}

static void autotest_assert_eq_impl(intmax_t a, const char *a_str, intmax_t b, const char *b_str)
{
	if (a != b) {
		printf("__AUTOTEST__$FAIL$Assertion failed: %s == %s\r\n", a_str, b_str);
		while (1) {
		}
	}
}

static void autotest_assert_neq_impl(intmax_t a, const char *a_str, intmax_t b, const char *b_str)
{
	if (a == b) {
		printf("__AUTOTEST__$FAIL$Assertion failed: %s != %s\r\n", a_str, b_str);
		while (1) {
		}
	}
}

#define autotest_assert(cond) autotest_assert_neq_impl((cond), #cond, 0, "0")
#define autotest_assert_eq(a, b) autotest_assert_eq_impl((a), #a, (b), #b)

#define TEST_STACK_SIZE 0x800

static const char *sentinel_data =
	"In computing, a linear-feedback shift register (LFSR) is a shift register whose input bit is a linear function of its previous state.";
static int task_run_counts[3];

static void task_main(void *args)
{
	(void)args;

	char sentinel[128];
	memcpy(sentinel, sentinel_data, sizeof(sentinel));
	int run_count = 0;

	while (1) {
		task_t t = osGetTID();
		autotest_assert_eq(memcmp(sentinel, sentinel_data, sizeof(sentinel)), 0);
		run_count++;
		task_run_counts[t - 1]++;
		autotest_assert_eq(run_count, task_run_counts[t - 1]);

		if (t == 3 && task_run_counts[2] == 16) {
			autotest_assert_eq(task_run_counts[0], 16);
			autotest_assert_eq(task_run_counts[1], 16);
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

	autotest_start("lab1_rr");
	osKernelInit();

	for (int i = 0; i < 3; i++) {
		TCB tcb = {
			.ptask = task_main,
			.stack_high = 0,
			.stack_size = TEST_STACK_SIZE,
			.state = READY,
			.tid = (task_t)(i + 1),
		};

		if (osCreateTask(&tcb) != RTX_OK) {
			autotest_fail("create task failed");
		}
		if (tcb.tid == TID_NULL) {
			autotest_fail("no tid assigned");
		}
	}

	osKernelStart();
	autotest_fail("kernel start returned");

	while (1) {
	}
}
