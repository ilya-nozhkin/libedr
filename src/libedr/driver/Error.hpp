#ifndef LIBEDR_DRIVER_ERROR_HPP
#define LIBEDR_DRIVER_ERROR_HPP

#include "libedr/driver/Action.hpp"
#include "libedr/driver/AnyAction.hpp"
#include "libedr/driver/TransactionBuffer.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/driver/jtag/JtagChainAction.hpp"
#include "libedr/util/vss/VSS.hpp"
#include "libedr/util/vss/VSSPayloads.hpp"

#include <format>
#include <memory_resource>
#include <tuple>

namespace edr {

static inline constexpr size_t g_min_error_data_chunk_size = 1024;

struct ActionError;

struct CauseUnsupportedAction {
  static inline constexpr auto g_id = CauseID::UnsupportedAction;

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("Unsupported action");
  }
};

struct CauseAllocationFailure {
  static inline constexpr auto g_id = CauseID::AllocationFailure;

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("Allocation failure");
  }
};

struct CauseStringMessage {
  static inline constexpr auto g_id = CauseID::StringMessage;

  using Payload = vss::Payload<vss::String>;
  using FormattedPayload = vss::Payload<vss::String::Format>;

  template <class... Args> CauseStringMessage(Args &&...) {}

  std::string_view Message() { return vss::Get<0>(*this).View(); }

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Value("{}", Message());
  }

  template <class... Args>
  static void EmplacePayload(vss::OutputStream auto &os,
                             std::format_string<Args...> format,
                             Args &&...args) {
    FormattedPayload::Emplace(os, format, std::forward<Args>(args)...);
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size);
  }

  CauseStringMessage(const CauseStringMessage &) = delete;
  CauseStringMessage(CauseStringMessage &&) = delete;
};

struct CauseNestedError {
  static inline constexpr auto g_id = CauseID::NestedError;

  using ErrorOrLost = vss::Variant<ActionError, CauseAllocationFailure>;
  using Payload = vss::Payload<ErrorOrLost>;

  template <StructureFormatter F> void Format(F &fmt) {
    auto &error_or_lost = vss::Get<0>(*this);
    error_or_lost.Visit([&fmt](auto &cause) { fmt.Value("{}", cause); });
  }

  using PayloadWithError = vss::Payload<ErrorOrLost::Option<ActionError>>;
  using PayloadWithLost =
      vss::Payload<ErrorOrLost::Option<CauseAllocationFailure>>;

  static void EmplacePayload(vss::OutputStream auto &os, ActionError *error) {
    if (nullptr != error)
      PayloadWithError::Emplace(os, vss::Copy(*error));
    else
      PayloadWithLost::Emplace(os);
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size);
  }

  CauseNestedError(const CauseNestedError &) = delete;
  CauseNestedError(CauseNestedError &&) = delete;
};

struct CauseTerminated {
  static inline constexpr auto g_id = CauseID::Terminated;

  using Payload = vss::Payload<vss::String>;

  std::string_view GetEntity() { return vss::Get<0>(*this).View(); }

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Value("{} is already terminated", GetEntity());
  }

  static void EmplacePayload(vss::OutputStream auto &os,
                             std::string_view entity_name) {
    Payload::Emplace(os, entity_name);
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size);
  }

  CauseTerminated(const CauseTerminated &) = delete;
  CauseTerminated(CauseTerminated &&) = delete;
};

struct CauseErrno {
  static inline constexpr auto g_id = CauseID::Errno;

  int32_t return_value;
  int32_t errno_value;

  using Payload = vss::Payload<vss::String>;

  CauseErrno(std::string_view /*function_name*/, int32_t return_value)
      : return_value(return_value), errno_value(errno) {}

  std::string_view GetFunction() { return vss::Get<0>(*this).View(); }

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Value("'{}' returned {},", GetFunction(), return_value);
    fmt.Field("errno", "{} ({})", errno_value, strerror(errno_value));
  }

  static void EmplacePayload(vss::OutputStream auto &os,
                             std::string_view function_name,
                             int32_t /*return_value*/) {
    Payload::Emplace(os, function_name);
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size);
  }

  CauseErrno(const CauseErrno &) = delete;
  CauseErrno(CauseErrno &&) = delete;
};

struct CauseInvalidArgument {
  static inline constexpr auto g_id = CauseID::InvalidArgument;

  using Payload = vss::Payload<vss::String>;

  std::string_view GetArgumentName() { return vss::Get<0>(*this).View(); }

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Value("Invalid value of argument '{}'", GetArgumentName());
  }

  static void EmplacePayload(vss::OutputStream auto &os,
                             std::string_view argument_name) {
    Payload::Emplace(os, argument_name);
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size);
  }

  CauseInvalidArgument(const CauseInvalidArgument &) = delete;
  CauseInvalidArgument(CauseInvalidArgument &&) = delete;
};

struct CauseTimeoutInCycles {
  static inline constexpr auto g_id = CauseID::TimeoutInCycles;

  uint32_t num_cycles;

  CauseTimeoutInCycles(uint32_t num_cycles) : num_cycles(num_cycles) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Value("Timeout after {} cycles", num_cycles);
  }
};

struct CauseTargetError {
  static inline constexpr auto g_id = CauseID::TargetError;

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Value("The target indicated an error without details");
  }
};

struct CauseIDGetter {
  template <class C> consteval CauseID operator()() { return C::g_id; }
};

using ActionOrUnknown = vss::Variant<AnyAction, ActionID>;

using Cause = vss::VariantBase<
    CauseID, CauseIDGetter, CauseUnsupportedAction, CauseAllocationFailure,
    CauseStringMessage, CauseNestedError, CauseTerminated, CauseErrno,
    CauseInvalidArgument, CauseTimeoutInCycles, CauseTargetError,
    CauseInvalidJtagTapID, CauseInvalidJtagState, CauseUnstableJtagState,
    CauseIRLengthTooBig>;

struct ActionError final {
  using Payload = vss::Payload<ActionOrUnknown, Cause>;
  using WithKnownAction =
      vss::Payload<ActionOrUnknown::Option<AnyAction>, Cause>;
  using WithUnknownAction =
      vss::Payload<ActionOrUnknown::Option<ActionID>, Cause>;

  ActionError() = delete;

  static void EmplacePayload(vss::OutputStream auto &os) = delete;

  static void EmplacePayload(vss::OutputStream auto &os,
                             vss::CopyAdapter<ActionError> error) {
    Payload::Emplace(os, vss::Copy(vss::Get<0>(error.ref)),
                     vss::Copy(vss::Get<1>(error.ref)));
  }

  template <IsAction TAction>
  static void EmplacePayload(vss::OutputStream auto &os, TAction *failed_action,
                             Cause &cause) {
    if (nullptr != failed_action && failed_action->IsValid())
      WithKnownAction::Emplace(os, vss::Copy(*failed_action), vss::Copy(cause));
    else
      WithUnknownAction::Emplace(os,
                                 nullptr == failed_action
                                     ? ActionID::InvalidAction
                                     : failed_action->discriminant,
                                 vss::Copy(cause));
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size);
  }

  AnyAction *FailedAction() {
    auto &action_or_unknown = vss::Get<0>(*this);
    return action_or_unknown.As<AnyAction>();
  }

  ActionID FailedActionID() {
    auto &action_or_unknown = vss::Get<0>(*this);
    auto *as_id = action_or_unknown.As<ActionID>();
    if (nullptr != as_id)
      return *as_id;

    auto *as_action = action_or_unknown.As<AnyAction>();
    return as_action->discriminant;
  }

  Cause &GetCause() { return vss::Get<1>(*this); }

  template <class C> struct Option {
    template <IsAction TAction, class... Args>
    Option(TAction & /*failed_action*/, Args &&...) {}

    using Payload = vss::Payload<ActionOrUnknown, Cause::Option<C>>;

    using WithKnownAction =
        vss::Payload<ActionOrUnknown::Option<AnyAction>, Cause::Option<C>>;
    using WithUnknownAction =
        vss::Payload<ActionOrUnknown::Option<ActionID>, Cause::Option<C>>;

    template <IsAction TAction, class... Args>
    static void EmplacePayload(vss::OutputStream auto &os,
                               TAction *failed_action, Args &&...args) {
      if (nullptr != failed_action && failed_action->IsValid())
        WithKnownAction::Emplace(
            os, vss::Copy(*failed_action),
            std::forward_as_tuple<Args...>(std::forward<Args>(args)...));
      else
        WithUnknownAction::Emplace(
            os,
            nullptr == failed_action ? ActionID::InvalidAction
                                     : failed_action->discriminant,
            std::forward_as_tuple<Args...>(std::forward<Args>(args)...));
    }

    std::optional<size_t> SizeOfPayload(Payload & /*payload*/,
                                        size_t /*max_size*/) = delete;

    Option(const Option &) = delete;
    Option(Option &&) = delete;
  };

  ActionError(const ActionError &) = delete;
  ActionError(ActionError &&) = delete;
};

class Error {
  using Buffer = TransactionBuffer<g_min_error_data_chunk_size>;

public:
  static Error Success() { return Error(false); }

  Error() : m_failed_to_allocate_buffer(true) {}

  template <class T, class... Args>
  static Error Make(std::pmr::memory_resource &resource, Args &&...args) {
    Buffer buffer(resource);
    vss::Emplace<Cause::Option<T>>(buffer, std::forward<Args>(args)...);
    if (buffer.HadAllocationFailure())
      return Error(true);

    return Error(std::move(buffer));
  }

  template <class... Args>
  static Error MakeCauseStringMessage(std::pmr::memory_resource &resource,
                                      std::format_string<Args...> format,
                                      Args &&...args) {
    Buffer buffer(resource);
    vss::Emplace<Cause::Option<CauseStringMessage>>(
        buffer, format, std::forward<Args>(args)...);
    if (buffer.HadAllocationFailure())
      return Error(true);

    return Error(std::move(buffer));
  }

  static Error Copy(std::pmr::memory_resource &resource, Error &from) {
    if (from.m_failed_to_allocate_buffer)
      return Error(true);

    Buffer buffer(resource);

    from.m_buffer->ForEachChunk([&](std::span<std::byte> bytes) {
      auto [_, dest] = buffer.Allocate(bytes.size());
      if (nullptr != dest)
        memcpy(dest, bytes.data(), bytes.size());
    });

    if (buffer.HadAllocationFailure())
      return Error(true);

    return Error(std::move(buffer));
  }

  operator bool() { return m_failed_to_allocate_buffer || m_buffer; }

  Cause *GetCause() {
    if (!m_buffer)
      return nullptr;

    return reinterpret_cast<Cause *>(m_buffer->From(m_buffer->Begin()).Get());
  }

private:
  bool m_failed_to_allocate_buffer;
  std::optional<Buffer> m_buffer;

  explicit Error(bool failed_to_allocate_buffer)
      : m_failed_to_allocate_buffer(failed_to_allocate_buffer) {}

  explicit Error(Buffer &&buffer) : m_buffer(std::move(buffer)) {}
};

} // namespace edr

template <> struct std::formatter<edr::CauseID, char> {
  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    auto it = ctx.begin();
    while (*it != '}')
      it++;

    return it;
  }

  template <class FmtContext>
  FmtContext::iterator format(const edr::CauseID &id, FmtContext &ctx) const {
    return std::format_to(ctx.out(), "{}", static_cast<uint32_t>(id));
  }
};

template <> struct std::formatter<edr::Cause, char> {
  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    auto it = ctx.begin();
    while (*it != '}')
      it++;

    return it;
  }

  template <class FmtContext>
  FmtContext::iterator format(edr::Cause &cause, FmtContext &ctx) const {
    if (!cause.IsValid())
      return std::format_to(ctx.out(), "Cause {}", cause.discriminant);

    auto it = ctx.out();
    cause.Visit([&it](auto &cause) { it = std::format_to(it, "{}", cause); });
    return it;
  }
};

template <> struct std::formatter<edr::ActionError, char> {
  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    auto it = ctx.begin();
    while (*it != '}')
      it++;

    return it;
  }

  template <class FmtContext>
  FmtContext::iterator format(edr::ActionError &error, FmtContext &ctx) const {
    auto it = ctx.out();

    auto *action = error.FailedAction();
    auto &cause = error.GetCause();
    if (nullptr == action)
      it = std::format_to(it, "Action {} => FAILED: {}", error.FailedActionID(),
                          cause);
    else
      it = std::format_to(it, "{} => FAILED, cause: {}", *action, cause);

    return it;
  }
};

template <> struct std::formatter<edr::Error, char> {
  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    auto it = ctx.begin();
    while (*it != '}')
      it++;

    return it;
  }

  template <class FmtContext>
  FmtContext::iterator format(edr::Error &error, FmtContext &ctx) const {
    if (!error)
      return std::format_to(ctx.out(), "Success");

    auto *cause = error.GetCause();
    if (nullptr == cause)
      return std::format_to(
          ctx.out(),
          "Unknown error: The cause is lost due to an allocation failure");

    return std::format_to(ctx.out(), "{}", *cause);
  }
};

#endif
