/*
 */
#include "hds_core.h"
static void remove_first_ele_from_q(struct hds_process_t *dispatcher_list_head);

void init_hds_core_state() {
	//init the process queues
	hds_core_state.p0q = hds_core_state.p0q_last = hds_core_state.p1q =
			hds_core_state.p1q_last = hds_core_state.p2q =
					hds_core_state.p2q_last = hds_core_state.rtq =
							hds_core_state.rtq_last = NULL;
	if (pthread_mutex_init(&hds_core_state.p0q_mutex, NULL ) != 0) {
		serror("Failed to initialize mutex: p0q_mutex");
	}
	if (pthread_mutex_init(&hds_core_state.p1q_mutex, NULL ) != 0) {
		serror("Failed to initialize mutex: p1q_mutex");
	}
	if (pthread_mutex_init(&hds_core_state.p2q_mutex, NULL ) != 0) {
		serror("Failed to initialize mutex: p2q_mutex");
	}
	if (pthread_mutex_init(&hds_core_state.rtq_mutex, NULL ) != 0) {
		serror("Failed to initialize mutex: rtq_mutex");
	}
}
/**
 * @brief Main routine for dispatcher thread
 * @param args
 */
void *hds_dispatcher(void *args) {
	/*
	 * Working of dispatcher thread:
	 * -----------------------------
	 * 1. Read the next process from jb_dispatch_list
	 * 2. Find its type and accordingly place this process in one out of the
	 * 		four process queues that we have.
	 * 		2.1) Also set the arrival time of this process before putting into
	 * 				the target process queue.
	 * 		2.2) Remove this process from job dispatch list.
	 * 3. Sleep for 1 second
	 * 4. Repeat step 1 till there  are no more processes left in the job dispatch
	 * 		list.
	 * 		4.1) when no more processes sleep for 1 seconds.
	 */

	while (1) {
		if (hds_state.shutdown_in_progress == true) {
			break;
		}
		if (!hds_config.job_dispatch_list) {
			// no more processes in job dispatch list
			sleep(1);
			continue;
		}

		/*
		 * Since first element of the job dispatch list is the one at head of
		 * the list, we will process first element of the list.
		 */
		switch (hds_config.job_dispatch_list->priority) {
		case 0:
			//realtime process -- highest priority but non-interruptable
			pthread_mutex_lock(&hds_core_state.rtq_mutex);
			if (insert_process_to_q(&hds_core_state.rtq,&hds_core_state.rtq_last,hds_config.job_dispatch_list)!=HDS_OK){
				serror("Failed to insert a process in real time Q");
				//perform cleanup if needed
				break;
			}
			pthread_mutex_unlock(&hds_core_state.rtq_mutex);
			//remove this process from dispatch queue
			remove_first_ele_from_q(&hds_config.job_dispatch_list);
			break;
		case 1:
			//user time process of priority 1 -- highest priority
			break;
		case 2:
			//user time process of priority 1 -- medium priority
			break;
		case 3:
			//user time process of priority 1 -- lowest priority
			break;
		}
	}
	sdebug("dispatcher: Shutting down..");
	//clean dispatch list
	cleanup_process_dispatch_list();
	pthread_exit(NULL );
}
/**
 * @brief Main routine for scheduler thread
 * @param args
 */
void *hds_scheduler(void *args) {

	while (1) {
		if (hds_state.shutdown_in_progress == true) {
			break;
		}
	}
	sdebug("scheduler: Shutting down..");
	pthread_exit(NULL );
}
/**
 * @Main routine for cpu thread
 * @param args
 */
void *hds_cpu(void *args) {
	while (1) {
		if (hds_state.shutdown_in_progress == true) {
			break;
		}
	}
	sdebug("cpu: Shutting down..");
	pthread_exit(NULL );
}

// //////////// Simple API for dealing with queues of type process_t //////

static int insert_process_to_q(struct process_queue_t **qhead,struct process_queue_t **q_last,
		struct hds_process_t *process_frm_dispatch_list) {

}
static struct process_queue_t * get_first_element(struct process_queue_t *qhead) {

}
static void remove_first_ele_from_q(struct hds_process_t *dispatcher_list_head){
	//remove the first element
}
