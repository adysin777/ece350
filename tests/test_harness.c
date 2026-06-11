#include "test_harness.h"

#include "main.h"

#include <stdio.h>

#define TEST_SUITE_MAGIC 0xECE35003u /* bump whenever test_suite_state_t changes */

typedef struct {
	uint32_t magic;
	uint32_t index;
	uint32_t passed;
	uint32_t failed;
	uint32_t attempted; /* set before dispatch; still set after reset = test hung */
} test_suite_state_t;

/* Survives NVIC_SystemReset so the suite can resume at the next test */
static test_suite_state_t g_suite __attribute__((section(".noinit")));

extern void (*const g_test_fns[])(void);
extern const char *const g_test_names[];
extern const unsigned g_test_count;

static void test_suite_finish(int passed, const char *reason)
{
	const char *name = g_test_names[g_suite.index];

	if (passed) {
		printf("[PASS] %s\r\n", name);
		printf("__AUTOTEST__$PASS$\r\n");
		g_suite.passed++;
	} else {
		printf("[FAIL] %s", name);
		if (reason != NULL && reason[0] != '\0') {
			printf(": %s", reason);
		}
		printf("\r\n");
		if (reason != NULL && reason[0] != '\0') {
			printf("__AUTOTEST__$FAIL$%s\r\n", reason);
		} else {
			printf("__AUTOTEST__$FAIL$\r\n");
		}
		g_suite.failed++;
	}

	g_suite.index++;
	g_suite.attempted = 0;

	if (g_suite.index >= g_test_count) {
		printf("\r\n===== SUITE COMPLETE: %u passed, %u failed (of %u) =====\r\n",
		       g_suite.passed, g_suite.failed, g_test_count);
		HAL_Delay(200);
		while (1) {
		}
	}

	HAL_Delay(100);
	NVIC_SystemReset();
}

void test_harness_pass(void)
{
	test_suite_finish(1, NULL);
}

void test_harness_fail(const char *reason)
{
	test_suite_finish(0, reason);
}

void test_harness_assert_eq_impl(intmax_t a, const char *a_str, intmax_t b, const char *b_str)
{
	if (a != b) {
		printf("__AUTOTEST__$FAIL$Assertion failed: %s == %s\r\n", a_str, b_str);
		test_suite_finish(0, "assertion failed");
	}
}

void test_harness_assert_neq_impl(intmax_t a, const char *a_str, intmax_t b, const char *b_str)
{
	if (a == b) {
		printf("__AUTOTEST__$FAIL$Assertion failed: %s != %s\r\n", a_str, b_str);
		test_suite_finish(0, "assertion failed");
	}
}

void test_suite_run(void)
{
	if (g_suite.magic != TEST_SUITE_MAGIC) {
		g_suite.magic = TEST_SUITE_MAGIC;
		g_suite.index = 0;
		g_suite.passed = 0;
		g_suite.failed = 0;
		g_suite.attempted = 0;
		printf("\r\n===== ECE350 D1 Test Suite (%u tests) =====\r\n", g_test_count);
	}

	/* A reset arrived while a test was still running: it hung or crashed.
	 * Mark it failed and move on instead of retrying forever. */
	if (g_suite.attempted && g_suite.index < g_test_count) {
		printf("[FAIL] %s: hung or crashed (reset while running)\r\n",
		       g_test_names[g_suite.index]);
		printf("__AUTOTEST__$FAIL$hung\r\n");
		g_suite.failed++;
		g_suite.index++;
		g_suite.attempted = 0;
	}

	if (g_suite.index >= g_test_count) {
		printf("\r\n===== SUITE COMPLETE: %u passed, %u failed (of %u) =====\r\n",
		       g_suite.passed, g_suite.failed, g_test_count);
		while (1) {
		}
	}

	printf("\r\n\r\n__AUTOTEST__$START$%s (%u/%u)\r\n",
	       g_test_names[g_suite.index], g_suite.index + 1, g_test_count);

	g_suite.attempted = 1;
	g_test_fns[g_suite.index]();

	test_suite_finish(0, "test returned unexpectedly");
}
