/*
 * user_tasks.h
 *
 *  Created on: 2020. jún. 25.
 *      Author: epagris
 */

#ifndef TASKS_USER_TASKS_H_
#define TASKS_USER_TASKS_H_

#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "utils.h"

#include "flexptp/task_ptp.h"

//extern uint32_t g_ui32SysClock;

// TASK REGISTRATION
void reg_task_eth(); // register ETH task
//void reg_task_ptp(); // register PTP task
//void unreg_task_ptp();  // unregister PTP task
void reg_task_cli(); // register CLI task
void unreg_task_cli(); // unregister CLI task
void reg_task_audio(); // register audio task

// HARDWARE INITIALIZATION
void hwinit_task_audio();

//bool task_ptp_is_operating() ; // query ptp task operating state

#endif /* TASKS_USER_TASKS_H_ */
