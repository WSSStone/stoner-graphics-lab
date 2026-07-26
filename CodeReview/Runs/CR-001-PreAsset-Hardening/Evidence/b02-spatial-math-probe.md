# B02-S10 Spatial Math Probe Evidence

- Probe source: `/tmp/cr001_spatial_math_probe.cpp`
- Host: macOS arm64
- Debug-like compile/run: pass; reproduced all findings
- Optimized Release compile/run: pass; reproduced identical results
- Repository changes from probe: none
- GitHub Actions used: none

```text
quat_huge=0,0 quat_equivalent=0 quat_nonfinite_finite=0
matrix_zero_negative_tol=1 matrix_nan=1 inverse00=nan
transform_composition_matches=0 transform_inverse_succeeded=1 transform_inverse_roundtrip=0 zero_scale_negative_tol=1
plane_scaled_distance=-1 box_center_finite=0 sphere_inf_valid=1 sphere_far_inside=1 plane_negative_tol=1
```

`matrix_zero_negative_tol=1` and `matrix_nan=1` mean `TryInverse` returned
success. `transform_inverse_succeeded=1` together with
`transform_inverse_roundtrip=0` is the false-success condition that reaches
scene PreserveWorld reparenting.
