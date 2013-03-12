/**
 * @file hds_ui.h
 * @brief header file for hds_ui.c
 */
/* WINDOW drawing rules
 * --------------------
 * window's coordinates are specified in its handle in hds_state
 *  do not cross these coordinates for.e.g writing must be in between
 *  console.wptr.beg_x+1 till console.wptr.height-1
 *  and width:
 *  	conso;e.wptr.beg_y+1 till console.wptr.width-1
 *
 */
#ifndef _HDS_GUI_H_
#define _HDS_GUI_H_

#ifndef HDS_DTYPES_H_
	#include "hds_dtypes.h"
#endif

#include "hds_common.h"
//routines
int init_curses(); //initialize curses mode and other secondary routines
int hds_ui_main(); //draw the curses UI for HDS
#endif

