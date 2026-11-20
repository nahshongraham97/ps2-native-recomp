# Native retail startup and content source review

SOURCE_REVIEW_NATIVE_RETAIL_STARTUP_AND_CONTENT_VERIFIED

## Findings

- The manifest’s 209 distinct words match the original retail ELF at their file-backed addresses. The offline verifier checks exact ELF identity, word sequence and source order before emitting anything. The generated bodies use native-owned arguments, prefix and directory storage and an actual retained content file owner.
- The emitted entry orders CRT prerequisite binding, thread setup, prefix initialization, platform setup, argv0 directory derivation and the remaining main continuation. The startup leaf archive and expected-negative entry link fixture remain distinct. No successful test or production stand-in fills a missing binding.
- The corrected CLI writes all three known files into an exclusively created sibling directory and publishes with one directory rename. Its cleanup paths are precomputed, native cleanup does not allocate new paths, and removing an empty final build directory uses a directory-only operation. Existing populated output is verified byte-for-byte.
- The content owner uses one retained Windows root handle for both identity and normalized path, denies delete sharing on this handle, checks canonical containment before file open and checks containment and disk regularity on the opened handle. The file retains the root owner and preserves the startup keys with an exact final ;1 decode. The aggregate source scope is fourteen code/schema files plus the build entry file, fifteen total.
- The first validation attempt stopped before compilation because test_native_file was defined only under PS2_GENERATED. The existing fixture now has one EXCLUDE_FROM_ALL definition in native_game.cmake, links the native content archive, and retains the original fixture source. The old CMake and review bytes remain archived with the prebuild failure. No startup binding changed.

## Unbound production symbols

- initialize_native_retail_crt
- configure_native_retail_thread
- initialize_native_retail_platform
- continue_native_retail_main

## Scope

- Source-only. No build, fixture, game, model or physical endpoint ran. The generated leaves and archive do not establish a working retail boot.
- The runtime content call consumes supplied source-style keys through the native file handle. The general original 2209E0 resource-name producer, kernel/platform startup and the remaining game path are still unbound.
- Containment is scoped to the implemented Windows retained-root and pre/post-open checks. No kernel-relative traversal across arbitrary concurrent namespace changes is claimed.
