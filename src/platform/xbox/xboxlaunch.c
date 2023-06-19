// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 Ryzee119

#include <xboxkrnl/xboxkrnl.h>
#include <windows.h>
#include <stdbool.h>
#include <string.h>

static bool symbolicLinkToXboxPath(const CHAR driveLetter, PCHAR fullPath, INT maximumLength)
{
    NTSTATUS status;
    HANDLE symbolicLinkHandle;
    OBJECT_ATTRIBUTES symbolicLinkObject;
    CHAR symbolicLinkBuffer[] = "\\??\\?:";

    OBJECT_STRING symbolicLinkString = {
        .Length = sizeof(symbolicLinkBuffer) - 1,
        .MaximumLength = sizeof(symbolicLinkBuffer),
        .Buffer = symbolicLinkBuffer
    };

    OBJECT_STRING fullPathObject = {
        .Length = 0,
        .MaximumLength = maximumLength - 1,
        .Buffer = fullPath
    };

    symbolicLinkBuffer[4] = driveLetter;

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

static bool isAlphaNumeric(const char character)
{
    return (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9');
}

// Returns the first character that is A-Z or a-z from the start of the input string.
static const char *firstAlphaNumericCharacter(const char *str)
{
    while (*str != '\0') {
        if (isAlphaNumeric(*str)) {
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
    // If the input path is already in 'Xbox format', copy it directly and leave
    if (memcmp(input, "\\Device\\", 8) == 0) {
        strcpy(output, input);
        return STATUS_SUCCESS;
    }

    const char *driveSeparator = strchr(input, ':');
    const char *pathStart = input;
    char driveLetter = '\0';

    output[0] = '\0';

    if (strlen(input) == 0)
    {
        return ERROR_BAD_PATHNAME;
    }

    // If the path has a ':', we assume "driveLetter:\pathStart"
    if (driveSeparator) {
        // Paths like ":foo/path" will cause issues. Leave with error
        if (driveSeparator == input) {
            return ERROR_BAD_PATHNAME;
        }

        driveLetter = *(driveSeparator - 1);
        if (isAlphaNumeric(driveLetter) == false) {
            return ERROR_BAD_PATHNAME;
        }
        pathStart = firstAlphaNumericCharacter(driveSeparator);
    } else {
        pathStart = firstAlphaNumericCharacter(input);
    }

    // At this point pathStart should look like "foo\bar.txt"
    // The variable driveLetter will contain the symbolically linked partition letter unless we
    // were supplied a relative path. If we were supplied a relative path, determine the currently running xbe directory
    // and use this path to prefix. If that fails, we fallback to CdRom0.
    if (driveLetter != '\0') {
        if (symbolicLinkToXboxPath(driveLetter, output, MAX_PATH) == false)
        {
            // Not mounted or invalid drive letter
            return ERROR_INVALID_DRIVE;
        }
    } else {
        nxGetCurrentXbeNtPath(output);
        // nxGetCurrentXbeNtPath returns \Device\*\foo\bar.xbe. So drop the last file name.
        // First check for the last \. If that fails try a /.
        char * lastSlash = strrchr(output, '\\');
        if (lastSlash == NULL) {
            lastSlash = strrchr(output, '/');
        }

        if (lastSlash) {
            lastSlash[1] = '\0';
        }
        else
        {
            // If it couldn't find a slash, something is wrong. Fallback to the CdRom0 path.
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
