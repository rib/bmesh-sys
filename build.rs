// build.rs for bmesh-sys
//
// This crate vendors a subset of Blender's C++ source (under `vendor/`) and
// compiles it into a static library that the Rust FFI in `src/lib.rs` links
// against.

use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=shim/shim.h");
    println!("cargo:rerun-if-changed=shim/shim.cc");
    println!("cargo:rerun-if-changed=shim/misc_stubs.cc");
    println!("cargo:rerun-if-changed=shim/clog_stubs.h");

    let vendor = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("vendor");
    let bmesh_core = vendor.join("bmesh/intern/bmesh_core.cc");

    if !bmesh_core.exists() {
        panic!(
            "bmesh-sys: vendor/bmesh not populated; skipping native compile. \
             Run `vendor.sh` and rebuild."
        );
    }

    let mut b = cc::Build::new();
    // Resolve shim path to an absolute path so `-include` works regardless
    // of cc-rs's working directory.
    let manifest = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let clog_stubs = manifest.join("shim/clog_stubs.h");

    b.cpp(true)
        .std("c++20")
        .warnings(false)
        .flag_if_supported("-funsigned-char")
        // Don't define WITH_GMP / WITH_BULLET — Blender's source uses
        // `#ifdef WITH_FOO` (presence check), so even WITH_FOO=0 enables
        // the gated code. We leave them undefined so the bullet-hull and
        // GMP-boolean paths stay disabled.
        .define("WITH_TBB", Some("0"))
        .define("BMESH_EMBEDDED_BUILD", Some("1"))
        // Force-include the stubs header into every TU so symbol stubs
        // (e.g. BM_verts_calc_rotate_beauty) are visible everywhere.
        .flag("-include")
        .flag(clog_stubs.to_str().expect("clog_stubs path not utf8"))
        .include(vendor.join("bmesh"))
        .include(vendor.join("bmesh/intern"))
        .include(vendor.join("bmesh/tools"))
        .include(vendor.join("blenlib"))
        .include(vendor.join("blenkernel"))
        .include(vendor.join("blenloader"))
        .include(vendor.join("guardedalloc"))
        .include(vendor.join("dna"))
        .include(vendor.join("atomic"))
        // Eigen (MPL-2.0) — header-only, lets us link math_matrix.cc /
        // math_solvers.cc and the bevel / smooth_laplacian / circularize
        // operators that depend on Eigen-backed linear algebra.
        .include(vendor.join("eigen"))
        // Blender's thin C wrapper around Eigen (linear_solver, svd, etc.).
        .include(vendor.join("eigen_capi"))
        .include("shim");

    // guardedalloc's mallocn_intern.hh picks a malloc_usable_size strategy by
    // platform. Its first branch keys off HAVE_MALLOC_STATS_H, which Blender's
    // CMake sets via `check_symbol_exists(malloc_stats "malloc.h" ...)` — a
    // GLIBC-only symbol. Without it we fall through to the `#else`, which emits
    // `#pragma message "We don't know how to use malloc_usable_size ..."`.
    // Mirror Blender: define it on glibc Linux (target_env == "gnu"). On musl,
    // macOS, Windows and the BSDs the header's own branches apply, so we leave
    // it undefined there (matching upstream, which also wouldn't define it).
    let target_os = std::env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    let target_env = std::env::var("CARGO_CFG_TARGET_ENV").unwrap_or_default();
    if target_os == "linux" && target_env == "gnu" {
        b.define("HAVE_MALLOC_STATS_H", None);
    }

    // Explicit compile list. Many BLI headers `#include "intern/foo_inline.cc"`
    // so vendor/blenlib/intern/ holds .cc files we do NOT want compiled as
    // their own translation units. Keep the list curated.
    let bmesh_files: &[&str] = &[
        "bmesh/intern/bmesh_core.cc",
        "bmesh/intern/bmesh_construct.cc",
        "bmesh/intern/bmesh_delete.cc",
        "bmesh/intern/bmesh_structure.cc",
        "bmesh/intern/bmesh_iterators.cc",
        "bmesh/intern/bmesh_query.cc",
        "bmesh/intern/bmesh_marking.cc",
        "bmesh/intern/bmesh_polygon.cc",
        "bmesh/intern/bmesh_mesh.cc",
        "bmesh/intern/bmesh_interp.cc",
        "bmesh/intern/bmesh_mods.cc",
        // Operators framework + opdefines table. The full set of operator
        // implementations follows in `bmesh_operator_files` below.
        "bmesh/intern/bmesh_operators.cc",
        "bmesh/intern/bmesh_opdefines.cc",
        // Algorithmic intern files pulled in by the operator + tools sets:
        //   - bmesh_walkers + impl: BMW_* (used by dissolve, removedoubles)
        //   - bmesh_edgeloop: BMEdgeLoopStore (used by subdivide_edgering)
        //   - bmesh_callback_generic: generic callback machinery
        //   - bmesh_polygon_edgenet: polygon edgenet construction
        "bmesh/intern/bmesh_walkers.cc",
        "bmesh/intern/bmesh_walkers_impl.cc",
        "bmesh/intern/bmesh_edgeloop.cc",
        "bmesh/intern/bmesh_callback_generic.cc",
        "bmesh/intern/bmesh_polygon_edgenet.cc",
        "bmesh/intern/bmesh_mesh_tessellate.cc",
    ];
    // bmesh/operators/ + bmesh/tools/ — compiled wholesale. vendor.sh already
    // curates which files land in these dirs (it copies every bmo_*.cc /
    // tools/bmesh_*.cc except a documented few with hard external deps), so we
    // just compile everything present. Globbing keeps these lists in sync
    // across Blender versions automatically: a version bump that adds, drops,
    // or renames an operator/tool needs no edits here. (Operators whose exec
    // symbols are referenced by the pristine opdefines table but whose .cc is
    // not vendored — currently only bmo_mesh_convert.cc — are stubbed in
    // shim/misc_stubs.cc.)
    let bmesh_operator_files = scan_cc(&vendor, "bmesh/operators");
    let bmesh_tools_files = scan_cc(&vendor, "bmesh/tools");
    let blenlib_files: &[&str] = &[
        "blenlib/intern/listbase.cc",
        "blenlib/intern/BLI_mempool.cc",
        "blenlib/intern/BLI_memarena.cc",
        "blenlib/intern/BLI_ghash.cc",
        "blenlib/intern/BLI_ghash_utils.cc",
        "blenlib/intern/BLI_assert.cc",
        "blenlib/intern/math_base.cc",
        "blenlib/intern/math_vector.cc",
        "blenlib/intern/math_geom.cc",
        "blenlib/intern/math_matrix.cc",
        "blenlib/intern/math_rotation.cc",
        "blenlib/intern/math_rotation_c.cc",
        "blenlib/intern/math_solvers.cc",
        "blenlib/intern/array_utils.cc",
        "blenlib/intern/array_utils_c.cc",
        "blenlib/intern/bit_span.cc",
        "blenlib/intern/bit_ref.cc",
        // String helpers pulled in by customdata.cc's name-uniquing path.
        "blenlib/intern/string.cc",
        "blenlib/intern/string_ref.cc",
        "blenlib/intern/string_utf8.cc",
        "blenlib/intern/string_utils.cc",
        // Self-contained geometric / data-structure helpers pulled in by
        // the broader bmesh operators (heap, polygon triangulation, BVH,
        // scanfill, etc.). All purely algorithmic — no Blender system deps.
        "blenlib/intern/BLI_heap.cc",
        "blenlib/intern/BLI_heap_simple.cc",
        "blenlib/intern/polyfill_2d.cc",
        "blenlib/intern/polyfill_2d_beautify.cc",
        "blenlib/intern/convexhull_2d.cc",
        "blenlib/intern/BLI_kdopbvh.cc",
        "blenlib/intern/scanfill.cc",
        "blenlib/intern/stack.cc",
        // Additional helpers pulled in by the operators/tools tree:
        //   - BLI_linklist:    LinkNode (used by dissolve, removedoubles, ...)
        //   - noise_c:         turbulence-noise helpers (mirror, distort, ...)
        //   - sort_utils:      qsort comparators
        //   - math_matrix_c:   non-Eigen 3x3/4x4 inversion / transform helpers
        //     (math_matrix.cc itself + math_solvers.cc both require Eigen and
        //     stay excluded — the `_c` variant has fallback paths)
        "blenlib/intern/BLI_linklist.cc",
        "blenlib/intern/noise_c.cc",
        "blenlib/intern/sort_utils.cc",
        "blenlib/intern/math_matrix_c.cc",
        "blenlib/intern/rand.cc",
    ];
    let guardedalloc_files: &[&str] = &[
        "guardedalloc/intern/mallocn.cc",
        "guardedalloc/intern/mallocn_lockfree_impl.cc",
        "guardedalloc/intern/memory_usage.cc",
    ];
    // blenkernel: vendoring customdata.cc directly so A/B tests can exercise
    // BMesh's real customdata interp. Heavy stubbing of pulled-in BKE / BLO
    // headers lives under shim/blenkernel_stubs.cc and vendor/blenkernel
    // stub headers. See PR-Y2 in /home/rib/.claude/plans for the strategy.
    let blenkernel_files: &[&str] = &["blenkernel/intern/customdata.cc"];

    // Blender's thin C wrapper around Eigen — required by bevel
    // (linear_solver for the offset-adjust LS problem), smooth_laplacian
    // (linear_solver for the Laplacian system), and math_solvers.cc.
    let eigen_capi_files: &[&str] = &[
        "eigen_capi/intern/linear_solver.cc",
        "eigen_capi/intern/matrix.cc",
        "eigen_capi/intern/svd.cc",
        "eigen_capi/intern/eigenvalues.cc",
    ];

    let mut all_files: Vec<String> = Vec::new();
    for f in bmesh_files
        .iter()
        .chain(blenlib_files)
        .chain(guardedalloc_files)
        .chain(blenkernel_files)
        .chain(eigen_capi_files)
    {
        all_files.push((*f).to_string());
    }
    all_files.extend(bmesh_operator_files);
    all_files.extend(bmesh_tools_files);

    for f in &all_files {
        let p = vendor.join(f);
        if !p.exists() {
            println!(
                "cargo:warning=bmesh-sys: missing vendored file {}",
                p.display()
            );
            continue;
        }
        b.file(&p);
        println!("cargo:rerun-if-changed={}", p.display());
    }
    b.file("shim/shim.cc");
    b.file("shim/misc_stubs.cc");

    b.compile("bmesh_static");
}

/// Return the `.cc` files directly under `vendor/<rel>/`, as vendor-relative
/// paths (e.g. `"bmesh/operators/bmo_bevel.cc"`), sorted for deterministic
/// build order. Used for the operators/ and tools/ trees, which are compiled
/// wholesale (vendor.sh decides which files are present). Returns empty if the
/// directory is absent.
fn scan_cc(vendor: &std::path::Path, rel: &str) -> Vec<String> {
    let dir = vendor.join(rel);
    let mut out: Vec<String> = match std::fs::read_dir(&dir) {
        Ok(entries) => entries
            .filter_map(|e| e.ok())
            .filter_map(|e| e.file_name().into_string().ok())
            .filter(|name| name.ends_with(".cc"))
            .map(|name| format!("{rel}/{name}"))
            .collect(),
        Err(_) => Vec::new(),
    };
    out.sort();
    out
}
