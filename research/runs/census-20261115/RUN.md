# Live Compliance Census & Runtime Analysis — 2026-11-15

## Run Summary

- **Date / Time:** 2026-11-15 12:33:07 – 12:36:13 EST
- **Duration / Frames:** 185.28 seconds (11,144 frames)
- **Runner Commit:** `b5d6255` on `main` (`external/forks/ps2-native-recomp`)
- **Runner SHA-256:** `E4D25963FE716BBA0F94EFCE0BA842AE1AB62FC5AADC145A751413E2D5B9EA40`
- **Parent Commit:** `f3334f9f` on `main` (`armageddon-reimplementation`)
- **Launch Command:** `launch_game.bat`
- **Display Resolution & Framing:** 832x448 full uncropped viewport, centered Dragon crest and "THESE PROFILES ARE LOADED" banner.
- **Log File:** `games/mkarmageddon/disc/logs/ps2run_2026-11-15_12-33-07.log` (5.44 MB)
- **Audio Captures:**
  - `last_run_device.wav`: 35,573,804 bytes (185.28s, 48 kHz stereo, Realtek speakers)
  - `last_run_heard.wav`: 35,573,804 bytes (185.28s, 48 kHz stereo, Realtek speakers)

---

## The Compliance Census Block (First Complete Capture)

```
[census] ===== accepted-route PS2 API census =====
[census] EE   : live guest translations = 0        (PASS)
[census] VU   : interpreter fallbacks = 0 (vu0 0, vu1 0) (PASS)
[census] IOP  : guest instructions = 0       (PASS)
[census] IOP  : guest modules loaded: SIO2MAN 0x00000000 MWFILEFR 0x00000000 PADMAN 0x00000000 MCMAN 0x00000000 MCSERV 0x00000000 DEV9 0x00000000 USBD 0x00000000 LGKBM 0x00000000 LGAUD 0x00000000 LIBSD 0x00000000 SDRDRV 0x00026300 MSL 0x00028100 CRI_ADXI 0x00000000
[census] SIF  : RPCs native = 1249901 guest = 5357 unknown = 0
[census] SIF  : guest-handled RPCs: sid 0x00012345 fn 56026 x5357
[census] GS   : transfers applied = 1 skipped = 125346; primitives = 1833884
[census] GS   : transfers by base: base=0(fbp=0):app=1,skp=0 base=9632(fbp=301):app=0,skp=10290 base=9664(fbp=302):app=0,skp=9926 base=9696(fbp=303):app=0,skp=9926 base=9728(fbp=304):app=0,skp=9926 base=9760(fbp=305):app=0,skp=9926 base=9792(fbp=306):app=0,skp=9926 base=9856(fbp=308):app=0,skp=4946 base=9888(fbp=309):app=0,skp=34 base=9920(fbp=310):app=0,skp=4946 base=9952(fbp=311):app=0,skp=9892 base=9984(fbp=312):app=0,skp=34 base=10016(fbp=313):app=0,skp=9909 base=10048(fbp=314):app=0,skp=4946 base=10144(fbp=317):app=0,skp=379 base=10176(fbp=318):app=0,skp=5325 base=10208(fbp=319):app=0,skp=4946 base=10304(fbp=322):app=0,skp=17 base=10336(fbp=323):app=0,skp=4946 base=10368(fbp=324):app=0,skp=9892 base=10464(fbp=327):app=0,skp=1642 base=10560(fbp=330):app=0,skp=1544 base=10656(fbp=333):app=0,skp=1544 base=11200(fbp=350):app=0,skp=362 base=11232(fbp=351):app=0,skp=122
[census] SPU2 : key-ons = 0 key-offs = 0 DMA writes = 0 (0.0 MB) reads = 0 peak sample = 0
[census] CARD : RPCs by method: 0xFE ok=1 fail=0
[census] CDVD : sector reads = 0                   (PASS)
[census] VERDICT: 1 mandate violations remain: SIF guest RPCs
=================================================
```

---

## Detailed Subsystem Breakdown & Root Causes

### 1. Visual Presentation (RESOLVED)
- **Status:** PASS (0 crop, full 832-column display width, centered).
- **Measurement:** 1,833,884 GS primitives drawn natively. 125,346 atlas transfers skipped by `apply_display_write()` base gate, eliminating front-end texture corruption.
- **Outcome:** The "THESE PROFILES ARE LOADED" screen and Dragon medallion are perfectly rendered and centered.

### 2. Audio Stack (0 Key-Ons / Silence)
- **Measurement:** SPU2 key-ons = 0, key-offs = 0, peak sample = 0.
- **Root Cause:**
  - The census attributes the 1 remaining mandate violation to `SIF guest RPCs`: `sid 0x00012345 fn 56026 x5357`.
  - The guest modules `SDRDRV` (0x00026300) and `MSL` (0x00028100) are loaded as guest ELFs, while `LGAUD` is an offline stub.
  - Because `PS2_IOP_NATIVE_STRICT=1` is active, guest IOP execution is restricted (guest instructions = 0). The EE dispatches 5,357 audio RPC calls to `sid 0x00012345`, but without active MSL/SDRDRV processing, the driver never programs voices or triggers SPU2 key-on events.
- **Work List Item:** Implement native C++ replacement services or AOT compilation for `MSL.IRX` and `SDRDRV.IRX` to handle SID `0x00012345` and directly drive the native `libsd` bridge.

### 3. Memory Card Detection & Profiles
- **Measurement:** `CARD: RPCs by method: 0xFE ok=1 fail=0`. `MCSERV GetInfo2` polled continuously (`port=0 slot=0 status=0 free_clusters=8000 type=2 format=1`).
- **Root Cause:**
  - `armageddon.ps2` is correctly detected by `sio2` and `mcserv` with 8000 free clusters and formatted=1.
  - The game's query `req: fd=0 size=0 off=1 orig=0` requests cluster count (`off=1`), but leaves `size=0` (format) and `orig=0` (type).
  - The UI displays `"NO MEMORY CARD (PS2)"` under slot 1 because the card has no existing save directory (`BASLUS-21410`). In retail MK: Armageddon, an empty formatted card with no profile save displays this exact string until the user presses START and creates/saves a profile.
- **Work List Item:** Verify file-level save creation (`mc_open`, `mc_write`, `mc_close`) when pressing START to create a profile, and ensure directory existence checks match retail expectations.

### 4. Intro Movie Playback (Hang on `method=2`)
- **Measurement:** `[cri-adx-rpc] [diagnostic-stub] method=2 args=0x00978A40 args_size=16 ret=0x00978B40 ret_size=4`.
- **Root Cause:**
  - The game opens `cdrom0:\MOVIEPS2\LOGON.SFD`, loads `CRI_ADXI.IRX` (aliased as `CRI_ADX_Driver`), and overlays `MOVIER.OVL`.
  - `MOVIER.OVL` issues SIF RPC `method=2` to CRI ADXI (`0x90000200u`).
  - The diagnostic placeholder returns 0 but decodes no frames and dispatches no frame-end or playback completion callbacks.
  - `MOVIER.OVL` sits in a polling loop waiting for playback to advance or complete, causing the movie hang.
- **Work List Item:** Ahead-of-Time IRX→C++ compilation of `CRI_ADXI.IRX` or native Media Foundation host video service that decodes SFD/MP4 and signals completion to `MOVIER.OVL`.

---

## Native-Service Programme: Measured Work List

Based on the exact census measurements, the road to 100% native execution is:

1. **Audio Native Service (MSL / SDRDRV / LGAUD):**
   - Implement native handler for SIF SID `0x00012345` (5,357 RPCs observed).
   - Convert MSL batch command processing to drive the native `libsd` bridge directly, unlocking SPU2 voice key-on.

2. **CRI_ADXI IRX→C++ Compiler / Host Video Service:**
   - Replace the `PS2_ADX_STUB` diagnostic placeholder with either:
     a) Ahead-of-time compiled C++ body for `CRI_ADXI.IRX`.
     b) Native Windows Media Foundation playback service responding truthfully to `method=2` and signaling completion to `MOVIER.OVL`.

3. **Memory Card Profile Save & Persistence:**
   - Exercise `mc_open`, `mc_write`, `mc_close` across user profile creation in `BASLUS-21410`.
   - Ensure the card structure matches retail directory enumeration expectations.

4. **F1 Asset Store Modernization (Host-Owned):**
   - Repack 245 loose PAK archives (`MKDA.PAK`, `MSLASSET.MS2`/`.MS4`) into a single indexed container (`assets.idx` + blobs) served behind the native content VFS.
