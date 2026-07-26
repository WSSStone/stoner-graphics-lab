# B02-S05 Local Verification

- Fix commit: `60689e1`
- Host: macOS arm64
- Strict Debug fallback build: pass
- Full deterministic tests: pass
- Core foundation tests: `62/0`
- Strict Release build: pass
- ASan/UBSan build and full tests: pass
- Focused pre-fix probe result: `non-null`, exit `1`
- Focused post-fix probe result: `null`, exit `0`

The temporary probe source and executable remain outside the repository under
`/tmp`.
