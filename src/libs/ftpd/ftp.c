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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ftp.h"
#include "ftp_server.h"
#include "ftp_file.h"
#include "lwip/opt.h"
#include "lwip/api.h"


// static variables
static const char *no_conn_allowed = "421 No more connections allowed\r\n";
static server_stru_t ftp_links[FTP_NBR_CLIENTS];

// single ftp connection loop
static void ftp_task(void *param)
{
	// sanity check
	if (param == NULL)
		return;

	// parse parameter
	server_stru_t *ftp = (server_stru_t *)param;

	// save the instance number
	ftp->ftp_data.ftp_con_num = ftp->number;

	// feedback
	FTP_PRINTF("FTP %d connected\r\n", ftp->number);

	// service FTP server
	ftp_service(ftp->ftp_connection, &ftp->ftp_data);

	// delete the connection.
	netconn_delete(ftp->ftp_connection);

	// reset the socket to be sure
	ftp->ftp_connection = NULL;

	// feedback
	FTP_PRINTF("FTP %d disconnected\r\n", ftp->number);

	// clear handle
	ftp->task_handle = NULL;
}

static void ftp_start_task(server_stru_t *data, uint8_t index)
{
	// set number
	data->number = index;

	// start task with parameter
	sys_thread_new(data->task_name, ftp_task, data, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
	FTP_PRINTF("%s started\r\n\n\n", data->task_name);
}

// ftp server task
void ftp_server(void)
{
	struct netconn *ftp_srv_conn;
	struct netconn *ftp_client_conn;
	uint8_t index = 0;

	// Create the TCP connection handle
	ftp_srv_conn = netconn_new(NETCONN_TCP);

	// feedback
	if (ftp_srv_conn == NULL)
	{
		// error
		FTP_PRINTF("Failed to create socket\r\n");

		// go back
		return;
	}

	// Bind to port 21 (FTP) with default IP address
	netconn_bind(ftp_srv_conn, NULL, FTP_SERVER_PORT);

	// put the connection into LISTEN state
	netconn_listen(ftp_srv_conn);

	while (1)
	{
		// Wait for incoming connections
		if (netconn_accept(ftp_srv_conn, &ftp_client_conn) == ERR_OK)
		{
			// Look for the first unused connection
			for (index = 0; index < FTP_NBR_CLIENTS; index++)
			{
				if (ftp_links[index].ftp_connection == NULL && ftp_links[index].task_handle == NULL)
					break;
			}

			// all connections in use?
			if (index >= FTP_NBR_CLIENTS)
			{
				// tell that no connections are allowed
				netconn_write(ftp_client_conn, no_conn_allowed, strlen(no_conn_allowed), NETCONN_COPY);

				// delete the connection.
				netconn_delete(ftp_client_conn);

				// reset the socket to be sure
				ftp_client_conn = NULL;

				// feedback
				FTP_PRINTF("FTP connection denied, all connections in use\r\n");

			}
			// not all connections in use
			else
			{
				// copy client connection
				ftp_links[index].ftp_connection = ftp_client_conn;

				// zero out client connection
				ftp_client_conn = NULL;

				snprintf(ftp_links[index].task_name, 12, "ftp_task_%d", index);

				// try and start the FTP task for this connection
				ftp_start_task(&ftp_links[index], index);
			}
		}
	}

	// delete the connection.
	netconn_delete(ftp_srv_conn);
}
