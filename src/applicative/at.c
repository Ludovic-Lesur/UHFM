/*
 * at.c
 *
 *  Created on: 18 apr. 2020
 *      Author: Ludo
 */

#include <error.h>
#include "at.h"

#include "adc.h"
#include "aes.h"
#include "addon_sigfox_rf_protocol_api.h"
#include "flash_reg.h"
#include "lpuart.h"
#include "lptim.h"
#include "mapping.h"
#include "math.h"
#include "nvic.h"
#include "nvm.h"
#include "parser.h"
#include "sigfox_api.h"
#include "string.h"
#include "uhfm.h"

/*** AT local macros ***/

// Enabled commands.
#define AT_COMMANDS_NVM
#define AT_COMMANDS_SIGFOX
#define AT_COMMANDS_TEST_MODES
// Common macros.
#define AT_COMMAND_LENGTH_MIN			2
#define AT_COMMAND_BUFFER_LENGTH		128
#define AT_RESPONSE_BUFFER_LENGTH		128
#define AT_STRING_VALUE_BUFFER_LENGTH	16
// Parameters separator.
#define AT_CHAR_SEPARATOR				','
// Responses.
#define AT_RESPONSE_END					"\n"
#define AT_RESPONSE_TAB					"     "

/*** AT callbacks declaration ***/

static void AT_print_ok(void);
static void AT_print_command_list(void);
static void AT_read_callback(void);
static void AT_write_callback(void);
#ifdef AT_COMMANDS_NVM
static void AT_nvmr_callback(void);
static void AT_nvm_callback(void);
static void AT_get_id_callback(void);
static void AT_set_id_callback(void);
static void AT_get_key_callback(void);
static void AT_set_key_callback(void);
#endif
#ifdef AT_COMMANDS_SIGFOX
static void AT_so_callback(void);
static void AT_sb_callback(void);
static void AT_sf_callback(void);
#endif
#ifdef AT_COMMANDS_TEST_MODES
static void AT_tm_callback(void);
#endif

/*** AT local structures ***/

typedef struct {
	PARSER_mode_t mode;
	char* syntax;
	char* parameters;
	char* description;
	void (*callback)(void);
} AT_command_t;

typedef struct {
	// AT command buffer.
	volatile unsigned char command_buf[AT_COMMAND_BUFFER_LENGTH];
	volatile unsigned int command_buf_idx;
	volatile unsigned char line_end_flag;
	PARSER_context_t parser;
	char response_buf[AT_RESPONSE_BUFFER_LENGTH];
	unsigned int response_buf_idx;
	// Sigfox RC.
	sfx_rc_t sigfox_rc;
	sfx_u32 sigfox_rc_std_config[SIGFOX_RC_STD_CONFIG_SIZE];
	unsigned char sigfox_rc_idx;
} AT_context_t;

/*** AT local global variables ***/

static const AT_command_t AT_COMMAND_LIST[] = {
	{PARSER_MODE_COMMAND, "AT", "\0", "Ping command", AT_print_ok},
	{PARSER_MODE_COMMAND, "AT?", "\0", "List all available AT commands", AT_print_command_list},
	{PARSER_MODE_HEADER, "AT$R=", "address[dec]", "Read board register", AT_read_callback},
	{PARSER_MODE_HEADER, "AT$W=", "address[dec]", "Write board register", AT_write_callback},
#ifdef AT_COMMANDS_NVM
	{PARSER_MODE_COMMAND, "AT$NVMR", "\0", "Reset NVM data", AT_nvmr_callback},
	{PARSER_MODE_HEADER,  "AT$NVM=", "address[dec]", "Get NVM data", AT_nvm_callback},
	{PARSER_MODE_COMMAND, "AT$ID?", "\0", "Get Sigfox device ID", AT_get_id_callback},
	{PARSER_MODE_HEADER,  "AT$ID=", "id[hex]", "Set Sigfox device ID", AT_set_id_callback},
	{PARSER_MODE_COMMAND, "AT$KEY?", "\0", "Get Sigfox device key", AT_get_key_callback},
	{PARSER_MODE_HEADER,  "AT$KEY=", "key[hex]", "Set Sigfox device key", AT_set_key_callback},
#endif
#ifdef AT_COMMANDS_SIGFOX
	{PARSER_MODE_COMMAND, "AT$SO", "\0", "Sigfox send control message", AT_so_callback},
	{PARSER_MODE_HEADER,  "AT$SB=", "data[bit],(bidir_flag[bit])", "Sigfox send bit", AT_sb_callback},
	{PARSER_MODE_HEADER,  "AT$SF=", "data[hex],(bidir_flag[bit])", "Sigfox send frame", AT_sf_callback},
#endif
#ifdef AT_COMMANDS_TEST_MODES
	{PARSER_MODE_HEADER,  "AT$TM=", "rc_index[dec],test_mode[dec]", "Execute Sigfox test mode", AT_tm_callback},
#endif
};

static AT_context_t at_ctx = {
	.sigfox_rc = (sfx_rc_t) RC1,
	.sigfox_rc_idx = SFX_RC1
};

/*** AT local functions ***/

/* APPEND A STRING TO THE REPONSE BUFFER.
 * @param tx_string:	String to add.
 * @return:				None.
 */
static void AT_response_add_string(char* tx_string) {
	// Fill TX buffer with new bytes.
	while (*tx_string) {
		at_ctx.response_buf[at_ctx.response_buf_idx++] = *(tx_string++);
		// Manage rollover.
		if (at_ctx.response_buf_idx >= AT_RESPONSE_BUFFER_LENGTH) {
			at_ctx.response_buf_idx = 0;
		}
	}
}

/* APPEND A VALUE TO THE REPONSE BUFFER.
 * @param tx_value:		Value to add.
 * @param format:       Printing format.
 * @param print_prefix: Print base prefix is non zero.
 * @return:				None.
 */
static void AT_response_add_value(int tx_value, STRING_format_t format, unsigned char print_prefix) {
	// Local variables.
	char str_value[AT_STRING_VALUE_BUFFER_LENGTH];
	unsigned char idx = 0;
	// Reset string.
	for (idx=0 ; idx<AT_STRING_VALUE_BUFFER_LENGTH ; idx++) str_value[idx] = STRING_CHAR_NULL;
	// Convert value to string.
	STRING_value_to_string(tx_value, format, print_prefix, str_value);
	// Add string.
	AT_response_add_string(str_value);
}

/* SEND AT REPONSE OVER AT INTERFACE.
 * @param:	None.
 * @return:	None.
 */
static void AT_response_send(void) {
	// Local variables.
	unsigned int idx = 0;
	// Send response over UART.
	LPUART1_send_string(at_ctx.response_buf);
	// Flush response buffer.
	for (idx=0 ; idx<AT_RESPONSE_BUFFER_LENGTH ; idx++) at_ctx.response_buf[idx] = STRING_CHAR_NULL;
	at_ctx.response_buf_idx = 0;
}

/* PRINT OK THROUGH AT INTERFACE.
 * @param:	None.
 * @return:	None.
 */
static void AT_print_ok(void) {
	AT_response_add_string("OK");
	AT_response_add_string(AT_RESPONSE_END);
	AT_response_send();
}

/* PRINT AN ERROR THROUGH AT INTERFACE.
 * @param error_source:	8-bits error source.
 * @param error_code:	16-bits error code.
 * @return:				None.
 */
static void AT_print_status(UHFM_status_t status) {
	AT_response_add_string("ERROR ");
	if (status < 0x0100) {
		AT_response_add_value(0, STRING_FORMAT_HEXADECIMAL, 1);
		AT_response_add_value(status, STRING_FORMAT_HEXADECIMAL, 0);
	}
	else {
		AT_response_add_value(status, STRING_FORMAT_HEXADECIMAL, 1);
	}
	AT_response_add_string(AT_RESPONSE_END);
	AT_response_send();
}

#define AT_status_check(status, success, error_base) { if (status != success) { AT_print_status(error_base + status); goto errors;} }

/* PRINT ALL SUPPORTED AT COMMANDS.
 * @param:	None.
 * @return:	None.
 */
static void AT_print_command_list(void) {
	// Local variables.
	unsigned int idx = 0;
	// Commands loop.
	for (idx=0 ; idx<(sizeof(AT_COMMAND_LIST) / sizeof(AT_command_t)) ; idx++) {
		// Print syntax.
		AT_response_add_string(AT_COMMAND_LIST[idx].syntax);
		// Print parameters.
		AT_response_add_string(AT_COMMAND_LIST[idx].parameters);
		AT_response_add_string(AT_RESPONSE_END);
		// Print description.
		AT_response_add_string(AT_RESPONSE_TAB);
		AT_response_add_string(AT_COMMAND_LIST[idx].description);
		AT_response_add_string(AT_RESPONSE_END);
		AT_response_send();
	}
}

/* AT$R EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void AT_read_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_SUCCESS;
	int register_address = 0;
	// Read address parameter.
	parser_status = PARSER_get_parameter(&at_ctx.parser, PARSER_PARAMETER_TYPE_DECIMAL, AT_CHAR_SEPARATOR, 1, &register_address);
	AT_status_check(parser_status, PARSER_SUCCESS, UHFM_ERROR_BASE_PARSER);
	// Get data.
	switch (register_address) {
	case UHFM_REGISTER_ADDRESS_BOARD_ID:
		break;
	default:
		break;
	}
errors:
	return;
}

/* AT$W EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void AT_write_callback(void) {

}

#ifdef AT_COMMANDS_NVM
/* AT$NVMR EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void AT_nvmr_callback(void) {
	// Reset all NVM field to default value.
	NVM_enable();
	NVM_reset_default();
	NVM_disable();
	AT_print_ok();
	return;
}

/* AT$NVM EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void AT_nvm_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_SUCCESS;
	int address = 0;
	unsigned char nvm_data = 0;
	// Read address parameters.
	parser_status = PARSER_get_parameter(&at_ctx.parser, STRING_FORMAT_DECIMAL, AT_CHAR_SEPARATOR, 1, &address);
	if (parser_status != PARSER_SUCCESS) goto errors;
	// Read byte at requested address.
	NVM_enable();
	NVM_read_byte((unsigned short) address, &nvm_data);
	// Print data.
	AT_response_add_value(nvm_data, STRING_FORMAT_HEXADECIMAL, 1);
	AT_response_add_string(AT_RESPONSE_END);
	AT_response_send();
errors:
	NVM_disable();
	return;
}

/* AT$ID? EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void AT_get_id_callback(void) {
	// Local variables.
	unsigned char idx = 0;
	unsigned char id_byte = 0;
	// Retrieve device ID in NVM.
	NVM_enable();
	for (idx=0 ; idx<ID_LENGTH ; idx++) {
		NVM_read_byte((NVM_ADDRESS_SIGFOX_DEVICE_ID + ID_LENGTH - idx - 1), &id_byte);
		AT_response_add_value(id_byte, STRING_FORMAT_HEXADECIMAL, (idx==0 ? 1 : 0));
	}
	NVM_disable();
	AT_response_add_string(AT_RESPONSE_END);
	AT_response_send();
	return;
}

/* AT$ID EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void AT_set_id_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	unsigned char device_id[ID_LENGTH];
	unsigned char extracted_length = 0;
	unsigned char idx = 0;
	// Read ID parameter.
	parser_status = PARSER_get_byte_array(&at_ctx.parser, AT_CHAR_SEPARATOR, 1, ID_LENGTH, device_id, &extracted_length);
	if ((parser_status != PARSER_SUCCESS) || (extracted_length != ID_LENGTH)) goto errors;
	// Write device ID in NVM.
	NVM_enable();
	for (idx=0 ; idx<ID_LENGTH ; idx++) {
		NVM_write_byte((NVM_ADDRESS_SIGFOX_DEVICE_ID + ID_LENGTH - idx - 1), device_id[idx]);
	}
	AT_print_ok();
errors:
	NVM_disable();
	return;
}

/* AT$KEY? EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void AT_get_key_callback(void) {
	// Local variables.
	unsigned char idx = 0;
	unsigned char key_byte = 0;
	// Retrieve device key in NVM.
	NVM_enable();
	for (idx=0 ; idx<AES_BLOCK_SIZE ; idx++) {
		NVM_read_byte((NVM_ADDRESS_SIGFOX_DEVICE_KEY + idx), &key_byte);
		AT_response_add_value(key_byte, STRING_FORMAT_HEXADECIMAL, (idx==0 ? 1 : 0));
	}
	NVM_disable();
	AT_response_add_string(AT_RESPONSE_END);
	AT_response_send();
	return;
}

/* AT$KEY EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void AT_set_key_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	unsigned char device_key[AES_BLOCK_SIZE];
	unsigned char extracted_length = 0;
	unsigned char idx = 0;
	// Read key parameter.
	parser_status = PARSER_get_byte_array(&at_ctx.parser, AT_CHAR_SEPARATOR, 1, AES_BLOCK_SIZE, device_key, &extracted_length);
	if ((parser_status != PARSER_SUCCESS) || (extracted_length != AES_BLOCK_SIZE)) goto errors;
	// Write device ID in NVM.
	NVM_enable();
	for (idx=0 ; idx<AES_BLOCK_SIZE ; idx++) {
		NVM_write_byte((NVM_ADDRESS_SIGFOX_DEVICE_KEY + idx), device_key[idx]);
	}
	AT_print_ok();
errors:
	NVM_disable();
	return;
}
#endif

#ifdef AT_COMMANDS_SIGFOX
/* PRINT SIGFOX DOWNLINK DATA ON AT INTERFACE.
 * @param dl_payload:	Downlink data to print.
 * @return:				None.
 */
static void AT_print_dl_payload(sfx_u8* dl_payload) {
	AT_response_add_string("+RX=");
	unsigned char idx = 0;
	for (idx=0 ; idx<8 ; idx++) {
		AT_response_add_value(dl_payload[idx], STRING_FORMAT_HEXADECIMAL, 0);
	}
	AT_response_add_string(AT_RESPONSE_END);
	AT_response_send();
}

/* AT$SO EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void AT_so_callback(void) {
	// Local variables.
	// Send Sigfox OOB frame.
	SIGFOX_API_open(&at_ctx.sigfox_rc);
	SIGFOX_API_set_std_config(at_ctx.sigfox_rc_std_config, SFX_FALSE);
	AT_response_add_string("Sigfox library running...");
	AT_response_add_string(AT_RESPONSE_END);
	AT_response_send();
	SIGFOX_API_send_outofband(SFX_OOB_SERVICE);
	AT_print_ok();
	SIGFOX_API_close();
	return;
}

/* AT$SB EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void AT_sb_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	int data = 0;
	int bidir_flag = 0;
	sfx_u8 dl_payload[SIGFOX_DOWNLINK_DATA_SIZE_BYTES];
	// First try with 2 parameters.
	parser_status = PARSER_get_parameter(&at_ctx.parser, STRING_FORMAT_BOOLEAN, AT_CHAR_SEPARATOR, 0, &data);
	if (parser_status == PARSER_SUCCESS) {
		// Try parsing downlink request parameter.
		parser_status =  PARSER_get_parameter(&at_ctx.parser, STRING_FORMAT_BOOLEAN, AT_CHAR_SEPARATOR, 1, &bidir_flag);
		// Send Sigfox bit with specified downlink request.
		SIGFOX_API_open(&at_ctx.sigfox_rc);
		SIGFOX_API_set_std_config(at_ctx.sigfox_rc_std_config, SFX_FALSE);
		AT_response_add_string("Sigfox library running...");
		AT_response_add_string(AT_RESPONSE_END);
		AT_response_send();
		SIGFOX_API_send_bit((sfx_bool) data, dl_payload, 2, (sfx_bool) bidir_flag);
		if (bidir_flag != SFX_FALSE) {
			AT_print_dl_payload(dl_payload);
		}
	}
	else {
		// Try with 1 parameter.
		parser_status = PARSER_get_parameter(&at_ctx.parser, STRING_FORMAT_BOOLEAN, AT_CHAR_SEPARATOR, 1, &data);
		// Send Sigfox bit with no downlink request (by default).
		SIGFOX_API_open(&at_ctx.sigfox_rc);
		SIGFOX_API_set_std_config(at_ctx.sigfox_rc_std_config, SFX_FALSE);
		AT_response_add_string("Sigfox library running...");
		AT_response_add_string(AT_RESPONSE_END);
		AT_response_send();
		SIGFOX_API_send_bit((sfx_bool) data, dl_payload, 2, 0);
	}
	AT_print_ok();
	SIGFOX_API_close();
	return;
}

/* AT$SF EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void AT_sf_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	sfx_u8 data[SIGFOX_UPLINK_DATA_MAX_SIZE_BYTES];
	unsigned char extracted_length = 0;
	int bidir_flag = 0;
	sfx_u8 dl_payload[SIGFOX_DOWNLINK_DATA_SIZE_BYTES];
	// First try with 2 parameters.
	parser_status = PARSER_get_byte_array(&at_ctx.parser, AT_CHAR_SEPARATOR, 0, 12, data, &extracted_length);
	if (parser_status == PARSER_SUCCESS) {
		// Try parsing downlink request parameter.
		parser_status =  PARSER_get_parameter(&at_ctx.parser, STRING_FORMAT_BOOLEAN, AT_CHAR_SEPARATOR, 1, &bidir_flag);
		// Send Sigfox frame with specified downlink request.
		SIGFOX_API_open(&at_ctx.sigfox_rc);
		SIGFOX_API_set_std_config(at_ctx.sigfox_rc_std_config, SFX_FALSE);
		AT_response_add_string("Sigfox library running...");
		AT_response_add_string(AT_RESPONSE_END);
		AT_response_send();
		SIGFOX_API_send_frame(data, extracted_length, dl_payload, 2, bidir_flag);
		if (bidir_flag != 0) {
			AT_print_dl_payload(dl_payload);
		}
	}
	else {
		// Try with 1 parameter.
		parser_status = PARSER_get_byte_array(&at_ctx.parser, AT_CHAR_SEPARATOR, 1, 12, data, &extracted_length);
		// Send Sigfox frame with no downlink request (by default).
		SIGFOX_API_open(&at_ctx.sigfox_rc);
		SIGFOX_API_set_std_config(at_ctx.sigfox_rc_std_config, SFX_FALSE);
		AT_response_add_string("Sigfox library running...");
		AT_response_add_string(AT_RESPONSE_END);
		AT_response_send();
		SIGFOX_API_send_frame(data, extracted_length, dl_payload, 2, 0);
	}
	AT_print_ok();
	SIGFOX_API_close();
	return;
}
#endif

#ifdef AT_COMMANDS_TEST_MODES
/* AT$TM EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void AT_tm_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	sfx_error_t sfx_status = SFX_ERR_NONE;
	int rc_index = 0;
	int test_mode = 0;
	// Read RC parameter.
	parser_status = PARSER_get_parameter(&at_ctx.parser, STRING_FORMAT_DECIMAL, AT_CHAR_SEPARATOR, 0, &rc_index);
	if (parser_status != PARSER_SUCCESS) goto errors;
	// Read test mode parameter.
	parser_status =  PARSER_get_parameter(&at_ctx.parser, STRING_FORMAT_DECIMAL, AT_CHAR_SEPARATOR, 1, &test_mode);
	if (parser_status != PARSER_SUCCESS) goto errors;
	// Call test mode function wth public key.
	AT_response_add_string("Sigfox addon running...");
	AT_response_add_string(AT_RESPONSE_END);
	AT_response_send();
	sfx_status = ADDON_SIGFOX_RF_PROTOCOL_API_test_mode((sfx_rc_enum_t) rc_index, (sfx_test_mode_t) test_mode);
	if (sfx_status == SFX_ERR_NONE) {
		AT_print_ok();
	}
	else {
		AT_response_add_string("SFX_ERROR_");
		AT_response_add_value(sfx_status, STRING_FORMAT_HEXADECIMAL, 1);
		AT_response_add_string(AT_RESPONSE_END);
		AT_response_send();
	}
errors:
	return;
}
#endif

/* RESET AT PARSER.
 * @param:	None.
 * @return:	None.
 */
static void AT_reset_parser(void) {
	// Reset parsing variables.
	at_ctx.command_buf_idx = 0;
	at_ctx.line_end_flag = 0;
	at_ctx.parser.rx_buf = (unsigned char*) at_ctx.command_buf;
	at_ctx.parser.rx_buf_length = 0;
	at_ctx.parser.separator_idx = 0;
	at_ctx.parser.start_idx = 0;
}

/* PARSE THE CURRENT AT COMMAND BUFFER.
 * @param:	None.
 * @return:	None.
 */
static void AT_decode(void) {
	// Local variables.
	unsigned int idx = 0;
	unsigned char decode_success = 0;
	// Empty or too short command.
	if (at_ctx.command_buf_idx < AT_COMMAND_LENGTH_MIN) {
		AT_print_status(UHFM_ERROR_BASE_PARSER + PARSER_ERROR_UNKNOWN_COMMAND);
		goto errors;
	}
	// Update parser length.
	at_ctx.parser.rx_buf_length = (at_ctx.command_buf_idx - 1); // To ignore line end.
	// Loop on available commands.
	for (idx=0 ; idx<(sizeof(AT_COMMAND_LIST) / sizeof(AT_command_t)) ; idx++) {
		// Check type.
		if (PARSER_compare(&at_ctx.parser, AT_COMMAND_LIST[idx].mode, AT_COMMAND_LIST[idx].syntax) == PARSER_SUCCESS) {
			// Execute callback and exit.
			AT_COMMAND_LIST[idx].callback();
			decode_success = 1;
			break;
		}
	}
	if (decode_success == 0) {
		AT_print_status(UHFM_ERROR_BASE_PARSER + PARSER_ERROR_UNKNOWN_COMMAND); // Unknown command.
		goto errors;
	}
errors:
	AT_reset_parser();
	return;
}

/*** AT functions ***/

/* INIT AT MANAGER.
 * @param:	None.
 * @return:	None.
 */
void AT_init(void) {
	// Init context.
	unsigned int idx = 0;
	for (idx=0 ; idx<AT_COMMAND_BUFFER_LENGTH ; idx++) at_ctx.command_buf[idx] = '\0';
	for (idx=0 ; idx<AT_RESPONSE_BUFFER_LENGTH ; idx++) at_ctx.response_buf[idx] = '\0';
	at_ctx.response_buf_idx = 0;
	// Reset parser.
	AT_reset_parser();
	// Enable LPUART.
	LPUART1_enable_rx();
}

/* MAIN TASK OF AT COMMAND MANAGER.
 * @param:	None.
 * @return:	None.
 */
void AT_task(void) {
	// Trigger decoding function if line end found.
	if (at_ctx.line_end_flag != 0) {
		LPUART1_disable_rx();
		AT_decode();
		LPUART1_enable_rx();
	}
}

/* FILL AT COMMAND BUFFER WITH A NEW BYTE (CALLED BY USART INTERRUPT).
 * @param rx_byte:	Incoming byte.
 * @return:			None.
 */
void AT_fill_rx_buffer(unsigned char rx_byte) {
	// Append byte if LF flag is not allready set.
	if (at_ctx.line_end_flag == 0) {
		// Store new byte.
		at_ctx.command_buf[at_ctx.command_buf_idx] = rx_byte;
		// Manage index.
		at_ctx.command_buf_idx++;
		if (at_ctx.command_buf_idx >= AT_COMMAND_BUFFER_LENGTH) {
			at_ctx.command_buf_idx = 0;
		}
	}
	// Set LF flag to trigger decoding.
	if (rx_byte == STRING_CHAR_LF) {
		at_ctx.line_end_flag = 1;
	}
}

/* PRINT SIGFOX LIBRARY RESULT.
 * @param test_result:	Test result.
 * @param rssi:			Downlink signal rssi in dBm.
 */
void AT_print_test_result(unsigned char test_result, int rssi_dbm) {
	// Check result.
	if (test_result == 0) {
		AT_response_add_string("Test failed.");
	}
	else {
		AT_response_add_string("Test passed. RSSI=");
		AT_response_add_value(rssi_dbm, STRING_FORMAT_DECIMAL, 0);
		AT_response_add_string("dBm");
	}
	AT_response_add_string(AT_RESPONSE_END);
	AT_response_send();
}
