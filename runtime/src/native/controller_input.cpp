#include "mka/controller_input.hpp"

#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::input {
ControllerState digital_state(std::uint16_t buttons,bool connected,
    const std::array<std::uint8_t,4>& axes,std::uint8_t left,std::uint8_t right,
    std::optional<NativeControllerAxes> native_axes) {
    ControllerState state;
    state.buttons=buttons; state.connected=connected; state.axes=axes;
    state.native_axes=std::move(native_axes);
    for(unsigned i=0;i<state.pressure.size();++i)
        state.pressure[i]=(buttons&(1u<<i))?255:0;
    state.pressure[8]=(buttons&(1u<<8))?left:0;
    state.pressure[9]=(buttons&(1u<<9))?right:0;
    return state;
}
std::string encode_pressure(const std::array<std::uint8_t,16>& pressure) {
    const char hex[]="0123456789abcdef";
    std::string result(32,'0');
    for(std::size_t i=0;i<pressure.size();++i) {
        result[2*i]=hex[pressure[i]>>4];result[2*i+1]=hex[pressure[i]&15];
    }
    return result;
}
std::optional<std::array<std::uint8_t,16>> decode_pressure(std::string_view text) {
    if(text.size()!=32)return std::nullopt;
    const auto digit=[](char c)->int {
        if(c>='0'&&c<='9')return c-'0';
        if(c>='a'&&c<='f')return c-'a'+10;
        if(c>='A'&&c<='F')return c-'A'+10;
        return -1;
    };
    std::array<std::uint8_t,16> result{};
    for(std::size_t i=0;i<result.size();++i) {
        const int high=digit(text[2*i]),low=digit(text[2*i+1]);
        if(high<0||low<0)return std::nullopt;
        result[i]=static_cast<std::uint8_t>(high*16+low);
    }
    return result;
}
bool ControllerInput::publish(std::uint32_t player,const ControllerState& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(sequence_==std::numeric_limits<std::uint64_t>::max()) return false;
    try {
        auto result=players_.try_emplace(player);
        result.first->second={state,++sequence_};
        return true;
    } catch(const std::bad_alloc&) { return false; }
      catch(const std::length_error&) { return false; }
}
std::optional<ControllerSample> ControllerInput::sample(std::uint32_t player) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found=players_.find(player);
    if(found==players_.end())return std::nullopt;
    return found->second;
}
ControllerInput& controllers() { static ControllerInput input;return input; }
} // namespace mka::input
