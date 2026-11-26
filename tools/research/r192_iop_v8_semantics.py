"""Explicit frozen-R192-v8 compatibility projection, not R3000 hardware truth.

The independent reference is the unchanged original cpu.cpp. This projection
only specializes its established instruction behavior; no instruction decoder
or compiler is introduced at runtime. Queued R3000 emission remains separate.
"""
from pathlib import Path
import hashlib

REFERENCE_CPU_SHA256 = '4c9537efc70ab6361f9a5c07e572499c187c2d5c45b327c085a1a94a275f5f32'
REFERENCE_KERNEL_SHA256 = '694c9e1fe073d383ffa8b7151468ccfca4271c5aec277f2f6f22929253f279bc'
MODE = 'frozen_R192_v8_immediate_load_reference_compatibility'


def validate_source(path: Path):
    if hashlib.sha256(path.read_bytes()).hexdigest() != REFERENCE_CPU_SHA256:
        raise ValueError('frozen_R192_v8_cpu_source_pin_mismatch')
    if hashlib.sha256(path.with_name('kernel.cpp').read_bytes()).hexdigest() != REFERENCE_KERNEL_SHA256:
        raise ValueError('frozen_R192_v8_kernel_source_pin_mismatch')


def install(generator, source: Path):
    """Install before PIC source-site relocation substitutions are applied."""
    validate_source(source)
    original = generator.emit

    def emit(word, address, delay):
        op, rs, rt, rd, sa, fn, imm = generator.fields(word)
        # Source v8 ADD/SUB/ADDI wrap, unlike the hardware trapping variants.
        projected = word
        if op == 0 and fn in (0x20, 0x22):
            projected = (word & ~63) | (fn + 1)
        elif op == 8:
            projected = (word & 0x03FFFFFF) | (9 << 26)
        # Source v8 recognizes SYNC/CACHE as no-ops. These are explicit source
        # compatibility decisions, never claims about the custom IOP hardware.
        if (op == 0 and fn == 15) or op == 0x2F:
            return ['    ++n;']
        if op == 0 and fn in (12, 13):
            # Pin BOTH original sources: v8 syscall() and BREAK merely emit
            # these exact diagnostics then continue. This is emphatically not
            # R3000 exception behavior and is exclusive to this named mode.
            log = (f'    std::fprintf(stderr, "[iop] syscall at 0x%08X (v0 0x%X)\\n", seg | 0x{address:x}u, cpu.gpr[2]);'
                   if fn == 12 else f'    std::fprintf(stderr, "[iop] break at 0x%08X\\n", seg | 0x{address:x}u);')
            return ['    ++n;', log]
        if op == 0x10:
            if rs == 0:
                value = f'cpu.cop0[{rd}]' if rd in (12, 13, 14) else '0x1Fu' if rd == 15 else '0u'
                return ['    ++n;'] + ([f'    cpu.gpr[{rt}] = {value};'] if rt else [])
            if rs == 4:
                return ['    ++n;'] + ([f'    cpu.cop0[{rd}] = {generator.g(rt)};'] if rd in (12, 13, 14) else [])
            if fn == 16:
                return ['    ++n;', '    cpu.cop0[12] = (cpu.cop0[12] & ~0xFu) | ((cpu.cop0[12] >> 2u) & 0xFu);']
            return ['    ++n;']
        lines = original(projected, address, delay)
        result = []
        for line in lines:
            # v8 memory accessors define their own unaligned behavior. The
            # hardware-mode address-error exception path is not the reference.
            if 'if (addr & ' in line and 'iopRaiseException' in line:
                if 'memory.store' not in line:
                    continue
                start = line.index('if (addr & ')
                end = line.index('} memory.store', start)
                line = line[:start] + line[end + 2:]
            if 'if (cpu.pendingLoad)' in line:
                continue
            if 'cpu.pendingLoad = true;' in line:
                line = f'      cpu.gpr[{rt}] = v;'
            line = line.replace(f'(cpu.pendingLoad && cpu.pendingLoadReg == {rt}u ? cpu.pendingLoadValue : {generator.g(rt)})', generator.g(rt))
            result.append(line)
        if any('pendingLoad' in line for line in result):
            raise ValueError('unremoved_queued_load_in_frozen_v8_projection')
        return result

    generator.emit = emit
    return {'mode': MODE, 'reference_cpu_source': str(source.resolve()), 'reference_cpu_sha256': REFERENCE_CPU_SHA256,
            'differences_from_hardware_mode': ['immediate loads and MFC0', 'nontrapping ADD/SUB/ADDI',
                'reference memory access alignment', 'source COP0 subset/unknown reads zero', 'source SYNC/CACHE no-op'],
            'reference_kernel_sha256': REFERENCE_KERNEL_SHA256,
            'remaining_service_refusals': ['unknown imports and code destinations'], 'native_credit': 0}
