# Local source-review coordination

This fork follows [MODEL_COORDINATION.md](../MODEL_COORDINATION.md). That file is the current model/tool/request policy; this page does not maintain another model roster or configuration.

The earlier version described an active `scratch/lm_agent.py` harness, automatic duplicate-call blocking, and guaranteed loop/hallucination prevention. That script was not present in either checkout at the 2026-11-18 audit, and those deployment/guarantee claims were unsupported. They must not guide current requests.

The parent repository documents [local_source_review.py (canonical parent)](https://github.com/nahshongraham97/armageddon-reimplementation/blob/main/tools/research/local_source_review.py) as the streaming exact-file function-tool bridge when LM Studio has no configured MCP integration. Its existence establishes source availability, not that a model is loaded, a request ran, or a review is correct. Inspect actual tools/loaded state before each request; preserve saved/loaded model settings. Follow the parent policy for request scope, monitoring, confirmed-loop interruption, idle verification, and reconciliation. No model is a test oracle merely because its name appears in an old table.

Use verified repository evidence to resolve every returned factual claim. An unavailable model or tool remains an explicit limitation; it does not certify source or require changing the user's settings.
