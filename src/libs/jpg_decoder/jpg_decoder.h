// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef LV_JPGDECODER_H
#define LV_JPGDECODER_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef JPEG_DECODER_QUEUE_SIZE
#define JPEG_DECODER_QUEUE_SIZE 64
#endif

//jpg Decompression compelte cb. Buffer must be freed with free() when complete.
typedef void (*jpg_complete_cb_t)(void *img, void *mem, int w, int h, void *user_data);

/**
 * @brief Initialise the jpeg_decoder library. Must be called before use.
 * @param colour_depth 16 or 32 for RGB565 or RGBA8888 output.
 * @param max_dimension The maximum width or height Of the output image. The aspect ratio is maintained.
 * This will downscale an image, but it will not upscale.
 * @return void
 */
void jpeg_decoder_init(int colour_depth, int max_dimension);

/**
 * @brief Deinitialise the jpeg_decoder library
 */
void jpeg_decoder_deinit();

/**
 * @brief Queue a jpeg file for asynchronous decompression
 * @param fn The filename of the jpeg file.
 * @param complete_cb The callback function which is called when decompression is complete. Note this is called from thread context.
 * @param user_data A user defined variable that is returned with the complete_cb.
 * @return A handle for the jpeg job, or NULL on error.
 */
void *jpeg_decoder_queue(const char *fn, jpg_complete_cb_t complete_cb, void *user_data);

/**
 * @brief Abort a previously queued decompression job.
 * @param handle The handle returned by jpeg_decoder_queue(). If the job is finished, this has no effect.
 */
void jpeg_decoder_abort(void *handle);

/**
 * @brief Free a jpeg given to user with the callback
 * 
 */
void jpeg_decoder_free(void *jpeg);

#ifdef __cplusplus
}
#endif

#endif /* LV_JPGTURBO_H */
