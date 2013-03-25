/**
 * @file hds_common.h
 * @brief header file for hds_common.c
 */
#ifndef _HDS_COMMON_H_
#define _HDS_COMMON_H_

#ifndef HDS_DTYPES_H_
#include "hds_dtypes.h"
#endif

#include<unistd.h>
#include<execinfo.h>
#include<stdlib.h>
#include <pthread.h>
#include<curses.h>
#include<signal.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
#include <error.h>
#include <errno.h>

#include "hds_config.h"
#include "hds_error.h"
#include "cdk_wrap.h"

#ifdef HAVE_XCURSES
char *XCursesProgramName = "HOST";
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif
/**
 * @def PARENT_WAIT_FOR_CHILD_THREADS
 * @brief Defines how long parent thread waits for all child threads before
 * 		doing a final exit.
 */
#define PARENT_WAIT_FOR_CHILD_THREADS 3
/**
 * @def LOG_BUFF_SIZE
 * @brief Size of log buffer used in logging macros.
 */
#define LOG_BUFF_SIZE 512
#define HDS_PAGE_SIZE 4096
/**
 * @struct window_state_t
 * @brief A structure for holding window related information.
 */
struct window_state_t {
	WINDOW *wptr;
	int beg_x, beg_y, height, width; //window coordinates
	int cur_x, cur_y;
	CDKSCREEN *cdksptr;
};
/**
 * @enum storage_loc_t
 * @brief Tells about current location of a given page.
 */
typedef enum {
	SL_NCM, /**< storage location is non-compressed-memory */
	SL_CC, /**< storage location is compressed cache*/
	SL_SWAP /**< storage location is swap*/
} storage_loc_t;
/**
 * @struct HDS_STATE
 * @brief Main structure which holds state information for hds. Almost all
 * major variables needed by hds are present in this structure. Note that
 * variables in this structure are accessible throughout hds.
 */
struct HDS_STATE {
	struct window_state_t output_win, input_win, console_win;
	// ----------global labels------------------------
	bool color_ok;
	char log_buffer[LOG_BUFF_SIZE];
	char result_msg[LOG_BUFF_SIZE];

	bool curses_ready;
	bool gui_ready;
	int hori_pad, vert_pad;

	//cdk specific window pointers
	CDK_PARAMS params;
	CDKSWINDOW *console;
	CDKSWINDOW *output_screen;
	CDKSCREEN *master_screen;
	WINDOW *cursesWin; //main curses window--stdscr
	CDKENTRY *read_input;
	// mutex for getting lock if log_buffer
	pthread_mutex_t log_buffer_lock;
	bool shutdown_in_progress;
	bool shutdown_completed;
	int recieved_signal_code;
	FILE *log_ptr;

	pthread_t hds_dispatcher;
	pthread_t hds_scheduler;
	pthread_t hds_cpu;

} hds_state;

#define sprint_result(s) snprintf(hds_state.result_msg, LOG_BUFF_SIZE,s );\
		write_to_result_window(hds_state.result_msg,1);

#define vprint_result(s,...) snprintf(hds_state.result_msg, LOG_BUFF_SIZE,s,__VA_ARGS__ );\
		write_to_result_window(hds_state.result_msg,1);

//some debug,warning and error macros
/**
 * @def sdebug(s)
 * @brief Debug macro which accepts a single string. Message is expanded to
 * include location of call in the source file along with other information.
 * The message after expansion is saved in a global log buffer and then finally
 * passed to log_generic().
 */
#define sdebug(s) pthread_mutex_lock(&hds_state.log_buffer_lock); \
				   snprintf(hds_state.log_buffer, LOG_BUFF_SIZE,"[" __FILE__ ":%i] Debug: " s "",__LINE__); \
				   log_generic(hds_state.log_buffer,LOG_DEBUG); \
				   pthread_mutex_unlock(&hds_state.log_buffer_lock);
/**
 * @def var_debug(s, ...)
 * @brief Debug macro which accepts multiple parameters. Message is expanded to
 * include location of call in the source file along with other information.
 * The message after expansion is saved in a global log buffer and then finally
 * passed to log_generic().
 */
#define var_debug(s, ...) pthread_mutex_lock(&hds_state.log_buffer_lock); \
						   snprintf(hds_state.log_buffer,LOG_BUFF_SIZE, "[" __FILE__ ":%i] Debug: " s "" ,\
								    __LINE__,__VA_ARGS__);\
						   log_generic(hds_state.log_buffer,LOG_DEBUG);\
	                       pthread_mutex_unlock(&hds_state.log_buffer_lock);
/**
 * @def swarn(s)
 * @brief Warning macro which accepts single string. Message is expanded to
 * include location of call in the source file along with other information.
 * The message after expansion is saved in a global log buffer and then finally
 * passed to log_generic().
 */
#define swarn(s) pthread_mutex_lock(&hds_state.log_buffer_lock); \
				  snprintf(hds_state.log_buffer,LOG_BUFF_SIZE, "[" __FILE__ ":%i] Warning: " s "",__LINE__);\
		          log_generic(hds_state.log_buffer,LOG_WARN);\
		          pthread_mutex_unlock(&hds_state.log_buffer_lock);
/**
 * @def var_warn(s,...)
 * @brief Warning macro which accepts multiple parameters. Message is expanded to
 * include location of call in the source file along with other information.
 * The message after expansion is saved in a global log buffer and then finally
 * passed to log_generic().
 */
#define var_warn(s, ...) pthread_mutex_lock(&hds_state.log_buffer_lock); \
						  snprintf(hds_state.log_buffer,LOG_BUFF_SIZE, "[" __FILE__ ":%i] Warning: " s "",\
		                           __LINE__,__VA_ARGS__); \
		                  log_generic(hds_state.log_buffer,LOG_WARN);\
		                  pthread_mutex_unlock(&hds_state.log_buffer_lock);
/**
 * @def serror(s)
 * @brief Error macro which accepts single string. Message is expanded to
 * include location of call in the source file along with other information.
 * The message after expansion is saved in a global log buffer and then finally
 * passed to log_generic().
 */
#define serror(s) pthread_mutex_lock(&hds_state.log_buffer_lock); \
			       snprintf(hds_state.log_buffer,LOG_BUFF_SIZE, "[" __FILE__ ":%i] Error: " s "",__LINE__);\
		           log_generic(hds_state.log_buffer,LOG_ERROR);\
		           pthread_mutex_unlock(&hds_state.log_buffer_lock);
/**
 * @def var_error(s,...)
 * @brief Error macro which accepts variable parameters. Message is expanded to
 * include location of call in the source file along with other information.
 * The message after expansion is saved in a global log buffer and then finally
 * passed to log_generic().
 */
#define var_error(s, ...) pthread_mutex_lock(&hds_state.log_buffer_lock); \
						   snprintf(hds_state.log_buffer,LOG_BUFF_SIZE, "[" __FILE__ ":%i] Error: " s "",\
		                            __LINE__,__VA_ARGS__);\
		                   log_generic(hds_state.log_buffer,LOG_ERROR);\
	                       pthread_mutex_unlock(&hds_state.log_buffer_lock);

/*
 * Error reporting can be done through two ways. first is to use macros
 * which in turn use log_generic(). other is to use report_error() which accepts
 * error codes in place of message strings. report_error() after converting the
 * error code to a string calls log_generic().
 */

//routines
int init_hds_state();
void log_generic(const char* msg, log_level_t log_level);
void report_error(error_codes_t error_code); //report errors which are fatal
void close_ui();
void destroy_win(WINDOW *local_win);
void set_focus_to_console();
void destroy_cdkscreens();
void display_help();
long int gettime_in_nsecs();
int open_log_file();
void write_to_result_window(const char* msg,int num_rows);
void print_help();
void execute_commands(const char *command);


#endif
