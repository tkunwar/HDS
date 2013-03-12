/**
 * @file hds_common.c
 * @brief This file includes common code which will be used by other sections
 * 		  of hds. It includes logging macros,routines and init routines.
 */
#include "hds_common.h"
//=========== routines declaration============
static int calculate_msg_center_position(int msg_len);
static void log_msg_to_console(const char* msg, log_level_t level);
static void init_window_pointers();
static void init_cdks_pointers();
static void destroy_hds_window(); //main routine for deleting windows
static void redraw_cdkscreens(); //draw cdkscrens after drawing a popup window or
//===========================================
/**
 * @brief Destroy cdk screen pointers that are present in hds_state.
 */
void destroy_cdkscreens() {
	destroyCDKScreen(hds_state.output_win.cdksptr);

	//first desteoy the entry widget
	destroyCDKEntry(hds_state.read_input);
	destroyCDKScreen(hds_state.input_win.cdksptr);

	destroyCDKScreen(hds_state.console_win.cdksptr);
}
/**
 * @brief Init the hds_state
 *
 * Initialize hds_state structure which is accessible throughout. Ensure
 * that pointers and other variables are initialized to null or empty or
 * relevant values.
 * @return Return ACP_OK if operation was successful else an error code.
 */
int init_hds_state() {
	//reset the hds_state
	init_window_pointers();
	init_cdks_pointers();
	hds_state.color_ok = false;
	hds_state.gui_ready = false;
	hds_state.cursesWin = NULL;
	/*
	 * now for all three windows we set the cur_x and cur_y to 0
	 */

	hds_state.output_win.beg_x = hds_state.output_win.beg_y =
			hds_state.output_win.cur_x = hds_state.output_win.cur_y =
					hds_state.output_win.height = hds_state.output_win.width =
							0;

	hds_state.input_win.beg_x = hds_state.input_win.beg_y =
			hds_state.input_win.cur_x = hds_state.input_win.cur_y =
					hds_state.input_win.height = hds_state.input_win.width = 0;

	hds_state.console_win.beg_x = hds_state.console_win.beg_y =
			hds_state.console_win.cur_x = hds_state.console_win.cur_y =
					hds_state.console_win.height = hds_state.console_win.width =
							0;
	//init cdk specific pointers
	hds_state.console = NULL;
	hds_state.cursesWin = NULL;

	//init the log_buffer_lock
	if ((pthread_mutex_init(&hds_state.log_buffer_lock, NULL )) != 0) {
		fprintf(stderr, "\nprocess1_mutex_init failed");
		return HDS_ERR_HDS_STATE_INIT;
	}
	hds_state.shutdown_in_progress = false;
	hds_state.shutdown_completed = false;
	hds_state.recieved_signal_code = 0;
	//TODO: also init global_labels

	hds_state.log_ptr = NULL;
	hds_state.read_input = NULL;
	return HDS_OK;
}
int open_log_file() {
	if (!(hds_state.log_ptr = fopen(HDS_LOG_FILE, "w")) < 0) {
		fprintf(stderr, "Failed to open log_file: %s", hds_config.log_filename);
		return HDS_ERR_CONFIG_ABORT;
	}
	return HDS_OK;
}
/**
 * @brief Initialize window pointers of hds_state
 */
static void init_window_pointers() {
	hds_state.output_win.wptr = hds_state.input_win.wptr =
			hds_state.console_win.wptr = NULL;
}
/**
 * @brief Initialize cdk screen pointers of hds_state
 */
static void init_cdks_pointers() {
	hds_state.output_win.cdksptr = hds_state.input_win.cdksptr =
			hds_state.console_win.cdksptr = NULL;
	hds_state.master_screen = NULL;
}
/**
 * @brief Generic logger routine.
 *
 * Prints a message at LINES-1,COLS/2. Avoid using it. as it will overwrite
 * last logged message. Also requires a full screen refresh.
 * @param msg The message which is to be logged
 * @param log_level The priority of this message- whether it's a debug,warning
 * 			or error message.
 */
void log_generic(const char* msg, log_level_t log_level) {
	if (hds_state.gui_ready) {
		//log this msg to console
		log_msg_to_console(msg, log_level);
		return;
	}
	int msg_len = strlen(msg);
	int beg_x = calculate_msg_center_position(msg_len);
	/*
	 * if previously a message was logged, then logging this msg will
	 * corrupt the display. so we will delete this line and write the msg
	 */
	move(LINES - 1, 0);
	deleteln();
	//donot forget to add horizontal padding
	fprintf(hds_state.log_ptr, "%s\n", msg);
	fflush(hds_state.log_ptr);
	mvprintw(LINES - 1, beg_x + hds_state.hori_pad, "%s", msg);
	refresh();
}

/**
 * @brief Finds central position where a message can be displayed.
 *
 * Returns the position where message should be displayed such that	it appears
 * in the center of screen.
 * @param msg_len Length of message to be displayed
 * @return The beginning location from where message will be displayed
 */
static int calculate_msg_center_position(int msg_len) {
	return ((MIN_COLS - msg_len) / 2);
}

/**
 * @brief Converts error codes into understandable error strings.
 * @param error_code The error code which is to be converted
 */
void report_error(error_codes_t error_code) {
	char msg[LOG_BUFF_SIZE];
	switch (error_code) {
	case HDS_ERR_INIT_UI:
		snprintf(msg, LOG_BUFF_SIZE, "Failed to initialize curses UI!");
		break;
	case HDS_ERR_NO_COLOR:
		snprintf(msg, LOG_BUFF_SIZE, "No color support !");
		break;
	case HDS_ERR_COLOR_INIT_FAILED:
		snprintf(msg, LOG_BUFF_SIZE, "Failed to initialize color support !");
		break;
	case HDS_ERR_WINDOW_SIZE_TOO_SHORT:
		snprintf(msg, LOG_BUFF_SIZE,
				"Window size too short.Must be %d LINESx %d COLS!", MIN_LINES,
				MIN_COLS);
		break;
	case HDS_ERR_CDK_CONSOLE_DRAW:
		snprintf(msg, LOG_BUFF_SIZE, "Failed to draw CDK window: console");
		break;
	case HDS_ERR_WINDOW_CONFIG_MISMATCH:
		snprintf(msg, LOG_BUFF_SIZE,
				"Window configuration mismatch, check hds_config.h");
		break;
	case HDS_ERR_THREAD_INIT:
		snprintf(msg, LOG_BUFF_SIZE, "Failed to create threads");
		break;
	case HDS_ERR_TIME_READ:
		snprintf(msg, LOG_BUFF_SIZE, "Failed to read timestamp");
		break;
	case HDS_ERR_GENERIC:
		snprintf(msg, LOG_BUFF_SIZE, "An unknown error has occurred!");
		break;
	default:
		snprintf(msg, LOG_BUFF_SIZE, "An unknown error has occurred!");
		break;
	}
	//just report the error
	log_generic(msg, LOG_ERROR);
}
/**
 * @brief Destroys all windows and clears UI.
 *
 * Call it when hds is being closed down.
 */
void close_ui() {
	sdebug("Closing down UI");
	//delete windows
	destroy_hds_window();
	//all windows and associated cdkscreens are gone now
	hds_state.gui_ready = false;

//    sdebug("Press any key to exit!!");
//    getch();

	/* Exit CDK. */
	endCDK();
	endwin();
}
/**
 * @brief Destroys all window pointers.
 */
static void destroy_hds_window() {
	//destroy window as well as evrything it contains--label,cdkscreens,widgets
	//etc.
	//order can be widgets->cdkscreen->window_pointers
	//place any window specific code in the case that matches

	//first cdkscreens
	destroy_cdkscreens();

	//now window pointers
	delwin(hds_state.output_win.wptr);
	delwin(hds_state.input_win.wptr);
	delwin(hds_state.console_win.wptr);

}
/**
 * @brief Brings focus to console window
 *
 * When processing user response through key presses one of the keys might
 * trigger events in other windows causing console window to loose focus. Then it
 * becomes necessary to transfer focus to console window. For e.g. as F2 will
 * bring Help window, giving focus to console is necessary else, program will
 * no longer be able to get attention.
 */
void set_focus_to_console() {
	activateCDKSwindow(hds_state.console, 0);
}

/**
 * @brief log message to console window
 *
 * Log a message to CDK console that we have created. When UI is fully ready,
 * then log_generic() will call this routine only.
 * @param msg The message which is to be logged
 * @param log_level One of LOG_DEBUG,LOG_WARN or LOG_ERR
 */
static void log_msg_to_console(const char *msg, log_level_t log_level) {
	char log_msg[LOG_BUFF_SIZE];
	/*
	 * color scheme: <C> is for centering
	 * </XX> defines a color scheme out of 64 available
	 * <!XX> ends turns this scheme off
	 *
	 * some of interest:
	 * 	32: yellow foreground on black background
	 * 	24: green foreground on black background
	 * 	16: red foreground on black background
	 * 	17: white foreground on black background
	 */
	switch (log_level) {
	case LOG_DEBUG:
		snprintf(log_msg, LOG_BUFF_SIZE, "<C></24>%s<!24>", msg);
		break;
	case LOG_WARN:
		snprintf(log_msg, LOG_BUFF_SIZE, "<C></32>%s<!32>", msg);
		break;
	case LOG_ERROR:
		snprintf(log_msg, LOG_BUFF_SIZE, "<C></16>%s<!16>", msg);
		break;
	default:
		//centre placement but no color
		snprintf(log_msg, LOG_BUFF_SIZE, "<C>%s", msg);
		break;
	}
	fprintf(hds_state.log_ptr, "%s\n", msg);
	fflush(hds_state.log_ptr);
	addCDKSwindow(hds_state.console, log_msg, BOTTOM);
}
/**
 * @brief Redraw cdk screens.
 *
 * After a popup screen is drawn in CDK, it is necessary to redraw under-
 * lying CDK windows otherwise they will not be displayed on the screen and
 * program is left in an inconsistent state.
 */
static void redraw_cdkscreens() {
	drawCDKScreen(hds_state.output_win.cdksptr);
	drawCDKScreen(hds_state.input_win.cdksptr);
	drawCDKScreen(hds_state.console_win.cdksptr);
}
/**
 * @brief Display help options
 *
 * Displays help options in a popup window. Also do a redraw of all cdkscreens
 * since we will be displaying popup windows in master_cdkscreen.
 */
void display_help() {
	const char *mesg[15];
	const char *buttons[] = { "<OK>" };
	mesg[0] = "<C>This program shows how a compressed paging mechanism has";
	mesg[1] = "<C>advantage over traditional paging systems.";
	mesg[2] = "<C></U>Project authors";
	mesg[3] = "<C>Amrita,Rashmi and Varsha";

	popupDialog(hds_state.master_screen, (CDK_CSTRING2) mesg, 4,
			(CDK_CSTRING2) buttons, 1);
	redraw_cdkscreens();
}

/**
 * @brief Returns current time in nanoseconds
 * @return A long integer representing time in nano seconds.
 */
long int gettime_in_nsecs() {
	struct timespec ts;
	if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == -1) {
		serror("Failed to get timestamp");
		return 0;
	}

	return ((ts.tv_nsec / 1000));
}

void gettime_in_mseconds() {
	struct timeval start, end;

	long mtime, seconds, useconds;

	gettimeofday(&start, NULL );
	usleep(2000);
	gettimeofday(&end, NULL );

	seconds = end.tv_sec - start.tv_sec;
	useconds = end.tv_usec - start.tv_usec;

	mtime = ((seconds) * 1000 + useconds / 1000.0) + 0.5;

	printf("Elapsed time: %ld milliseconds\n", mtime);
}
