//! Sanity tests for the guarded-allocator bookkeeping pass-throughs.
//!
//! The counters are process-global, so this file holds a single test and no
//! others — each integration-test file runs as its own process, keeping the
//! readings free of concurrent allocator activity from unrelated tests.

use bmesh_sys::owned::BMeshOwned;
use bmesh_sys::*;

/// Mesh creation routes through the guarded allocator: both counters rise
/// while the mesh is alive and the block count falls back once it is freed.
#[test]
fn mesh_allocations_are_visible_in_counters() {
    let blocks_before = unsafe { bms_mem_blocks_in_use() };
    let bytes_before = unsafe { bms_mem_in_use() };

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

    let blocks_live = unsafe { bms_mem_blocks_in_use() };
    let bytes_live = unsafe { bms_mem_in_use() };
    assert!(
        blocks_live > blocks_before,
        "block count did not rise: {blocks_before} -> {blocks_live}"
    );
    assert!(
        bytes_live > bytes_before,
        "byte count did not rise: {bytes_before} -> {bytes_live}"
    );

    drop(bm);
    let blocks_after = unsafe { bms_mem_blocks_in_use() };
    assert!(
        blocks_after < blocks_live,
        "block count did not fall after free: {blocks_live} -> {blocks_after}"
    );
}
