# Feature 026 Normalized Reports

Store deterministic text or canonical JSON summaries here. Stable sections may
contain Asset identities, request/result categories, generations, counts,
limits, and content digests. They must omit absolute paths, timestamps,
process/thread IDs, addresses, native diagnostics, and worker completion order.

Host, compiler, duration, and peak RSS are allowed only in a separately marked
telemetry section excluded from normalized artifact digests.
