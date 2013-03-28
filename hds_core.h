
#ifndef HDS_CORE_H_
#define HDS_CORE_H_

#include "hds_common.h"
#define SMALLEST_TIME_QUANTUM 1
/**
 * @struct hds_resource_state
 * @brief Denotes the current state of resources available for further allocation.
 */
struct hds_resource_state_t{
	int avail_memory;
	int avail_scanner;
	int avail_printer;
	pthread_mutex_t avail_resource_mutex;
}hds_resource_state;

struct process_queue_t{
	unsigned long int arrival_time;
	int priority;
	int cpu_req;
	int memory_req;
	int printer_req;
	int scanner_req;
	int pid; /**< A non zero pid would mean it has not yet run for once. Once a process runs
	 	 	 	 	 	 it will have a valid pid. It could be in either suspended/running state.*/
	struct process_queue_t *next;
};

struct hds_core_state_t{
	struct process_queue_t *rtq,*rtq_last;
	struct process_queue_t *p1q,*p1q_last;
	struct process_queue_t *p2q,*p2q_last;
	struct process_queue_t *p3q,*p3q_last;

	//mutexes for ensuring exclusive access to the process queues
	pthread_mutex_t rtq_mutex;
	pthread_mutex_t p1q_mutex;
	pthread_mutex_t p2q_mutex;
	pthread_mutex_t p3q_mutex;
	/*
	 * As of now we are not adding new processes to dispatch list at run time
	 * so we donot need secure access to dispatch list since dispatcher will only
	 * be accessing it. If however, processes are to be added via commandline
	 * then, dispatch list will need to be secured.
	 */
	struct process_queue_t active_process;
	pthread_mutex_t active_process_lock;
	pthread_mutex_t next_to_run_process_lock;
	struct process_queue_t next_to_run_process;
	bool active_process_valid;
	bool next_to_run_process_valid;
}hds_core_state;

void init_hds_core_state();
void init_hds_resource_state();
void *hds_dispatcher(void *args);
void *hds_scheduler(void *args);
void *hds_cpu(void *args);
void *hds_stats_manager(void *args);
#endif /* HDS_CORE_H_ */
