#pragma once

#include "native_closure_codegen.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace recomp {

// Values and source registers exist only while compiling the original code.
// The emitted interface contains typed native arguments and ordinary locals.
using NativeValueId = std::uint32_t;
enum class NativeValueType { signed_word, owner_reference };
enum class NativeReturnType { signed_word, source_void };
enum class NativeEffectKind { parameter, constant_word, add_word, subtract_word,
    load_field, store_field, return_word, return_void };
struct NativeValue {
    NativeValueId id{};
    NativeValueType type{NativeValueType::signed_word};
};
struct NativeEffect {
    NativeEffectKind kind{};
    std::uint32_t source_pc{};
    NativeValue result{};
    NativeValueId left{};
    NativeValueId right{};
    std::int32_t constant{};
    std::uint32_t field{};
};
struct NativeBasicBlock {
    std::uint32_t source_entry{};
    std::vector<NativeEffect> effects;
};
struct NativeFunctionIr {
    std::string native_name;
    std::uint32_t source_entry{};
    NativeReturnType source_return{NativeReturnType::source_void};
    std::vector<NativeValue> parameters;
    std::vector<NativeBasicBlock> blocks;
};

struct NativeFieldContract {
    std::uint32_t source_address{};
    std::string owner_type;
    std::string read_method;
    std::string write_method;
    std::string failure_type;
    NativeValueType value_type{NativeValueType::signed_word};
};
struct NativeFunctionContract {
    std::string native_name;
    std::uint32_t source_entry{};
    std::vector<NativeClosureWord> words;
    std::uint32_t source_gp{};
    NativeReturnType source_return{NativeReturnType::source_void};
    std::vector<NativeValueType> source_parameters;
    std::vector<NativeFieldContract> fields;
};
struct NativeTypedManifest {
    std::string source_sha256;
    std::vector<NativeFunctionContract> functions;
};
enum class NativeTypedError {
    none, source_identity, source_word, malformed_contract, unsupported_instruction,
    unsupported_control_flow, unbound_memory, unbound_value, unbound_target,
    unbound_owner_effect,
    allocation, native_exception
};
struct NativeTypedFailure {
    NativeTypedError kind{NativeTypedError::none};
    std::uint32_t source_pc{};
    const char* operation{""};
};

// The contract is immutable evidence, not a guest address registry in the
// generated program. This first closed pair is source specific; the IR is not.
NativeTypedManifest retail_scalar_mode_manifest();

} // namespace recomp
