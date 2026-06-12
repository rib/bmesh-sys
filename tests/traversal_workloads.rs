//! Sanity tests for the whole-mesh traversal micro-workloads.

use bmesh_sys::owned::BMeshOwned;
use bmesh_sys::*;

/// An empty mesh has nothing to walk.
#[test]
fn empty_mesh_walks_to_zero() {
    let bm = BMeshOwned::new();
    assert_eq!(unsafe { bms_bench_disk_walk_sum(bm.raw()) }, 0);
    assert_eq!(unsafe { bms_bench_radial_walk_sum(bm.raw()) }, 0);
    assert_eq!(unsafe { bms_bench_vert_position_sum(bm.raw()) }, 0.0);
}

/// A lone quad: each of the 4 verts has 2 incident edges (disk total 8),
/// each of the 4 edges has exactly 1 radial loop (radial total 4), and the
/// unit-square coordinates sum to 4.0.
#[test]
fn lone_quad_walk_counts() {
    let bm = BMeshOwned::new();
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
    assert_eq!(bm.totedge(), 4);

    assert_eq!(unsafe { bms_bench_disk_walk_sum(bm.raw()) }, 8);
    assert_eq!(unsafe { bms_bench_radial_walk_sum(bm.raw()) }, 4);
    assert_eq!(unsafe { bms_bench_vert_position_sum(bm.raw()) }, 4.0);
}

/// Two triangles sharing an edge: 4 verts, 5 edges, 6 loops. The disk
/// total is 2 * totedge = 10; the shared edge contributes 2 radial loops
/// and the 4 boundary edges 1 each, so the radial total is totloop = 6.
#[test]
fn shared_edge_walk_counts() {
    let bm = BMeshOwned::new();
    let v0 = bm.add_vert([0.0, 0.0, 0.0]);
    let v1 = bm.add_vert([1.0, 0.0, 0.0]);
    let v2 = bm.add_vert([1.0, 1.0, 0.0]);
    let v3 = bm.add_vert([0.0, 1.0, 0.0]);
    // Safety: all pointers came from this same mesh's add_vert.
    let f0 = unsafe { bm.add_face(&[v0, v1, v2]) };
    let f1 = unsafe { bm.add_face(&[v0, v2, v3]) };
    assert!(!f0.is_null() && !f1.is_null());
    assert_eq!(bm.totedge(), 5);

    assert_eq!(unsafe { bms_bench_disk_walk_sum(bm.raw()) }, 10);
    assert_eq!(unsafe { bms_bench_radial_walk_sum(bm.raw()) }, 6);
    assert_eq!(unsafe { bms_bench_vert_position_sum(bm.raw()) }, 4.0);
}
