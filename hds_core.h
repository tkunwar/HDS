
#ifndef HDS_CORE_H_
#define HDS_CORE_H_

#include "hds_common.h"
#define SMALLEST_TIME_QUANTUM 1
struct process_queue_t{
	unsigned long int arrival_time;
	int priority;
	int cpu_time;
	int memory_req;
	int printer_req;
	int scanner_req;
	struct process_queue_t *next;
}realtime_q,p0_q,p1_q,p2_q;

struct hds_core_state_t{
	struct process_queue_t *rtq,*rtq_last;
	struct process_queue_t *p0q,*p0q_last;
	struct process_queue_t *p1q,*p1q_last;
	struct process_queue_t *p2q,*p2q_last;

	//mutexes for ensuring exclusive access to the process queues
	pthread_mutex_t rtq_mutex;
	pthread_mutex_t p0q_mutex;
	pthread_mutex_t p1q_mutex;
	pthread_mutex_t p2q_mutex;
	/*
	 * As of now we are not adding new processes to dispatch list at run time
	 * so we donot need secure access to dispatch list since dispatcher will only
	 * be accessing it. If however, processes are to be added via commandline
	 * then, dispatch list will need to be secured.
	 */

}hds_core_state;

void init_hds_core_state();
void *hds_dispatcher(void *args);
void *hds_scheduler(void *args);
void *hds_cpu(void *args);

#endif /* HDS_CORE_H_ */
