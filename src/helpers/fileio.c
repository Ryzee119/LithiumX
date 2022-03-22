// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lvgl.h"

bool lv_fs_exists(const char *path)
{
    lv_fs_file_t fp;
    bool exists = false;
    if (lv_fs_open(&fp, path, LV_FS_MODE_RD) == LV_FS_RES_OK)
    {
        exists = true;
        lv_fs_close(&fp);
    }
    return exists;
}

uint32_t lv_fs_size(lv_fs_file_t *fp)
{
    uint32_t file_size;
    lv_fs_seek(fp, 0U, LV_FS_SEEK_END);
    lv_fs_tell(fp, &file_size);
    lv_fs_seek(fp, 0U, LV_FS_SEEK_SET);
    return file_size;
}

// Open, Read, Close. This allocates memory and must be freed by user when finished.
uint8_t *lv_fs_orc(const char *fn, uint32_t *br)
{
    lv_fs_file_t fp;
    uint32_t file_size;
    uint8_t *data;

    if (lv_fs_open(&fp, fn, LV_FS_MODE_RD) != LV_FS_RES_OK)
    {
        return NULL;
    }

    file_size = lv_fs_size(&fp);

    data = lv_mem_alloc(file_size);
    if (data == NULL)
    {
        lv_fs_close(&fp);
        return NULL;
    }

    if (lv_fs_read(&fp, data, file_size, br) != LV_FS_RES_OK)
    {
        lv_mem_free(data);
        data = NULL;
    }
    lv_fs_close(&fp);
    return data;
}
