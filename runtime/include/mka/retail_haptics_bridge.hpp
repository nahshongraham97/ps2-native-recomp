#pragma once
#include "mka/haptics.hpp"

namespace mka::input {
struct RetailActuatorPacket {
    std::uint32_t sequence=0,task=0,length=0;
    ActuatorBytes values{};
    std::array<std::uint8_t,32> raw{};
};
enum class RetailHapticError { none, malformed_packet, unsupported_task,
    unsupported_length, endpoint_already_bound, player_already_bound,
    unbound_or_stale_endpoint, missing_alignment, generation_exhausted,
    allocation_failure, publication_failed };
struct RetailActuatorDecode {
    RetailActuatorPacket packet;
    RetailHapticError error=RetailHapticError::none;
    explicit operator bool()const{return error==RetailHapticError::none;}
};
// Exact original32-byte data contract, not the unrelated SDK RPC07 buffer.
RetailActuatorDecode decode_retail_actuator_packet(const std::uint8_t*,std::size_t);
struct HapticEndpoint { std::uint64_t identity=0,generation=0; };
struct HapticBindingResult {
    HapticEndpoint endpoint;
    RetailHapticError error=RetailHapticError::none;
    explicit operator bool()const{return error==RetailHapticError::none;}
};
struct RetailHapticDelivery {
    RetailHapticError error=RetailHapticError::none;
    HapticError publication_error=HapticError::none;
    explicit operator bool()const{return error==RetailHapticError::none;}
};
struct RetailHapticSnapshot {std::uint32_t player=0;RetailActuatorPacket packet;};
class RetailHapticsBridge {
public:
    // Service must outlive this bridge. Destruction publishes explicit stop
    // requests for owned bindings. Identity is an opaque full-width native
    // endpoint key, never an allocated dummy IOP pointer or memory address.
    explicit RetailHapticsBridge(Haptics& service):service_(service){}
    ~RetailHapticsBridge();
    RetailHapticsBridge(const RetailHapticsBridge&)=delete;
    RetailHapticsBridge& operator=(const RetailHapticsBridge&)=delete;
    HapticBindingResult bind(std::uint64_t identity,std::uint32_t player);
    RetailHapticDelivery deliver(HapticEndpoint,const std::uint8_t*,std::size_t);
    RetailHapticDelivery close(HapticEndpoint);
    RetailHapticDelivery close_all();
    std::optional<RetailHapticSnapshot> sample(HapticEndpoint)const;
private:
    struct EndpointState {std::uint64_t generation;std::uint32_t player;
        std::optional<RetailActuatorPacket> last;};
    Haptics& service_;
    mutable std::mutex mutex_;
    std::map<std::uint64_t,EndpointState> endpoints_;
};
// Successful delivery means data was accepted into host-owned Haptics. It
// does not mean SDL applied motors, nor recreate a SIF DMA completion/status.
// Wire sequence is opaque: original118FF0 increments before submission,
// so failed submissions may skip values and uint32 wrap is valid.
RetailHapticsBridge& retail_haptics_bridge();
} // namespace mka::input
