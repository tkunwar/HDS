/**
 * @file hds_config.c
 * @brief This file contains code which deals with parsing configuration file
 * and saves this information in hds_config structure which is globally
 * accessible.
 */
#include "hds_config.h"
//-----------------------------------
//defnie routine before using them
static void print_loaded_configs();
//-----------------------------------
/**
 * @brief Initialise hds_config structure with default values
 */
static void init_hds_config() {
	hds_config.cell_compaction_enabled = true;
	hds_config.max_buff_size = 100;
	hds_config.swappiness = 70;
	hds_config.stats_refresh_rate = 1;
}
/**
 * @brief Display the configuration that has been parsed from the configuration
 * file.
 */
static void print_loaded_configs() {
	fprintf(stderr, "\nLoaded configs:");
	fprintf(stderr, "\n\tmax_buffer_size: %d", hds_config.max_buff_size);
	fprintf(stderr, "\n\tswappiness: %d", hds_config.swappiness);
	if (hds_config.cell_compaction_enabled == true) {
		fprintf(stderr, "\n\tcell_compaction_enabled: true");
	} else {
		fprintf(stderr, "\n\tcell_compaction_enabled: false");
	}
	fprintf(stderr, "\n");
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
	const char* s_val=NULL; // will be used to store string values
	int val;
	bool config_fallback_enabled = false;
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
	//read config_fallback_enabled field
	if (config_lookup_string(&cfg, "config_fallback_enabled", &s_val)) {
		if (strncmp(s_val, "true", 4) == 0) {
			config_fallback_enabled = true;
		}
	} else {
		fprintf(stderr, "\nWarning: Field conf_fallback_enabled not found in "
				"config file");
	}
	//read max_buffer_size
	if (config_lookup_int(&cfg, "max_buffer_size", &val)) {
		// if loaded value is acceptable to us, then save it
		if (val < 100) {
			fprintf(stderr, "\nError: max_buffer_size is too small");
			if (config_fallback_enabled == false) {
				// this is unacceptable, abort
				fprintf(stderr,
						"\nConfiguration prohibits using default values."
								" Exiting!");
				return HDS_ERR_CONFIG_ABORT;
			}
		} else {
			hds_config.max_buff_size = val;
		}
	} else {
		fprintf(stderr, "\nWarning: Field max_buffer_size not found in config"
				" file!");
	}
	//read swappiness field
	if (config_lookup_int(&cfg, "swappiness", &val)) {
		if (val >= 80 && val <= 95) {
			fprintf(stderr,
					"\nWarning: swappiness value is too high.\nPerformance may "
							"be affected!");
			hds_config.swappiness = val;
		} else if (val > 95) {
			fprintf(stderr, "\nError: swappiness value is too high.");
			if (config_fallback_enabled == false) {
				fprintf(stderr,
						"\nConfiguration prohibits using default values."
								" Exiting!");
				return HDS_ERR_CONFIG_ABORT;
			}
		} else {
			hds_config.swappiness = val;
		}
	} else {
		fprintf(stderr,
				"\nWarning: Field swappiness not found in config file!");
	}
	//read cell_compaction_enabled
	if (config_lookup_string(&cfg, "cell_compaction_enabled", &s_val)) {
		if (strncmp(s_val, "true", 4) == 0) {
			hds_config.cell_compaction_enabled = true;
		} else {
			hds_config.cell_compaction_enabled = false;
		}
	} else {
		fprintf(stderr, "\nWarning: Field cell_compaction_enabled not found"
				" in config file!");
	}
	//read stats_refresh field
	if (config_lookup_int(&cfg, "stats_refresh_rate", &val)) {
		if (val < 0) {
			fprintf(stderr, "\nWarning: Invalid value for stats_refresh_rate!");
			if (config_fallback_enabled == false) {
				fprintf(stderr,
						"\nConfiguration prohibits using default values."
								" Exiting!");
				return HDS_ERR_CONFIG_ABORT;
			}else{
				hds_config.stats_refresh_rate =1;
			}
		} else {
			hds_config.stats_refresh_rate = val;
		}
	} else {
		fprintf(stderr,
				"\nWarning: Field stats_refresh_rate not found in config file!");
	}
	// read log file name
		if (config_lookup_string(&cfg, "log_filename", &s_val)) {
			if (!s_val){
				if (config_fallback_enabled == false) {
					fprintf(stderr,
							"\nConfiguration prohibits using default values."
									" Exiting!");
					return HDS_ERR_CONFIG_ABORT;
				}else{
					strcpy(hds_config.log_filename,"hds_output.log");
				}
			}else{
					strcpy(hds_config.log_filename,s_val);
				}
		} else {
			fprintf(stderr,
					"\nError: Field log_filename not found in config file!");
		}
	config_destroy(&cfg);
//	print_loaded_configs();
	return HDS_OK;
}
