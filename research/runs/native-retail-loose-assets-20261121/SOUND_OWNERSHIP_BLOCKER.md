# Next native sound owner boundary

The real-file queue check passes from clean source d8c6e3e63640621e02879e1a092f33fcf6a5ad53, with source-ordered permanent/announcr/shell_music/shell completions and missing/queued-cancel/active-cancel statuses2/3/4. These are actual files and native handles, not Main initialization or playback. The full-entry negative link remains exactly41 unresolved effects.

The concrete production composition gap is ownership. NativeRetailSoundBankJobs owns an AudioBankSession directly and publishes AudioBankHandle. Main.playback owns NativePlaybackOwner, which owns AudioBankService/AudioBankProtocol and their separate exclusive session; playback calls use explicitly associated BankWireToken values. Its current public API cannot consume the queue-produced native handle directly. AudioBankSession rejects foreign-session handles. Opening another session or converting catalog IDs/slots/pointers into tokens would not establish the required shared native ownership and lifecycle.

The next implementation needs a proven shared native bank owner/typed playback admission, the actual current-sound archive adoption at12D7E0, and live reference cleanup before retirement. Then the real catalog can publish pending before jobs, publish the actual handle before clearing pending on completion, and let12ABD0 observe that same catalog. Source84 describes this game contract. Missing live cleanup and context behavior remain named required effects; this work adds no successful production stub.

Evidence paths are the active fork research/runs/native-retail-startup-banks-20261121/clean-validation-01/ and parent source84. No game run, physical endpoint, frames or audio. Owner no action.
