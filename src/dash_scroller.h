// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _TITLELIST_H
#define _TITLELIST_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
#include "dash.h"
#include <stdio.h>
#include "xml/src/xml.h"
#include <stdint.h>

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

typedef struct title
{
    xbe_header_t xbe_header;
    xbe_certificate_t xbe_cert;
    lv_obj_t *image_container;
    lv_obj_t *thumb_default;
    lv_obj_t *thumb_jpeg;
    void *jpeg_handle;
    char title_str[64];                      // Clean title
    char title_folder[DASH_MAX_PATHLEN]; // String of the folder containing the executable
    char title_exe[DASH_MAX_PATHLEN];    // String of the name of the launch executable
    bool has_thumbnail;
    bool has_xml;
    struct xml_document *xbe_xml; // Struct of the above XML data
} title_t;

void titlelist_init(void);
void titlelist_deinit(void);
int titlelist_add(title_t *xbe, char *title_folder, const char *title_exe, lv_obj_t *parent);
void titlelist_remove(title_t *title);
void titlelist_abort_offscreen(void);
struct xml_string *title_get_synopsis(struct xml_document *xml, const char *node_name);
void xbe_use_default_thumbail(title_t *xbe);

#ifdef __cplusplus
}
#endif

#endif
