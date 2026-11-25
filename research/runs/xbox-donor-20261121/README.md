# Xbox subsystem donor analysis

## Measured capability comparison — static evidence

The generated `capability_comparison` in [PUBLICATION.json](PUBLICATION.json) contains 11 capability rows, 70 measured function sets, and 34 additional PS2/Xbox role pairs. 8 pairs have PS2 starts outside the raw catalogue: they belong only in the cross-map/report and add no native ledger rows, census entries or credit. Every pair remains inferred, has native verification `none`, and bars adoption. The existing 18 mapping validations remain separate.

Instruction totals below cover the explicitly reported saved listing, authenticated byte interval or bounded direct-flow sets. They are not complete subsystem totals, semantic equivalence, or estimates of implementation effort. Full addresses, separate file offsets, byte hashes, calls, layout limits and native-source pins are in the generated object.

| Capability | PS2 measured instructions | Xbox measured instructions | Easier route within measured scope | Measured reason |
| --- | ---: | ---: | --- | --- |
| Renderer core | 151 | 171 | unknown | Plugin registration is common; complete device Open/Start/Stop and scene consumers remain untransformed. |
| Streams and chunks | 208 | 178 | equal | Matched chunk structure/version gate and existing native read cursors; neither side removes the parsing or lifetime work. |
| Plugin registry | 113 | 117 | equal | Both sides traverse extension IDs and callbacks; the native callback ABI and ownership transformation are required for either. |
| Camera and view window | 84 | 55 | equal | Window/reciprocal operations match in role; projection/depth arithmetic and attached-frame behavior still need independent verification. |
| Matrix and transforms | 258 | 199 | PS2 | PS2 AOT already preserves the gameplay-critical EE accumulator order; Xbox SSE product is a candidate with a known ordering difference, so changing kernels adds an unclosed numerical obligation. |
| Heap | 26 | 12 | equal | Both module lifecycles require real allocation; existing native Permanent/heap services already remove console pool representations. General allocator-algorithm comparison remains unknown. |
| Materials | 76 | 55 | unknown | Only bounded module/container behavior is established; high-level material/shader and asset-consumer contracts are incomplete. |
| Geometry | 54 | 32 | unknown | The 40-byte module registration does not recover mesh/vertex/triangle/skinning semantics; shorter constructor code cannot select a complete rendering route. |
| Textures | 585 | 365 | equal | Native image/raster/texture owners and bounded PS2 atlas decode already exist. Shared scalar evidence is useful, while native backing formats differ; no full texture-decoder route advantage is proved. |
| World | 51 | 30 | unknown | A four-byte module slot and tested container lifetime do not establish sector/BSP/atomic draw algorithms or full scene linkage. |
| Viewport and HD presentation | 27 | 141 | Xbox | The actual Xbox setup supplies measured 1280x720/720x480/640x480 branches and widescreen parameters; it is the bounded HD reference. This is a reference choice, not adoption or visual parity. |

The chunk/header/registry candidates are independently rejoined to the original images. Xbox `002b97a0` and PS2 `0019f1e0` both enforce unsigned versions `0x34000..0x36003`; Xbox `002c24b0` and PS2 `001a2bc0` use extension type 3, header-read/skip dependencies, payload-plus-12 accounting and read/always callbacks. These matched operations and call shapes are E3/E5 identification, not a differential. The Xbox function's summed Ghidra size is not used as a contiguous byte interval; direct-control-flow decoding preserves its separate original ranges.

Current source inventory: 17 native renderer C++ files / 4543 lines versus 5 GS comparison files / 6433 lines. The original registration has 14 module rows and 28 constructor/destructor roles; the tracked AOT inventory and earlier authenticated joins already cover these callbacks. This does not establish a complete RenderWare ownership total or native scene renderer.

Continue transforming the existing generated PS2 units onto the existing native file, allocation, upload, sampler, fence and GPU boundaries. Modules 0401–0412 describe behavior/dependencies already represented in AOT; their typed conversion remains necessary. Rewriting those retail lifecycles by hand creates duplicates. GS/VU packet translation can exercise the current producers as an interim comparison route, but changing its target resolution alone retains guest GS/PSM/VRAM and VU state and does not establish correct projection, geometry, material or asset interpretation. PS2 packed textures, platform raster headers, skin/morph/sector layouts, alpha roles and movie timing remain separate risks.

Xbox's original video setup is a bounded HD reference; it supplies 1280×720, 720×480 and 640×480 branches. Rendering, logical UI coordinates, movie aspect/timing and simulation remain independent. No Xbox algorithm is adopted by this report.

The clean librw checkout's MIT license (copyright 2014 aap) was inspected at the commit recorded in the generated object, `18532c2e13efbc1aa43f0b00b4459831f9414de0`. Camera, material, geometry, matrix and plugin structures were read only for organization; no implementation was copied, and those structures do not prove either retail ABI.

Regenerate from the existing hash-pinned local images and tracked evidence:

```powershell
python -B tools/research/measure_xbox_capabilities.py --parent-root ../../.. --output build/oracle-telemetry/xbox-capabilities-replay.json
```

The analyzer uses the already installed Capstone x86 decoder. It regenerates PS2 registrations from the tracked parser, obtains seed extents by matching tracked original-body hashes, and remeasures current source inventories. Ignored historical telemetry and generated AOT C++ are not analyzer inputs. A missing librw checkout reuses the explicitly recorded tracked license audit without fetching or copying source.

Not verified: full function/subsystem coverage where bounds are stated as partial, cross-platform numerical/functional equivalence, complete asset layouts, native scene drawing and whole-game acceptance remain OPEN.

Contradictions with earlier documents: the old 0409 note's `001a4a80/001a4ac0` addresses are not the authentic registered constructors/destructors, which are `001c4a80/001c4ac0`. The earlier Phase 4 link has been replaced with this work item's actual capability record. Summed Ghidra body sizes are not contiguous intervals.

Falsification attempts: actual registration duplicate-ID mutation refused; changed original image identity refused; identification-only native differential promotion refused; mapping file offset used as VA refused; changed saved-listing original bytes refused; arbitrary grouped-instruction span refused; source-ID file offset used as VA selects different original bytes.
<!-- CAPABILITY_COMPARISON:END -->


The current native Windows architecture and verified PS2 behavior remain the target. Xbox functions are candidates for individual boundaries only after matching source behavior, errors, ordering, lifetime and asset identities. As documented in `PORT_STATUS.md`, targeted Xbox donor evidence has been adopted at verified boundaries—specifically Xbox 040C raster-extension metadata definitions in CoreInit registration (`research/runs/native-scene-registration-20261122/`) and 0407 scalar format/channel-conversion evidence in `runtime/src/native/retail_renderer_rasters.cpp` (`research/runs/native-xbox-renderer-0407-20261122/`). No console backend, Xbox platform library, or emulation model is adopted into the native runtime; all implementations bind to native host owners.

The owner-provided `.xiso` path is an extracted directory. setup-01/inputs.json pins its actual default.xbe and supplied extension sources. The core XBE loader and1624-entry XTLID analyzer compiled from those sources plus the official XboxDev/xtlid v0.1.2 table. The Gradle daemon could not connect over local networking. direct javac against the installed Ghidra jars completed successfully. Original extension source bytes are unchanged. The official XbSymbolDatabase3.1.160 scanner is installed with recorded ZIP/executable identities.

import-01 records the failed Windows batch parsing of the parenthesized input path. The next import uses a byte-identical copy at private-inputs/xbox-mka-20261121/default.xbe. The original source and PS2 project remain preserved. Analysis has no visible game window or audio output and never runs the game executable. Program loading, function mapping, sound request/completion/cancellation correspondence, source transformation and adoption remain unverified until matching records exist.

Import02 failed in LaunchSupport because its Java-home save file under Roaming is read-only. The JDK itself already compiled the extension successfully. Import03 uses the exact Java bootstrap class and VM options from support/launch.bat and support/launch.properties, with dedicated application settings/cache/temp directories. Existing Ghidra user settings and PS2 project are preserved.

Xbox import03 completed successfully and saved 22,579 discovered functions under local-research/ghidra-xbox/projects/MKAR-Xbox-20261121. Both symbol analyzers ran; the separate scanner returned 672 candidate labels. Original file-mapped bytes independently match the first 9 sound-catalog records except version-specific string pointers, and one 48-byte four-bank descriptor matches exactly. Whole catalog, callers, cancellation and behavioral equivalence remain UNVERIFIED. Targeted donor metadata and scalar formats have been adopted for 040C and 0407 per PORT_STATUS.md; wholesale donor runtime adoption remains rejected.

All344 original catalog records now match between PS2 and Xbox after excluding only each image's string pointer values. The comparison includes every other original word and exact bank name. All554 saved instructions across11 Xbox catalog candidate functions match their original file-backed section bytes. This establishes static data/byte correspondence only. Callback, queue/cancellation, sample identity, error, timing and lifetime equivalence remain under review. The next bounded export includes the actual Xbox job enqueue/cancel functions37C170/37C2A0 and native bank-retirement23C700, plus references to the mode entry pointer table56C960.

Sound-boundary02 exports six candidate functions, including actual enqueue37C170/cancel37C2A0 and the23C700 bank-retirement wrapper. All saved instruction bytes join the original XBE sections. Their C output is orientation only. ABI/callback/ownership and PS2 equivalence remain under review. No Xbox platform library is adopted into the native runtime.
