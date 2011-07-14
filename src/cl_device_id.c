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

#include "cl_platform_id.h"
#include "cl_device_id.h"
#include "cl_internals.h"
#include "cl_utils.h"
#include "cl_defs.h"
#include "intel/cl_device_data.h"
#include "CL/cl.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct _cl_device_id intel_gt2_device = {
  .max_compute_unit = 60,
  .max_work_item_sizes = {512, 512, 512},
  .max_work_group_size = 512,
  .max_clock_frequency = 1350,

  /* Common fields between GT1 and GT2 */
  #include "cl_gen6_device.h"
};

static struct _cl_device_id intel_gt1_device = {
  .max_compute_unit = 24,
  .max_work_item_sizes = {256, 256, 256},
  .max_work_group_size = 256,
  .max_clock_frequency = 1000,

  /* Common fields between GT1 and GT2 */
  #include "cl_gen6_device.h"
};

LOCAL cl_device_id
cl_get_gt_device(void)
{
  cl_device_id ret = NULL;
  int device_id = cl_intel_get_device_id();

  if (device_id == PCI_CHIP_SANDYBRIDGE_GT1   ||
      device_id == PCI_CHIP_SANDYBRIDGE_M_GT1 ||
      device_id == PCI_CHIP_SANDYBRIDGE_S_GT) {
    intel_gt1_device.vendor_id = device_id;
    intel_gt1_device.platform = intel_platform;
    ret = &intel_gt1_device;
  }
  else if (device_id == PCI_CHIP_SANDYBRIDGE_GT2      ||
           device_id == PCI_CHIP_SANDYBRIDGE_M_GT2    ||
           device_id == PCI_CHIP_SANDYBRIDGE_GT2_PLUS ||
           device_id == PCI_CHIP_SANDYBRIDGE_M_GT2_PLUS) {
    intel_gt2_device.vendor_id = device_id;
    intel_gt2_device.platform = intel_platform;
    ret = &intel_gt2_device;
  }
  return ret;
}

LOCAL cl_int
cl_get_device_ids(cl_platform_id    platform,
                  cl_device_type    device_type,
                  cl_uint           num_entries,
                  cl_device_id *    devices,
                  cl_uint *         num_devices)
{
  /* Check parameter consistency */
  if (UNLIKELY(num_entries == 0 && devices == NULL))
    return CL_SUCCESS;
  if (UNLIKELY(devices == NULL))
    return CL_INVALID_VALUE;
  if (UNLIKELY(platform != NULL && platform != intel_platform))
    return CL_INVALID_PLATFORM;
  if (UNLIKELY(device_type == CL_DEVICE_TYPE_CPU))
    return CL_INVALID_DEVICE_TYPE;

  /* Detect our device (reject a non intel one or gen<6) */
  if (UNLIKELY((*devices = cl_get_gt_device()) != NULL)) {
    if (num_devices)
      *num_devices = 1;
    return CL_SUCCESS;
  }
  else {
    if (num_devices)
      *num_devices = 0;
    return CL_DEVICE_NOT_FOUND;
  }
}

#define DECL_FIELD(CASE,FIELD)                                      \
  case JOIN(CL_DEVICE_,CASE):                                       \
      if (param_value_size < sizeof(((cl_device_id)NULL)->FIELD))   \
        return CL_INVALID_VALUE;                                    \
      if (param_value_size_ret != NULL)                             \
        *param_value_size_ret = sizeof(((cl_device_id)NULL)->FIELD);\
      memcpy(param_value,                                           \
             &device->FIELD,                                        \
             sizeof(((cl_device_id)NULL)->FIELD));                  \
        return CL_SUCCESS;

#define DECL_STRING_FIELD(CASE,FIELD)                               \
  case JOIN(CL_DEVICE_,CASE):                                       \
    if (param_value_size < device->JOIN(FIELD,_sz))                 \
      return CL_INVALID_VALUE;                                      \
    if (param_value_size_ret != NULL)                               \
      *param_value_size_ret = device->JOIN(FIELD,_sz);              \
    memcpy(param_value, device->FIELD, device->JOIN(FIELD,_sz));    \
    return CL_SUCCESS;

LOCAL cl_int
cl_get_device_info(cl_device_id     device,
                   cl_device_info   param_name,
                   size_t           param_value_size,
                   void *           param_value,
                   size_t *         param_value_size_ret)
{
  if (UNLIKELY(device != &intel_gt1_device && device != &intel_gt2_device))
    return CL_INVALID_DEVICE;
  if (UNLIKELY(param_value == NULL))
    return CL_INVALID_VALUE;

  /* Find the correct parameter */
  switch (param_name) {
    DECL_FIELD(TYPE, device_type)
    DECL_FIELD(VENDOR_ID, vendor_id)
    DECL_FIELD(MAX_COMPUTE_UNITS, max_compute_unit)
    DECL_FIELD(MAX_WORK_ITEM_DIMENSIONS, max_work_item_dimensions)
    DECL_FIELD(MAX_WORK_ITEM_SIZES, max_work_item_sizes)
    DECL_FIELD(MAX_WORK_GROUP_SIZE, max_work_group_size)
    DECL_FIELD(PREFERRED_VECTOR_WIDTH_CHAR, preferred_vector_width_char)
    DECL_FIELD(PREFERRED_VECTOR_WIDTH_SHORT, preferred_vector_width_short)
    DECL_FIELD(PREFERRED_VECTOR_WIDTH_INT, preferred_vector_width_int)
    DECL_FIELD(PREFERRED_VECTOR_WIDTH_LONG, preferred_vector_width_long)
    DECL_FIELD(PREFERRED_VECTOR_WIDTH_FLOAT, preferred_vector_width_float)
    DECL_FIELD(PREFERRED_VECTOR_WIDTH_DOUBLE, preferred_vector_width_double)
    DECL_FIELD(PREFERRED_VECTOR_WIDTH_HALF, preferred_vector_width_half)
    DECL_FIELD(NATIVE_VECTOR_WIDTH_CHAR, native_vector_width_char)
    DECL_FIELD(NATIVE_VECTOR_WIDTH_SHORT, native_vector_width_short)
    DECL_FIELD(NATIVE_VECTOR_WIDTH_INT, native_vector_width_int)
    DECL_FIELD(NATIVE_VECTOR_WIDTH_LONG, native_vector_width_long)
    DECL_FIELD(NATIVE_VECTOR_WIDTH_FLOAT, native_vector_width_float)
    DECL_FIELD(NATIVE_VECTOR_WIDTH_DOUBLE, native_vector_width_double)
    DECL_FIELD(NATIVE_VECTOR_WIDTH_HALF, native_vector_width_half)
    DECL_FIELD(MAX_CLOCK_FREQUENCY, max_clock_frequency)
    DECL_FIELD(ADDRESS_BITS, address_bits)
    DECL_FIELD(MAX_MEM_ALLOC_SIZE, max_mem_alloc_size)
    DECL_FIELD(IMAGE_SUPPORT, image_support)
    DECL_FIELD(MAX_READ_IMAGE_ARGS, max_read_image_args)
    DECL_FIELD(MAX_WRITE_IMAGE_ARGS, max_write_image_args)
    DECL_FIELD(IMAGE2D_MAX_WIDTH, image2d_max_width)
    DECL_FIELD(IMAGE2D_MAX_HEIGHT, image2d_max_height)
    DECL_FIELD(IMAGE3D_MAX_WIDTH, image3d_max_width)
    DECL_FIELD(IMAGE3D_MAX_HEIGHT, image3d_max_height)
    DECL_FIELD(IMAGE3D_MAX_DEPTH, image3d_max_depth)
    DECL_FIELD(MAX_SAMPLERS, max_samplers)
    DECL_FIELD(MAX_PARAMETER_SIZE, max_parameter_size)
    DECL_FIELD(MEM_BASE_ADDR_ALIGN, mem_base_addr_align)
    DECL_FIELD(MIN_DATA_TYPE_ALIGN_SIZE, min_data_type_align_size)
    DECL_FIELD(SINGLE_FP_CONFIG, single_fp_config)
    DECL_FIELD(GLOBAL_MEM_CACHE_TYPE, global_mem_cache_type)
    DECL_FIELD(GLOBAL_MEM_CACHELINE_SIZE, global_mem_cache_line_size)
    DECL_FIELD(GLOBAL_MEM_CACHE_SIZE, global_mem_cache_size)
    DECL_FIELD(GLOBAL_MEM_SIZE, global_mem_size)
    DECL_FIELD(MAX_CONSTANT_BUFFER_SIZE, max_constant_buffer_size)
    DECL_FIELD(MAX_CONSTANT_ARGS, max_constant_args)
    DECL_FIELD(LOCAL_MEM_TYPE, local_mem_type)
    DECL_FIELD(LOCAL_MEM_SIZE, local_mem_size)
    DECL_FIELD(ERROR_CORRECTION_SUPPORT, error_correction_support)
    DECL_FIELD(HOST_UNIFIED_MEMORY, host_unified_memory)
    DECL_FIELD(PROFILING_TIMER_RESOLUTION, profiling_timer_resolution)
    DECL_FIELD(ENDIAN_LITTLE, endian_little)
    DECL_FIELD(AVAILABLE, available)
    DECL_FIELD(COMPILER_AVAILABLE, compiler_available)
    DECL_FIELD(EXECUTION_CAPABILITIES, execution_capabilities)
    DECL_FIELD(QUEUE_PROPERTIES, queue_properties)
    DECL_FIELD(PLATFORM, platform)
    DECL_STRING_FIELD(NAME, name)
    DECL_STRING_FIELD(VENDOR, vendor)
    DECL_STRING_FIELD(VERSION, version)
    DECL_STRING_FIELD(PROFILE, profile)
    DECL_STRING_FIELD(OPENCL_C_VERSION, opencl_c_version)
    default: return CL_INVALID_VALUE;
  };
}

