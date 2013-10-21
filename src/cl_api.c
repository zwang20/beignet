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
#include "cl_context.h"
#include "cl_command_queue.h"
#include "cl_enqueue.h"
#include "cl_event.h"
#include "cl_program.h"
#include "cl_kernel.h"
#include "cl_mem.h"
#include "cl_image.h"
#include "cl_sampler.h"
#include "cl_alloc.h"
#include "cl_utils.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"
#include "CL/cl_intel.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#ifndef CL_VERSION_1_2
#define CL_MAP_WRITE_INVALIDATE_REGION              (1 << 2)
#define CL_DEVICE_TYPE_CUSTOM                       (1 << 4)
#define CL_MEM_HOST_WRITE_ONLY                      (1 << 7)
#define CL_MEM_HOST_READ_ONLY                       (1 << 8)
#define CL_MEM_HOST_NO_ACCESS                       (1 << 9)
typedef intptr_t cl_device_partition_property;
#endif

#define FILL_GETINFO_RET(TYPE, ELT, VAL, RET) \
	do { \
	  if (param_value && param_value_size < sizeof(TYPE)*ELT) \
	      return CL_INVALID_VALUE;  \
	  if (param_value) { \
	      memcpy(param_value, (VAL), sizeof(TYPE)*ELT); \
	  } \
          \
	  if (param_value_size_ret) \
	      *param_value_size_ret = sizeof(TYPE)*ELT; \
	  return RET; \
	} while(0)

inline cl_int
handle_events(cl_command_queue queue, cl_int num, const cl_event *wait_list,
              cl_event* event, enqueue_data* data, cl_command_type type)
{
  cl_int status = cl_event_wait_events(num, wait_list, queue);
  cl_event e;
  if(event != NULL || status == CL_ENQUEUE_EXECUTE_DEFER) {
    e = cl_event_new(queue->ctx, queue, type, event!=NULL);
    if(event != NULL)
      *event = e;
    if(status == CL_ENQUEUE_EXECUTE_DEFER) {
      cl_event_new_enqueue_callback(e, data, num, wait_list);
    }
  }
  return status;
}

/* The following code checking overlap is from Appendix of openCL spec 1.1 */
inline cl_bool check_copy_overlap(const size_t src_offset[3],
                                  const size_t dst_offset[3],
                                  const size_t region[3],
                                  size_t row_pitch, size_t slice_pitch)
{
  const size_t src_min[] = {src_offset[0], src_offset[1], src_offset[2]};
  const size_t src_max[] = {src_offset[0] + region[0],
                            src_offset[1] + region[1],
                            src_offset[2] + region[2]};
  const size_t dst_min[] = {dst_offset[0], dst_offset[1], dst_offset[2]};
  const size_t dst_max[] = {dst_offset[0] + region[0],
                            dst_offset[1] + region[1],
                            dst_offset[2] + region[2]};
  // Check for overlap
  cl_bool overlap = CL_TRUE;
  unsigned i;
  size_t dst_start = dst_offset[2] * slice_pitch +
                     dst_offset[1] * row_pitch + dst_offset[0];
  size_t dst_end = dst_start + (region[2] * slice_pitch +
                   region[1] * row_pitch + region[0]);
  size_t src_start = src_offset[2] * slice_pitch +
                     src_offset[1] * row_pitch + src_offset[0];
  size_t src_end = src_start + (region[2] * slice_pitch +
                   region[1] * row_pitch + region[0]);

  for (i=0; i != 3; ++i) {
    overlap = overlap && (src_min[i] < dst_max[i])
                      && (src_max[i] > dst_min[i]);
  }

  if (!overlap) {
    size_t delta_src_x = (src_offset[0] + region[0] > row_pitch) ?
                          src_offset[0] + region[0] - row_pitch : 0;
    size_t delta_dst_x = (dst_offset[0] + region[0] > row_pitch) ?
                          dst_offset[0] + region[0] - row_pitch : 0;
    if ( (delta_src_x > 0 && delta_src_x > dst_offset[0]) ||
         (delta_dst_x > 0 && delta_dst_x > src_offset[0]) ) {
      if ( (src_start <= dst_start && dst_start < src_end) ||
           (dst_start <= src_start && src_start < dst_end) )
        overlap = CL_TRUE;
    }
    if (region[2] > 1) {
      size_t src_height = slice_pitch / row_pitch;
      size_t dst_height = slice_pitch / row_pitch;
      size_t delta_src_y = (src_offset[1] + region[1] > src_height) ?
                            src_offset[1] + region[1] - src_height : 0;
      size_t delta_dst_y = (dst_offset[1] + region[1] > dst_height) ?
                            dst_offset[1] + region[1] - dst_height : 0;
      if ( (delta_src_y > 0 && delta_src_y > dst_offset[1]) ||
           (delta_dst_y > 0 && delta_dst_y > src_offset[1]) ) {
        if ( (src_start <= dst_start && dst_start < src_end) ||
             (dst_start <= src_start && src_start < dst_end) )
          overlap = CL_TRUE;
      }
    }
  }
  return overlap;
}

static cl_int
cl_check_device_type(cl_device_type device_type)
{
  const cl_device_type valid =  CL_DEVICE_TYPE_GPU
                              | CL_DEVICE_TYPE_CPU
                              | CL_DEVICE_TYPE_ACCELERATOR
                              | CL_DEVICE_TYPE_DEFAULT
                              | CL_DEVICE_TYPE_CUSTOM;

  if( (device_type & valid) == 0) {
    return CL_INVALID_DEVICE_TYPE;
  }
  if(UNLIKELY(!(device_type & CL_DEVICE_TYPE_DEFAULT) && !(device_type & CL_DEVICE_TYPE_GPU)))
    return CL_DEVICE_NOT_FOUND;

  return CL_SUCCESS;
}

static cl_int
cl_device_id_is_ok(const cl_device_id device)
{
  return device != cl_get_gt_device() ? CL_FALSE : CL_TRUE;
}

cl_int
clGetPlatformIDs(cl_uint          num_entries,
                 cl_platform_id * platforms,
                 cl_uint *        num_platforms)
{
  if(UNLIKELY(platforms == NULL && num_platforms == NULL))
    return CL_INVALID_VALUE;
  if(UNLIKELY(num_entries == 0 && platforms != NULL))
    return CL_INVALID_VALUE;

  return cl_get_platform_ids(num_entries, platforms, num_platforms);
}

cl_int
clGetPlatformInfo(cl_platform_id    platform,
                  cl_platform_info  param_name,
                  size_t            param_value_size,
                  void *            param_value,
                  size_t *          param_value_size_ret)
{
  /* Only one platform. This is easy */
  if (UNLIKELY(platform != NULL && platform != intel_platform))
    return CL_INVALID_PLATFORM;

  return cl_get_platform_info(platform,
                              param_name,
                              param_value_size,
                              param_value,
                              param_value_size_ret);
}

cl_int
clGetDeviceIDs(cl_platform_id platform,
               cl_device_type device_type,
               cl_uint        num_entries,
               cl_device_id * devices,
               cl_uint *      num_devices)
{
  cl_int err = CL_SUCCESS;

  /* Check parameter consistency */
  if (UNLIKELY(devices == NULL && num_devices == NULL))
    return CL_INVALID_VALUE;
  if (UNLIKELY(platform && platform != intel_platform))
    return CL_INVALID_PLATFORM;
  if (UNLIKELY(devices && num_entries == 0))
    return CL_INVALID_VALUE;

  err = cl_check_device_type(device_type);
  if(err != CL_SUCCESS)
    return err;

  return cl_get_device_ids(platform,
                           device_type,
                           num_entries,
                           devices,
                           num_devices);
}

cl_int
clGetDeviceInfo(cl_device_id   device,
                cl_device_info param_name,
                size_t         param_value_size,
                void *         param_value,
                size_t *       param_value_size_ret)
{
  return cl_get_device_info(device,
                            param_name,
                            param_value_size,
                            param_value,
                            param_value_size_ret);
}

cl_int
clCreateSubDevices(cl_device_id                         in_device,
                   const cl_device_partition_property * properties,
                   cl_uint                              num_devices,
                   cl_device_id *                       out_devices,
                   cl_uint *                            num_devices_ret)
{
  NOT_IMPLEMENTED;
  return 0;
}

cl_int
clRetainDevice(cl_device_id device)
{
  // XXX stub for C++ Bindings
  return CL_SUCCESS;
}

cl_int
clReleaseDevice(cl_device_id device)
{
  // XXX stub for C++ Bindings
  return CL_SUCCESS;
}

cl_context
clCreateContext(const cl_context_properties *  properties,
                cl_uint                        num_devices,
                const cl_device_id *           devices,
                void (* pfn_notify) (const char*, const void*, size_t, void*),
                void *                         user_data,
                cl_int *                       errcode_ret)
{
  cl_int err = CL_SUCCESS;
  cl_context context = NULL;

  /* Assert parameters correctness */
  INVALID_VALUE_IF (devices == NULL);
  INVALID_VALUE_IF (num_devices == 0);
  INVALID_VALUE_IF (pfn_notify == NULL && user_data != NULL);

  /* Now check if the user is asking for the right device */
  INVALID_DEVICE_IF (cl_device_id_is_ok(*devices) == CL_FALSE);

  context = cl_create_context(properties,
                           num_devices,
                           devices,
                           pfn_notify,
                           user_data,
                           &err);
error:
  if (errcode_ret)
    *errcode_ret = err;
  return context;
}

cl_context
clCreateContextFromType(const cl_context_properties *  properties,
                        cl_device_type                 device_type,
                        void (CL_CALLBACK *pfn_notify) (const char *, const void *, size_t, void *),
                        void *                         user_data,
                        cl_int *                       errcode_ret)
{
  cl_context context = NULL;
  cl_int err = CL_SUCCESS;
  cl_device_id devices[1];
  cl_uint num_devices = 1;

  INVALID_VALUE_IF (pfn_notify == NULL && user_data != NULL);

  err = cl_check_device_type(device_type);
  if(err != CL_SUCCESS) {
    goto error;
  }

  err = cl_get_device_ids(NULL,
                          device_type,
                          1,
                          &devices[0],
                          &num_devices);
  if (err != CL_SUCCESS) {
    goto error;
  }

  context = cl_create_context(properties,
                              num_devices,
                              devices,
                              pfn_notify,
                              user_data,
                              &err);
error:
  if (errcode_ret)
    *errcode_ret = err;
  return context;
}

cl_int
clRetainContext(cl_context context)
{
  cl_int err = CL_SUCCESS;
  CHECK_CONTEXT (context);
  cl_context_add_ref(context);
error:
  return err;
}

cl_int
clReleaseContext(cl_context context)
{
  cl_int err = CL_SUCCESS;
  CHECK_CONTEXT (context);
  cl_context_delete(context);
error:
  return err;
}

cl_int
clGetContextInfo(cl_context      context,
                 cl_context_info param_name,
                 size_t          param_value_size,
                 void *          param_value,
                 size_t *        param_value_size_ret)
{
  cl_int err = CL_SUCCESS;
  CHECK_CONTEXT (context);

  if (param_name == CL_CONTEXT_DEVICES) {
    FILL_GETINFO_RET (cl_device_id, 1, &context->device, CL_SUCCESS);
  } else if (param_name == CL_CONTEXT_NUM_DEVICES) {
    cl_uint n = 1;
    FILL_GETINFO_RET (cl_uint, 1, &n, CL_SUCCESS);
  } else if (param_name == CL_CONTEXT_REFERENCE_COUNT) {
    cl_uint ref = context->ref_n;
    FILL_GETINFO_RET (cl_uint, 1, &ref, CL_SUCCESS);
  } else if (param_name == CL_CONTEXT_PROPERTIES) {
    if(context->prop_len > 0) {
      FILL_GETINFO_RET (cl_context_properties, context->prop_len, context->prop_user, CL_SUCCESS);
    } else {
      cl_context_properties n = 0;
      FILL_GETINFO_RET (cl_context_properties, 1, &n, CL_SUCCESS);
    }
  } else {
    return CL_INVALID_VALUE;
  }

error:
  return err;
}

cl_command_queue
clCreateCommandQueue(cl_context                   context,
                     cl_device_id                 device,
                     cl_command_queue_properties  properties,
                     cl_int *                     errcode_ret)
{
  cl_command_queue queue = NULL;
  cl_int err = CL_SUCCESS;
  CHECK_CONTEXT (context);

  INVALID_DEVICE_IF (device != context->device);
  INVALID_VALUE_IF (properties & ~(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE));

  if(properties & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) {/*not supported now.*/
    err = CL_INVALID_QUEUE_PROPERTIES;
    goto error;
  }

  queue = cl_context_create_queue(context, device, properties, &err);
error:
  if (errcode_ret)
    *errcode_ret = err;
  return queue;
}

cl_int
clRetainCommandQueue(cl_command_queue command_queue)
{
  cl_int err = CL_SUCCESS;
  CHECK_QUEUE (command_queue);
  cl_command_queue_add_ref(command_queue);
error:
  return err;
}

cl_int
clReleaseCommandQueue(cl_command_queue command_queue)
{
  cl_int err = CL_SUCCESS;
  CHECK_QUEUE (command_queue);
  cl_command_queue_delete(command_queue);
error:
  return err;
}

cl_int
clGetCommandQueueInfo(cl_command_queue       command_queue,
                      cl_command_queue_info  param_name,
                      size_t                 param_value_size,
                      void *                 param_value,
                      size_t *               param_value_size_ret)
{
  cl_int err = CL_SUCCESS;
  CHECK_QUEUE (command_queue);

  if (param_name == CL_QUEUE_CONTEXT) {
    FILL_GETINFO_RET (cl_context, 1, &command_queue->ctx, CL_SUCCESS);
  } else if (param_name == CL_QUEUE_DEVICE) {
    FILL_GETINFO_RET (cl_device_id, 1, &command_queue->ctx->device, CL_SUCCESS);
  } else if (param_name == CL_QUEUE_REFERENCE_COUNT) {
    cl_uint ref = command_queue->ref_n;
    FILL_GETINFO_RET (cl_uint, 1, &ref, CL_SUCCESS);
  } else if (param_name == CL_QUEUE_PROPERTIES) {
    FILL_GETINFO_RET (cl_command_queue_properties, 1, &command_queue->props, CL_SUCCESS);
  } else {
    return CL_INVALID_VALUE;
  }

error:
  return err;
}

cl_mem
clCreateBuffer(cl_context    context,
               cl_mem_flags  flags,
               size_t        size,
               void *        host_ptr,
               cl_int *      errcode_ret)
{
  cl_mem mem = NULL;
  cl_int err = CL_SUCCESS;
  CHECK_CONTEXT (context);

  mem = cl_mem_new_buffer(context, flags, size, host_ptr, &err);
error:
  if (errcode_ret)
    *errcode_ret = err;
  return mem;
}

cl_mem
clCreateSubBuffer(cl_mem                buffer,
                  cl_mem_flags          flags,
                  cl_buffer_create_type buffer_create_type,
                  const void *          buffer_create_info,
                  cl_int *              errcode_ret)
{
#if 0
  cl_int err = CL_SUCCESS;
  CHECK_MEM (buffer);
  NOT_IMPLEMENTED;
error:
#endif
  return NULL;
}

cl_mem
clCreateImage(cl_context context,
              cl_mem_flags flags,
              const cl_image_format *image_format,
              const cl_image_desc *image_desc,
              void *host_ptr,
              cl_int * errcode_ret)
{
  cl_mem mem = NULL;
  cl_int err = CL_SUCCESS;
  CHECK_CONTEXT (context);

  mem = cl_mem_new_image(context,
                         flags,
                         image_format,
                         image_desc,
                         host_ptr,
                         &err);
error:
  if (errcode_ret)
    *errcode_ret = err;
  return mem;
}

cl_mem
clCreateImage2D(cl_context              context,
                cl_mem_flags            flags,
                const cl_image_format * image_format,
                size_t                  image_width,
                size_t                  image_height,
                size_t                  image_row_pitch,
                void *                  host_ptr,
                cl_int *                errcode_ret)
{
  cl_mem mem = NULL;
  cl_int err = CL_SUCCESS;
  CHECK_CONTEXT (context);
  cl_image_desc image_desc;
  memset(&image_desc, 0, sizeof(image_desc));

  image_desc.image_type = CL_MEM_OBJECT_IMAGE2D;
  image_desc.image_width = image_width;
  image_desc.image_height = image_height;
  image_desc.image_row_pitch = image_row_pitch;

  mem = cl_mem_new_image(context,
                         flags,
                         image_format,
                         &image_desc,
                         host_ptr,
                         &err);
error:
  if (errcode_ret)
    *errcode_ret = err;
  return mem;
}

cl_mem
clCreateImage3D(cl_context              context,
                cl_mem_flags            flags,
                const cl_image_format * image_format,
                size_t                  image_width,
                size_t                  image_height,
                size_t                  image_depth,
                size_t                  image_row_pitch,
                size_t                  image_slice_pitch,
                void *                  host_ptr,
                cl_int *                errcode_ret)
{
  cl_mem mem = NULL;
  cl_int err = CL_SUCCESS;
  CHECK_CONTEXT (context);
  cl_image_desc image_desc;

  image_desc.image_type = CL_MEM_OBJECT_IMAGE3D;
  image_desc.image_width = image_width;
  image_desc.image_height = image_height;
  image_desc.image_depth = image_depth;
  image_desc.image_row_pitch = image_row_pitch;
  image_desc.image_slice_pitch = image_slice_pitch;

  mem = cl_mem_new_image(context,
                         flags,
                         image_format,
                         &image_desc,
                         host_ptr,
                         &err);
error:
  if (errcode_ret)
    *errcode_ret = err;
  return mem;
}

cl_int
clRetainMemObject(cl_mem memobj)
{
  cl_int err = CL_SUCCESS;
  CHECK_MEM (memobj);
  cl_mem_add_ref(memobj);
error:
  return err;
}

cl_int
clReleaseMemObject(cl_mem memobj)
{
  cl_int err = CL_SUCCESS;
  CHECK_MEM (memobj);
  cl_mem_delete(memobj);
error:
  return err;
}

cl_int
clGetSupportedImageFormats(cl_context         ctx,
                           cl_mem_flags       flags,
                           cl_mem_object_type image_type,
                           cl_uint            num_entries,
                           cl_image_format *  image_formats,
                           cl_uint *          num_image_formats)
{
  cl_int err = CL_SUCCESS;
  CHECK_CONTEXT (ctx);
  if (UNLIKELY(num_entries == 0 && image_formats != NULL)) {
    err = CL_INVALID_VALUE;
    goto error;
  }
  if (UNLIKELY(image_type != CL_MEM_OBJECT_IMAGE2D &&
               image_type != CL_MEM_OBJECT_IMAGE3D)) {
    err = CL_INVALID_VALUE;
    goto error;
  }
  err = cl_image_get_supported_fmt(ctx,
                                   image_type,
                                   num_entries,
                                   image_formats,
                                   num_image_formats);

error:
  return err;
}

cl_int
clGetMemObjectInfo(cl_mem      memobj,
                   cl_mem_info param_name,
                   size_t      param_value_size,
                   void *      param_value,
                   size_t *    param_value_size_ret)
{
  cl_int err = CL_SUCCESS;
  CHECK_MEM(memobj);

  err = cl_get_mem_object_info(memobj,
                               param_name,
                               param_value_size,
                               param_value,
                               param_value_size_ret);
error:
  return err;
}

cl_int
clGetImageInfo(cl_mem         mem,
               cl_image_info  param_name,
               size_t         param_value_size,
               void *         param_value,
               size_t *       param_value_size_ret)
{
  return cl_get_image_info(mem,
                           param_name,
                           param_value_size,
                           param_value,
                           param_value_size_ret);
}

cl_int
clSetMemObjectDestructorCallback(cl_mem  memobj,
                                 void (CL_CALLBACK *pfn_notify) (cl_mem, void*),
                                 void * user_data)
{
  cl_int err = CL_SUCCESS;
  CHECK_MEM(memobj);
  INVALID_VALUE_IF (pfn_notify == 0);

  cl_mem_dstr_cb *cb = (cl_mem_dstr_cb*)malloc(sizeof(cl_mem_dstr_cb));
  if (!cb) {
    err = CL_OUT_OF_HOST_MEMORY;
    goto error;
  }

  memset(cb, 0, sizeof(cl_mem_dstr_cb));
  cb->pfn_notify = pfn_notify;
  cb->user_data = user_data;
  cb->next = memobj->dstr_cb;
  memobj->dstr_cb = cb;

error:
  return err;
}

cl_sampler
clCreateSampler(cl_context         context,
                cl_bool            normalized,
                cl_addressing_mode addressing,
                cl_filter_mode     filter,
                cl_int *           errcode_ret)
{
  cl_sampler sampler = NULL;
  cl_int err = CL_SUCCESS;
  CHECK_CONTEXT (context);
  sampler = cl_sampler_new(context, normalized, addressing, filter, &err);
error:
  if (errcode_ret)
    *errcode_ret = err;
  return sampler;
}

cl_int
clRetainSampler(cl_sampler sampler)
{
  cl_int err = CL_SUCCESS;
  CHECK_SAMPLER (sampler);
  cl_sampler_add_ref(sampler);
error:
  return err;
}

cl_int
clReleaseSampler(cl_sampler sampler)
{
  cl_int err = CL_SUCCESS;
  CHECK_SAMPLER (sampler);
  cl_sampler_delete(sampler);
error:
  return err;
}

cl_int
clGetSamplerInfo(cl_sampler       sampler,
                 cl_sampler_info  param_name,
                 size_t           param_value_size,
                 void *           param_value,
                 size_t *         param_value_size_ret)
{
  NOT_IMPLEMENTED;
  return 0;
}

cl_program
clCreateProgramWithSource(cl_context     context,
                          cl_uint        count,
                          const char **  strings,
                          const size_t * lengths,
                          cl_int *       errcode_ret)
{
  cl_program program = NULL;
  cl_int err = CL_SUCCESS;
  cl_uint i;

  CHECK_CONTEXT (context);
  INVALID_VALUE_IF (count == 0);
  INVALID_VALUE_IF (strings == NULL);
  for(i = 0; i < count; i++) {
    if(UNLIKELY(strings[i] == NULL)) {
      err = CL_INVALID_VALUE;
      goto error;
    }
  }
  program = cl_program_create_from_source(context,
                                          count,
                                          strings,
                                          lengths,
                                          &err);
error:
  if (errcode_ret)
    *errcode_ret = err;
  return program;
}

cl_program
clCreateProgramWithBinary(cl_context             context,
                          cl_uint                num_devices,
                          const cl_device_id *   devices,
                          const size_t *         lengths,
                          const unsigned char ** binaries,
                          cl_int *               binary_status,
                          cl_int *               errcode_ret)
{
  cl_program program = NULL;
  cl_int err = CL_SUCCESS;

  CHECK_CONTEXT (context);
  program = cl_program_create_from_binary(context,
                                          num_devices,
                                          devices,
                                          lengths,
                                          binaries,
                                          binary_status,
                                          &err);
error:
  if (errcode_ret)
    *errcode_ret = err;
  return program;
}
cl_int
clRetainProgram(cl_program program)
{
  cl_int err = CL_SUCCESS;
  CHECK_PROGRAM (program);
  cl_program_add_ref(program);
error:
  return err;
}

cl_int
clReleaseProgram(cl_program program)
{
  cl_int err = CL_SUCCESS;
  CHECK_PROGRAM (program);
  cl_program_delete(program);
error:
  return err;
}

cl_int
clBuildProgram(cl_program            program,
               cl_uint               num_devices,
               const cl_device_id *  device_list,
               const char *          options,
               void (CL_CALLBACK *pfn_notify) (cl_program, void*),
               void *                user_data)
{
  cl_int err = CL_SUCCESS;
  CHECK_PROGRAM(program);
  INVALID_VALUE_IF (num_devices > 1);
  INVALID_VALUE_IF (num_devices == 0 && device_list != NULL);
  INVALID_VALUE_IF (num_devices != 0 && device_list == NULL);
  INVALID_VALUE_IF (pfn_notify  == 0 && user_data   != NULL);

  /* Everything is easy. We only support one device anyway */
  if (num_devices != 0) {
    assert(program->ctx);
    INVALID_DEVICE_IF (device_list[0] != program->ctx->device);
  }

  /* TODO support create program from binary */
  assert(program->source_type == FROM_LLVM ||
         program->source_type == FROM_SOURCE ||
         program->source_type == FROM_BINARY);
  if((err = cl_program_build(program, options)) != CL_SUCCESS) {
    goto error;
  }
  program->is_built = CL_TRUE;

  if (pfn_notify) pfn_notify(program, user_data);

error:
  return err;
}

cl_int
clUnloadCompiler(void)
{
  return CL_SUCCESS;
}

cl_int
clGetProgramInfo(cl_program       program,
                 cl_program_info  param_name,
                 size_t           param_value_size,
                 void *           param_value,
                 size_t *         param_value_size_ret)
{
  cl_int err = CL_SUCCESS;
  char * ret_str = "";

  CHECK_PROGRAM (program);

  if (param_name == CL_PROGRAM_REFERENCE_COUNT) {
    cl_uint ref = program->ref_n;
    FILL_GETINFO_RET (cl_uint, 1, (&ref), CL_SUCCESS);
  } else if (param_name == CL_PROGRAM_CONTEXT) {
    cl_context context = program->ctx;
    FILL_GETINFO_RET (cl_context, 1, &context, CL_SUCCESS);
  } else if (param_name == CL_PROGRAM_NUM_DEVICES) {
    cl_uint num_dev = 1; // Just 1 dev now.
    FILL_GETINFO_RET (cl_uint, 1, &num_dev, CL_SUCCESS);
  } else if (param_name == CL_PROGRAM_DEVICES) {
    cl_device_id dev_id = program->ctx->device;
    FILL_GETINFO_RET (cl_device_id, 1, &dev_id, CL_SUCCESS);
  } else if (param_name == CL_PROGRAM_SOURCE) {

    if (!program->source)
      FILL_GETINFO_RET (char, 1, &ret_str, CL_SUCCESS);
    FILL_GETINFO_RET (char, (strlen(program->source) + 1),
                   program->source, CL_SUCCESS);
  } else if (param_name == CL_PROGRAM_BINARY_SIZES) {
    FILL_GETINFO_RET (size_t, 1, (&program->bin_sz), CL_SUCCESS);
  } else if (param_name == CL_PROGRAM_BINARIES) {
    if (!param_value)
      return CL_SUCCESS;

    /* param_value points to an array of n
       pointers allocated by the caller */
    if (program->bin_sz > 0) {
      memcpy(*((void **)param_value), program->bin, program->bin_sz);
    } else {
      memcpy(*((void **)param_value), ret_str, 1);
    }

    return CL_SUCCESS;
  } else {
    return CL_INVALID_VALUE;
  }

error:
    return err;
}

cl_int
clGetProgramBuildInfo(cl_program             program,
                      cl_device_id           device,
                      cl_program_build_info  param_name,
                      size_t                 param_value_size,
                      void *                 param_value,
                      size_t *               param_value_size_ret)
{
  cl_int err = CL_SUCCESS;
  char * ret_str = "";

  CHECK_PROGRAM (program);
  INVALID_DEVICE_IF (device != program->ctx->device);

  if (param_name == CL_PROGRAM_BUILD_STATUS) {
    cl_build_status status;

    if (!program->is_built)
      status = CL_BUILD_NONE;
    else if (program->ker_n > 0)
      status = CL_BUILD_SUCCESS;
    else
      status = CL_BUILD_ERROR;
    // TODO: Support CL_BUILD_IN_PROGRESS ?

    FILL_GETINFO_RET (cl_build_status, 1, &status, CL_SUCCESS);
  } else if (param_name == CL_PROGRAM_BUILD_OPTIONS) {
    if (program->is_built && program->build_opts)
      ret_str = program->build_opts;

    FILL_GETINFO_RET (char, (strlen(ret_str)+1), ret_str, CL_SUCCESS);
  } else if (param_name == CL_PROGRAM_BUILD_LOG) {
    // TODO: need to add logs in backend when compiling.
    FILL_GETINFO_RET (char, (strlen(ret_str)+1), ret_str, CL_SUCCESS);
  } else {
    return CL_INVALID_VALUE;
  }

error:
    return err;
}

cl_kernel
clCreateKernel(cl_program   program,
               const char * kernel_name,
               cl_int *     errcode_ret)
{
  cl_kernel kernel = NULL;
  cl_int err = CL_SUCCESS;

  CHECK_PROGRAM (program);
  if (program->is_built == CL_FALSE) {
    err = CL_INVALID_PROGRAM_EXECUTABLE;
    goto error;
  }
  INVALID_VALUE_IF (kernel_name == NULL);
  kernel = cl_program_create_kernel(program, kernel_name, &err);

error:
  if (errcode_ret)
    *errcode_ret = err;
  return kernel;
}

cl_int
clCreateKernelsInProgram(cl_program      program,
                         cl_uint         num_kernels,
                         cl_kernel *     kernels,
                         cl_uint *       num_kernels_ret)
{
  cl_int err = CL_SUCCESS;

  CHECK_PROGRAM (program);
  if (program->is_built == CL_FALSE) {
    err = CL_INVALID_PROGRAM_EXECUTABLE;
    goto error;
  }
  if (kernels && num_kernels < program->ker_n) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if(num_kernels_ret)
    *num_kernels_ret = program->ker_n;

  if(kernels)
    err = cl_program_create_kernels_in_program(program, kernels);

error:
  return err;
}

cl_int
clRetainKernel(cl_kernel kernel)
{
  cl_int err = CL_SUCCESS;
  CHECK_KERNEL(kernel);
  cl_kernel_add_ref(kernel);
error:
  return err;
}

cl_int
clReleaseKernel(cl_kernel kernel)
{
  cl_int err = CL_SUCCESS;
  CHECK_KERNEL(kernel);
  cl_kernel_delete(kernel);
error:
  return err;
}

cl_int
clSetKernelArg(cl_kernel     kernel,
               cl_uint       arg_index,
               size_t        arg_size,
               const void *  arg_value)
{
  cl_int err = CL_SUCCESS;
  CHECK_KERNEL(kernel);
  err = cl_kernel_set_arg(kernel, arg_index, arg_size, arg_value);
error:
  return err;
}

cl_int
clGetKernelInfo(cl_kernel        kernel,
                cl_kernel_info   param_name,
                size_t           param_value_size,
                void *           param_value,
                size_t *         param_value_size_ret)
{
  cl_int err;

  CHECK_KERNEL(kernel);

  if (param_name == CL_KERNEL_CONTEXT) {
    FILL_GETINFO_RET (cl_context, 1, &kernel->program->ctx, CL_SUCCESS);
  } else if (param_name == CL_KERNEL_PROGRAM) {
    FILL_GETINFO_RET (cl_program, 1, &kernel->program, CL_SUCCESS);
  } else if (param_name == CL_KERNEL_NUM_ARGS) {
    cl_uint n = kernel->arg_n;
    FILL_GETINFO_RET (cl_uint, 1, &n, CL_SUCCESS);
  } else if (param_name == CL_KERNEL_REFERENCE_COUNT) {
    cl_int ref = kernel->ref_n;
    FILL_GETINFO_RET (cl_int, 1, &ref, CL_SUCCESS);
  } else if (param_name == CL_KERNEL_FUNCTION_NAME) {
    const char * n = cl_kernel_get_name(kernel);
    FILL_GETINFO_RET (cl_char, strlen(n)+1, n, CL_SUCCESS);
  } else {
    return CL_INVALID_VALUE;
  }

error:
  return err;
}

cl_int
clGetKernelWorkGroupInfo(cl_kernel                   kernel,
                         cl_device_id                device,
                         cl_kernel_work_group_info   param_name,
                         size_t                      param_value_size,
                         void *                      param_value,
                         size_t *                    param_value_size_ret)
{
  return cl_get_kernel_workgroup_info(device,
                                      param_name,
                                      param_value_size,
                                      param_value,
                                      param_value_size_ret);
}

cl_int
clWaitForEvents(cl_uint          num_events,
                const cl_event * event_list)
{
  cl_int err = CL_SUCCESS;
  cl_context ctx = NULL;

  if(num_events > 0 && event_list)
    ctx = event_list[0]->ctx;

  TRY(cl_event_check_waitlist, num_events, event_list, NULL, ctx);

  while(cl_event_wait_events(num_events, event_list, NULL) == CL_ENQUEUE_EXECUTE_DEFER) {
    usleep(8000);       //sleep 8ms to wait other thread
  }

error:
  return err;
}

cl_int
clGetEventInfo(cl_event      event,
               cl_event_info param_name,
               size_t        param_value_size,
               void *        param_value,
               size_t *      param_value_size_ret)
{
  cl_int err = CL_SUCCESS;
  CHECK_EVENT(event);

  if (param_name == CL_EVENT_COMMAND_QUEUE) {
    FILL_GETINFO_RET (cl_command_queue, 1, &event->queue, CL_SUCCESS);
  } else if (param_name == CL_EVENT_CONTEXT) {
    FILL_GETINFO_RET (cl_context, 1, &event->ctx, CL_SUCCESS);
  } else if (param_name == CL_EVENT_COMMAND_TYPE) {
    FILL_GETINFO_RET (cl_command_type, 1, &event->type, CL_SUCCESS);
  } else if (param_name == CL_EVENT_COMMAND_EXECUTION_STATUS) {
    cl_event_update_status(event);
    FILL_GETINFO_RET (cl_int, 1, &event->status, CL_SUCCESS);
  } else if (param_name == CL_EVENT_REFERENCE_COUNT) {
    cl_uint ref = event->ref_n;
    FILL_GETINFO_RET (cl_int, 1, &ref, CL_SUCCESS);
  } else {
    return CL_INVALID_VALUE;
  }

error:
  return err;

}

cl_event
clCreateUserEvent(cl_context context,
                  cl_int *   errcode_ret)
{
  cl_int err = CL_SUCCESS;
  cl_event event = NULL;
  CHECK_CONTEXT(context);

  TRY_ALLOC(event, cl_event_new(context, NULL, CL_COMMAND_USER, CL_TRUE));

error:
  if(errcode_ret)
    *errcode_ret = err;
  return event;
}

cl_int
clRetainEvent(cl_event  event)
{
  cl_int err = CL_SUCCESS;

  CHECK_EVENT(event);
  cl_event_add_ref(event);

error:
  return err;
}

cl_int
clReleaseEvent(cl_event  event)
{
  cl_int err = CL_SUCCESS;

  CHECK_EVENT(event);
  cl_event_delete(event);

error:
  return err;
}

cl_int
clSetUserEventStatus(cl_event    event,
                     cl_int      execution_status)
{
  cl_int err = CL_SUCCESS;

  CHECK_EVENT(event);
  if(execution_status > CL_COMPLETE) {
    err = CL_INVALID_VALUE;
    goto error;
  }
  if(event->status != CL_SUBMITTED) {
    err = CL_INVALID_OPERATION;
    goto error;
  }

  cl_event_set_status(event, execution_status);
error:
  return err;
}

cl_int
clSetEventCallback(cl_event     event,
                   cl_int       command_exec_callback_type,
                   void (CL_CALLBACK * pfn_notify) (cl_event, cl_int, void *),
                   void *       user_data)
{
  cl_int err = CL_SUCCESS;

  CHECK_EVENT(event);
  if((pfn_notify == NULL) ||
    (command_exec_callback_type > CL_SUBMITTED) ||
    (command_exec_callback_type < CL_COMPLETE)) {
    err = CL_INVALID_VALUE;
    goto error;
  }
  err = cl_event_set_callback(event, command_exec_callback_type, pfn_notify, user_data);

error:
  return err;

}

cl_int
clGetEventProfilingInfo(cl_event             event,
                        cl_profiling_info    param_name,
                        size_t               param_value_size,
                        void *               param_value,
                        size_t *             param_value_size_ret)
{
  cl_int err = CL_SUCCESS;
  cl_ulong ret_val;

  CHECK_EVENT(event);

  if (!(event->queue->props & CL_QUEUE_PROFILING_ENABLE) ||
          event->type == CL_COMMAND_USER ||
          event->status != CL_COMPLETE) {
    err = CL_PROFILING_INFO_NOT_AVAILABLE;
    goto error;
  }

  if ((param_name != CL_PROFILING_COMMAND_QUEUED &&
          param_name != CL_PROFILING_COMMAND_SUBMIT &&
          param_name != CL_PROFILING_COMMAND_START &&
          param_name != CL_PROFILING_COMMAND_END) ||
          (param_value && param_value_size < sizeof(cl_ulong))) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  err = cl_event_profiling(event, param_name, &ret_val);

  if (err == CL_SUCCESS) {
    if (param_value)
      *(cl_ulong*)param_value = ret_val;
    if (param_value_size_ret)
      *param_value_size_ret = sizeof(cl_ulong);
  }
error:
  return err;
}

cl_int
clFlush(cl_command_queue command_queue)
{
  /* have nothing to do now, as currently
   * clEnqueueNDRangeKernel will flush at
   * the end of each calling. we may need
   * to optimize it latter.*/
  return 0;
}

cl_int
clFinish(cl_command_queue command_queue)
{
  cl_int err = CL_SUCCESS;

  CHECK_QUEUE (command_queue);
  err = cl_command_queue_finish(command_queue);

error:
  return err;
}

cl_int
clEnqueueReadBuffer(cl_command_queue command_queue,
                    cl_mem           buffer,
                    cl_bool          blocking_read,
                    size_t           offset,
                    size_t           size,
                    void *           ptr,
                    cl_uint          num_events_in_wait_list,
                    const cl_event * event_wait_list,
                    cl_event *       event)
{
  cl_int err = CL_SUCCESS;
  enqueue_data *data, defer_enqueue_data = { 0 };
  CHECK_QUEUE(command_queue);
  CHECK_MEM(buffer);
  if (command_queue->ctx != buffer->ctx) {
     err = CL_INVALID_CONTEXT;
     goto error;
  }

  if (!ptr || !size || offset + size > buffer->size) {
     err = CL_INVALID_VALUE;
     goto error;
  }

  if (buffer->flags & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS)) {
     err = CL_INVALID_OPERATION;
     goto error;
  }

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, buffer->ctx);

  data = &defer_enqueue_data;
  data->type    = EnqueueReadBuffer;
  data->mem_obj = buffer;
  data->ptr     = ptr;
  data->offset  = offset;
  data->size    = size;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_READ_BUFFER) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_enqueue_handle(data);
    if(event) cl_event_set_status(*event, CL_COMPLETE);
  }

error:
  return err;
}

cl_int
clEnqueueReadBufferRect(cl_command_queue command_queue,
                        cl_mem           buffer,
                        cl_bool          blocking_read,
                        const size_t *   buffer_origin,
                        const size_t *   host_origin,
                        const size_t *   region,
                        size_t           buffer_row_pitch,
                        size_t           buffer_slice_pitch,
                        size_t           host_row_pitch,
                        size_t           host_slice_pitch,
                        void *           ptr,
                        cl_uint          num_events_in_wait_list,
                        const cl_event * event_wait_list,
                        cl_event *       event)
{
  cl_int err = CL_SUCCESS;
  enqueue_data *data, no_wait_data = { 0 };

  CHECK_QUEUE(command_queue);
  CHECK_MEM(buffer);

  if (command_queue->ctx != buffer->ctx) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  if (!ptr || !region || region[0] == 0 || region[1] == 0 || region[2] == 0) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if(buffer_row_pitch == 0)
    buffer_row_pitch = region[0];
  if(buffer_slice_pitch == 0)
    buffer_slice_pitch = region[1] * buffer_row_pitch;

  if(host_row_pitch == 0)
    host_row_pitch = region[0];
  if(host_slice_pitch == 0)
    host_slice_pitch = region[1] * host_row_pitch;

  if (buffer_row_pitch < region[0] ||
      host_row_pitch < region[0]) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if ((buffer_slice_pitch < region[1] * buffer_row_pitch || buffer_slice_pitch % buffer_row_pitch != 0 ) ||
      (host_slice_pitch < region[1] * host_row_pitch || host_slice_pitch % host_row_pitch != 0 )) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if ((buffer_origin[2]+region[2])*buffer_slice_pitch + (buffer_origin[1]+region[1])*buffer_row_pitch + buffer_origin[0] + region[0] > buffer->size) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, buffer->ctx);

  data = &no_wait_data;
  data->type        = EnqueueReadBufferRect;
  data->mem_obj     = buffer;
  data->ptr         = ptr;
  data->origin[0]   = buffer_origin[0]; data->origin[1] = buffer_origin[1]; data->origin[2] = buffer_origin[2];
  data->host_origin[0]  = host_origin[0]; data->host_origin[1] = host_origin[1]; data->host_origin[2] = host_origin[2];
  data->region[0]   = region[0];  data->region[1] = region[1];  data->region[2] = region[2];
  data->row_pitch   = buffer_row_pitch;
  data->slice_pitch = buffer_slice_pitch;
  data->host_row_pitch   = host_row_pitch;
  data->host_slice_pitch = host_slice_pitch;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_READ_BUFFER_RECT) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_enqueue_handle(data);
    if(event) cl_event_set_status(*event, CL_COMPLETE);
  }

 error:
  return err;
}

cl_int
clEnqueueWriteBuffer(cl_command_queue    command_queue,
                     cl_mem              buffer,
                     cl_bool             blocking_write,
                     size_t              offset,
                     size_t              size,
                     const void *        ptr,
                     cl_uint             num_events_in_wait_list,
                     const cl_event *    event_wait_list,
                     cl_event *          event)
{
  cl_int err = CL_SUCCESS;
  enqueue_data *data, no_wait_data = { 0 };

  CHECK_QUEUE(command_queue);
  CHECK_MEM(buffer);
  if (command_queue->ctx != buffer->ctx) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  if (!ptr || !size || offset + size > buffer->size) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (buffer->flags & (CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)) {
    err = CL_INVALID_OPERATION;
    goto error;
  }

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, buffer->ctx);

  data = &no_wait_data;
  data->type      = EnqueueWriteBuffer;
  data->mem_obj   = buffer;
  data->const_ptr = ptr;
  data->offset    = offset;
  data->size      = size;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_WRITE_BUFFER) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_enqueue_handle(data);
    if(event) cl_event_set_status(*event, CL_COMPLETE);
  }

 error:
  return err;
}

cl_int
clEnqueueWriteBufferRect(cl_command_queue     command_queue,
                         cl_mem               buffer,
                         cl_bool              blocking_write,
                         const size_t *       buffer_origin,
                         const size_t *       host_origin,
                         const size_t *       region,
                         size_t               buffer_row_pitch,
                         size_t               buffer_slice_pitch,
                         size_t               host_row_pitch,
                         size_t               host_slice_pitch,
                         const void *         ptr,
                         cl_uint              num_events_in_wait_list,
                         const cl_event *     event_wait_list,
                         cl_event *           event)
{
  cl_int err = CL_SUCCESS;
  enqueue_data *data, no_wait_data = { 0 };

  CHECK_QUEUE(command_queue);
  CHECK_MEM(buffer);

  if (command_queue->ctx != buffer->ctx) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  if (!ptr || !region || region[0] == 0 || region[1] == 0 || region[2] == 0) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if(buffer_row_pitch == 0)
    buffer_row_pitch = region[0];
  if(buffer_slice_pitch == 0)
    buffer_slice_pitch = region[1] * buffer_row_pitch;

  if(host_row_pitch == 0)
    host_row_pitch = region[0];
  if(host_slice_pitch == 0)
    host_slice_pitch = region[1] * host_row_pitch;

  if (buffer_row_pitch < region[0] ||
      host_row_pitch < region[0]) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if ((buffer_slice_pitch < region[1] * buffer_row_pitch || buffer_slice_pitch % buffer_row_pitch != 0 ) ||
      (host_slice_pitch < region[1] * host_row_pitch || host_slice_pitch % host_row_pitch != 0 )) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if ((buffer_origin[2]+region[2])*buffer_slice_pitch + (buffer_origin[1]+region[1])*buffer_row_pitch + buffer_origin[0] + region[0] > buffer->size) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, buffer->ctx);

  data = &no_wait_data;
  data->type        = EnqueueWriteBufferRect;
  data->mem_obj     = buffer;
  data->const_ptr   = ptr;
  data->origin[0]   = buffer_origin[0]; data->origin[1] = buffer_origin[1]; data->origin[2] = buffer_origin[2];
  data->host_origin[0]  = host_origin[0]; data->host_origin[1] = host_origin[1]; data->host_origin[2] = host_origin[2];
  data->region[0]   = region[0];  data->region[1] = region[1];  data->region[2] = region[2];
  data->row_pitch   = buffer_row_pitch;
  data->slice_pitch = buffer_slice_pitch;
  data->host_row_pitch   = host_row_pitch;
  data->host_slice_pitch = host_slice_pitch;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_WRITE_BUFFER_RECT) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_enqueue_handle(data);
    if(event) cl_event_set_status(*event, CL_COMPLETE);
  }

error:
  return err;
}

cl_int
clEnqueueCopyBuffer(cl_command_queue     command_queue,
                    cl_mem               src_buffer,
                    cl_mem               dst_buffer,
                    size_t               src_offset,
                    size_t               dst_offset,
                    size_t               cb,
                    cl_uint              num_events_in_wait_list,
                    const cl_event *     event_wait_list,
                    cl_event *           event)
{
  cl_int err = CL_SUCCESS;
  enqueue_data *data, no_wait_data = { 0 };

  CHECK_QUEUE(command_queue);
  CHECK_MEM(src_buffer);
  CHECK_MEM(dst_buffer);

  if (command_queue->ctx != src_buffer->ctx) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  if (command_queue->ctx != dst_buffer->ctx) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  if (src_offset < 0 || src_offset + cb > src_buffer->size) {
    err = CL_INVALID_VALUE;
    goto error;
  }
  if (dst_offset < 0 || dst_offset + cb > src_buffer->size) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  /* Check overlap */
  if (src_buffer == dst_buffer
         && (src_offset <= dst_offset && dst_offset <= src_offset + cb - 1)
         && (dst_offset <= src_offset && src_offset <= dst_offset + cb - 1)) {
    err = CL_MEM_COPY_OVERLAP;
    goto error;
  }

  // TODO: Need to check the sub buffer cases.
  err = cl_mem_copy(command_queue, src_buffer, dst_buffer, src_offset, dst_offset, cb);

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, src_buffer->ctx);

  data = &no_wait_data;
  data->type = EnqueueCopyBuffer;
  data->queue = command_queue;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_COPY_BUFFER) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_command_queue_flush(command_queue);
  }
  return 0;

error:
  return err;
}

cl_int
clEnqueueCopyBufferRect(cl_command_queue     command_queue,
                        cl_mem               src_buffer,
                        cl_mem               dst_buffer,
                        const size_t *       src_origin,
                        const size_t *       dst_origin,
                        const size_t *       region,
                        size_t               src_row_pitch,
                        size_t               src_slice_pitch,
                        size_t               dst_row_pitch,
                        size_t               dst_slice_pitch,
                        cl_uint              num_events_in_wait_list,
                        const cl_event *     event_wait_list,
                        cl_event *           event)
{
  cl_int err = CL_SUCCESS;
  enqueue_data *data, no_wait_data = { 0 };

  CHECK_QUEUE(command_queue);
  CHECK_MEM(src_buffer);
  CHECK_MEM(dst_buffer);

  if ((command_queue->ctx != src_buffer->ctx) ||
      (command_queue->ctx != dst_buffer->ctx)) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  if (!region || region[0] == 0 || region[1] == 0 || region[2] == 0) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if(src_row_pitch == 0)
    src_row_pitch = region[0];
  if(src_slice_pitch == 0)
    src_slice_pitch = region[1] * src_row_pitch;

  if(dst_row_pitch == 0)
    dst_row_pitch = region[0];
  if(dst_slice_pitch == 0)
    dst_slice_pitch = region[1] * dst_row_pitch;

  if (src_row_pitch < region[0] ||
      dst_row_pitch < region[0]) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if ((src_slice_pitch < region[1] * src_row_pitch || src_slice_pitch % src_row_pitch != 0 ) ||
      (dst_slice_pitch < region[1] * dst_row_pitch || dst_slice_pitch % dst_row_pitch != 0 )) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if ((src_origin[2]+region[2])*src_slice_pitch + (src_origin[1]+region[1])*src_row_pitch + src_origin[0] + region[0] > src_buffer->size ||
      (dst_origin[2]+region[2])*dst_slice_pitch + (dst_origin[1]+region[1])*dst_row_pitch + dst_origin[0] + region[0] > dst_buffer->size) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (src_buffer == dst_buffer && (src_row_pitch != dst_row_pitch || src_slice_pitch != dst_slice_pitch)) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (src_buffer == dst_buffer &&
      check_copy_overlap(src_origin, dst_origin, region, src_row_pitch, src_slice_pitch)) {
    err = CL_MEM_COPY_OVERLAP;
    goto error;
  }

  cl_mem_copy_buffer_rect(command_queue, src_buffer, dst_buffer, src_origin, dst_origin, region,
                          src_row_pitch, src_slice_pitch, dst_row_pitch, dst_slice_pitch);

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, src_buffer->ctx);

  data = &no_wait_data;
  data->type = EnqueueCopyBufferRect;
  data->queue = command_queue;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_COPY_BUFFER_RECT) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_command_queue_flush(command_queue);
  }

error:
  return err;
}

cl_int
clEnqueueReadImage(cl_command_queue      command_queue,
                   cl_mem                mem,
                   cl_bool               blocking_read,
                   const size_t *        origin,
                   const size_t *        region,
                   size_t                row_pitch,
                   size_t                slice_pitch,
                   void *                ptr,
                   cl_uint               num_events_in_wait_list,
                   const cl_event *      event_wait_list,
                   cl_event *            event)
{
  cl_int err = CL_SUCCESS;
  enqueue_data *data, no_wait_data = { 0 };

  CHECK_QUEUE(command_queue);
  CHECK_IMAGE(mem, image);
  if (command_queue->ctx != mem->ctx) {
     err = CL_INVALID_CONTEXT;
     goto error;
  }

  if (!origin || !region || origin[0] + region[0] > image->w || origin[1] + region[1] > image->h || origin[2] + region[2] > image->depth) {
     err = CL_INVALID_VALUE;
     goto error;
  }

  if (!row_pitch)
    row_pitch = image->bpp*region[0];
  else if (row_pitch < image->bpp*region[0]) {
     err = CL_INVALID_VALUE;
     goto error;
  }

  if (image->slice_pitch) {
    if (!slice_pitch)
      slice_pitch = row_pitch*region[1];
    else if (slice_pitch < row_pitch*region[1]) {
      err = CL_INVALID_VALUE;
      goto error;
    }
  }
  else if (slice_pitch) {
     err = CL_INVALID_VALUE;
     goto error;
  }

  if (!ptr) {
     err = CL_INVALID_VALUE;
     goto error;
  }

  if (mem->flags & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS)) {
     err = CL_INVALID_OPERATION;
     goto error;
  }

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, mem->ctx);

  data = &no_wait_data;
  data->type        = EnqueueReadImage;
  data->mem_obj     = mem;
  data->ptr         = ptr;
  data->origin[0]   = origin[0];  data->origin[1] = origin[1];  data->origin[2] = origin[2];
  data->region[0]   = region[0];  data->region[1] = region[1];  data->region[2] = region[2];
  data->row_pitch   = row_pitch;
  data->slice_pitch = slice_pitch;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_READ_IMAGE) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_enqueue_handle(data);
    if(event) cl_event_set_status(*event, CL_COMPLETE);
  }

error:
  return err;
}

cl_int
clEnqueueWriteImage(cl_command_queue     command_queue,
                    cl_mem               mem,
                    cl_bool              blocking_write,
                    const size_t *       origin,
                    const size_t *       region,
                    size_t               row_pitch,
                    size_t               slice_pitch,
                    const void *         ptr,
                    cl_uint              num_events_in_wait_list,
                    const cl_event *     event_wait_list,
                    cl_event *           event)
{
  cl_int err = CL_SUCCESS;
  enqueue_data *data, no_wait_data = { 0 };

  CHECK_QUEUE(command_queue);
  CHECK_IMAGE(mem, image);
  if (command_queue->ctx != mem->ctx) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  if (!origin || !region || origin[0] + region[0] > image->w || origin[1] + region[1] > image->h || origin[2] + region[2] > image->depth) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (!row_pitch)
    row_pitch = image->bpp*region[0];
  else if (row_pitch < image->bpp*region[0]) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (image->slice_pitch) {
    if (!slice_pitch)
      slice_pitch = row_pitch*region[1];
    else if (slice_pitch < row_pitch*region[1]) {
      err = CL_INVALID_VALUE;
      goto error;
    }
  }
  else if (slice_pitch) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (!ptr) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (mem->flags & (CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)) {
    err = CL_INVALID_OPERATION;
    goto error;
  }

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, mem->ctx);

  data = &no_wait_data;
  data->type        = EnqueueWriteImage;
  data->mem_obj     = mem;
  data->const_ptr   = ptr;
  data->origin[0]   = origin[0];  data->origin[1] = origin[1];  data->origin[2] = origin[2];
  data->region[0]   = region[0];  data->region[1] = region[1];  data->region[2] = region[2];
  data->row_pitch   = row_pitch;
  data->slice_pitch = slice_pitch;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_WRITE_IMAGE) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_enqueue_handle(data);
    if(event) cl_event_set_status(*event, CL_COMPLETE);
  }

error:
  return err;
}

cl_int
clEnqueueCopyImage(cl_command_queue      command_queue,
                   cl_mem                src_mem,
                   cl_mem                dst_mem,
                   const size_t *        src_origin,
                   const size_t *        dst_origin,
                   const size_t *        region,
                   cl_uint               num_events_in_wait_list,
                   const cl_event *      event_wait_list,
                   cl_event *            event)
{
  cl_int err = CL_SUCCESS;
  enqueue_data *data, no_wait_data = { 0 };
  cl_bool overlap = CL_TRUE;
  cl_int i = 0;

  CHECK_QUEUE(command_queue);
  CHECK_IMAGE(src_mem, src_image);
  CHECK_IMAGE(dst_mem, dst_image);
  if (command_queue->ctx != src_mem->ctx ||
      command_queue->ctx != dst_mem->ctx) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  if (src_image->fmt.image_channel_order != dst_image->fmt.image_channel_order ||
      src_image->fmt.image_channel_data_type != dst_image->fmt.image_channel_data_type) {
    err = CL_IMAGE_FORMAT_MISMATCH;
    goto error;
  }

  if (!src_origin || !region || src_origin[0] + region[0] > src_image->w ||
      src_origin[1] + region[1] > src_image->h || src_origin[2] + region[2] > src_image->depth) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (!dst_origin || !region || dst_origin[0] + region[0] > dst_image->w ||
      dst_origin[1] + region[1] > dst_image->h || dst_origin[2] + region[2] > dst_image->depth) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if ((src_image->image_type == CL_MEM_OBJECT_IMAGE2D && (src_origin[2] != 0 || region[2] != 1)) ||
      (dst_image->image_type == CL_MEM_OBJECT_IMAGE2D && (dst_origin[2] != 0 || region[2] != 1))) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (src_image == dst_image) {
    for(i = 0; i < 3; i++)
      overlap = overlap && (src_origin[i] < dst_origin[i] + region[i])
                        && (dst_origin[i] < src_origin[i] + region[i]);
    if(overlap == CL_TRUE) {
      err = CL_MEM_COPY_OVERLAP;
      goto error;
    }
  }

  cl_mem_kernel_copy_image(command_queue, src_image, dst_image, src_origin, dst_origin, region);

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, src_mem->ctx);

  data = &no_wait_data;
  data->type = EnqueueCopyImage;
  data->queue = command_queue;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_COPY_IMAGE) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_command_queue_flush(command_queue);
  }

error:
  return err;
}

cl_int
clEnqueueCopyImageToBuffer(cl_command_queue  command_queue,
                           cl_mem            src_mem,
                           cl_mem            dst_buffer,
                           const size_t *    src_origin,
                           const size_t *    region,
                           size_t            dst_offset,
                           cl_uint           num_events_in_wait_list,
                           const cl_event *  event_wait_list,
                           cl_event *        event)
{
  cl_int err = CL_SUCCESS;
  enqueue_data *data, no_wait_data = { 0 };

  CHECK_QUEUE(command_queue);
  CHECK_IMAGE(src_mem, src_image);
  CHECK_MEM(dst_buffer);
  if (command_queue->ctx != src_mem->ctx ||
      command_queue->ctx != dst_buffer->ctx) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  if (dst_offset + region[0]*region[1]*region[2]*src_image->bpp > dst_buffer->size) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (!src_origin || !region || src_origin[0] + region[0] > src_image->w ||
      src_origin[1] + region[1] > src_image->h || src_origin[2] + region[2] > src_image->depth) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (src_image->image_type == CL_MEM_OBJECT_IMAGE2D && (src_origin[2] != 0 || region[2] != 1)) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  cl_mem_copy_image_to_buffer(command_queue, src_image, dst_buffer, src_origin, dst_offset, region);

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, src_mem->ctx);

  data = &no_wait_data;
  data->type = EnqueueCopyImageToBuffer;
  data->queue = command_queue;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_COPY_IMAGE_TO_BUFFER) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_command_queue_flush(command_queue);
  }

error:
  return err;
}

cl_int
clEnqueueCopyBufferToImage(cl_command_queue  command_queue,
                           cl_mem            src_buffer,
                           cl_mem            dst_mem,
                           size_t            src_offset,
                           const size_t *    dst_origin,
                           const size_t *    region,
                           cl_uint           num_events_in_wait_list,
                           const cl_event *  event_wait_list,
                           cl_event *        event)
{
  cl_int err = CL_SUCCESS;
  enqueue_data *data, no_wait_data = { 0 };

  CHECK_QUEUE(command_queue);
  CHECK_MEM(src_buffer);
  CHECK_IMAGE(dst_mem, dst_image);
  if (command_queue->ctx != src_buffer->ctx ||
      command_queue->ctx != dst_mem->ctx) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  if (src_offset + region[0]*region[1]*region[2]*dst_image->bpp > src_buffer->size) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (!dst_origin || !region || dst_origin[0] + region[0] > dst_image->w ||
      dst_origin[1] + region[1] > dst_image->h || dst_origin[2] + region[2] > dst_image->depth) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (dst_image->image_type == CL_MEM_OBJECT_IMAGE2D && (dst_origin[2] != 0 || region[2] != 1)) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  cl_mem_copy_buffer_to_image(command_queue, src_buffer, dst_image, src_offset, dst_origin, region);

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, dst_mem->ctx);

  data = &no_wait_data;
  data->type = EnqueueCopyBufferToImage;
  data->queue = command_queue;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_COPY_BUFFER_TO_IMAGE) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_command_queue_flush(command_queue);
  }

error:
  return err;
}

static cl_int _cl_map_mem(cl_mem mem, void **ptr, void **mem_ptr, size_t offset, size_t size)
{
  cl_int slot = -1;
  int err = CL_SUCCESS;
  if (!(*ptr = cl_mem_map_gtt_unsync(mem))) {
    err = CL_MAP_FAILURE;
    goto error;
  }
  *ptr = (char*)(*ptr) + offset;
  if(mem->flags & CL_MEM_USE_HOST_PTR) {
    assert(mem->host_ptr);
    //only calc ptr here, will do memcpy in enqueue
    *mem_ptr = mem->host_ptr + offset;
  } else {
    *mem_ptr = *ptr;
  }
  /* Record the mapped address. */
  if (!mem->mapped_ptr_sz) {
    mem->mapped_ptr_sz = 16;
    mem->mapped_ptr = (cl_mapped_ptr *)malloc(
          sizeof(cl_mapped_ptr) * mem->mapped_ptr_sz);
    if (!mem->mapped_ptr) {
      cl_mem_unmap_gtt(mem);
      err = CL_OUT_OF_HOST_MEMORY;
      goto error;
    }
    memset(mem->mapped_ptr, 0, mem->mapped_ptr_sz * sizeof(cl_mapped_ptr));
    slot = 0;
  } else {
   int i = 0;
    for (; i < mem->mapped_ptr_sz; i++) {
      if (mem->mapped_ptr[i].ptr == NULL) {
        slot = i;
        break;
      }
   }
    if (i == mem->mapped_ptr_sz) {
      cl_mapped_ptr *new_ptr = (cl_mapped_ptr *)malloc(
          sizeof(cl_mapped_ptr) * mem->mapped_ptr_sz * 2);
      if (!new_ptr) {
        cl_mem_unmap_gtt (mem);
        err = CL_OUT_OF_HOST_MEMORY;
        goto error;
      }
      memset(new_ptr, 0, 2 * mem->mapped_ptr_sz * sizeof(cl_mapped_ptr));
      memcpy(new_ptr, mem->mapped_ptr,
             mem->mapped_ptr_sz * sizeof(cl_mapped_ptr));
      slot = mem->mapped_ptr_sz;
      mem->mapped_ptr_sz *= 2;
      free(mem->mapped_ptr);
      mem->mapped_ptr = new_ptr;
    }
  }
  assert(slot != -1);
  mem->mapped_ptr[slot].ptr = *mem_ptr;
  mem->mapped_ptr[slot].v_ptr = *ptr;
  mem->mapped_ptr[slot].size = size;
  mem->map_ref++;
error:
  if (err != CL_SUCCESS)
    *mem_ptr = NULL;
  return err;
}

void *
clEnqueueMapBuffer(cl_command_queue  command_queue,
                   cl_mem            buffer,
                   cl_bool           blocking_map,
                   cl_map_flags      map_flags,
                   size_t            offset,
                   size_t            size,
                   cl_uint           num_events_in_wait_list,
                   const cl_event *  event_wait_list,
                   cl_event *        event,
                   cl_int *          errcode_ret)
{
  cl_int err = CL_SUCCESS;
  void *ptr = NULL;
  void *mem_ptr = NULL;
  enqueue_data *data, no_wait_data = { 0 };

  CHECK_QUEUE(command_queue);
  CHECK_MEM(buffer);
  if (command_queue->ctx != buffer->ctx) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  if (!size || offset + size > buffer->size) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if ((map_flags & CL_MAP_READ &&
       buffer->flags & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS)) ||
      (map_flags & (CL_MAP_WRITE | CL_MAP_WRITE_INVALIDATE_REGION) &&
       buffer->flags & (CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)))
  {
    err = CL_INVALID_OPERATION;
    goto error;
  }

  err = _cl_map_mem(buffer, &ptr, &mem_ptr, offset, size);
  if (err != CL_SUCCESS)
    goto error;

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, buffer->ctx);

  data = &no_wait_data;
  data->type        = EnqueueMapBuffer;
  data->mem_obj     = buffer;
  data->offset      = offset;
  data->size        = size;
  data->ptr         = ptr;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_MAP_BUFFER) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_enqueue_handle(data);
    if(event) cl_event_set_status(*event, CL_COMPLETE);
  }

error:
  if (errcode_ret)
    *errcode_ret = err;
  return mem_ptr;
}

void *
clEnqueueMapImage(cl_command_queue   command_queue,
                  cl_mem             mem,
                  cl_bool            blocking_map,
                  cl_map_flags       map_flags,
                  const size_t *     origin,
                  const size_t *     region,
                  size_t *           image_row_pitch,
                  size_t *           image_slice_pitch,
                  cl_uint            num_events_in_wait_list,
                  const cl_event *   event_wait_list,
                  cl_event *         event,
                  cl_int *           errcode_ret)
{
  cl_int err = CL_SUCCESS;
  void *ptr  = NULL;
  void *mem_ptr = NULL;
  enqueue_data *data, no_wait_data = { 0 };

  CHECK_QUEUE(command_queue);
  CHECK_IMAGE(mem, image);
  if (command_queue->ctx != mem->ctx) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  if (!origin || !region || origin[0] + region[0] > image->w || origin[1] + region[1] > image->h || origin[2] + region[2] > image->depth) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  if (!image_row_pitch || (image->slice_pitch && !image_slice_pitch)) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  *image_row_pitch = image->row_pitch;
  if (image_slice_pitch)
    *image_slice_pitch = image->slice_pitch;

  if ((map_flags & CL_MAP_READ &&
       mem->flags & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS)) ||
      (map_flags & (CL_MAP_WRITE | CL_MAP_WRITE_INVALIDATE_REGION) &&
       mem->flags & (CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)))
  {
    err = CL_INVALID_OPERATION;
    goto error;
  }

  if (!(ptr = cl_mem_map_gtt_unsync(mem))) {
    err = CL_MAP_FAILURE;
    goto error;
  }

  size_t offset = image->bpp*origin[0] + image->row_pitch*origin[1] + image->slice_pitch*origin[2];
  size_t size;
  if(region[2] == 1) {
    if(region[1] == 1)
      size = image->bpp * region[0];
    else
      size = image->row_pitch * (region[1] - 1) + (image->bpp * (origin[0] + region[0]));
  } else {
    size = image->slice_pitch * (region[2] - 1);
    size += image->row_pitch * (origin[1] + region[1]);
    size += image->bpp * (origin[0] + region[0]);
  }

  err = _cl_map_mem(mem, &ptr, &mem_ptr, offset, size);
  if (err != CL_SUCCESS)
    goto error;

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, mem->ctx);

  data = &no_wait_data;
  data->type        = EnqueueMapImage;
  data->mem_obj     = mem;
  data->origin[0]   = origin[0];  data->origin[1] = origin[1];  data->origin[2] = origin[2];
  data->region[0]   = region[0];  data->region[1] = region[1];  data->region[2] = region[2];
  data->row_pitch   = *image_row_pitch;
  if (image_slice_pitch)
    data->slice_pitch = *image_slice_pitch;
  data->ptr         = ptr;
  data->offset      = offset;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_MAP_IMAGE) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_enqueue_handle(data);
    if(event) cl_event_set_status(*event, CL_COMPLETE);
  }

error:
  if (errcode_ret)
    *errcode_ret = err;
  return mem_ptr; //TODO: map and unmap first
}

cl_int
clEnqueueUnmapMemObject(cl_command_queue  command_queue,
                        cl_mem            memobj,
                        void *            mapped_ptr,
                        cl_uint           num_events_in_wait_list,
                        const cl_event *  event_wait_list,
                        cl_event *        event)
{
  cl_int err = CL_SUCCESS;
  enqueue_data *data, no_wait_data = { 0 };

  CHECK_QUEUE(command_queue);
  CHECK_MEM(memobj);
  if (command_queue->ctx != memobj->ctx) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, memobj->ctx);

  data = &no_wait_data;
  data->type        = EnqueueUnmapMemObject;
  data->mem_obj     = memobj;
  data->ptr         = mapped_ptr;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_UNMAP_MEM_OBJECT) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_enqueue_handle(data);
    if(event) cl_event_set_status(*event, CL_COMPLETE);
  }

error:
  return err;
}

cl_int
clEnqueueNDRangeKernel(cl_command_queue  command_queue,
                       cl_kernel         kernel,
                       cl_uint           work_dim,
                       const size_t *    global_work_offset,
                       const size_t *    global_work_size,
                       const size_t *    local_work_size,
                       cl_uint           num_events_in_wait_list,
                       const cl_event *  event_wait_list,
                       cl_event *        event)
{
  size_t fixed_global_off[] = {0,0,0};
  size_t fixed_global_sz[] = {1,1,1};
  size_t fixed_local_sz[] = {1,1,1};
  cl_int err = CL_SUCCESS;
  cl_uint i;
  enqueue_data *data, no_wait_data = { 0 };

  CHECK_QUEUE(command_queue);
  CHECK_KERNEL(kernel);

  /* Check number of dimensions we have */
  if (UNLIKELY(work_dim == 0 || work_dim > 3)) {
    err = CL_INVALID_WORK_DIMENSION;
    goto error;
  }

  /* We need a work size per dimension */
  if (UNLIKELY(global_work_size == NULL)) {
    err = CL_INVALID_GLOBAL_WORK_SIZE;
    goto error;
  }

  if (global_work_offset != NULL)
    for (i = 0; i < work_dim; ++i) {
      if (UNLIKELY(~0LL - global_work_offset[i] > global_work_size[i])) {
        err = CL_INVALID_GLOBAL_OFFSET;
        goto error;
      }
    }

  /* Local sizes must be non-null and divide global sizes */
  if (local_work_size != NULL)
    for (i = 0; i < work_dim; ++i)
      if (UNLIKELY(local_work_size[i] == 0 || global_work_size[i] % local_work_size[i])) {
        err = CL_INVALID_WORK_GROUP_SIZE;
        goto error;
      }

  /* Queue and kernel must share the same context */
  assert(kernel->program);
  if (command_queue->ctx != kernel->program->ctx) {
    err = CL_INVALID_CONTEXT;
    goto error;
  }

  /* XXX No event right now */
  //FATAL_IF(num_events_in_wait_list > 0, "Events are not supported");
  //FATAL_IF(event_wait_list != NULL, "Events are not supported");
  //FATAL_IF(event != NULL, "Events are not supported");

  if (local_work_size != NULL)
    for (i = 0; i < work_dim; ++i)
      fixed_local_sz[i] = local_work_size[i];
  if (global_work_size != NULL)
    for (i = 0; i < work_dim; ++i)
      fixed_global_sz[i] = global_work_size[i];
  if (global_work_offset != NULL)
    for (i = 0; i < work_dim; ++i)
      fixed_global_off[i] = global_work_offset[i];

  /* Do device specific checks are enqueue the kernel */
  err = cl_command_queue_ND_range(command_queue,
                                  kernel,
                                  work_dim,
                                  fixed_global_off,
                                  fixed_global_sz,
                                  fixed_local_sz);
  if(err != CL_SUCCESS)
    goto error;

  data = &no_wait_data;
  data->type = EnqueueNDRangeKernel;
  data->queue = command_queue;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_NDRANGE_KERNEL) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_command_queue_flush(command_queue);
  }

error:
  return err;
}

cl_int
clEnqueueTask(cl_command_queue   command_queue,
              cl_kernel          kernel,
              cl_uint            num_events_in_wait_list,
              const cl_event *   event_wait_list,
              cl_event *         event)
{
  const size_t global_size[3] = {1, 0, 0};
  const size_t local_size[3]  = {1, 0, 0};

  return clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, global_size, local_size,
                                num_events_in_wait_list, event_wait_list, event);
}

cl_int
clEnqueueNativeKernel(cl_command_queue   command_queue,
                      void (*user_func)(void *),
                      void *             args,
                      size_t             cb_args,
                      cl_uint            num_mem_objects,
                      const cl_mem *     mem_list,
                      const void **      args_mem_loc,
                      cl_uint            num_events_in_wait_list,
                      const cl_event *   event_wait_list,
                      cl_event *         event)
{
  cl_int err = CL_SUCCESS;
  void *new_args = NULL;
  enqueue_data *data, no_wait_data = { 0 };
  cl_int i;

  if(user_func == NULL ||
    (args == NULL && cb_args > 0) ||
    (args == NULL && num_mem_objects ==0) ||
    (args != NULL && cb_args == 0) ||
    (num_mem_objects > 0 && (mem_list == NULL || args_mem_loc == NULL)) ||
    (num_mem_objects == 0 && (mem_list != NULL || args_mem_loc != NULL))) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  //Per spec, need copy args
  if (cb_args)
  {
    new_args = malloc(cb_args);
    if (!new_args)
    {
      err = CL_OUT_OF_HOST_MEMORY;
      goto error;
    }
    memcpy(new_args, args, cb_args);

    for (i=0; i<num_mem_objects; ++i)
    {
      CHECK_MEM(mem_list[i]);
      args_mem_loc[i] = new_args + (args_mem_loc[i] - args);  //change to new args
    }
  }

  TRY(cl_event_check_waitlist, num_events_in_wait_list, event_wait_list, event, command_queue->ctx);

  data = &no_wait_data;
  data->type        = EnqueueNativeKernel;
  data->mem_list    = mem_list;
  data->ptr         = new_args;
  data->size        = cb_args;
  data->offset      = (size_t)num_mem_objects;
  data->const_ptr   = args_mem_loc;
  data->user_func   = user_func;

  if(handle_events(command_queue, num_events_in_wait_list, event_wait_list,
                   event, data, CL_COMMAND_NATIVE_KERNEL) == CL_ENQUEUE_EXECUTE_IMM) {
    err = cl_enqueue_handle(data);
    if(event) cl_event_set_status(*event, CL_COMPLETE);
  }

error:
  return err;
}

cl_int
clEnqueueMarker(cl_command_queue     command_queue,
                cl_event *           event)
{
  cl_int err = CL_SUCCESS;
  CHECK_QUEUE(command_queue);
  if(event == NULL) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  cl_event_marker(command_queue, event);
error:
  return err;
}

cl_int
clEnqueueWaitForEvents(cl_command_queue  command_queue,
                       cl_uint           num_events,
                       const cl_event *  event_list)
{
  cl_int err = CL_SUCCESS;
  CHECK_QUEUE(command_queue);
  err = clWaitForEvents(num_events, event_list);

error:
  return err;
}

cl_int
clEnqueueBarrier(cl_command_queue  command_queue)
{
  cl_int err = CL_SUCCESS;
  CHECK_QUEUE(command_queue);
  cl_command_queue_set_barrier(command_queue);

error:
  return err;
}

#define EXTFUNC(x)                      \
  if (strcmp(#x, func_name) == 0)       \
    return (void *)x;

void*
clGetExtensionFunctionAddress(const char *func_name)
{
  if (func_name == NULL)
    return NULL;
#ifdef HAS_OCLIcd
  /* cl_khr_icd */
  EXTFUNC(clIcdGetPlatformIDsKHR)
#endif
  EXTFUNC(clCreateProgramWithLLVMIntel)
  EXTFUNC(clGetGenVersionIntel)
  EXTFUNC(clMapBufferIntel)
  EXTFUNC(clUnmapBufferIntel)
  EXTFUNC(clMapBufferGTTIntel)
  EXTFUNC(clUnmapBufferGTTIntel)
  EXTFUNC(clPinBufferIntel)
  EXTFUNC(clUnpinBufferIntel)
  EXTFUNC(clReportUnfreedIntel)
  EXTFUNC(clCreateBufferFromLibvaIntel)
  EXTFUNC(clCreateImageFromLibvaIntel)
  return NULL;
}

#undef EXTFUNC

cl_int
clReportUnfreedIntel(void)
{
  return cl_report_unfreed();
}

void*
clMapBufferIntel(cl_mem mem, cl_int *errcode_ret)
{
  void *ptr = NULL;
  cl_int err = CL_SUCCESS;
  CHECK_MEM (mem);
  ptr = cl_mem_map(mem);
error:
  if (errcode_ret)
    *errcode_ret = err;
  return ptr;
}

cl_int
clUnmapBufferIntel(cl_mem mem)
{
  cl_int err = CL_SUCCESS;
  CHECK_MEM (mem);
  err = cl_mem_unmap(mem);
error:
  return err;
}

void*
clMapBufferGTTIntel(cl_mem mem, cl_int *errcode_ret)
{
  void *ptr = NULL;
  cl_int err = CL_SUCCESS;
  CHECK_MEM (mem);
  ptr = cl_mem_map_gtt(mem);
error:
  if (errcode_ret)
    *errcode_ret = err;
  return ptr;
}

cl_int
clUnmapBufferGTTIntel(cl_mem mem)
{
  cl_int err = CL_SUCCESS;
  CHECK_MEM (mem);
  err = cl_mem_unmap_gtt(mem);
error:
  return err;
}

cl_int
clPinBufferIntel(cl_mem mem)
{
  cl_int err = CL_SUCCESS;
  CHECK_MEM (mem);
  cl_mem_pin(mem);
error:
  return err;
}

cl_int
clUnpinBufferIntel(cl_mem mem)
{
  cl_int err = CL_SUCCESS;
  CHECK_MEM (mem);
  cl_mem_unpin(mem);
error:
  return err;
}

cl_int
clGetGenVersionIntel(cl_device_id device, cl_int *ver)
{
  return cl_device_get_version(device, ver);
}

cl_program
clCreateProgramWithLLVMIntel(cl_context              context,
                             cl_uint                 num_devices,
                             const cl_device_id *    devices,
                             const char *            filename,
                             cl_int *                errcode_ret)
{
  return cl_program_create_from_llvm(context,
                                     num_devices,
                                     devices,
                                     filename,
                                     errcode_ret);
}

cl_mem
clCreateBufferFromLibvaIntel(cl_context  context,
                             unsigned int bo_name,
                             cl_int *errorcode_ret)
{
  cl_mem mem = NULL;
  cl_int err = CL_SUCCESS;
  CHECK_CONTEXT (context);

  mem = cl_mem_new_libva_buffer(context, bo_name, &err);

error:
  if (errorcode_ret)
    *errorcode_ret = err;
  return mem;
}

cl_mem
clCreateImageFromLibvaIntel(cl_context context,
                            const cl_libva_image *info,
                            cl_int *errorcode_ret)
{
  cl_mem mem = NULL;
  cl_int err = CL_SUCCESS;
  CHECK_CONTEXT (context);

  if (!info) {
    err = CL_INVALID_VALUE;
    goto error;
  }

  mem = cl_mem_new_libva_image(context,
                               info->bo_name, info->offset, info->width, info->height,
                               info->fmt, info->row_pitch,
                               &err);

error:
  if (errorcode_ret)
    *errorcode_ret = err;
  return mem;
}

