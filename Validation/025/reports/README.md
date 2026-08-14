# Normalized Reports

Store compact deterministic text or canonical JSON summaries here. A report
records stable subjects, counts, result categories, profile/generation/key
digests, and the exact verification command. It must omit absolute host paths,
timestamps, process/thread IDs, completion order, and native diagnostic text.

Timing, peak RSS, compiler, and host labels are allowed only in an explicitly
non-deterministic telemetry section excluded from artifact comparison digests.
