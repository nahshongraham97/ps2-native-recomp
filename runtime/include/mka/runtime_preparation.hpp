#pragma once
#include "mka/audio_controls.hpp"
namespace mka::sound {
// Typed native constructor/cache owner. This does not claim per-play cloning,
// output selection, an activated deadline, a source callback or voice start.
class NativeRuntimePreparation final {
public:
    const std::shared_ptr<const PreparedProgram>& data() const noexcept{return data_;}
    const AudioControlFactors& initial_controls() const noexcept{return controls_;}
    std::uint16_t runtime_flags() const noexcept{return static_cast<std::uint16_t>(data_->effective_flags);}
    std::size_t initial_command_index() const noexcept{return 0;}
    const std::shared_ptr<AudioResourceDomain>& resource_domain()const noexcept{return domain_;}
    bool same_bank(const NativeRuntimePreparation& other)const noexcept{return bank_==other.bank_;}
private:
    NativeRuntimePreparation(std::shared_ptr<const PreparedProgram> data,
        std::shared_ptr<const void> bank,std::shared_ptr<AudioResourceDomain> domain)
        :data_(std::move(data)),bank_(std::move(bank)),domain_(std::move(domain)),controls_(initial_program_control_factors()){}
    std::shared_ptr<const PreparedProgram> data_;
    std::shared_ptr<const void> bank_;
    std::shared_ptr<AudioResourceDomain> domain_;
    AudioControlFactors controls_;
    friend class PreparedBank;
};
// One semantic acquisition, distinct from independently retained immutable data.
// A reference retains the actual bank record. Release is exactly once and may
// run on another native thread; it does not acknowledge source output cleanup.
class RuntimeProgramReference final {
public:
    RuntimeProgramReference(const RuntimeProgramReference&)=delete;
    RuntimeProgramReference& operator=(const RuntimeProgramReference&)=delete;
    ~RuntimeProgramReference();
    std::shared_ptr<const NativeRuntimePreparation> preparation() const;
    bool active() const;
    bool release(PreparationFailure&);
private:
    struct Impl;
    explicit RuntimeProgramReference(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> state_;
    friend class PreparedBank;
};
} // namespace mka::sound
