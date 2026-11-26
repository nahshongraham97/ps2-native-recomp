#pragma once
// Bounded reference-route connector. Original code identity is established at
// fresh-load time; restored Module records never manufacture a source owner.
#include "iop/r192_static_metadata_guard.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <atomic>
#include <limits>
#include <memory>
#include <span>

namespace ps2::iop::loader_owner {
namespace aot = r192_aot;
namespace md = metadata_guard;
struct Facts { int id{}; std::string name; std::uint32_t base{}, size{}, entry{}, gp{}; };
struct Record { md::Kind kind{}; std::uint32_t offset{}; std::vector<std::uint32_t> words, relocations; };
struct Source {
    std::string digest, filename_name, buffer_name;
    const aot::PicTemplate *code{};
    std::uint32_t entry_offset{}, gp_offset{};
    std::vector<Record> records;
};
struct OwnerHandle { std::size_t index{}; std::uint64_t epoch{}, generation{}; };
struct EntryIdentity {
    OwnerHandle owner;
    std::uint32_t table_offset{}, ordinal{}, target_offset{};
    aot::PicBody compiled_body{};
};
// Observation only, not permission to invoke after this check. It owns its
// digest and exposes no BoundPic/Module reference or borrowed word buffer.
struct EntryProof {
    OwnerHandle owner;
    int loaded_id{};
    std::uint32_t base{}, target_offset{};
    std::string original_digest;
    aot::PicBody compiled_body{};
};
inline std::string image_sha256(std::span<const std::uint8_t> bytes) {
    if (bytes.size() > std::numeric_limits<ULONG>::max()) throw std::runtime_error("original_image_too_large");
    struct Algorithm { BCRYPT_ALG_HANDLE h{}; ~Algorithm() { if (h) BCryptCloseAlgorithmProvider(h, 0); } } algorithm;
    struct Hash { BCRYPT_HASH_HANDLE h{}; std::vector<std::uint8_t> storage; ~Hash() { if (h) BCryptDestroyHash(h); } } hash;
    if (BCryptOpenAlgorithmProvider(&algorithm.h, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) throw std::runtime_error("native_SHA_provider_failed");
    ULONG size{}, written{};
    if (BCryptGetProperty(algorithm.h, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&size), sizeof size, &written, 0) || written != sizeof size)
        throw std::runtime_error("native_SHA_object_size_failed");
    hash.storage.resize(size);
    if (BCryptCreateHash(algorithm.h, &hash.h, hash.storage.data(), size, nullptr, 0, 0) ||
        BCryptHashData(hash.h, const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), 0))
        throw std::runtime_error("native_SHA_input_failed");
    std::array<std::uint8_t, 32> result{};
    if (BCryptFinishHash(hash.h, result.data(), static_cast<ULONG>(result.size()), 0)) throw std::runtime_error("native_SHA_finish_failed");
    static constexpr char hex[] = "0123456789abcdef";
    std::string text; text.reserve(64);
    for (auto b : result) { text += hex[b >> 4]; text += hex[b & 15]; }
    return text;
}

class Session {
public:
    using Load = std::function<int(const std::vector<std::uint8_t> &, const std::string &, const std::string &)>;
    using Fact = std::function<Facts(int)>;
    using List = std::function<std::vector<Facts>()>;
    Session(std::span<const Source> sources, md::Guard::Read read, aot::Services legacy, Load load, Fact fact, List list)
        : sources_(sources.begin(), sources.end()), read_(std::move(read)), legacy_(legacy), load_(std::move(load)), fact_(std::move(fact)), list_(std::move(list)), epoch_(next()) {
        guard_ = std::make_unique<md::Guard>(read_);
        for (std::size_t i = 0; i < sources_.size(); ++i) {
            const auto &s = sources_[i];
            if (s.digest.size() != 64 || !s.code || !s.code->body || s.records.empty() ||
                aot::find_pic_template(s.filename_name.c_str(), s.digest.c_str()) != s.code ||
                aot::find_pic_template(s.buffer_name.c_str(), s.digest.c_str()) != s.code) fail("unproved_source_catalogue");
            for (std::size_t j = 0; j < i; ++j) if (sources_[j].digest == s.digest) fail("ambiguous_source_catalogue_digest");
        }
    }
    bool poisoned() const { return poisoned_; }
    std::size_t admitted_owners() const { ready(); return owners_.size(); }
    OwnerHandle load(const std::vector<std::uint8_t> &bytes, const std::string &path, const std::string &args) {
        ready();
        const auto digest = image_sha256(bytes);
        const Source *source{};
        for (const auto &s : sources_) if (s.digest == digest) source = &s;
        if (!source) fail("unknown_original_image_before_loader");
        // Everything after actual loader entry may observe mutated reference
        // state. Exceptions poison the session; no rollback/synthetic success.
        try {
            const auto id = load_(bytes, path, args);
            if (id <= 0) fail("actual_loader_failed");
            const auto facts = fact_(id);
            if (facts.id != id || !facts.size || facts.size != source->code->total_size ||
                facts.base >= kRamSize || facts.size > kRamSize - facts.base ||
                facts.entry != facts.base + source->entry_offset || facts.gp != facts.base + source->gp_offset ||
                facts.name != (path == "buffer" ? source->buffer_name : source->filename_name)) fail("actual_loaded_facts_not_original_source");
            verify_exclusive(facts);
            auto owner = std::make_unique<Owner>(); owner->source = source; owner->facts = facts; owner->generation = next();
            if (!owner->bound.bind(*source->code, facts.base)) fail("original_PIC_binding_failed");
            owner->guard = guard_->bind_owner(owner->bound.module, legacy_, source->records.size());
            for (const auto &record : source->records) {
                if (record.kind == md::Kind::registry_root) fail("invented_SDK_root_not_allowed");
                auto words = record.words;
                for (auto offset : record.relocations) {
                    if ((offset & 3u) || offset / 4 >= words.size()) fail("original_record_relocation_bounds");
                    words[offset / 4] += facts.base;
                }
                std::vector<std::uint8_t> canonical;
                for (auto word : words) for (unsigned i = 0; i < 4; ++i) canonical.push_back(static_cast<std::uint8_t>(word >> (8 * i)));
                guard_->bind_record(owner->guard, record.kind, record.offset, std::move(canonical));
            }
            guard_->seal_owner(owner->guard);
            const OwnerHandle result{owners_.size(), epoch_, owner->generation};
            owners_.push_back(std::move(owner)); return result;
        } catch (...) { poisoned_ = true; throw; }
    }
    EntryIdentity export_entry(OwnerHandle handle, std::uint32_t table, std::uint32_t ordinal) const {
        const auto &owner = get(handle); guard_->verify();
        const Record *record{};
        for (const auto &r : owner.source->records) if (r.kind == md::Kind::exports && r.offset == table) record = &r;
        if (!record || record->words.size() < 7 || ordinal >= record->words.size() - 6) fail("unproved_original_export_slot");
        const auto site = 20u + ordinal * 4u;
        if (std::count(record->relocations.begin(), record->relocations.end(), site) != 1) fail("export_slot_not_original_R_MIPS32");
        const auto target = record->words[5 + ordinal];
        if (!guard_->permits_entry(owner.guard, owner.facts.base + target)) fail("export_target_is_not_code");
        return {handle, table, ordinal, target, owner.bound.module.pic_body};
    }
    EntryProof validate_entry(const EntryIdentity &entry) const {
        const auto expected = export_entry(entry.owner, entry.table_offset, entry.ordinal);
        if (entry.target_offset != expected.target_offset || entry.compiled_body != expected.compiled_body) fail("forged_compiled_entry_identity");
        const auto &owner = get(entry.owner);
        return {entry.owner, owner.facts.id, owner.facts.base, expected.target_offset,
                owner.source->digest, expected.compiled_body};
    }
    void retire(OwnerHandle handle) const { (void)get(handle); fail("module_retirement_lifetime_unbound"); }
    // The old epoch is invalid even if the progressive actual parser returns
    // false or throws. Restored metadata is retained only as observations.
    bool restore(const std::function<bool()> &actual_byte_load) {
        ready(); invalidate();
        try {
            if (!actual_byte_load()) { poisoned_ = true; return false; }
            restored_ = list_(); return true;
        } catch (...) { poisoned_ = true; throw; }
    }
    std::vector<Facts> restored_range_candidates(std::uint32_t address) const {
        ready(); std::vector<Facts> result; address &= 0x1FFFFFFFu;
        for (const auto &f : restored_) if (f.size && f.base <= address && address - f.base < f.size) result.push_back(f);
        return result; // Never choose one, infer a raw SHA, or admit a code owner.
    }
private:
    struct Owner { const Source *source{}; Facts facts; std::uint64_t generation{}; aot::BoundPic bound; md::OwnerHandle guard; };
    std::vector<Source> sources_; md::Guard::Read read_; aot::Services legacy_; Load load_; Fact fact_; List list_;
    std::unique_ptr<md::Guard> guard_; std::vector<std::unique_ptr<Owner>> owners_; std::vector<Facts> restored_;
    std::uint64_t epoch_{}; mutable bool poisoned_{};
    inline static std::atomic<std::uint64_t> sequence_{0};
    static std::uint64_t next() {
        auto value = sequence_.load();
        do { if (value == std::numeric_limits<std::uint64_t>::max()) fail("owner_generation_exhausted"); }
        while (!sequence_.compare_exchange_weak(value, value + 1));
        return value + 1;
    }
    [[noreturn]] static void fail(const char *text) { throw std::runtime_error(text); }
    void ready() const { if (poisoned_) fail("owner_session_poisoned_requires_rebuild"); }
    const Owner &get(OwnerHandle h) const {
        ready(); if (h.epoch != epoch_ || h.index >= owners_.size() || owners_[h.index]->generation != h.generation) fail("stale_code_owner_generation");
        const auto &owner = *owners_[h.index]; const auto current = fact_(owner.facts.id);
        if (current.id != owner.facts.id || current.base != owner.facts.base || current.size != owner.facts.size ||
            current.entry != owner.facts.entry || current.gp != owner.facts.gp || current.name != owner.facts.name) fail("loaded_owner_facts_changed");
        verify_exclusive(current);
        return owner;
    }
    void verify_exclusive(const Facts &facts) const {
        try {
            std::size_t same_id{};
            for (const auto &other : list_()) {
                if (other.id == facts.id) {
                    ++same_id;
                    if (other.name != facts.name || other.base != facts.base || other.size != facts.size ||
                        other.entry != facts.entry || other.gp != facts.gp) fail("loaded_owner_fact_list_disagrees");
                    continue;
                }
                if (!other.size) continue; // Explicit native HLE boundary, no original code range.
                if (other.base >= kRamSize || other.size > kRamSize - other.base) fail("loaded_module_fact_bounds_unproved");
                if (std::uint64_t(facts.base) < std::uint64_t(other.base) + other.size &&
                    std::uint64_t(other.base) < std::uint64_t(facts.base) + facts.size) fail("overlapping_loaded_module_facts");
            }
            if (same_id != 1) fail("loaded_owner_fact_list_disagrees");
        } catch (...) { poisoned_ = true; throw; }
    }
    void invalidate() {
        // Bump first: allocation failure rebuilding the guard cannot resurrect
        // any previous token. restore() catches/poisons all subsequent failure.
        epoch_ = next(); owners_.clear(); restored_.clear();
        try { guard_ = std::make_unique<md::Guard>(read_); } catch (...) { poisoned_ = true; throw; }
    }
};
} // namespace ps2::iop::loader_owner
