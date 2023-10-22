// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 Ryzee119

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
