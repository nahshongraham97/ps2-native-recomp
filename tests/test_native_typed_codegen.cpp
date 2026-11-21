#include "native_typed_codegen.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

namespace fs=std::filesystem;

namespace {
bool check(bool value, std::string_view operation) {
    if(!value) std::cerr<<"native typed codegen: "<<operation<<'\n';
    return value;
}
bool absent(std::string_view text,std::string_view word) {
    return text.find(word)==std::string_view::npos;
}
bool write_new(const fs::path& path,const std::string& text) {
    if(fs::exists(path)) return false;
    std::ofstream out(path,std::ios::binary);
    out.write(text.data(),static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(out);
}
}

int main(int argc,char** argv) {
    if(argc!=3) {
        std::cerr<<"usage: test_native_typed_codegen ORIGINAL_ELF EXCLUSIVE_OUTPUT_DIR\n";
        return 2;
    }
    try {
        std::ifstream in(argv[1],std::ios::binary);
        if(!check(static_cast<bool>(in),"open original ELF")) return 1;
        const std::vector<std::uint8_t> elf(
            std::istreambuf_iterator<char>{in},std::istreambuf_iterator<char>{});
        auto manifest=recomp::retail_scalar_mode_manifest();
        recomp::NativeTypedFailure failure;
        auto source=recomp::emit_native_typed_functions(elf,manifest,failure);
        if(!check(source.has_value(),"emit source proved scalar functions")) return 1;
        if(!check(source->lowered.size()==2 &&
                  source->lowered[0].blocks.size()==1 &&
                  source->lowered[1].blocks.size()==1 &&
                  source->lowered[0].blocks[0].effects.back().kind==recomp::NativeEffectKind::return_word &&
                  source->lowered[1].blocks[0].effects.back().kind==recomp::NativeEffectKind::return_void,
                  "typed SSA and source return signatures")) return 1;
        for(const auto word:{"Cpu","c.r[","g_ram","g_spr","ld32(","st32(","resolve(","physical(","dispatch(","0x0064dea4"})
            if(!check(absent(source->header,word)&&absent(source->implementation,word),
                      "no guest ABI in emitted C++")) return 1;
        if(!check(source->header.find("std::optional<std::int32_t> retail_scalar_mode_get_185cb0")!=std::string::npos &&
                  source->header.find("bool retail_scalar_mode_set_185cc0")!=std::string::npos &&
                  source->implementation.find("session.scalar_mode(failure)")!=std::string::npos &&
                  source->implementation.find("session.store_scalar_mode_from_source(")!=std::string::npos,
                  "typed native signatures and real owner methods")) return 1;

        // Every refusal uses the same verified original ELF, except the
        // deliberately corrupt source word and source identity cases.
        auto changed=manifest;
        changed.functions[0].fields[0].source_address++;
        if(!check(!recomp::emit_native_typed_functions(elf,changed,failure) &&
                  failure.kind==recomp::NativeTypedError::unbound_memory,
                  "unknown GP field fails before emission")) return 1;
        changed=manifest;
        auto alias=changed.functions[0].fields.front();
        alias.read_method="settings";
        alias.write_method="store_scalar_mode_from_source";
        changed.functions[0].fields.push_back(alias);
        if(!check(!recomp::emit_native_typed_functions(elf,changed,failure) &&
                  failure.kind==recomp::NativeTypedError::malformed_contract,
                  "duplicate source address with divergent native destination refused")) return 1;
        changed=manifest;
        changed.functions.resize(1);
        changed.functions[0].source_entry=0x00324500u;
        changed.functions[0].source_return=recomp::NativeReturnType::source_void;
        changed.functions[0].words={{0x00324500u,0x03e00008u},
                                    {0x00324504u,0x00000000u}};
        if(!check(!recomp::emit_native_typed_functions(elf,changed,failure) &&
                  failure.kind==recomp::NativeTypedError::unbound_owner_effect,
                  "source-verified no-effect body cannot claim native owner success")) return 1;
        changed=manifest;
        changed.functions[0].words[1].word^=1;
        if(!check(!recomp::emit_native_typed_functions(elf,changed,failure) &&
                  failure.kind==recomp::NativeTypedError::source_word,
                  "changed original instruction refused")) return 1;
        changed=manifest;
        changed.source_sha256[0]='0';
        if(!check(!recomp::emit_native_typed_functions(elf,changed,failure) &&
                  failure.kind==recomp::NativeTypedError::source_identity,
                  "changed original identity refused")) return 1;
        changed=manifest;
        changed.functions.resize(1);
        changed.functions[0].source_entry=0x0018d460u;
        changed.functions[0].words={{0x0018d460u,0x3c050019u},{0x0018d464u,0x0000202du}};
        if(!check(!recomp::emit_native_typed_functions(elf,changed,failure) &&
                  failure.kind==recomp::NativeTypedError::unsupported_instruction,
                  "unlowered original instruction refused")) return 1;
        changed.functions[0].source_entry=0x0018d458u;
        changed.functions[0].words={{0x0018d458u,0x0c06365cu},{0x0018d45cu,0x00000000u}};
        if(!check(!recomp::emit_native_typed_functions(elf,changed,failure) &&
                  failure.kind==recomp::NativeTypedError::unbound_target,
                  "unbound original call target refused")) return 1;
        changed.functions[0].source_entry=0x0018e174u;
        changed.functions[0].words={{0x0018e174u,0x0040f809u},{0x0018e178u,0x2484db60u}};
        if(!check(!recomp::emit_native_typed_functions(elf,changed,failure) &&
                  failure.kind==recomp::NativeTypedError::unbound_target,
                  "unbound original indirect target refused")) return 1;

        const fs::path output(argv[2]);
        if(!check(!fs::exists(output),"exclusive output directory")) return 1;
        fs::create_directories(output);
        if(!check(write_new(output/"native_typed_retail.hpp",source->header) &&
                  write_new(output/"native_typed_retail.cpp",source->implementation),
                  "write generated native source")) return 1;
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"native typed codegen: "<<e.what()<<'\n';
    }
    return 1;
}
