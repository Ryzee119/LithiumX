// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lvgl.h"
#include "lv_port_indev.h"
#include "dash.h"
#include "dash_styles.h"
#include "helpers/nano_debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <xboxkrnl/xboxkrnl.h>
#include <nxdk/format.h>
#include <nxdk/mount.h>
#include <nxdk/path.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <windows.h>

static const char *video_region_str(uint32_t code);
static char *game_region_str(uint32_t code);
static const char *xbox_get_verion();
static const char *tray_state_str(uint32_t tray_state);

void platform_init(int *w, int *h)
{
    *w = 1280;
    *h = 720;
    if (XVideoSetMode(*w, *h, LV_COLOR_DEPTH, REFRESH_DEFAULT) == false)
    {
        // Fall back to 480p. This should always be available
        *w = 640;
        *h = 480;
        XVideoSetMode(*w, *h, LV_COLOR_DEPTH, REFRESH_DEFAULT);
    }

    // nxdk automounts D to the root xbe path. Lets undo that
    if (nxIsDriveMounted('D'))
    {
        nxUnmountDrive('D');
    }

    /*Remount root xbe path to A:\\*/
    char targetPath[MAX_PATH];
    nxGetCurrentXbeNtPath(targetPath);
    char *finalSeparator = strrchr(targetPath, '\\');
    *(finalSeparator + 1) = '\0';
    nxMountDrive('A', targetPath);

    nxMountDrive('D', "\\Device\\CdRom0");
    nxMountDrive('C', "\\Device\\Harddisk0\\Partition2\\");
    nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
    nxMountDrive('F', "\\Device\\Harddisk0\\Partition6\\");
    nxMountDrive('G', "\\Device\\Harddisk0\\Partition7\\");
    nxMountDrive('X', "\\Device\\Harddisk0\\Partition3\\");
    nxMountDrive('Y', "\\Device\\Harddisk0\\Partition4\\");
    nxMountDrive('Z', "\\Device\\Harddisk0\\Partition5\\");

    CreateDirectoryA("E:\\UDATA", NULL);
    CreateDirectoryA("E:\\UDATA\\LithiumX", NULL);
    FILE *fp = fopen("E:\\UDATA\\LithiumX\\TitleMeta.xbx", "wb");
    if (fp)
    {
        fprintf(fp, "TitleName=LithiumX Dashboard\r\n");
        fclose(fp);
    }
}

// Xbox specific
void platform_flush_cache_cb()
{
    // Source: https://github.com/dracc/NevolutionX/blob/master/Sources/wipeCache.cpp
    const char *partitions[] = {
        "\\Device\\Harddisk0\\Partition3", // "X"
        "\\Device\\Harddisk0\\Partition4", // "Y"
        "\\Device\\Harddisk0\\Partition5"  // "Z"
    };
    const int partition_cnt = sizeof(partitions) / sizeof(partitions[0]);
    for (int i = 0; i < partition_cnt; i++)
    {
        if (nxFormatVolume(partitions[i], 0) == false)
        {
            nano_debug(LEVEL_ERROR, "ERROR: Could not format %s\n", partitions[i]);
        }
        else
        {
            nano_debug(LEVEL_TRACE, "TRACE: Formatted %s ok!\n", partitions[i]);
        }
    }
}

// Xbox specific
void platform_launch_dvd()
{
    ULONG tray_state = 0x70;
    NTSTATUS status = HalReadSMCTrayState(&tray_state, NULL);

    // Check if media detected
    if (NT_SUCCESS(status) && tray_state == 0x60)
    {
        dash_set_launch_folder("DVDROM");
        lv_set_quit(LV_QUIT_OTHER);
    }
}

// Small text label shown about the main menu. This is called every 2 seconds.
// Useful to show temperatures or other occassionally changing info. Return a string with the text
const char *platform_realtime_info_cb(void)
{
    ULONG tray_state = 0x70, cpu_temp = 0, mb_temp = 0;
    static char rt_text[256];
    char temp_unit = 'C';

    // Try read temps from ADM temperature monitor
    HalReadSMBusValue(0x98, 0x01, FALSE, (ULONG *)&cpu_temp);
    HalReadSMBusValue(0x98, 0x00, FALSE, (ULONG *)&mb_temp);
    if (cpu_temp == 0 || mb_temp == 0)
    {
        // If it fails, its probable a 1.6. Read SMC instead
        HalReadSMBusValue(0x20, 0x09, FALSE, (ULONG *)&cpu_temp);
        HalReadSMBusValue(0x20, 0x0A, FALSE, (ULONG *)&mb_temp);
    }

    if (settings_use_fahrenheit)
    {
        cpu_temp = (ULONG)(((float)cpu_temp * 1.8f) + 32);
        mb_temp = (ULONG)(((float)mb_temp * 1.8f) + 32);
        temp_unit = 'F';
    }

    HalReadSMCTrayState(&tray_state, NULL);

    lv_snprintf(rt_text, sizeof(rt_text), "Tray State: %s, CPU: %lu%c, MB: %lu%c",
                tray_state_str(tray_state),
                cpu_temp, temp_unit,
                mb_temp, temp_unit);

    return rt_text;
}

// Info shown in the 'System Information' screen.
void platform_show_info_cb(lv_obj_t *parent)
{
    lv_obj_t *label;
    uint8_t mac_address[0x06];
    // FIXME: IP ADDRESS
    uint32_t mem_size, mem_used, video_region, game_region, encoder_check;
    ULONG type;
    char serial_number[0x0D];
    const char *encoder;
    MM_STATISTICS MemoryStatistics;
    MemoryStatistics.Length = sizeof(MM_STATISTICS);
    MmQueryStatistics(&MemoryStatistics);
    ExQueryNonVolatileSetting(XC_FACTORY_SERIAL_NUMBER, &type, &serial_number, sizeof(serial_number), NULL);
    ExQueryNonVolatileSetting(XC_FACTORY_ETHERNET_ADDR, &type, &mac_address, sizeof(mac_address), NULL);
    ExQueryNonVolatileSetting(XC_FACTORY_AV_REGION, &type, &video_region, sizeof(video_region), NULL);
    ExQueryNonVolatileSetting(XC_FACTORY_GAME_REGION, &type, &game_region, sizeof(game_region), NULL);

    if (HalReadSMBusValue(0xd4, 0x00, FALSE, (ULONG *)&encoder_check) == 0)
    {
        encoder = "Focus FS454";
    }
    else if (HalReadSMBusValue(0xe0, 0x00, FALSE, (ULONG *)&encoder_check) == 0)
    {
        encoder = "Microsoft Xcalibur";
    }
    else if (HalReadSMBusValue(0x8a, 0x00, FALSE, (ULONG *)&encoder_check) == 0)
    {
        encoder = "Conexant CX25871";
    }
    else
    {
        encoder = "Unknown";
    }

    label = lv_label_create(parent);
    lv_label_set_recolor(label, true);
    lv_label_set_text_fmt(label, "%s Version:# %s", DASH_MENU_COLOR, xbox_get_verion());

    label = lv_label_create(parent);
    lv_label_set_recolor(label, true);
    lv_label_set_text_fmt(label, "%s Serial Number:# %s", DASH_MENU_COLOR, serial_number);

    label = lv_label_create(parent);
    lv_label_set_recolor(label, true);
    lv_label_set_text_fmt(label, "%s Mac Address :# %02x:%02x:%02x:%02x:%02x:%02x", DASH_MENU_COLOR,
                          mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]);

    label = lv_label_create(parent);
    lv_label_set_recolor(label, true);
    lv_label_set_text_fmt(label, "%s Encoder:# %s", DASH_MENU_COLOR, encoder);

    label = lv_label_create(parent);
    lv_label_set_recolor(label, true);
    lv_label_set_text_fmt(label, "%s Kernel:# %u.%u.%u.%u", DASH_MENU_COLOR,
                          XboxKrnlVersion.Major,
                          XboxKrnlVersion.Minor,
                          XboxKrnlVersion.Build,
                          XboxKrnlVersion.Qfe);

    label = lv_label_create(parent);
    lv_label_set_recolor(label, true);
    lv_label_set_text_fmt(label, "%s Video Region:# %s", DASH_MENU_COLOR, video_region_str(video_region));

    label = lv_label_create(parent);
    lv_label_set_recolor(label, true);
    lv_label_set_text_fmt(label, "%s Game Region:# %s", DASH_MENU_COLOR, game_region_str(game_region));

    mem_size = (MemoryStatistics.TotalPhysicalPages * PAGE_SIZE) / 1024U / 1024U;
    mem_used = mem_size - ((MemoryStatistics.AvailablePages * PAGE_SIZE) / 1024U / 1024U);
    label = lv_label_create(parent);
    lv_label_set_recolor(label, true);
    lv_label_set_text_fmt(label, "%s RAM:# %d/%d MB", DASH_MENU_COLOR, mem_used, mem_size);

    label = lv_label_create(parent);
    lv_label_set_recolor(label, true);
    lv_label_set_text_fmt(label, "%s Build Commit:# %s", DASH_MENU_COLOR, BUILD_VERSION);
}

void platform_quit(lv_quit_event_t event)
{
    char launch_path[DASH_MAX_PATHLEN];
    if (event == LV_REBOOT)
    {
        HalReturnToFirmware(HalRebootRoutine);
    }
    else if (event == LV_SHUTDOWN)
    {
        HalInitiateShutdown();
    }
    else if (event == LV_QUIT_OTHER)
    {
        const char *path = dash_get_launch_folder();
        if (strcmp(path, "MSDASH") == 0)
        {
            // FIXME: Do we need to eject disk?
            lv_snprintf(launch_path, DASH_MAX_PATHLEN, "C:\\xboxdash.xbe");
        }
        else if (strcmp(path, "DVDROM") == 0)
        {
            lv_snprintf(launch_path, DASH_MAX_PATHLEN, "D:\\default.xbe");
        }
        else
        {
            // Drop the first two characters from path as they are lvgl specific
            lv_snprintf(launch_path, DASH_MAX_PATHLEN, "%s%c%s", &path[2], DASH_PATH_SEPARATOR, DASH_LAUNCH_EXE);
        }
        nano_debug(LEVEL_TRACE, "TRACE: Launching %s\n", launch_path);
        XLaunchXBE(launch_path);
        // If we get here, XLaunchXBE didnt work. Reboot instead.
        nano_debug(LEVEL_ERROR, "ERROR: Could not launch %s\n", launch_path);
        HalReturnToFirmware(HalRebootRoutine);
    }
}

static const char *video_region_str(uint32_t code)
{
    switch (code)
    {
    case 0x00400100:
        return "NTSC-M";
    case 0x00400200:
        return "NTSC-J";
    case 0x00800300:
        return "PAL-I";
    case 0x00400400:
        return "PAL-M";
    default:
        return "Unknown:";
    }
}

static char *game_region_str(uint32_t code)
{
    static char out[256];
    const char *reg0 = "", *reg1 = "", *reg2 = "", *reg3 = "";
    if (code & 0x00000001)
        reg0 = "North America/";
    if (code & 0x00000002)
        reg1 = "JAP/";
    if (code & 0x00000004)
        reg2 = "EU/AU/";
    if (code & 0x80000000)
        reg3 = "DEBUG/";
    lv_snprintf(out, sizeof(out), "%s%s%s%s", reg0, reg1, reg2, reg3);
    int len = strlen(out);
    out[len - 1] = '\0'; // Knock out last dash
    return out;
}

static const char *xbox_get_verion()
{
    static const char *ver_string = NULL;
    unsigned int encoder_check;
    char ver[6];

    // Return previously calculated version
    if (ver_string != NULL)
    {
        return ver_string;
    }

    HalReadSMBusValue(0x20, 0x01, 0, (ULONG *)&ver[0]);
    HalReadSMBusValue(0x20, 0x01, 0, (ULONG *)&ver[1]);
    HalReadSMBusValue(0x20, 0x01, 0, (ULONG *)&ver[2]);
    ver[3] = 0;
    ver[4] = 0;
    ver[5] = 0;

    if (strchr(ver, 'D') != NULL)
    {
        ver_string = "DEVKIT or DEBUGKIT";
    }
    else if (strcmp(ver, ("B11")) == 0)
    {
        ver_string = "DEBUGKIT Green";
    }
    else if (strcmp(ver, ("P01")) == 0)
    {
        ver_string = "v1.0";
    }
    else if (strcmp(ver, ("P05")) == 0)
    {
        ver_string = "v1.1";
    }
    else if (strcmp(ver, ("P11")) == 0 || strcmp(ver, ("1P1")) == 0 || strcmp(ver, ("11P")) == 0)
    {
        ver_string = (HalReadSMBusValue(0xD4, 0x00, 0, (ULONG *)&encoder_check) == 0) ? "v1.4" : "v1.2/v1.3";
    }
    else if (strcmp(ver, ("P2L")) == 0)
    {
        ver_string = "v1.6";
    }
    else
    {
        ver_string = "Unknown";
    }
    return ver_string;
}

static const char *tray_state_str(uint32_t tray_state)
{
    switch (tray_state & 0x70)
    {
    case 0x00:
        return "Closed";
    case 0x10:
        return "Open";
    case 0x20:
        return "Unloading";
    case 0x30:
        return "Opening";
    case 0x40:
        return "No Media";
    case 0x50:
        return "Closing";
    case 0x60:
        return "Media Detected";
    default:
        return "Unknown State";
    }
}
