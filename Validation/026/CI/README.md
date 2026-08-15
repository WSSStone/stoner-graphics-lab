# Feature 026 CI Evidence

Each Windows, macOS, and Linux Debug/strict Release job, plus Linux ASan/UBSan
and TSan, uploads exactly one uniquely named normalized artifact even on
failure. This directory records workflow run IDs, job conclusions, artifact
names, downloaded SHA-256 digests, and the final revision. Raw Actions logs and
downloaded artifact payloads are not committed.

## Successful Closeout Run

- Workflow run: `31882332020`
- Implementation revision: `8427e13354aa4b6f5aaae4330334eaebcd1acc6f`
- Conclusion: `success`
- Duration: `16m 34s`
- Matrix: Windows/macOS/Linux Debug, Windows/macOS/Linux strict Release,
  Linux ASan/UBSan, and Linux TSan

| Artifact | SHA-256 |
|---|---|
| `runtime-asset-manager-debug-linux` | `edfca57469885d30f676a524632405a745c5280130a4ddc62d37e571017afab6` |
| `runtime-asset-manager-debug-macos` | `0fb36f0b7ca571432efe8be73b85d94645ca331201c63735e2eaa51bd9bba522` |
| `runtime-asset-manager-debug-windows` | `b8b631054d3b38c6b80ed1bcaa88510e074372dec99a63cf65ffebe2cdd4162c` |
| `runtime-asset-manager-release-linux` | `e5de251f686a4907ebb483fe2395006105ed6adcf0e29dc658d2a5db3849b6c4` |
| `runtime-asset-manager-release-macos` | `b012f29f3b02f8ba8fc73bed10cb854718d0546c6b74986c2968a1d8f738ee85` |
| `runtime-asset-manager-release-windows` | `3d68bc7678f42f8ebc268e0aa47cfeec3c3587cccc5f1e7f8a91ee131a06b561` |
| `runtime-asset-manager-linux-asan-ubsan` | `a4f3b6848739aa66c04b5906dc0a2c7e4ffded2319d55f7ca51ee3a0ad32e5c3` |
| `runtime-asset-manager-linux-tsan` | `b549a4088f0168b8065bdbce86db5b79958ff525345e9058d61f90e993742ed8` |

`gh run download 31882332020 --dir Validation/026/CI/downloaded` retrieved all
eight artifacts. Every downloaded normalized JSON report records `passed: true`;
the ignored download directory is intentionally not committed. The digests
above are the SHA-256 archive digests reported by the GitHub Artifacts API.

The two Windows annotations are an upstream Node 20 deprecation warning from
`ilammy/msvc-dev-cmd@v1`; project compilation used strict warnings and passed.
Run `31873192897` validated the initial closeout revision. Review then identified
terminal-publication, ready-cache handoff, and shared-dependency cancellation
races; revision `8427e13` fixed them and added controlled regression coverage,
and run `31882332020` is the authoritative closeout run.
