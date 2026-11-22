# Focused root GPU correction review

Uncommitted root correction at native-retail-permission-textures-20261122 validation01: one remaining admission bypass found. This reviewer ran no GPU/IO tests and edited no canonical/author source. Root was notified promptly. This record preserves the pre-fix17-path source snapshot; final admission acceptance awaits the follow-up correction.

## Corrected movie order

validate_image_upload now keeps arithmetic/byte checks before submission but moves INT_MAX beside queried host limits AFTER collect_video and incoming GL error handling. The old movie API still calls validate_movie_upload first, preserving its original invalid_frame overflow policy. The representable huge-span ordering edge is source-reviewed only, not runtime-exercised or allocated. This resolves the earlier c27b8bb ordering finding.

The real small2x2 movie upload control uses valid conversion metadata under a real incompatible caller PBO, sentinel texture binding and unpack alignment. It checks real frame/metadata creation and state restoration, then malformed bytes refuse through the original validator. This is appropriate coverage of the shared storage API; it does not read back movie pixels, present, decode or play a movie. Existing105 ordinary-image exact readbacks remain separate.

## P2: public drain/resume bypasses failed-delete Session seal

The new image_delete_outcome_unknown checks in movie_current and Session::poll_retirement/retry_retirement correctly block normal texture/movie calls, object-name access and automatic retirement deletion retry. Every plain-bool read in those paths follows actual owner-thread/context admission, so no new wrong-thread plain-bool read was found there. Unknown ownership remains retained and actual borrowed-context recovery is explicitly required; this seals only the failing Session, not all Sessions sharing the GL context.

However GlMovieBackend::begin_context_drain, poll_context_drain and retry_context_drain call completion begin/poll/retry directly after only owner/context/cleaned checks. They never inspect image_delete_outcome_unknown, so a failed revoke can still be followed by real fence insertion/poll/retry on the sealed Session. resume_after_drain likewise lacks the sticky flag and can report successful resume, contradicting the new native_gl_texture.hpp contract that Session resume is refused. Refuse all four methods on the sticky condition AFTER their owner-thread/context admission. Resume requires that admission before reading the non-atomic flag; do not simply read it on arbitrary threads. Do not replace retirement admission with revoked-rejecting movie_current, since retirement intentionally occurs after ordinary revocation.

This is a source-proved control-flow bypass, not a fault-injected GL error result. The failed-delete outcome and actual borrowed-context recovery remain uninjected/unimplemented. Normal successful retirement must remain available for unsealed Sessions.

## Saved evidence

Validation01 authenticates887 exact source pins at this reviewed snapshot, all saved raw command/log hashes and source manifests, ten current binary identities (seven executed by primary, three compile-only),106 readback files and exactly63 required unresolved effects. Source snapshot17paths is retained compressed; correction patch/JSON are pinned unchanged. These saved component results do not prove the unexercised sticky branch safe. No reviewer execution or whole-game/physical endpoint acceptance claim. Prior GPU/asyncIO/permission review records and rejected-profile rollback remain unchanged.
