# Local source-review model workflow

Current rule, 2026-11-18. This file owns the local-model procedure; historical
copies do not override it. The default documented 9B model is
`huihui-qwythos-9b-claude-mythos-5-1m-abliterated-i1`. Do not use a 4B model.
At most two hosted Sol subagents may run concurrently; the available hosted
Sol model is `gpt-6.1-sol`. Preserve model files, loaded context, saved settings,
and MCP configuration. Never kill LM Studio or unload a model to cancel work.

## Prepare one source question

1. Check `lms ps --json` for the loaded identifier, status and queued requests.
   Do not overlap local requests. If none is loaded, use direct source review
   and record that limitation; do not change the saved profile.
2. Give one exact local path, verified file length/range, one factual question,
   and the relevant project context. Request a finding with literal line
   evidence. A focused task is not an output-token or reasoning limit.
   Verify that the selected range actually includes every assignment or
   symbol needed to answer. Do not ask the model to infer omitted fields.
   For generated formatting/control order, verify actual emitted output;
   an argument-list transcription alone cannot establish generated behavior.
3. Inspect the actual tool configuration. The 2026-11-18 check of
   `~/.lmstudio/mcp.json` found an empty `mcpServers` map. In that
   configuration use `tools/research/local_source_review.py`: its request-scoped
   `read_file` tool reads repository files and returns numbered lines. Do not
   claim that this bridge uses a configured MCP server.
4. If relevant MCP integrations are configured later, use the native streaming
   `POST /api/v1/chat` route with the exact integration and read tool. Do not
   send unsupported integration identifiers or browser calls for local files.
   The function-tool bridge instead uses `/v1/chat/completions`; it does not
   provide LM Studio's stateful MCP chat features.
5. Never set output-token, reasoning, sampler, context, or generation-time
   overrides. Keep the owner's saved inference settings. Supply the tools
   needed by the assigned task; broader research needs separate relevant
   tools, not a source-reader falsely described as a general-purpose agent.

## Stream, monitor, and correct

1. Consume SSE incrementally with streaming enabled. Retain the exact process
   handle. If orchestration returns a running cell ID, wait on that cell before
   polling its returned process ID. Output truncation is not a model failure;
   inspect the retained trace and same process before starting another request.
2. Record streamed text/reasoning events, tool names/arguments/results and
   completion status in the request trace. The bridge currently records all
   raw deltas; it prints content deltas, which this model may include `<think>`
   text in. Distinct reasoning fields are retained in the trace. Provide the
   user a source-grounded findings summary; do not describe a hidden field as
   having been displayed. Do not flood the conversation with whole tool results.
3. Productive generation may finish without a time or token cap. Long duration,
   high token count or a long answer alone is not a loop. Parser errors must be
   recorded while the stream continues to drain.
4. **Cancel a confirmed loop at request level.** Repeated identical reads or
   substantive repeated reasoning without new evidence justify interruption.
   Record the exact repetition, close only that request, and verify the server
   is idle before retrying. The former blanket no-interruption rule is revoked.
5. Correct the specific failure: malformed tool arguments, empty/out-of-range
   reads, missing path or faulty inference. The exact-file reader must report
   file length and returned range and reject starts beyond EOF. Do not send
   repeated empty reads or ask the model to reconsider indefinitely.
6. Independently check every claimed line and inference against the current
   source. Source presence does not prove runtime reachability, correctness or
   whole-game acceptance. Send one focused correction with the discrepancy and
   source, then verify it. An unresolved correction stays explicitly unresolved;
   another attempt on that same claim requires the owner's instruction.
7. Record completion, useful evidence, unsupported claims and any cancellation.
   When another useful task remains, send it within six minutes after the last
   output, without overlapping or sending filler. Task scope stays narrow;
   inference output and reasoning stay uncapped.

## Current evidence and limits

The bridge and its file-range/loop handling are in
`tools/research/local_source_review.py`. Per-request records and raw streaming traces are local evidence, not verified facts by themselves. Current examples
are recorded in `research/doc-audit-20261118/`; unsupported donor-to-Armageddon
inferences are discarded even when the model successfully read the source.

Do not use Modly or computer automation for LM Studio. Use direct server
access. For Ghidra tasks use a private project copy per session and verify the
actual program identity; do not infer another agent's session or tool access.
