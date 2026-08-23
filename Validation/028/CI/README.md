# Feature 028 CI Evidence

Final closeout records hosted regular, scheduled/manual medium, and required
Windows Vulkan plus macOS Vulkan/Metal run IDs, revision IDs, artifact names,
and SHA-256 values here. Downloaded artifacts remain in ignored `downloaded/`.

## Local Pre-Final Medium Evidence

The current uncommitted Feature 028 worktree based on inherited revision
`66a20cc42881d3747d836f9f45257c37f7f3e039` passed the complete local M4 Pro
Metal medium profile. This is scale evidence for T097 and preparation for T113;
it is not yet final-revision CI evidence and does not complete T113.

| Field | Lantern | Sponza |
|---|---:|---:|
| Generation | `f60c3069294c588e146073d736dd8cd325760c8d1645ae6c412cf58c45a709d3` | `b39afe90356b699eed761aa548587883c9300d54568cb0eb94f279a09e84096b` |
| Reachable / reused | 37 / 37 | 189 / 189 |
| Lifecycle | 1,000 / warm-up 20 | 1,000 / warm-up 20 |
| Native seconds | 260.393 | 1,362.027 |
| Peak RSS bytes | 542,261,248 | 332,562,432 |
| RSS growth bytes | 0 | 16,613,376 |
| Terminal owners / stale | 0 / rejected | 0 / rejected |

Total profile time was 1,724.162 seconds against the 1,800-second budget. The
summary SHA-256 is
`f0f65ca90c6527aefdfb1ab7ff1553eda57cebd324d62b00eb17e5021aacb0d6` and the
4,288-entry artifact manifest SHA-256 is
`0ff719304444d704418d0f177d089b3f46f6028c47a4c676bff85844f9752194`.
