/**
 * @file hds.c
 * @brief main source file for hds
 */
#include"hds.h"
//========== routines declaration============
void process_user_response();
void hds_shutdown(); //close down program -- successful termination
static void signal_handler(int);
void init_signals(void);
//int start_main_worker_threads();

struct sigaction sigact;
char *progname;

//===========================================
/**
 * @brief main function. Entry point for hds.
 * @param argc No. of parameters passed to this program
 * @param argv A pointer to list of parameters
 * @return Sucess or failure to OS
 */
int main(int argc, char *argv[]) {
	progname = *(argv);
	init_signals();

	CDKparseParams(argc, argv, &hds_state.params, CDK_CLI_PARAMS);

	//initialize hds_state
	if (init_hds_state() != HDS_OK) {
		fprintf(stderr, "hds_state_init: failed. Aborting");
		exit(EXIT_FAILURE);
	}
	//intialize configuration
	if (load_config() == HDS_ERR_CONFIG_ABORT) {
		exit(EXIT_FAILURE);
	}
	if (open_log_file() != HDS_OK) {
		exit(EXIT_FAILURE);
	}
	//initialize curses and cdk mode
	if (init_curses() != HDS_OK) {
		exit(EXIT_FAILURE);
	}

	//try to draw windows
	if (hds_ui_main() != HDS_OK) {
		exit(EXIT_FAILURE);
	}
	sdebug("HOST Shell: Intialized UI");
	//now start main worker threads
	if (start_main_worker_threads() != HDS_OK) {
		exit(EXIT_FAILURE);
	}
	//now wait for user input and process key-presses
	sdebug("Waiting for user command");
	process_user_response();
	exit(EXIT_SUCCESS);
}

/**
 * @brief Here we go in a loop to wait for user response (KBHITs) and take actions
 * accordingly.
 */
void process_user_response() {
	int ch;
	char *info;

	//the main loop for waiting for key presses
	while ((ch = getch())) {
		switch (ch) {
		case KEY_NPAGE: // pagedown
			// pagedown key will be bound to output window
			activateCDKSwindow(hds_state.console, 0);
			break;
		case KEY_PPAGE: //pageup
			// pagedown key will be assigned to console window
			activateCDKSwindow(hds_state.output_screen, 0);
			break;
		case KEY_F(4):
			hds_shutdown();
			break;
		case KEY_F(1):
		case KEY_F(2):
		case KEY_F(3):
		case KEY_F(5):
		case KEY_F(6):
		case KEY_F(7):
		case KEY_F(8):
		case KEY_F(9):
		case KEY_F(10):
		case KEY_F(11):
		case KEY_F(12):
			break;
		default:
//			if (hds_state.shutdown_in_progress){
//				hds_shutdown();
//			}
			// if anything other than F4 was pressed inject it
			// to entry widget.
			injectCDKEntry(hds_state.read_input, ch);
			//read anything from user
			info = activateCDKEntry(hds_state.read_input, 0);
			if (hds_state.read_input->exitType == vESCAPE_HIT) {
				sdebug("pressed ESC");
			} else if (hds_state.read_input->exitType == vNORMAL) {
				//write_to_result_window(info,1);
				execute_commands(info);
			}
			cleanCDKEntry(hds_state.read_input);
			break;
		}
	}
}
/**
 * @brief Prepares program for shutdown.
 *
 * Clean up any memory blocks,files etc. This is a callback routine which will
 * be executed whenever program terminates. In simple words, it is the last
 * routine to be executed.
 */
void hds_shutdown() {
	//other stuff related to cleanup but it's a normal cleanup
	//signal that we are shutting down
	if (hds_state.parent_pid != getpid()) {
		//simply exit we are a child process
		exit(EXIT_SUCCESS);
	}
//	hds_state.shutdown_in_progress = true;
//	var_debug("Parent sleeping for %d seconds to let child threads finish.",PARENT_WAIT_FOR_CHILD_THREADS);
	//wait for child threads
//	sleep(PARENT_WAIT_FOR_CHILD_THREADS);

	//shut down GUI
	if (hds_state.gui_ready == TRUE) {
		close_ui();
	} else {
		/* Exit CDK. */
		destroy_cdkscreens();
		endCDK();
	}
	//collect master threads
	if (pthread_join(hds_state.hds_dispatcher, NULL ) != 0) {
		fprintf(stderr, "\nError in collecting thread: hds_dispatcher");
		exit(EXIT_FAILURE);
	}
	if (pthread_join(hds_state.hds_scheduler, NULL ) != 0) {
		fprintf(stderr, "\nError in collecting thread: hds_scheduler");
		exit(EXIT_FAILURE);
	}
	if (pthread_join(hds_state.hds_cpu, NULL ) != 0) {
		fprintf(stderr, "\nError in collecting thread: hds_cpu");
		exit(EXIT_FAILURE);
	}
	if (pthread_join(hds_state.hds_stats_manager, NULL ) != 0) {
		fprintf(stderr, "\nError in collecting thread: hds_stats_manager");
		exit(EXIT_FAILURE);
	}
	fclose(hds_state.log_ptr);
	sigemptyset(&sigact.sa_mask);
	ExitProgram(EXIT_SUCCESS);
}
/**
 * @brief Intialize our signal handler
 *
 * Register what all signals can we mask. These are those signals
 * that we will be able to catch in our program.
 */
void init_signals(void) {
	sigact.sa_handler = signal_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, (struct sigaction *) NULL );

	sigaddset(&sigact.sa_mask, SIGSEGV);
	sigaction(SIGSEGV, &sigact, (struct sigaction *) NULL );

	sigaddset(&sigact.sa_mask, SIGBUS);
	sigaction(SIGBUS, &sigact, (struct sigaction *) NULL );

	sigaddset(&sigact.sa_mask, SIGQUIT);
	sigaction(SIGQUIT, &sigact, (struct sigaction *) NULL );

	sigaddset(&sigact.sa_mask, SIGHUP);
	sigaction(SIGHUP, &sigact, (struct sigaction *) NULL );

}
/**
 * @brief Signal handler routine.
 *
 * When a signal is raised then this routine will catch them. In this routine
 * we will mostly set flags and make an exit call so that hds_shutdown can
 * begin the shutdown and cleanup process.
 *
 * @param sig The signal code
 */
static void signal_handler(int sig) {
	//set the signal code that we got
	hds_state.recieved_signal_code = sig;
//	sdebug("Prepairing to exit gracefully..");
	hds_state.shutdown_in_progress = true;
	hds_shutdown();
}
int start_main_worker_threads() {
	//init the hds_resource state
	init_hds_resource_state();

	//first init the hds_core state
	init_hds_core_state();

	//create main hds_dispatcher thread
	if (pthread_create(&hds_state.hds_dispatcher, NULL, hds_dispatcher, NULL )
			!= 0) {
		serror("\nFailed to create dispatcher thread");
		return HDS_ERR_THREAD_INIT;
	}
	//create scheduler thread
	if (pthread_create(&hds_state.hds_scheduler, NULL, hds_scheduler, NULL )
			!= 0) {
		serror("\nFailed to create scheduler thread");
		return HDS_ERR_THREAD_INIT;
	}
	//create cpu_thread
	if (pthread_create(&hds_state.hds_cpu, NULL, hds_cpu, NULL ) != 0) {
		serror("\nFailed to create cpu thread");
		return HDS_ERR_THREAD_INIT;
	}
	// create_stats thread
	if (pthread_create(&hds_state.hds_stats_manager, NULL, hds_stats_manager,
			NULL ) != 0) {
		serror("\nFailed to create cpu thread");
		return HDS_ERR_THREAD_INIT;
	}
	return HDS_OK;
}

