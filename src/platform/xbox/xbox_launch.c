// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 Ryzee119

#include "lithiumx.h"
#include <xboxkrnl/xboxkrnl.h>
#include <windows.h>
#include <stdbool.h>
#include <string.h>

static bool symbolicLinkToXboxPath(const CHAR *driveLetter, INT driveLength, PCHAR fullPath, INT maximumLength)
{
    NTSTATUS status;
    HANDLE symbolicLinkHandle;
    OBJECT_ATTRIBUTES symbolicLinkObject;
    CHAR symbolicLinkBuffer[MAX_PATH];

    // Path needs to be the the format "\??\X:" where X is the driveLetter
    strcpy(&symbolicLinkBuffer[0], "\\??\\");
    strncpy(&symbolicLinkBuffer[4], driveLetter, driveLength);
    strcpy(&symbolicLinkBuffer[4 + driveLength], ":");

    OBJECT_STRING symbolicLinkString = {
        .Length = strlen(symbolicLinkBuffer),
        .MaximumLength = MAX_PATH,
        .Buffer = symbolicLinkBuffer
    };

    OBJECT_STRING fullPathObject = {
        .Length = 0,
        .MaximumLength = maximumLength - 1,
        .Buffer = fullPath
    };

    InitializeObjectAttributes(&symbolicLinkObject, &symbolicLinkString, OBJ_CASE_INSENSITIVE, NULL, NULL);
    status = NtOpenSymbolicLinkObject(&symbolicLinkHandle, &symbolicLinkObject);
    if (!NT_SUCCESS(status))
    {
        return false;
    }

    status = NtQuerySymbolicLinkObject(symbolicLinkHandle, &fullPathObject, NULL);
    NtClose(symbolicLinkHandle);
    if (!NT_SUCCESS(status))
    {
        return false;
    }

    // The returned string isn't terminated.
    fullPath[fullPathObject.Length] = '\0';
    return true;
}

// Returns the first character that is A-Z or a-z from the start of the input string.
static const char *firstAlphaNumericCharacter(const char *str)
{
    while (*str != '\0') {
        if (isalnum(*str)) {
            return str;
        }
        str++;
    }
    return NULL;
}

/**
 * Converts a DOS-style path (eg. "c:/foo/bar.txt") to a XBOX-style
 * path (eg. "\Device\Harddisk0\Partition2\foo\bar.txt").
**/
int XboxGetFullLaunchPath(const char *input, char *output)
{
    const char *driveLetter = NULL;
    const char *pathStart = NULL;

    int inputPathLen = strlen(input);
    if (inputPathLen == 0)
    {
        return ERROR_BAD_PATHNAME;
    }

    // If the input path is already in 'Xbox format', copy it directly and leave
    if (memcmp(input, "\\Device\\", (inputPathLen < 8) ? inputPathLen : 8) == 0) {
        strcpy(output, input);
        return STATUS_SUCCESS;
    }

    // Get C:\foo\bar.txt from a path with prefixes like \\??\\C:\foo\bar.txt
    const char *drivePathStart = firstAlphaNumericCharacter(input);
    // Get :\foo\bar.text from C:\foo\bar.txt
    const char *driveSeparator = strchr(drivePathStart, ':');

    output[0] = '\0';

    // If the path has a ':', we assume "driveLetter:\pathStart"
    if (driveSeparator) {
        driveLetter = drivePathStart;
        pathStart = firstAlphaNumericCharacter(driveSeparator);
    } else {
        pathStart = drivePathStart;
    }

    // At this point pathStart should look like "foo\bar.txt"
    // The variable driveLetter will contain the symbolically linked partition string unless we
    // were supplied a relative path. If we were supplied a relative path, determine the currently running xbe directory
    // and use this path to prefix. If that fails, we fallback to CdRom0.
    if (driveLetter != NULL) {
        int driveLetterLen = driveSeparator - drivePathStart;
        if (symbolicLinkToXboxPath(driveLetter, driveLetterLen, output, MAX_PATH) == false)
        {
            // Not mounted or invalid drive letter
            return ERROR_INVALID_DRIVE;
        }
    // Path didnt have a ":" so it doesnt have a driveLetter. Fallback to relative path processing
    } else {
        nxGetCurrentXbeNtPath(output);
        // nxGetCurrentXbeNtPath returns \Device\*\foo\bar.xbe. So drop the last file name to get \Device\*\foo\
        // First check for the last \. If that fails try a /.
        char *lastSlash = NULL;
        if (lastSlash == NULL) {
            lastSlash = strrchr(output, '\\');
        }
        if (lastSlash == NULL) {
            lastSlash = strrchr(output, '/');
        }
        if (lastSlash) {
            lastSlash[1] = '\0';
        }
        // If it couldn't find a slash, something is wrong. Fallback to the CdRom0 path.
        else
        {
            strcpy(output, "\\Device\\CdRom0\\");
        }
    }

    // Make sure the prefix ends in a backslash
    size_t endOfPrefix = strlen(output);
    if (output[endOfPrefix - 1] != '\\' && output[endOfPrefix - 1] != '/')
    {
        output[endOfPrefix++] = '\\';
    }

    strcpy(&output[endOfPrefix], pathStart);

    // Replace any remaining forward slashes with backslashes
    char *slash = strchr(output, '/');
    while (slash)
    {
        *slash = '\\';
        slash = strchr(slash + 1, '/');
    }

    return STATUS_SUCCESS;
}

#define VIRTUAL_ATTACH 0x1EE7CD01
#define VIRTUAL_DETACH 0x1EE7CD02

#define VIRTUAL_ATTACH_CERBIOS 0xCE52B01
#define VIRTUAL_DETACH_CERBIOS 0xCE52B02

#define MAX_IMAGE_SLICES 8

// MAX_IMAGE_SLICES + 1 is a compatibility extension required for some kernels
typedef struct attach_slice_data {
    uint32_t num_slices;
    ANSI_STRING slice_files[MAX_IMAGE_SLICES + 1];
} attach_slice_data_t;

typedef struct _ATTACH_SLICE_DATA_CERBIOS {
    uint8_t num_slices;
    uint8_t DeviceType;
    uint8_t Reserved1;
    uint8_t Reserved2;
    ANSI_STRING slice_files[MAX_IMAGE_SLICES];
    ANSI_STRING mount_point;
} attach_slice_data_cerbios_t;

void xbox_mount_iso(const char *path)
{
    char *xbox_path = lv_mem_alloc(MAX_PATH);
    XboxGetFullLaunchPath(path, xbox_path);
}

static int sort_slices(const void* a, const void* b) {
    ANSI_STRING* strA = (ANSI_STRING*)a;
    ANSI_STRING* strB = (ANSI_STRING*)b;

    // Compare the ANSI_STRING contents using strncmp
    return strncmp(strA->Buffer, strB->Buffer, LV_MIN(strA->Length, strB->Length));
}

void platform_launch_iso(const char *path)
{
    uint8_t num_slices = 0;
    ANSI_STRING dev_name;
    attach_slice_data_cerbios_t *slices_cerbios;
    attach_slice_data_t *slices;

    char *xbox_path = lv_mem_alloc(MAX_PATH);
    char *sym_path = lv_mem_alloc(MAX_PATH);

    // Prepare search folder for supported ISOs
    strcpy(sym_path, path);

    char *end_path = strrchr(sym_path, '\\');
    strcpy(end_path, "\\");
    XboxGetFullLaunchPath(sym_path, xbox_path);
    strcpy(end_path, "\\*");

    // Find all ISO, CCI files and set num_slices
    ANSI_STRING slice_files[MAX_IMAGE_SLICES];
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFileA(sym_path, &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        const char *matching_ext = &path[strlen(path) - 4];
        int end = strlen(xbox_path);
        do
        {
            if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0)
            {
                char *ext = &findData.cFileName[strlen(findData.cFileName) - 4];
                if (strcasecmp(ext, matching_ext) == 0)
                {
                    strcat(xbox_path, findData.cFileName);
                    char *slice_str = strdup(xbox_path);
                    RtlInitAnsiString(&slice_files[num_slices], slice_str);
                    slice_files[num_slices].MaximumLength = slice_files[num_slices].Length + 1;
                    num_slices++;
                    xbox_path[end] = '\0';
                }
            }
        } while (FindNextFileA(hFind, &findData) != 0);
    }

    if (num_slices == 0)
    {
        lv_mem_free(xbox_path);
        lv_mem_free(sym_path);
        return;
    }

    // Should be sorted numerically - assumes split isos are atleast numbered sequentially
    qsort(slice_files, num_slices, sizeof(ANSI_STRING), sort_slices);

    // CerBios has special handling of CCI/ISO images
    if (XboxKrnlVersion.Build >= 8008)
    {
        slices_cerbios = lv_mem_alloc(sizeof(attach_slice_data_cerbios_t));
        memset(slices_cerbios, 0, sizeof(attach_slice_data_cerbios_t));

        bool is_cci = strcmp((char *)(path + strlen(path) - 3), "cci") == 0;
        slices_cerbios->DeviceType = (is_cci) ? 0x64 : 0x44; // Means CCI or normal ISO
        slices_cerbios->num_slices = num_slices;
        RtlInitAnsiString(&slices_cerbios->mount_point, "\\Device\\CdRom0");
        RtlInitAnsiString(&dev_name, "\\Device\\Virtual0\\Image0");
        memcpy(slices_cerbios->slice_files, slice_files, sizeof(slice_files));
    }
    else
    {
        slices = lv_mem_alloc(sizeof(attach_slice_data_t));
        memset(slices, 0, sizeof(attach_slice_data_t));
        slices->num_slices = num_slices;
        RtlInitAnsiString(&dev_name, "\\Device\\CdRom1");
        memcpy(slices->slice_files, slice_files, sizeof(slice_files));
    }

    OBJECT_ATTRIBUTES obj_attr;
    InitializeObjectAttributes(&obj_attr, &dev_name, OBJ_CASE_INSENSITIVE, NULL, NULL);

    HANDLE handle;
    IO_STATUS_BLOCK io_status;

    NTSTATUS status = NtOpenFile(&handle, GENERIC_READ | SYNCHRONIZE, &obj_attr, &io_status, FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("ERROR: Could not open %s\n", dev_name.Buffer);
        goto iso_cleanup;
    }

    nxUnmountDrive('D');

    if (XboxKrnlVersion.Build >= 8008)
    {
        NtDeviceIoControlFile(handle, NULL, NULL, NULL, &io_status, VIRTUAL_DETACH_CERBIOS, NULL, 0, NULL, 0);
        NtDeviceIoControlFile(handle, NULL, NULL, NULL, &io_status, VIRTUAL_ATTACH_CERBIOS, slices_cerbios, sizeof(attach_slice_data_cerbios_t), NULL, 0);
    }
    else
    {
        NtDeviceIoControlFile(handle, NULL, NULL, NULL, &io_status, VIRTUAL_DETACH, NULL, 0, NULL, 0);
        NtDeviceIoControlFile(handle, NULL, NULL, NULL, &io_status, VIRTUAL_ATTACH, slices, sizeof(attach_slice_data_t), NULL, 0);
    }

    // If the user did a quick reboot, or somehow got back to us make sure we can use the volume again
    IoDismountVolumeByName(&dev_name);

iso_cleanup:
    lv_mem_free(xbox_path);
    lv_mem_free(sym_path);
    NtClose(handle);
}
