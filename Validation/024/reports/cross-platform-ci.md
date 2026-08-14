# Feature 024 Cross-Platform CI

Status: **pass**

## Run Identity

- Commit: `945076d5074c1256bec6ac6c841fc19449fc5e85`
- Pull request: [#9](https://github.com/WSSStone/stoner-graphics-lab/pull/9)
- Required CI: [31766671726](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/31766671726)
- Native probe: [31766671729](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/31766671729)
- SCons: 4.10.1; C++20 strict warnings

## Job Matrix

| Job | Toolchain / profile | Result |
|---|---|---|
| Linux headless | GCC 13.3.0, Debug, Lavapipe | Pass |
| macOS headless | Apple Clang 21.0.0, Debug | Pass |
| Windows headless | MSVC 19.51, Debug | Pass |
| Linux Release | GCC 13.3.0, strict Release | Pass |
| macOS Release | Apple Clang 21.0.0, strict Release | Pass |
| Windows Release | MSVC 19.51, strict Release | Pass |
| Linux ASan + UBSan | GCC Debug, address/undefined | Pass |
| Linux ThreadSanitizer Asset | GCC Debug, thread | Pass |
| Linux Feature 024 native | Lavapipe Vulkan | Pass |

The native workflow ran twice because both push and pull-request triggers
matched the same commit. Both runs passed; the PR-triggered run is the retained
closeout identity.

## Feature 024 Artifacts

Hashes below are GitHub's SHA-256 digests of the uploaded zip artifacts.

| Artifact | ID | SHA-256 |
|---|---:|---|
| `static-model-debug-Linux` | 9206868330 | `51141e3a1439a299c76eb5a01a0bd2e3278ff4ebb9bcb60d657c3834b1cd0595` |
| `static-model-debug-macOS` | 9206715668 | `189fdf277e9c1dc29958b3ca3b6a2a284519ffbd376c74d8f5a671b9ac3dcc2b` |
| `static-model-debug-Windows` | 9206851730 | `e1a1eac56ff093ca2c1e9d2712e9fe0408eff4b49e737618562b15d25b86342b` |
| `static-model-release-Linux` | 9206673293 | `4a87e94be637c8f3f0a2321fdb6e49c878bb5aa25a0c92ca4feabbaff66434da` |
| `static-model-release-macOS` | 9206680865 | `26593f52b0a98b88b436d32f19c461856413f981788c7f408d18b373335f1a8b` |
| `static-model-release-Windows` | 9206758169 | `ac4b657533260c0cefe029a9dcedfedcd33a34d817d0f26a6703d637341e8b57` |
| `feature-024-native-probe-linux` | 9206664204 | `1ede643e9690b93c760f41c21d53e4b201ff127546ce76ebe394734a1f8d9917` |

Unpacked report evidence:

- Linux/macOS deterministic report:
  `d6075ff78ab291b3c554f8d373ae56b1201f50cfe24cec9badc245881b6aa9c2`
- Windows deterministic report (CRLF serialization):
  `22578e98bfedf3354abf0c7fc83435828d69a32280212433b66c3c64e73164f9`
- Native aggregate report:
  `c4294d03cbfb567d6ac368b2712e554bbfa1e98399997052003a67c86df2961e`
- Native indexed static-mesh readback:
  `9aaa5f37c9bd1109479f4e2bb4b17fd15f93a4e3c0a33bef1e61e2017f2a92d4`

## Performance And Numeric Tolerances

The 100,000-vertex, 300,000-index, 16-primitive fixture hash was identical on
all platforms: `559828d17e85f5b22df4bd8a8e3eb8255b0aefc0fd11fbb027fe0b72a86a4b57`.
CI's non-reference one-run samples passed their 60-second watchdog: Linux
0.0302 s, macOS 0.0322 s, and Windows 0.0394 s. The pass/fail 5-second limit
remains exclusive to the local Apple M4 Pro five-run reference gate.

Native deferred evidence used the existing format-aware tolerances: RGBA8 up
to `2/255` (`0.00784314`), half-float material channels `0.001`, AO `0.002`,
depth `0.0001`, and decoded world-normal dot product at least `0.999`. Static
mesh indexed attachment readback is a semantic pass/fail gate and does not use
a broader numeric tolerance.

## Refreshed Feature 018/019 Evidence

- `triangle-demo-Linux` artifact 9206867148, zip SHA-256
  `4a7d3e9389bfab6f22a35fb3356ceaa97e39639bdb5e6f194e0817e635bc2288`;
  unpacked native report SHA-256
  `63824a4d16cad1e404b67179588a7d92ad949c63cac14b7b9b23435dff8d8f12`.
- `deferred-renderer-Linux` artifact 9206867448, zip SHA-256
  `a6270c2e2d7c180b563c2d4b8a5d91d3c98479cb77cb01052ed0ff5c2e96e594`;
  unpacked readback SHA-256
  `caeb8bb5b544ce0ab756d2639a981272b24cd45331c3b5587da7cf8d14f87124`.

Both refreshed reports are retained under `Validation/018/Linux/` and
`Validation/019/Linux/`.
