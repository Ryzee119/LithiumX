/*
 * ftps_file.c
 *
 *  Created on: Feb 18, 2020
 *      Author: Sander
 */

#include "ftp.h"
#include "ftp_file.h"
#include "ftp_server.h"
#include <fileapi.h>
#include <windef.h>
#include <winbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <nxdk/mount.h>

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE) (LONG_PTR)-1)
#endif

#ifdef NXDK
#include <xboxkrnl/xboxkrnl.h>
static const char root_drives[][3] = {"/C", "/D", "/E", "/F", "/G", "/R", "/S", "/V", "/W", "/A", "/B", "/P", "/X", "/Y", "/Z"};
static int root_index;
#define FILE_DBG DbgPrint
#else
#include <timezoneapi.h>
#include <handleapi.h>
#define FILE_DBG printf
#endif

static char *get_win_path(const char *in, char* out)
{
	strcpy(out, in);

	// Replace "/" with "\"
	char *sep = strchr(out, '/');
	while (sep)
	{
		*sep = '\\';
		sep = strchr(out, '/');
	}

#ifdef NXDK
	// Replace \E\ with "E:\" etc.
	if (out[0] == '\\' && out[1] != '\0')
	{
		out[0] = out[1];
		out[1] = ':';
	}
#endif
	return out;
}

#ifdef FTP_CUSTOM_ROOT_PATH
static int is_root_path(const char *path)
{
	int cnt = sizeof(root_drives) / sizeof(root_drives[0]);
	for (int i = 0; i < cnt; i++)
	{
		if (strcmp(path, root_drives[i]) == 0)
		{
			return 1;
		}
	}
	return 0;
}
#endif

static uint8_t win_to_ftps_attr(DWORD attr)
{
	uint8_t fattrib = 0;
	fattrib |= (attr & FILE_ATTRIBUTE_DIRECTORY) ? AM_DIR : 0;
	fattrib |= (attr & FILE_ATTRIBUTE_HIDDEN)    ? AM_HID : 0;
	fattrib |= (attr & FILE_ATTRIBUTE_ARCHIVE)   ? AM_ARC : 0;
	fattrib |= (attr & FILE_ATTRIBUTE_SYSTEM)    ? AM_SYS : 0;
	fattrib |= (attr & FILE_ATTRIBUTE_READONLY)  ? AM_RDO : 0;
	return fattrib;
}

static void win_to_ftps_time(FILETIME filetime, uint16_t *fdate, uint16_t *ftime)
{
	uint16_t year, month, day, h, m, s;
// Ref http://elm-chan.org/fsw/ff/doc/sfileinfo.html
#ifdef NXDK
	TIME_FIELDS ft;
	RtlTimeToTimeFields((PLARGE_INTEGER)&filetime, &ft);
	year = ((ft.Year - 1980) << 9) & 0xFE00;
	month = (ft.Month << 5) & 0x01E0;
	day = ft.Day & 0x001F;
	h = (ft.Hour << 11) & 0xF800;
	m = (ft.Minute << 5) & 0x07E0;
	s = (ft.Second / 2) & 0x001F;
#else
	SYSTEMTIME st;
	FileTimeToSystemTime(&filetime, &st);
	year = ((st.wYear - 1980) << 9) & 0xFE00;
	month = (st.wMonth << 5) & 0x01E0;
	day = st.wDay & 0x001F;
	h = (st.wHour << 11) & 0xF800;
	m = (st.wMinute << 5) & 0x07E0;
	s = (st.wSecond / 2) & 0x001F;
#endif
	*fdate = year | month | day;
	*ftime = h | m | s;
}

FRESULT ftps_f_stat(const char *path, FILINFO *nfo)
{
	// Ref http://elm-chan.org/fsw/ff/doc/stat.html
	char win_path[_MAX_LFN];
	char *p = get_win_path(path, win_path);
	DWORD attr = GetFileAttributesA(p);
	HANDLE hfile = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

#ifdef FTP_CUSTOM_ROOT_PATH
	if (is_root_path(path))
	{
		nfo->fattrib = AM_DIR;

		// Create default year and time for root dirs
		nfo->fdate = 20 << 9; //Year 2000
		nfo->fdate |= 1 << 5; //Jan
		nfo->fdate |= 1 << 0; //1st
		nfo->ftime = 0;
		return FR_OK;
	}
#endif

	if (attr == INVALID_FILE_ATTRIBUTES && hfile == INVALID_HANDLE_VALUE)
	{
		FILE_DBG("Could not find %s %08x %08x\n", path, attr, hfile);
		return FR_NO_FILE;
	}

	// Apply the files attributes
	nfo->fattrib = win_to_ftps_attr(attr);

	// Get its filesize
	nfo->fsize = GetFileSize(hfile, NULL);
	if (nfo->fsize == INVALID_FILE_SIZE)
	{
		nfo->fsize = 0;
	}

	// Get its timestamp
	FILETIME lastWriteTime;
	if (!GetFileTime(hfile, NULL, NULL, &lastWriteTime))
	{
		nfo->fdate = 0;
		nfo->ftime = 0;
	}
	win_to_ftps_time(lastWriteTime, &nfo->fdate, &nfo->ftime);
	CloseHandle(hfile);
	return FR_OK;
}

FRESULT ftps_f_opendir(DIR *dp, const char *path)
{
	char *p = NULL;
	dp->h = INVALID_HANDLE_VALUE;
	FILE_DBG("Opening directory %s\n", path);

// If a custom root path is defined, we return the custom directory
#ifdef FTP_CUSTOM_ROOT_PATH
	//FTP client is request the root directory listing
	if (strcmp(path, "/") == 0 || strcmp(path, "\\") == 0)
	{
		root_index = 0;
		strcpy(dp->path, "root");
		return FR_OK;
	}
#endif

	if (p == NULL)
	{
		p = get_win_path(path, dp->path);
	}

	// Check that the directory exists
	DWORD res = GetFileAttributesA(dp->path);
	if (res == INVALID_FILE_ATTRIBUTES)
	{
		FILE_DBG("Could not find %s\n", dp->path);
		return FR_NO_PATH;
	}

	// Add \\* to the end of the path in preparation for searching
	int len = strlen(dp->path);
	if (dp->path[len - 1] == '\\')
	{
		dp->path[len + 0] = '*';
		dp->path[len + 1] = '\0';
	}
	else
	{
		dp->path[len + 0] = '\\';
		dp->path[len + 1] = '*';
		dp->path[len + 2] = '\0';
	}
	FILE_DBG("Search path is %s\n", dp->path);
	return FR_OK;
}

FRESULT ftps_f_readdir(DIR *dp, FILINFO *nfo)
{
	WIN32_FIND_DATA findFileData;
	FILE_DBG("Looking for files in %s\n", dp->path);

#ifdef FTP_CUSTOM_ROOT_PATH
	if (strcmp(dp->path, "root") == 0)
	{
		// null terminate the filename in case we don't find anything
		nfo->fname[0] = '\0';

		for(root_index; root_index < (sizeof(root_drives) / sizeof(root_drives[0])); root_index++)
		{
			if(!nxIsDriveMounted(root_drives[root_index][1]))
				continue;

			nfo->fname[0] = root_drives[root_index][1];
			nfo->fname[1] = '\0';
			nfo->fattrib = AM_DIR;

			// Create default year and time for root dirs
			nfo->fdate = 20 << 9; //Year 2000
			nfo->fdate |= 1 << 5; //Jan
			nfo->fdate |= 1 << 0; //1st
			nfo->ftime = 0;

			// Break out of the loop once we find a valid drive
			break;
		}

		return FR_OK;
	}
#endif

	if (dp->h == INVALID_HANDLE_VALUE)
	{
		dp->h = FindFirstFile(dp->path, &findFileData);
		if (dp->h == INVALID_HANDLE_VALUE)
		{
			FILE_DBG("First file. No files found\n");
			nfo->fname[0] = '\0';
			return FR_OK;
		}
	}
	else
	{
		if (FindNextFile(dp->h, &findFileData) == 0)
		{
			FILE_DBG("No more files\n");
			nfo->fname[0] = '\0';
			CloseHandle(dp->h);
			return FR_OK;
		}
	}

	// Populate the file info struct
	strncpy(nfo->fname, findFileData.cFileName, _MAX_LFN);
	nfo->fattrib = win_to_ftps_attr(findFileData.dwFileAttributes);
	nfo->fsize = findFileData.nFileSizeLow;
	win_to_ftps_time(findFileData.ftLastWriteTime, &nfo->fdate, &nfo->ftime);
	FILE_DBG("Found %s in %s\n", nfo->fname, dp->path);
	return FR_OK;
}

FRESULT ftps_f_unlink(const char *path)
{
	char win_path[_MAX_LFN];
	char *p = get_win_path(path, win_path);
	FILE_DBG("Deleting %s\n", path);
	DWORD attr = GetFileAttributesA(p);
	if (attr & FILE_ATTRIBUTE_DIRECTORY)
	{
		RemoveDirectory(p);
	}
	else
	{
		DeleteFile(p);
	}
	return FR_OK;
}

static void NTAPI async_writer(PKSTART_ROUTINE StartRoutine, PVOID StartContext)
{
	FIL *fp = (FIL *)StartContext;
	HANDLE hfile = fp->h;
	int cache_index = 0;
	DWORD bw;
	while (1)
	{
		NtWaitForSingleObject(fp->cache_mutex[cache_index], FALSE, NULL);
		if (fp->thread_running == 0)
		{
			break;
		}
		WriteFile(hfile, (LPVOID)fp->cache_buf[cache_index], FILE_CACHE_SIZE, &bw, NULL);
		NtReleaseMutant(fp->cache_mutex[cache_index], NULL);
		cache_index ^= 1;
	}
	PsTerminateSystemThread(0);
}

FRESULT ftps_f_open(FIL *fp, const char *path, uint8_t mode)
{
	DWORD access = 0, disposition = 0;
	access |= (mode & FA_READ) ? GENERIC_READ : 0;
	access |= (mode & FA_WRITE) ? GENERIC_WRITE : 0;
	disposition = (mode & FA_CREATE_ALWAYS) ? CREATE_ALWAYS : OPEN_EXISTING;

	get_win_path(path, fp->path);
	FILE_DBG("Opening %s\n", fp->path);
	HANDLE hfile = CreateFileA(fp->path, access, FILE_SHARE_READ, NULL, disposition, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
	if (hfile == INVALID_HANDLE_VALUE)
	{
		return FR_NO_FILE;
	}
	fp->h = hfile;
	fp->write_total = -1;
	fp->bytes_cached = 0;
	fp->cache_index = 0;
	fp->thread_running = 0;

	if (access & GENERIC_WRITE)
	{
		fp->thread_running = 1;
		fp->write_total = 0;
		NtCreateMutant(&fp->cache_mutex[0], NULL, TRUE);
		NtCreateMutant(&fp->cache_mutex[1], NULL, FALSE);
		PsCreateSystemThreadEx(&fp->write_thread, 0, 4096, 0, NULL, NULL, fp, FALSE, FALSE, async_writer);
	}
	//FIXME async_reader?

	return FR_OK;
}

size_t ftps_f_size(FIL *fp)
{
	HANDLE hfile = fp->h;
	uint32_t sz = GetFileSize(hfile, NULL);
	if (sz == INVALID_FILE_SIZE)
	{
		sz = 0;
	}
	return sz;
}

FRESULT ftps_f_close(FIL *fp)
{
	HANDLE hfile = fp->h;
	FRESULT res = FR_OK;
	DWORD bw;

	// Did we have the file opened as write?
	if (fp->write_total != -1)
	{
		//Shutdown async_writer thread
		fp->thread_running = 0;
		NtReleaseMutant(fp->cache_mutex[0], NULL);
		NtReleaseMutant(fp->cache_mutex[1], NULL);
		NtWaitForSingleObject(fp->write_thread, FALSE, NULL);
		NtClose(fp->write_thread);
		NtClose(fp->cache_mutex[0]);
		NtClose(fp->cache_mutex[1]);

		// If we have pending data in cache, write it out.
		if (fp->bytes_cached > 0)
		{
			// Have to write out a full sector even if the remaining bytes is less to maintain
			// zero buffering. The size if fixed below.
			int write_len = (fp->bytes_cached + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
			res = WriteFile(hfile, (LPVOID)fp->cache_buf[fp->cache_index], write_len, &bw, NULL) ? FR_OK : FR_INVALID_PARAMETER;
			fp->write_total += fp->bytes_cached;
		}

		// Fix the final output size.
		#ifdef NXDK
		NTSTATUS status;
		IO_STATUS_BLOCK iostatusBlock;
		FILE_END_OF_FILE_INFORMATION endOfFile;
		FILE_ALLOCATION_INFORMATION allocation;

		endOfFile.EndOfFile.QuadPart = fp->write_total;
		allocation.AllocationSize.QuadPart = fp->write_total;
		status = NtSetInformationFile(hfile, &iostatusBlock, &endOfFile, sizeof(endOfFile), FileEndOfFileInformation);
		if (!NT_SUCCESS(status))
			FILE_DBG("Error setting File End information");

		status = NtSetInformationFile(hfile, &iostatusBlock, &allocation, sizeof(allocation), FileAllocationInformation);
		if (!NT_SUCCESS(status))
			FILE_DBG("Error setting File Allocation information");
		#else
		FILE_END_OF_FILE_INFO endOfFile;
		endOfFile.EndOfFile.QuadPart = fp->write_total;
		SetFileInformationByHandle(hfile, FileEndOfFileInfo, &endOfFile, sizeof(endOfFile));
		#endif
	}

	CloseHandle(hfile);
	return res;
}

FRESULT ftps_f_write(FIL *fp, const void *buffer, uint32_t buflen, uint32_t *written)
{
	HANDLE hfile = fp->h;
	FRESULT res = FR_OK;

	const uint8_t *prcvbuf = (const uint8_t *)buffer;
	// Write the correct amount of bytes to fill up to FILE_CACHE_SIZE exactly.
	int next_spot = fp->bytes_cached + buflen;
	int len = (next_spot < FILE_CACHE_SIZE) ? buflen : (buflen - (next_spot - FILE_CACHE_SIZE));
	memcpy(&fp->cache_buf[fp->cache_index][fp->bytes_cached], prcvbuf, len);
	fp->bytes_cached += len;

	// If we have filled the file write cache, write it out.
	assert(fp->bytes_cached <= FILE_CACHE_SIZE);
	if (fp->bytes_cached == FILE_CACHE_SIZE)
	{
		//Get ownership of next buffer
		NtWaitForSingleObject(fp->cache_mutex[fp->cache_index ^ 1], FALSE, NULL);
		//Release ownership of current buffer to start writing in the async_writer thread
		NtReleaseMutant(fp->cache_mutex[fp->cache_index], NULL);
		fp->cache_index ^= 1;
		fp->write_total += FILE_CACHE_SIZE;

		// If we have remaining bytes, put them in the now cleared cache buffer.
		int remaining = buflen - len;
		assert(remaining >= 0);
		if (remaining > 0)
		{
			memcpy(fp->cache_buf[fp->cache_index], &prcvbuf[len], remaining);
		}
		fp->bytes_cached = remaining;
	}

	return res;
}

FRESULT ftps_f_read(FIL *fp, void *buffer, uint32_t len, uint32_t *read)
{
	HANDLE hfile = fp->h;
	return (ReadFile(hfile, (LPVOID)buffer, len, (LPDWORD)read, NULL)) ? FR_OK : FR_INVALID_PARAMETER;
}

FRESULT ftps_f_mkdir(const char *path)
{
	char win_path[_MAX_LFN];
	get_win_path(path, win_path);
	FILE_DBG("Creating dir %s\n", win_path);
	BOOL res = CreateDirectoryA(win_path, NULL);
	FILE_DBG("%s path: %s; success: %d\n", __FUNCTION__, win_path, res != 0);
	return (res) ? FR_OK : FR_INVALID_PARAMETER;
}

FRESULT ftps_f_rename(const char *from, const char *to)
{
	char win_from[_MAX_LFN];
	char win_to[_MAX_LFN];
	get_win_path(from, win_from);
	get_win_path(to, win_to);
	return (MoveFile(win_from, win_to)) ? FR_OK : FR_INVALID_PARAMETER;
}

FRESULT ftps_f_utime(const char *path, const FILINFO *fno)
{
	// http://elm-chan.org/fsw/ff/doc/utime.html
	FILE_DBG("%s: NOT IMPLEMENTED\n", __FUNCTION__);
	return FR_OK;
}

FRESULT ftps_f_getfree(const char *path, uint32_t *nclst, void *fs)
{
	// Not implemented
	FILE_DBG("%s: NOT IMPLEMENTED\n", __FUNCTION__);
	return FR_OK;
}
