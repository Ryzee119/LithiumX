/*
 * texture swizzling routines
 *
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2013 espes
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_XBOX_SWIZZLE_H
#define HW_XBOX_SWIZZLE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>

void swizzle_generate_masks(
  unsigned int width, unsigned int height, unsigned int depth,
  uint32_t* mask_x, uint32_t* mask_y, uint32_t* mask_z
);

uint32_t swizzle_fill_pattern(uint32_t pattern, uint32_t value);

void swizzle_box(
  const uint8_t *src_buf,
  unsigned int width,
  unsigned int height,
  unsigned int depth,
  uint8_t *dst_buf,
  unsigned int row_pitch,
  unsigned int slice_pitch,
  unsigned int bytes_per_pixel
);

void swizzle_box_offset(
  const uint8_t *src_buf,
  unsigned int src_width,
  unsigned int src_height,
  unsigned int src_depth,
  uint8_t *dst_buf,
  unsigned int dst_xofs,
  unsigned int dst_yofs,
  unsigned int dst_zofs,
  unsigned int dst_width,
  unsigned int dst_height,
  unsigned int dst_depth,
  unsigned int row_pitch,
  unsigned int slice_pitch,
  unsigned int bytes_per_pixel
);

void unswizzle_box(
  const uint8_t *src_buf,
  unsigned int width,
  unsigned int height,
  unsigned int depth,
  uint8_t *dst_buf,
  unsigned int row_pitch,
  unsigned int slice_pitch,
  unsigned int bytes_per_pixel
);

void unswizzle_rect(
  const uint8_t *src_buf,
  unsigned int width,
  unsigned int height,
  uint8_t *dst_buf,
  unsigned int pitch,
  unsigned int bytes_per_pixel
);

void swizzle_rect(
  const uint8_t *src_buf,
  unsigned int width,
  unsigned int height,
  uint8_t *dst_buf,
  unsigned int pitch,
  unsigned int bytes_per_pixel
);

void swizzle_rect_offset(
  const uint8_t *src_buf,
  unsigned int src_width,
  unsigned int src_height,
  uint8_t *dst_buf,
  unsigned int dst_xofs,
  unsigned int dst_yofs,
  unsigned int dst_width,
  unsigned int dst_height,
  unsigned int pitch,
  unsigned int bytes_per_pixel
);

#endif
