/**
 * @file hds_error.h
 * @brief This file defines error codes which will be used throughout hds.
 * @author Tej
 */
#ifndef HDS_ERROR_H_
#define HDS_ERROR_H_

#ifndef HDS_DTYPES_H_
	#include "hds_dtypes.h"
#endif

/**
 * @enum error_codes_t
 * @brief Defines error codes
 */
typedef enum {
    HDS_ERR_HDS_STATE_INIT = 2, /**< Error in initializing hds_state */
    HDS_ERR_NO_CONFIG_FILE,  /**<  Configuration file is missing */
    HDS_ERR_CONFIG_ABORT,  /**< Fatal error in accessing configuration file */
    HDS_ERR_INIT_UI ,    /**< Error initializing UI */
    HDS_ERR_WINDOW_SIZE_TOO_SHORT, /**< UI error: window size specified is too short */
    HDS_ERR_NO_COLOR,  /**< UI Error: curses does not have support for colors */
    HDS_ERR_COLOR_INIT_FAILED,/**< UI Error: Failed to activate color mode*/
    HDS_ERR_CDK_CONSOLE_DRAW,/**< Error drawing console window */
    HDS_ERR_WINDOW_CONFIG_MISMATCH,/**< UI Error: config mismatch in specified windows's sizes */
    HDS_ERR_DRAW_WINDOWS,/**< Error in drawing windows*/
    HDS_ERR_THREAD_INIT, /**< Error in thread creation*/
    HDS_ERR_TIME_READ,
    HDS_ERR_FILE_IO,
    HDS_ERR_NO_MEM,
    HDS_ERR_MEM_FAULT,
    HDS_ERR_NO_SUCH_ELEMENT,
    HDS_ERR_INVALID_PROCESS,
    HDS_ERR_NO_RESOURCE,
    HDS_ERR_GENERIC /** Generic error: not sure what it is, but it's fishy anyway*/
} error_codes_t;
/**
 * @enum log_level_t
 * @brief Defines logging levels.
 */
typedef enum {
    LOG_DEBUG, /**< The message is for debugging purposes. */
    LOG_WARN, /**< The message is a warning */
    LOG_ERROR /**< The message is an error */
} log_level_t;

#endif /* HDS_ERROR_H_ */
