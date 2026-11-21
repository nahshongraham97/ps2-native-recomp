#include "native_typed_codegen.hpp"

#include "r5900.hpp"

#include <array>
#include <bit>
#include <cctype>
#include <new>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace recomp {
namespace {

bool fail(NativeTypedFailure& f, NativeTypedError kind, std::uint32_t pc,
          const char* operation) noexcept {
    f={kind,pc,operation};
    return false;
}

bool identifier(std::string_view s, bool qualified=false) noexcept {
    if(s.empty()) return false;
    bool first=true;
    for(std::size_t i=0;i<s.size();++i) {
        const unsigned char c=static_cast<unsigned char>(s[i]);
        if(qualified && c==':' && i+1<s.size() && s[i+1]==':' && !first) {
            ++i;first=true;continue;
        }
        if(first) {
            if(!(std::isalpha(c)||c=='_')) return false;
            first=false;
        } else if(!(std::isalnum(c)||c=='_')) return false;
    }
    return !first;
}

bool contract_valid(const NativeFunctionContract& c, NativeTypedFailure& f) noexcept {
    if(!identifier(c.native_name)||c.words.empty()||c.words.front().pc!=c.source_entry||
       c.source_parameters.size()>4||c.fields.empty())
        return fail(f,NativeTypedError::malformed_contract,c.source_entry,"typed signature or source range");
    for(std::size_t i=0;i<c.words.size();++i)
        if(c.words[i].pc!=c.source_entry+4*i)
            return fail(f,NativeTypedError::malformed_contract,c.source_entry,"noncontiguous source function");
    const auto& first=c.fields.front();
    if(!identifier(first.owner_type,true)||!identifier(first.failure_type,true)||
       !identifier(first.read_method)||!identifier(first.write_method)||
       first.value_type!=NativeValueType::signed_word)
        return fail(f,NativeTypedError::malformed_contract,c.source_entry,"native field contract");
    for(std::size_t i=0;i<c.fields.size();++i) {
        const auto& field=c.fields[i];
        if(field.owner_type!=first.owner_type||field.failure_type!=first.failure_type||
           !identifier(field.read_method)||!identifier(field.write_method)||
           field.value_type!=NativeValueType::signed_word)
            return fail(f,NativeTypedError::malformed_contract,c.source_entry,"mixed native owner contract");
        for(std::size_t j=0;j<i;++j)
            if(field.source_address==c.fields[j].source_address)
                return fail(f,NativeTypedError::malformed_contract,c.source_entry,
                            "duplicate source field binding");
    }
    for(const auto type:c.source_parameters)
        if(type!=NativeValueType::signed_word)
            return fail(f,NativeTypedError::malformed_contract,c.source_entry,"unsupported native parameter type");
    return true;
}

std::optional<NativeFunctionIr> lower(const NativeFunctionContract& c,
                                      NativeTypedFailure& f) {
    if(!contract_valid(c,f)) return {};
    NativeFunctionIr out;
    out.native_name=c.native_name;
    out.source_entry=c.source_entry;
    out.source_return=c.source_return;
    out.blocks.push_back({c.source_entry,{}});
    auto& effects=out.blocks.back().effects;
    std::array<std::optional<NativeValueId>,32> regs{};
    NativeValueId next=1;
    for(std::size_t i=0;i<c.source_parameters.size();++i) {
        const NativeValue v{next++,c.source_parameters[i]};
        out.parameters.push_back(v);
        effects.push_back({NativeEffectKind::parameter,c.source_entry,v,0,0,
            static_cast<std::int32_t>(i),0});
        regs[4+i]=v.id;
    }
    bool returned=false;
    auto value_of=[&](std::uint32_t r,std::uint32_t pc)->std::optional<NativeValueId> {
        if(r==0) {
            const NativeValue v{next++,NativeValueType::signed_word};
            effects.push_back({NativeEffectKind::constant_word,pc,v,0,0,0,0});
            return v.id;
        }
        if(!regs[r]) fail(f,NativeTypedError::unbound_value,pc,"source register has no typed definition");
        return regs[r];
    };
    auto field_at=[&](const Insn& i)->std::optional<std::uint32_t> {
        if(i.rs()!=28) {
            fail(f,NativeTypedError::unbound_memory,i.pc,"memory base is not proved GP symbol");
            return {};
        }
        const auto address=c.source_gp+static_cast<std::uint32_t>(i.simm());
        for(std::uint32_t k=0;k<c.fields.size();++k)
            if(c.fields[k].source_address==address) return k;
        fail(f,NativeTypedError::unbound_memory,i.pc,"GP symbol has no native field binding");
        return {};
    };
    auto data=[&](const Insn& i)->bool {
        using enum Op;
        switch(i.op) {
        case Sll:
            if(i.word==0) return true;
            break;
        case Lw: {
            const auto field=field_at(i);if(!field) return false;
            const NativeValue v{next++,NativeValueType::signed_word};
            effects.push_back({NativeEffectKind::load_field,i.pc,v,0,0,0,*field});
            if(i.rt()!=0) regs[i.rt()]=v.id;
            return true;
        }
        case Sw: {
            const auto field=field_at(i);if(!field) return false;
            const auto value=value_of(i.rt(),i.pc);if(!value) return false;
            effects.push_back({NativeEffectKind::store_field,i.pc,{},*value,0,0,*field});
            return true;
        }
        case Addiu: {
            const auto left=value_of(i.rs(),i.pc);if(!left) return false;
            const NativeValue v{next++,NativeValueType::signed_word};
            effects.push_back({NativeEffectKind::add_word,i.pc,v,*left,0,i.simm(),0});
            if(i.rt()!=0) regs[i.rt()]=v.id;
            return true;
        }
        case Addu: case Subu: {
            const auto left=value_of(i.rs(),i.pc);if(!left) return false;
            const auto right=value_of(i.rt(),i.pc);if(!right) return false;
            const NativeValue v{next++,NativeValueType::signed_word};
            effects.push_back({i.op==Addu?NativeEffectKind::add_word:NativeEffectKind::subtract_word,
                i.pc,v,*left,*right,0,0});
            if(i.rd()!=0) regs[i.rd()]=v.id;
            return true;
        }
        case J: case Jal: case Jr: case Jalr:
            return fail(f,NativeTypedError::unbound_target,i.pc,"control target has no typed callee");
        case Lb: case Lbu: case Lh: case Lhu: case Lwu: case Ld: case Sb: case Sh: case Sd:
            return fail(f,NativeTypedError::unbound_memory,i.pc,"memory width has no typed field contract");
        default: break;
        }
        return fail(f,NativeTypedError::unsupported_instruction,i.pc,"instruction has no typed lowering");
    };
    for(std::size_t i=0;i<c.words.size();) {
        const auto& word=c.words[i];
        const Insn insn=decode(word.word,word.pc);
        if(insn.op==Op::Jr) {
            if(insn.rs()!=31||i+1>=c.words.size()||i+2!=c.words.size()) {
                fail(f,NativeTypedError::unbound_target,insn.pc,"return target or delay slot not closed");return {};
            }
            const auto& slot=c.words[i+1];
            const Insn delayed=decode(slot.word,slot.pc);
            if(has_delay_slot(delayed.op)||!data(delayed)) {
                if(f.kind==NativeTypedError::none)
                    fail(f,NativeTypedError::unsupported_control_flow,slot.pc,"control in return delay slot");
                return {};
            }
            if(c.source_return==NativeReturnType::signed_word) {
                if(!regs[2]) {fail(f,NativeTypedError::unbound_value,insn.pc,"source return word");return {};}
                effects.push_back({NativeEffectKind::return_word,insn.pc,{},*regs[2],0,0,0});
            } else effects.push_back({NativeEffectKind::return_void,insn.pc,{}});
            returned=true;i+=2;continue;
        }
        if(has_delay_slot(insn.op)||flow(insn.op)!=Flow::None) {
            fail(f,NativeTypedError::unbound_target,insn.pc,"control flow has no typed target");return {};
        }
        if(!data(insn)) return {};
        ++i;
    }
    if(!returned) {fail(f,NativeTypedError::unsupported_control_flow,c.source_entry,"function has no closed return");return {};}
    bool touches_owner=false;
    for(const auto& e:effects)
        if(e.kind==NativeEffectKind::load_field||e.kind==NativeEffectKind::store_field)
            touches_owner=true;
    if(!touches_owner) {
        fail(f,NativeTypedError::unbound_owner_effect,c.source_entry,
             "no verified native owner effect or validation policy");
        return {};
    }
    return out;
}

std::string value(NativeValueId id) {return "v"+std::to_string(id);}

void render(const NativeFunctionContract& c,const NativeFunctionIr& ir,
            std::ostringstream& header,std::ostringstream& impl) {
    const auto& owner=c.fields.front().owner_type;
    const auto& failure=c.fields.front().failure_type;
    const bool returns_word=c.source_return==NativeReturnType::signed_word;
    const auto type=returns_word?"std::optional<std::int32_t>":"bool";
    auto signature=[&](std::ostringstream& s) {
        s<<type<<' '<<c.native_name<<'('<<owner<<"& session";
        for(std::size_t i=0;i<c.source_parameters.size();++i)
            s<<", std::int32_t arg"<<i;
        s<<", "<<failure<<"& failure)";
    };
    signature(header);header<<";\n";
    signature(impl);impl<<" {\n";
    for(const auto& block:ir.blocks) for(const auto& e:block.effects) {
        switch(e.kind) {
        case NativeEffectKind::parameter:
            impl<<"    const std::int32_t "<<value(e.result.id)<<" = arg"<<e.constant<<";\n";break;
        case NativeEffectKind::constant_word:
            impl<<"    const std::int32_t "<<value(e.result.id)<<" = "<<e.constant<<";\n";break;
        case NativeEffectKind::add_word: case NativeEffectKind::subtract_word:
            impl<<"    const std::int32_t "<<value(e.result.id)
                <<" = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>("<<value(e.left)<<") "
                <<(e.kind==NativeEffectKind::add_word?'+':'-')<<' ';
            if(e.right) impl<<"static_cast<std::uint32_t>("<<value(e.right)<<")";
            else impl<<"static_cast<std::uint32_t>("<<e.constant<<")";
            impl<<");\n";break;
        case NativeEffectKind::load_field:
            impl<<"    const auto read"<<e.result.id<<" = session."
                <<c.fields[e.field].read_method<<"(failure);\n"
                <<"    if (!read"<<e.result.id<<") return {};\n"
                <<"    const std::int32_t "<<value(e.result.id)<<" = *read"<<e.result.id<<";\n";break;
        case NativeEffectKind::store_field:
            impl<<"    if (!session."<<c.fields[e.field].write_method<<'('
                <<value(e.left)<<", failure)) return "<<(returns_word?"{}":"false")<<";\n";break;
        case NativeEffectKind::return_word:
            impl<<"    return "<<value(e.left)<<";\n";break;
        case NativeEffectKind::return_void:
            impl<<"    return true;\n";break;
        }
    }
    impl<<"}\n";
}

} // namespace

std::optional<NativeTypedSources> emit_native_typed_functions(
    std::span<const std::uint8_t> original_elf,
    const NativeTypedManifest& manifest, NativeTypedFailure& failure) noexcept {
    failure={};
    try {
        if(manifest.functions.empty()) {
            fail(failure,NativeTypedError::malformed_contract,0,"empty typed function set");return {};
        }
        std::vector<NativeClosureWord> words;
        for(std::size_t i=0;i<manifest.functions.size();++i) {
            const auto& c=manifest.functions[i];
            if(!contract_valid(c,failure)) return {};
            for(std::size_t j=0;j<i;++j)
                if(c.native_name==manifest.functions[j].native_name||
                   c.source_entry==manifest.functions[j].source_entry) {
                    fail(failure,NativeTypedError::malformed_contract,c.source_entry,
                         "duplicate typed function identity");return {};
                }
            words.insert(words.end(),c.words.begin(),c.words.end());
        }
        NativeClosureFailure source_failure;
        if(!verify_native_closure_source(original_elf,manifest.source_sha256,words,source_failure)) {
            fail(failure,source_failure.kind==NativeClosureError::source_word?
                 NativeTypedError::source_word:NativeTypedError::source_identity,0,
                 "original ELF identity or typed source words");return {};
        }
        NativeTypedSources result;
        for(const auto& c:manifest.functions) {
            auto ir=lower(c,failure);
            if(!ir) return {};
            result.lowered.push_back(std::move(*ir));
        }
        std::ostringstream header,impl;
        header<<"#pragma once\n#include \"mka/retail_game_session.hpp\"\n"
              <<"#include <bit>\n#include <cstdint>\n#include <optional>\nnamespace mka::retail::generated {\n";
        impl<<"#include \"native_typed_retail.hpp\"\nnamespace mka::retail::generated {\n";
        for(std::size_t i=0;i<manifest.functions.size();++i)
            render(manifest.functions[i],result.lowered[i],header,impl);
        header<<"} // namespace mka::retail::generated\n";
        impl<<"} // namespace mka::retail::generated\n";
        result.header=header.str();result.implementation=impl.str();
        return result;
    } catch(const std::bad_alloc&) {
        fail(failure,NativeTypedError::allocation,0,"typed emission allocation");
    } catch(const std::length_error&) {
        fail(failure,NativeTypedError::allocation,0,"typed emission extent");
    } catch(...) {
        fail(failure,NativeTypedError::native_exception,0,"typed emission exception");
    }
    return {};
}

} // namespace recomp
