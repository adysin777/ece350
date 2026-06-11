/*
 * Unified D1 test suite — 19 tests in one binary.
 * Called from main.c via test_suite_run().
 */

#include "common.h"
#include "k_task.h"
#include "kernel_svc.h"
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

/* ===================== Test 13: Kernel TCB copy ===================== */
static void test13_task1(void *args)
{
	(void)args;

	printf("PASS: kernel kept its own copy of TCB\r\n");
	autotest_pass();

	while (1) {
		/* no yield — TaskFail must never run if kernel copied Task1 correctly */
	}
}

static void test13_task_fail(void *args)
{
	(void)args;
	autotest_fail("Task1 configuration was lost");
	while (1) {
	}
}

static void test13(void)
{
	osKernelInit();

	TCB mytask = {
		.ptask = test13_task1,
		.stack_high = 0,
		.stack_size = STK_400,
		.state = DORMANT,
		.tid = 0xff,
	};

	autotest_assert_eq(osCreateTask(&mytask), RTX_OK);
	autotest_assert(mytask.tid > 0 && mytask.tid < MAX_TASKS);
	autotest_assert_neq(mytask.stack_high, 0);

	TCB task_readback = {
		.ptask = NULL,
		.stack_high = 0,
		.stack_size = 0,
		.state = DORMANT,
		.tid = 0xff,
	};

	autotest_assert_eq(osTaskInfo(mytask.tid, &task_readback), RTX_OK);
	autotest_assert_eq(task_readback.tid, mytask.tid);
	autotest_assert_neq(task_readback.stack_high, 0);
	autotest_assert_eq((U32)task_readback.ptask, (U32)test13_task1);
	autotest_assert_eq(task_readback.stack_size, STK_400);
	autotest_assert_eq(task_readback.state, READY);

	/* Reuse the same struct — kernel must still start Task1, not TaskFail */
	mytask.ptask = test13_task_fail;
	autotest_assert_eq(osCreateTask(&mytask), RTX_OK);

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Test 14: Yield / exit / recreate ===================== */
static int test14_i_test;

static void test14_task1(void *args)
{
	(void)args;

	test14_i_test++;
	osYield();

	TCB mytask = {
		.ptask = test14_task1,
		.stack_high = 0,
		.stack_size = STK_400,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&mytask), RTX_OK);
	osTaskExit();
	autotest_fail("task exit returned");
}

static void test14_task2(void *args)
{
	(void)args;

	int last = 0;

	while (1) {
		printf("Back to you %d\r\n", test14_i_test);
		/* Task1 recreates itself then exits; round robin runs us before
		 * the new instance gets a turn, so the counter advances every
		 * OTHER one of our turns: same value or +1, never more/less. */
		autotest_assert(test14_i_test == last || test14_i_test == last + 1);
		last = test14_i_test;

		if (test14_i_test >= 10) {
			autotest_pass();
		}

		osYield();
	}
}

static void test14(void)
{
	test14_i_test = 0;
	osKernelInit();

	TCB tcb1 = {
		.ptask = test14_task1,
		.stack_high = 0,
		.stack_size = STK_400,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&tcb1), RTX_OK);

	TCB tcb2 = {
		.ptask = test14_task2,
		.stack_high = 0,
		.stack_size = STK_400,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&tcb2), RTX_OK);

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Test 15: lab1_rr (partner) ===================== */
/* Round robin + per-task stack integrity + task continuity. */
static const char test15_sentinel_data[] =
	"In computing, a linear-feedback shift register (LFSR) is a shift "
	"register whose input bit is a linear function of its previous state.";
static int test15_runs[3];

static void test15_task(void *args)
{
	(void)args;

	char sentinel[128];
	memcpy(sentinel, test15_sentinel_data, sizeof(sentinel));
	int run_count = 0;

	while (1) {
		task_t t = osGetTID();
		/* partner's memcmp was missing the length arg — fixed here */
		autotest_assert_eq(memcmp(sentinel, test15_sentinel_data,
					  sizeof(sentinel)), 0);
		run_count++;
		test15_runs[t - 1]++;
		autotest_assert_eq(run_count, test15_runs[t - 1]);

		if (t == 3 && test15_runs[2] == 16) {
			autotest_assert_eq(test15_runs[0], 16);
			autotest_assert_eq(test15_runs[1], 16);
			autotest_pass();
		}

		osYield();
	}
}

static void test15(void)
{
	test15_runs[0] = test15_runs[1] = test15_runs[2] = 0;
	osKernelInit();

	for (int i = 0; i < 3; i++) {
		TCB tcb = {
			.ptask = test15_task,
			.stack_high = 0,
			.stack_size = STK_800,
			.state = READY,
			.tid = (task_t)(i + 1),
		};
		autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
		autotest_assert_neq(tcb.tid, TID_NULL);
	}

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Test 16: lab1_max_test (partner) ===================== */
/* Max tasks, create failure when full, exit, TID + stack reuse.
 * Partner used stack_size 0x100 — below our STACK_SIZE min (0x200), so
 * STK_400 is used instead. */
static int test16_total_runs;
static int test16_task_runs[MAX_TASKS];
static const task_t test16_random_tid = 4; /* chosen by fair dice roll */
static U32 test16_exited_stack_hi;

static void test16_dummy(void *args)
{
	(void)args;
	while (1) {
		osYield();
	}
}

static void test16_task(void *args)
{
	(void)args;

	task_t t = osGetTID();
	test16_task_runs[t]++;
	test16_total_runs++;
	printf("Task %u hello\r\n", t);
	osYield();

	if (t == test16_random_tid) {
		/* Task 4 yields mid-body; by the time it resumes, tasks 5-15 have
		 * finished their first run and 1-3 may have started a second.
		 * Require one full round (every slot ran at least once), not
		 * exactly one run each. */
		autotest_assert(test16_total_runs >= MAX_TASKS - 1);
		for (int i = 1; i < MAX_TASKS; i++) {
			autotest_assert(test16_task_runs[i] >= 1);
		}
		TCB tcb;
		autotest_assert_eq(osTaskInfo(t, &tcb), RTX_OK);
		test16_exited_stack_hi = tcb.stack_high;
		osTaskExit();
		autotest_fail("task exit returned");
	}
	osYield();

	if (t == test16_random_tid + 1) {
		TCB tcb = {
			.ptask = test16_dummy,
			.stack_high = 0,
			.stack_size = STK_400,
			.state = DORMANT,
			.tid = TID_NULL,
		};
		autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
		autotest_assert_eq(tcb.stack_high, test16_exited_stack_hi);
		autotest_assert_eq(tcb.tid, test16_random_tid);
		autotest_pass();
	}

	/* partner's tasks fell off the end; ours must never return */
	while (1) {
		osYield();
	}
}

static void test16(void)
{
	test16_total_runs = 0;
	for (int i = 0; i < MAX_TASKS; i++) {
		test16_task_runs[i] = 0;
	}
	test16_exited_stack_hi = 0;

	osKernelInit();

	for (int i = 1; i < MAX_TASKS; i++) {
		TCB tcb = {
			.ptask = test16_task,
			.stack_high = 0,
			.stack_size = STK_400,
			.state = READY,
			.tid = (task_t)i,
		};
		autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
	}

	TCB extra = {
		.ptask = test16_task,
		.stack_high = 0,
		.stack_size = STK_400,
		.state = READY,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&extra), RTX_ERR);

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Test 17: lab1_init (partner) ===================== */
/* osTaskInfo on the currently running task: state == RUNNING, fields match. */
static int test17_task1_ran;

static void test17_task1(void *args)
{
	(void)args;

	task_t task = osGetTID();
	TCB tcb;
	autotest_assert_eq(osTaskInfo(task, &tcb), RTX_OK);
	autotest_assert_eq(tcb.tid, task);
	autotest_assert_eq((U32)tcb.ptask, (U32)test17_task1);
	autotest_assert(tcb.stack_high);
	autotest_assert_eq(tcb.state, RUNNING);
	test17_task1_ran = 1;

	while (1) {
		osYield();
	}
}

static void test17_task2(void *args)
{
	(void)args;

	task_t task = osGetTID();
	TCB tcb;
	autotest_assert_eq(osTaskInfo(task, &tcb), RTX_OK);
	autotest_assert_eq(tcb.tid, task);
	autotest_assert_eq((U32)tcb.ptask, (U32)test17_task2);
	autotest_assert(tcb.stack_high);
	autotest_assert_eq(tcb.state, RUNNING);
	autotest_assert(test17_task1_ran);
	autotest_pass();

	while (1) {
		osYield();
	}
}

static void test17(void)
{
	test17_task1_ran = 0;
	osKernelInit();

	/* partner's version declared `tcb` twice in one scope — fixed here */
	TCB tcb1 = {
		.ptask = test17_task1,
		.stack_high = 0,
		.stack_size = STK_800,
		.state = READY,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&tcb1), RTX_OK);
	autotest_assert_neq(tcb1.tid, TID_NULL);

	TCB tcb2 = {
		.ptask = test17_task2,
		.stack_high = 0,
		.stack_size = STK_800,
		.state = READY,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&tcb2), RTX_OK);
	autotest_assert_neq(tcb2.tid, TID_NULL);

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Test 18: basic_svc (partner) ===================== */
/* SVC preserves caller registers. Partner used their own `svc 17` add
 * syscall + kstate internals; adapted to our SVC_GET_TID, whose result
 * (r0) must equal the first created task's TID == 1. */
static void test18_task(void *args)
{
	(void)args;

	printf("Hello from task\r\n");
	U32 result = 0xdddddddd;
	int corruptions = -1;

	__asm volatile(
		"mov r2, #0x22222222\n"
		"mov r3, #0x33333333\n"
		"mov r6, #0x66666666\n"
		"svc %[svcnum]\n"
		"mov r12, #0\n"
		"cmp r2, #0x22222222\n"
		"it ne\n"
		"addne r12, #1\n"
		"cmp r3, #0x33333333\n"
		"it ne\n"
		"addne r12, #1\n"
		"cmp r6, #0x66666666\n"
		"it ne\n"
		"addne r12, #1\n"
		"mov %[res], r0\n"
		"mov %[cor], r12\n"
		: [res] "=r"(result), [cor] "=r"(corruptions)
		: [svcnum] "I"(SVC_GET_TID)
		: "r0", "r1", "r2", "r3", "r6", "r12", "memory");

	printf("Got %d corrupted regs\r\n", corruptions);
	printf("Got result %u\r\n", result);
	autotest_assert_eq(corruptions, 0);
	autotest_assert_eq(result, 1);
	autotest_pass();

	while (1) {
		osYield();
	}
}

static void test18(void)
{
	osKernelInit();

	TCB tcb = {
		.ptask = test18_task,
		.stack_high = 0,
		.stack_size = STK_400,
		.state = DORMANT,
		.tid = TID_NULL,
	};
	autotest_assert_eq(osCreateTask(&tcb), RTX_OK);
	autotest_assert_eq(tcb.tid, (task_t)1);

	osKernelStart();
	autotest_fail("kernel start returned");
}

/* ===================== Suite dispatch table ===================== */
void (*const g_test_fns[])(void) = {
	test00, test01, test02, test03, test04, test05, test06,
	test07, test08, test09, test10, test11, test12, test13, test14,
	test15, test16, test17, test18,
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
	"test13_tcb_copy",
	"test14_back_to_you",
	"test15_lab1_rr",
	"test16_lab1_max",
	"test17_lab1_init",
	"test18_basic_svc",
};

const unsigned g_test_count = sizeof(g_test_fns) / sizeof(g_test_fns[0]);
