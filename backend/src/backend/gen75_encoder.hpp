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
 * \file gen75_context.hpp
 */
#ifndef __GBE_GEN75_ENCODER_HPP__
#define __GBE_GEN75_ENCODER_HPP__

#include "backend/gen_encoder.hpp"
#include "backend/gen7_encoder.hpp"


namespace gbe
{
  /* This class is used to implement the HSW
     specific logic for encoder. */
  class Gen75Encoder : public Gen7Encoder
  {
  public:
    ~Gen75Encoder(void) override { }

    Gen75Encoder(uint32_t simdWidth, uint32_t gen, uint32_t deviceID)
         : Gen7Encoder(simdWidth, gen, deviceID) { }

    /*! Jump indexed instruction */
    void JMPI(GenRegister src, bool longjmp = false) override;
    /*! Patch JMPI/BRC/BRD (located at index insnID) with the given jump distance */
    void patchJMPI(uint32_t insnID, int32_t jip, int32_t uip) override;
    virtual void ATOMIC(GenRegister dst, uint32_t function, GenRegister src, GenRegister bti, uint32_t srcNum, bool useSends);
    void UNTYPED_READ(GenRegister dst, GenRegister src, GenRegister bti, uint32_t elemNum) override;
    void UNTYPED_WRITE(GenRegister src, GenRegister data, GenRegister bti, uint32_t elemNum, bool useSends) override;
    void setHeader(GenNativeInstruction *insn) override;
    void setDPUntypedRW(GenNativeInstruction *insn, uint32_t bti, uint32_t rgba,
                   uint32_t msg_type, uint32_t msg_length, uint32_t response_length) override;
    void setTypedWriteMessage(GenNativeInstruction *insn, unsigned char bti,
                                      unsigned char msg_type, uint32_t msg_length,
                                      bool header_present) override;
    unsigned setAtomicMessageDesc(GenNativeInstruction *insn, unsigned function, unsigned bti, unsigned srcNum) override;
    unsigned setUntypedReadMessageDesc(GenNativeInstruction *insn, unsigned bti, unsigned elemNum) override;
    unsigned setUntypedWriteMessageDesc(GenNativeInstruction *insn, unsigned bti, unsigned elemNum) override;
  };
}
#endif /* __GBE_GEN75_ENCODER_HPP__ */
