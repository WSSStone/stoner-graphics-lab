# Mutation Fixture Contract

Mutation probes alter authoritative source evidence after import and before
publication. A request must fail once with `SourceChanged`, publish no partial
payload, and perform no implicit retry.

