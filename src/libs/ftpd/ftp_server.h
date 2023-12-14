/*
 * ftp_server.h
 *
 *  Created on: Aug 31, 2020
 *      Author: Sande
 */

#ifndef _FTP_SERVER_H_
#define _FTP_SERVER_H_

#include <stdio.h>
#include "lwip/opt.h"
#include "lwip/api.h"
#include "ftp_file.h"

// version number
#define FTP_VERSION				"2020-08-20"

// ftp user and admin
#define FTP_USER_NAME_DEFAULT	"xbox"
#define FTP_USER_PASS_DEFAULT	"xbox"

// Disconnect client this many seconds of inactivity
#define FTP_TIME_OUT_S			300

// parameter buffer size
#define FTP_PARAM_SIZE			_MAX_LFN + 8

// current working directory (CWD) size
#define FTP_CWD_SIZE			_MAX_LFN + 8

// command (CMD) size
#define FTP_CMD_SIZE			5

// size of file buffer for reading a file
#define FTP_BUF_SIZE			1420

// Use passive mode or not
#define USE_PASSIVE_MODE		1

// used for a bugfix which works around ports which are already in use (from a previous connection)
#define PORT_INCREMENT_OFFSET	25

// Data Connection mode enumeration typedef
typedef enum {
	DCM_NOT_SET,
	DCM_PASSIVE,
	DCM_ACTIVE
} dcm_type;

// ftp log in enumeration typedef
typedef enum {
	FTP_USER_NONE,
	FTP_USER_USER_NO_PASS,
	FTP_USER_USER_LOGGED_IN
} ftp_user_t;

/**
 * Structure that contains all variables used in FTP connection.
 * This is not nicely done since code is ported from C++ to C. The
 * C++ private object variables are listed inside this structure.
 */
typedef struct {
	// sockets
	struct netconn *listdataconn;
	struct netconn *dataconn;
	struct netconn *ctrlconn;
	struct netbuf *inbuf;

	// ip addresses
	ip_addr_t ipclient;
	ip_addr_t ipserver;

	// port
	uint16_t data_port;
	uint8_t data_port_incremented;

	// file variables, not created on stack but static on boot
	// to avoid overflow and ensure alignment in memory
	FIL file;
	FILINFO finfo;
	char lfn[_MAX_LFN + 1];

	// buffer for command sent by client
	char command[FTP_CMD_SIZE];

	// buffer for parameters sent by client
	char parameters[FTP_PARAM_SIZE];

	// buffer for origin path for Rename command
	char path_rename[FTP_CWD_SIZE];

	// buffer for path that is currently used
	char path[FTP_CWD_SIZE];

	// connection mode (not set, active or passive)
	uint8_t ftp_con_num;

	// state which tells which user is logged in
	ftp_user_t user;

	// data connection mode state
	dcm_type data_conn_mode;

	// file restart position
	uint32_t file_restart_pos;
} ftp_data_t;

// structure for ftp commands
typedef struct {
	const char *cmd;
	void (*func)(ftp_data_t *ftp);
} ftp_cmd_t;

/**
 * Service the FTP server connection that is accepted.
 *
 * @param ctrlcn Connection that was created for FTP server
 * @param ftp The FTP structure containing all variables
 */
extern void ftp_service(struct netconn *ctrlcn, ftp_data_t *ftp);

/**
 * Setter functions for username and password
 */
extern void ftp_set_username(const char *name);
extern void ftp_set_password(const char *pass);

uint8_t ftp_eth_is_connected(void);

#endif /* ETH_FTP_FTP_SERVER_H_ */
