#include <pthread.h>
#include <dirent.h>

typedef struct {
    pthread_mutex_t mutex;
} CRITICAL_SECTION;

typedef struct {
    char cFileName[260]; // Max path length
    unsigned long dwFileAttributes;
} WIN32_FIND_DATA;

typedef unsigned long DWORD;
typedef void * HANDLE;
typedef unsigned long LONG_PTR;

#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define INVALID_HANDLE_VALUE ((HANDLE) (LONG_PTR)-1)
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFF

void InitializeCriticalSection(CRITICAL_SECTION *cs);
void EnterCriticalSection(CRITICAL_SECTION *cs);
void LeaveCriticalSection(CRITICAL_SECTION *cs);
void *FindFirstFileA(const char *path, WIN32_FIND_DATA *findData);
int FindNextFileA(void *hFindFile, WIN32_FIND_DATA *findData);
void FindClose(void *hFindFile);
unsigned long GetFileAttributes(const char *path);

#define FindFirstFile FindFirstFileA
#define FindNextFile FindNextFileA