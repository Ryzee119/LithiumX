// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _DASH_DATABASE_H
#define _DASH_DATABASE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lithiumx.h"
#include "libs/toml/toml.h"

typedef struct __attribute((packed))
{
    uint32_t dwMagic;                // 0x0000 - magic number [should be "XBEH"]
    uint8_t pbDigitalSignature[256]; // 0x0004 - digital signature
    uint32_t dwBaseAddr;             // 0x0104 - base address
    uint32_t dwSizeofHeaders;        // 0x0108 - size of headers
    uint32_t dwSizeofImage;          // 0x010C - size of image
    uint32_t dwSizeofImageHeader;    // 0x0110 - size of image header
    uint32_t dwTimeDate;             // 0x0114 - timedate stamp
    uint32_t dwCertificateAddr;      // 0x0118 - certificate address
    uint32_t dwSections;             // 0x011C - number of sections
    uint32_t dwSectionHeadersAddr;   // 0x0120 - section headers address

    struct InitFlags // 0x0124 - initialization flags
    {
        uint32_t bMountUtilityDrive : 1;  // mount utility drive flag
        uint32_t bFormatUtilityDrive : 1; // format utility drive flag
        uint32_t bLimit64MB : 1;          // limit development kit run time memory to 64mb flag
        uint32_t bDontSetupHarddisk : 1;  // don't setup hard disk flag
        uint32_t Unused : 4;              // unused (or unknown)
        uint32_t Unused_b1 : 8;           // unused (or unknown)
        uint32_t Unused_b2 : 8;           // unused (or unknown)
        uint32_t Unused_b3 : 8;           // unused (or unknown)
    } dwInitFlags;

    uint32_t dwEntryAddr;                // 0x0128 - entry point address
    uint32_t dwTLSAddr;                  // 0x012C - thread local storage directory address
    uint32_t dwPeStackCommit;            // 0x0130 - size of stack commit
    uint32_t dwPeHeapReserve;            // 0x0134 - size of heap reserve
    uint32_t dwPeHeapCommit;             // 0x0138 - size of heap commit
    uint32_t dwPeBaseAddr;               // 0x013C - original base address
    uint32_t dwPeSizeofImage;            // 0x0140 - size of original image
    uint32_t dwPeChecksum;               // 0x0144 - original checksum
    uint32_t dwPeTimeDate;               // 0x0148 - original timedate stamp
    uint32_t dwDebugPathnameAddr;        // 0x014C - debug pathname address
    uint32_t dwDebugFilenameAddr;        // 0x0150 - debug filename address
    uint32_t dwDebugUnicodeFilenameAddr; // 0x0154 - debug unicode filename address
    uint32_t dwKernelImageThunkAddr;     // 0x0158 - kernel image thunk address
    uint32_t dwNonKernelImportDirAddr;   // 0x015C - non kernel import directory address
    uint32_t dwLibraryVersions;          // 0x0160 - number of library versions
    uint32_t dwLibraryVersionsAddr;      // 0x0164 - library versions address
    uint32_t dwKernelLibraryVersionAddr; // 0x0168 - kernel library version address
    uint32_t dwXAPILibraryVersionAddr;   // 0x016C - xapi library version address
    uint32_t dwLogoBitmapAddr;           // 0x0170 - logo bitmap address
    uint32_t dwSizeofLogoBitmap;         // 0x0174 - logo bitmap size
} xbe_header_t;

typedef struct __attribute((packed))
{
    uint32_t dwSize;                              // 0x0000 - size of certificate
    uint32_t dwTimeDate;                          // 0x0004 - timedate stamp
    uint32_t dwTitleId;                           // 0x0008 - title id
    uint16_t wszTitleName[40];                    // 0x000C - title name (unicode)
    uint32_t dwAlternateTitleId[0x10];            // 0x005C - alternate title ids
    uint32_t dwAllowedMedia;                      // 0x009C - allowed media types
    uint32_t dwGameRegion;                        // 0x00A0 - game region
    uint32_t dwGameRatings;                       // 0x00A4 - game ratings
    uint32_t dwDiskNumber;                        // 0x00A8 - disk number
    uint32_t dwVersion;                           // 0x00AC - version
    uint8_t bzLanKey[16];                         // 0x00B0 - lan key
    uint8_t bzSignatureKey[16];                   // 0x00C0 - signature key
    uint8_t bzTitleAlternateSignatureKey[16][16]; // 0x00D0 - alternate signature keys
} xbe_certificate_t;

#define MAX_COMMAND_LEN 65535
#define MAX_META_LEN 64
#define MAX_OVERVIEW_LEN 4096

enum {
        DB_INDEX_ID,
        DB_INDEX_TITLE_ID,
        DB_INDEX_TITLE,
        DB_INDEX_LAUNCH_PATH,
        DB_INDEX_PAGE,
        DB_INDEX_DEVELOPER,
        DB_INDEX_PUBLISHER,
        DB_INDEX_RELEASE_DATE,
        DB_INDEX_OVERVIEW,
        DB_INDEX_LAST_LAUNCH,
        DB_INDEX_RATING,
        DB_INDEX_MAX,
    };

#define SQL_MAX_COMMAND_LEN 512
#define SQL_TITLE_DB_ID "id"
#define SQL_TITLE_TITLE_ID "title_id"
#define SQL_TITLE_NAME "title"
#define SQL_TITLE_LAUNCH_PATH "launch_path"
#define SQL_TITLE_PAGE "page"
#define SQL_TITLE_DEVELOPER "developer"
#define SQL_TITLE_PUBLISHER "publisher"
#define SQL_TITLE_RELEASE_DATE "release_date"
#define SQL_TITLE_OVERVIEW "overview"
#define SQL_TITLE_LAST_LAUNCH "last_launch"
#define SQL_TITLE_RATING "rating"

#define SQL_SETTINGS_FAHRENHEIT "settings_use_fahrenheit"
#define SQL_SETTINGS_AUTOLAUNCH_DVD "settings_auto_launch_dvd"
#define SQL_SETTINGS_DEFAULT_PAGE_INDEX "settings_default_page_index"
#define SQL_SETTINGS_THEME_COLOR "settings_theme_colour"
#define SQL_SETTINGS_EARLIEST_RECENT_DATE "settings_earliest_recent_date"
#define SQL_SETTINGS_PAGE_SORTS "settings_page_sorts_str"
#define SQL_SETTINGS_MAX_RECENT "settings_max_recent"

#define SQL_TITLES_NAME "xbox_titles"
#define SQL_SETTINGS_NAME "settings"
#define SQL_FLUSH "COMMIT"

#define SQL_TITLE_DELETE_TABLE \
    "DROP TABLE IF EXISTS " SQL_TITLES_NAME

#define SQL_TITLE_DELETE_ENTRIES \
    "DELETE FROM " SQL_TITLES_NAME

#define SQL_TITLE_CHECK_TABLE \
    "SELECT 1 FROM sqlite_master WHERE type='table' AND name=\"" SQL_TITLES_NAME "\""

#define SQL_TITLE_CHECK_TABLE_COLUMNS \
    "PRAGMA table_info(" SQL_TITLES_NAME ")"

#define SQL_TITLE_COUNT \
    "SELECT COUNT(*) FROM " SQL_TITLES_NAME

#define SQL_TITLE_GET_BY_ID \
    "SELECT * FROM " SQL_TITLES_NAME " WHERE " SQL_TITLE_DB_ID " = %d"

#define SQL_TITLE_SET_LAST_LAUNCH_DATETIME \
    "UPDATE " SQL_TITLES_NAME " SET " SQL_TITLE_LAST_LAUNCH " = \"%s\" WHERE " SQL_TITLE_DB_ID " = %d"

#define SQL_TITLE_GET_RECENT \
    "SELECT %s FROM " SQL_TITLES_NAME " WHERE " SQL_TITLE_LAST_LAUNCH " != \"0\"" \
    " AND " SQL_TITLE_LAST_LAUNCH " > \"%s\" ORDER BY " SQL_TITLE_LAST_LAUNCH " DESC LIMIT %d"

#define SQL_TITLE_GET_SORTED_LIST \
    "SELECT %s FROM "SQL_TITLES_NAME" WHERE "SQL_TITLE_PAGE" = \"%s\" ORDER BY %s COLLATE NOCASE %s"

#define SQL_TITLE_GET_LAUNCH_PATH \
    "SELECT  "SQL_TITLE_LAUNCH_PATH " FROM " SQL_TITLES_NAME " WHERE " SQL_TITLE_DB_ID " = %d"

#define SQL_TITLE_CREATE_TABLE                             \
    "CREATE TABLE IF NOT EXISTS " SQL_TITLES_NAME " ("     \
            SQL_TITLE_DB_ID        " INTEGER PRIMARY KEY," \
            SQL_TITLE_TITLE_ID     " TEXT,"                \
            SQL_TITLE_NAME         " TEXT,"                \
            SQL_TITLE_LAUNCH_PATH  " TEXT,"                \
            SQL_TITLE_PAGE         " TEXT,"                \
            SQL_TITLE_DEVELOPER    " TEXT,"                \
            SQL_TITLE_PUBLISHER    " TEXT,"                \
            SQL_TITLE_RELEASE_DATE " DATE,"                \
            SQL_TITLE_OVERVIEW     " TEXT,"                \
            SQL_TITLE_LAST_LAUNCH  " DATETIME,"            \
            SQL_TITLE_RATING       " FLOAT)"

#define SQL_TITLE_INSERT                \
    "INSERT INTO "SQL_TITLES_NAME  " (" \
            SQL_TITLE_DB_ID        ", " \
            SQL_TITLE_TITLE_ID     ", " \
            SQL_TITLE_NAME         ", " \
            SQL_TITLE_LAUNCH_PATH  ", " \
            SQL_TITLE_PAGE         ", " \
            SQL_TITLE_DEVELOPER    ", " \
            SQL_TITLE_PUBLISHER    ", " \
            SQL_TITLE_RELEASE_DATE ", " \
            SQL_TITLE_OVERVIEW     ", " \
            SQL_TITLE_LAST_LAUNCH  ", " \
            SQL_TITLE_RATING       ") " \
            "VALUES(?,?,?,?,?,?,?,?,?,?,?)"
#define SQL_TITLE_INSERT_FORMAT "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s"
#define SQL_TITLE_INSERT_CNT 11

#define SQL_SETTINGS_DELETE_TABLE \
    "DROP TABLE IF EXISTS "SQL_SETTINGS_NAME

#define SQL_SETTINGS_CHECK_TABLE \
    "SELECT 1 FROM sqlite_master WHERE type='table' AND name='" SQL_SETTINGS_NAME "'"

#define SQL_SETTINGS_CHECK_TABLE_COLUMNS \
    "PRAGMA table_info(" SQL_SETTINGS_NAME ")"

#define SQL_SETTINGS_CREATE_TABLE                           \
    "CREATE TABLE IF NOT EXISTS " SQL_SETTINGS_NAME " ("    \
            SQL_SETTINGS_FAHRENHEIT           " INTEGER, "  \
            SQL_SETTINGS_AUTOLAUNCH_DVD       " INTEGER, "  \
            SQL_SETTINGS_DEFAULT_PAGE_INDEX   " INTEGER, "  \
            SQL_SETTINGS_THEME_COLOR          " INTEGER, "  \
            SQL_SETTINGS_EARLIEST_RECENT_DATE " DATETIME, " \
            SQL_SETTINGS_PAGE_SORTS           " TEXT    , " \
            SQL_SETTINGS_MAX_RECENT           " INTEGER)"

#define SQL_SETTINGS_DELETE_ENTRIES \
    "DELETE FROM " SQL_SETTINGS_NAME

#define SQL_SETTINGS_INSERT_OLD                    \
    "INSERT INTO "SQL_SETTINGS_NAME           " (" \
            SQL_SETTINGS_FAHRENHEIT           ", " \
            SQL_SETTINGS_AUTOLAUNCH_DVD       ", " \
            SQL_SETTINGS_DEFAULT_PAGE_INDEX   ", " \
            SQL_SETTINGS_THEME_COLOR          ", " \
            SQL_SETTINGS_EARLIEST_RECENT_DATE ", " \
            SQL_SETTINGS_PAGE_SORTS           ", " \
            SQL_SETTINGS_MAX_RECENT           ") " \
            "VALUES (%d, %d, %d, %d, \"%s\", \"%s\", %d)"

#define SQL_SETTINGS_INSERT                        \
    "INSERT INTO "SQL_SETTINGS_NAME           " (" \
            SQL_SETTINGS_FAHRENHEIT           ", " \
            SQL_SETTINGS_AUTOLAUNCH_DVD       ", " \
            SQL_SETTINGS_DEFAULT_PAGE_INDEX   ", " \
            SQL_SETTINGS_THEME_COLOR          ", " \
            SQL_SETTINGS_EARLIEST_RECENT_DATE ", " \
            SQL_SETTINGS_PAGE_SORTS           ", " \
            SQL_SETTINGS_MAX_RECENT           ") " \
            "VALUES (?,?,?,?,?,?,?)"
#define SQL_SETTINGS_INSERT_FORMAT "(%s,%s,%s,%s,%s,%s,%s)"
#define SQL_SETTINGS_INSERT_CNT 7

#define SQL_SETTINGS_READ                          \
    "SELECT "                                      \
            SQL_SETTINGS_FAHRENHEIT           ", " \
            SQL_SETTINGS_AUTOLAUNCH_DVD       ", " \
            SQL_SETTINGS_DEFAULT_PAGE_INDEX   ", " \
            SQL_SETTINGS_THEME_COLOR          ", " \
            SQL_SETTINGS_EARLIEST_RECENT_DATE ", " \
            SQL_SETTINGS_PAGE_SORTS           ", " \
            SQL_SETTINGS_MAX_RECENT                \
            " FROM " SQL_SETTINGS_NAME

typedef int (*sqlcmd_callback)(void*,int,char**, char**);

bool db_open();
bool db_close();
bool db_init(char *err_msg, int err_msg_len);
bool db_rebuild(toml_table_t *paths);
void db_command_with_callback(const char *command, sqlcmd_callback callback, void *param);
void db_insert(const char *command, int argc, const char *format, ...);
bool db_xbe_parse(const char *xbe_path, const char *xbe_folder, char *title, char *title_id);
#ifdef __cplusplus
}
#endif

#endif
