# Feature 028 CI Evidence

Final closeout records hosted regular, scheduled/manual medium, and required
Windows Vulkan plus macOS Vulkan/Metal run IDs, revision IDs, artifact names,
and SHA-256 values here. Downloaded artifacts remain in ignored `downloaded/`.

## Final-Revision Local Medium Evidence

Revision `0a5ad11f8d511a9b54da33da086b15cf530ca68a` passed the complete local M4 Pro
Metal medium profile with two disjoint package roots running concurrently under
one shared deadline. This is final-revision local scale and lifecycle evidence;
the separate manual Linux medium CI run remains required for T113 closeout.

| Field | Lantern | Sponza |
|---|---:|---:|
| Generation | `f60c3069294c588e146073d736dd8cd325760c8d1645ae6c412cf58c45a709d3` | `b39afe90356b699eed761aa548587883c9300d54568cb0eb94f279a09e84096b` |
| Reachable / reused | 37 / 37 | 189 / 189 |
| Lifecycle | 1,000 / warm-up 20 | 1,000 / warm-up 20 |
| Native seconds | 282.281 | 1,416.776 |
| Peak RSS bytes | 541,032,448 | 337,231,872 |
| RSS growth bytes | 0 | 10,633,216 |
| Terminal owners / stale | 0 / rejected | 0 / rejected |

Total profile time was 1,483.692 seconds against the 1,800-second budget. The
summary SHA-256 is
`d4e5d69bbd7d4dbce21651304140b1e79edd6acce30bcbecce2f96b078163e23` and the
4,289-entry artifact manifest SHA-256 is
`a1388edf26f758ab1bdc6908c63a7355bf3ff8ce106c5cd278da46320dc80ce9`.
Consumer verification returned `Passed` against target-profile digest
`82681655db59befc20366978759ae42ace9ed805e2d4e0adc7de7980854f4c8f`.

## Final-Revision Hosted Runs

Hosted run `32711618360` passed revision
`0a5ad11f8d511a9b54da33da086b15cf530ca68a`. All four Windows/Linux/macOS
regular producers, all four independent Linux artifact consumers, and both
sanitizer jobs completed successfully. Manual medium run `32716106974` targets
the same revision and remains in progress. Hardware run `32711618382` also
targets the same revision and remains queued for the required self-hosted
runners.

Hosted run `32711618360` uploaded these immutable artifacts:

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `production-regular-windows-vulkan-1` | `1e7da2410a9d4438eafb1a7890fab00dae6774ee677404a296602a715a68758f` | 1,069,695,002 |
| `production-regular-linux-vulkan-1` | `1d5fc9bfcad9786847feb70267becbecb5c7ef6afec63834e1b868f93de6fafa` | 1,069,692,856 |
| `production-regular-macos-metal-1` | `d41b8d62e6e82c88d280ff956b72102526703daddf8268757eab1894ffa327d9` | 1,070,862,987 |
| `production-regular-macos-intel-metal-1` | `87f63f0eb411020115cbc3abdb6848c7947ee998ca78031cca8184550902775e` | 1,070,867,453 |
| `production-consumer-windows-vulkan-1` | `ba72ee2f430dfdc0e998660fe6a83f859254cee9e468470baa8fc1d7a06abf50` | 106,665 |
| `production-consumer-linux-vulkan-1` | `97913bff316872bffc45a17259d7e6dab850c96178af363c649f9a735159c0a5` | 105,491 |
| `production-consumer-macos-metal-1` | `94f35e5a1422095559d41b63b804d3e5989c4dc65bcdb34ee059601848dde5bb` | 105,219 |
| `production-consumer-macos-intel-metal-1` | `f93206f65c0492efcda9c5281d6e5690706f8fb5b3433b6ab0ff9865585d4ca7` | 106,150 |
| `production-sanitizer-asan-ubsan-1` | `dece1f0bb157f053e3db8521cfbeceebfb85be17ffe706b74a1aea3013d92554` | 1,909 |
| `production-sanitizer-tsan-1` | `efedc7b050e99740cbeb60d8146f881c3b6071686146011fe8ed5490339ff80f` | 1,476 |
