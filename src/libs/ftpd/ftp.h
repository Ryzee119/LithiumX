/*
 FTP Server for STM32-E407 and ChibiOS
 Copyright (C) 2015 Jean-Michel Gallego

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

#ifndef _FTPS_H_
#define _FTPS_H_

#include <stdio.h>
#include "lwip/opt.h"
#include "lwip/api.h"
#include "ftp_server.h"

// stack size for ftp task
#define FTP_TASK_STACK_SIZE 4096

// initial FTP port
#define FTP_SERVER_PORT 21

// Data port in passive mode
#define FTP_DATA_PORT 55600

// number of clients we want to serve simultaneously, same as netbuf limit
#define FTP_NBR_CLIENTS 10

#ifdef FTP_DEBUG
#define FTP_CONN_DEBUG(ftp, f, ...) printf("[%d] " f, ftp->ftp_con_num, ##__VA_ARGS__)
#define FTP_PRINTF printf
#else
#define FTP_CONN_DEBUG(ftp, f, ...)
#define FTP_PRINTF(...)
#endif

// define a structure of parameters for a ftp thread
typedef struct
{
	uint8_t number;
	struct netconn *ftp_connection;
	sys_thread_t *task_handle;
	ftp_data_t ftp_data;
	char task_name[12];
} server_stru_t;

/**
 * Start the FTP server.
 *
 * This code creates a socket on port 21 to listen for incoming
 * FTP client connections. If this creation fails the code returns
 * Immediately. If the socket is created the task continues.
 *
 * The task loops indefinitely and waits for connections. When a
 * connection is found a port is assigned to the incoming client.
 * A separate task is started for each connection which handles
 * The FTP commands. When the client disconnects the task is
 * stopped.
 *
 * An incoming connection is denied when:
 * - The memory on the CMS is not available
 * - The maximum number of clients is connected
 * - The application is running
 */
void ftp_server(void);

#endif // _FTPS_H_
