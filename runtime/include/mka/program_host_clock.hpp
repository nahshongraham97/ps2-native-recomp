#pragma once
#include "mka/program_executor.hpp"
#include <functional>
#include <memory>
namespace mka::sound {
enum class ProgramHostClockError {none,unsupported_host,invalid_source,invalid_frequency,query,regressed,reentry,overflow,native_exception,allocation_failed};
struct ProgramHostClockFailure {ProgramHostClockError kind=ProgramHostClockError::none;std::int64_t native_code=0;};
using ProgramHostTickReader=std::function<std::optional<std::int64_t>(ProgramHostClockFailure&)>;
// One explicitly owned native epoch shared by all users. This is not a claim
// that its origin equals the original kernel clock epoch. No pause/reset exists.
class NativeProgramHostClock final {
public:
    static std::shared_ptr<NativeProgramHostClock> create(ProgramHostClockFailure&);
    static std::shared_ptr<NativeProgramHostClock> create_with_reader(std::int64_t frequency,ProgramHostTickReader,ProgramHostClockFailure&);
    ~NativeProgramHostClock();
    NativeProgramHostClock(const NativeProgramHostClock&)=delete;
    NativeProgramHostClock& operator=(const NativeProgramHostClock&)=delete;
    std::optional<ProgramClockWord> read(ProgramHostClockFailure&) noexcept;
private:
    struct Impl;
    explicit NativeProgramHostClock(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> state_;
};
} // namespace mka::sound
