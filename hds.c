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
//	if (load_config() == HDS_ERR_CONFIG_ABORT) {
//		exit(EXIT_FAILURE);
//	}
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
	//now start main worker threads
//	if (start_main_worker_threads() != HDS_OK) {
//		exit(EXIT_FAILURE);
//	}
	//now wait for user input and process key-presses
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
			// if anything other than F4 was pressed inject it
			// to entry widget.
			injectCDKEntry(hds_state.read_input,ch);
			//read anything from user
			info = activateCDKEntry(hds_state.read_input, 0);
			if (hds_state.read_input->exitType == vESCAPE_HIT) {
				sdebug("pressed ESC");
			} else if (hds_state.read_input->exitType == vNORMAL) {
				var_debug("Got: %s", info);
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
	hds_state.shutdown_in_progress = true;
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
//	if (pthread_join(hds_state.gmm_main_thread, NULL ) != 0) {
//		fprintf(stderr, "\nError in collecting thread: gmm_main");
//		exit(EXIT_FAILURE);
//	}
//	if (pthread_join(hds_state.cmm_main_thread,NULL)!=0){
//			fprintf(stderr,"\nError in collecting thread: cmm_main");
//			exit(EXIT_FAILURE);
//	}
	fclose(hds_state.log_ptr);
	sigemptyset(&sigact.sa_mask);
//	exit(EXIT_SUCCESS);
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

	sigaddset(&sigact.sa_mask, SIGKILL);
	sigaction(SIGKILL, &sigact, (struct sigaction *) NULL );
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
	// Attempt to perform cleanup_after_failure when we have SIGINT,SIGKILL and SIGQUIT
	// else give up
//	if ((sig == SIGINT) || (sig == SIGQUIT) || (sig == SIGKILL)) {
	sdebug("Prepairing to exit gracefully..");
	hds_state.shutdown_in_progress = true;
	//wait while cleanup_after_failure is finished
	//a mutex lock would have been more efficient
//		while(hds_state.shutdown_completed==false){
//			sleep(1);
//		}
//	}
//	exit(EXIT_FAILURE);
	hds_shutdown();
}
//int start_main_worker_threads() {
//	//create main hds_gmm thread
//	if (pthread_create(&hds_state.gmm_main_thread, NULL, hds_gmm_main, NULL )
//			!= 0) {
//		serror("\nThread1 creation failed ");
//		return ACP_ERR_THREAD_INIT;
//	}
//	return ACP_OK;
//}
