# Native loose assets

Owner-directed native host service change. Replace archive lookup with actual opened loose files under /game/art, preserve legacy named identifiers only at the lookup boundary, and fail missing files/directories without opening MKDA.PAK. Original PAK row offsets and order are not native asset identity. No production consumer of source_index/offset was found outside the package itself; tests used those archive implementation details.

The original245 basenames are all under \ps2dvd\art. Xbox art has243 matching names and different payload lengths, so it is not used as a byte-equivalent replacement. Original archive remains unchanged. New extraction requires around1.4GB; observed available capacity154GB. Full native gameplay, visual/audio behavior and source reachability remain OPEN.

Implementation uses NativeContentVolume::open_directory_files, retaining the selected directory identity and each actual NativeContentFile. Alias resolution still uses the existing NativeRetailFilePaths. NativeRetailPackage now owns a file per member and decodes only the proven original prefix or a native basename. Archive offsets/source ordinals are removed from its API; case-ambiguous native names fail. Nonregular entries and directory escapes fail. The new tools/extract_retail_art.py is an offline preparer only, preserves originals and refuses existing output. Component and original corpus checks pending.

Build validation-01 exit0 for both existing package clients after updating their contract to loose filename/length/bytes. Offline extraction exit0,245 files and1408827392 bytes verified. Runtime executions pending; tests use only native file/CPU services and produce no game window or physical sound.

validation-01 native clients both exit0. test_native_retail_package uses the frozen independent original manifest for all245 name/size/first/tail/range checks, with no PAK in its native content root. test_native_retail_package_errors proves archive-only refusal, missing-file no-fallback, different loose bytes winning, true empty files, invalid names/ranges, wrong thread, file lifetime after parent/alias retirement, retained directory identity, and235 failed allocation cuts with child-handle retirement. Additional directory-service boundary checks and scoped source/publication review pending. No game or physical endpoints, frames/audio none.

Added directly affected boundary checks for moved/invalid/traversing directory access, unknown aliases, an actual externally prepared escaping art junction, and shortening a retained file. These new checks are unbuilt. Native exception failures are now retained as native_exception rather than mislabeled source_open.

validation-02 build exit0 for the two changed package clients and affected content-volume/alias clients. Existing UTF8 fixture uses deprecated u8path warning; no build failure. Additional executions pending.

Actual junction fixture preparation was denied by Windows through both cmd mklink and PowerShell New-Item. Both failures are retained separately. No escaping-junction execution is credited; source containment checks and prior independent owner coverage remain distinct. Continue directly affected native clients with the nonjunction cases.

validation-02 both package clients pass, with309 allocation cuts, moved/invalid/traversing directory keys, unknown aliases and actual shortened-file failure. Affected NativeContentVolume owner and native alias clients also pass, with the owner client explicitly skipping symlink creation(error1314). No actual escaping-junction execution is credited. All client records have exact commands/binary hashes/logs. These executions used the preserved working tree audio adoption, so clean-source rebuild is the next gate.
