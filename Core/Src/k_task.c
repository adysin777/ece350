/*
 * k_task.c
 *
 * Public RTX task API. Each function issues an SVC; implementation lives in
 * SVC_Handler_Main (kernel.c). Keep SVC numbers in sync with the dispatcher.
 *
 * Args must be in r0/r1 before svc (AAPCS). Explicit mov avoids GCC putting
 * the return-value register in r0 instead of the task pointer.
 */

#include "k_task.h"
#include "kernel_svc.h"

void osKernelInit(void)
{
	__asm volatile("svc %0" :: "I"(SVC_KERNEL_INIT) : "memory");
}

int osCreateTask(TCB *task)
{
	int ret;
	__asm volatile(
		"mov r0, %1\n\t"
		"svc %2\n\t"
		"mov %0, r0"
		: "=r"(ret)
		: "r"(task), "I"(SVC_CREATE_TASK)
		: "r0", "memory");
	return ret;
}

int osKernelStart(void)
{
	int ret;
	__asm volatile(
		"svc %1\n\t"
		"mov %0, r0"
		: "=r"(ret)
		: "I"(SVC_KERNEL_START)
		: "r0", "memory");
	return ret;
}

void osYield(void)
{
	__asm volatile("svc %0" :: "I"(SVC_YIELD) : "memory");
}

int osTaskInfo(task_t TID, TCB *task_copy)
{
	int ret;
	__asm volatile(
		"mov r0, %1\n\t"
		"mov r1, %2\n\t"
		"svc %3\n\t"
		"mov %0, r0"
		: "=r"(ret)
		: "r"(TID), "r"(task_copy), "I"(SVC_TASK_INFO)
		: "r0", "r1", "memory");
	return ret;
}

task_t osGetTID(void)
{
	task_t ret;
	__asm volatile(
		"svc %1\n\t"
		"mov %0, r0"
		: "=r"(ret)
		: "I"(SVC_GET_TID)
		: "r0", "memory");
	return ret;
}

int osTaskExit(void)
{
	int ret;
	__asm volatile(
		"svc %1\n\t"
		"mov %0, r0"
		: "=r"(ret)
		: "I"(SVC_TASK_EXIT)
		: "r0", "memory");
	return ret;
}
