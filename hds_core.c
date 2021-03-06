/*
 */
#include "hds_core.h"
static int remove_first_ele_from_dispatcher_q(
		struct hds_process_t **dispatcher_list_head);
static int insert_process_to_q_from_dispatch_list(
		struct process_queue_t **qhead, struct process_queue_t **q_last,
		struct hds_process_t *process_frm_dispatch_list);
static bool can_process_be_admitted(struct process_queue_t *next_to_run);
static struct process_queue_t *find_next_process_tobe_executed();
static void remove_process_from_queue(struct process_queue_t *p);
static void degrade_priority_and_save_to_q(struct process_queue_t *p);
static void del_node(struct process_queue_t **qhead,
		struct process_queue_t *to_be_deleted);
static void child_function();
static void process_user_jobq(struct process_queue_t **qhead);
static int insert_process_to_q_from_user_job_q(struct process_queue_t **qhead,
		struct process_queue_t **q_last,
		struct process_queue_t *process_frm_user_jobq);
static int remove_first_ele_from_user_job_q(struct process_queue_t **user_job_q);
static int allocate_resources(struct process_queue_t *process);
static int free_resources(struct process_queue_t *process);
static int insert_mem_block_to_list(struct mem_block_t *mblock_to_attached);
static int find_smallest_free_mblock(unsigned int pid, int mem_req);
static void _consolidate_memory();
static int allocate_from_free_pool(unsigned int pid, int mem_req);
static void cleanup_mem_block_list();
static MEM_HANDLE allocate_mem(unsigned int pid, unsigned int mem_req);
void print_memory_maps();

struct mem_block_t* SortedMerge(struct mem_block_t* a, struct mem_block_t* b);
void FrontBackSplit(struct mem_block_t* source, struct mem_block_t** frontRef,
		struct mem_block_t** backRef);
void MergeSort(struct mem_block_t** headRef);

void init_hds_resource_state() {
	// available _memory will at any point be less than 64 MB. Since that much
	// we are reserving for realtime processes.
	// NOTE: Any resource allocation/deallocation must explicittly update
	// global hds_resource_state fields.
	max_available_resource.avail_memory = hds_config.max_resources.memory - 64;

	max_available_resource.avail_printer = hds_config.max_resources.printer;
	max_available_resource.avail_scanner = hds_config.max_resources.scanner;
}
void init_hds_core_state() {
	//init the process queues
	hds_core_state.p1q = hds_core_state.p1q_last = hds_core_state.p2q =
			hds_core_state.p2q_last = hds_core_state.p3q =
					hds_core_state.p3q_last = hds_core_state.rtq =
							hds_core_state.rtq_last = NULL;
	hds_core_state.user_job_q = hds_core_state.user_job_q_last = NULL;

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
	if (pthread_mutex_init(&max_available_resource.avail_resource_mutex, NULL )
			!= 0) {
		serror("Failed to initialize mutex: avail_resource_mutex");
	}
	if (pthread_mutex_init(&hds_core_state.active_process_lock, NULL ) != 0) {
		serror("Failed to initialize mutex: active_process_lock");
	}
//	hds_core_state.active_process.pid = hds_core_state.next_to_run_process.pid =
//			-1; //to check if next_to_run process has been seen by cpu
//	hds_core_state.active_process.priority =
//			hds_core_state.next_to_run_process.priority = -1; //to check if it's the first
//															  //time we are running
	hds_core_state.active_process_valid =
			hds_core_state.next_to_run_process_valid = false;

	// Initialize global memory pool info.
	hds_core_state.mem_block_list = hds_core_state.mem_block_list_last = NULL;
	hds_core_state.global_memory_info.max_mem_size =
			(max_available_resource.avail_memory);
	hds_core_state.global_memory_info.mem_available =
			hds_core_state.global_memory_info.max_mem_size;
	hds_core_state.global_memory_info.mem_block_id_counter = 1;
	/*
	 * We will first 64 MB for realtime processes. So free_pool will start from
	 * 65 till hds_config.max_resources.memory
	 */
	hds_core_state.global_memory_info.free_pool_start = 65;
	hds_core_state.global_memory_info.free_pool_end =
			hds_config.max_resources.memory; // should always point to hds_config.max_resources.memory

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
			if (insert_process_to_q_from_dispatch_list(&hds_core_state.rtq,
					&hds_core_state.rtq_last,
					hds_config.job_dispatch_list)!=HDS_OK) {
				serror("Failed to insert a process in real time Q");
				//perform cleanup if needed
				break;
			}
			pthread_mutex_unlock(&hds_core_state.rtq_mutex);
//			var_debug(
//					"dispatcher: Adding process(PID:%d PRI:%d CPU:%d MEM:%d PRN:%d SCN:%d) to rtq",
//					hds_config.job_dispatch_list->pid,
//					hds_config.job_dispatch_list->priority,
//					hds_config.job_dispatch_list->cpu_req,
//					hds_config.job_dispatch_list->memory_req,
//					hds_config.job_dispatch_list->printer_req,
//					hds_config.job_dispatch_list->scanner_req)
//			;

			//remove this process from dispatch queue
			remove_first_ele_from_dispatcher_q(&hds_config.job_dispatch_list);
			break;

		default:
			//this is a user process so we add it to user job queue.
			// all processes other than realtime processes go this user_job_q
			if (insert_process_to_q_from_dispatch_list(
					&hds_core_state.user_job_q, &hds_core_state.user_job_q_last,
					hds_config.job_dispatch_list) != HDS_OK) {
				serror("Failed to insert a process in user job q");
				//perform cleanup if needed
				break;
			}
			remove_first_ele_from_dispatcher_q(&hds_config.job_dispatch_list);
			// now we need to process processes in job_q and put them to respective
			// process queues
			process_user_jobq(&hds_core_state.user_job_q);
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
 * @brief Move processes from user_jobq to their respective priority based queues.
 * @param qhead The pointer to the head of the user job queue.
 */
static void process_user_jobq(struct process_queue_t **qhead) {
	switch ((*qhead)->priority) {
	case 1:
		//user time process of priority 1 -- highest priority
		pthread_mutex_lock(&hds_core_state.p1q_mutex);
		if (insert_process_to_q_from_user_job_q(&hds_core_state.p1q,
				&hds_core_state.p1q_last, *qhead) != HDS_OK) {
			serror("Failed to insert a process in p1Q");
			//perform cleanup if needed
			break;
		}
		pthread_mutex_unlock(&hds_core_state.p1q_mutex);
		//remove this process from dispatch queue
		remove_first_ele_from_user_job_q(qhead);
		break;
	case 2:
		//user time process of priority 2 -- medium priority
		pthread_mutex_lock(&hds_core_state.p2q_mutex);
		if (insert_process_to_q_from_user_job_q(&hds_core_state.p2q,
				&hds_core_state.p2q_last, *qhead) != HDS_OK) {
			serror("Failed to insert a process in p2Q");
			//perform cleanup if needed
			break;
		}
		pthread_mutex_unlock(&hds_core_state.p2q_mutex);
		//remove this process from dispatch queue
		remove_first_ele_from_user_job_q(qhead);
		break;
	case 3:
		//user time process of priority 3 -- lowest priority
		pthread_mutex_lock(&hds_core_state.p3q_mutex);
		if (insert_process_to_q_from_user_job_q(&hds_core_state.p3q,
				&hds_core_state.p3q_last, *qhead) != HDS_OK) {
			serror("Failed to insert a process in p3Q");
			//perform cleanup if needed
			break;
		}
		pthread_mutex_unlock(&hds_core_state.p3q_mutex);
		//remove this process from dispatch queue
		remove_first_ele_from_user_job_q(qhead);
		break;
	default:
		//this is a process having unrecognized priority. we remove it
		serror(
				"Found a process with bogus priority. Removing from the user job list")
		;
		remove_first_ele_from_user_job_q(qhead);
		break;
	}
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
	struct process_queue_t *next_process = NULL, *tmp = NULL;
	while (1) {
		if (hds_state.shutdown_in_progress == true) {
			break;
		}
//		var_debug("scheduler: parent: %d, I belong to %d process",hds_state.parent_pid,getpid());
		//find next highest process which has passed the admission test.
		next_process = find_next_process_tobe_executed();

		if (next_process == NULL ) {
			//no process could pass the admission test possibly because, resources
			// are held by older processes have not yet been released
//			sdebug(
//					"Could not find a process to be scheduled for next execution.");
			sleep(1);
			continue;
		}
//		var_debug(
//				"scheduler: Selected next process(PID:%d PRI:%d CPU:%d MEM:%d PRN:%d SCN:%d)",
//				next_process->pid, next_process->priority,
//				next_process->cpu_req, next_process->memory_req,
//				next_process->printer_req, next_process->scanner_req);

		// now we have a process which will be scheduled for execution in the
		// next cycle. However we will schedule it only when it has higher
		// priority than the currently executing process. If not then we will wait
		// until execution of active process it over. However, while we are waiting
		// a new process of higher priority might arrive from dispatch queue
		// therefore better would be that we continue up.

		//for the first time, since active_process and next_to_run process both
		// have their priorities set to -1. to avoid this check for first time
		// we will check if active_process's is valid
		if ((hds_core_state.active_process.priority < next_process->priority)
				&& (hds_core_state.active_process_valid == true)) {
//			sdebug("scheduler: Active process cant be interrupted !");
			sleep(1);
			continue;
		}

		// here we have a process which is at higher priority than the currently
		// executing process. Now we should save it in hds_core_state.next_to_run
		// process such that in the next cycle cpu thread will find it and execute it.
		// However what if next_to_run process has not yet run ? CPU might not have
		// seen it as of now.
		// To handle such case, we check if next_to_run process is a high priority
		// process as compared to the next_process. If it's so then we will not
		// interrupt it rather we will sleep and continue all the way up.
		// however if it's at lower priority then we will replace it by
		// next_process attributes and insert the next_to_run process into
		// its queue *without* lowering the priority.
		/*
		 * how do we know that next_to_run process has been seen and serviced
		 * by cpu_thread. If cpu has seen and started the next_to_run process,
		 * then it will reset the next_to_run process->pid to -1. Note that
		 * by default all pids are 0 unless they have been started, in that case
		 * pid will be a valid linux PID.
		 */

		if (hds_core_state.next_to_run_process_valid == false) {
			//it will be replaced for sure. this handles the initial case
//			var_debug(
//					"scheduler: Setting next to be executed process(PID:%d PRI:%d CPU:%d MEM:%d PRN:%d SCN:%d)",
//					next_process->pid, next_process->priority,
//					next_process->cpu_req, next_process->memory_req,
//					next_process->printer_req, next_process->scanner_req);

			pthread_mutex_lock(&hds_core_state.next_to_run_process_lock);
			hds_core_state.next_to_run_process.arrival_time =
					next_process->arrival_time;
			hds_core_state.next_to_run_process.cpu_req = next_process->cpu_req;
			hds_core_state.next_to_run_process.memory_req =
					next_process->memory_req;
			hds_core_state.next_to_run_process.pid = next_process->pid;
			hds_core_state.next_to_run_process.printer_req =
					next_process->printer_req;
			hds_core_state.next_to_run_process.priority =
					next_process->priority;
			hds_core_state.next_to_run_process.scanner_req =
					next_process->scanner_req;

			//validate next_to_run process
			hds_core_state.next_to_run_process_valid = true;
			pthread_mutex_unlock(&hds_core_state.next_to_run_process_lock);
			// now that next_process has been submitted to cpu remove it from
			// its current queue.
			remove_process_from_queue(next_process);
			continue;
		}
		/*
		 * if next_to_run process is valid one,then we will check if it can
		 * be replaced.
		 */
		if ((hds_core_state.next_to_run_process.priority
				<= next_process->priority)) {
			// we will follow fcfs for processes with equal priority
			//next_to_run process cant be replaced
//			sdebug("Cant replace next_to_run process");
			sleep(1);
			continue;
		} else {
			// since next_to_run process is a valid process it means
			// next_to_run process has not yet been executed even once and its
			// priority is even less than next_process so we will replace
			// it by next_process

			// first degrade priority of next_to_run process and save it to
			// appropriate queues
//			var_debug(
//					"scheduler: Replacing next_to_run process with(PID:%d PRI:%d CPU:%d MEM:%d PRN:%d SCN:%d)",
//					next_process->pid, next_process->priority,
//					next_process->cpu_req, next_process->memory_req,
//					next_process->printer_req, next_process->scanner_req);

			pthread_mutex_lock(&hds_core_state.next_to_run_process_lock);
			// this process has not been executed even once. should it priority
			// be degraded
			degrade_priority_and_save_to_q(&hds_core_state.next_to_run_process);
			pthread_mutex_unlock(&hds_core_state.next_to_run_process_lock);

			//now copy next_process into next_to_run process
			pthread_mutex_lock(&hds_core_state.next_to_run_process_lock);
			hds_core_state.next_to_run_process.arrival_time =
					next_process->arrival_time;
			hds_core_state.next_to_run_process.cpu_req = next_process->cpu_req;
			hds_core_state.next_to_run_process.memory_req =
					next_process->memory_req;
			hds_core_state.next_to_run_process.pid = next_process->pid;
			hds_core_state.next_to_run_process.printer_req =
					next_process->printer_req;
			hds_core_state.next_to_run_process.priority =
					next_process->priority;
			hds_core_state.next_to_run_process.scanner_req =
					next_process->scanner_req;

			//validate next_to_run process
			hds_core_state.next_to_run_process_valid = true;
			pthread_mutex_unlock(&hds_core_state.next_to_run_process_lock);

			// now that next_process has been submitted to cpu remove it from
			// its current queue.
			remove_process_from_queue(next_process);
		}
		//sleep for one second before going up
		sleep(1);
	}
	sdebug("scheduler: Shutting down..");
	pthread_exit(NULL );
}
static void degrade_priority_and_save_to_q(struct process_queue_t *p) {
	struct process_queue_t * newp = NULL;
	newp = (struct process_queue_t*) malloc(sizeof(struct process_queue_t));
	if (!newp) {
		serror("Failed to allocate memory for new process");
		return;
	}
	//copy all attributes from next_to_run process
	newp->arrival_time = p->arrival_time;
	newp->cpu_req = p->cpu_req;
	newp->memory_req = p->memory_req;
	newp->pid = p->pid;
	newp->printer_req = p->printer_req;
	if (p->priority != 3) {
		newp->priority = p->priority + 1;
	}

	newp->scanner_req = p->scanner_req;
	//now depending upon the priority of newp insert into a queue
	switch (newp->priority) {
	case 0:
		//it will never be here since a realtime process cant be interrupted anyway
		break;
	case 1:
		// it cant be here as well, since a realtime process in next_to_run process
		// cant be replaced
		break;
	case 2:
		pthread_mutex_lock(&hds_core_state.p2q_mutex);
		hds_core_state.p2q_last->next = newp;
		hds_core_state.p2q_last = newp;
		hds_core_state.p2q_last->next = NULL;
		pthread_mutex_unlock(&hds_core_state.p2q_mutex);
		break;
	case 3:
		pthread_mutex_lock(&hds_core_state.p3q_mutex);
		hds_core_state.p3q_last->next = newp;
		hds_core_state.p3q_last = newp;
		hds_core_state.p3q_last->next = NULL;
		pthread_mutex_unlock(&hds_core_state.p3q_mutex);
		break;
	default:
		break;
	}
}
static void remove_process_from_queue(struct process_queue_t *p) {
	/*
	 * depending upon type of p, we will remove it from its corresponding queue.
	 */

	switch (p->priority) {
	case 0:
		pthread_mutex_lock(&hds_core_state.rtq_mutex);
		del_node(&hds_core_state.rtq, p);
		pthread_mutex_unlock(&hds_core_state.rtq_mutex);
		break;
	case 1:
		while ((pthread_mutex_trylock(&hds_core_state.p1q_mutex) == EBUSY)) {
			sdebug("someone already has lock of p1q");
			sleep(1);
		}
//		pthread_mutex_lock(&hds_core_state.p1q_mutex);
		del_node(&hds_core_state.p1q, p);
		pthread_mutex_unlock(&hds_core_state.p1q_mutex);
		break;
	case 2:
		pthread_mutex_lock(&hds_core_state.p2q_mutex);
		del_node(&hds_core_state.p2q, p);
		pthread_mutex_unlock(&hds_core_state.p2q_mutex);
		break;
	case 3:
		pthread_mutex_lock(&hds_core_state.p3q_mutex);
		del_node(&hds_core_state.p3q, p);
		pthread_mutex_unlock(&hds_core_state.p3q_mutex);
		break;
	default:
		break;
	}
}
static void del_node(struct process_queue_t **qhead,
		struct process_queue_t *to_be_deleted) {
	struct process_queue_t *tmp = *qhead;
	if (!qhead) {
		//thats bad
		return;
	}
	if (!*qhead) {
		serror("Trying to delete a NULL node from process queue");
		return;
	}
	//if head is to deleted
	if (*qhead == to_be_deleted) {
		tmp = *qhead;
		*qhead = (*qhead)->next;
		free(tmp);
		return;
	}
	//find the element before to_be_deleted
	for (; tmp->next != NULL && tmp->next != to_be_deleted; tmp = tmp->next)
		;

	//now tmp points to the one node before to_be_deleted
	tmp->next = to_be_deleted->next;
	tmp = to_be_deleted;
	free(to_be_deleted);
}
static struct process_queue_t *find_next_process_tobe_executed() {
	struct process_queue_t *node = NULL;
	//search begins from rtq down to p3q

	pthread_mutex_lock(&hds_core_state.rtq_mutex);
	//for realtime jobs, the process at head of queue will be executed
	// that is use fcfs for realtime processes
	if (hds_core_state.rtq != NULL ) {
		node = hds_core_state.rtq;
	}
	pthread_mutex_unlock(&hds_core_state.rtq_mutex);
	if (node)
		return node;

	node = NULL;
	pthread_mutex_lock(&hds_core_state.p1q_mutex);
	for (node = hds_core_state.p1q; node != NULL ; node = node->next) {
		if (can_process_be_admitted(node) == true) {
			break;
		}
	}
	pthread_mutex_unlock(&hds_core_state.p1q_mutex);
	if (node)
		return node;

	node = NULL;
	pthread_mutex_lock(&hds_core_state.p2q_mutex);
	for (node = hds_core_state.p2q; node != NULL ; node = node->next) {
		if (can_process_be_admitted(node) == true) {
			break;
		}
	}
	pthread_mutex_unlock(&hds_core_state.p2q_mutex);
	if (node)
		return node;

	node = NULL;
	pthread_mutex_lock(&hds_core_state.p3q_mutex);
	for (node = hds_core_state.p3q; node != NULL ; node = node->next) {
		if (can_process_be_admitted(node) == true) {
			break;
		}
	}
	pthread_mutex_unlock(&hds_core_state.p3q_mutex);
	return node;
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
	pthread_mutex_lock(&max_available_resource.avail_resource_mutex);
	if ((next_to_run->memory_req < max_available_resource.avail_memory)) {
		can_be_admitted = true;
	}
	pthread_mutex_unlock(&max_available_resource.avail_resource_mutex);
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
	int status;
	while (1) {
		if (hds_state.shutdown_in_progress == true) {
			break;
		}
//		var_debug("cpu: parent: %d, I belong to %d process",
//				hds_state.parent_pid, getpid());
		/*
		 * if next_to_run process's priority is invalid (-1) then it means that
		 * scheduler has not submitted any job to us.
		 * what if next_to_run process is invalid simply because scheduler did
		 * not give us any process but current process can be executed. ?
		 */
		if ((hds_core_state.next_to_run_process_valid == false)
				&& (hds_core_state.active_process_valid == false)) {
			swarn("cpu: No valid process in next_to_run process.");
			sleep(1);
			continue;
		}
		/*
		 * although scheduler has made sure that next_to_run process will be at higher
		 * priority than the current process but before setting it active we will
		 * do a check.
		 */
		if (hds_core_state.active_process_valid == false) {
			/*
			 * this is the initial condition. we will set the next_to_run process
			 * as the active process.
			 */
			pthread_mutex_lock(&hds_core_state.active_process_lock);
			hds_core_state.active_process.arrival_time =
					hds_core_state.next_to_run_process.arrival_time;
			hds_core_state.active_process.cpu_req =
					hds_core_state.next_to_run_process.cpu_req;
			hds_core_state.active_process.memory_req =
					hds_core_state.next_to_run_process.memory_req;
			hds_core_state.active_process.pid =
					hds_core_state.next_to_run_process.pid; // this should be zero for a beginning process
			hds_core_state.active_process.printer_req =
					hds_core_state.next_to_run_process.printer_req;
			hds_core_state.active_process.priority =
					hds_core_state.next_to_run_process.priority;
			hds_core_state.active_process.scanner_req =
					hds_core_state.next_to_run_process.scanner_req;

			//validate active process
			hds_core_state.active_process_valid = true;
			pthread_mutex_unlock(&hds_core_state.active_process_lock);
			//now invalidate the next_to_run process.
			pthread_mutex_lock(&hds_core_state.next_to_run_process_lock);
			hds_core_state.next_to_run_process_valid = false;
			pthread_mutex_unlock(&hds_core_state.next_to_run_process_lock);
		} else {
			/*
			 * active process has been serviced by cpu. so we will check if next
			 * run process's priority is higher as compared to active process.
			 * if not then we will continue executing active process. If yes
			 * then we will degrade priority of active  process and move it back
			 * to qeueues and set next_to_run process as the active process.
			 */
			if ((hds_core_state.next_to_run_process.priority
					< hds_core_state.active_process.priority)
					&& (hds_core_state.next_to_run_process_valid == true)) {
				var_debug(
						"cpu: Interrupting active process with process(PID:%d PRI:%d CPU:%d MEM:%d PRN:%d SCN:%d)",
						hds_core_state.next_to_run_process.pid,
						hds_core_state.next_to_run_process.priority,
						hds_core_state.next_to_run_process.cpu_req,
						hds_core_state.next_to_run_process.memory_req,
						hds_core_state.next_to_run_process.printer_req,
						hds_core_state.next_to_run_process.scanner_req);
				//current process will be interrupted.
				degrade_priority_and_save_to_q(&hds_core_state.active_process);
				pthread_mutex_lock(&hds_core_state.active_process_lock);
				//now set next_process as the active process
				hds_core_state.active_process.arrival_time =
						hds_core_state.next_to_run_process.arrival_time;
				hds_core_state.active_process.cpu_req =
						hds_core_state.next_to_run_process.cpu_req;
				hds_core_state.active_process.memory_req =
						hds_core_state.next_to_run_process.memory_req;
				hds_core_state.active_process.pid =
						hds_core_state.next_to_run_process.pid; // this should be zero for a beginning process
				hds_core_state.active_process.printer_req =
						hds_core_state.next_to_run_process.printer_req;
				hds_core_state.active_process.priority =
						hds_core_state.next_to_run_process.priority;
				hds_core_state.active_process.scanner_req =
						hds_core_state.next_to_run_process.scanner_req;

				//validate active process
				hds_core_state.active_process_valid = true;
				pthread_mutex_unlock(&hds_core_state.active_process_lock);
				//now invalidate the next_to_run process.
				pthread_mutex_lock(&hds_core_state.next_to_run_process_lock);
				hds_core_state.next_to_run_process_valid = false;
				pthread_mutex_unlock(&hds_core_state.next_to_run_process_lock);
			}
			//else we will continue executing active process.
		}

		/*
		 * if still we have an invalid active process then we go up. This will
		 * ensure that we do not deal with bogus processes.
		 */
		if (hds_core_state.active_process_valid == false) {
			serror("Active process is invalid..");
			sleep(1);
			continue;
		}
		// from here onwards we will have a valid active process

		// ///////// Resource deallocation //////////////////
		/*
		 * if cpu_time of active process has become 0 then it means it's time
		 * to kill it and deallocate all resoiurces
		 * 1. kill it
		 * 2. deallocate resources
		 * 3. invalidate active process
		 */
		if (hds_core_state.active_process.cpu_req <= 0) {
			if (hds_core_state.active_process.pid > 1) {
				//kill it
				var_debug("cpu: killing child process: %d",
						hds_core_state.active_process.pid);
				//remember that child process is already stopped so before
				// killing it we will activate it
				kill(hds_core_state.active_process.pid, SIGCONT);
				//now kill it
				kill(hds_core_state.active_process.pid, SIGTERM);

				// *****deallocate resources *****
				free_resources(&hds_core_state.active_process);
				//collect this process
				waitpid(hds_core_state.active_process.pid, &status, WNOHANG);

				//invalidate it, such that it will be set as next_to_run process
				// in next cycle
				hds_core_state.active_process_valid = false;

				print_memory_maps();
				//now go up
				continue;
			} else {
				//this is probably a bogus process. how can a process have a pid
				// less than 1 and also cpu_time less than or equal to 0.
				// probably a config error
				serror("Probably a bogus process.");
				sleep(1);
				continue;
			}
		}

		/*
		 * now we will execute active_process. however we will check if it's the
		 * first time for active_process. If not then execute it for 1 quantum
		 * of time and update stats. If yes, the spawn a new process,save its pid
		 * in the active process. execute it for 1 quantum and then update stats.
		 *
		 */
		if (hds_core_state.active_process.pid == 0) {
			// this is the first time for this process. we will fork a new child
			// process
			hds_core_state.active_process.pid = fork();
			switch (hds_core_state.active_process.pid) {
			case -1:
				serror("cpu: fork() failed")
				;
				//this is bad
				break;
			case 0:
				child_function();
				break;
			default:
				//parent code
				/*
				 * since child is in an infinite loop. it will not come back
				 * to execute next lines code. So we can continue our work even
				 * though entire code will be duplicated in child.
				 *
				 * Since child will become active the moment it will be instantiated
				 * we stop it here so that we will run it later on.
				 */
				kill(hds_core_state.active_process.pid, SIGSTOP);
				var_debug("cpu: spawned new child process: %d",
						hds_core_state.active_process.pid)
				;
				/*
				 * Our resource allocation routine requires the pid therefore,
				 * we will perform resource allocation once we have obtained the
				 * pid.
				 */
				// ****do the resource allocation here ******
				if (allocate_resources(&hds_core_state.active_process) != HDS_OK) {
					serror("Resource allocation failed.");
					var_debug("cpu: killing premature child process: %d",
							hds_core_state.active_process.pid);
					kill(hds_core_state.active_process.pid, SIGCONT);
					kill(hds_core_state.active_process.pid, SIGTERM);

					waitpid(hds_core_state.active_process.pid, &status,
							WNOHANG);

					//invalidate it, such that it will be set as next_to_run process
					// in next cycle
					hds_core_state.active_process_valid = false;
					//now go up
					continue;
				}
				print_memory_maps();
				break;
			}
		}
		// The code following next, will never be executed in child.
		// if not then, behaviour is undefined
		var_debug(
				"cpu: Going to run process(PID:%d PRI:%d CPU:%d MEM:%d PRN:%d SCN:%d)",
				hds_core_state.active_process.pid,
				hds_core_state.active_process.priority,
				hds_core_state.active_process.cpu_req,
				hds_core_state.active_process.memory_req,
				hds_core_state.active_process.printer_req,
				hds_core_state.active_process.scanner_req);

		//now run the child process
		kill(hds_core_state.active_process.pid, SIGCONT);
		sleep(1);
		kill(hds_core_state.active_process.pid, SIGSTOP);

		//now update stats for this process
		pthread_mutex_lock(&hds_core_state.active_process_lock);
		hds_core_state.active_process.cpu_req =
				hds_core_state.active_process.cpu_req - 1;
		pthread_mutex_unlock(&hds_core_state.active_process_lock);
	}
	sdebug("cpu: Shutting down..");
	/*
	 * TODO: we need a cleanup routine which will find all child processes who have been
	 * spwaned and kill them. we can find such processes by examining all 4 queues
	 * and looking for a process with pid > 1.
	 */
	sdebug("cpu: Cleaning up mem_block_list");
	cleanup_mem_block_list(hds_core_state.mem_block_list);
	pthread_exit(NULL );
}
/**
 * @brief This routine must do all the resource allocation for a given process.
 * 	   Therefore it will recieve a process structure which will define the
 * 	   resource requirements for that process.
 */
static int allocate_resources(struct process_queue_t *process) {
	unsigned int mem_handle = -1;
	if (process->pid == -1) {
		return HDS_ERR_INVALID_PROCESS;
	}
	//begin memory allocation for this routine.
	if ((mem_handle = allocate_mem(process->pid, process->memory_req)) == 0) {
		return HDS_ERR_NO_RESOURCE;
	}

	//mem_handle is a valid one
	process->allocate_resource.mem_block_handle = mem_handle;
	return HDS_OK;
}
/**
 * @brief Free up any resources held by this process.
 * @param process The process which is to be dealocated.
 * @return
 */
static int free_resources(struct process_queue_t *process) {
	// as of now we only free memory allocated to this process
	free_mem(process->pid, process->allocate_resource.mem_block_handle);
	return HDS_OK;
}
static void child_function() {
	/*
	 * what our processes would do ? Since I am not sure about whether debug()
	 * routines would work in this situation. for now child process simply sleep
	 * for a while. again back in infinite loop.
	 */
	while (1) {
//		var_debug("Child process with PID: %d working.", getpid());
//		if (hds_state.shutdown_in_progress){
//			exit(EXIT_SUCCESS);
//		}
		usleep(1000);
	}
}
// //////////// Simple API for dealing with queues of type process_t //////
static int insert_process_to_q_from_user_job_q(struct process_queue_t **qhead,
		struct process_queue_t **q_last,
		struct process_queue_t *process_frm_user_jobq) {
	struct process_queue_t *node = NULL;
	node = (struct process_queue_t *) malloc(sizeof(struct process_queue_t));
	if (!node) {
		serror("malloc: failed ");
		return HDS_ERR_NO_MEM;
	}
	//TODO: since granularity is in seconds,get time in seconds
	node->arrival_time = gettime_in_nsecs();

	node->cpu_req = process_frm_user_jobq->cpu_req;
	node->memory_req = process_frm_user_jobq->memory_req;
	node->printer_req = process_frm_user_jobq->printer_req;
	node->priority = process_frm_user_jobq->priority;
	node->scanner_req = process_frm_user_jobq->scanner_req;
	node->pid = process_frm_user_jobq->pid;
	node->next = NULL;

	if (!*qhead || !*q_last) {
		//initial state or case where there is only one element in the user job
		// q and that has been deleted.
		*q_last = *qhead = node;
	} else {
		(*q_last)->next = node;
		*q_last = node;
		(*q_last)->next = NULL;
	}
	return HDS_OK;
}
static int remove_first_ele_from_user_job_q(struct process_queue_t **user_job_q) {
	//remove the first element
	struct process_queue_t *to_be_deleted = *user_job_q;
	if (hds_core_state.user_job_q == NULL ) {
		swarn("User Job queue is empty !! Nothing to remove");
		return HDS_OK;
	}
	//	var_debug("Removing [%u] next [%u]",to_be_deleted->page.page_id,((*node)->next)->page.page_id);
	//	var_debug("Trying to remove page %u", (*node)->page.page_id);
	// first case itself should handle head being null. this one is not needed.
	if (!*user_job_q) {
		serror("Trying to delete a NULL page from user job q");
		return HDS_ERR_MEM_FAULT;
	}
	// &hds_
	hds_core_state.user_job_q = (*user_job_q)->next;
	free(to_be_deleted);
	return HDS_OK;
}

static int insert_process_to_q_from_dispatch_list(
		struct process_queue_t **qhead, struct process_queue_t **q_last,
		struct hds_process_t *process_frm_dispatch_list) {
	struct process_queue_t *node = NULL;
	node = (struct process_queue_t *) malloc(sizeof(struct process_queue_t));
	if (!node) {
		serror("malloc: failed ");
		return HDS_ERR_NO_MEM;
	}
	//TODO: since granularity is in seconds,get time in seconds
	node->arrival_time = gettime_in_nsecs();

	node->cpu_req = process_frm_dispatch_list->cpu_req;
	node->memory_req = process_frm_dispatch_list->memory_req;
	node->printer_req = process_frm_dispatch_list->printer_req;
	node->priority = process_frm_dispatch_list->priority;
	node->scanner_req = process_frm_dispatch_list->scanner_req;
	node->pid = process_frm_dispatch_list->pid;
	node->next = NULL;

	if (!*qhead || !*q_last) {
		//initial state
		*q_last = *qhead = node;
	} else {
		(*q_last)->next = node;
		*q_last = node;
		(*q_last)->next = NULL;
	}
	return HDS_OK;
}
//static struct process_queue_t * get_first_element(struct process_queue_t *qhead) {
//
//}
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

void print_current_cpu_stats() {
	/*
	 * We will print info about active process,next_to_run_process and available
	 * resources when demanded.
	 */
	sprint_result("<C>System Statistics");
	sprint_result("<C>Resource Status");
	sprint_result("\t\t\tMemory\tPrinter\tScanner");
	vprint_result("Max. Resources: \t%d\t%d\t%d",
			hds_config.max_resources.memory, hds_config.max_resources.printer,
			hds_config.max_resources.scanner);
	pthread_mutex_lock(&max_available_resource.avail_resource_mutex);
	vprint_result("Avail. Resources: \t%d\t%d\t%d",
			max_available_resource.avail_memory,
			max_available_resource.avail_printer,
			max_available_resource.avail_scanner);
	pthread_mutex_unlock(&max_available_resource.avail_resource_mutex);
	sprint_result("<C>Process Status");
	sprint_result("\t\t PID\tPRI\tCPU_REQ\tMEM_REQ\tPRN_REQ\tSCN_REQ");
	pthread_mutex_lock(&hds_core_state.active_process_lock);
	vprint_result("Active:\t\t%d\t%d\t%d\t%d\t%d\t%d",
			hds_core_state.active_process.pid,
			hds_core_state.active_process.priority,
			hds_core_state.active_process.cpu_req,
			hds_core_state.active_process.memory_req,
			hds_core_state.active_process.printer_req,
			hds_core_state.active_process.scanner_req);
	pthread_mutex_unlock(&hds_core_state.active_process_lock);
	pthread_mutex_lock(&hds_core_state.next_to_run_process_lock);
	vprint_result("NextSchdld:\t%d\t%d\t%d\t%d\t%d\t%d",
			hds_core_state.next_to_run_process.pid,
			hds_core_state.next_to_run_process.priority,
			hds_core_state.next_to_run_process.cpu_req,
			hds_core_state.next_to_run_process.memory_req,
			hds_core_state.next_to_run_process.printer_req,
			hds_core_state.next_to_run_process.scanner_req);
	pthread_mutex_unlock(&hds_core_state.next_to_run_process_lock);
	// we can print the original loaded process dispactch queue
	// we will need to maintain one extra field in process structure that is name.
	// it can be populated at runtime by dispatcher and will help in giving a
	// proper identification. names will be like: P1,P2,P3 etc. Note that name will
	// be just an integer but when accessing it, prefix it by 'P'
	// also we will maintaina execution queue which will be an array maintained
	// by cpu thread which will just point out that for every quantum which process
	// was executed. like : 1,3,3,3,2,1,4,1 but will be printed as :
	// P1,P3,P3,P3,P2,P1,P4,P1
	// also print lets say 20 such entries per line only.
}
void *hds_stats_manager(void *args) {
	while (1) {
		if (hds_state.shutdown_in_progress == true) {
			break;
		}
		if (hds_state.stats_manager_active == true) {
			clear_result_window();
			print_current_cpu_stats();
			sleep(1);
		} else {
			sleep(1);
		}
	}
	sdebug("stats manager shutting down");
	pthread_exit(NULL );
}

// //////////// Memory mgmt API ////////////////////////
/**
 * @brief Allocate memory for the given PID.
 * @param pid The process id for which memory allocation request has been made.
 * @param mem_req Memory required in MBs.
 * @return A memory handle on success or 0 indicating failure.
 */
MEM_HANDLE allocate_mem(unsigned int pid, unsigned int mem_req) {
	/*
	 * Note: For any allocation made here , the changes must be reflected in
	 * hds_core_state.global_pool_info as well as in hds_global_resource_state.
	 * ------------------------------------------------------------------
	 *
	 * 1. check if memory is available in hds_core_state.global_memory_pool_info
	 * 2. Look into free pool if we can allocate a contiguous space.
	 * 3. if not , start searching for free mem blocks in hds_core_state.mem_block_list
	 * 		See if any such block can meet our requirement using <best fit strategy>.
	 * 4. If still our requirement is not met, (but memory is available, most
	 * 		probably because it's in smaller freed blocks, but these blocks
	 * 		have not yet been merged).
	 * 5. If step 4, then  initiate memory_compaction and see from step 2,
	 * 		if our requirement can be met now.
	 * 6. If still, our requirement is not met (and memory is available), then
	 * 		something went wrong. Throw EXCEPTION.
	 * 7. If all OK, do the allocation, update hds_core_state.global_pool_info,
	 * 		hds_core_state.mem_blocks_list and max_available_resource.memory.
	 * 		Finally return the mem_handle.
	 *
	 * mem_block.start_pos and mem_block.end_pos both are inclusive to a process.
	 * For e.g. if for a process (start=12,end=25) then memory_size is
	 * (25-12 +1) = 14 .
	 *
	 */
	unsigned int mem_handle;
	if (hds_core_state.global_memory_info.mem_available < mem_req) {
		return 0;
	}

	//1. try allocating from pool
	mem_handle = allocate_from_free_pool(pid, mem_req);
	if (mem_handle) {
		return mem_handle;
	}

	//2. Not enough memory available in free pool as contigous space
	// check for freed blocks
	mem_handle = find_smallest_free_mblock(pid, mem_req);
	if (mem_handle != -1) {
		var_debug("Found handle %d using best-fit for pid: %d", mem_handle, pid);
		return mem_handle;
	}

	//3. We could not find a free mem_block which has sufficient space to
	// fulfill this request. Now we will try to perform memory compaction
	// again check if we can allocate from free pool.
	_consolidate_memory();

	//4. Since we have done compaction, there should have been some space in
	// free pool
	mem_handle = allocate_from_free_pool(pid, mem_req);
	if (!mem_handle) {
		//this is bad.
		var_error("Out of Memory. Request:(pid=%d,req=%d)", pid, mem_req);
		return 0;
	}
	return mem_handle;
}
static int allocate_from_free_pool(unsigned int pid, int mem_req) {
	struct mem_block_t mb;
	//check free pool for contiguous free space
	int mem_available = hds_core_state.global_memory_info.free_pool_end
			- hds_core_state.global_memory_info.free_pool_start + 1;

	if (mem_available >= mem_req) {
		//allocating from free pool
		mb.pid = pid;
		mb.size = mem_req;
		mb.start_pos = hds_core_state.global_memory_info.free_pool_start;
		mb.end_pos = mb.start_pos + mb.size - 1;

		//now attach a node to hds_core_state.mem_block_list
		if (insert_mem_block_to_list(&mb) != HDS_OK)
			return 0; //return 0 as mem_handle in case of failure
		else {
			//lets update global resources
			hds_core_state.global_memory_info.free_pool_start = mb.end_pos + 1;
			hds_core_state.global_memory_info.mem_available =
					hds_core_state.global_memory_info.mem_available - mb.size;
			max_available_resource.avail_memory =
					max_available_resource.avail_memory - mb.size;
			var_debug("Created new handle %d from free pool for pid: %d",
					mb.mem_block_id, pid);
			return mb.mem_block_id;
		}
	}
	return 0;
}
static int insert_mem_block_to_list(struct mem_block_t *mblock_to_attached) {
	struct mem_block_t *node = NULL;
	node = (struct mem_block_t *) malloc(sizeof(struct mem_block_t));
	if (!node) {
		serror("malloc: failed ");
		return HDS_ERR_NO_MEM;
	}
	//fill up proper values
	node->mem_block_id = hds_core_state.global_memory_info.mem_block_id_counter;
	// also modify mblock_to_attached so that we let the caller have
	// the mem_block_handle
	mblock_to_attached->mem_block_id =
			hds_core_state.global_memory_info.mem_block_id_counter;
	hds_core_state.global_memory_info.mem_block_id_counter++;

	node->pid = mblock_to_attached->pid;
	node->size = mblock_to_attached->size;
	node->start_pos = mblock_to_attached->start_pos;
	node->end_pos = mblock_to_attached->end_pos;
	node->next = NULL;

	if (!hds_core_state.mem_block_list || !hds_core_state.mem_block_list_last) {
		//initial state
		hds_core_state.mem_block_list = hds_core_state.mem_block_list_last =
				node;
	} else {
		hds_core_state.mem_block_list_last->next = node;
		hds_core_state.mem_block_list_last = node;
		hds_core_state.mem_block_list_last->next = NULL;
	}
	return HDS_OK;
}
/**
 * @brief finds the smallest memory block in hds_core_state.mem_block_list using
 *        best fit strategy.
 * @param mem_req The amount of memory required.
 * @param pid PID for which this request is being made.
 * @return Return the mem_block_id as memory handle after marking this page
 *          as belonging to this PID.
 */
static int find_smallest_free_mblock(unsigned int pid, int mem_req) {
	/*
	 * A free block is identified as one for which pid is set to -1.
	 */
	struct mem_block_t *node = hds_core_state.mem_block_list;
	if (!node)
		return -1;
	struct mem_block_t *smallest_free_mblock = NULL;
	//iterate over list of mem_blocks and find the smallest free block
	while (node) {
		// only consider nodes which are free and have sufficient size to meet
		// current request
		if ((node->pid == -1) && ((node->size) >= mem_req)) {
			if (!smallest_free_mblock) {
				//initial case
				smallest_free_mblock = node;
			} else {
				// smallest possible node using best-fit strategy
				if ((smallest_free_mblock->size) > (node->size)) {
					smallest_free_mblock = node;
				}
			}
		}
		node = node->next;
	}
	if (!smallest_free_mblock) {
		return -1;
	} else {
		//make this block valid and associate it with this pid
		smallest_free_mblock->pid = pid;
		return smallest_free_mblock->mem_block_id;
	}
}
void free_mem(unsigned int pid, MEM_HANDLE mem_handle) {
	/*
	 * 1. Check if this mem_handle is valid and that it indeed belongs to
	 *  	this PID (Security ?).
	 * 2. If yes, then mark this block as inactive by setting the PID of this
	 * 		handle to -1.
	 */
	struct mem_block_t *mb = hds_core_state.mem_block_list;
	if (!mb) {
		swarn("No mem_block_list: Invalid free() request.");
		return;
	}
	var_debug("free(): request from: pid %d handle: %d",pid,mem_handle);
	while (mb) {
		if (mb->mem_block_id == mem_handle) {
			//check if this handle indeed belongs to this pid
			if (mb->pid == pid) {
				//free up this block
				var_debug("Freeing mem_block: %d", mem_handle);
				mb->pid = -1;
				return;
			} else {
				// this is an access violation
				var_error(
						"Memory access violation: pid: %d tried to free handle %d, which does not belong to it.",
						pid, mem_handle);
				return;
			}
		}
		mb = mb->next;
	}
}
/**
 * @brief Cleanup the mem_block_list present in hds_core_state
 */
static void cleanup_mem_block_list(struct mem_block_t *mblock_list){
	struct mem_block_t *prev_node = mblock_list, *cur_node = mblock_list;
		if (!mblock_list){
			hds_core_state.mem_block_list_last = NULL;
			return ;
		}
		print_memory_maps();

		do {
			prev_node = cur_node;
			cur_node = cur_node->next;
			free(prev_node);
		} while (cur_node);
		//we have emptied page_table so set the pointer to null.
		hds_core_state.mem_block_list = hds_core_state.mem_block_list_last = NULL;
}
/**
 * @brief This routine performs memory compaction. It tries to add all freed
 * 			blocks to contigous free pool while maintaining the same PID to
 * 			mem_handle mapping. It will merge the smaller free blocks back to
 * 			the global_memory_pool.
 *
 */
static void _consolidate_memory() {
	/*
	 * 1. We will keep the mem_handle to PID mapping same and size of all active
	 * 		blocks also cant change. Usual attributes can change. (Right ?)
	 *   Only change will be mem_block.start_pos and mem_block.end_pos.
	 * 2. How to do it ?
	 * 		2.1) Sort all such blocks based on their start pos.
	 * 		2.2) Then arrange all blocks in the increasing order of start_pos
	 * 			 while allocating correct <size> of memory.
	 *
	 * 	For e.g.
	 * 	 p1(5,9) ,p2(2,3), p3(12-15), p4(22-25)
	 * 	 Then sort then as per their start pos.
	 * 	 ->    p2(2,3) , p1(5,9) p3(12,15) p4(22,25)
	 * 	sizes:    2          5         4         4          :(end_pos-start_pos+1)
	 *
	 *	Compact: p2(65,67),p2(68,73),p3(74,78),p4(79,83)  ... free_pool_start = 84
	 * 	Now assumming entire pool has been reset, start allocating from position
	 * 	65 (since first 64 MB is reserved for realtime processes.)
	 * 	allocation can be made as :
	 * 	--------------------
	 * 	free_pool_start_index =65
	 * 	for node in sorted_mem_block_list:
	 * 		  mem_size_of_cur_node = node.end_pos - node.start_pos + 1
	 * 		  node.start_pos = free_pool_start_index
	 * 		  node.end_pos = node.start_pos + mem_size_of_cur_node
	 * 		  free_pool_start_index = free_pool_start_index + mem_size_of_cur_node
	 *
	 * 	#end of compaction.
	 */
	struct mem_block_t *mb = NULL;
	unsigned int size;
	sdebug("Performing memory compaction.");
	//1. sort the mem_block_list
	MergeSort(&hds_core_state.mem_block_list);
	mb = hds_core_state.mem_block_list;

	hds_core_state.global_memory_info.free_pool_start = 65;

	while (mb) {
		size = mb->size;
		mb->start_pos = hds_core_state.global_memory_info.free_pool_start;
		mb->end_pos = mb->start_pos + size - 1;
		hds_core_state.global_memory_info.free_pool_start =
				hds_core_state.global_memory_info.free_pool_start + size;
		mb = mb->next;
	}
}
void print_memory_maps(){
	struct mem_block_t *node = NULL;
	if (!hds_core_state.mem_block_list){
		return;
	}
	node = hds_core_state.mem_block_list;
	sdebug("mem_handle\tpid\tsize\tstart_pos\tend_pos");
	while(node){
		var_debug("%d\t%d\t%d\t%d\t%d",node->mem_block_id,node->pid,node->size,node->start_pos,node->end_pos);
		node= node->next;
	}
}
/**
 * @brief The merge sort routine.
 * @param headRef The pointer to first mblock in the list.
 */
void MergeSort(struct mem_block_t** headRef) {
	struct mem_block_t* head = *headRef;
	struct mem_block_t* a;
	struct mem_block_t* b;

	if ((head == NULL )|| (head->next == NULL)){
	return;
}

	FrontBackSplit(head, &a, &b);

	MergeSort(&a);
	MergeSort(&b);

	*headRef = SortedMerge(a, b);
}
/**
 * @brief Merge two sorted sub-lists. This routine is invoked when recursion is
 * 		  winding up.
 * @param a The first sub-list
 * @param b The second sub-list
 * @return The merged list
 */
struct mem_block_t* SortedMerge(struct mem_block_t* a, struct mem_block_t* b) {
	struct mem_block_t* result = NULL;

	if (a == NULL )
		return (b);
	else if (b == NULL )
		return (a);

	if (a->start_pos <= b->start_pos) {
		result = a;
		result->next = SortedMerge(a->next, b);
	} else {
		result = b;
		result->next = SortedMerge(a, b->next);
	}
	return (result);
}

/**
 * @brief This routine splits a given linked list in to two halves. If the
 * 			length is odd, the extra node should go in the front list. Uses
 * 			the fast/slow pointer strategy.
 * @param source The list which is to be split.
 * @param frontRef The first sub-list after splitting
 * @param backRef The second sub-list after splitting
 */
void FrontBackSplit(struct mem_block_t* source, struct mem_block_t** frontRef,
		struct mem_block_t** backRef) {
	struct mem_block_t* fast;
	struct mem_block_t* slow;
	if (source == NULL || source->next == NULL ) {
		*frontRef = source;
		*backRef = NULL;
	} else {
		slow = source;
		fast = source->next;

		while (fast != NULL ) {
			fast = fast->next;
			if (fast != NULL ) {
				slow = slow->next;
				fast = fast->next;
			}
		}

		*frontRef = source;
		*backRef = slow->next;
		slow->next = NULL;
	}
}

