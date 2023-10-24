//make -f Makefile.nxdk -j && /d/Games/Emulators/xemu/xemu.exe -device lpc47m157 -serial stdio
#include <xboxkrnl/xboxkrnl.h>
#include <nxdk/format.h>
#include <nxdk/mount.h>
#include <nxdk/path.h>
#include <nxdk/net.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <hal/debug.h>
#include <windows.h>

#include <time.h>
#include <stdlib.h>
#include <lvgl.h>
#include "lithiumx.h"
#include "../platform.h"
#include "lvgl_drivers/lv_port_disp.h"
#include "lvgl_drivers/lv_port_indev.h"

#include "lwip/api.h"
#include "lwip/tcpip.h"
#include "lwip/apps/sntp.h"
#include "lwip/netif.h"
#include "ftpd/ftp.h"
#include "xbox_info.h"

static ULONG platform_cpu_temp, platform_mb_temp, platform_tray_state = 0x70;
static UCHAR platform_temp_unit;

void xbox_sntp_set_time(uint32_t ntp_s)
{
    DbgPrint("GOT TIME\n");
    static const LONGLONG NT_EPOCH_TIME_OFFSET = ((LONGLONG)(369 * 365 + 89) * 24 * 3600);
    LARGE_INTEGER xbox_nt_time, ntp_nt_time;

    KeQuerySystemTime(&xbox_nt_time);
    ntp_nt_time.QuadPart = ((uint64_t)ntp_s + NT_EPOCH_TIME_OFFSET) * 10000000;
    NtSetSystemTime(&ntp_nt_time, NULL);
}

static void ftp_startup(void *param)
{
    DbgPrint("STARTING FTP\n");
    ftp_server();
}

static void sntp_startup(void *param)
{
    DbgPrint("STARTING SNTP\n");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void autolaunch_dvd(void *param)
{
    char targetPath[MAX_PATH];
    while (1)
    {
        Sleep(1000);
        // Check if we have media inserted in the DVD ROM
        HalReadSMCTrayState(&platform_tray_state, NULL);
        xbox_get_temps(&platform_cpu_temp, &platform_mb_temp, &platform_temp_unit);

        if (dash_settings.auto_launch_dvd == false)
        {
            continue;
        }
        if (platform_tray_state == 0x60)
        {
            // Prevent recursive launch by checking the current xbe isnt launched from the DVD itself
            nxGetCurrentXbeNtPath(targetPath);
            static const char *cd_path = "\\Device\\CdRom0";
            if (strncmp(targetPath, cd_path, strlen(cd_path)) == 0)
            {
                continue;
            }

            // Prep to launch
            lvgl_getlock();
            strcpy(dash_launch_path, "__DVD__");
            lv_set_quit(LV_QUIT_OTHER);
            lvgl_removelock();
        }
    }
}

static void network_startup(void *param)
{
    DbgPrint("STARTING NETWORK\n");
    nxNetInit(NULL);
    sys_thread_new("ftp_startup", ftp_startup, NULL, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
    //SNTP should be started in TCPIP thread
    tcpip_callback(sntp_startup, NULL);
}

void platform_init(int *w, int *h)
{
    // First try 720p. This is the preferred resolution
    *w = 1280;
    *h = 720;
    if (XVideoSetMode(*w, *h, LV_COLOR_DEPTH, REFRESH_DEFAULT) == false)
    {
        // Fall back to 640*480
        *w = 640;
        *h = 480;
        if (XVideoSetMode(*w, *h, LV_COLOR_DEPTH, REFRESH_DEFAULT) == false)
        {
            // Try whatever else the xbox is happy with
            VIDEO_MODE xmode;
            void *p = NULL;
            while (XVideoListModes(&xmode, 0, 0, &p))
            {
                if (xmode.width == 1080) continue;
                if (xmode.width == 720) continue; // 720x480 doesnt work on pbkit for some reason
                XVideoSetMode(xmode.width, xmode.height, xmode.bpp, xmode.refresh);;
                break;
            }
            
            *w = xmode.width;
            *h = xmode.height;
        }
    }

    // Show a loading screen as early as possible so we indicate some sign of life
    debugClearScreen();
    const char *loading_str = "Mounting Partitions";
    debugMoveCursor(*w / 2 - ((strlen(loading_str) / 2) * 8), *h / 2);
    debugPrint("%s ", loading_str);

    // nxdk automounts D to the root xbe path. Lets undo that
    if (nxIsDriveMounted('D'))
    {
        nxUnmountDrive('D');
    }

    // Mount the DVD drive
    nxMountDrive('D', "\\Device\\CdRom0");

    // Mount root of LithiumX xbe to Q:
    char targetPath[MAX_PATH];
    nxGetCurrentXbeNtPath(targetPath);
    *(strrchr(targetPath, '\\') + 1) = '\0';
    nxMountDrive('Q', targetPath);
    debugPrint(".");

    // Mount stock partitions
    nxMountDrive('C', "\\Device\\Harddisk0\\Partition2\\");
    nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
    nxMountDrive('X', "\\Device\\Harddisk0\\Partition3\\");
    nxMountDrive('Y', "\\Device\\Harddisk0\\Partition4\\");
    nxMountDrive('Z', "\\Device\\Harddisk0\\Partition5\\");
    debugPrint(".");

    // Mount extended partitions
    // NOTE: Both the retail kernel and modified kernels will mount these partitions
    // if they exist and silently fail if they don't. So we can just try to mount them
    // and not worry about checking if they exist.
    nxMountDrive('F', "\\Device\\Harddisk0\\Partition6\\");
    nxMountDrive('G', "\\Device\\Harddisk0\\Partition7\\");
    nxMountDrive('R', "\\Device\\Harddisk0\\Partition8\\");
    nxMountDrive('S', "\\Device\\Harddisk0\\Partition9\\");
    nxMountDrive('V', "\\Device\\Harddisk0\\Partition10\\");
    nxMountDrive('W', "\\Device\\Harddisk0\\Partition11\\");
    nxMountDrive('A', "\\Device\\Harddisk0\\Partition12\\");
    nxMountDrive('B', "\\Device\\Harddisk0\\Partition13\\");
    nxMountDrive('P', "\\Device\\Harddisk0\\Partition14\\");
    debugPrint(".");

    CreateDirectoryA("E:\\UDATA", NULL);
    CreateDirectoryA("E:\\UDATA\\LithiumX", NULL);
    FILE *fp = fopen("E:\\UDATA\\LithiumX\\TitleMeta.xbx", "wb");
    if (fp)
    {
        fprintf(fp, "TitleName=LithiumX Dashboard\r\n");
        fclose(fp);
    }
    debugPrint(".");

    sys_thread_new("XBOX_DVD_AUTOLAUNCH", autolaunch_dvd, NULL, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
    sys_thread_new("XBOX_NETWORK", network_startup, NULL, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
}

void nvnetdrv_stop_txrx (void);
int XboxGetFullLaunchPath(const char *input, char *output);
void usbh_core_deinit();
void platform_launch_iso(const char *path);

void platform_quit(lv_quit_event_t event)
{
    nvnetdrv_stop_txrx();
    usbh_core_deinit();
    debugClearScreen();

    if (event == LV_REBOOT)
    {
        printf("LV_REBOOT\n");
        HalReturnToFirmware(HalRebootRoutine);
    }
    else if (event == LV_SHUTDOWN)
    {
        printf("SHUTDOWN\n");
        HalInitiateShutdown();
    }
    else if (event == LV_QUIT_OTHER)
    {
        if (strcmp(dash_launch_path, "__MSDASH__") == 0)
        {
            // FIXME: Do we need to eject disk?
            strcpy(dash_launch_path, "C:\\xboxdash.xbe");
        }
        else if (strcmp(dash_launch_path, "__DVD__") == 0)
        {
            strcpy(dash_launch_path, "\\Device\\CdRom0\\default.xbe");
        }
        if (dash_launcher_is_iso(dash_launch_path, NULL, NULL))
        {
            platform_launch_iso(dash_launch_path);
        }
        else
        {
            char xbox_launch_path[MAX_PATH];
            XboxGetFullLaunchPath(dash_launch_path, xbox_launch_path);

            DbgPrint("Launching %s\n", dash_launch_path);
            DbgPrint("Launching %s\n", xbox_launch_path);
            XLaunchXBE(xbox_launch_path);
            DbgPrint("Error launching. Reboot\n");
            Sleep(500);
            DbgPrint("ERROR: Could not launch %s\n", dash_launch_path);
            HalReturnToFirmware(HalRebootRoutine);
        }
    }
}

void info_update_callback(lv_timer_t *timer)
{
    static CHAR info_text[1024];
    lv_obj_t *window = timer->user_data;
    lv_obj_t *label = lv_obj_get_child(window, 0);
    lv_label_set_recolor(label, true);

    const CHAR *encoder;
    static UCHAR mac_address[0x06], serial_number[0x0D];
    static ULONG video_region, game_region;
    ULONG type, encoder_check;

    static int clock_calc = 0;
    static ULONG cpu_speed, gpu_speed;
    if (clock_calc ^= 1)
    {
        static ULONGLONG f_rdtsc = 0;
        static DWORD f_ticks = 0;
        cpu_speed = xbox_get_cpu_frequency(&f_rdtsc, &f_ticks) / 1000;
        gpu_speed = xbox_get_gpu_frequency();
    }

    static bool first = 1;
    if (first)
    {
        first = 0;
        ExQueryNonVolatileSetting(XC_FACTORY_SERIAL_NUMBER, &type, &serial_number, sizeof(serial_number), NULL);
        ExQueryNonVolatileSetting(XC_FACTORY_ETHERNET_ADDR, &type, &mac_address, sizeof(mac_address), NULL);
        ExQueryNonVolatileSetting(XC_FACTORY_AV_REGION, &type, &video_region, sizeof(video_region), NULL);
        ExQueryNonVolatileSetting(XC_FACTORY_GAME_REGION, &type, &game_region, sizeof(game_region), NULL);
    }

    encoder = get_encoder_str();

    lv_snprintf(info_text, sizeof(info_text),
                "%s Date/Time:# %s\n"
                "%s IP:# %s\n"
                "%s Tray State:# %s\n"
                "%s RAM:# %s MB\n"
                "%s CPU:# %lu%c, %s MB:# %lu%c\n"
                "%s CPU Freq:# %luMHz, %s GPU Freq:# %luMHz\n"
                "%s Hardware Version:# %s\n"
                "%s Serial Number:# %s\n"
                "%s Mac Address :# %02x:%02x:%02x:%02x:%02x:%02x\n"
                "%s Encoder:# %s\n"
                "%s Kernel:# %u.%u.%u.%u\n"
                "%s Video Region:# %s\n"
                "%s Game Region:# %s\n"
                "%s Build Commit:# %s\n",
                DASH_MENU_COLOR, xbox_get_date_time(),
                DASH_MENU_COLOR, xbox_get_ip_address(),
                DASH_MENU_COLOR, tray_state_str(platform_tray_state),
                DASH_MENU_COLOR, xbox_get_ram_usage(),
                DASH_MENU_COLOR, platform_cpu_temp, platform_temp_unit, DASH_MENU_COLOR, platform_mb_temp, platform_temp_unit,
                DASH_MENU_COLOR, cpu_speed, DASH_MENU_COLOR, gpu_speed,
                DASH_MENU_COLOR, xbox_get_verion(),
                DASH_MENU_COLOR, serial_number,
                DASH_MENU_COLOR, mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5],
                DASH_MENU_COLOR, encoder,
                DASH_MENU_COLOR, XboxKrnlVersion.Major, XboxKrnlVersion.Minor, XboxKrnlVersion.Build, XboxKrnlVersion.Qfe,
                DASH_MENU_COLOR, video_region_str(video_region),
                DASH_MENU_COLOR, game_region_str(game_region),
                DASH_MENU_COLOR, BUILD_VERSION);
    lv_label_set_text_static(label, info_text);
}

static void window_closed(lv_event_t *event)
{
    lv_obj_t *window = lv_event_get_target(event);
    lv_timer_del(lv_event_get_user_data(event));
}

void platform_system_info(lv_obj_t *window)
{
    lv_timer_t *timer = lv_timer_create(info_update_callback, 1000, window);
    lv_obj_t *label = lv_label_create(window);
    lv_obj_add_event_cb(window, window_closed, LV_EVENT_DELETE, timer);
    lv_timer_ready(timer);
}

void platform_flush_cache()
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
            DbgPrint("ERROR: Could not format %s\n", partitions[i]);
        }
        else
        {
            DbgPrint("TRACE: Formatted %s ok!\n", partitions[i]);
        }
    }
}

// YYYY-MM-DD HH:MM:SS
void platform_get_iso8601_time(char time_str[20])
{
    static SYSTEMTIME st = {.wHour = 255};
    static DWORD tick_cnt = 0;

    DWORD change = KeTickCount - tick_cnt;
    if (change > 1000)
    {
        st.wSecond += change / 1000;
        tick_cnt = KeTickCount - (change % 1000);
        while (st.wSecond > 59)
        {
            st.wMinute++;
            st.wSecond -= 60;
        }
        while (st.wMinute > 59)
        {
            st.wHour++;
            st.wMinute -= 60;
        }
    }
    if (st.wHour > 23)
    {
        // This is quite slow (reads from EEPROM etc) so we only read once then
        // work out time based on tick count. Will read from EEPROM once first time or at midnight
        GetLocalTime(&st);
        tick_cnt = KeTickCount;
    }

    lv_snprintf(time_str, 20, "%04d-%02d-%02d %02d:%02d:%02d",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

/*
 * Copyright (C) 2014, Galois, Inc.
 * This sotware is distributed under a standard, three-clause BSD license.
 * Please see the file LICENSE, distributed with this software, for specific
 * terms and conditions.
 */

#define isdigit(c) (c >= '0' && c <= '9')

double atof(const char *s)
{
    // This function stolen from either Rolf Neugebauer or Andrew Tolmach.
    // Probably Rolf.
    double a = 0.0;
    int e = 0;
    int c;
    while ((c = *s++) != '\0' && isdigit(c))
    {
        a = a * 10.0 + (c - '0');
    }
    if (c == '.')
    {
        while ((c = *s++) != '\0' && isdigit(c))
        {
            a = a * 10.0 + (c - '0');
            e = e - 1;
        }
    }
    if (c == 'e' || c == 'E')
    {
        int sign = 1;
        int i = 0;
        c = *s++;
        if (c == '+')
            c = *s++;
        else if (c == '-')
        {
            c = *s++;
            sign = -1;
        }
        while (isdigit(c))
        {
            i = i * 10 + (c - '0');
            c = *s++;
        }
        e += i * sign;
    }
    while (e > 0)
    {
        a *= 10.0;
        e--;
    }
    while (e < 0)
    {
        a *= 0.1;
        e++;
    }
    return a;
}

size_t strnlen(const char *s, size_t count)
{
    const char *sc;

    for (sc = s; count-- && *sc != '\0'; ++sc)
        /* nothing */;
    return sc - s;
}

int strcasecmp(const char *s1, const char *s2)
{
  int result;

  while (1) {
    result = tolower(*s1) - tolower(*s2);
    if (result != 0 || *s1 == '\0')
      break;

    ++s1;
    ++s2;
  }

  return result;
}