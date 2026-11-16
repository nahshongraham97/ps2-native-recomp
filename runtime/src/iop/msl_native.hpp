#pragma once

#include <cstddef>
#include <cstdint>

namespace ps2::iop::msl_native {

// The native sound service (decision B, increment 4).
//
// The retail game talks to MSL, an IOP module that owns SIF sid 0x00012345 and
// answers RPC 56026 with a command list. MSL turned those commands into SPU2
// voices. This service replaces that module completely: the runtime owns the
// sid, parses the command list natively, reads the retail sound bank from the
// disc through the native file service and starts voices in the native audio
// engine. The captured MSL and SDRDRV bodies are no longer loaded, so no guest
// service remains on the accepted route.
//
// Protocol (decoded from the captured MSL body and the retail bank):
//   op 0x03 id value   - set a parameter; id 0 with a value is a play request
//                        for that sound number, id 0x100FC asks for the staging
//                        buffer address.
//   op 0x06 ...        - stage a block for the sequencer.
//   op 0x04 ...        - execute/commit the staged block.
//   op 0x09 / 0x0A ... - load a sound bank (a name follows the header).
//   op 0x0B / 0x0D / 0x11 / 0x0C - flags.
// The reply is four words, as the retail server's was.
//
// Sound bank (disc-files/SNDSPS2/MSLASSET.MS2):
//   header  [2][sound_count][table_bytes]
//   table   9229 entries of 12 bytes: [id][sequence_offset][sequence_size]
//   at sequence_offset: a list of (pattern_index, duration) pairs; the
//   pattern table (right after the header's table) holds 12-byte records
//   [file_offset][size][id] pointing at the sample data, which is stored in
//   the SPU2's own ADPCM block format.
void install();
void reset();

// Frame tick: advances the sequencer's note timing. Called from the native
// mixer once per rendered audio batch.
void tick(std::size_t frames);

} // namespace ps2::iop::msl_native
