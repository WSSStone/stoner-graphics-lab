# B02-S07 Scalar, Vector, And Color Probe

## Environment

- Host: macOS arm64
- Language mode: C++20
- Reviewed head: `c669f678348205dab531e67e2e7e22fffa8ccd17`
- Builds: `-Wall -Wextra -Werror` and
  `-O2 -DNDEBUG -Wall -Wextra -Werror`

## Result

Both builds produced:

```text
lerp0=nan lerp1=-inf
finite_norms=0,0,0 lengths=0,0,0
infinite_norm=nan,0,0
negative_tolerance=0
invalid_color=255,255,0,255
```

Inputs:

- Lerp endpoints: `FLT_MAX`, `-FLT_MAX`; Alpha `0` and `1`.
- Finite vectors: one `FLT_MAX` axis component in each dimension.
- Invalid vector: positive infinity on X.
- Invalid tolerance: `-1` comparing `1` with itself.
- Invalid color: NaN, positive infinity, negative infinity, NaN.

The temporary source and executables remain under `/tmp`.
