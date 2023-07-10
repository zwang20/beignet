#include "utest_helper.hpp"
#include <cstring>

const size_t w = 80;
const size_t h = 48;
const size_t mv_w = (w + 15) / 16;
const size_t mv_h = (h + 15) / 16;

void cpu_result(uint8_t *srcImg, uint8_t *refImg, int16_t *mv, uint16_t *residual)
{
  for (uint32_t j = 0; j <= mv_h - 1; ++j) {
    for (uint32_t i = 0; i <= mv_w - 1; ++i) {
      uint32_t mv_num = j * mv_w + i;
      int16_t mv_x = mv[mv_num * 2] >> 2;
      int16_t mv_y = mv[mv_num * 2 + 1] >> 2;
      int16_t src_mb_x = i * 16;
      int16_t src_mb_y = j * 16;
      int16_t ref_mb_x = src_mb_x + mv_x;
      int16_t ref_mb_y = src_mb_y + mv_y;

      uint16_t res = 0;
      int16_t sy = src_mb_y, ry = ref_mb_y;
      for (uint32_t a = 0; a < 16; a++) {
        int16_t sx = src_mb_x;
        int16_t rx = ref_mb_x;
        for (uint32_t b = 0; b < 16; b++) {
          uint8_t src_pixel = srcImg[sy * w + sx];
          uint8_t ref_pixel = refImg[ry * w + rx];
          res += abs(src_pixel - ref_pixel);
          sx++;
          rx++;
        }
        sy++;
        ry++;
      }
      residual[mv_num] = res;
    }
  }
}

void compiler_skip_check(void) {
  if (!cl_check_device_side_avc_motion_estimation()) {
    return;
  }
  if (!cl_check_reqd_subgroup())
    return;

  OCL_CREATE_KERNEL("compiler_skip_check");

  cl_image_format format;
  cl_image_desc desc;

  memset(&desc, 0x0, sizeof(cl_image_desc));
  memset(&format, 0x0, sizeof(cl_image_format));

  uint8_t *image_data1 = (uint8_t *)malloc(w * h); // src
  uint8_t *image_data2 = (uint8_t *)malloc(w * h); // ref
  for (size_t j = 0; j < h; j++) {
    for (size_t i = 0; i < w; i++) {
      if (i >= 32 && i <= 47 && j >= 16 && j <= 31)
        image_data1[w * j + i] = 100;
      else
        image_data1[w * j + i] = j + i;
      if (i >= 33 && i <= 48 && j >= 14 && j <= 29)
        image_data2[w * j + i] = 99;
      else
        image_data2[w * j + i] = (h - 1) + (w - 1) - (j + i);
    }
  }

  format.image_channel_order = CL_R;
  format.image_channel_data_type = CL_UNORM_INT8;
  desc.image_type = CL_MEM_OBJECT_IMAGE2D;
  desc.image_width = w;
  desc.image_height = h;
  desc.image_row_pitch = 0;
  OCL_CREATE_IMAGE(buf[0], CL_MEM_COPY_HOST_PTR, &format, &desc, image_data1); // src
  OCL_CREATE_IMAGE(buf[1], CL_MEM_COPY_HOST_PTR, &format, &desc, image_data2); // ref

  int16_t *input_mv = (int16_t *)malloc(mv_w * mv_h * sizeof(int16_t) * 2);
  // Generate input mv data
  for (uint32_t j = 0; j <= mv_h - 1; ++j) {
    for (uint32_t i = 0; i <= mv_w - 1; ++i) {
      uint32_t mv_num = j * mv_w + i;
      if (i == 32 / 16 && j == 16 / 16) {
        input_mv[mv_num * 2] = 1;
        input_mv[mv_num * 2 + 1] = -2;
      } else {
        input_mv[mv_num * 2] = (mv_num) % 2;
        input_mv[mv_num * 2 + 1] = (mv_num) % 3;
        if (i == mv_w - 1)
          input_mv[mv_num * 2] *= -1;
        if (j == mv_h - 1)
          input_mv[mv_num * 2 + 1] *= -1;
      }
      input_mv[mv_num * 2] <<= 2;
      input_mv[mv_num * 2 + 1] <<= 2;
    }
  }

  uint16_t *cpu_resi = (uint16_t *)malloc(mv_w * mv_h * sizeof(uint16_t));
  cpu_result(image_data1, image_data2, input_mv, cpu_resi);

  OCL_CREATE_BUFFER(buf[2], CL_MEM_COPY_HOST_PTR, mv_w * mv_h * sizeof(int16_t) * 2, input_mv);
  OCL_CREATE_BUFFER(buf[3], 0, mv_w * mv_h * sizeof(uint16_t), NULL);
  OCL_CREATE_BUFFER(buf[4], 0, mv_w * mv_h * sizeof(uint32_t) * 16 * 8, NULL);
  OCL_CREATE_BUFFER(buf[5], 0, mv_w * mv_h * sizeof(uint32_t) * 8 * 8, NULL);

  OCL_SET_ARG(0, sizeof(cl_mem), &buf[0]);
  OCL_SET_ARG(1, sizeof(cl_mem), &buf[1]);
  OCL_SET_ARG(2, sizeof(cl_mem), &buf[2]);
  OCL_SET_ARG(3, sizeof(cl_mem), &buf[3]);
  OCL_SET_ARG(4, sizeof(cl_mem), &buf[4]);
  OCL_SET_ARG(5, sizeof(cl_mem), &buf[5]);

  globals[0] = w;
  globals[1] = h / 16;
  locals[0] = 16;
  locals[1] = 1;
  OCL_NDRANGE(2);

  OCL_MAP_BUFFER(3);
  OCL_MAP_BUFFER(4);
  OCL_MAP_BUFFER(5);
  auto *residual = (uint16_t *)buf_data[3];
#define VME_DEBUG 0
#if VME_DEBUG
  uint32_t *dwo = (uint32_t *)buf_data[4];
  uint32_t *pld = (uint32_t *)buf_data[5];
  std::cout << std::endl;
  for (uint32_t j = 0; j <= mv_h - 1; ++j) {
    for (uint32_t i = 0; i <= mv_w - 1; ++i) {
      uint32_t mv_num = j * mv_w + i;
      std::cout << "******* mv num = " << mv_num << ": " << std::endl;
      std::cout << "payload register result: " << std::endl;
      for (uint32_t row_num = 0; row_num < 8; row_num++) {
        for (int32_t idx = 7; idx >= 0; idx--)
          printf("%.8x ", pld[mv_num * 64 + row_num * 8 + idx]);
        printf("\n");
      }
      std::cout << std::endl;
      std::cout << "writeback register result: " << std::endl;
      for (uint32_t row_num = 0; row_num < 4; row_num++) {
        for (int32_t wi = 7; wi >= 0; wi--)
          printf("%.8x ", dwo[mv_num * 16 * 4 + row_num * 16 + wi]);
        printf("\n");
        for (int32_t wi = 15; wi >= 8; wi--)
          printf("%.8x ", dwo[mv_num * 16 * 4 + row_num * 16 + wi]);
        printf("\n");
      }
      std::cout << std::endl;
      std::cout << "residual: ";
      std::cout << residual[mv_num] << std::endl;
    }
  }
  std::cout << "cpu residual: " << std::endl;
  for (uint32_t j = 0; j <= mv_h - 1; ++j) {
    for (uint32_t i = 0; i <= mv_w - 1; ++i) {
      uint32_t mv_num = j * mv_w + i;
      std::cout << cpu_resi[mv_num] << " ";
    }
  }
  std::cout << std::endl;
  std::cout << "gpu residual: " << std::endl;
  for (uint32_t j = 0; j <= mv_h - 1; ++j) {
    for (uint32_t i = 0; i <= mv_w - 1; ++i) {
      uint32_t mv_num = j * mv_w + i;
      std::cout << residual[mv_num] << " ";
    }
  }
#endif
  std::cout << std::endl;
  for (uint32_t j = 0; j <= mv_h - 1; ++j) {
    for (uint32_t i = 0; i <= mv_w - 1; ++i) {
      uint32_t mv_num = j * mv_w + i;
      OCL_ASSERT(cpu_resi[mv_num] == residual[mv_num]);
    }
  }

  OCL_UNMAP_BUFFER(3);
  OCL_UNMAP_BUFFER(4);
  OCL_UNMAP_BUFFER(5);

  free(image_data1);
  free(image_data2);
  free(input_mv);
  free(cpu_resi);
}

MAKE_UTEST_FROM_FUNCTION(compiler_skip_check);
