//! RAII wrapper that owns a `BMesh` and frees it on drop.
//!
//! This is **not** a safe abstraction. Most methods take or return raw
//! `*mut BM*` pointers

use std::ptr;

use crate::{
    bms_edge_create, bms_face_create_verts, bms_mesh_create, bms_mesh_free, bms_snapshot,
    bms_totedge, bms_totface, bms_totvert, bms_vert_create, BMEdge, BMFace, BMVert, BMesh,
};

pub struct BMeshOwned {
    inner: *mut BMesh,
}

impl Default for BMeshOwned {
    fn default() -> Self {
        Self::new()
    }
}

impl BMeshOwned {
    pub fn new() -> Self {
        let inner = unsafe { bms_mesh_create() };
        assert!(!inner.is_null(), "bms_mesh_create returned null");
        Self { inner }
    }

    /// Raw pointer to the underlying mesh. Returning the pointer is safe;
    /// any use of it is unsafe.
    pub fn raw(&self) -> *mut BMesh {
        self.inner
    }

    pub fn add_vert(&self, co: [f32; 3]) -> *mut BMVert {
        unsafe { bms_vert_create(self.inner, co.as_ptr()) }
    }

    /// # Safety
    /// `v1` and `v2` must reference live verts of *this* mesh.
    pub unsafe fn add_edge(&self, v1: *mut BMVert, v2: *mut BMVert) -> *mut BMEdge {
        unsafe { bms_edge_create(self.inner, v1, v2, false) }
    }

    /// Create a face from N vertex pointers. The shim looks up or allocates
    /// the necessary edges (matching `BM_face_create_verts`).
    ///
    /// # Safety
    /// Every pointer in `verts` must reference a live vert of *this* mesh.
    pub unsafe fn add_face(&self, verts: &[*mut BMVert]) -> *mut BMFace {
        unsafe { bms_face_create_verts(self.inner, verts.as_ptr(), verts.len() as i32, false) }
    }

    pub fn totvert(&self) -> usize {
        unsafe { bms_totvert(self.inner) as usize }
    }
    pub fn totedge(&self) -> usize {
        unsafe { bms_totedge(self.inner) as usize }
    }
    pub fn totface(&self) -> usize {
        unsafe { bms_totface(self.inner) as usize }
    }

    /// Returns a flat-buffer snapshot of the current mesh state (no normals).
    pub fn snapshot(&self) -> RawSnapshot {
        self.snapshot_inner(false)
    }

    /// Snapshot that also includes face normals (one [f32;3] per face, read
    /// from bmesh's `f->no` field — caller must have called
    /// `bms_mesh_normals_update` beforehand to make them current).
    pub fn snapshot_with_normals(&self) -> RawSnapshot {
        self.snapshot_inner(true)
    }

    fn snapshot_inner(&self, with_normals: bool) -> RawSnapshot {
        let nv = self.totvert();
        let ne = self.totedge();
        let nf = self.totface();

        let mut verts: Vec<f32> = vec![0.0; nv * 3];
        let mut edges: Vec<i32> = vec![0; ne * 2];
        let mut face_offsets: Vec<i32> = vec![0; nf];
        let mut face_lens: Vec<i32> = vec![0; nf];
        let face_verts_cap = nf.max(1) * 32;
        let mut face_verts: Vec<i32> = vec![0; face_verts_cap];

        let mut face_normals_buf: Vec<f32> = if with_normals {
            vec![0.0; nf * 3]
        } else {
            Vec::new()
        };
        let (fn_ptr, fn_cap) = if with_normals {
            (face_normals_buf.as_mut_ptr(), (nf * 3) as i32)
        } else {
            (std::ptr::null_mut(), 0)
        };

        let ok = unsafe {
            bms_snapshot(
                self.inner,
                verts.as_mut_ptr(),
                (nv * 3) as i32,
                edges.as_mut_ptr(),
                (ne * 2) as i32,
                face_offsets.as_mut_ptr(),
                nf as i32,
                face_verts.as_mut_ptr(),
                face_verts_cap as i32,
                face_lens.as_mut_ptr(),
                nf as i32,
                fn_ptr,
                fn_cap,
            )
        };
        assert!(ok, "bms_snapshot returned false — buffer too small");

        RawSnapshot {
            verts,
            edges,
            face_offsets,
            face_lens,
            face_verts,
            face_normals: if with_normals {
                Some(face_normals_buf)
            } else {
                None
            },
        }
    }
}

impl Drop for BMeshOwned {
    fn drop(&mut self) {
        if !self.inner.is_null() {
            unsafe { bms_mesh_free(self.inner) };
            self.inner = ptr::null_mut();
        }
    }
}

/// Flat-buffer view of a mesh, suitable for canonicalization
pub struct RawSnapshot {
    pub verts: Vec<f32>,                // 3 floats per vert
    pub edges: Vec<i32>,                // 2 vert-indices per edge
    pub face_offsets: Vec<i32>,         // offset of each face into face_verts
    pub face_lens: Vec<i32>,            // vert count of each face
    pub face_verts: Vec<i32>,           // concatenated face vertex indices
    pub face_normals: Option<Vec<f32>>, // 3 floats per face when populated
}
