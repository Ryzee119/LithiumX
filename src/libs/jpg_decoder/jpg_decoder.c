// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

/* Uses jpegturbo to decompress a jpeg file. Decompression is run in a different thread to minimise blocking.
 * jpegs are queued with jpeg_decoder_queue(). A callback is made when the compression is complete.
 * Up to JPEG_DECODER_QUEUE_SIZE
 * files can be queued.
 * SDL2 is used for portable thread, mutex and atomic support
 */

#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <jpeglib.h>
#include "jpg_decoder.h"
#include "lvgl.h"

typedef enum
{
    STATE_FREE,          // Mempool item free and ready to use
    STATE_DECOMP_QUEUED, // Mempool item currently queued
    STATE_DECOMP_ABORTED // Mempool item had started decompression, but was aborted
} jpeg_image_state_t;

typedef struct jpeg
{
    char fn[256];       // Stores the filename for this jpeg
    SDL_atomic_t state; // jpeg_image_state_t. Track state of jpeg decompression
    void *user_data;    // User data to be returned on complete_cb;
    uint8_t *mem;
    uint8_t *decompressed_image;
    jpg_complete_cb_t complete_cb; // Callback for jpeg decompression complete. Warning: Called from decomp thread context.
    struct jpeg *next;             // Singley linked list for decompression queue
} jpeg_t;

static int jpeg_decoder_running = 0;
static int jpeg_colour_depth;                      // What colour depth should the decompress jpeg be (16 (RGB565) or 32 (BGRA))
static int jpeg_max_dimension;                     // The maximum output dimension of the width or height (whichever is larger)
static SDL_mutex *jpegdecomp_qmutex;               // Mutex for the jpeg decompressor thread queue
static SDL_sem *jpegdecomp_queue;                  // Semaphore to track nubmer of items in decompressor queue
static SDL_Thread *jpegdecomp_thread;              // Thread for the jpeg decompressor
static jpeg_t jpeg_mpool[JPEG_DECODER_QUEUE_SIZE]; // Local mempool for jpeg objects
static jpeg_t *jpeg_mpool_free;                    // Stores a free pointer in mempool that can be used to quickly allocate from pool
static jpeg_t *jpegdecomp_qhead;                   // Tracks a singly linked list of queued jpegs for decompression in thread
static jpeg_t *jpegdecomp_qtail;                   // Tracks a singly linked list of queued jpegs for decompression in thread

static void error_exit_stub(j_common_ptr cinfo)
{
    (void)cinfo;
}

static int decomp_thread(void *ptr)
{
    FILE *jfile;
    jpeg_t *jpeg;
    struct jpeg_decompress_struct jinfo;
    struct jpeg_error_mgr jerr;
    JSAMPARRAY line_buffer;
    int row_stride;
    void *old_line_buffer;

    while (1)
    {
        // Wait for a jpeg item to be in the decomp queue.
        SDL_SemWait(jpegdecomp_queue);

        if (jpeg_decoder_running == 0)
        {
            return 0;
        }
        jpeg = jpegdecomp_qhead;

        jfile = fopen(jpeg->fn, "rb");
        if (jfile == NULL)
        {
            printf("Could not open %s\n", jpeg->fn);
            goto leave_error;
        }

        jpeg_create_decompress(&jinfo);
        jinfo.err = jpeg_std_error(&jerr);
        jinfo.err->error_exit = error_exit_stub;

        jpeg_stdio_src(&jinfo, jfile);
        if (jpeg_read_header(&jinfo, TRUE) != JPEG_HEADER_OK)
        {
            printf("Invalid jpeg file at %s\n", jpeg->fn);
            jpeg_destroy_decompress(&jinfo);
            fclose(jfile);
            goto leave_error;
        }
        jinfo.out_color_space = (jpeg_colour_depth == 16) ? JCS_RGB565 : JCS_EXT_BGRA;

        int max_size;
        jinfo.scale_num = 9;
        jinfo.scale_denom = 8;
        do
        {
            jinfo.scale_num--;
            jpeg_calc_output_dimensions(&jinfo);
            max_size = (jinfo.output_width < jinfo.output_height) ? jinfo.output_height : jinfo.output_width;
            if (jinfo.scale_num == 1)
                break;
        } while (max_size > jpeg_max_dimension);
        jinfo.do_fancy_upsampling = false;
        jinfo.do_block_smoothing = false;
        jinfo.two_pass_quantize = false;
        jinfo.dct_method = JDCT_FASTEST;
        jinfo.dither_mode = JDITHER_NONE;
        jpeg_start_decompress(&jinfo);
        jinfo.output_components = jpeg_colour_depth / 8;
        row_stride = jinfo.output_width * jinfo.output_components;
        line_buffer = (*jinfo.mem->alloc_sarray)((j_common_ptr)&jinfo, JPOOL_IMAGE, row_stride, 1);

        old_line_buffer = line_buffer[0]; // Save the original allocation

        jpeg->mem = malloc(jinfo.output_width * jinfo.output_height * (jpeg_colour_depth / 8) + 16);
        //Get a 16 byte aligned pointer to return to the user
        jpeg->decompressed_image = (uint8_t *)(((intptr_t)jpeg->mem + 16) & ~0x0F);

        while (jinfo.output_scanline < jinfo.output_height)
        {
            jpeg_image_state_t state = SDL_AtomicGet(&jpeg->state);
            if (state == STATE_DECOMP_ABORTED)
            {
                free(jpeg->mem);
                jpeg->decompressed_image = NULL;
            }

            if (jpeg->decompressed_image == NULL)
            {
                break;
            }

            // Save a memcpy and put our buffer directly into JSAMPARRAY
            line_buffer[0] = (void *)&jpeg->decompressed_image[jinfo.output_scanline * row_stride];
            jpeg_read_scanlines(&jinfo, line_buffer, 1);

            if (jinfo.output_scanline % 100 == 0)
            {
                SDL_Delay(20);
            }
        }

        line_buffer[0] = old_line_buffer; // Restore original allocation so it gets cleared
        jpeg_destroy_decompress(&jinfo);
        fclose(jfile);

        jpeg->complete_cb(jpeg->decompressed_image, jpeg->mem, jinfo.output_width, jinfo.output_height, jpeg->user_data);

    leave_error:
        // We have finished with the object, remove it from the queue.
        SDL_LockMutex(jpegdecomp_qmutex);
        if (jpegdecomp_qhead != NULL)
        {
            jpegdecomp_qhead = jpegdecomp_qhead->next;
        }
        SDL_AtomicSet(&jpeg->state, STATE_FREE);
        SDL_UnlockMutex(jpegdecomp_qmutex);
    }
    return 0;
}

void jpeg_decoder_init(int colour_depth, int max_dimension)
{
    assert(colour_depth == 16 || colour_depth == 32);

    if (jpeg_decoder_running == 1)
    {
        return;
    }
    jpeg_decoder_running = 1;

    memset(jpeg_mpool, 0, sizeof(jpeg_mpool));
    jpeg_colour_depth = colour_depth;
    jpeg_max_dimension = max_dimension;
    jpegdecomp_qhead = NULL;
    jpegdecomp_qtail = NULL;
    jpeg_mpool_free = &jpeg_mpool[0];
    jpegdecomp_qmutex = SDL_CreateMutex();
    jpegdecomp_queue = SDL_CreateSemaphore(0);
    jpegdecomp_thread = SDL_CreateThread(decomp_thread, "jpegdecomp_thread", (void *)NULL);

    assert(jpegdecomp_qmutex != NULL);
    assert(jpegdecomp_thread != NULL);
    assert(jpegdecomp_queue != NULL);
}

void jpeg_decoder_deinit()
{
    int thread_status;
    jpeg_decoder_running = 0;      // This will make decomp thread quit on next run
    SDL_SemPost(jpegdecomp_queue); // Force thread run.
    SDL_WaitThread(jpegdecomp_thread, &thread_status);
    SDL_DestroyMutex(jpegdecomp_qmutex);
    SDL_DestroySemaphore(jpegdecomp_queue);
}

void *jpeg_decoder_queue(const char *fn, jpg_complete_cb_t complete_cb, void *user_data)
{

    jpeg_t *jpeg = NULL;

    // Allocate a object from local mempool
    SDL_LockMutex(jpegdecomp_qmutex);
    if (jpeg_mpool_free != NULL)
    {
        // Quick fetch if possible
        jpeg = jpeg_mpool_free;
        jpeg_mpool_free = NULL;
    }
    else
    {
        for (int i = 0; i < JPEG_DECODER_QUEUE_SIZE; i++)
        {
            jpeg_image_state_t state = SDL_AtomicGet(&jpeg_mpool[i].state);
            if (state == STATE_FREE)
            {
                jpeg = &jpeg_mpool[i];
                break;
            }
        }
    }
    SDL_UnlockMutex(jpegdecomp_qmutex);

    if (jpeg == NULL)
    {
        return NULL;
    }

    strncpy(jpeg->fn, fn, sizeof(jpeg->fn) - 1);
    jpeg->user_data = user_data;
    jpeg->complete_cb = complete_cb;
    SDL_AtomicSet(&jpeg->state, STATE_DECOMP_QUEUED);

    SDL_LockMutex(jpegdecomp_qmutex);
    if (jpegdecomp_qhead == NULL)
    {
        jpegdecomp_qhead = jpeg;
        jpegdecomp_qtail = jpegdecomp_qhead;
        jpegdecomp_qtail->next = NULL;
    }
    else
    {
        jpegdecomp_qtail->next = jpeg;
        jpegdecomp_qtail = jpeg;
        jpegdecomp_qtail->next = NULL;
    }
    SDL_UnlockMutex(jpegdecomp_qmutex);
    SDL_SemPost(jpegdecomp_queue);

    return jpeg;
}

void jpeg_decoder_abort(void *handle)
{
    jpeg_t *jpeg = (jpeg_t *)handle;
    SDL_AtomicCAS(&jpeg->state, STATE_DECOMP_QUEUED, STATE_DECOMP_ABORTED);
}
