/*
 * Unified D1 test suite — all 13 course tests in one binary.
 * Called from main.c via test_suite_run().
 */

#include "common.h"
#include "k_task.h"
#include "helper.h"
#include "test_harness.h"
#include "main.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define STK_400 0x400
#define STK_800 0x800

/* ===================== Test 0: Init and print ===================== */
static void test00(void)
{
	osKernelInit();
	printf("Hello from the ECE350 kernel\r\n");
	autotest_pass();
}

/* ===================== Test 1: Create task ===================== */
static void test01_idle(void *args)
{
	(void)args;
	while (1) {
		osYield();
	}
}

static void test01(void)
{
	osKernelInit();

	TCB tcb = {
		.ptask = test01_idle,
		.stack_high = 0,
		.stack_size = STK_400,
		.state = DORMANT,
		.tid = TID_NULL,
	};

	autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
	autotest_assert_neq(tcb.tid, TID_NULL);
	autotest_assert(tcb.tid < MAX_TASKS);

	TCB info = {0};
	autotest_assert_eq(osTaskInfo(tcb.tid, &info), RTX_OK);
	autotest_assert_eq(info.tid, tcb.tid);
	autotest_assert_eq((U32)info.ptask, (U32)test01_idle);
	autotest_assert(info.stack_high != 0);
	autotest_assert_eq(info.stack_size, STK_400);
	autotest_assert_eq(info.state, READY);

	autotest_pass();
}

/* ===================== Test 2: Kernel start ===================== */
static int test02_t1_runs;
static int test02_t2_runs;

static void test02_task1(void *args)
{
	(void)args;
	autotest_assert_eq(test02_t2_runs, 0);
	test02_t1_runs++;
	while (1) {
		osYield();
	}
}

static void test02_task2(void *args)
{
	(void)args;
	test02_t2_runs++;
	autotest_assert_eq(test02_t1_runs, 1);
	autotest_pass();
	while (1) {
		osYield();
	}
}

static void test02(void)
{
	test02_t1_runs = 0;
	test02_t2_runs = 0;
	osKernelInit();

	TCB tcb1 = {
		.ptask = test02_task1,
		.stack_high = 0,
		.stack_size = STK_400,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&tcb1), RTX_OK);

	TCB tcb2 = {
		.ptask = test02_task2,
		.stack_high = 0,
		.stack_size = STK_400,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&tcb2), RTX_OK);

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Test 3: Round robin ===================== */
static int test03_turn;

static void test03_task(void *args)
{
	(void)args;

	while (1) {
		task_t t = osGetTID();
		autotest_assert_eq(t, (task_t)((test03_turn % 3) + 1));
		test03_turn++;

		if (test03_turn >= 3 * 16) {
			autotest_assert_eq(t, (task_t)3);
			autotest_pass();
		}

		osYield();
	}
}

static void test03(void)
{
	test03_turn = 0;
	osKernelInit();

	for (int i = 0; i < 3; i++) {
		TCB tcb = {
			.ptask = test03_task,
			.stack_high = 0,
			.stack_size = STK_400,
			.state = DORMANT,
			.tid = TID_NULL,
		};
		autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
		autotest_assert_eq(tcb.tid, (task_t)(i + 1));
	}

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Test 4: Task exit ===================== */
static int test04_c1, test04_c2, test04_c3;

static void test04_task(void *args)
{
	(void)args;

	while (1) {
		task_t t = osGetTID();

		if (t == 1) {
			test04_c1++;
		} else if (t == 2) {
			autotest_assert_eq(test04_c1, test04_c2 + 1);
			test04_c2++;
			if (test04_c2 == 5) {
				osTaskExit();
				autotest_fail("task exit returned");
			}
		} else if (t == 3) {
			test04_c3++;
			autotest_assert_eq(test04_c1, test04_c3);
			autotest_assert_eq(test04_c2, (test04_c3 <= 5) ? test04_c3 : 5);

			if (test04_c3 == 20) {
				autotest_assert_eq(test04_c1, 20);
				autotest_assert_eq(test04_c2, 5);
				autotest_pass();
			}
		}

		osYield();
	}
}

static void test04(void)
{
	test04_c1 = test04_c2 = test04_c3 = 0;
	osKernelInit();

	for (int i = 0; i < 3; i++) {
		TCB tcb = {
			.ptask = test04_task,
			.stack_high = 0,
			.stack_size = STK_400,
			.state = DORMANT,
			.tid = TID_NULL,
		};
		autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
	}

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Test 5: Stack preservation ===================== */
#define TEST05_SENTINEL_LEN 96

static const char test05_s1[TEST05_SENTINEL_LEN] =
	"Task one keeps this string on its own stack across yields.";
static const char test05_s2[TEST05_SENTINEL_LEN] =
	"Task two also parks a different string on its private stack.";

static int test05_r1, test05_r2;

static void test05_task(void *args)
{
	(void)args;

	task_t t = osGetTID();
	const char *src = (t == 1) ? test05_s1 : test05_s2;
	char local[TEST05_SENTINEL_LEN];
	memcpy(local, src, TEST05_SENTINEL_LEN);
	int my_runs = 0;

	while (1) {
		autotest_assert_eq(memcmp(local, src, TEST05_SENTINEL_LEN), 0);
		my_runs++;
		if (t == 1) {
			test05_r1 = my_runs;
		} else {
			test05_r2 = my_runs;
		}

		if (t == 2 && my_runs == 16) {
			autotest_assert_eq(test05_r1, 16);
			autotest_pass();
		}

		osYield();
	}
}

static void test05(void)
{
	test05_r1 = test05_r2 = 0;
	osKernelInit();

	for (int i = 0; i < 2; i++) {
		TCB tcb = {
			.ptask = test05_task,
			.stack_high = 0,
			.stack_size = STK_400,
			.state = DORMANT,
			.tid = TID_NULL,
		};
		autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
	}

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Test 6: Task continuity ===================== */
static int test06_global[2];

static void test06_task(void *args)
{
	(void)args;

	task_t t = osGetTID();
	int counter = 0;

	while (1) {
		counter++;
		test06_global[t - 1]++;
		autotest_assert_eq(counter, test06_global[t - 1]);

		if (t == 2 && counter == 16) {
			autotest_assert_eq(test06_global[0], 16);
			autotest_pass();
		}

		osYield();
	}
}

static void test06(void)
{
	test06_global[0] = test06_global[1] = 0;
	osKernelInit();

	for (int i = 0; i < 2; i++) {
		TCB tcb = {
			.ptask = test06_task,
			.stack_high = 0,
			.stack_size = STK_400,
			.state = DORMANT,
			.tid = TID_NULL,
		};
		autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
	}

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Test 7: MAX_TASKS-1 user tasks ===================== */
static int test07_runs[MAX_TASKS];

static void test07_task(void *args)
{
	(void)args;

	while (1) {
		task_t t = osGetTID();
		test07_runs[t]++;

		if (t == MAX_TASKS - 1 && test07_runs[t] == 4) {
			for (int i = 1; i < MAX_TASKS; i++) {
				autotest_assert_eq(test07_runs[i], 4);
			}
			autotest_pass();
		}

		osYield();
	}
}

static void test07(void)
{
	for (int i = 0; i < MAX_TASKS; i++) {
		test07_runs[i] = 0;
	}

	osKernelInit();

	for (int i = 1; i < MAX_TASKS; i++) {
		TCB tcb = {
			.ptask = test07_task,
			.stack_high = 0,
			.stack_size = STK_400,
			.state = DORMANT,
			.tid = TID_NULL,
		};
		autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
		autotest_assert_eq(tcb.tid, (task_t)i);
	}

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Test 8: Too many tasks ===================== */
static void test08_idle(void *args)
{
	(void)args;
	while (1) {
		osYield();
	}
}

static void test08(void)
{
	osKernelInit();

	for (int i = 1; i < MAX_TASKS; i++) {
		TCB tcb = {
			.ptask = test08_idle,
			.stack_high = 0,
			.stack_size = STK_400,
			.state = DORMANT,
			.tid = TID_NULL,
		};
		autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
	}

	TCB extra = {
		.ptask = test08_idle,
		.stack_high = 0,
		.stack_size = STK_400,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&extra), RTX_ERR);

	autotest_pass();
}

/* ===================== Test 9: Stack too big ===================== */
static void test09_idle(void *args)
{
	(void)args;
	while (1) {
		osYield();
	}
}

static void test09(void)
{
	osKernelInit();

	TCB huge = {
		.ptask = test09_idle,
		.stack_high = 0,
		.stack_size = 0x1000,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&huge), RTX_ERR);

	TCB ok = {
		.ptask = test09_idle,
		.stack_high = 0,
		.stack_size = STK_800,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&ok), RTX_OK);

	autotest_pass();
}

/* ===================== Test 10: Stack reuse ===================== */
static const task_t test10_chosen = 4;
static U32 test10_stack_hi;

static void test10_dummy(void *args)
{
	(void)args;
	while (1) {
		osYield();
	}
}

static void test10_task(void *args)
{
	(void)args;

	while (1) {
		task_t t = osGetTID();

		if (t == test10_chosen) {
			osTaskExit();
			autotest_fail("task exit returned");
		}

		if (t == test10_chosen + 1) {
			TCB tcb = {
				.ptask = test10_dummy,
				.stack_high = 0,
				.stack_size = STK_800,
				.state = DORMANT,
				.tid = TID_NULL,
			};
			autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
			autotest_assert_eq(tcb.tid, test10_chosen);
			autotest_assert_eq(tcb.stack_high, test10_stack_hi);
			autotest_pass();
		}

		osYield();
	}
}

static void test10(void)
{
	osKernelInit();

	int created = 0;
	while (1) {
		TCB tcb = {
			.ptask = test10_task,
			.stack_high = 0,
			.stack_size = STK_800,
			.state = DORMANT,
			.tid = TID_NULL,
		};
		if (osCreateTask(&tcb) != RTX_OK) {
			break;
		}
		created++;
	}

	autotest_assert(created >= (int)test10_chosen + 1);

	TCB info = {0};
	autotest_assert_eq(osTaskInfo(test10_chosen, &info), RTX_OK);
	test10_stack_hi = info.stack_high;

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Test 11: Robustness (500 lives) ===================== */
static int test11_lives;

static void test11_phoenix(void *args)
{
	(void)args;

	test11_lives++;
	if (test11_lives >= 500) {
		autotest_pass();
	}

	TCB tcb = {
		.ptask = test11_phoenix,
		.stack_high = 0,
		.stack_size = STK_400,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	if (osCreateTask(&tcb) != RTX_OK) {
		autotest_fail("recreate failed");
	}

	osTaskExit();
	autotest_fail("task exit returned");
}

static void test11(void)
{
	test11_lives = 0;
	osKernelInit();

	TCB tcb = {
		.ptask = test11_phoenix,
		.stack_high = 0,
		.stack_size = STK_400,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&tcb), RTX_OK);

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Test 12: SVC / BASEPRI ===================== */
static void test12_task(void *args)
{
	(void)args;

	/* Kernel is running; clear the mask so normal scheduling works. */
	__set_BASEPRI(0);
	__DSB();
	__ISB();

	autotest_pass();

	while (1) {
		osYield();
	}
}

static void test12(void)
{
	osKernelInit();

	/* Mask configurable IRQs before create/start. SVC (priority 0) still
	 * works; PendSV is blocked only while in thread mode on main. */
	__set_BASEPRI(1 << (8 - __NVIC_PRIO_BITS));

	TCB tcb = {
		.ptask = test12_task,
		.stack_high = 0,
		.stack_size = STK_400,
		.state = DORMANT,
		.tid = TID_NULL,
	};

	autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
	autotest_assert_neq(tcb.tid, TID_NULL);

	/* Pends PendSV, but BASEPRI holds it — control returns to main */
	autotest_assert_eq(osKernelStart(), RTX_OK);

	/* Unmask: the pending context switch fires and the task passes */
	__set_BASEPRI(0);
	__DSB();
	__ISB();

	autotest_fail("task never ran");
}

/* ===================== Suite dispatch table ===================== */
void (*const g_test_fns[])(void) = {
	test00, test01, test02, test03, test04, test05, test06,
	test07, test08, test09, test10, test11, test12,
};

const char *const g_test_names[] = {
	"test0_init_print",
	"test1_create_task",
	"test2_kernel_start",
	"test3_round_robin",
	"test4_task_exit",
	"test5_stack_preservation",
	"test6_task_continuity",
	"test7_max_tasks",
	"test8_err_too_many_tasks",
	"test9_err_stack_too_big",
	"test10_stack_reuse",
	"test11_robustness",
	"test12_svc",
};

const unsigned g_test_count = sizeof(g_test_fns) / sizeof(g_test_fns[0]);
