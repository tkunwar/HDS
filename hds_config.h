/**
 * @file hds_config.h
 * @brief Define configuration options. variables that might alter the
 * 		 behaviour of program such as window sizes, swappiness factor etc. Also
 * 		 contains the structure for saving the information loaded from config
 * 		 file. Globally accessible .
 */
#ifndef _HDS_CONFIG_H_
#define _HDS_CONFIG_H_

#ifndef HDS_DTYPES_H_
	#include "hds_dtypes.h"
#endif

#include<stdio.h>
#include<unistd.h>
#include <stdlib.h>
#include <string.h>
#include "libconfig.h"
#include <stdbool.h>
#include "hds_error.h"

#define HDS_LOG_FILE "hds_output.log"
/*
 * the program's max. screen coordinates for our program
 */
/**
 * @def MIN_LINES
 * @brief Minimum lines needed for hds to init UI mode.
 */
#define MIN_LINES 40
/**
 * @def MIN_COLS
 * @brief Minimum columns needed for hds to init UI mode.
 */
#define MIN_COLS 100

/*
 * output windows will be given preference over console window.
 * so first sizes of the two windows will be calculated then console's size.
 * if needed, console size can be overriden.
 *
 * Size is represented in no. of lines or rows.
 */
/**
 * @def OUTPUT_WINDOW_SIZE
 * @brief Size of status_window in no. of lines.
 */
#define OUTPUT_WINDOW_SIZE 22

/**
 * @def INPUT_WINDOW_SIZE
 * @brief Size of input_window in no. of lines.
 */
#define INPUT_WINDOW_SIZE 3
#define CONSOLE_SIZE 6



/**
 * @def RAW_MODE
 * @brief Turn on/off curses raw mode
 */
#define RAW_MODE off
/**
 * @def NOECHO
 * @brief Turn on/off curses no-echo feature
 */
#define NOECHO on
/**
 * @def KEYPAD_MODE
 * @brief Turn on/off curses keypad feature
 */
#define KEYPAD_MODE on
/**
 * @def CBREAK_MODE
 * @brief Turn on/off cbreak feature of curses
 */
#define CBREAK_MODE on
/**
 * @def HDS_CONF_FILE
 * @brief Path of Configuration file
 */
#define HDS_CONF_FILE "hds.conf"
/**
 * @struct hds_config_t
 * @brief Structure for storing information loaded from configuration file.
 */
struct process_config_t{
	int pid; // will be populated later on
	process_type_t type;
	int memory_req;
	int printer_req;
	int scanner_req;
	struct process_config_t *next;
};

struct max_resources_t{
	int memory;
	int printer;
	int scanner;
};
struct hds_config_t {
	struct process_config_t *process_config_list; //list of processes loaded from config file.
	struct process_config_t *process_config_last_element;
	struct max_resources_t max_resources;
	char log_filename[200];
	unsigned int process_id_counter;
} hds_config;

// --------routines-----------
int load_config();
void cleanup_process_config_list();
#endif

