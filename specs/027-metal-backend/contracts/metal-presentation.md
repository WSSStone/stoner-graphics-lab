# Contract: Metal Presentation

## Ownership

- Application exclusively owns the desktop window and its Cocoa view.
- The Metal presentation context borrows the validated platform-window handle.
- Backend creates, attaches, configures, detaches, and destroys its own
  `CAMetalLayer` on the required main thread.
- The layer is detached before the borrowed view/window becomes invalid.
- A drawable is frame-scoped and is never retained into an unrelated frame.

## Attach

Attach fails for headless, null, stale, foreign, closing, or unsupported window
handles. On success the context records stable window/layer identity, device,
pixel format, colorspace, logical size, framebuffer pixel size, display scale,
and maximum in-flight frame count. Partial attachment is unwound exactly once.

## Frame Lifecycle

1. Refresh logical size, framebuffer size, and display scale.
2. If extent is zero or the window is closing, return `Paused` without acquiring
   or submitting a drawable.
3. Apply pending layer configuration and set `drawableSize` in pixels.
4. Acquire at most one drawable for the frame.
5. If no drawable is temporarily available, return `Unavailable` without retry
   spinning and preserve a recoverable state.
6. Encode rendering into the drawable texture, present it on the same ordered
   command buffer, and retain the frame slot until native completion.
7. Release drawable/frame ownership at completion.

## Resize And Recovery

Resize generation is monotonic. A frame created for an old generation cannot be
submitted after reconfiguration. Minimize, occlusion, and temporary drawable
absence pause presentation without converting the device to failed. Restore
revalidates extent/configuration before the first resumed submission.

## Shutdown

Shutdown rejects new frames, drains presentation submissions, releases every
frame-scoped drawable, detaches the layer, clears its device reference, and then
releases layer ownership. The window remains Application-owned throughout.

## Visible Acceptance

An accepted run records at least 3,000 submitted/presented frames, 20 completed
resize/minimize/restore cycles, one correctly oriented capture with digest, zero
unrecovered lifecycle errors, clean close ordering, and exit code 0. Hosted
offscreen evidence cannot substitute for this tier.
