**Unresolved historical model hypothesis (2026-11-12; not a finding):** The -5 fallback claim in mcserv.cpp is likely a generic SDK comment not fully linked into this specific build, while Ghidra will reveal whether the actual code path expects -5 or something else at 0x0010F360. The model proposed checking a log and binary/source; no returned tool result or completed comparison appears in this document.

### Proposed log-read request (not proof the request executed)
**Tool:** filesystem-mcp
**Target:** `build/native-mkarmageddon-aot-current/diagnostics/native-mcserv-check-20261112/strict-boot-300.log`

```json
{"tool": "filesystem-mcp", "args": {"path": "C:\\Users\\nahsh\\mk port\\Armageddon-backup\\armageddon-reimplementation\\external\\forks\\ps2-native-recomp\\build\\native-mkarmageddon-aot-current\\diagnostics\\native-mcserv-check-20261112\\strict-boot-300.log", "name": "strict-boot-300.log"}}
```

**Historical proposed next steps (not current user instructions):**
- **User:** No action requested by this archived review.
- **Agent:** The original proposed PC/register investigation was not completed here. Current work follows root ROADMAP.md; do not treat that speculative address association as a recovered contract.

---


## Documentation audit evidence — 2026-11-18

The exact `strict-boot-300.log` file exists. Its lines114–118 record the MCSERV replacement and declined XMCMAN 0xFE result -5; line147 says it stopped after 28 frames with Exit. The filename does not establish 300 frames. This supports the reported log events, not the proposed SDK/binary hypothesis, a causal exit explanation, current MCSERV behavior, or a matching clean executable. The JSON above is a preserved proposed tool call, not a tool response.
