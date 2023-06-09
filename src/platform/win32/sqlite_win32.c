// Copyright 2023, Ryzee119
// SPDX-License-Identifier: MIT

/*
XBE_TITLE = nxdk\ sample\ -\ hello
GEN_XISO = $(XBE_TITLE).iso
NXDK_DIR ?= $(CURDIR)/../..
SRCS = \
	$(CURDIR)/main.c \
	$(CURDIR)/sqlite_nxdk.c \
	$(CURDIR)/sqlite/sqlite3.c
CFLAGS += \
	-DSQLITE_DISABLE_INTRINSIC=1 \
	-DSQLITE_WITHOUT_MSIZE=1 \
	-DSQLITE_DISABLE_LFS=1 \
	-DSQLITE_SYSTEM_MALLOC=1 \
	-DSQLITE_THREADSAFE=0 \
	-DSQLITE_NO_SYNC=1 \
	-DSQLITE_OS_OTHER=1 \
	-O2
include $(NXDK_DIR)/Makefile
*/

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include "sqlite3/sqlite3.h"

#define UNUSED_PARAMETER(x) (void)(x)

typedef struct _winFile
{
    sqlite3_file base;
    HANDLE h;
} winFile;

#define sql_DbgPrint (void)

static int winClose(sqlite3_file *id)
{
    winFile *f;

    f = (winFile *)id;
    if (CloseHandle(f->h))
    {
        return SQLITE_OK;
    }
    return SQLITE_IOERR_CLOSE;
}

static int winRead(sqlite3_file *id, void *pBuf, int amt, sqlite3_int64 offset)
{
    winFile *f;
    DWORD read;
    LARGE_INTEGER loffset;

    f = (winFile *)id;
    loffset.QuadPart = offset;

    if (SetFilePointerEx(f->h, loffset, NULL, FILE_BEGIN) == 0)
    {
        return SQLITE_IOERR_SEEK;
    }

    if (ReadFile(f->h, pBuf, (DWORD)amt, &read, NULL))
    {
        return (read != amt) ? SQLITE_IOERR_SHORT_READ : SQLITE_OK;
    }
    else
    {
        return SQLITE_IOERR_READ;
    }
}

static int winWrite(sqlite3_file *id, const void *pBuf, int amt, sqlite3_int64 offset)
{
    winFile *f;
    DWORD written;
    LARGE_INTEGER loffset;

    f = (winFile *)id;
    loffset.QuadPart = offset;

    if (SetFilePointerEx(f->h, loffset, NULL, FILE_BEGIN) == 0)
    {
        return SQLITE_IOERR_SEEK;
    }

    if (WriteFile(f->h, pBuf, (DWORD)amt, &written, NULL))
    {
        return SQLITE_OK;
    }
    else
    {
        return SQLITE_IOERR_WRITE;
    }
}

static int winTruncate(sqlite3_file *id, sqlite3_int64 nByte)
{
    return SQLITE_IOERR_TRUNCATE;
}

static int winSync(sqlite3_file *id, int flags)
{
    return SQLITE_OK;
}

static int winFileSize(sqlite3_file *id, sqlite3_int64 *pSize)
{
    winFile *f;
    LARGE_INTEGER fs;

    f = (winFile *)id;
    fs.LowPart = GetFileSize(f->h, (LPDWORD)&fs.HighPart);

    if (fs.LowPart == INVALID_FILE_SIZE)
    {
        return SQLITE_IOERR_FSTAT;
    }

    if (pSize)
    {
        *pSize = fs.QuadPart;
    }

    return SQLITE_OK;
}

static int winLock(sqlite3_file *id, int locktype)
{
    UNUSED_PARAMETER(id);
    UNUSED_PARAMETER(locktype);
    return SQLITE_OK;
}

static int winUnlock(sqlite3_file *id, int locktype)
{
    UNUSED_PARAMETER(id);
    UNUSED_PARAMETER(locktype);
    return SQLITE_OK;
}

static int winCheckReservedLock(sqlite3_file *id, int *pResOut)
{
    UNUSED_PARAMETER(id);
    *pResOut = 0;
    return SQLITE_OK;
}

static int winFileControl(sqlite3_file *id, int op, void *pArg)
{
    UNUSED_PARAMETER(id);
    UNUSED_PARAMETER(op);
    UNUSED_PARAMETER(pArg);
    return SQLITE_NOTFOUND;
}

static int winSectorSize(sqlite3_file *id)
{
    UNUSED_PARAMETER(id);
    return 4096;
}

static int winDeviceCharacteristics(sqlite3_file *id)
{
    UNUSED_PARAMETER(id);
    return SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN;
}

static const sqlite3_io_methods nxdk_io = {
    1,                        /* iVersion */
    winClose,                 /* xClose */
    winRead,                  /* xRead */
    winWrite,                 /* xWrite */
    winTruncate,              /* xTruncate */
    winSync,                  /* xSync */
    winFileSize,              /* xFileSize */
    winLock,                  /* xLock */
    winUnlock,                /* xUnlock */
    winCheckReservedLock,     /* xCheckReservedLock */
    winFileControl,           /* xFileControl */
    winSectorSize,            /* xSectorSize */
    winDeviceCharacteristics, /* xDeviceCharacteristics */
};

static DWORD sqlite_to_win_access(int sql_flags)
{
    DWORD attrib = 0;
    if (sql_flags & SQLITE_OPEN_READONLY)
    {
        attrib |= GENERIC_READ;
    }
    if (sql_flags & SQLITE_OPEN_READWRITE)
    {
        attrib |= GENERIC_READ | GENERIC_WRITE;
    }
    return attrib;
}

static DWORD sqlite_to_win_attr(int sql_flags)
{
    DWORD attrib = 0;
    if (sql_flags & SQLITE_OPEN_CREATE)
    {
        attrib |= OPEN_ALWAYS;
    }
    else
    {
        attrib |= OPEN_EXISTING;
    }
    return attrib;
}

static int winOpen(
    sqlite3_vfs *pVfs, const char *zFilename, sqlite3_file *id, int flags, int *pOutFlags)
{
    winFile *f;

    UNUSED_PARAMETER(pVfs);

    f = (winFile *)id;
    f->base.pMethods = &nxdk_io;
    f->h = CreateFileA(zFilename, sqlite_to_win_access(flags), FILE_SHARE_READ, NULL,
                       sqlite_to_win_attr(flags), FILE_ATTRIBUTE_NORMAL, NULL);
    if (f->h == INVALID_HANDLE_VALUE)
    {
        return SQLITE_CANTOPEN;
    }

    if (pOutFlags)
    {
        *pOutFlags = flags;
    }

    return SQLITE_OK;
}

static int winDelete(sqlite3_vfs *pVfs, const char *zFilename, int syncDir)
{
    UNUSED_PARAMETER(pVfs);
    UNUSED_PARAMETER(syncDir);
    if (DeleteFile(zFilename))
    {
        return SQLITE_OK;
    }
    else
    {
        return SQLITE_IOERR_DELETE_NOENT;
    }
}

static int winAccess(sqlite3_vfs *pVfs, const char *zFilename, int flags, int *pResOut)
{
    DWORD attr;

    UNUSED_PARAMETER(pVfs);

    *pResOut = 0;
    attr = GetFileAttributesA(zFilename);
    if (attr != INVALID_FILE_ATTRIBUTES)
    {
        if (flags == SQLITE_ACCESS_EXISTS || flags == SQLITE_ACCESS_READ)
        {
            *pResOut = 1;
        }
        if (flags == SQLITE_ACCESS_READWRITE)
        {
            if (!(attr & FILE_ATTRIBUTE_READONLY))
            {
                *pResOut = 1;
            }
        }
    }
    return SQLITE_OK;
}

static int winFullPathname(sqlite3_vfs *pVfs, const char *zRelative, int nFull, char *zFull)
{
    for (int i = 0; i < nFull; i++)
    {
        zFull[i] = zRelative[i];
        if (zRelative[i] == '\0')
        {
            break;
        }
    }
    zFull[nFull - 1] = '\0';
    return SQLITE_OK;
}

static void *winDlOpen(sqlite3_vfs *pVfs, const char *zFilename)
{
    UNUSED_PARAMETER(pVfs);
    UNUSED_PARAMETER(zFilename);
    return NULL;
}

static void winDlError(sqlite3_vfs *pVfs, int nBuf, char *zBufOut)
{
    UNUSED_PARAMETER(pVfs);
    sqlite3_snprintf(nBuf, zBufOut, "Loadable extensions are not supported");
    zBufOut[nBuf - 1] = '\0';
}

static void (*winDlSym(sqlite3_vfs *pVfs, void *pH, const char *zSym))(void)
{
    UNUSED_PARAMETER(pVfs);
    UNUSED_PARAMETER(pH);
    UNUSED_PARAMETER(zSym);
    return 0;
}

static void winDlClose(sqlite3_vfs *pVfs, void *pHandle)
{
    UNUSED_PARAMETER(pVfs);
    UNUSED_PARAMETER(pHandle);
    return;
}

static int winRandomness(sqlite3_vfs *pVfs, int nBuf, char *zBuf)
{
    UNUSED_PARAMETER(pVfs);
    for (int i = 0; i < nBuf; i++)
    {
        zBuf[i] = rand() % 0xFF;
    }
    return nBuf;
}

static int winSleep(sqlite3_vfs *pVfs, int microsec)
{
    Sleep((microsec+999)/1000);
    return microsec;
}

/* Return number of milliseconds since the Julian epoch of noon in Greenwich on
 * November 24, 4714 B.C according to the proleptic Gregorian calendar.
 */
static int winCurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *piNow)
{
    FILETIME ft;
    static const sqlite3_int64 winFiletimeEpoch = 23058135 * (sqlite3_int64)8640000;
    static const sqlite3_int64 max32BitValue = 4294967296;

    UNUSED_PARAMETER(pVfs);

    GetSystemTimePreciseAsFileTime(&ft);

    *piNow = winFiletimeEpoch + ((((sqlite3_int64)ft.dwHighDateTime) * max32BitValue) +
                                   (sqlite3_int64)ft.dwLowDateTime) / (sqlite3_int64)10000;
    return SQLITE_OK;
}

static int winCurrentTime(sqlite3_vfs *pVfs, double *prNow)
{
    sqlite3_int64 piNow;

    UNUSED_PARAMETER(pVfs);

    if (winCurrentTimeInt64(pVfs, &piNow) == SQLITE_OK)
    {
        piNow /= 86400000.0;
        *prNow = (double)piNow;
        return SQLITE_OK;
    }
    return SQLITE_ERROR;
}

static int winGetLastError(sqlite3_vfs *pVfs, int nBuf, char *zBuf)
{
    UNUSED_PARAMETER(pVfs);

    if (nBuf > 0)
    {
        zBuf[0] = '\0';
    }
    return GetLastError();
}

static sqlite3_vfs nxdk_vfs = {
    1,                   /* iVersion */
    sizeof(winFile),     /* szOsFile */
    MAX_PATH,            /* mxPathname */
    NULL,                /* pNext */
    "nxdk",              /* zName */
    NULL,                /* pAppData */
    winOpen,             /* xOpen */
    winDelete,           /* xDelete */
    winAccess,           /* xAccess */
    winFullPathname,     /* xFullPathname */
    winDlOpen,           /* xDlOpen */
    winDlError,          /* xDlError */
    winDlSym,            /* xDlSym */
    winDlClose,          /* xDlClose */
    winRandomness,       /* xRandomness */
    winSleep,            /* xSleep */
    winCurrentTime,      /* xCurrentTime */
    winGetLastError,     /* xGetLastError */
    winCurrentTimeInt64, /* xCurrentTimeInt64 */
    NULL,                /* xSetSystemCall */
    NULL,                /* xGetSystemCall */
    NULL,                /* xNextSystemCall */
};

sqlite3_vfs *sqlite_nxdk_fs(void)
{
    return &nxdk_vfs;
}

int sqlite3_os_init(void)
{
    sqlite3_vfs_register(sqlite_nxdk_fs(), 1);
    return SQLITE_OK;
}

int sqlite3_os_end(void)
{
    return SQLITE_OK;
}