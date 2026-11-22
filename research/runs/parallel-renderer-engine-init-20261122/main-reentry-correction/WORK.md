# Main transfer correction

Parent review found a concrete dcf002 contract violation at construct_registered:catching NativeMainReentry set after_effect_unknown_,so a legitimate transfer incorrectly poisoned subsequent registry inspection/cleanup. Parent AGENTS.md217..220 states:"Do not poison the owner with an error-only latch for this transfer."

Production change removes only that assignment. Transfer propagates;Busy guard unwinds;the preceding successful constructed prefix stays retained. Ordinary native exceptions still set the distinct after-effect latch. No callback is replayed or failed constructor appended during transfer.

Focused real native callback test transfers to actual FixtureMain with value23 after the real ErrorDomain constructor has succeeded. It checks exact Main identity/value,no error flag,14 inspectable registry entries,one live actual prefix instance,successful explicit prefix cleanup tocount0,and successful construction/cleanup afterward. A separate real registry case throws an ordinary native exception and verifies that inspection/cleanup remain blocked by the error-only latch. Other unimplemented factory bodies/tails remain explicit test fixtures.

validation01 freshly rebuilt/executed core,allocator,Session clients successfully after the production change. validation02 compiled only the focused core client after improving its assertion to check optional presence safely,then executed it successfully;allocator/Session exact unchanged binaries/receipts are reused. No broad unchanged rerun. Final856 raw source pins and exact unchanged60 missing names are recorded. Build02 observation is the actual exec tool command/result,not a fabricated raw compiler log. Original dcf002 source bytes are gzip-pinned in BEFORE.json and original06 native binaries remain under private build/history06. All existing frozen original source/results remain intact.

This corrects native control-transfer behavior;it adds no real missing renderer factories,object-extension services,platformopen/start or whole127260 implementation. No canonical/audio/profile/assets/model/driver/window/speaker/device/push/merge operation.
