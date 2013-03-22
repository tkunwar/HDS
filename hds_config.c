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
	strcpy(hds_config.log_filename, "hds_output.log");
	hds_config.process_config_list = NULL;
	hds_config.process_config_last_element = NULL;
	hds_config.process_id_counter = 0;

	hds_config.max_resources.memory = 0;
	hds_config.max_resources.printer = 0;
	hds_config.max_resources.scanner = 0;
}
//static void print_loaded_configs() {
//	struct process_config_t *pclist_iter = hds_config.process_config_list;
//
//	fprintf(stderr, "\nLoaded configs:");
//	fprintf(stderr, "\n\tlog_filename: %s", hds_config.log_filename);
//	fprintf(stderr,
//			"\nMax. resource available:\n\t memory(MB): %d printer(units): %d scanner(units): %d",
//			hds_config.max_resources.memory, hds_config.max_resources.printer,
//			hds_config.max_resources.scanner);
//	fprintf(stderr, "\nLoaded process config list:");
//	for (; pclist_iter != NULL ; pclist_iter = pclist_iter->next) {
//		fprintf(stderr,
//				"\n\tloaded: pid: %u type: %d memory_req: %d printer_req: %d scanner_req: %d",
//				pclist_iter->pid, pclist_iter->type, pclist_iter->memory_req,
//				pclist_iter->printer_req, pclist_iter->scanner_req);
//	}
//	fprintf(stderr, "\n");
//}
/**
 * @brief Adds a new entry in process_config_list.
 * @param node
 */
int add_new_process_config(struct process_config_t node) {
	struct process_config_t *tmp_node = NULL;
	//allocate memory to it
	tmp_node = (struct process_config_t *) malloc(
			sizeof(struct process_config_t));
	if (!tmp_node) {
		fprintf(stderr,
				"malloc failed while adding new process_config item to process_config_list");
		return HDS_ERR_NO_MEM;
	}
	tmp_node->next = NULL;
	//assign values to tmp_node
	tmp_node->type = node.type;
	tmp_node->memory_req = node.memory_req;
	tmp_node->scanner_req = node.scanner_req;
	tmp_node->printer_req = node.printer_req;
//	hds_config.process_id_counter = hds_config.process_id_counter +1;
	tmp_node->pid = (++hds_config.process_id_counter);

	if (hds_config.process_config_list == NULL ) {
		// this is first time
		hds_config.process_config_list = tmp_node;
		hds_config.process_config_last_element = tmp_node;
	} else {
		//add tmp_node to end of list
		hds_config.process_config_last_element->next = tmp_node;
		hds_config.process_config_last_element = tmp_node;
	}
	return HDS_OK;
}
void cleanup_process_config_list() {
	struct process_config_t *cur_node = hds_config.process_config_list;
	struct process_config_t *next_node = NULL;
	do {
		next_node = cur_node->next;
		free(cur_node);
		cur_node = next_node;
	} while (cur_node);
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
	config_setting_t *setting;
	config_setting_t *max_res_setting;
	struct process_config_t tmp_config;
	tmp_config.next = NULL;

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

	// find the max resources
	max_res_setting = config_lookup(&cfg, "max_resources");
	if (max_res_setting != NULL ) {
		if (!(config_setting_lookup_int(max_res_setting, "printer",
				&hds_config.max_resources.printer)
				&& (config_setting_lookup_int(max_res_setting, "memory",
						&hds_config.max_resources.memory))
				&& (config_setting_lookup_int(max_res_setting, "scanner",
						&hds_config.max_resources.scanner)))) {
			fprintf(stderr,
					"Error: Failed to determine max. resource limit from config file!");
			return HDS_ERR_NO_SUCH_ELEMENT;
		}
	} else {
		fprintf(stderr, "Error: No such fields 'max_resources'");
		return HDS_ERR_NO_SUCH_ELEMENT;
	}
	//lets read other values
	setting = config_lookup(&cfg, "process_list");
	if (setting != NULL ) {
		int count = config_setting_length(setting);
		int i;

		for (i = 0; i < count; ++i) {
			config_setting_t *process_config_from_file =
					config_setting_get_elem(setting, i);

			/* Only output the record if all of the expected fields are present. */
			if (!(config_setting_lookup_int(process_config_from_file, "type",
					&tmp_config.type)
					&& config_setting_lookup_int(process_config_from_file,
							"memory_req", &tmp_config.memory_req)
					&& config_setting_lookup_int(process_config_from_file,
							"printer_req", &tmp_config.printer_req)
					&& config_setting_lookup_int(process_config_from_file,
							"scanner_req", &tmp_config.scanner_req)))
				continue;

			//add this to process_config_list
			if (add_new_process_config(tmp_config) != HDS_OK) {
				// problem in mem IO
				return HDS_ERR_NO_MEM;
			}
		}
	}
	config_destroy(&cfg);
//	print_loaded_configs();
//	cleanup_process_config_list();

	return HDS_OK;
}
