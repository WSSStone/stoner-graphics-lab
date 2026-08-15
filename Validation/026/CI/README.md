# Feature 026 CI Evidence

Each Windows, macOS, and Linux Debug/strict Release job, plus Linux ASan/UBSan
and TSan, uploads exactly one uniquely named normalized artifact even on
failure. This directory records workflow run IDs, job conclusions, artifact
names, downloaded SHA-256 digests, and the final revision. Raw Actions logs and
downloaded artifact payloads are not committed.
