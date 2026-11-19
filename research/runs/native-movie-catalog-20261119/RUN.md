# Native retail movie catalog and asset components — 2026-11-19

validation-02/verification.json reports PASS_SCOPED_NATIVE_CATALOG_AND_ASSETS
with explicit executed/audited binary identity joins and 38 pre/post full asset
hash checks. validation-01 preserves the earlier checker and its consistent
observed identities; independent review caught and closed a reusable missing
identity-join assertion before publication.
All 66 actual native public API requests (33 IDs, both views) match a separate
original-ELF oracle. The semantic catalog has 41 selectable keys, distinct from
38 explicitly bound actual asset files whose full SHA256 and lengths are pinned.
The native-file integration fixture succeeds on 62 requests across all 38 files;
four unknown_key requests name exactly three missing assets: BLITZN.SFD,
BLITZW.SFD and KRYPT/V_FIN.SFD. V_FIN has no alternate and both requested views
select the same missing key. No missing-file alias/default or eager 41-entry
AssetStore is used.

validation-01 retains build.json, catalog.json, asset-binding.json,
original-requests.json, assets.tsv, launch.json and strict symbol audit receipts
for both targets. Both symbol audits pass with no forbidden namespace symbols.
The oracle records five negative checks. The verifier pins native headers,
source/generated includes and test sources. These are scoped root build/run
observations; the original parent source-handoff hashes remain historical.
compile-rejections.json records one positive compilation and seven rejected
missing/malformed datasets. emitter-rejections.json records one deterministic
positive regeneration and eight rejected review/observation/pin cases.
Production source/data remained unchanged. compile-inputs-supplement.json is a
post-run copy of unchanged fixture inputs checked against the original hashes.

Actual retail caller/factory and game input/view/language producers remain
unbound. Native player clock, audio mixer, decoder/presentation integration and
full-game acceptance remain OPEN. No game, decoder, audio/video device, driver
or settings operation occurred. See parent native-movie-ingress-20261119/
CATALOG_IMPLEMENTATION.md for source/API scope; CATALOG_ASSET_BINDING.md records
the separately owned integration contract.
