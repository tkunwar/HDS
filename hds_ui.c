/**
 * @file hds_ui.c
 * @brief This source file contains code for rendering curses based UI.
 */
#include "hds_ui.h"
//========== routines declarations ==============
static WINDOW *create_newwin(int height, int width, int starty, int startx);

static int draw_windows(); //draw parent windows
static void draw_output_window();
static void calculate_padding(int *hor_padding, int *ver_padding);
static void init_cdk();
static int check_window_configuration();
static void draw_input_window();
static void create_cdkscreens();
static void draw_console();
static void draw_output_console();

static BINDFN_PROTO (XXXCB);
//===============================================
static int XXXCB(EObjectType cdktype GCC_UNUSED, void *object GCC_UNUSED,
		void *clientData GCC_UNUSED, chtype key GCC_UNUSED) {
	return (TRUE);
}

/**
 * @brief Initialise curses mode for hds.
 * @return Return HDS_OK if successful else an error code.
 */
int init_curses() {
	//master curses window
	hds_state.cursesWin = initscr();
	if (hds_state.cursesWin == NULL ) {
		fprintf(stderr, "init_curses(): initscr() failed. Aborting");
		return HDS_ERR_INIT_UI;
	}
	//enusre that we do not have any window configuration mismatch before we
	//proceed any futher
	if (check_window_configuration() != HDS_OK) {
		return HDS_ERR_INIT_UI;
	}

	//init cdk specific routines
	init_cdk();
	//now enable some specific features
	if (CBREAK_MODE)
		cbreak(); //disable line buffering
	if (RAW_MODE)
		raw(); //get everything that is passed to u
	if (KEYPAD_MODE)
		keypad(stdscr, TRUE); //enable to accept key presses like F1,F2 etc.
	if (NOECHO)
		noecho(); //disable echoing

	hds_state.curses_ready = true;
	return HDS_OK;
}
/**
 * @brief Main UI routine
 *
 * Draw master windows including menubar,gmm_window,cmm_window and console
 * by calling helper routines.
 * @return HDS_OK if successful else an error code.
 */
int hds_ui_main() {
	//first draw parent windows
	if (draw_windows() != HDS_OK) {
		return HDS_ERR_DRAW_WINDOWS;
	}
	create_cdkscreens();
	//now display intial display screen for these windows
	draw_output_window();
//	draw_info_window();
	draw_input_window();
	draw_console();
	draw_output_console();
	//set gui_ok in hds_state
	hds_state.gui_ready = TRUE;
	//every thing is fine so far
	return HDS_OK;
}
/**
 * @brief Create CDK screens.
 *
 * CDK screen is the actual window segment to which we write
 * in CDK.
 */
static void create_cdkscreens() {
	//by now we have window pointers, lets create cdkscreens out of them.
	hds_state.console_win.cdksptr = initCDKScreen(hds_state.console_win.wptr);
	hds_state.output_win.cdksptr = initCDKScreen(hds_state.output_win.wptr);
	hds_state.input_win.cdksptr = initCDKScreen(hds_state.input_win.wptr);

	//also create the master cdkscreen which will be used for popup messages.
	hds_state.master_screen = initCDKScreen(hds_state.cursesWin);
}
/**
 * @brief Draw master windows. Actual implementation.
 * @return HDS_OK if all ok else an error code
 */
static int draw_windows() {
	/*
	 * 1.	calculate size of these three windows. Size of menubar and console
	 * 		are provided in hds_config.h.
	 * 2.	draw these windows. Also report any error as fatal if any occur
	 * 		while drawing windows
	 * 3.	also draw border around these windows.
	 * 4.	store window pointers in the hds_state structure with relevant
	 * 		information for windows.
	 *
	 * 	Note: these three master windows have full width equal to COLS
	 */

	int horizontal_padding, vertical_padding;
	int height, width, startx, starty = 1;

	/*
	 * leave one line after meubar_win, one after info_bar and one for the
	 * generic_debug line.
	 */
	//calculate paddings if needed
	calculate_padding(&horizontal_padding, &vertical_padding);

	//draw  window output_win
	height = OUTPUT_WINDOW_SIZE;
	width = MIN_COLS;
	startx = horizontal_padding;
	starty = vertical_padding;
	if ((hds_state.output_win.wptr = create_newwin(height, width, starty,
			startx)) == NULL ) {
		serror("Error: failed to create window:menubar");
		return HDS_ERR_DRAW_WINDOWS;
	}
	//save the starting coordinates of menubar
	hds_state.output_win.beg_x = startx;
	hds_state.output_win.beg_y = starty;
	hds_state.output_win.cur_x = startx;
	hds_state.output_win.cur_y = starty;
	hds_state.output_win.height = height;
	hds_state.output_win.width = width;

	//draw window input_win
	starty += height + 1;
	startx = horizontal_padding;
	height = INPUT_WINDOW_SIZE;
	width = MIN_COLS;

	if ((hds_state.input_win.wptr = create_newwin(height, width, starty, startx))
			== NULL ) {
		serror("Error: failed to create window:info_win");
		return HDS_ERR_DRAW_WINDOWS;
	}
	hds_state.input_win.beg_x = startx;
	hds_state.input_win.beg_y = starty;
	hds_state.input_win.cur_x = startx;
	hds_state.input_win.cur_y = starty;
	hds_state.input_win.height = height;
	hds_state.input_win.width = width;

	//draw window console
	starty += height + 1;
	startx = horizontal_padding;
	height = MIN_LINES - (OUTPUT_WINDOW_SIZE + 1 + INPUT_WINDOW_SIZE + 1);
	width = MIN_COLS;

	if ((hds_state.console_win.wptr = create_newwin(height, width, starty,
			startx)) == NULL ) {
		serror("Error: failed to create window:console_win");
		return HDS_ERR_DRAW_WINDOWS;
	}
	return HDS_OK;
}
/**
 * @brief Creates a new curses window of specified parameters.
 * @param height height of window in no. of lines
 * @param width width of window in no. of columns
 * @param starty starting x-coordinate of this window
 * @param startx starting y-coordinate of this window
 * @return
 */
static WINDOW *create_newwin(int height, int width, int starty, int startx) {
	WINDOW *local_win;

	local_win = newwin(height, width, starty, startx);
	refresh();
	if (!local_win)
		return NULL ;
	box(local_win, 0, 0);

	/* 0, 0 gives default characters
	 * for the vertical and horizontal
	 * lines			*/
	wrefresh(local_win); /* Show that box 		*/
	return local_win;
}
/**
 * @brief Draws the menubar window.
 *
 * Menubar window has the menu controls such as F2,F3 and F4 for
 * help,config and exiting respectively.
 */
static void draw_output_window() {
	CDKLABEL *lblHelp;
	const char *lblText[2]; //means 2 lines of strings of indefinite length
	//label for this window
	lblText[0] = "<C>Output";
	lblHelp = newCDKLabel(hds_state.output_win.cdksptr, CENTER, TOP,
			(CDK_CSTRING2) lblText, 1, FALSE, FALSE);
	drawCDKLabel(lblHelp, ObjOf (lblHelp)->box);
}
/**
 * @brief Calculates horizontal and vertical padding
 *
 * Paddings are needed to ensure that our curses UI is positioned in
 * center of terminal.
 * @param hor_padding variable where calculated horizontal padding will be stored
 * @param ver_padding variable where calculated vertical padding will be stored
 */
static void calculate_padding(int *hor_padding, int *ver_padding) {
	*hor_padding = (COLS - MIN_COLS) / 2;
	*ver_padding = (LINES - MIN_LINES) / 2;
	hds_state.hori_pad = *hor_padding;
	hds_state.vert_pad = *ver_padding;
}
/**
 * @brief Initialize CDK specific features
 */
static void init_cdk() {
	//for all the windows that we want lets create cdkscreen out of them
	initCDKColor();
}
/**
 * @brief Check if specified window dimensions are correct.
 * @return HDS_OK if window specification is OK else return an error code
 */
static int check_window_configuration() {
	int min_req_height;
	//parent window size check
	if (COLS < MIN_COLS || LINES < MIN_LINES) {
		report_error(HDS_ERR_WINDOW_SIZE_TOO_SHORT);
		return HDS_ERR_WINDOW_SIZE_TOO_SHORT;
	}
	/*
	 * ensure that window sizes are correctly speccified. we check if we
	 * adding all the window sizes plus 1 separators each time, we still have
	 * sufficient LINES remaining.
	 * for e.g.OUTPU_WINDOW_SIZE+1+INPUT_WINDOW_SIZE 1+CONSOLE_SIZE must be less than or
	 * equal to MIN_LINES. However note that we can adjust the size of
	 * console. However we still need at least 2 lines minimum for console.
	 *
	 */
	min_req_height = OUTPUT_WINDOW_SIZE + 1 + INPUT_WINDOW_SIZE + 1 + 3;
	if (MIN_LINES < min_req_height) {
		report_error(HDS_ERR_WINDOW_CONFIG_MISMATCH);
		return HDS_ERR_WINDOW_CONFIG_MISMATCH;
	}
	return HDS_OK;
}
/**
 * @brief Draws GMM window and its components including label-captions and
 * 			labels.
 */
static void draw_input_window() {
	const char *lblText[2];
	CDKLABEL *lblTitle = NULL;
	//set the title bar
	lblText[0] = "<C>Input";
	lblTitle = newCDKLabel(hds_state.input_win.cdksptr, CENTER, TOP,
			(CDK_CSTRING2) lblText, 1, FALSE, FALSE);
	drawCDKLabel(lblTitle, ObjOf (lblTitle)->box);

	//draw the input widget
	hds_state.read_input = newCDKEntry(hds_state.input_win.cdksptr,
			hds_state.input_win.cur_x + 1, hds_state.input_win.cur_y + 1, "",
			"<C>HDS>", A_NORMAL, '.', vMIXED, 40, 0, 256, false, false);

	bindCDKObject(vENTRY, hds_state.read_input, '?', XXXCB, 0);

	if (!hds_state.read_input) {
		serror("Failed to create input widget !!");
		return;
	}
	refreshCDKScreen(hds_state.input_win.cdksptr);
}
/**
 * @brief Draws result window.
 */
static void draw_output_console() {
	const char *console_title = "<C></B/U/7>Results";
	/* Create the scrolling window. */
	hds_state.output_screen = newCDKSwindow(hds_state.output_win.cdksptr,
			CDKparamValue(&hds_state.params, 'X', CENTER),
			CDKparamValue(&hds_state.params, 'Y', CENTER),
			CDKparamValue(&hds_state.params, 'H', hds_state.output_win.height),
			CDKparamValue(&hds_state.params, 'W', hds_state.output_win.width),
			console_title, 100, CDKparamValue(&hds_state.params, 'N', TRUE),
			CDKparamValue(&hds_state.params, 'S', FALSE));

	/* Is the window null. */
	if (hds_state.output_screen == 0) {
		report_error(HDS_ERR_CDK_CONSOLE_DRAW);
	}
	/* Draw the scrolling window. */
	drawCDKSwindow(hds_state.output_screen, ObjOf (hds_state.output_screen)->box);
}
/**
 * @brief Draws console window.
 */
static void draw_console() {
	const char *console_title = "<C></B/U/7>Log Window";
	/* Create the scrolling window. */
	hds_state.console = newCDKSwindow(hds_state.console_win.cdksptr,
			CDKparamValue(&hds_state.params, 'X', CENTER),
			CDKparamValue(&hds_state.params, 'Y', CENTER),
			CDKparamValue(&hds_state.params, 'H', hds_state.console_win.height),
			CDKparamValue(&hds_state.params, 'W', hds_state.console_win.width),
			console_title, 100, CDKparamValue(&hds_state.params, 'N', TRUE),
			CDKparamValue(&hds_state.params, 'S', FALSE));

	/* Is the window null. */
	if (hds_state.console == 0) {
		report_error(HDS_ERR_CDK_CONSOLE_DRAW);
	}
	/* Draw the scrolling window. */
	drawCDKSwindow(hds_state.console, ObjOf (hds_state.console)->box);
}
