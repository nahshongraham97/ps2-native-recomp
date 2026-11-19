# Sampling defaults — 2026-11-19

Removed five experimental environment overrides from native_gl_backend.cpp, replacing each lambda with the exact previous environment-absent float constant. Pixel shift0, texel offset0, sprite X shift-0.49, sprite Y shift0, triangle shift-0.49 are preserved, including surrounding arithmetic/order. No sampling correction or speculative axis change was introduced.

The existing test_gl_vs_raster fixture covers sprite UV/ST scales, bilinear/nearest cases, triangle strips/prim3 and font-atlas cases; it does not set these five variables. No test changes were necessary: root can run the same fixture with all five absent and again with deliberately different values to verify they no longer affect results. That execution has not occurred in this batch. Test font coverage requires the existing explicitly located font fixture. The fixture remains a software-reference comparison, not retail rendering/gameplay acceptance.

Source review confirmed the five old absent-variable defaults individually before replacement. JSON pins before/after backend bytes and reviewed test source. No build, graphics, game, driver/settings/model operation was performed. Root should serialize the existing test_gl_vs_raster target and changed ps2runtime backend object compilation.

Risk is limited to intentional removal of externally requested experimental shifts. Existing state/scene dimensions, sprite/triangle math, texture formats, and other controls are outside this patch; the existing GS-shaped path remains an open native-boundary concern. This does not establish universal sampling fidelity.
