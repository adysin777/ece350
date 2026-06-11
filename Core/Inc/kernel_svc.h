/*
 * kernel_svc.h
 *
 * SVC immediate numbers for k_task.c wrappers and SVC_Handler_Main dispatch.
 * Keep in sync: one number per public os* API.
 */

#ifndef INC_KERNEL_SVC_H_
#define INC_KERNEL_SVC_H_

#define SVC_KERNEL_INIT    0
#define SVC_CREATE_TASK    1
#define SVC_KERNEL_START   2
#define SVC_YIELD          3
#define SVC_TASK_INFO      4
#define SVC_GET_TID        5
#define SVC_TASK_EXIT      6

#endif /* INC_KERNEL_SVC_H_ */
