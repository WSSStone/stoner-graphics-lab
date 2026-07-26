# B02-S24 Platform Services Verification Evidence

- Fix commit: `1a3c4de2a8bd2e45c22777c778f087ed82192fa4`
- Hosted-build correction: `c2427ea166764cacb3b3da9d25bded25e02d1380`
- Verification host: macOS arm64
- Successful CI: `30195707555`
- Successful CR tools: `30195707558`
- Retained failed CI: `30195411349`

## Filesystem Short Read

```text
parent:
attempt=0 returned=1 size=268435456 final_file_size=1
exit=3

current:
attempt=0..11 returned=0 size=0 final_file_size=1
exit=0
```

## Dynamic Module Contract

```text
parent:
copyable=1
search_load=1
symbol=42
owner_after_free=0 alias_after_free=1
stale_symbol=0 alias_after_second_free=0
exit=3

current:
copyable=0
search_rejected=1
ownership_transferred=1
symbol=1
release_idempotent=1
exit=0
```

## Platform Time

```text
steady=1 exact_conversions=1 threads_monotonic=1
exit=0
```

## Local Maintained Gates

```text
strict-debug:   pass
strict-release: pass
fallback-strict: pass
CR tool tests:  20 passed
git diff --check: pass
```

The fix packet had already passed the strict ASan/UBSan build, full maintained
suite, and 41/41 Core platform assertions. The successful hosted Linux
sanitizer job repeated that coverage at the final verification head.

## Structured Remote Evidence

- `remote-ci-b02-platform-services-first-failure.json`
- `remote-ci-b02-platform-services.json`
- `remote-tools-b02-platform-services.json`
- `remote-pr-checks-b02-platform-services.json`

These files preserve run/job conclusions and links without embedding full
hosted logs in Git.
