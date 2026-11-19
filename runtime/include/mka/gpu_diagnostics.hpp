#pragma once
#include <atomic>
#include <cstdint>
#include <utility>

namespace mka::graphics {
// Written only by the context-owning render thread; total is a thread-safe
// observation contract for callers that do not own a GL context.
class GpuQueryCounter {
public:
    std::uint64_t session() const noexcept { return session_; }
    bool completed(std::uint64_t session, std::uint64_t serial, std::uint64_t nanoseconds) noexcept {
        if (session != session_ || serial == 0 || serial <= last_serial_) return false;
        last_serial_ = serial;
        const double next = total_ms_.load(std::memory_order_relaxed) + static_cast<double>(nanoseconds)/1000000.0;
        total_ms_.store(next,std::memory_order_release);
        return true;
    }
    // Context/query identities reset; process-lifetime accumulated time does not.
    void reset_session() noexcept { ++session_; last_serial_=0; }
    double total_ms() const noexcept { return total_ms_.load(std::memory_order_acquire); }
private:
    std::uint64_t session_=1, last_serial_=0;
    std::atomic<double> total_ms_{0.0};
};

// The destination operation must finish before its result is counted. Used by
// the actual IMAGE scene-upload path and tested with deterministic failures.
template<class Upload,class Record>
bool account_native_transfer(Upload&& upload,Record&& record) {
    const bool reached=std::forward<Upload>(upload)();
    std::forward<Record>(record)(reached);
    return reached;
}
} // namespace mka::graphics
