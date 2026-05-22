# bmesh-sys

FFI bindings to Blender's [BMesh](https://docs.blender.org/api/current/bmesh.html)
API, compiled from a vendored subset of Blender source via the `cc` crate.

The vendored code currently comes from Git commit
`ec6e62d40fa9e9d1bea33ad5d00148c99a4f0832` of Blender's `blender-v5.1-release`
branch (tag `v5.1.2`), plus Eigen at commit
`8a1083e9bf41b91fdea6546681f806154efdc25a`.

## Vendoring

[`vendor.sh`](vendor.sh) is the single source of truth for which upstream files
are vendored:

```
./vendor.sh [path/to/blender] [path/to/eigen]   # defaults: ../blender ../eigen
```

See [VENDOR.md](VENDOR.md) for the full breakdown of included / excluded
operators, stubs, and overlays.

## Layout

```
vendor/            - pristine upstream
shim/              - C-callable surface (bms_* functions) over the C++ bmesh API,
                     plus stubs for unvendored / unreached subsystems
src/               - raw FFI declarations (lib.rs) and thin RAII wrapper (owned.rs)
```

# Includes

This vendors almost all of BMesh's operators, but excludes the following categories:
- `Mesh` conversion operators (e.g. `bmo_mesh_to_bmesh.cc`) that pull in the non-BMesh Mesh data subsystem.
- `bmesh_intersect_edges.cc` which needs BKE_bvhutils.hh (indirect Mesh dep).

# Limited FFI Bindings

Currently the public FFI bindings are very limited, just enough to support some
minimal testing.

# Versioning

The crate is simply tagged with the Blender version it vendors (e.g. `5.1.0`),
and there is no semver guarantee between versions, since Blender doesn't
guarantee internal BMesh API stability between versions.

## License

Blender's BMesh source, and the Rust FFI bindings in this crate, are licensed
**GPL-2.0-or-later**.

See the [LICENSE-GPL-2.0.txt](LICENSE-GPL-2.0.txt) file for details.

Eigen (`vendor/eigen/`) is licensed **MPL-2.0**.

See `vendor/eigen/COPYING.MPL-2.0` for details.
