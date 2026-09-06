# Windows agent prompt — Feature 029 final-SHA SDR evidence

> **Cancelled / historical draft (2026-09-06): do not execute.** The maintainer
> explicitly waived this rerun in [closeout.md](closeout.md). The original
> instructions below are retained only as historical context; they are not an
> outstanding task or permission to collect new evidence.

请在物理 Windows 机器上完成 Feature 029 的新版本 T102 正式 SDR 采集和适用的
Windows 本地回归。统一受测软件 SHA 固定为：

```text
2ee7116ffb382c021ed575aff223c7b760a2ce7d
```

远端分支为 `origin/029-hdr-output-transform`。该 SHA 的 hosted CI
[34002580090](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34002580090)
已全部通过（14/14），包括 Windows/Linux/macOS strict Debug/Release 和 Linux
sanitizers。macOS 主 agent 正在这个 SHA 上重新采集；两端证据必须使用同一个 SHA。

先阅读 `AGENTS.md`、`specs/029-hdr-output-transform/windows-handoff.md`、
`quickstart.md`、`contracts/validation-evidence.md` 和 `tasks.md`。严格执行 handoff
的命令与证据链。可复用上一轮的操作脚本，但必须更新运行目录，并重新执行构建、cook、
校准和原生采集；旧 `1f46352` 记录不得改名或改 SHA 冒充新证据。

1. 检查工作区并保留用户改动，fetch 后安全 checkout 上述完整 SHA。需要隔离时创建
   worktree。记录每次正式命令前后的软件 SHA/clean-input guard。不得 reset/clean
   或覆盖已有证据。构建、TEMP、DDC、cooked publication 和原始证据位于本机 NTFS。
2. 用固定 SCons 4.10.1 完成 strict Debug 和 Release；执行 handoff/quickstart 中
   的输出契约、Python、production-content、strict-runtime、image-acceptance 和
   适用 Vulkan/Deferred 回归。记录精确 argv、结果和二进制 SHA-256。
3. 对 Lantern/Sponza 的已暂存源重新校验 corpus 固定摘要，然后 fresh cook Windows
   Vulkan closure，并验证 warm reuse、publication/generation 和 strict loading。
   不使用 macOS cooked 数据。已有 source 可以通过摘要验证后复用。
4. 使用物理 discrete Vulkan GPU，在 active Console 或 RDP session 中运行实际
   应用窗口。记录采集前后 session、adapter、capability 和 policy-diff 身份。
   RDP 不要求断开，但只能声明该 session 下的应用 GPU 输出和窗口 presentation。
5. 分别生成 Lantern/Sponza v3 校准和 Candidate：512×512、sampleCount=1、
   `Sdr.sRGB.v1`、`Sdr.KhronosPbrNeutral.v1`、exposure=0、无 insertion。
   校准至少 3 个独立进程、每进程 20 次，并验证原有八种 mutation。Demo 正式采集
   使用 `native-capture`，原生 probe、report、Candidate PNG/JSON、calibration 和
   `sdr-report` 必须完整链接同一 SHA、generation、settings 和同帧 readback/present。
6. 新 bundle 放入
   `Validation/029/SDR/Windows-Vulkan/{Lantern,Sponza}/2ee7116-20260906-01/`，
   本地回归摘要放入 `Validation/029/CI/windows-vulkan-2ee7116-20260906-01/`。
   若目录已存在则选新后缀，并在交接说明中报告，不覆盖。所有 report 内 artifact 路径
   使用仓库相对路径，并在转移前验证仍可解析。
7. 完成 evidence、shader/vector、architecture、roadmap 编号/依赖/锚点/任务引用/
   旧 phase 引用扫描和 `git diff --check`。输出 bounded JSON/PNG、命令摘要、二进制/
   generation/输入摘要、校验清单和剩余 findings。保留 Candidate `acceptance=null`。

Windows 只验证 SDR，不运行 HDR 验证。禁止自动对齐、裁剪、缩放、平移或重采样；
禁止借用 Feature 028 v2/carry-forward，禁止修改 Accepted registry、人工 attestation
或宣称 029 已完成。遇到需要代码修改的失败，保留第一个失败并回报主 agent，停止正式采集；
代码变化需要再次统一软件 SHA。

本机验证通过后，把有界交接包和 SHA-256 校验文件复制到云盘
`Y:\stoner\Validation\029\windows-vulkan-2ee7116-20260906-01\`（若实际映射不同，
报告实际路径）。ZIP 内保留仓库相对路径，附 manifest 和 handoff 说明；JSON ≤1 MiB、
artifact ≤64 个、单个 ≤64 MiB、总计 ≤256 MiB。EXE/PDB、大日志、DDC、cooked payload
和源文件留在本机 NTFS，不放进交接包。无需为这次任务重新下载已通过摘要检查的源素材。

完成后回报精确 SHA、全部门槛结果、ZIP 路径/大小/SHA-256、两个 Candidate 的路径，
以及任何失败或未完成项。此次不要求提交、推送或删除旧数据。
