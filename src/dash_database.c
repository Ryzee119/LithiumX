// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lithiumx.h"

static sqlite3 *db = NULL;
static SDL_mutex *db_mutex;
static int item_index;

static const char *no_meta = "No Meta-Data";
static const char *no_id = "00000000";

static void parse_folder(const char *page_title, const char *folderPath, const char *filename_to_find);
static bool parse_xml(const char *xml_path, char *title, char *title_id, char *developer,
                      char *publisher, char *release_date, float *rating, char *overview);

int db_rebuild_scanned_items;

void db_command_with_callback(const char *command, sqlcmd_callback callback, void *param)
{
    dash_printf(LEVEL_TRACE, "Processing SQL command %s\n", command);
    SDL_LockMutex(db_mutex);
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, command, callback, param, &err_msg);
    if (rc != SQLITE_OK)
    {
        dash_printf(LEVEL_ERROR, "SQL ERROR: %s\n", err_msg);
        sqlite3_free(err_msg);
        assert(rc == SQLITE_OK);
        SDL_UnlockMutex(db_mutex);
        return;
    }
    SDL_UnlockMutex(db_mutex);
}

void db_insert(const char *command, int argc, const char *format, ...)
{
    dash_printf(LEVEL_TRACE, "Processing SQL insert command %s\n", command);
    int rc;

    SDL_LockMutex(db_mutex);

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, command, -1, &stmt, NULL);
    assert(rc == SQLITE_OK);

    va_list args;
    va_start(args, format);
    for (int i = 0; i < argc; i++)
    {
        char *arg = va_arg(args, char *);
        assert(arg != NULL);
        rc = sqlite3_bind_text(stmt, i + 1, arg, -1, SQLITE_STATIC);
        dash_printf(LEVEL_TRACE, "Bind Text to index %d: \"%s\"\n", i + 1, arg);
        if (rc != SQLITE_OK)
        {
            dash_printf(LEVEL_ERROR, "SQL ERROR: %s\n", sqlite3_errmsg(db));
        }
        assert(rc == SQLITE_OK);
    }
    va_end(args);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        dash_printf(LEVEL_ERROR, "SQL ERROR: %s\n", sqlite3_errmsg(db));
    }
    assert(rc == SQLITE_DONE);
    rc = sqlite3_finalize(stmt);
    if (rc != SQLITE_OK)
    {
        dash_printf(LEVEL_ERROR, "SQL ERROR: %s\n", sqlite3_errmsg(db));
    }
    assert(rc == SQLITE_OK);
    SDL_UnlockMutex(db_mutex);
}

bool db_open()
{
    db_mutex = SDL_CreateMutex();
    sqlite3_initialize();
    int rc = sqlite3_open(DASH_DATABASE_PATH, &db);
    if (rc != 0)
    {
        dash_printf(LEVEL_ERROR, "SQL WARN: Could not opened %s."
                                 "Database has been opened in memory only\n", DASH_DATABASE_PATH);
        rc = sqlite3_open_v2(":memory:", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
        assert(rc == 0);
        return false;
    }
    return true;
}

bool db_close()
{
    db_command_with_callback(SQL_FLUSH, NULL, NULL);
    SDL_DestroyMutex(db_mutex);
    sqlite3_close(db);
    return true;
}

bool db_init(char *err_msg, int err_msg_len)
{
    sqlite3_stmt *stmt;
    int rc, index;
    bool need_game_rebuild = false;

    // Check we have a table called xbox_titles
    rc = sqlite3_prepare_v2(db, SQL_TITLE_CHECK_TABLE, -1, &stmt, NULL);
    assert(rc == SQLITE_OK);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_ROW)
    {
        lv_snprintf(err_msg, err_msg_len, "Database is not initialised or invalid. Database Rebuilt.");
        need_game_rebuild = true;
        dash_printf(LEVEL_WARN, "Database didn't have a table called \"%s\". It will be rebuilt\n", SQL_TITLES_NAME);
    }

    if (need_game_rebuild == false)
    {
        // Check that the xbox_titles table has the correct columns
        static const char *game_columns[] = {
            SQL_TITLE_DB_ID, SQL_TITLE_TITLE_ID, SQL_TITLE_NAME, SQL_TITLE_LAUNCH_PATH, SQL_TITLE_PAGE,
            SQL_TITLE_DEVELOPER, SQL_TITLE_PUBLISHER, SQL_TITLE_RELEASE_DATE,
            SQL_TITLE_OVERVIEW, SQL_TITLE_LAST_LAUNCH, SQL_TITLE_RATING};

        rc = sqlite3_prepare_v2(db, SQL_TITLE_CHECK_TABLE_COLUMNS, -1, &stmt, NULL);
        assert(rc == SQLITE_OK);
        index = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *columnName = (const char *)sqlite3_column_text(stmt, 1);
            if (strcmp(columnName, game_columns[index++]) != 0)
            {
                index = 0;
                break;
            }
        }
        sqlite3_finalize(stmt);
        if (index != DASH_ARRAY_SIZE(game_columns))
        {
            lv_snprintf(err_msg, err_msg_len, "Games title table invalid. Database Rebuilt.");
            rc = sqlite3_exec(db, SQL_TITLE_DELETE_TABLE, 0, 0, NULL);
            assert(rc == SQLITE_OK);
            need_game_rebuild = true;
            dash_printf(LEVEL_WARN, "Database table \"%s\" was missing or had an incorrect column. It will be rebuilt\n", SQL_TITLES_NAME);
        }
    }

    if (need_game_rebuild == false)
    {
        // Check the database has something in it. If it's empty it may not be an error, but may aswell
        // trigger a database re-scan/rebuild
        rc = sqlite3_prepare_v2(db, SQL_TITLE_COUNT, -1, &stmt, NULL);
        assert(rc == SQLITE_OK);
        rc = sqlite3_step(stmt);
        assert(rc == SQLITE_ROW);
        int count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        if (count == 0)
        {
            need_game_rebuild = true;
            dash_printf(LEVEL_TRACE, "Database table \"%s\" was empty. It will trigger a rescan.\n", SQL_TITLES_NAME);
        }
    }

    // Check that the settings table has the correct columns
    static const char *settings_columns[] = {
        SQL_SETTINGS_FAHRENHEIT, SQL_SETTINGS_AUTOLAUNCH_DVD, SQL_SETTINGS_DEFAULT_PAGE_INDEX,
        SQL_SETTINGS_THEME_COLOR, SQL_SETTINGS_EARLIEST_RECENT_DATE, SQL_SETTINGS_PAGE_SORTS, SQL_SETTINGS_MAX_RECENT};

    rc = sqlite3_prepare_v2(db, SQL_SETTINGS_CHECK_TABLE_COLUMNS, -1, &stmt, NULL);
    assert(rc == SQLITE_OK);
    index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *columnName = (const char *)sqlite3_column_text(stmt, 1);
        if (strcmp(columnName, settings_columns[index++]) != 0)
        {
            index = 0;
            break;
        }
    }
    sqlite3_finalize(stmt);
    if (index != DASH_ARRAY_SIZE(settings_columns))
    {
        lv_snprintf(err_msg, err_msg_len, "Settings invalid. Reset to default.");
        dash_printf(LEVEL_WARN, "Database table \"%s\" was invalid. Settings reset to default\n", SQL_SETTINGS_NAME);
        rc = sqlite3_exec(db, SQL_SETTINGS_DELETE_TABLE, 0, 0, NULL);
        assert(rc == SQLITE_OK);
        rc = sqlite3_exec(db, SQL_SETTINGS_CREATE_TABLE, NULL, 0, NULL);
        assert(rc == SQLITE_OK);
    }

    return !need_game_rebuild;
}

bool db_rebuild(toml_table_t *paths)
{
    toml_array_t *pages = toml_array_in(paths, "pages");
    int num_pages = pages ? (LV_MIN(toml_array_nelem(pages), DASH_MAX_PAGES)) : 0;
    int rc;

    assert(db);
    item_index = 0;
    db_rebuild_scanned_items = 0;

    // Create the tables if they dont exists
    rc = sqlite3_exec(db, SQL_TITLE_CREATE_TABLE, NULL, 0, NULL);
    assert(rc == SQLITE_OK);
    if (rc != SQLITE_OK)
    {
        return false;
    }

    // Scan through every page from the toml file
    for (int page = 0; page < num_pages; page++)
    {
        // Get the name of this page
        toml_datum_t name_str = toml_string_in(toml_table_at(pages, page), "name");
        assert(name_str.ok);

        // Get the search paths associated with this page
        toml_array_t *paths = toml_array_in(toml_table_at(pages, page), "paths");
        int num_paths = (paths) ? toml_array_nelem(paths) : 0;
        if (num_paths > DASH_MAX_PATHS_PER_PAGE)
        {
            num_paths = DASH_MAX_PATHS_PER_PAGE;
        }

        // Scan through every path associated with this page
        for (int path = 0; path < num_paths; path++)
        {
            toml_datum_t path_str = toml_string_at(paths, path);
            if (path_str.ok == 0)
            {
                continue;
            }

            // Check folder for folders containing "default.xbe"
            parse_folder(name_str.u.s, path_str.u.s, "default.xbe");
        }
    }
    return true;
}

static bool get_xml_str(char *xml, sxmltok_t *tokens, int num_tokens, char *key, char *buf, int buf_len)
{
    char str[32];
    for (int i = 0; i < num_tokens; i++)
    {
        if (tokens[i].type == SXML_STARTTAG)
        {
            int len = tokens[i].endpos - tokens[i].startpos;
            str[len] = '\0';
            strncpy(str, &xml[tokens[i].startpos], len);
            if (strcmp(str, key) == 0)
            {
                if (i + 1 >= num_tokens)
                {
                    break;
                }

                sxmltok_t *t = &tokens[i + 1];
                int len1 = t->endpos - t->startpos;
                if (t->type != SXML_CHARACTER)
                {
                    break;
                }

                int capped_len = DASH_MIN(buf_len, len1);
                strncpy(buf, &xml[t->startpos], capped_len);
                buf[capped_len] = '\0';
                return true;
            }
        }
    }
    return false;
}

// The dates in the xbmc xml format are dd MMM YYYY, We want it to be YYYY-MM-DD
static void convert_xml_date_to_iso8601(const char* input, char output[11]) {
    int day, year;
    char month[4];

    sscanf(input, "%d %3s %d", &day, month, &year);

    static const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    int monthNumber = 0;
    for (int i = 0; i < 12; i++) {
        if (strcmp(month, months[i]) == 0) {
            monthNumber = i + 1;
            break;
        }
    }
    lv_snprintf(output, 11, "%04d-%02d-%02d", year, monthNumber, day);
}

static bool parse_xml(const char *xml_path, char *title, char *title_id, char *developer,
                      char *publisher, char *release_date, float *rating, char *overview)
{
    sxml_t parser;
    sxmlerr_t err;
    sxmltok_t tokens[128];

    FILE *fp = fopen(xml_path, "rb");
    if (fp == NULL)
    {
        return false;
    }

    fseek(fp, 0L, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *xml_buf = lv_mem_alloc(sz + 1);
    if (fread(xml_buf, 1, sz, fp) != sz)
    {
        lv_mem_free(xml_buf);
        fclose(fp);
        return false;
    }
    fclose(fp);

    sxml_init(&parser);
    err = sxml_parse(&parser, xml_buf, sz, tokens, 128);
    if (err != SXML_SUCCESS)
    {
        lv_mem_free(xml_buf);
        return false;
    }
    char rating_str[12];
    get_xml_str(xml_buf, tokens, parser.ntokens, "title", title, MAX_META_LEN);
    get_xml_str(xml_buf, tokens, parser.ntokens, "developer", developer, MAX_META_LEN);
    get_xml_str(xml_buf, tokens, parser.ntokens, "publisher", publisher, MAX_META_LEN);
    get_xml_str(xml_buf, tokens, parser.ntokens, "release_date", release_date, MAX_META_LEN);
    get_xml_str(xml_buf, tokens, parser.ntokens, "titleid", title_id, MAX_META_LEN);
    get_xml_str(xml_buf, tokens, parser.ntokens, "overview", overview, MAX_OVERVIEW_LEN);
    get_xml_str(xml_buf, tokens, parser.ntokens, "rating", rating_str, sizeof(rating_str));
    *rating = atof(rating_str);
    lv_mem_free(xml_buf);
    char iso8601_date[11];
    convert_xml_date_to_iso8601(release_date, iso8601_date);
    strcpy(release_date, iso8601_date);

    return true;
}

static void clean_path(char *path)
{
    char *ptr = strchr(path, '/');
    while (ptr != NULL)
    {
        *ptr = '\\';
        ptr = strchr(ptr + 1, '/');
    }
}

static void parse_folder(const char *page_title, const char *folderPath, const char *filename_to_find)
{
    // static ok as non-reentrant - minimise stack usage
    static char searchPath[DASH_MAX_PATH];
    static char xmlPath[DASH_MAX_PATH];
    static char filePath[DASH_MAX_PATH];
    WIN32_FIND_DATA findData;
    HANDLE hFind;

    // Create a search path
    lv_snprintf(searchPath, sizeof(searchPath), "%s\\*", folderPath);
    clean_path(searchPath);

    // Find the first file/folder. Leave if folder is empty
    hFind = FindFirstFile(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return;
    }

    lvgl_getlock();
    char *title = lv_mem_alloc(MAX_META_LEN);
    char *developer = lv_mem_alloc(MAX_META_LEN);
    char *publisher = lv_mem_alloc(MAX_META_LEN);
    char *release_date = lv_mem_alloc(MAX_META_LEN);
    char *title_id = lv_mem_alloc(MAX_META_LEN);
    char *overview = lv_mem_alloc(MAX_OVERVIEW_LEN);
    float rating;
    lvgl_removelock();

    do
    {
        // Skip "." and ".." directories
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
            continue;

        // Ignore non-directories
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            continue;

        // Build the full path to the specific file we are looking for
        lv_snprintf(filePath, sizeof(filePath), "%s\\%s\\%s", folderPath, findData.cFileName, filename_to_find);
        clean_path(filePath);

        // Check if the file exists and its not a directory
        DWORD fileAttributes = GetFileAttributes(filePath);
        if (fileAttributes == INVALID_FILE_ATTRIBUTES || (fileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;

        // Check if an xml meta-data file is present by first building the path to it then parsing it
        lv_snprintf(xmlPath, sizeof(xmlPath), "%s\\%s\\_resources\\default.xml",
                    folderPath, findData.cFileName);
        clean_path(xmlPath);

        title[0] = '\0';
        developer[0] = '\0';
        publisher[0] = '\0';
        release_date[0] = '\0';
        title_id[0] = '\0';
        overview[0] = '\0';
        rating = 0.0f;

        lvgl_getlock();
        if (parse_xml(xmlPath, title, title_id, developer, publisher, release_date, &rating, overview) == false)
        {
            // Check xbe is valid and extract title string
            db_xbe_parse(filePath, findData.cFileName, title, title_id);
        }
        lvgl_removelock();

        if (title[0] == '\0')
            continue;
        if (developer[0] == '\0')
            strcpy(developer, no_meta);
        if (publisher[0] == '\0')
            strcpy(publisher, no_meta);
        if (release_date[0] == '\0')
            strcpy(release_date, "2000-01-01");
        if (title_id[0] == '\0')
            strcpy(title_id, no_id);
        if (overview[0] == '\0')
            strcpy(overview, no_meta);

        
        // Insert it into the database
        char item_index_str[8];
        char rating_str[8];
        lv_snprintf(item_index_str, sizeof(item_index_str), "%d", item_index++);
        lv_snprintf(rating_str, sizeof(rating_str), "%1.1f", rating);

        db_insert(SQL_TITLE_INSERT, SQL_TITLE_INSERT_CNT, SQL_TITLE_INSERT_FORMAT,
            item_index_str,
            title_id,
            title,
            filePath,
            page_title,
            developer,
            publisher,
            release_date,
            overview,
            "0", // Late played date - "0" = never launch
            rating_str);

        db_rebuild_scanned_items++;
    } while (FindNextFile(hFind, &findData));

    lvgl_getlock();
    lv_mem_free(title);
    lv_mem_free(developer);
    lv_mem_free(publisher);
    lv_mem_free(release_date);
    lv_mem_free(overview);
    lv_mem_free(title_id);
    lvgl_removelock();
    FindClose(hFind);
}

bool db_xbe_parse(const char *xbe_path, const char *xbe_folder, char *title, char *title_id)
{
    static xbe_header_t xbe_header;
    static xbe_certificate_t xbe_cert;
    FILE *fp = fopen(xbe_path, "rb");

    if (fp == NULL)
    {
        return false;
    }

    if (fread(&xbe_header, 1, sizeof(xbe_header_t), fp) != sizeof(xbe_header_t))
    {
        dash_printf(LEVEL_WARN, "Could not read header from %s", xbe_path);
        fclose(fp);
        return false;
    }

    if (strncmp((char *)&xbe_header.dwMagic, "XBEH", 4) != 0)
    {
        dash_printf(LEVEL_WARN, "Xbe %s magic header values invalid.", xbe_path);
        fclose(fp);
        return false;
    }

    uint32_t cert_addr = xbe_header.dwCertificateAddr - xbe_header.dwBaseAddr;
    if (fseek(fp, cert_addr, SEEK_SET) != 0)
    {
        dash_printf(LEVEL_WARN, "Xbe %s invalid certificate address. Could not seek to addr %08x\n", xbe_path, cert_addr);
        fclose(fp);
        return false;
    }

    if (fread(&xbe_cert, 1, sizeof(xbe_certificate_t), fp) != sizeof(xbe_certificate_t))
    {
        dash_printf(LEVEL_WARN, "Could not read certificate from %s. Invalid return length\n", xbe_path);
        fclose(fp);
        return false;
    }
    fclose(fp);

    int max_len = sizeof(xbe_cert.wszTitleName) / sizeof(uint16_t);
    for (int i = 0; i < max_len; i++)
    {
        if (xbe_cert.wszTitleName[i] == 0x0000)
        {
            max_len = i;
            if (max_len < 2)
            {
                max_len = strlen(xbe_folder);
                break;
            }
        }
    }
    title[max_len] = '\0';
    for (int i = 0; i < max_len; i++)
    {
        uint16_t unicode = xbe_cert.wszTitleName[i];
        // Replace non ascii with ' '
        title[i] = (unicode > 0x7E) ? ' ' : (unicode & 0x7F);
        if (unicode == 0x000)
        {
            break;
        }
    }

    // If the xbe doesnt seem to have a title, fall back to the folder name
    if (strlen(title) < 2)
    {
        strncpy(title, xbe_folder, max_len);
        dash_printf(LEVEL_TRACE, "Extracted title from XBE %s. Title \"%s\"\n", xbe_path, title);
    }

    // Sometimes the string has encoding issues. Fix the ones I know about.
    char *_str = strstr(title, "&amp;");
    if (_str)
    {
        _str++;
        // Turn "Hello &amp; World" into "Hello & World"
        strcpy(_str, _str + strlen("&amp;") - 1);
    }

    lv_snprintf(title_id, MAX_META_LEN, "%08x", xbe_cert.dwTitleId);
    return true;
}
