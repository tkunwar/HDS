/**
 * @file hds_config.c
 * @brief This file contains code which deals with parsing configuration file
 * and saves this information in hds_config structure which is globally
 * accessible.
 */
#include "hds_config.h"
//-----------------------------------
//define routine before using them
static void print_loaded_configs();
//-----------------------------------
/**
 * @brief Initialise hds_config structure with default values
 */
static void init_hds_config() {
	strncpy(hds_config.log_filename, "hds_output.log");
	hds_config.process_config_list = NULL;
}
/**
 * @brief Display the configuration that has been parsed from the configuration
 * file.
 */
static void print_loaded_configs() {
	fprintf(stderr, "\nLoaded configs:");
	fprintf(stderr, "\n\tlog_filename: %s", hds_config.log_filename);
	fprintf(stderr, "\n");
}
void add_new_process_config(struct process_config_t node) {

}
/**
 * @brief Read configuration file
 *
 * Main routine for reading configuration file and save the information in
 * hds_config declared in hds_config.h .
 * @return Return HDS_OK if everything went OK else an error code.
 */
int load_config() {
	config_t cfg;
	const char* s_val = NULL; // will be used to store string values
	int val;
	config_setting_t *setting;
	struct process_config_t tmp_config;
	tmp_config.next = NULL
	:

	// initialize hds_config state
	init_hds_config();

	/*Initialization */
	config_init(&cfg);
	/* Read the file. If there is an error, report it and exit. */
	if (!config_read_file(&cfg, HDS_CONF_FILE)) {
		printf("\n%s:%d - %s", config_error_file(&cfg), config_error_line(&cfg),
				config_error_text(&cfg));
		config_destroy(&cfg);
		return HDS_ERR_NO_CONFIG_FILE;
	}
	// read log file name
	if (config_lookup_string(&cfg, "log_filename", &s_val)) {
		strcpy(hds_config.log_filename, s_val);
	} else {
		fprintf(stderr,
				"\nError: Field log_filename not found in config file! Using default: %s",
				hds_config.log_filename);
	}

	//lets read other values
	setting = config_lookup(&cfg, "process_list");
	if(setting != NULL)
	  {
	    int count = config_setting_length(setting);
	    int i;

	    for(i = 0; i < count; ++i)
	    {
	      config_setting_t *process_config_from_file = config_setting_get_elem(setting, i);

	      /* Only output the record if all of the expected fields are present. */
	      if(!(config_setting_lookup_int(process_config_from_file, "type", &tmp_config.type)
	           && config_setting_lookup_int(process_config_from_file, "memory_req", &tmp_config.memory_req)
	           && config_setting_lookup_int(process_config_from_file, "printer_req", &tmp_config.printer_req)
	           && config_setting_lookup_int(process_config_from_file, "scanner_req", &tmp_config.scanner_req)))
	        continue;

	    }
	    putchar('\n');
	  }
	config_destroy(&cfg);
	print_loaded_configs();
	return HDS_OK;
}
