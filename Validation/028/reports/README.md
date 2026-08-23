# Feature 028 Reports

Checked-in reports are bounded summaries tied to an exact revision. Canonical
JSON is limited to 1 MiB, 64 artifacts, 64 MiB per artifact, and 256 MiB in
aggregate. Absolute paths, credentials, environment secrets, process IDs,
native pointers, and unrelated desktop content are forbidden.
