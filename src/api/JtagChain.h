#ifndef LIBEDR_API_JTAGCHAIN_H
#define LIBEDR_API_JTAGCHAIN_H

#include "Common.h"
#include "Context.h"
#include "Error.h"

#include "api/Jtag.h"
#include "libedr/driver/Driver.hpp"
#include "libedr/driver/jtag/JtagChain.h"
#include "libedr/driver/jtag/JtagChainAction.hpp"
#include "libedr/util/adt/BitStream.hpp"

enum class JCState : uint32_t {
  TestLogicReset = 0,
  RunTestIdle = 1,

  SelectDR = 2,
  CaptureDR = 3,
  ShiftDR = 4,
  Exit1DR = 5,
  PauseDR = 6,
  Exit2DR = 7,
  UpdateDR = 8,

  SelectIR = 9,
  CaptureIR = 10,
  ShiftIR = 11,
  Exit1IR = 12,
  PauseIR = 13,
  Exit2IR = 14,
  UpdateIR = 15,
};

class JtagChainTransaction {
  TRANSACTION_BODY(JtagChain);

public:
  void ForgetChainStructure() {
    if (!m_builder)
      return;

    m_builder->Add<edr::JCForgetChainStructure>();
  }

  void SetIRLength(uint32_t tap_id, uint32_t ir_length) {
    if (!m_builder)
      return;

    m_builder->Add<edr::JCSetIRLength>(tap_id, ir_length);
  }

  void GoToState(JCState state, uint32_t wait_for_cycles) {
    if (!m_builder)
      return;

    m_builder->Add<edr::JCGoToState>(static_cast<edr::JCState>(state),
                                     wait_for_cycles);
  }

  void SelectTAP(uint32_t tap_id) {
    if (!m_builder)
      return;

    m_builder->Add<edr::JCSelectTAP>(tap_id);
  }

  void WriteIR(uint32_t value) {
    if (!m_builder)
      return;

    m_builder->Add<edr::JCWriteIR>(value);
  }

  void WriteDR(uint64_t data, uint32_t num_data_bits) {
    if (!m_builder)
      return;

    m_builder->Add<edr::JCWriteDR>(data, num_data_bits);
  }

  void ShiftDR(uint64_t data, uint32_t num_data_bits) {
    if (!m_builder)
      return;

    m_builder->Add<edr::JCWriteDR>(data, num_data_bits);
  }

  uint64_t GetShiftedData() {
    if (ActionFail())
      return 0;

    auto *shift_dr_out = (*m_iterator)->Out<edr::JCShiftDR>();
    if (nullptr == shift_dr_out)
      return 0;

    uint64_t output = 0;
    edr::BitStream<uint64_t> dest_stream(&output, 8 * sizeof(output));

    auto src_stream = shift_dr_out->Bits();
    dest_stream.Write(src_stream, src_stream.GetNumBits());

    return output;
  }
};

class JtagChain : public DriverBase {
  DRIVER_BODY(JtagChain);

public:
  JtagChain(const std::shared_ptr<Context> &context_sp, const char *name,
            Jtag &jtag)
      : JtagChain(context_sp,
                  &context_sp->MakeWith<edr::JtagChain>(
                      context_sp->PersistFormat("{}", name), *jtag.Self())) {}
};

#endif
