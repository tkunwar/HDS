/*
 */
#include "hds_core.h"
static int remove_first_ele_from_dispatcher_q(
		struct hds_process_t **dispatcher_list_head);
static int insert_process_to_q(struct process_queue_t **qhead,
		struct process_queue_t **q_last,
		struct hds_process_t *process_frm_dispatch_list);

void init_hds_resource_state() {
	hds_resource_state.avail_memory = hds_config.max_resources.memory;
	hds_resource_state.avail_printer = hds_config.max_resources.printer;
	hds_resource_state.avail_scanner = hds_config.max_resources.scanner;
}
void init_hds_core_state() {
	//init the process queues
	hds_core_state.p1q = hds_core_state.p1q_last = hds_core_state.p2q =
			hds_core_state.p2q_last = hds_core_state.p3q =
					hds_core_state.p3q_last = hds_core_state.rtq =
							hds_core_state.rtq_last = NULL;
	if (pthread_mutex_init(&hds_core_state.p1q_mutex, NULL ) != 0) {
		serror("Failed to initialize mutex: p0q_mutex");
	}
	if (pthread_mutex_init(&hds_core_state.p2q_mutex, NULL ) != 0) {
		serror("Failed to initialize mutex: p1q_mutex");
	}
	if (pthread_mutex_init(&hds_core_state.p3q_mutex, NULL ) != 0) {
		serror("Failed to initialize mutex: p2q_mutex");
	}
	if (pthread_mutex_init(&hds_core_state.rtq_mutex, NULL ) != 0) {
		serror("Failed to initialize mutex: rtq_mutex");
	}

	if (pthread_mutex_init(&hds_core_state.next_to_run_process_lock, NULL )
			!= 0) {
		serror("Failed to initialize mutex: next_to_run_process_mutex");
	}
	if (pthread_mutex_init(&hds_resource_state.avail_resource_mutex, NULL )
			!= 0) {
		serror("Failed to initialize mutex: avail_resource_mutex");
	}
	hds_core_state.active_process.pid = hds_core_state.next_to_run_process.pid =
			0;
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
			//realtime process -- highest priority and non-interruptable
			pthread_mutex_lock(&hds_core_state.rtq_mutex);
			if (insert_process_to_q(&hds_core_state.rtq,
					&hds_core_state.rtq_last,
					hds_config.job_dispatch_list)!=HDS_OK) {
				serror("Failed to insert a process in real time Q");
				//perform cleanup if needed
				break;
			}
			pthread_mutex_unlock(&hds_core_state.rtq_mutex);
			//remove this process from dispatch queue
			remove_first_ele_from_dispatcher_q(&hds_config.job_dispatch_list);
			break;
		case 1:
			//user time process of priority 1 -- highest priority
			pthread_mutex_lock(&hds_core_state.p1q_mutex);
			if (insert_process_to_q(&hds_core_state.p1q,
					&hds_core_state.p1q_last,
					hds_config.job_dispatch_list)!=HDS_OK) {
				serror("Failed to insert a process in p1Q");
				//perform cleanup if needed
				break;
			}
			pthread_mutex_unlock(&hds_core_state.p1q_mutex);
			//remove this process from dispatch queue
			remove_first_ele_from_dispatcher_q(&hds_config.job_dispatch_list);
			break;
		case 2:
			//user time process of priority 2 -- medium priority
			pthread_mutex_lock(&hds_core_state.p2q_mutex);
			if (insert_process_to_q(&hds_core_state.p2q,
					&hds_core_state.p2q_last,
					hds_config.job_dispatch_list)!=HDS_OK) {
				serror("Failed to insert a process in p2Q");
				//perform cleanup if needed
				break;
			}
			pthread_mutex_unlock(&hds_core_state.p2q_mutex);
			//remove this process from dispatch queue
			remove_first_ele_from_dispatcher_q(&hds_config.job_dispatch_list);
			break;
		case 3:
			//user time process of priority 3 -- lowest priority
			pthread_mutex_lock(&hds_core_state.p3q_mutex);
			if (insert_process_to_q(&hds_core_state.p3q,
					&hds_core_state.p3q_last,
					hds_config.job_dispatch_list)!=HDS_OK) {
				serror("Failed to insert a process in p3Q");
				//perform cleanup if needed
				break;
			}
			pthread_mutex_unlock(&hds_core_state.p3q_mutex);
			//remove this process from dispatch queue
			remove_first_ele_from_dispatcher_q(&hds_config.job_dispatch_list);
			break;
		}
		//now sleep for 1 seconds before going to look for a new process
		sleep(1);
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
	/*
	 * Basic working of scheduler
	 * ---------------------------
	 * 1. Searching for a process begins from the highest priority queue to
	 * 		lower priority queues.
	 * 2. Look for the first process that satisfies the admission test.
	 * 3. Insert this process in hds_core_state.next_to_run_process.
	 * 4. Remove this process from its current queue.
	 */
	while (1) {
		if (hds_state.shutdown_in_progress == true) {
			break;
		}

	}
	sdebug("scheduler: Shutting down..");
	pthread_exit(NULL );
}
/**
 * @brief Check if this process can be admitted to cpu for executing in the next
 * 			cycle.
 * @param next_to_run The process which is to be tested.
 * @return true/false indicating whether this processes passed the admission test
 * 			or not.
 */
static bool can_process_be_admitted(struct process_queue_t *next_to_run) {
	bool can_be_admitted = false;
	pthread_mutex_lock(&hds_resource_state.avail_resource_mutex);
	if ((next_to_run->memory_req < hds_resource_state.avail_memory)
			&& (next_to_run->printer_req < hds_resource_state.avail_printer)
			&& (next_to_run->scanner_req < hds_resource_state.avail_scanner)){
		can_be_admitted = true;
	}
	pthread_mutex_unlock(&hds_resource_state.avail_resource_mutex);
	return can_be_admitted;
}
/**
 * @Main routine for cpu thread
 * @param args
 */
void *hds_cpu(void *args) {
	/*
	 * 1. cpu_thread will check for a new process after letting the current
	 * 		active process run for 1 second.
	 * 		Also if current current_process is not valid (pid =0) then check if
	 * 		next_to_run process is valid. if so then set next_to_run process
	 * 		as active.
	 *
	 * 2. check if next_to_run process has higher priority than the current
	 * 		active process ie. the current process is interruptible.
	 * 		2.1) If next_to_run process is a realtime process then execute it
	 * 				until completion.
	 *
	 * 3. If not then continue while updating stats for the current process.
	 * 		3.1) If current_process has been stopped then resume it by
	 * 				sending signal SIGCONT.
	 *
	 * 4. If current process is interruptible then
	 * 		4.1) decrease active_process's priority (if possible) and insert
	 * 			it into the new (lower) queue.
	 * 		4.2) Do not release resources allocated to it.
	 *
	 * 		4.3) Spawn a new process using attributes from next_to_run process.
	 * 		4.4) Set next_to_run process as the active_process.
	 * 5. sleep for a second .
	 * 6. Now interrupt the active process with SIGSTOP.
	 * 7. Update stats as necessary.
	 * 8.
	 */
	while (1) {
		if (hds_state.shutdown_in_progress == true) {
			break;
		}
		/*
		 * pid_t child_pid;¬
       17     //flush stdout and stderr so that child does not inherit them¬
       16     fflush(stdout);¬
       15     fflush(stderr);¬
       14 ¬
       13     child_pid = fork();¬
       12     switch(child_pid){¬
       11     case -1:¬
       10         printf("fork(): failed");¬
        9         break;¬
        8     case 0:¬
        7         child_function();¬
        6         break;¬
        5     default:¬
        4         parent_function(child_pid);¬
        3         break;¬
        2     }¬
		 */
		/*
		 * void parent_function(pid_t child_pid){¬
       10     int sleep_count = 0,status;¬
       11     printf("Interrupting child after every one second ");¬
       12     fflush(stdout);¬
       13     sleep(1);¬
       14     while(1){¬
       15         if (sleep_count == 10){¬
       16             break;¬
       17         }¬
       18         printf("\nStopping child");¬
       19 fflush(stdout);¬
       20 ¬
       21         kill(child_pid,SIGSTOP);¬
       22         sleep(1);¬
       23         printf("\nResuming child");¬
       24 fflush(stdout);¬
       25 ¬
       26         kill(child_pid,SIGCONT);¬
       27 ¬
       28         sleep(1);¬
       29         sleep_count++;¬
       30     }¬
       31     printf("Now killing child ..");¬
       32 fflush(stdout);¬
       33 ¬
       34     kill(child_pid,SIGKILL);¬
       35     waitpid(child_pid,&status,WNOHANG);¬
       36 ¬
       37     //check and verify how the child exited¬
       38     //¬
       39     if(WIFEXITED(status)){¬
       40         printf("CHild exited normally");¬
       41     }else if (WIFSIGNALED(status)){¬
       42         printf("killed by signal");¬
       43     }¬
       44 }¬
		 */
	}
	sdebug("cpu: Shutting down..");
	pthread_exit(NULL );
}
static void child_function(){
	/*
	 * what our processes would do ? Since I am not sure about whether debug()
	 * routines would work in this situation. for now child process simply sleep
	 * for a while. again back in infinite loop.
	 */
	while(1){
		var_debug("Child process with PID: %d working.",getpid());
		usleep(10000);
	}
}
// //////////// Simple API for dealing with queues of type process_t //////

static int insert_process_to_q(struct process_queue_t **qhead,
		struct process_queue_t **q_last,
		struct hds_process_t *process_frm_dispatch_list) {
	struct process_queue_t *node = NULL;
	node = (struct process_queue_t *) malloc(sizeof(struct process_queue_t));
	if (!node) {
		serror("malloc: failed ");
		return HDS_ERR_NO_MEM;
	}
	//TODO: since granularity is in seconds,get time in seconds
	node->arrival_time = gettime_in_nsecs();

	node->cpu_time = process_frm_dispatch_list->cpu_req;
	node->memory_req = process_frm_dispatch_list->memory_req;
	node->printer_req = process_frm_dispatch_list->printer_req;
	node->priority = process_frm_dispatch_list->priority;
	node->scanner_req = process_frm_dispatch_list->scanner_req;
	node->pid = process_frm_dispatch_list->pid;
	node->next = NULL;

	if (!*qhead && !*q_last) {
		//initial state
		*q_last = *qhead = node;
	} else {
		(*q_last)->next = node;
		*q_last = node;
		(*q_last)->next = NULL;
	}
	return HDS_OK;

}
static struct process_queue_t * get_first_element(struct process_queue_t *qhead) {

}
static int remove_first_ele_from_dispatcher_q(
		struct hds_process_t **dispatcher_list_head) {
	//remove the first element
	struct hds_process_t *to_be_deleted = *dispatcher_list_head;
	if (hds_config.job_dispatch_list == NULL ) {
		swarn("Job dispatch queue is empty !! Nothing to remove");
		return HDS_OK;
	}
	//	var_debug("Removing [%u] next [%u]",to_be_deleted->page.page_id,((*node)->next)->page.page_id);
	//	var_debug("Trying to remove page %u", (*node)->page.page_id);
	// first case itself should handle head being null. this one is not needed.
	if (!*dispatcher_list_head) {
		serror("Trying to delete a NULL page from dispatch queue");
		return HDS_ERR_MEM_FAULT;
	}

	hds_config.job_dispatch_list = (*dispatcher_list_head)->next;
	free(to_be_deleted);
	return HDS_OK;
}

void print_current_cpu_stats(){
	/*
	 * We will print info about active process,next_to_run_process and available
	 * resources when demanded.
	 */

}
