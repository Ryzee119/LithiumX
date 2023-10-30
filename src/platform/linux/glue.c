#include <pthread.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include "dash_linux.h"

// Initialize the critical section
void InitializeCriticalSection(CRITICAL_SECTION *cs)
{
    pthread_mutex_init(&cs->mutex, NULL);
}

// Enter the critical section
void EnterCriticalSection(CRITICAL_SECTION *cs)
{
    pthread_mutex_lock(&cs->mutex);
}

// Leave the critical section
void LeaveCriticalSection(CRITICAL_SECTION *cs)
{
    pthread_mutex_unlock(&cs->mutex);
}

static char *convert_path(const char *path)
{
    char *_path = malloc(256);
    strcpy(_path, path);
    char *end = strstr(_path, "\\*");
    if (end)
        *end = '\0';
    end = strstr(_path, "/*");
    if (end)
        *end = '\0';
    char *backslash;
    while ((backslash = strchr(_path, '\\')) != NULL)
    {
        *backslash = '/';
    }
    return _path;
}

typedef struct
{
    DIR* dir;
    char path[256];
} DIR_PATH;

void *FindFirstFileA(const char *path, WIN32_FIND_DATA *findData)
{
    char *_path = convert_path(path);
    DIR *dir = opendir(_path);

    if (dir != NULL)
    {
        struct dirent *entry = readdir(dir);
        if (entry != NULL)
        {
            strcpy(findData->cFileName, entry->d_name);

            strcat(_path, "/");
            strcat(_path, entry->d_name);
            struct stat statbuf;
            if (stat(_path, &statbuf) == 0)
            {
                if (S_ISDIR(statbuf.st_mode))
                {
                    findData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                }
                else
                {
                    findData->dwFileAttributes = 0;
                }
            }
            free(_path);
            DIR_PATH *dir_path = malloc(sizeof(DIR_PATH));
            dir_path->dir = dir;
            strcpy(dir_path->path, path);
            return dir_path;
        }
        else
        {
            closedir(dir);
        }
    }
    free(_path);
    return INVALID_HANDLE_VALUE;
}

int FindNextFileA(void *hFindFile, WIN32_FIND_DATA *findData)
{
    DIR_PATH *dir_path = (DIR_PATH *)hFindFile;
    DIR *dir = dir_path->dir;
    struct dirent *entry = readdir(dir);
    if (entry != NULL)
    {
        strcpy(findData->cFileName, entry->d_name);

        char *_path = convert_path(dir_path->path);
        strcat(_path, "/");
        strcat(_path, entry->d_name);
        struct stat statbuf;
        if (stat(_path, &statbuf) == 0)
        {
            if (S_ISDIR(statbuf.st_mode))
            {
                findData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            }
            else
            {
                findData->dwFileAttributes = 0;
            }
        }
        free(_path);
        return 1;
    }

    return 0;
}

void FindClose(void *hFindFile)
{
    if (hFindFile != NULL)
    {
        DIR_PATH *dir_path = (DIR_PATH *)hFindFile;
        closedir(dir_path->dir);
        free(dir_path);
    }
}

unsigned long GetFileAttributes(const char *path)
{
    struct stat statbuf;
    char *_path = convert_path(path);
    if (stat(_path, &statbuf) == 0)
    {
        free(_path);
        if (S_ISDIR(statbuf.st_mode))
        {
            return FILE_ATTRIBUTE_DIRECTORY;
        }
        else
        {
            return 0;
        }
    }
    free(_path);
    return INVALID_FILE_ATTRIBUTES;
}
