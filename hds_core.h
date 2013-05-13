
#ifndef HDS_CORE_H_
#define HDS_CORE_H_

#include "hds_common.h"
#define SMALLEST_TIME_QUANTUM 1
typedef unsigned int MEM_HANDLE;
#define MEM_BLOCK_INACTIVE -1

/**
 * @struct hds_resource_state
 * @brief Denotes the current state of resources available for further allocation.
 */
struct hds_global_resource_state_t{
	int avail_memory;
	int avail_scanner;
	int avail_printer;
	pthread_mutex_t avail_resource_mutex;
}max_available_resource;


struct global_memory_pool_info_t{
	unsigned int max_mem_size;
	unsigned int mem_available;
	unsigned int free_pool_start;
	unsigned int free_pool_end;
	unsigned int mem_block_id_counter;
};
struct mem_block_t{
	unsigned int mem_block_id; /**< A unique id of this block. */
	int pid; /**< Indicates the PID of the process to which this block belongs.
				Set to -1 to indicate that this block is free */
	unsigned int size; /**< Size of this block. For sake of simplicity, we assume that
	 	 	 	 	 	 	 every memory request is in MB and that every allocation request
	 	 	 	 	 	 	 will be positive integer only.*/
	unsigned int start_pos; /**< Beginning position for this block. */
	unsigned int end_pos; /**< End position for this block.*/
	struct mem_block_t *next;
};
/**
 * @struct hds_resource_t
 * @brief This structure holds information about allocated resources to a process.
 * 		This structure will be present for every process and will be part of
 * 		the process structure.
 */
struct hds_allocated_resource_t{
	unsigned int mem_block_handle; /**< The pointer to memory block that has been
	 	 	 	 	 	 	 	 	 	allocated to a process. Note that in ideal
	 	 	 	 	 	 	 	 	 	conditions, we expect it to be a list of
	 	 	 	 	 	 	 	 	 	handles. But for simplicity, we assume that
	 	 	 	 	 	 	 	 	 	per process there will be only single
	 	 	 	 	 	 	 	 	 	memory allocation request and thus, single
	 	 	 	 	 	 	 	 	 	memory_block_handle.
	 	 	 	 	 	 	 	 	 	*/
//	int printer_res;
//	int scanner_res;
};
struct process_queue_t{
	unsigned long int arrival_time;
	int priority;
	int cpu_req;
	int memory_req;
	int printer_req;
	int scanner_req;
	int pid; /**< A non zero pid would mean it has not yet run for once. Once a process runs
	 	 	 	 	 	 it will have a valid pid. It could be in either suspended/running state.*/
	struct hds_allocated_resource_t allocate_resource;
	struct process_queue_t *next;
};

struct hds_core_state_t{
	struct process_queue_t *rtq,*rtq_last;
	struct process_queue_t *user_job_q,*user_job_q_last;
	struct process_queue_t *p1q,*p1q_last;
	struct process_queue_t *p2q,*p2q_last;
	struct process_queue_t *p3q,*p3q_last;

	struct global_memory_pool_info_t global_memory_info;
	struct mem_block_t *mem_block_list,*mem_block_list_last;

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
	struct process_queue_t next_to_run_process;
	pthread_mutex_t next_to_run_process_lock;

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
