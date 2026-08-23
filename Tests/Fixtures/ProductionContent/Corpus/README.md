# Production Corpus Negative Fixtures

The Python corpus-verifier tests materialize bounded package trees in temporary
directories from `../Failures/corpus-cases.json`. No fixture path is evaluated
against the repository filesystem, and no case performs network access.
