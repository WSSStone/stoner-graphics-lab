# Execution

Define dependency-ordered batches and bounded steps. Each step must state its
inspection or fix budget, expected evidence, required gates, and next command.
Implementation batches must use one `Inspect -> Fix -> Verify` triplet per
responsibility domain so that no inspection packet exceeds eight production
files or 1,500 production lines.
