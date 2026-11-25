"""One source-backed nullable reference symbol, for offline transform experiments.

The original32-bit storage is evidence only. There is no native producer binding,
pointer conversion, production effect or generic guest-memory mapping here.
"""
from __future__ import annotations
import hashlib
import struct
from pathlib import Path

ELF_SHA256 = "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f"
ADDRESS = 0x0064E348
GP = 0x006540F0
PRODUCER = [(0x18D620,0x0C05BD9C),(0x18D624,0x2409001C),
 (0x18D628,0x50400009),(0x18D630,0x50400005),(0x18D634,0xAF80A258),
 (0x18D638,0xAF82A258),(0x18D63C,0x8C430004),(0x18D640,0x10000002),
 (0x18D644,0xAF83A25C),(0x18D648,0xAF80A25C)]
READERS = [(0x18D320,0x8F84A258),(0x18D324,0x50800005),(0x18D328,0x0000202D),
 (0x18D32C,0x8C830004),(0x18D330,0x8F82A25C),(0x18D334,0x54620001),
 (0x18D338,0x0000202D),(0x18D4C4,0x8F85A258),(0x18D4C8,0x50A00005),
 (0x18D4CC,0x0000282D),(0x18D4D0,0x8CA30004),(0x18D4D4,0x8F82A25C),
 (0x18D4D8,0x54620001)]
RESET = [(0x18DA30,0xAF80A258),(0x18DA34,0xAF80A25C)]

class SymbolMapRefusal(ValueError):
    pass

def require(ok, reason):
    if not ok:
        raise SymbolMapRefusal(reason)

def object_ref64E348(image: bytes):
    require(hashlib.sha256(image).hexdigest() == ELF_SHA256, "symbol_map_original_ELF_pin")
    require(image[:6] == b"\x7fELF\x01\x01", "symbol_map_requires_ELF32_LE")
    ph = struct.unpack_from("<I",image,28)[0]
    stride,count = struct.unpack_from("<HH",image,42)
    require(stride >= 32 and ph+stride*count <= len(image), "symbol_map_PT_LOAD_bounds")
    loads = [(i,struct.unpack_from("<8I",image,ph+i*stride)) for i in range(count)]
    selected = [(i,h) for i,h in loads if h[0] == 1 and h[2] <= ADDRESS
                and ADDRESS+4 <= h[2]+h[5]]
    require(len(selected) == 1, "symbol_map_unique_storage_segment")
    segment_index,h = selected[0]
    require(h == (1,256,0x100000,0x100000,5561984,7096448,7,128),
            "symbol_map_segment_identity")
    file_end = h[2]+h[4]
    require(file_end <= ADDRESS and ADDRESS+8 <= h[2]+h[5],
            "symbol_map_reference_and_cookie_must_be_loader_zero_storage")
    # ELF MIPS_REGINFO carries the actual linker GP value. This is stronger
    # than guessing a GP from a nearby field or an agent-authored wrapper.
    sh = struct.unpack_from("<I",image,32)[0]
    ss,sn = struct.unpack_from("<HH",image,46)
    require(ss >= 40 and sh+ss*sn <= len(image), "symbol_map_section_bounds")
    reginfos = [struct.unpack_from("<10I",image,sh+i*ss) for i in range(sn)
                if struct.unpack_from("<I",image,sh+i*ss+4)[0] == 0x70000006]
    require(len(reginfos) == 1, "symbol_map_unique_MIPS_REGINFO")
    reginfo = reginfos[0]
    require(reginfo[5] == 24 and reginfo[4]+24 <= len(image),
            "symbol_map_REGINFO_bounds")
    regbytes = image[reginfo[4]:reginfo[4]+24]
    require(struct.unpack_from("<I",regbytes,20)[0] == GP, "symbol_map_linker_GP_identity")
    def word(address):
        matches = [o+address-v for _,(kind,o,v,_,n,*_) in loads
                   if kind == 1 and v <= address and address+4 <= v+n]
        require(len(matches) == 1, "symbol_map_instruction_not_uniquely_file_backed")
        return struct.unpack_from("<I",image,matches[0])[0]
    def join(witnesses):
        for pc,expected in witnesses:
            require(word(pc) == expected, "symbol_map_source_word_changed:"+f"{pc:08x}")
        return [{"pc":f"{pc:08x}","word":f"{w:08x}"} for pc,w in witnesses]
    producer,readers,reset = map(join,(PRODUCER,READERS,RESET))
    require(GP + (-0x5DA8) == ADDRESS, "symbol_map_GP_displacement")
    return {
        "schema":"retail_single_data_symbol_v1","address":f"{ADDRESS:08x}",
        "semantic_kind":"nullable_borrowed_object_reference",
        "original_storage_width_bits":32,"allowed_projection":"clear_borrowed_reference",
        "owner_binding":None,"pointee_binding":None,
        "producer_words":producer,"read_words":readers,"reset_words":reset,
        "gp_base":f"{GP:08x}","signed_GP_displacement":-0x5DA8,
        "unbound_companion_cookie":"0064e34c",
        "bss_mapping":{"pt_load_index":segment_index,"file_offset":h[1],
            "virtual_address":f"{h[2]:08x}","file_bytes":h[4],"memory_bytes":h[5],
            "file_backed_end_exclusive":f"{file_end:08x}",
            "birth_value":"loader zero; no file-backed symbol bytes",
            "reference_birth":"null","cookie_birth":0},
        "source_pins":{"original_elf_sha256":ELF_SHA256,
            "MIPS_REGINFO_file_offset":reginfo[4],
            "MIPS_REGINFO_sha256":hashlib.sha256(regbytes).hexdigest(),
            "map_source_sha256":hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "established_behavior":[
            "JAL0016F670 produces reference V0; published by SW V0 at0018D638",
            "Object word+4 is separately cached in0064E34C by0018D63C/0018D644",
            "Readers reject null or mismatched object word+4 versus cached cookie",
            "SWzero at0018DA30 clears a borrowed reference, not object retirement"],
        "limits":[
            "Actual native producer owner and pointee ABI are unbound",
            "Cookie0064E34C remains unbound; no mapping for other GP fields",
            "No source32-bit pointer bits may become a native pointer or handle",
            "Experimental clear IR only; native emission/production adoption refused"]
    }

def validated_symbol_map(image: bytes):
    symbol = object_ref64E348(image)
    return {symbol["address"]:symbol}
