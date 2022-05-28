// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "helpers/nano_debug.h"
#include "platform/platform.h"

#include "lvgl.h"
#include "lv_port_indev.h"
#include "dash.h"
#include "dash_styles.h"

#include <xboxkrnl/xboxkrnl.h>
#include <nxdk/format.h>
#include <nxdk/mount.h>
#include <nxdk/path.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <hal/debug.h>
#include <windows.h>

static const char *video_region_str(uint32_t code);
static char *game_region_str(uint32_t code);
static const char *xbox_get_verion();
static const char *tray_state_str(uint32_t tray_state);

extern int jpg_cache_size;
extern int lv_texture_cache_size;

//FIXME: Not currently in nxdk
int strcasecmp (const char *s1, const char *s2)
{
  const unsigned char *p1 = (const unsigned char *) s1;
  const unsigned char *p2 = (const unsigned char *) s2;
  int result;

  if (p1 == p2)
    return 0;

  while ((result = tolower(*p1) - tolower (*p2++)) == 0)
    if (*p1++ == '\0')
      break;

  return result;
}

void platform_init(int *w, int *h)
{
    //First try 720p. This is the preferred resolution
    *w = 1280;
    *h = 720;
    if (XVideoSetMode(*w, *h, LV_COLOR_DEPTH, REFRESH_DEFAULT) == false)
    {
        // Fall back to 640*480
        *w = 640;
        *h = 480;
        if (XVideoSetMode(*w, *h, LV_COLOR_DEPTH, REFRESH_DEFAULT) == false)
        {
            //Try whatever else the xbox is happy with
            VIDEO_MODE xmode;
            void *p = NULL;
            while (XVideoListModes(&xmode, 0, 0, &p));
            XVideoSetMode(xmode.width, xmode.height, xmode.bpp, xmode.refresh);
            *w = xmode.width;
            *h = xmode.height;
        }
    }
    debugPrint("Loading...");

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

    MM_STATISTICS MemoryStatistics;
    MemoryStatistics.Length = sizeof(MM_STATISTICS);
    MmQueryStatistics(&MemoryStatistics);
    uint32_t mem_size = (MemoryStatistics.TotalPhysicalPages * PAGE_SIZE);
    lv_texture_cache_size = mem_size / 4;
    jpg_cache_size = mem_size / 8;
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
        dash_set_launch_exe("%s", "DVDROM");
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

    //Read current RAM and RAM usage
    uint32_t mem_size, mem_used;
    MM_STATISTICS MemoryStatistics;
    MemoryStatistics.Length = sizeof(MM_STATISTICS);
    MmQueryStatistics(&MemoryStatistics);
    mem_size = (MemoryStatistics.TotalPhysicalPages * PAGE_SIZE) / 1024U / 1024U;
    mem_used = mem_size - ((MemoryStatistics.AvailablePages * PAGE_SIZE) / 1024U / 1024U);

    // Try read temps from ADM temperature monitor
    NTSTATUS cpu = HalReadSMBusValue(0x98, 0x01, FALSE, (ULONG *)&cpu_temp);
    NTSTATUS mb = HalReadSMBusValue(0x98, 0x00, FALSE, (ULONG *)&mb_temp);
    if (cpu != STATUS_SUCCESS  || mb != STATUS_SUCCESS)
    {
        // If it fails, its probably a 1.6. Read SMC instead
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

    char ip[20];
    platform_network_get_ip(ip, sizeof(ip));

    lv_snprintf(rt_text, sizeof(rt_text),
                "%s Tray State:# %s\n"
                "%s CPU:# %lu%c, %s MB:# %lu%c\n"
                "%s RAM:# %d/%d MB\n"
                "%s IP:# %s",
                DASH_MENU_COLOR, tray_state_str(tray_state),
                DASH_MENU_COLOR, cpu_temp, temp_unit, DASH_MENU_COLOR, mb_temp, temp_unit,
                DASH_MENU_COLOR, mem_used, mem_size,
                DASH_MENU_COLOR, ip);

    return rt_text;
}

// Info shown in the 'System Information' screen.
const char *platform_show_info_cb(void)
{
    static char info_text[512];
    uint8_t mac_address[0x06];
    uint32_t video_region, game_region, encoder_check;
    ULONG type;
    char serial_number[0x0D];
    const char *encoder;

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

    lv_snprintf(info_text, sizeof(info_text),
                "%s Hardware Version:# %s\n"
                "%s Serial Number:# %s\n"
                "%s Mac Address :# %02x:%02x:%02x:%02x:%02x:%02x\n"
                "%s Encoder:# %s\n"
                "%s Kernel:# %u.%u.%u.%u\n"
                "%s Video Region:# %s\n"
                "%s Game Region:# %s\n"
                "%s Build Commit:# %s\n",
                DASH_MENU_COLOR, xbox_get_verion(),
                DASH_MENU_COLOR, serial_number,
                DASH_MENU_COLOR, mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5],
                DASH_MENU_COLOR, encoder,
                DASH_MENU_COLOR, XboxKrnlVersion.Major, XboxKrnlVersion.Minor, XboxKrnlVersion.Build, XboxKrnlVersion.Qfe,
                DASH_MENU_COLOR, video_region_str(video_region),
                DASH_MENU_COLOR, game_region_str(game_region),
                DASH_MENU_COLOR, BUILD_VERSION);
    return info_text;
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
        const char *path = dash_get_launch_exe();
        if (strcmp(path, "MSDASH") == 0)
        {
            // FIXME: Do we need to eject disk?
            lv_snprintf(launch_path, DASH_MAX_PATHLEN, "C:\\xboxdash.xbe");
        }
        else if (strcmp(path, "DVDROM") == 0)
        {
            lv_snprintf(launch_path, DASH_MAX_PATHLEN, "\\Device\\CdRom0\\default.xbe");
        }
        else
        {
            // Drop the first two characters from path as they are lvgl specific
            lv_snprintf(launch_path, DASH_MAX_PATHLEN, "%s", &path[2]);
        }
        nano_debug(LEVEL_TRACE, "TRACE: Launching %s\n", launch_path);
        debugPrint("Launching\n");
        Sleep(500);
        debugClearScreen();
        XLaunchXBE(launch_path);
        // If we get here, XLaunchXBE didnt work. Reboot instead.
        debugPrint("Error launching. Reboot\n");
        Sleep(500);
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
