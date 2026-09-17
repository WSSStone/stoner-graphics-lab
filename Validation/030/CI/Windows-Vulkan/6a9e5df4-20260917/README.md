# Physical Windows validation at the reference-provenance freeze

Software: `6a9e5df4be780b31a8e3a9ec3ee18b35f0512838`. Physical NVIDIA RTX 3080, driver 32.0.15.8132, active RDP-Tcp#0 session 1, Default input desktop. D: is fixed local NTFS. RDP provides no physical-monitor scanout authority or Console equivalence.

Fresh strict Release build and both strict-files cooked packages passed. The 49 output validation Python tests, 14 interactive validation Python tests, 598 focused native assertions and output architecture check passed. The 84 freeze policy digests match committed bytes. No renderer code, output profiles, tolerances or Accepted records changed.

All five prescribed run/verify cases pass: Sponza endurance 120 warmup + 1000 measured frames; Lantern smoke 120; Sponza lifecycle 1120 frames/180 steps; Sponza mode 1120 frames/61 steps; Lantern integration 120 frames/12 steps. All have zero captures/readbacks/live idles and zero final native/presentation owners. Native maintenance1 is advertised and used by four cases, with Proven shutdown. Forced AcquireHistory smoke retains IdleAssumed: 117 proven releases, 3 pre-cleanup owners, compatibility terminal idle only.

The surface supports sRGB only. Sponza records 41 unsupported rejections and 20 retained-current requests; Lantern integration records 2 and 1. Neither claims an actual cross-profile switch. All rejected states and following presentations were independently checked.

The initial local preflight still assumed Console-only and rejected the allowed active RDP transport; that diagnostic is retained. The first endurance native run passed but its wrapper omitted SESSIONNAME and independent verify correctly rejected it. Its original reports remain here as failed-attempt evidence, not accepted machine results. The local orchestration now queries WTS directly. All five accepted cases used entirely new output paths and canonical committed file bytes after a fresh rebuild/recook. No report was patched to pass.

`checkpoint.json` links successful originals under SDR/UI and failed-attempt originals here. Raw commands/logs, cooked packages, leases and TEMP remain in the frozen checkout's ignored `Build/Validation/030/reference-fix-rdp-20260917`; initial attempt logs remain in `reference-fix-20260917`. The entry points were `prepare.py`, `cases.py`, `run_cases.py`, `package_windows.py`; each native invocation used existing `interactive_lab_validation.py run`, followed by `verify`, then independent `closeout`.

M4 must run its fourteen fixed cases at this new SHA. Formal 512x512 commands and separate human launch commands are prepared, not executed, pending same-SHA lifecycle prerequisites. Old 74e1c435 human decisions are historical and not copied forward.
