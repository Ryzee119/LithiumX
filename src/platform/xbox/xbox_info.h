#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <hal/xbox.h>
#include <lvgl.h>
#include "lithiumx.h"

static const char *video_region_str(unsigned int code)
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

static char *game_region_str(unsigned int code)
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

    HalWriteSMBusValue(0x20, 0x01, FALSE, 0); // reset rev pointer
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
    else if (strcmp(ver, ("P11")) == 0)
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

static const char *tray_state_str(ULONG tray_state)
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

static const char *get_encoder_str()
{
    ULONG encoder_check;
    static const char *encoder_str = NULL;

    // Return previously calculated string
    if (encoder_str != NULL)
    {
        return encoder_str;
    }

    if (HalReadSMBusValue(0xd4, 0x00, FALSE, &encoder_check) == 0)
    {
        encoder_str = "Focus FS454";
    }
    else if (HalReadSMBusValue(0xe0, 0x00, FALSE, &encoder_check) == 0)
    {
        encoder_str = "Microsoft Xcalibur";
    }
    else if (HalReadSMBusValue(0x8a, 0x00, FALSE, &encoder_check) == 0)
    {
        encoder_str = "Conexant CX25871";
    }
    else
    {
        encoder_str = "Unknown";
    }

    return encoder_str;
}

static const char *xbox_get_date_time()
{
    static char datetime[20];
    platform_get_iso8601_time(datetime);
    return datetime;
}

static const char *xbox_get_ram_usage()
{
    static char ram_usage[8];
    ULONG mem_size, mem_used;
    MM_STATISTICS MemoryStatistics;
    MemoryStatistics.Length = sizeof(MM_STATISTICS);
    MmQueryStatistics(&MemoryStatistics);
    mem_size = (MemoryStatistics.TotalPhysicalPages * PAGE_SIZE) / 1024U / 1024U;
    mem_used = mem_size - ((MemoryStatistics.AvailablePages * PAGE_SIZE) / 1024U / 1024U);

    lv_snprintf(ram_usage, sizeof(ram_usage), "%lu/%lu", mem_used, mem_size);
    return ram_usage;
}

static void xbox_get_temps(PULONG cpu, PULONG mb, UCHAR *unit)
{
    NTSTATUS scpu = HalReadSMBusValue(0x98, 0x01, FALSE, cpu);
    NTSTATUS smb = HalReadSMBusValue(0x98, 0x00, FALSE, mb);
    if (scpu != STATUS_SUCCESS  || smb != STATUS_SUCCESS)
    {
        // If it fails, its probably a 1.6. Read SMC instead
        HalReadSMBusValue(0x20, 0x09, FALSE, cpu);
        HalReadSMBusValue(0x20, 0x0A, FALSE, mb);
    }

    if (dash_settings.use_fahrenheit)
    {
        *cpu = (ULONG)(((float)*cpu * 1.8f) + 32);
        *mb = (ULONG)(((float)*mb * 1.8f) + 32);
        *unit = 'F';
    }
    else
    {
        *unit = 'C';
    }
}

static ULONG xbox_get_cpu_frequency(ULONGLONG *f_rdtsc, DWORD *f_ticks)
{
    ULONGLONG s_rdtsc;
    DWORD s_ticks;

    s_rdtsc = __rdtsc();
    s_ticks = KeTickCount;

    s_rdtsc -= *f_rdtsc;
    s_rdtsc /= s_ticks - *f_ticks;

    *f_rdtsc = __rdtsc();
    *f_ticks = KeTickCount;
    return (ULONG)s_rdtsc;
}

static ULONG xbox_get_gpu_frequency()
{
    ULONG nvclk_reg, current_nvclk;
    nvclk_reg = *((volatile ULONG *)0xFD680500);
    #define BASE_CLOCK_INT 16667
    current_nvclk = BASE_CLOCK_INT * ((nvclk_reg & 0xFF00) >> 8);
    current_nvclk /= 1 << ((nvclk_reg & 0x70000) >> 16);
    current_nvclk /= nvclk_reg & 0xFF;
    current_nvclk /= 1000;

    return current_nvclk;
}

static const char *xbox_get_ip_address()
{
    const char *no_ip = "0.0.0.0";
    static char ip[32];
    struct netif* netif = netif_default;
    if (netif == NULL || !netif_is_up(netif ) || !netif_is_link_up(netif))
    {
        return no_ip;
    }
    ip4addr_ntoa_r(netif_ip4_addr(netif), ip, sizeof(ip));
    return ip;
}
