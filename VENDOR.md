# Vendored BMesh operators

This document tracks which BMesh operator implementations from
`blender/source/blender/bmesh/operators/` and `bmesh/tools/` are vendored
into `bmesh-sys/vendor/bmesh/`, which are excluded, and what
stubbing was required to make them compile + link.

The vendor set is **maximally inclusive**: every operator whose dependency
chain stays inside `bmesh + blenlib + BKE_customdata + Eigen` is built.
The excluded set is small and well-defined — operators that pull in the
`Mesh` data structure or `BKE_bvhutils`.

The full list of files copied is defined by [`vendor.sh`](vendor.sh); that
script is the single source of truth for *which* upstream files are vendored
and is also the audit tool — see [Re-vendoring & audit](#re-vendoring--audit).

## Build mechanics

The operator framework is wired up as follows:

- `bmesh/intern/bmesh_operators.cc` — BMO core (op_init, op_exec, slot machinery).
- `bmesh/intern/bmesh_opdefines.cc` — operator definition table, kept **byte-for-byte pristine**. It still references the exec functions of the excluded mesh-convert operators; rather than editing the table, those exec symbols are defined as no-ops in `shim/misc_stubs.cc` (see [Stubs](#stubs-in-shimmisc_stubscc)). Keeping the table pristine means a Blender pin bump is a clean re-run of `vendor.sh`.
- `bmesh/operators/bmo_*.cc` — per-operator exec functions (40 included, 1 file excluded: `bmo_mesh_convert.cc`).
- `bmesh/tools/bmesh_*.cc` — deeper algorithmic backends (18 included, 1 excluded).
- `bmesh/intern/bmesh_{walkers,walkers_impl,edgeloop,callback_generic,polygon_edgenet,mesh_tessellate}.cc` — supporting infrastructure needed by the above.

Mesh creation in [`shim/shim.cc`](shim/shim.cc) (`bms_mesh_create`) sets
`BMeshCreateParams.use_toolflags = true`, which is required by every
`BMO_op_*` call (operator-flag storage is allocated lazily).

## Vendored operators (40)

| Operator | File | Notes |
|---|---|---|
| beautify | bmo_beautify.cc | Wraps tools/bmesh_beautify.cc (vendored). |
| bevel | bmo_bevel.cc | Wraps tools/bmesh_bevel.cc (vendored). Uses Eigen via the C-API wrapper for `adjust_the_cycle_or_chain`'s least-squares offset solver. `harden_normals` and `custom_profile` paths are reachable only when the caller opts in — `BM_lnorspace_update` / `BKE_lnor_space_custom_normal_to_data` / `BKE_curveprofile_init` are stubbed in `shim/misc_stubs.cc` for the link, but no shim path triggers them. |
| bisect_plane | bmo_bisect_plane.cc | Wraps tools/bmesh_bisect_plane.cc (vendored). |
| bridge_loops | bmo_bridge.cc | |
| circularize | bmo_circularize.cc | Uses `blender::math::invert<float,3>` from `math_matrix.cc` (Eigen-backed). |
| connect_verts | bmo_connect.cc | |
| connect_verts_concave | bmo_connect_concave.cc | |
| connect_verts_nonplanar | bmo_connect_nonplanar.cc | |
| connect_vert_pair | bmo_connect_pair.cc | |
| create_vert / create_edge / contextual_create | bmo_create.cc | |
| create_grid / create_uvsphere / create_icosphere / create_cone / create_circle / create_cube / create_monkey | bmo_primitive.cc | Primitive generation. Fully self-contained: the `BM_mesh_calc_uvs_*` helpers and the Suzanne monkey mesh (`monkeyv`/`monkeyf`/`monkeyuvs`) are defined in-file, and it pulls in no deps beyond the already-vendored `bmesh` + `blenlib` math + `BKE_customdata` set. No shim wrapper surfaced yet — callable via the BMO API. |
| delete | bmo_dupe.cc | Same file as duplicate/split. |
| dissolve_verts / dissolve_edges / dissolve_faces / dissolve_limit / dissolve_degenerate | bmo_dissolve.cc | Wraps tools/bmesh_decimate_dissolve.cc (vendored). |
| duplicate | bmo_dupe.cc | |
| edgenet_fill / edgenet_prepare | bmo_edgenet.cc | Wraps tools/bmesh_edgenet.cc (vendored). |
| extrude_face_region / extrude_face_indiv / extrude_edge_only / extrude_vert_indiv / extrude_discrete_faces / solidify_face_region | bmo_extrude.cc | |
| face_attribute_fill | bmo_fill_attribute.cc | |
| edgeloop_fill | bmo_fill_edgeloop.cc | |
| grid_fill | bmo_fill_grid.cc | |
| holes_fill | bmo_fill_holes.cc | |
| flatten_faces | bmo_flatten.cc | |
| convex_hull | bmo_hull.cc | The bullet-physics path (`#ifdef WITH_BULLET`) stays disabled — `WITH_BULLET` is deliberately *not* defined in build.rs so the body compiles to empty. The operator definition is still in the table; calling it is a no-op. |
| inset_individual / inset_region | bmo_inset.cc | |
| join_triangles | bmo_join_triangles.cc | |
| mirror | bmo_mirror.cc | |
| recalc_face_normals / flip_quad_tessellation / reverse_faces / reverse_colors / reverse_uvs | bmo_normals.cc | |
| offset_edgeloops | bmo_offset_edgeloops.cc | Surfaced via the `bms_offset_edgeloops` shim (`edges`, `use_cap_endpoint`; `edges.out` not surfaced). |
| planar_faces | bmo_planar_faces.cc | |
| poke | bmo_poke.cc | A/B-testable against our `face_poke` once we surface a `bms_poke` shim. |
| collapse / collapse_uvs / find_doubles / remove_doubles / pointmerge / pointmerge_facedata / average_vert_facedata / weld_verts | bmo_removedoubles.cc | |
| rotate_edges / bisect_edges | bmo_rotate_edges.cc | |
| smooth_laplacian_vert | bmo_smooth_laplacian.cc | Uses Eigen via the C-API wrapper for the Laplacian system solve. |
| split_edges | bmo_split_edges.cc | Wraps tools/bmesh_edgesplit.cc (vendored). |
| split | bmo_dupe.cc | |
| subdivide_edges | bmo_subdivide.cc | |
| subdivide_edgering | bmo_subdivide_edgering.cc | Uses `BKE_curve_forward_diff_bezier` from BKE_curve.cc — **stubbed to no-op** in `shim/misc_stubs.cc`. The smooth-subdivide path falls back to linear placement, which is acceptable for our A/B parity tests on topology + customdata. |
| symmetrize | bmo_symmetrize.cc | |
| triangulate | bmo_triangulate.cc | Wraps tools/bmesh_triangulate.cc (vendored). |
| unsubdivide | bmo_unsubdivide.cc | Wraps tools/bmesh_decimate_unsubdivide.cc (vendored). |
| transform / scale / translate / rotate / rotate_uvs / rotate_colors / smooth_vert / region_extend / spin / object_load_bmesh (excluded) / mesh_to_bmesh (excluded) | bmo_utils.cc | This file contains many small utility operators; the file as a whole is vendored. |
| wireframe | bmo_wireframe.cc | Wraps tools/bmesh_wireframe.cc. The vertex-group BKE_deform path is only reached when `defgrp_index != -1`; the operator entry point passes `-1`, so the only BKE_deform call (`BKE_defvert_find_weight`) is **stubbed to return 1.0f** in `shim/misc_stubs.cc` (unreachable in practice). |

## Excluded operators (1)

| Operator | File | Reason |
|---|---|---|
| mesh_to_bmesh / bmesh_to_mesh / object_load_bmesh | bmo_mesh_convert.cc | Pulls in `BKE_global.hh` plus the entire `Mesh` (non-BMesh) data subsystem. Architectural exclude — bmesh-sys is purposefully `Mesh`-free. The `.cc` is not compiled; exec symbols no-op-stubbed in `shim/misc_stubs.cc`. |

## Excluded tools/ backends (1)

| Tool | File | Reason |
|---|---|---|
| bmesh_intersect_edges | bmesh_intersect_edges.cc | `BKE_bvhutils.hh` (mesh BVH utilities) → indirect Mesh dep. |

## Additional BLI helpers vendored

These were added to `vendor/blenlib/intern/` to satisfy operator dependencies. All purely algorithmic — no Blender system deps:

| File | LOC | Used by |
|---|---|---|
| BLI_heap.cc / BLI_heap_simple.cc | ~600 | beautify, polyfill, dissolve |
| polyfill_2d.cc / polyfill_2d_beautify.cc | ~1650 | fill_*, triangulate, scanfill |
| convexhull_2d.cc | 840 | hull |
| BLI_kdopbvh.cc | 2400 | bvh queries (used internally by various ops) |
| scanfill.cc | 1150 | fill_holes, edgenet |
| stack.cc | 260 | many |
| BLI_linklist.cc | ~300 | dissolve, removedoubles, others |
| noise_c.cc | ~600 | mirror, distort |
| sort_utils.cc | ~80 | qsort comparators |
| math_matrix_c.cc | ~2000 | Provides `invert_m3` / `invert_m4_m4` / `copy_m4_m4` / `transpose_m3` / etc. The C-flavour API has always been here; with Eigen now linked we keep it alongside math_matrix.cc rather than forcing the no-Eigen fallback paths. |
| math_matrix.cc | ~1500 | C++ matrix algebra (`blender::math::invert<T,N>` + decomposition family). Used by circularize. Requires Eigen. |
| math_solvers.cc | ~150 | BLI least-squares / eigenvalue / SVD helpers. Wraps the Eigen C-API. |
| rand.cc | ~500 | BLI_rng_* (random number generator) |

## Additional bmesh/intern files vendored

Beyond the original `bmesh_core/construct/delete/...` set, the operator framework pulls in:

| File | Why |
|---|---|
| bmesh_operators.cc | BMO core (op_init/exec/finish, slot machinery). |
| bmesh_opdefines.cc | Operator definition table (pristine; excluded ops' exec symbols stubbed in `shim/misc_stubs.cc`). |
| bmesh_walkers.cc + bmesh_walkers_impl.cc | `BMW_*` traversal API (used by dissolve, removedoubles). |
| bmesh_edgeloop.cc | `BMEdgeLoopStore` (used by subdivide_edgering, bridge_loops). |
| bmesh_callback_generic.cc | Generic-callback dispatch. |
| bmesh_polygon_edgenet.cc | Polygon edgenet construction. |
| bmesh_mesh_tessellate.cc | `BM_mesh_calc_tessellation` (face triangulation). |

## Eigen integration

Eigen (MPL-2.0) is vendored under [`vendor/eigen/Eigen/`](vendor/eigen/Eigen/) as a header-only tree, and Blender's thin C-API wrapper (`linear_solver`, `matrix`, `svd`, `eigenvalues`) lives at [`vendor/eigen_capi/`](vendor/eigen_capi/). The wrapper translates 6 `EIG_*` calls used by bevel + smooth_laplacian into Eigen's sparse-LU / least-squares machinery, and also backs the C++ math algebra in `math_matrix.cc` + `math_solvers.cc`.

## Build-flag conventions

Set in [`build.rs`](build.rs):

- **`WITH_TBB=0`** — disables Threading Building Blocks. We single-thread bmesh.
- **`WITH_BULLET` / `WITH_GMP` undefined** — Blender uses `#ifdef WITH_X` (presence check); defining `WITH_BULLET=0` would *enable* the gated code. Both stay undefined.
- **`-include shim/clog_stubs.h`** — force-includes the CLG_LOG macro stubs into every TU.

## Stubs in `shim/misc_stubs.cc`

These functions are referenced by vendored code but currently never called:

- `BLI_task_parallel_mempool` / `BLI_task_parallel_range` (we run single-threaded)
- `BM_mesh_normals_update` (we have `bms_mesh_normals_update` doing per-face)
- `index_mask::get_static_index_mask_for_min_size` (template instantiation reached via customdata layer registry)
- `multires_mdisp_corners` / `cdf_read_data` / `cdf_write_data` (multires + file-backed CD paths)
- `BKE_curve_forward_diff_bezier` (smooth-subdivide bezier; falls back to linear)
- `BKE_lnor_spacearr_free` / `BKE_mesh_mdisp_flip` / `old_mdisps_bilinear` (multires + sculpt)
- `BKE_defvert_find_weight` (vertex-group weight lookup; wireframe + bevel operators pass `defgrp_index = -1` which skips the branch — stub returns 1.0f, the "no weighting" default)
- `BKE_curveprofile_init` (bevel's custom-profile path; only reached when `bp->custom_profile != nullptr` — shim never supplies one)
- `BM_lnorspace_update` / `BKE_lnor_space_custom_normal_to_data` (bevel's `harden_normals` path; shim never sets harden_normals=true)
- `BLI_system_backtrace` (debug)
- **Excluded-operator exec functions** — `bmo_{mesh_to_bmesh,bmesh_to_mesh,object_load_bmesh}_exec` (from `bmo_mesh_convert.cc`). That `.cc` isn't compiled, but the **pristine** `bmesh_opdefines.cc` table still references its exec symbols, so they're defined as no-ops here to satisfy the link. The operators stay present-but-no-op. If a Blender pin bump adds / renames / drops a mesh-convert operator, the link breaks with an undefined- or duplicate-symbol error pointing at this list — update it accordingly.

## Vendored headers replaced by stubs

A few headers under `vendor/` are **not** copies of upstream — they are stubs
that fully replace the upstream header. Keeping them as separate, reviewable
files (rather than hand-editing the vendored copy) means `vendor.sh` regenerates
them deterministically and the vendored tree carries no edited-upstream content.
The stub *sources* live in [`shim/vendor_stubs/`](shim/vendor_stubs/); `vendor.sh`
overlays them after copying pristine upstream.

| Vendored path | Source | Why a stub |
|---|---|---|
| `vendor/blenlib/wcwidth.h` | `shim/vendor_stubs/wcwidth.h` | `extern/wcwidth` only *declares* `mk_wcwidth()`; its impl (`wcwidth.c`) isn't compiled. `BLI_string_utf8.cc` includes it but the symbol is never exercised on any A/B path. Stub provides an inline `mk_wcwidth` returning 1 (one column / codepoint). |
| `vendor/blenloader/BLO_read_write.hh` | `shim/vendor_stubs/BLO_read_write.hh` | `customdata.cc` references the `.blend` read/write API, but bmesh-sys never serialises a mesh; the real header drags in the whole loader. Stub provides no-op `BlendWriter` / `BlendDataReader` + free functions. |
| `vendor/blenlib/BLT_translation.hh` | generated inline by `vendor.sh` | blentranslation not vendored; the translation macros are stubbed in `shim/clog_stubs.h`, so this only needs to exist and be empty. |

## In-place patches to vendored upstream

There is one in-place rewrite of vendored upstream code, applied
deterministically by `vendor.sh`:

- **guardedalloc include flattening** — `MEM_guardedalloc.h` and the
  guardedalloc `.cc`/`.hh` files `#include "../../source/blender/blenlib/BLI_*.h"`,
  baking in the original tree layout. `vendor.sh` rewrites those to plain
  `"BLI_*.h"` (blenlib is already on the cc include path). This is a mechanical
  path-prefix rewrite, not a logic change, and is version-stable.

Everything else under `vendor/` is a pristine copy from upstream.

## Re-vendoring & audit

[`vendor.sh`](vendor.sh) is idempotent. To bump the version of Blender:

```bash
cd bmesh-sys
./vendor.sh [path/to/blender] [path/to/eigen]   # defaults: ../blender ../eigen
git diff --stat -- vendor/                      # review local deltas
cargo build -p bmesh-sys                        # native compile + archive
cargo test -p bmesh-sys                         # smoke test the FFI
```

Because the script copies pristine upstream over `vendor/` every run, a
non-empty `git diff` on `vendor/` is *only* ever:

If needed, the stub overlays in [`shim/vendor_stubs/`](shim/vendor_stubs/) and
the excluded-operator exec stubs in `shim/misc_stubs.cc` are the two places
where upstream-divergent code is kept *out* of `vendor/`.

## What's exposed via the FFI

The shim only surfaces a handful of operators today (`bms_extrude_face_region`).
Adding more involves: writing a `bms_FOO` wrapper in
[`shim/shim.cc`](shim/shim.cc) that sets BM_ELEM_TAG / fills slots and calls
`BMO_op_initf("FOO ...")`, declare in [`shim/shim.h`](shim/shim.h), add the
`extern "C"` declaration in [`src/lib.rs`](src/lib.rs).