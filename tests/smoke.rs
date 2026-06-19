//! Minimal smoke tests for the bmesh-sys FFI.

use bmesh_sys::owned::BMeshOwned;
use bmesh_sys::*;
use std::ffi::CString;

/// A fresh mesh is empty and frees cleanly on drop.
#[test]
fn empty_mesh_has_no_elements() {
    let bm = BMeshOwned::new();
    assert_eq!(bm.totvert(), 0);
    assert_eq!(bm.totedge(), 0);
    assert_eq!(bm.totface(), 0);
}

/// Build a unit quad and verify element counts and the snapshot round-trip.
#[test]
fn build_quad() {
    let bm = BMeshOwned::new();
    let coords = [
        [0.0, 0.0, 0.0],
        [1.0, 0.0, 0.0],
        [1.0, 1.0, 0.0],
        [0.0, 1.0, 0.0],
    ];
    let verts: Vec<_> = coords.iter().map(|&c| bm.add_vert(c)).collect();
    assert!(verts.iter().all(|v| !v.is_null()));

    // Safety: every pointer in `verts` came from this same mesh's add_vert.
    let face = unsafe { bm.add_face(&verts) };
    assert!(!face.is_null());

    // A quad: 4 verts, 4 edges (auto-created by add_face), 1 face.
    assert_eq!(bm.totvert(), 4);
    assert_eq!(bm.totedge(), 4);
    assert_eq!(bm.totface(), 1);

    let snap = bm.snapshot();
    assert_eq!(snap.face_lens, vec![4]);
    assert_eq!(snap.face_offsets, vec![0]);

    // The face references all four distinct vertices.
    let mut idx: Vec<i32> = snap.face_verts[0..4].to_vec();
    idx.sort_unstable();
    assert_eq!(idx, vec![0, 1, 2, 3]);

    // Every input position is present in the snapshot's vertex buffer.
    for &c in &coords {
        let found = snap
            .verts
            .chunks_exact(3)
            .any(|p| p[0] == c[0] && p[1] == c[1] && p[2] == c[2]);
        assert!(found, "vertex {c:?} missing from snapshot");
    }
}

/// `bms_edge_split` (kernel op) inserts a vertex at the requested parameter.
#[test]
fn edge_split_inserts_midpoint() {
    let bm = BMeshOwned::new();
    let v0 = bm.add_vert([0.0, 0.0, 0.0]);
    let v1 = bm.add_vert([2.0, 0.0, 0.0]);
    // Safety: v0 and v1 came from this mesh's add_vert just above.
    let e = unsafe { bm.add_edge(v0, v1) };
    assert!(!e.is_null());
    assert_eq!(bm.totvert(), 2);
    assert_eq!(bm.totedge(), 1);

    // Split halfway from v0 toward v1 -> new vert at (1, 0, 0).
    let v_new = unsafe { bms_edge_split(bm.raw(), e, v0, 0.5, std::ptr::null_mut()) };
    assert!(!v_new.is_null());
    assert_eq!(bm.totvert(), 3);
    assert_eq!(bm.totedge(), 2);

    let snap = bm.snapshot();
    let has_midpoint = snap
        .verts
        .chunks_exact(3)
        .any(|p| (p[0] - 1.0).abs() < 1e-6 && p[1].abs() < 1e-6 && p[2].abs() < 1e-6);
    assert!(
        has_midpoint,
        "expected a vertex at the edge midpoint (1,0,0)"
    );
}

/// A custom float layer added on the vertex domain round-trips set -> get,
/// including for vertices created before the layer existed.
#[test]
fn customdata_float_layer_roundtrips() {
    let bm = BMeshOwned::new();
    let v = bm.add_vert([0.0, 0.0, 0.0]);

    let name = CString::new("weight").unwrap();
    let offset = unsafe {
        bms_layer_add_named(
            bm.raw(),
            BmsDomain::Vert as i32,
            CdType::Float as i32,
            name.as_ptr(),
        )
    };
    assert!(offset >= 0, "layer add failed (offset {offset})");

    // Default value for a freshly added layer is zero.
    let mut got = f32::NAN;
    unsafe { bms_elem_get_float(v as *mut std::ffi::c_void, offset, &mut got) };
    assert_eq!(got, 0.0);

    unsafe { bms_elem_set_float(v as *mut std::ffi::c_void, offset, 1.5) };
    let mut got = f32::NAN;
    unsafe { bms_elem_get_float(v as *mut std::ffi::c_void, offset, &mut got) };
    assert_eq!(got, 1.5);
}

/// `bms_vert_normals_read` copies per-vertex normals in vertex-table order
/// after a vertex-normal update, and reports the true vertex count so callers
/// can detect a too-small buffer.
#[test]
fn vert_normals_read_after_update() {
    let bm = BMeshOwned::new();
    // A quad in the XY plane wound CCW: every vertex normal should be +Z.
    let coords = [
        [0.0, 0.0, 0.0],
        [1.0, 0.0, 0.0],
        [1.0, 1.0, 0.0],
        [0.0, 1.0, 0.0],
    ];
    let verts: Vec<_> = coords.iter().map(|&c| bm.add_vert(c)).collect();
    // Safety: every pointer in `verts` came from this same mesh's add_vert.
    let face = unsafe { bm.add_face(&verts) };
    assert!(!face.is_null());

    unsafe { bms_mesh_vert_normals_update(bm.raw()) };

    let totvert = bm.totvert() as i32;
    assert_eq!(totvert, 4);

    let mut normals = vec![f32::NAN; (totvert * 3) as usize];
    let written = unsafe { bms_vert_normals_read(bm.raw(), normals.as_mut_ptr(), normals.len() as i32) };
    assert_eq!(written, totvert, "should report and write every vertex normal");

    for n in normals.chunks_exact(3) {
        assert!(n[0].abs() < 1e-6 && n[1].abs() < 1e-6, "expected an axis-aligned +Z normal, got {n:?}");
        assert!((n[2] - 1.0).abs() < 1e-6, "expected +Z normal, got {n:?}");
    }

    // A buffer that is too small writes nothing but still reports the true count.
    let mut sentinel = [42.0f32; 3];
    let count = unsafe { bms_vert_normals_read(bm.raw(), sentinel.as_mut_ptr(), sentinel.len() as i32) };
    assert_eq!(count, totvert, "true vertex count is reported even when truncated");
    assert_eq!(sentinel, [42.0, 42.0, 42.0], "nothing written when capacity is insufficient");
}

/// Port of Blender's `BMeshCoreTest.BMVertCreate` C++ gtest
/// (`source/blender/bmesh/tests/bmesh_core_test.cc`) onto the `bms_*` FFI.
///
/// Pins vertex-creation semantics: coordinate copy, zeroed normal, clean
/// header flags, and create-with-example copying customdata but *not* the
/// select flag. We use a named float layer + offset where the C++ test uses
/// the unnamed `BM_data_layer_add` / `BM_elem_float_data_*` by-type helpers;
/// the behaviour under test (customdata copy-from-example) is identical.
#[test]
fn bmesh_core_vert_create() {
    let bm = BMeshOwned::new();
    assert_eq!(bm.totvert(), 0);

    // Custom float layer on the vertex domain, so we can see it copied.
    let name = CString::new("test_float").unwrap();
    let offset = unsafe {
        bms_layer_add_named(
            bm.raw(),
            BmsDomain::Vert as i32,
            CdType::Float as i32,
            name.as_ptr(),
        )
    };
    assert!(offset >= 0);

    // bv1: created with an explicit coordinate, no example.
    let co1 = [1.0f32, 2.0, 0.0];
    let bv1 = bm.add_vert(co1);
    assert!(!bv1.is_null());
    let mut co = [0.0f32; 3];
    unsafe { bms_vert_co(bv1, co.as_mut_ptr()) };
    assert_eq!(co, [1.0, 2.0, 0.0]);
    let mut no = [9.0f32; 3];
    unsafe { bms_vert_no(bv1, no.as_mut_ptr()) };
    assert_eq!(no, [0.0, 0.0, 0.0], "new vertex normal should be zero");
    let elem1 = bv1 as *const std::ffi::c_void;
    assert_eq!(unsafe { bms_elem_htype(elem1) }, BM_VERT);
    assert_eq!(unsafe { bms_elem_hflag(elem1) }, 0);
    assert_eq!(unsafe { bms_elem_api_flag(elem1) }, 0);

    // bv2: created with no coordinate -> zeroed position.
    let bv2 = bm.add_vert([0.0, 0.0, 0.0]);
    assert!(!bv2.is_null());
    let mut co = [9.0f32; 3];
    unsafe { bms_vert_co(bv2, co.as_mut_ptr()) };
    assert_eq!(co, [0.0, 0.0, 0.0]);

    // Select bv2 and give it a customdata value, then create bv3 from it.
    unsafe { bms_vert_select_set(bm.raw(), bv2, true) };
    unsafe { bms_elem_set_float(bv2 as *mut std::ffi::c_void, offset, 1.5) };

    let bv3 = unsafe { bms_vert_create_example(bm.raw(), co1.as_ptr(), bv2) };
    assert!(!bv3.is_null());

    // Create-with-example copies customdata but NOT the select flag.
    let sel = unsafe { bms_elem_hflag(bv3 as *const std::ffi::c_void) } & BM_ELEM_SELECT;
    assert_eq!(sel, 0, "select flag must not be copied from the example");
    let mut got = f32::NAN;
    unsafe { bms_elem_get_float(bv3 as *mut std::ffi::c_void, offset, &mut got) };
    assert_eq!(got, 1.5, "customdata should be copied from the example");

    assert_eq!(bm.totvert(), 3);
}
