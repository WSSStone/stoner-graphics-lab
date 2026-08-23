# Feature 028 Sanitizers And Failure Regressions

## Status

T110 is partially complete. The current macOS arm64 worktree passed the focused
malformed-input, cancellation, failure-injection, rollback, shutdown, and
lifetime regressions below. Required Linux ASan/UBSan and applicable TSan runs
must execute on the final revision before T110 can be marked complete; macOS
results are not presented as Linux sanitizer evidence.

## Local Failure And Lifetime Gates

| Gate | Command | Result |
|---|---|---|
| Malformed glTF and bounded resolver/import rollback | `Build/Mac/Release/Tests/StonerTest --suite asset-gltf-malformed --suite asset-gltf-hardening` | PASS; 49-case malformed corpus plus resolver, limits, diagnostics, registry rollback, and mesh realization failure coverage |
| Cancellation, deadline, shutdown, and startup/shutdown races | `Build/Mac/Release/Tests/StonerTest --suite asset-manager-cancellation --suite asset-manager-shutdown` | PASS, 9 checks |
| Metal failure injection and Renderer realization rollback | `Build/Mac/Release/Tests/StonerTest --suite metal-failure-injection --suite renderer-static-model` | PASS; 9 named Metal failure points, zero terminal ownership, and 25 aggregate realization/rollback checks |
| Manager-independent handle lifetime and Feature 028 failure contracts | `Build/Mac/Release/Tests/StonerTest --suite asset-manager-lifetime --suite production-content` | PASS, 6 checks |

Metal failure evidence returned device, object, submission, and in-flight
ownership to zero. Renderer failures covered buffer, texture, shader, layout,
descriptor, sampler, pipeline, cancellation, and device-loss boundaries without
publishing a partial snapshot.

## Pending Final-Revision Linux Gates

The final closeout must run at least:

```text
scons config=debug strict=1 sanitizers=address,undefined
Build/Linux/Debug/Tests/StonerTest --suite asset-gltf-malformed --suite asset-gltf-hardening --suite asset-manager-cancellation --suite asset-manager-shutdown --suite renderer-static-model --suite production-content

scons config=debug strict=1 sanitizers=thread
Build/Linux/Debug/Tests/StonerTest --suite asset-manager-cancellation --suite asset-manager-shutdown --suite asset-manager-concurrency --suite renderer-static-model --suite production-content
```

ASan/UBSan must report no memory/undefined-behavior diagnostic. TSan is
applicable to Asset Manager and CPU Renderer contract suites; native GPU/runtime
lanes remain owned by their non-TSan jobs. Run IDs, revision, artifact names,
and digests will be recorded in `Validation/028/CI/README.md` before T110 is
checked.
