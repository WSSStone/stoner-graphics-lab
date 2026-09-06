# Camera, Window and Input Contract

**Status**: Proposed Feature 030 contract. Public names below are planned additions, not existing APIs.

## Ownership and update order

Application exposes value-only `FFreeCameraState`, `FCameraChangeSet`, `FInputOwnershipSnapshot`, `FWindowDisplayState` and a reusable `FFreeCameraController`. Neither public headers nor events contain ImGui, GLFW or graphics objects. Existing `FWindow`/private `IWindowDriver` gain engine cursor and clipboard services; Core owns filesystem/system details where appropriate. There is one GLFW callback owner.

Per event interval:

1. Poll native events into a bounded ordered stream, retaining the shared sequence across input, focus, resize and scale events.
2. Apply focus/extent generations and update raw physical state. While unfocused, discard navigation downs and clear stale pointer state; still process focus/close events.
3. Forward committed text, named keys, modifiers, pointer, buttons, scroll and focus to the private ImGui adapter before its frame begins, even when capture was asserted previously.
4. Build panels; collect typed UI commands and current capture/activation. Resolve a press-owner ledger for Keyboard, PointerGesture and Scroll, using widget activation and ongoing capture, not hover alone.
5. Quarantine every currently held navigation key on a transfer to UI or focus/capture loss. Release followed by a fresh press is necessary to re-arm it. UI-captured classes generate no camera action. A mouse press retains its owner until release/cancel even outside bounds.
6. Validate a candidate camera/settings snapshot, then atomically commit a monotonic revision for the next eligible frame. No earlier speculative camera update may leak into that frame.

An event-budget overflow cancels camera/UI gestures, releases cursor, clears deltas and quarantines input. Report one bounded overflow diagnostic; never drop a release and continue moving. Close/focus-lost flags have an independent coalesced latch so overflow cannot suppress them.

## Camera values and numeric policy

| Value | Contract |
| --- | --- |
| Basis/projection | +X forward, +Y right, +Z up; existing positive-X StandardZ perspective and Y convention |
| Pose | Finite position and normalized orientation; navigation has yaw/pitch, no roll |
| Movement | W/S pitch-aware forward; A/D horizontal right; E/Q world up/down; normalize summed axes |
| Speed | 1.5 world units/s default; adjustable [0.01, 100]; Shift multiplies by 4 |
| Look | Right mouse capture; 0.003 radians per logical pixel, pitch clamped to ±89 degrees |
| Vertical FOV | [20, 90] degrees; default extracted from workload; wheel changes -0.035 radians/unit within bounds |
| Near/far | Workload defaults 0.1/100; import preserves valid values: 0.0001 <= near < far <= 1,000,000 |
| Elapsed time | Reject non-finite/negative; clamp to 0.25 s; first active/resumed interval is zero |
| Initial/reset aspect | Current drawable width/height, never the frozen projection's stored aspect |
| Matrix validation | Affine/orthonormal component tolerance 1e-4; inverse identity tolerance 5e-4 |
| Repeatability test | One-second straight-motion sequences at 30/60/120 Hz: absolute position difference <=1e-4 units; identical look-event sequence gives orientation/matrix difference <=1e-4 |

Time-cadence equivalence does not mean applying Euler translation during arbitrarily different rotation sampling must be bit-identical. Mixed motion tests replay the same timestamped event boundaries before each integration step; same timestamps and cumulative duration must produce the same state within the matrix tolerance. Near/far edits are import-only for this milestone; the UI need not expose a camera-lens editor.

`FCameraChangeSet` contains flags `ContinuousMotion`, `Reset`, `PresetRestore`, `Cut`, `ProjectionChanged`, `ExtentChanged`, a revision and old/new extent. FOV/near/far changes set ProjectionChanged. Reset/import set discontinuity even for equal numerical pose. No temporal buffer or jitter is introduced.

## Cursor, focus and keyboard behavior

RMB starts look only from a viewport-owned press. Disabled-cursor mode belongs to Application; release, Escape, focus loss, minimize, close or input overflow restores normal cursor mode and invalidates absolute-pointer baselines. Ignore synthetic cursor-warp deltas. The first Escape cancels capture/active widget; it does not force lab exit. Window close or the lab Exit command exits. The legacy calibration mode can explicitly retain its own Escape-exit action after platform policy is removed.

Expose Left/RightSuper and standard editing/navigation keys alongside existing keys. On macOS use Super for copy/paste; elsewhere use Control. Repeat events feed UI editing without repeated camera press ownership. Text arrives as committed Unicode scalars, with invalid scalars rejected, not inferred from physical key codes.

## Text, clipboard and scale

`FWindowDisplayState` carries logical client extent, drawable extent, native content scale and a monotonic generation. Drawing/hit testing uses logical coordinates; pixel clipping uses the actual drawable/client ratio. Font raster scale may follow content scale; it must not apply the drawable ratio twice.

Private services expose result-bearing `SetCursorMode`, `ReadClipboardUtf8`, `WriteClipboardUtf8`; no clipboard pointers escape their operation lifetime. Empty clipboard is valid; unavailable clipboard returns Unsupported, with no destructive edit. Reject invalid UTF-8, over-budget strings and embedded NUL. Preserve valid non-BMP text through copy/paste even when the bundled font shows a replacement glyph. English labels, bundled font coverage and no full IME are explicit scope.

F1 toggles the UI when not captured for text editing; a visible panel control also toggles it. UI-off leaves a documented F1 restore shortcut. Text widgets retain their key semantics. After hide, clear UI ownership and require fresh navigation presses. Panels overlay the full drawable scene rather than resizing its viewport; no docking, extra native viewport or world editing is introduced.

## Acceptance mapping

FR-002–FR-010; US1/US2/US5; SC-001/SC-002/SC-004/SC-006. Tests include initial widget click, drag-outside, wheel capture, simultaneous movement, release-to-rearm, focus-lost followed by queued downs, pointer warp, Escape policy, UTF-8 invalid/non-BMP round-trip, clipboard unavailable, 100/150/200 percent scale and restore at changed aspect. Numerical camera fixtures remain separate from physical display evidence.
