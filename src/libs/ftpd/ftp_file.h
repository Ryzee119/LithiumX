/*
 * ftps_file.h
 *
 *  Created on: Feb 18, 2020
 *      Author: Sander
 */

#ifndef ETH_FTP_FTP_FILE_H_
#define ETH_FTP_FTP_FILE_H_

#include <stdio.h>
#include <stdint.h>
#include <fileapi.h>
#include "ftp_server.h"
#include "lwip/opt.h"

#define _MAX_LFN 255
#define FA_READ 0x01
#define FA_WRITE 0x02
#define FA_CREATE_ALWAYS 0x08

typedef struct
{
	HANDLE h;
	char path[_MAX_LFN];
    int root_index;
} dir_handle_t;

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define FILE_CACHE_SIZE (128 * 1024)

typedef struct
{
    HANDLE h;
    char path[_MAX_LFN];
    int cache_index;
    char cache_buf[2][FILE_CACHE_SIZE + TCP_MSS] __attribute__((aligned(PAGE_SIZE)));
    ULONGLONG write_total;
    ULONGLONG bytes_cached;
    HANDLE write_complete;
    BOOL opened_for_write;
} fil_handle_t;

#define DIR dir_handle_t
#define FIL fil_handle_t

/**
 * wrapper functions for file access from FTP server.
 */
typedef struct
{
    uint32_t fsize;  /* File size */
    uint16_t fdate;  /* Modified date */
    uint16_t ftime;  /* Modified time */
    uint8_t fattrib; /* File attribute */
    char fname[256]; /* File name */
} FILINFO;

typedef enum
{
    FR_OK = 0,              /* (0) Succeeded */
    FR_DISK_ERR,            /* (1) A hard error occurred in the low level disk I/O layer */
    FR_INT_ERR,             /* (2) Assertion failed */
    FR_NOT_READY,           /* (3) The physical drive cannot work */
    FR_NO_FILE,             /* (4) Could not find the file */
    FR_NO_PATH,             /* (5) Could not find the path */
    FR_INVALID_NAME,        /* (6) The path name format is invalid */
    FR_DENIED,              /* (7) Access denied due to prohibited access or directory full */
    FR_EXIST,               /* (8) Access denied due to prohibited access */
    FR_INVALID_OBJECT,      /* (9) The file/directory object is invalid */
    FR_WRITE_PROTECTED,     /* (10) The physical drive is write protected */
    FR_INVALID_DRIVE,       /* (11) The logical drive number is invalid */
    FR_NOT_ENABLED,         /* (12) The volume has no work area */
    FR_NO_FILESYSTEM,       /* (13) There is no valid FAT volume */
    FR_MKFS_ABORTED,        /* (14) The f_mkfs() aborted due to any problem */
    FR_TIMEOUT,             /* (15) Could not get a grant to access the volume within defined period */
    FR_LOCKED,              /* (16) The operation is rejected according to the file sharing policy */
    FR_NOT_ENOUGH_CORE,     /* (17) LFN working buffer could not be allocated */
    FR_TOO_MANY_OPEN_FILES, /* (18) Number of open files > FF_FS_LOCK */
    FR_INVALID_PARAMETER    /* (19) Given parameter is invalid */
} FRESULT;

#define AM_RDO 0x01 /* Read only */
#define AM_HID 0x02 /* Hidden */
#define AM_SYS 0x04 /* System */
#define AM_DIR 0x10 /* Directory */
#define AM_ARC 0x20 /* Archive */

FRESULT ftps_f_stat(const char *path, FILINFO *nfo);
FRESULT ftps_f_opendir(DIR *dp, const char *path);
FRESULT ftps_f_readdir(DIR *dp, FILINFO *fno);
FRESULT ftps_f_unlink(const char *path);
FRESULT ftps_f_open(FIL *fp, const char *path, uint8_t mode);
size_t ftps_f_size(FIL *fp);
FRESULT ftps_f_close(FIL *fp);
FRESULT ftps_f_write(FIL *fp, struct pbuf *p, uint32_t buflen, uint32_t *written);
FRESULT ftps_f_read(FIL *fp, void *buffer, uint32_t len, uint32_t *read, uint32_t position);
FRESULT ftps_f_mkdir(const char *path);
FRESULT ftps_f_rename(const char *from, const char *to);
FRESULT ftps_f_utime(const char *path, const FILINFO *fno);
FRESULT ftps_f_getfree(const char *path, uint32_t *nclst, void *fs);

#endif /* ETH_FTP_FTP_FILE_H_ */
