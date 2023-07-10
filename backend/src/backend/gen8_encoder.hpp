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
 */

/**
 * \file gen8_context.hpp
 */
#ifndef __GBE_GEN8_ENCODER_HPP__
#define __GBE_GEN8_ENCODER_HPP__

#include "backend/gen_encoder.hpp"

namespace gbe
{
  /* This class is used to implement the HSW
     specific logic for encoder. */
  class Gen8Encoder : public GenEncoder
  {
  public:
    ~Gen8Encoder(void) override { }

    Gen8Encoder(uint32_t simdWidth, uint32_t gen, uint32_t deviceID)
         : GenEncoder(simdWidth, gen, deviceID) { }

    /*! Jump indexed instruction */
    void JMPI(GenRegister src, bool longjmp = false) override;
    void FENCE(GenRegister dst, bool flushRWCache) override;
    /*! Patch JMPI/BRC/BRD (located at index insnID) with the given jump distance */
    void patchJMPI(uint32_t insnID, int32_t jip, int32_t uip) override;
    void F16TO32(GenRegister dest, GenRegister src0) override;
    void F32TO16(GenRegister dest, GenRegister src0) override;
    void LOAD_INT64_IMM(GenRegister dest, GenRegister value) override;
    bool needToSplitCmpBySrcType(GenEncoder *p, GenRegister src0, GenRegister src1) override;
    void ATOMIC(GenRegister dst, uint32_t function, GenRegister addr, GenRegister data, GenRegister bti, uint32_t srcNum, bool useSends) override;
    void ATOMICA64(GenRegister dst, uint32_t function, GenRegister src, GenRegister bti, uint32_t srcNum) override;
    void UNTYPED_READ(GenRegister dst, GenRegister src, GenRegister bti, uint32_t elemNum) override;
    void UNTYPED_WRITE(GenRegister src, GenRegister data, GenRegister bti, uint32_t elemNum, bool useSends) override;
    void UNTYPED_READA64(GenRegister dst, GenRegister src, uint32_t elemNum) override;
    void UNTYPED_WRITEA64(GenRegister src, uint32_t elemNum) override;
    void BYTE_GATHERA64(GenRegister dst, GenRegister src, uint32_t elemSize) override;
    void BYTE_SCATTERA64(GenRegister src, uint32_t elemSize) override;
    void setHeader(GenNativeInstruction *insn) override;
    void setDPUntypedRW(GenNativeInstruction *insn, uint32_t bti, uint32_t rgba,
                   uint32_t msg_type, uint32_t msg_length, uint32_t response_length) override;
    void setTypedWriteMessage(GenNativeInstruction *insn, unsigned char bti,
                                      unsigned char msg_type, uint32_t msg_length,
                                      bool header_present) override;
    void FLUSH_SAMPLERCACHE(GenRegister dst) override;
    void setDst(GenNativeInstruction *insn, GenRegister dest) override;
    void setSrc0(GenNativeInstruction *insn, GenRegister reg) override;
    void setSrc1(GenNativeInstruction *insn, GenRegister reg) override;
    uint32_t getCompactVersion() override { return 8; }
    void alu3(uint32_t opcode, GenRegister dst,
                       GenRegister src0, GenRegister src1, GenRegister src2) override;
    bool canHandleLong(uint32_t opcode, GenRegister dst, GenRegister src0,
                            GenRegister src1 = GenRegister::null()) override;
    void handleDouble(GenEncoder *p, uint32_t opcode, GenRegister dst, GenRegister src0, GenRegister src1 = GenRegister::null()) override;
    unsigned setAtomicMessageDesc(GenNativeInstruction *insn, unsigned function, unsigned bti, unsigned srcNum) override;
    unsigned setAtomicA64MessageDesc(GenNativeInstruction *insn, unsigned function, unsigned bti, unsigned srcNum, int type_long) override;
    unsigned setUntypedReadMessageDesc(GenNativeInstruction *insn, unsigned bti, unsigned elemNum) override;
    unsigned setUntypedWriteMessageDesc(GenNativeInstruction *insn, unsigned bti, unsigned elemNum) override;
    static void setSrc0WithAcc(GenNativeInstruction *insn, GenRegister reg, uint32_t accN);
    static void setSrc1WithAcc(GenNativeInstruction *insn, GenRegister reg, uint32_t accN);

    void MATH_WITH_ACC(GenRegister dst, uint32_t function, GenRegister src0, GenRegister src1,
                       uint32_t dstAcc, uint32_t src0Acc, uint32_t src1Acc);
    void MADM(GenRegister dst, GenRegister src0, GenRegister src1, GenRegister src2,
              uint32_t dstAcc, uint32_t src0Acc, uint32_t src1Acc, uint32_t src2Acc);
    /*! A64 OBlock read */
    void OBREADA64(GenRegister dst, GenRegister header, uint32_t bti, uint32_t elemSize) override;
    /*! A64 OBlock write */
    void OBWRITEA64(GenRegister header, uint32_t bti, uint32_t elemSize) override;
  };
}
#endif /* __GBE_GEN8_ENCODER_HPP__ */
