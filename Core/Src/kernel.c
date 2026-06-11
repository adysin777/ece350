/*
 * kernel.c
 *
 * RTX kernel: SVC_Handler_Main and internal k_* implementations.
 * Public API lives in k_task.c (os* -> svc only).
 */

#include "common.h"
#include "kernel_svc.h"
#include "k_task.h"      /* osYield for the NULL task */
#include "stm32f4xx.h"   /* SCB, NVIC, __set_PSP */

#define MAX_TASK_STACK 0x800

/* --- Kernel state (extend as your design requires) --- */
static TCB tasks[MAX_TASKS];
/* AAPCS requires 8-byte aligned stacks */
static uint8_t task_stacks[MAX_TASKS][MAX_TASK_STACK] __attribute__((aligned(8)));
static task_t current_tid;
static U8 kernel_initialized;
static U8 kernel_running;

/* --- Internal handlers (real logic goes here) --- */
static void k_kernel_init(void);
static int k_create_task(TCB *task);
static int k_kernel_start(void);
static void k_yield(void);
static int k_task_info(task_t tid, TCB *task_copy);
static task_t k_get_tid(void);
static int k_task_exit(void);
static void init_task_stack(TCB *t);
static task_t scheduler_pick_next(void);
static void pend_pendsv(void);

/* Called from PendSV_Handler (os_cpu.s) — must NOT be static */
U32 PendSV_C_Handler(U32 psp_r4);

static uint8_t get_svc_number(unsigned int *sp)
{
	/* Stacked PC points after the svc instruction (Thumb, 2 bytes). */
	return ((uint8_t *)sp[6])[-2];
}

void SVC_Handler_Main(unsigned int *stack_pointer)
{
	uint8_t svc = get_svc_number(stack_pointer);

	switch (svc) {
	case SVC_KERNEL_INIT:
		k_kernel_init();
		break;

	case SVC_CREATE_TASK:
		stack_pointer[0] = (unsigned int)k_create_task((TCB *)stack_pointer[0]);
		break;

	case SVC_KERNEL_START:
		stack_pointer[0] = (unsigned int)k_kernel_start();
		break;

	case SVC_YIELD:
		k_yield();
		break;

	case SVC_TASK_INFO:
		stack_pointer[0] = (unsigned int)k_task_info(
			(task_t)stack_pointer[0],
			(TCB *)stack_pointer[1]);
		break;

	case SVC_GET_TID:
		stack_pointer[0] = (unsigned int)k_get_tid();
		break;

	case SVC_TASK_EXIT:
		stack_pointer[0] = (unsigned int)k_task_exit();
		break;

	default:
		stack_pointer[0] = (unsigned int)RTX_ERR;
		break;
	}
}

/* NULL task: runs only when no user task is READY. Never exits. */
static void null_task(void *args)
{
	(void)args;
	while (1) {
		osYield();
	}
}

static void k_kernel_init(void)
{
	kernel_running = 0;
	current_tid = TID_NULL;

	for (int i = 0; i < MAX_TASKS; i++) {
		tasks[i].state = DORMANT;
		tasks[i].tid = i;
		tasks[i].ptask = NULL;
		tasks[i].stack_high = 0;
		tasks[i].stack_size = 0;
		tasks[i].saved_psp = 0;
	}

	/* Slot 0 is the NULL task — always READY when not running */
	tasks[TID_NULL].ptask = null_task;
	tasks[TID_NULL].stack_size = STACK_SIZE;
	tasks[TID_NULL].stack_high = (U32)&task_stacks[TID_NULL][MAX_TASK_STACK];
	init_task_stack(&tasks[TID_NULL]);
	tasks[TID_NULL].state = READY;

	/* PendSV must be the lowest priority so context switches run last */
	NVIC_SetPriority(PendSV_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);

	kernel_initialized = 1;

	return;
}

static int k_create_task(TCB *task)
{
	if (!kernel_initialized || task == NULL || task->ptask == NULL) {
		return RTX_ERR;
	}
		
	if (task->stack_size < STACK_SIZE || (task->stack_size & 7)) {
		return RTX_ERR;
	}

	/* Slot 0 (TID_NULL) reserved for the NULL task — user tasks start at 1 */
	int slot = -1;
	for (int i = 1; i < MAX_TASKS; i++) {
		if (tasks[i].state == DORMANT) {
			slot = i;
			break;
		}
	}

	if (slot < 0) {
		return RTX_ERR;
	}

	if (task->stack_size > MAX_TASK_STACK) {
		return RTX_ERR;
	}

	tasks[slot].ptask = task->ptask;
	tasks[slot].stack_size = task->stack_size;
	tasks[slot].tid = slot;
	tasks[slot].stack_high = (U32)&task_stacks[slot][MAX_TASK_STACK];
	init_task_stack(&tasks[slot]);

	tasks[slot].state = READY;
	task->tid = slot;
	task->stack_high = tasks[slot].stack_high;

	return RTX_OK;
}

static int k_kernel_start(void)
{
	if (!kernel_initialized || kernel_running)
		return RTX_ERR;

	/* Need at least one READY user task to run */
	task_t first = scheduler_pick_next();
	if (first == TID_NULL)
		return RTX_ERR;

	kernel_running = 1;

	/* PSP = 0 tells PendSV "nothing to save" on this first switch */
	__set_PSP(0);
	pend_pendsv();

	/* SVC returns to main, but PendSV tail-chains immediately and
	 * switches to the first task — main never resumes. */
	return RTX_OK;
}

static void k_yield(void)
{
	if (!kernel_running)
		return;

	/* Current task is marked READY inside PendSV_C_Handler */
	pend_pendsv();
}

static int k_task_info(task_t tid, TCB *task_copy)
{
	if (!kernel_initialized || task_copy == NULL || tid >= MAX_TASKS)
		return RTX_ERR;

	if (tasks[tid].state == DORMANT)
		return RTX_ERR;

	*task_copy = tasks[tid];
	return RTX_OK;
}

static task_t k_get_tid(void)
{
	if (!kernel_running)
		return (task_t)TID_NULL;
	return current_tid;
}

static int k_task_exit(void)
{
	if (!kernel_running || current_tid == TID_NULL ||
	    tasks[current_tid].state != RUNNING)
		return RTX_ERR;

	tasks[current_tid].state = DORMANT;
	pend_pendsv();

	/* PendSV switches away right after the SVC returns; the exiting
	 * task never runs again, so this value is never observed. */
	return RTX_OK;
}

static void init_task_stack(TCB *t) {
	/* Software block (R4-R11 + EXC_RETURN, 9 words) + hardware frame (8 words) */
	U32 *sp = (U32 *)(t->stack_high - 17 * 4);
    /* R4–R11 (first 8 words) — zeros OK */
    for (int i = 0; i < 8; i++)
        sp[i] = 0;
    sp[8]  = 0xFFFFFFFD;               /* EXC_RETURN: Thread mode, PSP, no FP frame */
    /* Hardware frame (next 8 words) */
    sp[9]  = 0;                        /* R0 */
    sp[10] = 0;                        /* R1 */
    sp[11] = 0;                        /* R2 */
    sp[12] = 0;                        /* R3 */
    sp[13] = 0;                        /* R12 */
    sp[14] = 0;                        /* LR — tasks must not return */
    sp[15] = (U32)t->ptask;            /* PC — task entry */
    sp[16] = 0x01000000;               /* xPSR */
    t->saved_psp = (U32)sp;
}

static task_t scheduler_pick_next(void) {
	/* Round-robin over user tasks only; NULL task is the idle fallback */
	for (int i = 1; i <= MAX_TASKS; i++) {
		task_t tid = (current_tid + i) % MAX_TASKS;
		if (tid == TID_NULL) {
			continue;
		}
		if (tasks[tid].state == READY) {
			return tid;
		}
	}

	return TID_NULL;
}

static void pend_pendsv(void)
{
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
	__DSB();
	__ISB();
}

/*
 * Called from PendSV_Handler (os_cpu.s).
 * psp_r4 = address of the outgoing task's saved R4-R11 block,
 *          or 0 if there is nothing to save (first switch).
 * Returns the incoming task's saved R4-R11 block address.
 */
U32 PendSV_C_Handler(U32 psp_r4)
{
	/* Save the outgoing task and put it back in the READY pool.
	 * Skipped on first switch (psp_r4 == 0) and for exited tasks
	 * (state already DORMANT). */
	if (psp_r4 != 0 && tasks[current_tid].state == RUNNING) {
		tasks[current_tid].saved_psp = psp_r4;
		tasks[current_tid].state = READY;
	}

	task_t next = scheduler_pick_next();
	if (tasks[next].state != READY) {
		/* No READY task (e.g. last task exited). Nothing valid to
		 * load — resume whatever stack we came from. D1 tests are
		 * expected to always keep at least one runnable task. */
		return psp_r4;
	}

	current_tid = next;
	tasks[next].state = RUNNING;

	return tasks[next].saved_psp;
}
