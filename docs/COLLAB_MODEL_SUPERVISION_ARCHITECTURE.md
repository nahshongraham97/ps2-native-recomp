# Collab Model Supervision & Tool Oversight Architecture

## 1. Overview

This document details the configuration, delegation structure, and runtime oversight harness for local LLM collaboration on the *Mortal Kombat: Armageddon* native recompilation project.

Local models serve as specialized static auditors, trace parsers, test validators, and code inspectors. To guarantee reliability and prevent runaway loops or hallucinations, models operate under an active streaming supervision harness with real-time tool injection.

---

## 2. Configured Model Roster & Role Specialization

Only local models with pre-existing configurations in LM Studio are permitted. Their configurations (prompt templates, context lengths, samplers, and GPU offloading) remain untouched.

| Model ID | Parameter Scale / Arch | Specialized Role | Core Responsibilities |
|---|---|---|---|
| **`huihui-qwythos-9b-claude-mythos-5-1m-abliterated-i1`** | 9B / Claude-Mythos (1M ctx) | **Lead Architectural Auditor & Long-Trace Analyst** | End-to-end subsystem audits, massive SIF/RPC execution trace analysis, cross-referencing PS2 hardware specs against native C++ implementations, and acceptance gate verification. |
| **`huihui-qwen3.5-9b-abliterated`** | 9B / Qwen3.5 | **C++ Logic & Algorithm Auditor** | Deep review of low-level algorithms: MPEG-2 VLC bitstream parsing, IDCT, SPU2 ADPCM block decoding, pitch formulas, and OpenGL shader state mapping. |
| **`qwen3.5-9b-deepseek-v4-flash`** | 9B / DeepSeek-v4 Distill | **Static Analysis & Edge-Case Prober** | Probing for boundary bugs, off-by-one errors, texture coordinate clamping bounds, race conditions in non-blocking RPC callbacks, and unhandled hardware register edge cases. |
| **`nvidia-nemotron-3-nano-4b`** | 4B / Nemotron | **Test Oracle & Regression Validator** | Auditing test suites (`test_gl_vs_raster`, `test_gl_lazy_readback`, `test_spu2_mix`), verifying test coverage against retail edge cases, and checking acceptance census metrics. |
| **`qwen3.5-4b`** | 4B / Qwen3.5 | **Fast Telemetry & Diagnostic Parser** | Rapid, low-latency parsing of build logs, register histograms, VFS file tables, and diagnostic outputs without GPU VRAM overhead. |

---

## 3. Tool Injection Layer (Function Calling Without MCP)

Because LM Studio lacks pre-configured MCP tools, tools are provided directly at the API request level via standard OpenAI-compatible function calling schemas:

1. **`read_file(path, start_line, end_line)`**: Inspects exact source code line ranges with dual-workspace path resolution (`external/forks/ps2-native-recomp` and `armageddon-reimplementation`).
2. **`grep_search(directory, pattern, file_extension)`**: Recursively searches for symbols, registers, opcodes, or constants across the repository.
3. **`list_directory(path)`**: Discovers workspace structures, test outputs, frame captures, and asset trees.

---

## 4. Real-Time Stream Monitoring & Loop Interception Harness

All model interactions are managed by the runner harness in `scratch/lm_agent.py`:

```
+-------------------------------------------------------------+
|                  Local LM Studio Endpoint                   |
|               (http://127.0.0.1:1234/v1)                    |
+-------------------------------------------------------------+
                              |
               Server-Sent Events (stream=True)
                              v
+-------------------------------------------------------------+
|               Real-Time Stream Monitor                      |
|  - Token-by-token parsing                                   |
|  - Sliding-window text repetition detector (2000 chars)     |
|  - Duplicate tool-call interceptor                          |
+-------------------------------------------------------------+
         |                                           |
    [Loop / Repetition]                        [Valid Action]
         |                                           |
         v                                           v
+-----------------------------+             +-----------------+
| Immediate Socket Abort &    |             | Local Tool      |
| [CORRECTION SYSTEM NOTICE]  |             | Execution       |
+-----------------------------+             +-----------------+
         |                                           |
         +--------------------+----------------------+
                              |
                              v
               Tool Result fed to Assistant
```

### Safety & Grounding Guardrails

1. **Token Streaming**: Requests are sent with `"stream": true`. Tool chunks and response tokens are parsed incrementally as Server-Sent Events (`data: {...}`).
2. **Identical Tool-Call Interception**: Tracks `(function_name, arguments)`. If an identical call is attempted more than once, it is blocked immediately with an error notification prompting the model to synthesize with existing data.
3. **Sliding-Window Text Repetition Detection**: Monitors the trailing 2,000 characters for identical multi-word phrases. If an infinite generation loop starts, the connection is instantly severed.
4. **No-Hallucination Policy**: If a tool reports an error or file not found, the model is strictly forbidden from speculating or guessing contents. An automated correction notice forces the model to rectify its query using verified data.
