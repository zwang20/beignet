#include <cstring>
#include "utest_helper.hpp"
static void runtime_pipe_query() {
  if(!cl_check_ocl20(false))
    return;
  const size_t w = 16;
  const size_t sz = 8;
  cl_uint retnum, retsz;
  /* pipe write kernel */
  OCL_CALL2(clCreatePipe, buf[0], ctx, 0, sz, w, nullptr);
  OCL_CALL(clGetPipeInfo, buf[0], CL_PIPE_MAX_PACKETS, sizeof(retnum), &retnum, nullptr);
  OCL_CALL(clGetPipeInfo, buf[0], CL_PIPE_PACKET_SIZE, sizeof(retsz), &retsz, nullptr);

  /*Check result */
  OCL_ASSERT(sz == retsz && w == retnum);
}
MAKE_UTEST_FROM_FUNCTION(runtime_pipe_query);
