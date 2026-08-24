# Feature 028 CI Evidence

Final closeout records hosted regular, scheduled/manual medium, and required
Windows Vulkan plus macOS Vulkan/Metal run IDs, revision IDs, artifact names,
and SHA-256 values here. Downloaded artifacts remain in ignored `downloaded/`.

## Local Pre-Final Medium Evidence

The candidate scheduler worktree based on revision
`b84be1a6c0960da58e3c968f6be68c1f920094fe` passed the complete local M4 Pro
Metal medium profile with two disjoint package roots running concurrently under
one shared deadline. This is scale evidence for T097 and preparation for T113;
it is not yet final-revision CI evidence and does not complete T113.

| Field | Lantern | Sponza |
|---|---:|---:|
| Generation | `f60c3069294c588e146073d736dd8cd325760c8d1645ae6c412cf58c45a709d3` | `b39afe90356b699eed761aa548587883c9300d54568cb0eb94f279a09e84096b` |
| Reachable / reused | 37 / 37 | 189 / 189 |
| Lifecycle | 1,000 / warm-up 20 | 1,000 / warm-up 20 |
| Native seconds | 302.609 | 1,376.784 |
| Peak RSS bytes | 546,226,176 | 334,987,264 |
| RSS growth bytes | 0 | 14,188,544 |
| Terminal owners / stale | 0 / rejected | 0 / rejected |

Total profile time was 1,450.659 seconds against the 1,800-second budget. The
summary SHA-256 is
`322985859d519229a333626c25b523380cde9e2f714b8866973a3f270100006c` and the
4,289-entry artifact manifest SHA-256 is
`f84189a9ef28fb1f3f5ecfc92c57e4d2ad327d0cd103cd6b175ded86715f29e9`.
