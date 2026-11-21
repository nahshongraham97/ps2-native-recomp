#include "mka/retail_remaining_constructors.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail {
namespace {
constexpr std::array<unsigned, 64> md5_shifts{
    7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
    5,9,14,20, 5,9,14,20, 5,9,14,20, 5,9,14,20,
    4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
    6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21};
constexpr std::array<std::uint32_t, 64> md5_constants{
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};

void md5_block(std::array<std::uint32_t, 4>& state, const std::uint8_t* block) noexcept {
    std::array<std::uint32_t, 16> words{};
    for (std::size_t i = 0; i != 16; ++i) {
        const auto j = i * 4;
        words[i] = std::uint32_t(block[j]) | (std::uint32_t(block[j+1]) << 8) |
            (std::uint32_t(block[j+2]) << 16) | (std::uint32_t(block[j+3]) << 24);
    }
    auto a = state[0], b = state[1], c = state[2], d = state[3];
    for (std::size_t i = 0; i != 64; ++i) {
        std::uint32_t f;
        std::size_t g;
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5*i+1) & 15; }
        else if (i < 48) { f = b ^ c ^ d; g = (3*i+5) & 15; }
        else { f = c ^ (b | ~d); g = (7*i) & 15; }
        const auto next = b + std::rotl(a + f + md5_constants[i] + words[g], int(md5_shifts[i]));
        a = d; d = c; c = b; b = next;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

constexpr std::array<RetailStaticHandlerBody, 4> handler_order{
    RetailStaticHandlerBody::release_other_player,
    RetailStaticHandlerBody::two_player_animation,
    RetailStaticHandlerBody::execute_mk_background_script,
    RetailStaticHandlerBody::execute_script_function};
constexpr std::array<std::string_view, 4> action_names{
    "TwoPlayerAnimation", "ReleaseOtherPlayer", "ExecuteMKBgndScript", "ExecuteScriptFunction"};
} // namespace

std::uint32_t retail_static_key_hash(std::string_view bytes) noexcept {
    std::array<std::uint32_t, 4> state{0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};
    const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
    const auto full = bytes.size() / 64;
    for (std::size_t i = 0; i != full; ++i) md5_block(state, data + i*64);
    std::array<std::uint8_t, 64> tail{};
    const auto rest = bytes.size() % 64;
    if (rest) std::memcpy(tail.data(), data + full*64, rest);
    tail[rest] = 0x80;
    if (rest >= 56) {
        md5_block(state, tail.data());
        tail.fill(0);
    }
    const auto bit_length = std::uint64_t(bytes.size()) * 8;
    for (std::size_t i = 0; i != 8; ++i) tail[56+i] = std::uint8_t(bit_length >> (i*8));
    md5_block(state, tail.data());
    return state[0] ^ state[1] ^ state[2] ^ state[3];
}

std::shared_ptr<NativeRetailTwoServiceObjects> NativeRetailTwoServiceObjects::create(
    RetailRemainingConstructorFailure& failure) noexcept {
    failure = {};
    try {
        auto owner = std::shared_ptr<NativeRetailTwoServiceObjects>(new NativeRetailTwoServiceObjects);
        // 3BFAB0 zeroes the first byte. The other static service is a distinct
        // 351390 descriptor, initially unbound. No callback is registered here.
        owner->prefix_descriptor_ = NativeRetailPrefixDispatchDescriptor::create(failure.descriptor);
        if (!owner->prefix_descriptor_) {
            failure.kind = RetailRemainingConstructorError::descriptor;
            return {};
        }
        return owner;
    } catch (const std::bad_alloc&) {
        failure.kind = RetailRemainingConstructorError::allocation;
        return {};
    } catch (const std::length_error&) {
        failure.kind = RetailRemainingConstructorError::host_capacity;
        return {};
    } catch (...) {
        failure.kind = RetailRemainingConstructorError::native_exception;
        return {};
    }
}

namespace main_binding {
namespace {
bool missing_keys(StartupFailure& failure) {
    failure = {StartupError::native_exception, "retail static key owner missing"};
    return false;
}
bool duplicate_keys(StartupFailure& failure) {
    failure = {StartupError::already_initialized, "retail static key group already initialized"};
    return false;
}
bool apply_static_key(NativeRetailMainState& main,
    const std::shared_ptr<NativeRetailStaticKeyOwners>& owner,
    std::string_view literal, std::uint32_t& destination, StartupFailure& failure) {
    // 4ACC20 measures a C string. The hash helper itself accepts explicit
    // bytes, so only this source-call adaptor truncates at the first zero.
    const auto bytes = literal.substr(0, literal.find('\0'));
    const auto hash = retail_static_key_hash(bytes);
    try {
        // Snapshot the observer. It may replace its own function object while
        // running; the next source call takes the then-current observer.
        RetailStaticKeyObserver observer = main.static_key_observer;
        if (observer) observer(bytes, hash);
    } catch (const std::bad_alloc&) {
        failure = {StartupError::allocation, "retail static key observer allocation"};
        return false;
    } catch (const std::length_error&) {
        failure = {StartupError::host_capacity, "retail static key observer capacity"};
        return false;
    } catch (...) {
        failure = {StartupError::native_exception, "retail static key observer exception"};
        return false;
    }
    // The local shared owner keeps destination alive if the callback resets
    // the MainState owner. Never resume writes into a detached owner.
    if (main.static_key_initialization_failed || main.static_keys != owner) {
        failure = {StartupError::native_exception,
            main.static_key_initialization_failed ? "retail static key observer reentered failed construction" :
                "retail static key owner changed during observer"};
        return false;
    }
    destination = hash;
    return true;
}
template<class Action>
bool construct_static_group(NativeRetailMainState& main, StartupFailure& failure,
    bool NativeRetailStaticKeyOwners::*prerequisite,
    bool NativeRetailStaticKeyOwners::*ready, Action&& action) {
    failure = {};
    try {
        if (main.static_key_initialization_failed) {
            failure = {StartupError::native_exception, "retail static key initialization previously failed"};
            return false;
        }
        if (main.static_key_initialization_active) {
            // A callback may have replaced static_keys with a new owner. The
            // activity guard therefore belongs to MainState, not that owner.
            main.static_key_initialization_failed = true;
            failure = {StartupError::native_exception, "retail static key construction reentered"};
            return false;
        }
        if (!main.static_keys) {
            if (prerequisite) return missing_keys(failure);
            main.static_keys = std::make_shared<NativeRetailStaticKeyOwners>();
        }
        // Publish before the first observer call, mirroring the original
        // destination's stable static lifetime throughout the constructor.
        auto owner = main.static_keys;
        if (owner->construction_active) {
            // A callback can reenter while the outer stage is incomplete.
            // Poison MainState so the outer call also stops before its write.
            main.static_key_initialization_failed = true;
            owner->construction_failed = true;
            failure = {StartupError::native_exception, "retail static key construction reentered"};
            return false;
        }
        if (owner->construction_failed) {
            failure = {StartupError::native_exception, "retail static key partial construction"};
            return false;
        }
        if (owner.get()->*ready) return duplicate_keys(failure);
        if (prerequisite && !(owner.get()->*prerequisite)) return missing_keys(failure);
        owner->construction_active = true;
        main.static_key_initialization_active = true;
        bool complete = false;
        try { complete = action(owner); }
        catch (const std::bad_alloc&) {
            failure = {StartupError::allocation, "retail static key construction allocation"};
        } catch (const std::length_error&) {
            failure = {StartupError::host_capacity, "retail static key construction capacity"};
        } catch (...) {
            failure = {StartupError::native_exception, "retail static key construction exception"};
        }
        owner->construction_active = false;
        main.static_key_initialization_active = false;
        if (!complete || main.static_key_initialization_failed || main.static_keys != owner) {
            // This guard outlives owner replacement or reset. An observer
            // effect must never be replayed by a later constructor call.
            main.static_key_initialization_failed = true;
            owner->construction_failed = true;
            if (complete)
                failure = {StartupError::native_exception, "retail static key owner changed during construction"};
            return false;
        }
        owner.get()->*ready = true;
        return true;
    } catch (const std::bad_alloc&) {
        failure = {StartupError::allocation, "retail static key owner allocation"};
    } catch (const std::length_error&) {
        failure = {StartupError::host_capacity, "retail static key owner capacity"};
    } catch (...) {
        failure = {StartupError::native_exception, "retail static key owner exception"};
    }
    return false;
}
}
bool construct_retail_two_service_objects(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (main.two_service_objects) {
        failure = {StartupError::already_initialized, "retail two service objects already initialized"};
        return false;
    }
    RetailRemainingConstructorFailure construction;
    auto owner = NativeRetailTwoServiceObjects::create(construction);
    if (!owner) {
        const bool allocation = construction.kind == RetailRemainingConstructorError::allocation ||
            (construction.kind == RetailRemainingConstructorError::descriptor &&
             construction.descriptor.kind == RetailDispatchOwnerError::allocation);
        const bool capacity = construction.kind == RetailRemainingConstructorError::host_capacity ||
            (construction.kind == RetailRemainingConstructorError::descriptor &&
             construction.descriptor.kind == RetailDispatchOwnerError::host_capacity);
        failure.kind = allocation ? StartupError::allocation :
            capacity ? StartupError::host_capacity : StartupError::native_exception;
        failure.operation = "retail two service objects construction";
        return false;
    }
    main.two_service_objects = std::move(owner);
    return true;
}

bool construct_retail_empty_binding(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (main.empty_binding) {
        failure = {StartupError::already_initialized, "retail empty binding already initialized"};
        return false;
    }
    // 4E00B0 stores zero at +0; 4E00F0 stores zero at +4/+8 and returns
    // one before its MAN parser because 4E00B0 passes a null second arg.
    main.empty_binding.emplace();
    return true;
}

bool construct_retail_key_action_records(NativeRetailMainState& main, StartupFailure& failure) {
    return construct_static_group(main, failure, nullptr,
        &NativeRetailStaticKeyOwners::initial_ready,
        [&](const auto& owner) {
            for (std::size_t i = 0; i != action_names.size(); ++i)
                if (!apply_static_key(main, owner, action_names[i], owner->initial_keys[i], failure))
                    return false;
            // 648B80 selects the second computed key, then 648BBC the first.
            constexpr std::array<std::size_t, 4> source_order{1, 0, 2, 3};
            for (std::size_t i = 0; i != source_order.size(); ++i) {
                owner->initial_handlers[i].key = owner->initial_keys[source_order[i]];
                owner->initial_handlers[i].body = handler_order[i];
            }
            return true;
        });
}

bool construct_retail_first_paired_key_records(NativeRetailMainState& main, StartupFailure& failure) {
    constexpr std::array<std::string_view, 6> groups{
        "Position", "Position", "Position", "Rotation", "Rotation", "Rotation"};
    constexpr std::array<std::string_view, 6> fields{"x", "y", "z", "x", "y", "z"};
    return construct_static_group(main, failure, &NativeRetailStaticKeyOwners::initial_ready,
        &NativeRetailStaticKeyOwners::first_pairs_ready,
        [&](const auto& owner) {
            for (std::size_t i = 0; i != groups.size(); ++i) {
                auto& pair = owner->first_pairs[i];
                if (!apply_static_key(main, owner, groups[i], pair.group, failure) ||
                    !apply_static_key(main, owner, fields[i], pair.field, failure)) return false;
            }
            return true;
        });
}

bool construct_retail_second_paired_key_records(NativeRetailMainState& main, StartupFailure& failure) {
    constexpr std::array<std::string_view, 10> groups{
        "Position", "Position", "Position", "LookAt", "LookAt", "LookAt",
        "Roll", "FocalLength", "FarPlane", "NearPlane"};
    constexpr std::array<std::string_view, 10> fields{
        "x", "y", "z", "x", "y", "z", "value", "value", "value", "value"};
    return construct_static_group(main, failure, &NativeRetailStaticKeyOwners::first_pairs_ready,
        &NativeRetailStaticKeyOwners::second_pairs_ready,
        [&](const auto& owner) {
            for (std::size_t i = 0; i != groups.size(); ++i) {
                auto& pair = owner->second_pairs[i];
                if (!apply_static_key(main, owner, groups[i], pair.group, failure) ||
                    !apply_static_key(main, owner, fields[i], pair.field, failure)) return false;
            }
            return true;
        });
}

bool construct_retail_first_handler_records(NativeRetailMainState& main, StartupFailure& failure) {
    constexpr std::array<std::size_t, 4> source_order{1, 0, 2, 3};
    return construct_static_group(main, failure, &NativeRetailStaticKeyOwners::second_pairs_ready,
        &NativeRetailStaticKeyOwners::first_handlers_ready,
        [&](const auto& owner) {
            std::array<std::uint32_t, 4> computed{};
            for (std::size_t i = 0; i != action_names.size(); ++i)
                if (!apply_static_key(main, owner, action_names[i], computed[i], failure))
                    return false;
            for (std::size_t i = 0; i != source_order.size(); ++i) {
                auto& record = owner->first_handlers[i];
                record.key = computed[source_order[i]];
                record.body = handler_order[i];
            }
            return true;
        });
}

bool construct_retail_second_handler_records(NativeRetailMainState& main, StartupFailure& failure) {
    constexpr std::array<std::size_t, 4> source_order{1, 0, 2, 3};
    return construct_static_group(main, failure, &NativeRetailStaticKeyOwners::first_handlers_ready,
        &NativeRetailStaticKeyOwners::second_handlers_ready,
        [&](const auto& owner) {
            std::array<std::uint32_t, 4> computed{};
            for (std::size_t i = 0; i != action_names.size(); ++i)
                if (!apply_static_key(main, owner, action_names[i], computed[i], failure))
                    return false;
            for (std::size_t i = 0; i != source_order.size(); ++i) {
                auto& record = owner->second_handlers[i];
                record.key = computed[source_order[i]];
                record.body = handler_order[i];
            }
            return true;
        });
}
} // namespace main_binding

} // namespace mka::retail
