# Feature 023 Pre-Migration Baseline

- Baseline commit: `6696e127bf0b418095fe2e79bae40c88016e3bc9`
- Platform: macOS arm64
- Build: strict Debug, SCons 4.10.1, Apple Clang
- Captured before Feature 023 production-code changes.

## Normalized Regression Outcomes

| Suite | Passed | Failed |
|---|---:|---:|
| renderer-material | 30 | 0 |
| renderer-forward | 31 | 0 |
| deferred-renderer | 34 | 0 |
| triangle-demo | 28 | 0 |
| deferred-native | 2 | 0 |
| vulkan-native | 2 | 0 |

The local native runtime reports `VK_ERROR_INCOMPATIBLE_DRIVER` because the
available MoltenVK runtime cannot access Metal in this execution environment.
The runtime-independent failure lifecycle, zero-live-resource, optional-native,
and explicit-unavailable contracts pass. CI and supported Windows/macOS hosts
remain the native-success authority.

## Repository Shader Digests

| Repository path | SHA-256 |
|---|---|
| `Demo/StonerDemo/Shaders/Triangle.vert` | `18659d56aadbd6c69eec5a11b32ff0de7741384de284eea3e627fe83f23a094c` |
| `Demo/StonerDemo/Shaders/Triangle.vert.spv` | `1f26aeab6dfbb2414f62c6be313fd9b6d37b7e6b142ef02fd863a3b3e901cda1` |
| `Demo/StonerDemo/Shaders/Triangle.frag` | `127dff83a3f59330f8e15438ffdff64196f896705311b8991d0966f896422f84` |
| `Demo/StonerDemo/Shaders/Triangle.frag.spv` | `68b4535277d4f77dfccd32de133084b73be7e41c5b554ed19633b41a6a60c85c` |
| `Source/Renderer/Shaders/Deferred/Surface.vert` | `53e5b663d5736cb47ec1cf7b9be34e726d2862c862663434dfbd6ae7d8b786d5` |
| `Source/Renderer/Shaders/Deferred/Surface.vert.spv` | `b7f5fe075983bdab775c6b65ed5657e5eb97338d17020ef8c71b1562adae0daa` |
| `Source/Renderer/Shaders/Deferred/Surface.frag` | `ab1cac6c9efdaec99dc96b1f9d78520e80807f5aa83d981cfa2ba8db04695c95` |
| `Source/Renderer/Shaders/Deferred/Surface.frag.spv` | `c842af14abf8b71fd652505d6e7ef29c560dfbf7fdc76ab1d5f4a2dcc0c0ce25` |
| `Source/Renderer/Shaders/Deferred/Fullscreen.vert` | `dbc18587f05c7e04946f302019d40fc5e625ba9474f4a5dd295169aa5f0ac21a` |
| `Source/Renderer/Shaders/Deferred/Fullscreen.vert.spv` | `443542d79eaac5c1466cd7feb339696ee70e77e467c8adb9897d9d7d3d1ebe6d` |
| `Source/Renderer/Shaders/Deferred/Composition.frag` | `317fce7c7155fc59ea3823e3b32a5ccc7b36fd083e48cdf286a6e8ba6e8b3910` |
| `Source/Renderer/Shaders/Deferred/Composition.frag.spv` | `2483304cd3160a94f4bb1dd55ecbf33359646c8913edcdcfac902d7d33d45a96` |
| `Source/Renderer/Shaders/Deferred/DirectionalLight.frag` | `92b46466464f0d0d36e97b27c67f69ab8bf961faebfe81b20d5f357a508b16c9` |
| `Source/Renderer/Shaders/Deferred/DirectionalLight.frag.spv` | `d9e41d68001282655a8d1c595d40d1b819e2bba2ec619689346e35d4cd3574ec` |
| `Source/Renderer/Shaders/Deferred/PointLight.vert` | `09d49a826367a74a4f7ddb6a3fd91f991872760bb0da00ec7fef10996887489b` |
| `Source/Renderer/Shaders/Deferred/PointLight.vert.spv` | `4bd91aa8469b7c02f2185e792a91e231cb970dbff3ce17adf95f81b202ee24ef` |
| `Source/Renderer/Shaders/Deferred/PointLight.frag` | `e32879aa413a9eac7abafb684046c584a1c26d1704c983f48be27b0a8df8a2be` |
| `Source/Renderer/Shaders/Deferred/PointLight.frag.spv` | `8a1b677c1b8b6c069f779391e4abc6369af4e402f72aa585073d03436362c0cb` |
| `Source/Renderer/Shaders/Deferred/SpotLight.vert` | `09d49a826367a74a4f7ddb6a3fd91f991872760bb0da00ec7fef10996887489b` |
| `Source/Renderer/Shaders/Deferred/SpotLight.vert.spv` | `4bd91aa8469b7c02f2185e792a91e231cb970dbff3ce17adf95f81b202ee24ef` |
| `Source/Renderer/Shaders/Deferred/SpotLight.frag` | `3cbd61b343fba5b65338f570e24fe0cad4cae256a54d072c60ff6243715dfbc6` |
| `Source/Renderer/Shaders/Deferred/SpotLight.frag.spv` | `3d8d2d5462e004579dbf9d2a9f20a12176a0af8e6738642d60bb3773d98011f1` |
