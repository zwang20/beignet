/* 
 * Copyright © 2012 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#ifndef __OPENCL_CL_INTEL_H
#define __OPENCL_CL_INTEL_H

#include "CL/cl.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__CL_EXT_H) && !defined(cl_intel_accelerator)
#ifdef CL_VERSION_2_2
#include "CL/cl_ext_intel.h"
#else
/*********************************
* cl_intel_accelerator extension *
*********************************/
#define cl_intel_accelerator 1
#define cl_intel_motion_estimation 1

typedef struct _cl_accelerator_intel*     cl_accelerator_intel;
typedef cl_uint                           cl_accelerator_type_intel;
typedef cl_uint                           cl_accelerator_info_intel;

typedef struct _cl_motion_estimation_desc_intel {
    cl_uint mb_block_type;
    cl_uint subpixel_mode;
    cl_uint sad_adjust_mode;
    cl_uint search_path_type;
} cl_motion_estimation_desc_intel;

/* Error Codes */
#define CL_INVALID_ACCELERATOR_INTEL            -1094
#define CL_INVALID_ACCELERATOR_TYPE_INTEL       -1095
#define CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL -1096
#define CL_ACCELERATOR_TYPE_NOT_SUPPORTED_INTEL -1097

/* Deprecated Error Codes */
#define CL_INVALID_ACCELERATOR_INTEL_DEPRECATED            -6000
#define CL_INVALID_ACCELERATOR_TYPE_INTEL_DEPRECATED       -6001
#define CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL_DEPRECATED -6002
#define CL_ACCELERATOR_TYPE_NOT_SUPPORTED_INTEL_DEPRECATED -6003

/* cl_accelerator_type_intel */
#define CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL     0x0

/* cl_accelerator_info_intel */
#define CL_ACCELERATOR_DESCRIPTOR_INTEL                 0x4090
#define CL_ACCELERATOR_REFERENCE_COUNT_INTEL            0x4091
#define CL_ACCELERATOR_CONTEXT_INTEL                    0x4092
#define CL_ACCELERATOR_TYPE_INTEL                       0x4093

/*cl_motion_detect_desc_intel flags */
#define CL_ME_MB_TYPE_16x16_INTEL                       0x0
#define CL_ME_MB_TYPE_8x8_INTEL                         0x1
#define CL_ME_MB_TYPE_4x4_INTEL                         0x2

#define CL_ME_SUBPIXEL_MODE_INTEGER_INTEL               0x0
#define CL_ME_SUBPIXEL_MODE_HPEL_INTEL                  0x1
#define CL_ME_SUBPIXEL_MODE_QPEL_INTEL                  0x2

#define CL_ME_SAD_ADJUST_MODE_NONE_INTEL                0x0
#define CL_ME_SAD_ADJUST_MODE_HAAR_INTEL                0x1

#define CL_ME_SEARCH_PATH_RADIUS_2_2_INTEL              0x0
#define CL_ME_SEARCH_PATH_RADIUS_4_4_INTEL              0x1
#define CL_ME_SEARCH_PATH_RADIUS_16_12_INTEL            0x5

extern CL_API_ENTRY cl_accelerator_intel CL_API_CALL
clCreateAcceleratorINTEL(
    cl_context                  /* context */,
    cl_accelerator_type_intel   /* accelerator_type */,
    size_t                      /* descriptor_size */,
    const void*                 /* descriptor */,
    cl_int*                     /* errcode_ret */ ) CL_EXT_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_accelerator_intel
    (CL_API_CALL *clCreateAcceleratorINTEL_fn)(
    cl_context                  /* context */,
    cl_accelerator_type_intel   /* accelerator_type */,
    size_t                      /* descriptor_size */,
    const void*                 /* descriptor */,
    cl_int*                     /* errcode_ret */ ) CL_EXT_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_int CL_API_CALL
clGetAcceleratorInfoINTEL
(
    cl_accelerator_intel        /* accelerator */,
    cl_accelerator_info_intel   /* param_name */,
    size_t                      /* param_value_size */,
    void*                       /* param_value */,
    size_t*                     /* param_value_size_ret */ ) CL_EXT_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int
    (CL_API_CALL *clGetAcceleratorInfoINTEL_fn)(
    cl_accelerator_intel        /* accelerator */,
    cl_accelerator_info_intel   /* param_name */,
    size_t                      /* param_value_size */,
    void*                       /* param_value */,
    size_t*                     /* param_value_size_ret */ ) CL_EXT_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_int CL_API_CALL
clRetainAcceleratorINTEL(
    cl_accelerator_intel        /* accelerator */ ) CL_EXT_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int
    (CL_API_CALL *clRetainAcceleratorINTEL_fn)(
    cl_accelerator_intel        /* accelerator */ ) CL_EXT_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_int CL_API_CALL
clReleaseAcceleratorINTEL(
    cl_accelerator_intel        /* accelerator */ ) CL_EXT_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int
    (CL_API_CALL *clReleaseAcceleratorINTEL_fn)(
    cl_accelerator_intel        /* accelerator */ ) CL_EXT_SUFFIX__VERSION_1_2;
#endif
#endif

#define CL_MEM_PINNABLE (1 << 10)

/* Track allocations and report current number of unfreed allocations */
extern CL_API_ENTRY cl_int CL_API_CALL
clReportUnfreedIntel(void);

typedef CL_API_ENTRY cl_int (CL_API_CALL *clReportUnfreedIntel_fn)(void);

/* 1 to 1 mapping of drm_intel_bo_map */
extern CL_API_ENTRY void* CL_API_CALL
clMapBufferIntel(cl_mem, cl_int*);

typedef CL_API_ENTRY void* (CL_API_CALL *clMapBufferIntel_fn)(cl_mem, cl_int*);

/* 1 to 1 mapping of drm_intel_bo_unmap */
extern CL_API_ENTRY cl_int CL_API_CALL
clUnmapBufferIntel(cl_mem);

typedef CL_API_ENTRY cl_int (CL_API_CALL *clUnmapBufferIntel_fn)(cl_mem);

/* 1 to 1 mapping of drm_intel_gem_bo_map_gtt */
extern CL_API_ENTRY void* CL_API_CALL
clMapBufferGTTIntel(cl_mem, cl_int*);

typedef CL_API_ENTRY void* (CL_API_CALL *clMapBufferGTTIntel_fn)(cl_mem, cl_int*);

/* 1 to 1 mapping of drm_intel_gem_bo_unmap_gtt */
extern CL_API_ENTRY cl_int CL_API_CALL
clUnmapBufferGTTIntel(cl_mem);

typedef CL_API_ENTRY cl_int (CL_API_CALL *clUnmapBufferGTTIntel_fn)(cl_mem);

/* Pin /Unpin the buffer in GPU memory (must be root) */
extern CL_API_ENTRY cl_int CL_API_CALL
clPinBufferIntel(cl_mem);
extern CL_API_ENTRY cl_int CL_API_CALL
clUnpinBufferIntel(cl_mem);

typedef CL_API_ENTRY cl_int (CL_API_CALL *clPinBufferIntel_fn)(cl_mem);
typedef CL_API_ENTRY cl_int (CL_API_CALL *clUnpinBufferIntel_fn)(cl_mem);

/* Get the generation of the Gen device (used to load the proper binary) */
extern CL_API_ENTRY cl_int CL_API_CALL
clGetGenVersionIntel(cl_device_id device, cl_int *ver);

typedef CL_API_ENTRY cl_int (CL_API_CALL *clGetGenVersionIntel_fn)(
                             cl_device_id device,
                             cl_int *ver);

/* Create a program from a LLVM source file */
extern CL_API_ENTRY cl_program CL_API_CALL
clCreateProgramWithLLVMIntel(cl_context              /* context */,
                             cl_uint                 /* num_devices */,
                             const cl_device_id *    /* device_list */,
                             const char *            /* file */,
                             cl_int *                /* errcode_ret */);

typedef CL_API_ENTRY cl_program (CL_API_CALL *clCreateProgramWithLLVMIntel_fn)(
                                 cl_context              /* context */,
                                 cl_uint                 /* num_devices */,
                                 const cl_device_id *    /* device_list */,
                                 const char *            /* file */,
                                 cl_int *                /* errcode_ret */);

/* Create buffer from libva's buffer object */
extern CL_API_ENTRY cl_mem CL_API_CALL
clCreateBufferFromLibvaIntel(cl_context      /* context */,
                             unsigned int    /* bo_name */,
                             cl_int *        /* errcode_ret */);

typedef CL_API_ENTRY cl_mem (CL_API_CALL *clCreateBufferFromLibvaIntel_fn)(
                             cl_context     /* context */,
                             unsigned int   /* bo_name */,
                             cl_int *       /* errcode_ret */);

/* Create image from libva's buffer object */
typedef struct _cl_libva_image {
    unsigned int            bo_name;
    uint32_t                offset;
    uint32_t                width;
    uint32_t                height;
    cl_image_format         fmt;
    uint32_t                row_pitch;
    uint32_t                reserved[8];
} cl_libva_image;

extern CL_API_ENTRY cl_mem CL_API_CALL
clCreateImageFromLibvaIntel(cl_context               /* context */,
                            const cl_libva_image *   /* info */,
                            cl_int *                 /* errcode_ret */);

typedef CL_API_ENTRY cl_mem (CL_API_CALL *clCreateImageFromLibvaIntel_fn)(
                             cl_context             /* context */,
                             const cl_libva_image * /* info */,
                             cl_int *               /* errcode_ret */);

/* Create buffer from libva's buffer object */
extern CL_API_ENTRY cl_int CL_API_CALL
clGetMemObjectFdIntel(cl_context   /* context */,
                      cl_mem       /* Memory Obejct */,
                      int*         /* returned fd */);

typedef CL_API_ENTRY cl_int (CL_API_CALL *clGetMemObjectFdIntel_fn)(
                             cl_context   /* context */,
                             cl_mem       /* Memory Obejct */,
                             int*         /* returned fd */);

typedef struct _cl_import_buffer_info_intel {
    int                     fd;
    int                     size;
} cl_import_buffer_info_intel;

typedef struct _cl_import_image_info_intel {
    int                     fd;
    int                     size;
    cl_mem_object_type      type;
    cl_image_format         fmt;
    uint32_t                offset;
    uint32_t                width;
    uint32_t                height;
    uint32_t                row_pitch;
} cl_import_image_info_intel;

/* Create memory object from external buffer object by fd */
extern CL_API_ENTRY cl_mem CL_API_CALL
clCreateBufferFromFdINTEL(cl_context                            /* context */,
                          const cl_import_buffer_info_intel *   /* info */,
                          cl_int *                              /* errcode_ret */);

typedef CL_API_ENTRY cl_mem (CL_API_CALL *clCreateBufferFromFdINTEL_fn)(
                             cl_context                            /* context */,
                             const cl_import_buffer_info_intel *   /* info */,
                             cl_int *                              /* errcode_ret */);

extern CL_API_ENTRY cl_mem CL_API_CALL
clCreateImageFromFdINTEL(cl_context                            /* context */,
                         const cl_import_image_info_intel *    /* info */,
                         cl_int *                              /* errcode_ret */);

typedef CL_API_ENTRY cl_mem (CL_API_CALL *clCreateImageFromFdINTEL_fn)(
                             cl_context                            /* context */,
                             const cl_import_image_info_intel *    /* info */,
                             cl_int *                              /* errcode_ret */);

#ifndef CL_VERSION_2_0
typedef cl_uint  cl_kernel_sub_group_info;

/* cl_khr_sub_group_info */
#define CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE_KHR	0x2033
#define CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE_KHR		0x2034

extern CL_API_ENTRY cl_int CL_API_CALL
clGetKernelSubGroupInfoKHR(cl_kernel /* in_kernel */,
						   cl_device_id /*in_device*/,
						   cl_kernel_sub_group_info /* param_name */,
						   size_t /*input_value_size*/,
						   const void * /*input_value*/,
						   size_t /*param_value_size*/,
						   void* /*param_value*/,
						   size_t* /*param_value_size_ret*/ );

typedef CL_API_ENTRY cl_int
     ( CL_API_CALL * clGetKernelSubGroupInfoKHR_fn)(cl_kernel /* in_kernel */,
						      cl_device_id /*in_device*/,
						      cl_kernel_sub_group_info /* param_name */,
						      size_t /*input_value_size*/,
						      const void * /*input_value*/,
						      size_t /*param_value_size*/,
						      void* /*param_value*/,
						      size_t* /*param_value_size_ret*/ );
#endif

/* cl_intel_required_subgroup_size extension*/
#define CL_DEVICE_SUB_GROUP_SIZES_INTEL                 0x4108
#define CL_KERNEL_SPILL_MEM_SIZE_INTEL                  0x4109
#define CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL          0x410A

#ifdef __cplusplus
}
#endif

#endif /* __OPENCL_CL_INTEL_H */

