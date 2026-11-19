# Bounded host startup cleanup — 2026-11-19

Completed only host/main.cpp edits: removed five redundant native-enabling environment writes, removed MKA_INTERCEPT_COMBAT forced registration while preserving the exact is_mka recognition and single retail observer/leaf registration, appended the verified executable-parent games/mkarmageddon/disc-files/SLUS_214.10 candidate after all existing candidates, and corrected stale interpreter/default comments. Prior dirty source was snapshotted byte-for-byte; startup-main.patch contains only this batch's diff.

Static checks confirm all eight original candidates retain their order, exactly one extracted-content candidate is appended, the registration call remains exactly once and the redundant opt-ins/forced hook override are absent. Mandatory selectors/static AOT registration were not changed.

The existing Ninja object target required CMake regeneration in dry-run. With parent coordination, its exact saved per-object compiler command was run directly in the configured build_native_gl_full directory with original target flags. Compile exit0, empty stdout/stderr; objectSHA5374815568ea8b066c41c0fd3f6a0b104d2159a8caafb3740550c6676996ce52. No CMake regeneration, executable link or game launch occurred. Full command/source/object/before-after executable hashes are in host-object-build.json; executable bytes remained unchanged.

Source beforeSHAa5ab8c27a33b4aeb16c22e6c757e91bbb271af6b241efaafe10354416c320824; afterSHAd27527a619096794d0a0481b8d4797ffe037e666a83b74ff8f438558c91de275. This verifies one host translation unit, not a complete current executable or game behavior.

Root separately owns no-argument canonical launch routing and required window dependencies. Historical launchers/backups/config/settings were untouched. Existing historical VU_RECORD destination metadata code is retained; current dispatcher has no interpreter/live compiler. Full native player integration and remaining fixed-memory/device architecture are not closed by startup cleanup. The initial STARTUP_DEFAULTS_AUDIT is a historical source snapshot superseded for main.cpp by this record.
