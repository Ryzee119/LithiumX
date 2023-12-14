/*
 FTP Server for STM32-E407 and ChibiOS
 Copyright (C) 2015 Jean-Michel Gallego
 Copyright (C) 2022 Ryan Wendland (Xbox/Win32 tweaks)

 See readme.txt for information

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

#include "ftp_server.h"
#include "ftp_file.h"
#include "ftp.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>

#include "lwip/opt.h"
#include "lwip/api.h"

static const char *ftp_user_name = FTP_USER_NAME_DEFAULT;
static const char *ftp_user_pass = FTP_USER_PASS_DEFAULT;

#define FTP_USER_NAME_OK(name) (!strcmp(name, ftp_user_name))
#define FTP_USER_PASS_OK(pass) (!strcmp(pass, ftp_user_pass))
#define FTP_IS_LOGGED_IN(p_ftp) (p_ftp->user == FTP_USER_USER_LOGGED_IN)

uint8_t ftp_eth_is_connected(void)
{
	return 1;
}

// =========================================================
//
//              Send a response to the client
//
// =========================================================

static void ftp_send(ftp_data_t *ftp, const char *fmt, ...)
{
	// send buffer
	char send_buffer[FTP_BUF_SIZE];

	// Create vaarg list
	va_list args;
	va_start(args, fmt);

	// Write string to buffer
	vsnprintf(send_buffer, FTP_BUF_SIZE, fmt, args);

	// Close vaarg list
	va_end(args);

	// send to endpoint
	if (netconn_write(ftp->ctrlconn, send_buffer, strlen(send_buffer), NETCONN_COPY) != ERR_OK)
		FTP_CONN_DEBUG(ftp, "Error sending command!\r\n");

	// debugging
	FTP_CONN_DEBUG(ftp, "%s", send_buffer);
}

// Create string YYYYMMDDHHMMSS from date and time
//
// parameters:
//    date, time
//
// return:
//    pointer to string

static char *data_time_to_str(char *str, uint16_t date, uint16_t time)
{
	static const char *month_str[] =
	{
		"Jan", "Feb" , "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	//MMM DD YYYY
	uint32_t year =  ((date & 0xFE00) >> 9) + 1980;
	uint32_t month = (date & 0x01E0) >> 5;
	uint32_t day = date & 0x001F;
	if (month > 12) month = 12;
	sprintf(str, "%s %02d %04d", month_str[month - 1], day, year);
	return str;
}

// Calculate date and time from first parameter sent by MDTM command (YYYYMMDDHHMMSS)
//
// parameters:
//   pdate, ptime: pointer of variables where to store data
//
// return:
//    length of (time parameter + space) if date/time are ok
//    0 if parameter is not YYYYMMDDHHMMSS

static int8_t date_time_get(char *parameters, uint16_t *pdate, uint16_t *ptime)
{
	// Date/time are expressed as a 14 digits long string
	//   terminated by a space and followed by name of file
	if (strlen(parameters) < 15 || parameters[14] != ' ')
		return 0;
	for (uint8_t i = 0; i < 14; i++)
		if (!isdigit(parameters[i]))
			return 0;

	parameters[14] = 0;
	*ptime = atoi(parameters + 12) >> 1; // seconds
	parameters[12] = 0;
	*ptime |= atoi(parameters + 10) << 5; // minutes
	parameters[10] = 0;
	*ptime |= atoi(parameters + 8) << 11; // hours
	parameters[8] = 0;
	*pdate = atoi(parameters + 6); // days
	parameters[6] = 0;
	*pdate |= atoi(parameters + 4) << 5; // months
	parameters[4] = 0;
	*pdate |= (atoi(parameters) - 1980) << 9; // years

	return 15;
}

// =========================================================
//
//             Get a command from the client
//
// =========================================================

static int ftp_read_command(ftp_data_t *ftp)
{
	// loop and check for packet every second
	for (uint32_t i = 0; i < FTP_TIME_OUT_S; i++)
	{
		// receive data
		int8_t net_err = netconn_recv(ftp->ctrlconn, &ftp->inbuf);

		// reception was ok?
		if (net_err == ERR_OK)
		{
			return 0;
		}

		// other error than timeout?
		if (net_err != ERR_TIMEOUT)
			break;

		// link down?
		if (!ftp_eth_is_connected())
			break;
	}

	// all good
	return -1;
}

// =========================================================
//
//             Parse the last command
//
// =========================================================
// return: -1 syntax error
//          0 command without parameters
//          >0 length of parameters

static int ftp_parse_command(ftp_data_t *ftp)
{
	char *pbuf;
	uint16_t buflen;
	int ret = 0;
	int8_t i;

	// get data from recieved packet
	netbuf_data(ftp->inbuf, (void **)&pbuf, &buflen);

	// zero the command and parameter buffers
	memset(ftp->command, 0, FTP_CMD_SIZE);
	memset(ftp->parameters, 0, FTP_PARAM_SIZE);

	// no data?
	if (buflen == 0)
		goto deletebuf;

	// reset index to zero
	i = 0;

	// copy command loop
	do
	{
		// command may only contain characters, not the case?
		if (!isalpha(pbuf[i]))
			break;

		// copy character
		ftp->command[i] = pbuf[i];

		// increment index
		i++;
	} while (i < buflen && i < (FTP_CMD_SIZE - 1));

	// When the command contains parameters, the character after the
	// command is a space. If this character is not a space, we only
	// received a command.
	if (pbuf[i] != ' ')
		goto deletebuf;

	// remove leading spaces for parameters
	while (pbuf[i] == ' ')
		i++;

	// set return variable to zero, it will contain the string length
	ret = 0;

	// search for the end of the parameter string
	while (pbuf[i + ret] != '\n' && pbuf[i + ret] != '\r' && (i + ret) < buflen)
		ret++;

	// will the parameter data fit the given buffer?
	if (ret + 1 >= FTP_PARAM_SIZE)
	{
		// parameter string does not fit the given buffer, set error code
		ret = -1;

		//
		goto deletebuf;
	}

	// copy parameters from the pbuf
	strncpy(ftp->parameters, pbuf + i, ret);

// delete buf tag
deletebuf:

	// feedback
	FTP_CONN_DEBUG(ftp, "Incomming: %s %s\r\n", ftp->command, ftp->parameters);

	// delete buffer
	netbuf_delete(ftp->inbuf);

	// return error code
	return ret;
}

// =========================================================
//
//               Functions for data connection
//
// =========================================================

static int pasv_con_open(ftp_data_t *ftp)
{
	// If this is not already done, create the TCP connection handle
	// to listen to client to open data connection
	if (ftp->listdataconn != NULL)
		return 0;

	// create new socket
	ftp->listdataconn = netconn_new(NETCONN_TCP);

	// create was ok?
	if (ftp->listdataconn == NULL)
	{
		FTP_CONN_DEBUG(ftp, "Error in opening listening con, creation failed\r\n");
		return -1;
	}

	// Bind listdataconn to port (FTP_DATA_PORT + num) with default IP address
	int8_t err = netconn_bind(ftp->listdataconn, IP_ADDR_ANY, ftp->data_port);
	if (err != ERR_OK)
	{
		FTP_CONN_DEBUG(ftp, "Error in opening listening con, bind failed %d\r\n", err);
		return -1;
	}

	//
	// netconn_set_recvtimeout(ftp->listdataconn, 5000);

	// Put the connection into LISTEN state
	err = netconn_listen(ftp->listdataconn);
	if (err != ERR_OK)
	{
		FTP_CONN_DEBUG(ftp, "Error in opening listening con, listen failed %d\r\n", err);
		return -1;
	}

	// all good
	return 0;
}

static void pasv_con_close(ftp_data_t *ftp)
{
	// reset datacon mode
	ftp->data_conn_mode = DCM_NOT_SET;

	// delete listdataconn socket
	if (ftp->listdataconn == NULL)
		return;

	// close socket
	netconn_close(ftp->listdataconn);

	// delete socket
	netconn_delete(ftp->listdataconn);

	// set to null to be sure
	ftp->listdataconn = NULL;
}

static int data_con_open(ftp_data_t *ftp)
{
	// no connection mode set?
	if (ftp->data_conn_mode == DCM_NOT_SET)
	{
		FTP_CONN_DEBUG(ftp, "No connecting mode defined\r\n");
		return -1;
	}

	// feedback
	FTP_CONN_DEBUG(ftp, "Data conn in %s mode\r\n", (ftp->data_conn_mode == DCM_PASSIVE ? "passive" : "active"));

	// are we in passive mode?
	if (ftp->data_conn_mode == DCM_PASSIVE)
	{
		// in passive mode the connection to the client should already be made
		// and the listen data socket should be initialized (not NULL).
		if (ftp->listdataconn == NULL)
		{
			return -1;
		}

		// Wait for connection from client for 500ms
		// netconn_set_recvtimeout(ftp->listdataconn, 500);

		// accept connection
		if (netconn_accept(ftp->listdataconn, &ftp->dataconn) != ERR_OK)
		{
			FTP_CONN_DEBUG(ftp, "Error in data conn: netconn_accept\r\n");
			return -1;
		}
	}
	// we are in active mode
	else
	{
		//  Create a new TCP connection handle
		ftp->dataconn = netconn_new(NETCONN_TCP);

		// was creation succesfull?
		if (ftp->dataconn == NULL)
		{
			FTP_CONN_DEBUG(ftp, "Error in data conn: netconn_new\r\n");
			return -1;
		}

		//  Connect to data port with client IP address
		if (netconn_bind(ftp->dataconn, IP_ADDR_ANY, 0) != ERR_OK)
		{
			FTP_CONN_DEBUG(ftp, "Error in data conn: netconn_bind\r\n");
			netconn_delete(ftp->dataconn);
			ftp->dataconn = NULL;
			return -1;
		}

		// did connection fail?
		if (netconn_connect(ftp->dataconn, &ftp->ipclient, ftp->data_port) != ERR_OK)
		{
			FTP_CONN_DEBUG(ftp, "Error in data conn: netconn_connect\r\n");
			netconn_delete(ftp->dataconn);
			ftp->dataconn = NULL;
			return -1;
		}
	}

	// all good
	return 0;
}

static void data_con_close(ftp_data_t *ftp)
{
	// reset datacon mode
	ftp->data_conn_mode = DCM_NOT_SET;

	// socket already closed?
	if (ftp->dataconn == NULL)
		return;

	// close socket
	netconn_close(ftp->dataconn);

	// delete socket
	netconn_delete(ftp->dataconn);

	// set to null, to be sure
	ftp->dataconn = NULL;
}

// =========================================================
//
//                  Functions on files
//
// =========================================================

static void path_up_a_level(char *path)
{
	// is there a dash in the string?
	if (strchr(path, '/'))
	{
		// get position
		uint32_t pos = strlen(path) - 1;

		// go up a folder
		while (path[pos] != '/')
		{
			// clear character
			path[pos] = 0;

			// update position
			pos = strlen(path) - 1;
		}

		// remove the dash on which the wile loop exits, but only
		// when we are not root
		if (strlen(path) > 1)
			path[pos] = 0;
	}
}

// Make complete path/name from cwdName and parameters
//
// 3 possible cases:
//   parameters can be absolute path, relative path or only the name
//
// parameters:
//   fullName : where to store the path/name
//
// return:
//   true, if done

static uint8_t path_build(char *current_path, char *ftp_param)
{
	// Should we go to the root directory or is the parameter buffer empty?
	if (!strcmp(ftp_param, "/") || strlen(ftp_param) == 0)
	{
		// go to root directory
		strncpy(current_path, "/", FTP_CWD_SIZE);
	}
	// should we go up a directory?
	else if (strcmp(ftp_param, "..") == 0)
	{
		// remove characters until '/' is found
		path_up_a_level(current_path);
	}
	// The incoming parameter doesn't contain a slash? this means that
	// the parameter is only the folder name and it should be appended
	else if (ftp_param[0] != '/')
	{
		// should we concatinate '/'?
		if (current_path[strlen(current_path) - 1] != '/')
			strncat(current_path, "/", FTP_CWD_SIZE);

		// concatinate parameter to string
		strncat(current_path, ftp_param, FTP_CWD_SIZE);
	}
	// The incoming parameter starts with a slash. This means that
	// the parameter is the whole path.
	else
	{
		strncpy(current_path, ftp_param, FTP_CWD_SIZE);
	}

	// If the string is longer than 1 character and ends with '/', remove it
	uint16_t strl = strlen(current_path) - 1;
	if (current_path[strl] == '/' && strl > 1)
		current_path[strl] = 0;

	// does the string fit? success
	if (strlen(current_path) < FTP_CWD_SIZE)
		return 1;

	// failed
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//			FTP commands
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// print working directory
static void ftp_cmd_pwd(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// reply
	ftp_send(ftp, "257 \"%s\" is your current directory\r\n", ftp->path);
}

// change working directory
static void ftp_cmd_cwd(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// no parmeters given?
	if (strlen(ftp->parameters) == 0)
	{
		ftp_send(ftp, "501 No directory name\r\n");
		return;
	}

	// can we build a path from the parameters?
	if (!path_build(ftp->path, ftp->parameters))
	{
		ftp_send(ftp, "500 Command line too long\r\n");
		return;
	}

	// is this not the root path and doesn't the path exist?
	if (strcmp(ftp->path, "/") != 0 && ftps_f_stat(ftp->path, &ftp->finfo) != FR_OK)
	{
		ftp_send(ftp, "550 Failed to change directory to %s\r\n", ftp->path);
		return;
	}

	// send directory to client
	ftp_send(ftp, "250 Directory successfully changed.\r\n");
}

// Change the remote machine working directory to the parent of the current remote machine working directory.
static void ftp_cmd_cdup(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// set directory to root
	path_up_a_level(ftp->path);

	// send ack to client
	ftp_send(ftp, "250 Directory successfully changed to root.\r\n");
}

static void ftp_cmd_mode(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	if (!strcmp(ftp->parameters, "S"))
		ftp_send(ftp, "200 S Ok\r\n");
	// else if( ! strcmp( parameters, "B" ))
	//   ftp_send(ftp,  "200 B Ok\r\n");
	else
		ftp_send(ftp, "504 Only S(tream) is suported\r\n");
}

static void ftp_cmd_stru(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	if (!strcmp(ftp->parameters, "F"))
		ftp_send(ftp, "200 F Ok\r\n");
	else
		ftp_send(ftp, "504 Only F(ile) is suported\r\n");
}

static void ftp_cmd_type(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	if (!strcmp(ftp->parameters, "A"))
		ftp_send(ftp, "200 TYPE is now ASCII\r\n");
	else if (!strcmp(ftp->parameters, "I"))
		ftp_send(ftp, "200 TYPE is now 8-bit binary\r\n");
	else
		ftp_send(ftp, "504 Unknow TYPE\r\n");
}

static void ftp_cmd_pasv(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

#if USE_PASSIVE_MODE == 1
	// set data port
	ftp->data_port = FTP_DATA_PORT + ftp->data_port_incremented + (ftp->ftp_con_num * PORT_INCREMENT_OFFSET);

	// open connection ok?
	if (pasv_con_open(ftp) == 0)
	{
		// close data connection, just to be sure
		data_con_close(ftp);

		// reply that we are entering passive mode
		unsigned int ip_addr = ip_addr_get_ip4_u32(&ftp->ipserver);
		ftp_send(ftp, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d).\r\n", ip_addr & 0xFF,(ip_addr >> 8) & 0xFF, (ip_addr >> 16) & 0xFF,
				 (ip_addr >> 24) & 0xFF, ftp->data_port >> 8, ftp->data_port & 255);

		// feedback
		FTP_CONN_DEBUG(ftp, "Data port set to %u\r\n", ftp->data_port);

		// set state
		ftp->data_conn_mode = DCM_PASSIVE;
	}
	// open connection gave an error
	else
	{
		// send error
		ftp_send(ftp, "425 Can't set connection management to passive\r\n");

		// reset data conn mode
		ftp->data_conn_mode = DCM_NOT_SET;
	}
#else
	// send error
	ftp_send(ftp, "421 Passive mode not available\r\n");

	// reset data conn mode
	ftp->dataConnMode = DCM_NOT_SET;
#endif
}

static void ftp_cmd_port(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	uint8_t ip[4];
	uint8_t i;

	// close data connection just to be sure
	data_con_close(ftp);

	// parameter valid?
	if (strlen(ftp->parameters) == 0)
	{
		// send error to client
		ftp_send(ftp, "501 no parameters given\r\n");
		ftp->data_conn_mode = DCM_NOT_SET;
		return;
	}

	// Start building IP
	char *p = ftp->parameters - 1;

	// build IP
	for (i = 0; i < 4 && p != NULL; i++)
	{
		// valid pointer?
		if (p == NULL)
			break;

		// convert number
		ip[i] = atoi(++p);

		// find next comma
		p = strchr(p, ',');
	}

	// get port
	if (p != NULL)
	{
		// read upper octet of port
		if (i == 4)
			ftp->data_port = 256 * atoi(++p);

		// go to next comma
		p = strchr(p, ',');

		// read lower octet of port
		if (p != NULL)
			ftp->data_port += atoi(++p);
	}

	// error parsing IP and port?
	if (p == NULL)
	{
		ftp_send(ftp, "501 Can't interpret parameters\r\n");
		ftp->data_conn_mode = DCM_NOT_SET;
		return;
	}

	// build IP address
	IP_ADDR4(&ftp->ipclient, ip[0], ip[1], ip[2], ip[3]);

	// send ack to client
	ftp_send(ftp, "200 PORT command successful\r\n");

	// feedback
	FTP_CONN_DEBUG(ftp, "Data IP set to %s\r\n", ipaddr_ntoa(&ftp->ipclient));
	FTP_CONN_DEBUG(ftp, "Data port set to %u\r\n", ftp->data_port);

	// set data connection mode
	ftp->data_conn_mode = DCM_ACTIVE;
}

static void ftp_cmd_list(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	DIR dir;

	// can we open the directory?
	if (ftps_f_opendir(&dir, ftp->path) != FR_OK)
	{
		ftp_send(ftp, "550 Can't open directory %s\r\n", ftp->parameters);
		return;
	}

	// open data connection
	if (data_con_open(ftp) != 0)
	{
		ftp_send(ftp, "425 Can't create connection\r\n");
		return;
	}

	// accept the command
	ftp_send(ftp, "150 Accepted data connection\r\n");

	// working buffer
	char dir_name_buf[_MAX_LFN];

	// loop until errors occur
	// FIXME, maybe I could read async somehow?
	while (ftps_f_readdir(&dir, &ftp->finfo) == FR_OK)
	{
		// last entry read?
		if (ftp->finfo.fname[0] == 0)
			break;

		// file name is not valid?
		if (ftp->finfo.fname[0] == '.')
			continue;

		char *fname = ftp->lfn[0] == 0 ? ftp->finfo.fname : ftp->lfn;
		char date_str_buff[64];
		char *date_str = data_time_to_str(date_str_buff, ftp->finfo.fdate, ftp->finfo.ftime);

		// list command given? (to give support for NLST)
		if (strcmp(ftp->command, "LIST"))
		{
			snprintf(dir_name_buf, _MAX_LFN, "%s\r\n", fname);
		}
		// is it a directory?
		else if (ftp->finfo.fattrib & AM_DIR)
		{
			snprintf(dir_name_buf, _MAX_LFN, "drwxr-xr-x 1 XBOX XBOX 0 %s %s\n", date_str, fname);
		// just a file
		}
		else
		{
			snprintf(dir_name_buf, _MAX_LFN, "-rw-r--r-- 1 XBOX XBOX %u %s %s\n", ftp->finfo.fsize, date_str, fname);
		}
		// write data to endpoint
		netconn_write(ftp->dataconn, dir_name_buf, strlen(dir_name_buf), NETCONN_COPY);
	}

	// close data connection
	data_con_close(ftp);

	// all was good
	ftp_send(ftp, "226 Directory send OK.\r\n");
}

static void ftp_cmd_dele(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// parameters valid?
	if (strlen(ftp->parameters) == 0)
	{
		ftp_send(ftp, "501 No file name\r\n");
		return;
	}

	// can we build a valid path?
	if (!path_build(ftp->path, ftp->parameters))
	{
		ftp_send(ftp, "500 Command line too long\r\n");
		return;
	}

	// does the file exist?
	if (ftps_f_stat(ftp->path, &ftp->finfo) != FR_OK)
	{
		// go up a level again
		path_up_a_level(ftp->path);

		// send error to client
		ftp_send(ftp, "550 file %s not found\r\n", ftp->parameters);

		// go back
		return;
	}

	// can we delete the file?
	if (ftps_f_unlink(ftp->path) != FR_OK)
	{
		// go up a level again
		path_up_a_level(ftp->path);

		// send error to client
		ftp_send(ftp, "450 Can't delete %s\r\n", ftp->parameters);

		// go back
		return;
	}

	// all good
	ftp_send(ftp, "250 Deleted %s\r\n", ftp->parameters);

	// go up a level again
	path_up_a_level(ftp->path);
}

static void ftp_cmd_noop(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// no operation
	ftp_send(ftp, "200 Zzz...\r\n");
}

#include <profileapi.h>
static void ftp_cmd_retr(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// parmeter ok?
	if (strlen(ftp->parameters) == 0)
	{
		ftp_send(ftp, "501 No file name\r\n");
		return;
	}

	// can we create a valid path from the parameter?
	if (!path_build(ftp->path, ftp->parameters))
	{
		ftp_send(ftp, "500 Command line too long\r\n");
		return;
	}

	// does the chosen file exist?
	if (ftps_f_stat(ftp->path, &ftp->finfo) != FR_OK)
	{
		// go up a level again
		path_up_a_level(ftp->path);

		// send error to client
		ftp_send(ftp, "550 File %s not found\r\n", ftp->parameters);

		// go back
		return;
	}

	// can we open the file?
	if (ftps_f_open(&ftp->file, ftp->path, FA_READ) != FR_OK)
	{
		// go up a level again
		path_up_a_level(ftp->path);

		// send error to client
		ftp_send(ftp, "450 Can't open %s\r\n", ftp->parameters);

		// go back
		return;
	}

	// can we connect to the client?
	if (data_con_open(ftp) != 0)
	{
		// go up a level again
		path_up_a_level(ftp->path);

		// close file
		ftps_f_close(&ftp->file);

		// send error to client
		ftp_send(ftp, "425 Can't create connection\r\n");

		// go back
		return;
	}

	// feedback
	FTP_CONN_DEBUG(ftp, "Sending %s\r\n", ftp->parameters);

	// grab the optional start position and reset it for next time
	uint32_t restart_position = ftp->file_restart_pos;
	if (restart_position > ftp->finfo.fsize)
	{
		ftp_send(ftp, "530 Invalid restart position\r\n");
		goto done;
	}
	ftp->file_restart_pos = 0;

	// send accept to client
	ftp_send(ftp, "150 Connected to port %u, %lu bytes to download\r\n", ftp->data_port, ftp->finfo.fsize - restart_position);

	// variables used in loop
	uint32_t total_bytes_read = 0;
	uint32_t bytes_transferred = 0;
	uint32_t bytes_read;

	// loop while reading is OK
	while (1)
	{
		// read from file ok?
		bytes_read = 0;
		if (ftps_f_read(&ftp->file, ftp->file.cache_buf[0], FILE_CACHE_SIZE, &bytes_read, restart_position + total_bytes_read) != FR_OK)
		{
			ftp_send(ftp, "550 File read failure\r\n");
			break;
		}
		total_bytes_read += bytes_read;

		// done with file
		if(bytes_read == 0)
		{
			ftp_send(ftp, "226 File successfully transferred\r\n");
			break;
		}

		// write data to socket
		bytes_transferred = 0;
		while (bytes_transferred < bytes_read)
		{
			uint32_t xfer_remain = (bytes_read - bytes_transferred);
			uint32_t xfer_len = (xfer_remain < FTP_BUF_SIZE) ? xfer_remain : FTP_BUF_SIZE;
			err_t con_err = netconn_write(ftp->dataconn, &ftp->file.cache_buf[0][bytes_transferred], xfer_len, NETCONN_COPY);
			if (con_err != ERR_OK)
			{
				ftp_send(ftp, "426 LWIP network error code %d, transfer aborted\r\n", con_err);
				goto done;
			}
			bytes_transferred += xfer_len;
		}
	}
	done:

	// feedback
	FTP_CONN_DEBUG(ftp, "Sent %u bytes\r\n", bytes_transferred);

	// close file
	ftps_f_close(&ftp->file);

	// go up a level again
	path_up_a_level(ftp->path);

	// close data socket
	data_con_close(ftp);
}

static void ftp_cmd_stor(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// argument valid?
	if (strlen(ftp->parameters) == 0)
	{
		ftp_send(ftp, "501 No file name\r\n");
		return;
	}

	// is the path valid?
	if (!path_build(ftp->path, ftp->parameters))
	{
		ftp_send(ftp, "500 Command line too long\r\n");
		return;
	}

	// does the path exist?
	if (ftps_f_open(&ftp->file, ftp->path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
	{
		// go up a level again
		path_up_a_level(ftp->path);

		// send error to client
		ftp_send(ftp, "450 Can't open/create %s\r\n", ftp->parameters);

		// go back
		return;
	}

	// can we set up a data connection?
	if (data_con_open(ftp) != 0)
	{
		// go up a level again
		path_up_a_level(ftp->path);

		// send error to client
		ftp_send(ftp, "425 Can't create connection\r\n");

		// close file
		ftps_f_close(&ftp->file);

		// go back
		return;
	}

	// feedback
	FTP_CONN_DEBUG(ftp, "Receiving %s\r\n", ftp->parameters);

	// reply to ftp client that we are ready
	ftp_send(ftp, "150 Connected to port %u\r\n", ftp->data_port);

	//
	struct pbuf *p = NULL;
	int8_t file_err = 0;
	int8_t con_err = 0;
	uint32_t bytes_written = 0;
	while (1)
	{
		// receive data from ftp client ok?
		con_err = netconn_recv_tcp_pbuf(ftp->dataconn, &p);

		// socket closed? (end of file)
		if (con_err == ERR_CLSD)
		{
			break;
		}

		// other error?
		if (con_err != ERR_OK && con_err != ERR_CLSD)
		{
			ftp_send(ftp, "426 Error during file transfer: %d\r\n", con_err);
			break;
		}

		// housekeeping
		file_err = ftps_f_write(&ftp->file, p, p->tot_len, (uint32_t *)&bytes_written);
		pbuf_free(p);

		// error in nested loop?
		if (file_err != FR_OK)
		{
			ftp_send(ftp, "451 Communication error during transfer\r\n");
			break;
		}
	}

	// close file
	file_err = ftps_f_close(&ftp->file);
	if (file_err != FR_OK)
	{
		ftp_send(ftp, "451 Communication error during transfer\r\n");
	}

	// feedback
	FTP_CONN_DEBUG(ftp, "Wrote %u bytes\r\n", ftp->file.write_total);

	// go up a level again
	path_up_a_level(ftp->path);

	// close data connection
	data_con_close(ftp);

	// all was good
	if (file_err == FR_OK)
	{
		ftp_send(ftp, "226 File successfully transferred\r\n");
	}
}

static void ftp_cmd_mkd(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// valid parameters?
	if (strlen(ftp->parameters) == 0)
	{
		ftp_send(ftp, "501 No directory name\r\n");
		return;
	}

	// can we build a path?
	if (!path_build(ftp->path, ftp->parameters))
	{
		ftp_send(ftp, "500 Command line too long\r\n");
		return;
	}

	// does the path not exist already?
	if (ftps_f_stat(ftp->path, &ftp->finfo) == FR_OK)
	{
		// go up a level again
		path_up_a_level(ftp->path);

		// send error to client
		ftp_send(ftp, "521 \"%s\" directory already exists\r\n", ftp->parameters);

		// go back
		return;
	}

	// make the directory
	if (ftps_f_mkdir(ftp->path) != FR_OK)
	{
		// go up a level again
		path_up_a_level(ftp->path);

		// send error to client
		ftp_send(ftp, "550 Can't create \"%s\"\r\n", ftp->parameters);

		// go back
		return;
	}

	// feedback
	FTP_CONN_DEBUG(ftp, "Creating directory %s\r\n", ftp->parameters);

	path_up_a_level(ftp->path);

	// check the result
	ftp_send(ftp, "257 \"%s\" created\r\n", ftp->parameters);
}

static void ftp_cmd_rmd(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// valid parameter?
	if (strlen(ftp->parameters) == 0)
	{
		ftp_send(ftp, "501 No directory name\r\n");
		return;
	}

	// Can we build path?
	if (!path_build(ftp->path, ftp->parameters))
	{
		ftp_send(ftp, "500 Command line too long\r\n");
		return;
	}

	// feedback
	FTP_CONN_DEBUG(ftp, "Deleting %s\r\n", ftp->path);

	// file does exist?
	if (ftps_f_stat(ftp->path, &ftp->finfo) != FR_OK)
	{
		// go up a level again
		path_up_a_level(ftp->path);

		// send error to client
		ftp_send(ftp, "550 Directory \"%s\" not found\r\n", ftp->parameters);
		return;
	}

	// remove file ok?
	if (ftps_f_unlink(ftp->path) != FR_OK)
	{
		// go up a level again
		path_up_a_level(ftp->path);

		// send error to client
		ftp_send(ftp, "501 Can't delete \"%s\"\r\n", ftp->parameters);
		return;
	}

	// all good
	ftp_send(ftp, "250 \"%s\" removed\r\n", ftp->parameters);

	// go up a level again
	path_up_a_level(ftp->path);
}

static void ftp_cmd_rnfr(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// parameters ok?
	if (strlen(ftp->parameters) == 0)
	{
		ftp_send(ftp, "501 No file name\r\n");
		return;
	}

	// copy path to path_rename since this will be used
	memcpy(ftp->path_rename, ftp->path, FTP_CWD_SIZE);

	// can we build a path with the specified file name?
	if (!path_build(ftp->path_rename, ftp->parameters))
	{
		ftp_send(ftp, "500 Command line too long\r\n");
		return;
	}

	// does the file exist?
	if (ftps_f_stat(ftp->path_rename, &ftp->finfo) != FR_OK)
	{
		ftp_send(ftp, "550 file \"%s\" not found\r\n", ftp->parameters);
		return;
	}

	// feedback
	FTP_CONN_DEBUG(ftp, "Renaming %s\r\n", ftp->path_rename);

	// reply to client
	ftp_send(ftp, "350 RNFR accepted - file exists, ready for destination\r\n");
}

static void ftp_cmd_rnto(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// do we have parameters?
	if (!strlen(ftp->parameters))
	{
		ftp_send(ftp, "501 No file name\r\n");
		return;
	}

	// is the rnfr already specified?
	if (!strlen(ftp->path_rename))
	{
		ftp_send(ftp, "503 Need RNFR before RNTO\r\n");
		return;
	}

	// can we build a path with the specified file name?
	if (!path_build(ftp->path, ftp->parameters))
	{
		ftp_send(ftp, "500 Command line too long\r\n");
		return;
	}

	// does the file exist?
	if (ftps_f_stat(ftp->path, &ftp->finfo) == FR_OK)
	{
		ftp_send(ftp, "553 \"%s\" already exists\r\n", ftp->parameters);

		// remove file name from path
		path_up_a_level(ftp->path);

		// go back
		return;
	}

	// feedback
	FTP_CONN_DEBUG(ftp, "Renaming %s to %s\r\n", ftp->path_rename, ftp->path);

	// rename went ok?
	if (ftps_f_rename(ftp->path_rename, ftp->path) != FR_OK)
	{
		ftp_send(ftp, "451 Rename/move failure\r\n");
	}
	else
	{
		ftp_send(ftp, "250 File successfully renamed or moved\r\n");
	}

	// remove file name from path
	path_up_a_level(ftp->path);
}

static void ftp_cmd_feat(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// print features
	ftp_send(ftp, "211 Extensions supported:\r\n MDTM\r\n MLSD\r\n SIZE\r\n SITE FREE\r\n211 End.\r\n");
}

static void ftp_cmd_syst(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// print system and version
	ftp_send(ftp, "215 FTP Server, V1.0\r\n");
}

static void ftp_cmd_mdtm(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	char *fname;
	uint16_t date, time;
	uint8_t gettime;

	gettime = date_time_get(ftp->parameters, &date, &time);
	fname = ftp->parameters + gettime;

	if (strlen(fname) == 0)
	{
		ftp_send(ftp, "501 No file name\r\n");
	}

	if (!path_build(ftp->path, fname))
	{
		ftp_send(ftp, "500 Command line too long\r\n");
		return;
	}

	if (ftps_f_stat(ftp->path, &ftp->finfo) != FR_OK)
	{
		// go up a level again
		path_up_a_level(ftp->path);

		// send error to client
		ftp_send(ftp, "550 file \"%s\" not found\r\n", ftp->parameters);

		// go back
		return;
	}

	// go up a level again
	path_up_a_level(ftp->path);
	if (!gettime)
	{
		char date_str[64];
		ftp_send(ftp, "213 %s\r\n", data_time_to_str(date_str, ftp->finfo.fdate, ftp->finfo.ftime));
		return;
	}
	ftp->finfo.fdate = date;
	ftp->finfo.ftime = time;
	if (ftps_f_utime(ftp->path, &ftp->finfo) == FR_OK)
		ftp_send(ftp, "200 Ok\r\n");
	else
		ftp_send(ftp, "550 Unable to modify time\r\n");
}

static void ftp_cmd_size(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	if (strlen(ftp->parameters) == 0)
	{
		ftp_send(ftp, "501 No file name\r\n");
		return;
	}

	if (!path_build(ftp->path, ftp->parameters))
	{
		ftp_send(ftp, "500 Command line too long\r\n");
		return;
	}
	if (ftps_f_stat(ftp->path, &ftp->finfo) != FR_OK || (ftp->finfo.fattrib & AM_DIR))
	{
		// send error to client
		ftp_send(ftp, "550 No such file\r\n");
	}
	else
	{
		ftp_send(ftp, "213 %lu\r\n", ftp->finfo.fsize);
		ftps_f_close(&ftp->file);
	}

	// go up a level again
	path_up_a_level(ftp->path);
}

static void ftp_cmd_site(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	ftp_send(ftp, "550 Unknown SITE command %s\r\n", ftp->parameters);
	/*
	if (!strcmp(ftp->parameters, "FREE"))
	{
		void *fs;
		uint32_t free_clust;
		ftps_f_getfree("0:", &free_clust, fs);
		// ftp_send(ftp, "211 %lu MB free of %lu MB capacity\r\n", free_clust * fs->csize >> 11, (fs->n_fatent - 2) * fs->csize >> 11);
	}
	else
	{
		ftp_send(ftp, "550 Unknown SITE command %s\r\n", ftp->parameters);
	}
	*/
}

static void ftp_cmd_stat(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// print status
	ftp_send(ftp, "221 FTP Server status: you will be disconnected after %d minutes of inactivity\r\n", FTP_TIME_OUT_S / 60);
}

static void ftp_cmd_auth(ftp_data_t *ftp)
{
	// no tls or ssl available
	ftp_send(ftp, "504 Not available\r\n");
}

static void ftp_cmd_user(ftp_data_t *ftp)
{
	// is this the normal user that is trying to log in?
	if (FTP_USER_NAME_OK(ftp->parameters))
	{
		// all good
		ftp_send(ftp, "331 OK. Password required\r\n");

		// waiting for user password
		ftp->user = FTP_USER_USER_NO_PASS;
	}
	// unknown user
	else
	{
		// not a user and not an admin, error
		ftp_send(ftp, "530 Username not known\r\n");
	}
}

static void ftp_cmd_pass(ftp_data_t *ftp)
{
	// in idle state?
	if (ftp->user == FTP_USER_NONE)
	{
		// user not specified
		ftp_send(ftp, "530 User not specified\r\n");
	}
	// is this the normal user that is trying to log in?
	else if (FTP_USER_PASS_OK(ftp->parameters))
	{
		// username and password accepted
		ftp_send(ftp, "230 OK, logged in as user\r\n");

		// user enabled
		ftp->user = FTP_USER_USER_LOGGED_IN;
	}
	// unknown password
	else
	{
		// error, return
		ftp_send(ftp, "530 Password not correct\r\n");
	}
}

static void ftp_cmd_rest(ftp_data_t *ftp)
{
	// are we not yet logged in?
	if (!FTP_IS_LOGGED_IN(ftp))
		return;

	// sets the restart file position
	uint32_t pos = strtoul(ftp->parameters, NULL, 0);
	ftp->file_restart_pos = pos;
	ftp_send(ftp, "350 Restarting at %d\r\n", pos);
}

static ftp_cmd_t ftpd_commands[] = {
	//
	{"PWD", ftp_cmd_pwd},	//
	{"CWD", ftp_cmd_cwd},	//
	{"CDUP", ftp_cmd_cdup}, //
	{"MODE", ftp_cmd_mode}, //
	{"STRU", ftp_cmd_stru}, //
	{"TYPE", ftp_cmd_type}, //
	{"PASV", ftp_cmd_pasv}, //
	{"PORT", ftp_cmd_port}, //
	{"NLST", ftp_cmd_list}, //
	{"LIST", ftp_cmd_list}, //
	{"MLSD", ftp_cmd_list}, //
	{"DELE", ftp_cmd_dele}, //
	{"NOOP", ftp_cmd_noop}, //
	{"RETR", ftp_cmd_retr}, //
	{"STOR", ftp_cmd_stor}, //
	{"MKD", ftp_cmd_mkd},	//
	{"RMD", ftp_cmd_rmd},	//
	{"RNFR", ftp_cmd_rnfr}, //
	{"RNTO", ftp_cmd_rnto}, //
	{"FEAT", ftp_cmd_feat}, //
	{"MDTM", ftp_cmd_mdtm}, //
	{"SIZE", ftp_cmd_size}, //
	{"SITE", ftp_cmd_site}, //
	{"STAT", ftp_cmd_stat}, //
	{"SYST", ftp_cmd_syst}, //
	{"AUTH", ftp_cmd_auth}, //
	{"USER", ftp_cmd_user}, //
	{"PASS", ftp_cmd_pass}, //
	{"REST", ftp_cmd_rest}, //
	{NULL, NULL}			//
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//			process a command
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint8_t ftp_process_command(ftp_data_t *ftp)
{
	// quit command given?
	if (!strcmp(ftp->command, "QUIT"))
		return 0;

	// command pointer
	ftp_cmd_t *cmd = ftpd_commands;

	// loop through all known commands
	while (cmd->cmd != NULL && cmd->func != NULL)
	{
		// is this the expected command?
		if (!strcmp(cmd->cmd, ftp->command))
			break;

		// increment
		cmd++;
	}

	// TODO: only allow RETR to follow a REST

	// did we find a command?
	if (cmd->cmd != NULL && cmd->func != NULL)
		cmd->func(ftp);
	// no command found, unknown
	else
		ftp_send(ftp, "500 Unknown command\r\n");

	// ftp is still running
	return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//			Main FTP server
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ftp_service(struct netconn *ctrlcn, ftp_data_t *ftp)
{
	uint16_t dummy;
	ip_addr_t ippeer;

	// reset the working directory to root
	strncpy(ftp->path, "/", FTP_CWD_SIZE);
	memset(ftp->path_rename, 0, FTP_CWD_SIZE);

	// variables initialization
	ftp->ctrlconn = ctrlcn;
	ftp->listdataconn = NULL;
	ftp->dataconn = NULL;
	ftp->data_port = 0;
	ftp->data_conn_mode = DCM_NOT_SET;
	ftp->user = FTP_USER_NONE;

	// bugfix which works around ports which are already in use (from a previous connection)
	ftp->data_port_incremented = (ftp->data_port_incremented + 1) % PORT_INCREMENT_OFFSET;

	//  Get the local and peer IP
	netconn_addr(ftp->ctrlconn, &ftp->ipserver, &dummy);
	netconn_peer(ftp->ctrlconn, &ippeer, &dummy);

	// send welcome message
	ftp_send(ftp, "220 -> CMS FTP Server, FTP Version %s\r\n", FTP_VERSION);

	// feedback
	FTP_CONN_DEBUG(ftp, "Client connected!\r\n");

	// Set disconnection timeout to one second
	// netconn_set_recvtimeout(ftp->ctrlconn, 1000);

	// loop until quit command
	while (1)
	{
		// Was there an error while receiving?
		if (ftp_read_command(ftp) != 0)
			break;

		// was there an error while parsing?
		if (ftp_parse_command(ftp) < 0)
			break;

		// quit command received?
		if (!ftp_process_command(ftp))
		{
			// send goodbye command
			ftp_send(ftp, "221 Goodbye\r\n");

			// break from loop
			break;
		}
	}

	// Close listen connection
	pasv_con_close(ftp);

	// Close the connections (to be sure)
	data_con_close(ftp);

	// feedback
	FTP_CONN_DEBUG(ftp, "Client disconnected\r\n");
}

void ftp_set_username(const char *name)
{
	if (name == NULL)
		return;
	ftp_user_name = name;
}

void ftp_set_password(const char *pass)
{
	if (pass == NULL)
		return;
	ftp_user_pass = pass;
}
