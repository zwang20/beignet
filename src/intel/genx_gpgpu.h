/* 
 * Copyright © 2012 Intel Corporation
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Benjamin Segovia <benjamin.segovia@intel.com>
 */

#ifndef __GENX_GPGPU_H__
#define __GENX_GPGPU_H__

#include "cl_utils.h"

#include <stdlib.h>
#include <stdint.h>

enum gen6_cache_control {
  cc_gtt        = 0x0,    /* don't use L3, use GTT for LLC */
  cc_mlc        = 0x1,    /* IVB: use L3, use GTT for LLC; SNB: UC */
  cc_llc        = 0x2,
  cc_llc_mlc    = 0x3,
};

#define MAX_SURFACES   255
#define MAX_SAMPLERS   16

/* Use this structure to bind kernels in the gpgpu state */
typedef struct genx_gpgpu_kernel
{
  const char *name;        /* kernel name and bo name */
  uint32_t grf_blocks;     /* register blocks kernel wants (in 8 reg blocks) */
  uint32_t cst_sz;         /* indicates if kernel needs constants */
  const uint32_t *bin;     /* binary code of the kernel */
  int32_t size;            /* kernel code size */
  struct _drm_intel_bo *bo;/* kernel code in the proper addr space */
  int32_t barrierID;       /* barrierID for _this_ kernel */
} genx_gpgpu_kernel_t;

/* Convenient abstraction of the device */
struct intel_driver;

/* Covenient way to talk to the device */
typedef struct genx_gpgpu_state genx_gpgpu_state_t;

/* Buffer object as exposed by drm_intel */
struct _drm_intel_bo;

/* Allocate and initialize a GPGPU state */
extern genx_gpgpu_state_t* genx_gpgpu_state_new(struct intel_driver*);

/* Destroy and deallocate a GPGPU state */
extern void genx_gpgpu_state_delete(genx_gpgpu_state_t*);

/* Set surface descriptor in the current binding table */
extern void gpgpu_bind_surf_2d(genx_gpgpu_state_t*,
                               int32_t index,
                               struct _drm_intel_bo* obj_bo,
                               uint32_t offset,
                               uint32_t format,
                               int32_t w,
                               int32_t h,
                               uint32_t is_dst,
                               int32_t vert_line_stride,
                               int32_t vert_line_stride_offset,
                               uint32_t cchint);

/* Set shared surface descriptor in the current binding table
 * Automatically determines and sets tiling mode which is transparently
 * supported by media block read/write
 */
extern void gpgpu_bind_shared_surf_2d(genx_gpgpu_state_t*,
                                      int32_t index,
                                      struct _drm_intel_bo* obj_bo,
                                      uint32_t offset,
                                      uint32_t format,
                                      int32_t w,
                                      int32_t h,
                                      uint32_t is_dst,
                                      int32_t vert_line_stride,
                                      int32_t vert_line_stride_offset,
                                      uint32_t cchint);

/* Set typeless buffer descriptor in the current binding table */
extern void gpgpu_bind_buf(genx_gpgpu_state_t*,
                           int32_t index,
                           struct _drm_intel_bo* obj_bo,
                           uint32_t offset,
                           uint32_t size,
                           uint32_t cchint);

/* Configure state, size in 512-bit units */
extern void gpgpu_state_init(genx_gpgpu_state_t*,
                             uint32_t max_thr,
                             uint32_t size_vfe_entry,
                             uint32_t num_vfe_entries,
                             uint32_t size_cs_entry,
                             uint32_t num_cs_entries);

/* Set the buffer object where to report performance counters */
extern void gpgpu_set_perf_counters(genx_gpgpu_state_t*, struct _drm_intel_bo *perf);

/* Fills current constant buffer with data */
extern void gpgpu_upload_constants(genx_gpgpu_state_t*, void* data, uint32_t size);

/* Setup all indirect states */
extern void gpgpu_states_setup(genx_gpgpu_state_t*, genx_gpgpu_kernel_t* kernel, uint32_t ker_n);

/* Make HW threads use barrierID */
extern void gpgpu_update_barrier(genx_gpgpu_state_t*, uint32_t barrierID, uint32_t thread_n);

/* Set a sampler (TODO: add other sampler fields) */
extern void gpgpu_set_sampler(genx_gpgpu_state_t*, uint32_t index, uint32_t non_normalized);

/* Allocate the batch buffer and return the BO used for the batch buffer */
extern void gpgpu_batch_reset(genx_gpgpu_state_t*, size_t sz);

/* Atomic begin, pipeline select, urb, pipeline state and constant buffer */
extern void gpgpu_batch_start(genx_gpgpu_state_t*);

/* atomic end with possibly inserted flush */
extern void gpgpu_batch_end(genx_gpgpu_state_t*, int32_t flush_mode);

/* Emit MI_FLUSH */
extern void gpgpu_flush(genx_gpgpu_state_t*);

/* Enqueue a MEDIA object with no inline data */
extern void gpgpu_run(genx_gpgpu_state_t*, int32_t ki);

/* Enqueue a MEDIA object with inline data to push afterward. Returns the
 * pointer where to push. sz is the size of the data we are going to pass
 */
extern char* gpgpu_run_with_inline(genx_gpgpu_state_t*, int32_t ki, size_t sz);

#endif /* __GENX_GPGPU_H__ */

