//! Low-level FFI bindings to Blender's BMesh.
//!
//! All symbols here are `extern "C"` wrappers exported from `shim/shim.cc`.
//! The wrappers are deliberately prefixed `bms_*` (BMesh-Shim) so the FFI
//! surface is decoupled from Blender's own (C++ name-mangled) symbol names.

#![allow(non_camel_case_types, non_snake_case)]

use std::os::raw::{c_int, c_uint};

// ---- Opaque element types ----
//
// All struct fields live behind the FFI boundary; consumers only ever
// hold raw pointers.

#[repr(C)]
pub struct BMesh {
    _private: [u8; 0],
}
#[repr(C)]
pub struct BMVert {
    _private: [u8; 0],
}
#[repr(C)]
pub struct BMEdge {
    _private: [u8; 0],
}
#[repr(C)]
pub struct BMLoop {
    _private: [u8; 0],
}
#[repr(C)]
pub struct BMFace {
    _private: [u8; 0],
}
/// Common header shared by every BM element. Any element pointer can be
/// cast to `*mut BMHeader` for use in type-erased, mixed-element buffers.
#[repr(C)]
pub struct BMHeader {
    _private: [u8; 0],
}

// ---- Mesh lifecycle ----

unsafe extern "C" {
    pub fn bms_mesh_create() -> *mut BMesh;
    pub fn bms_mesh_free(bm: *mut BMesh);
}

// ---- Element creation ----

unsafe extern "C" {
    pub fn bms_vert_create(bm: *mut BMesh, co: *const f32) -> *mut BMVert;
    /// Like [`bms_vert_create`], but copies customdata (not flags) from `example`.
    pub fn bms_vert_create_example(
        bm: *mut BMesh,
        co: *const f32,
        example: *mut BMVert,
    ) -> *mut BMVert;
    pub fn bms_edge_create(
        bm: *mut BMesh,
        v1: *mut BMVert,
        v2: *mut BMVert,
        no_double: bool,
    ) -> *mut BMEdge;
    pub fn bms_face_create_verts(
        bm: *mut BMesh,
        verts: *const *mut BMVert,
        len: c_int,
        no_double: bool,
    ) -> *mut BMFace;
}

// ---- Element deletion ----

unsafe extern "C" {
    pub fn bms_vert_kill(bm: *mut BMesh, v: *mut BMVert);
    pub fn bms_edge_kill(bm: *mut BMesh, e: *mut BMEdge);
    pub fn bms_face_kill(bm: *mut BMesh, f: *mut BMFace);
}

// ---- Selection + read-only element field accessors ----

unsafe extern "C" {
    pub fn bms_vert_select_set(bm: *mut BMesh, v: *mut BMVert, select: bool);
    /// Copy the vertex coordinate into `out` (3 floats).
    pub fn bms_vert_co(v: *const BMVert, out: *mut f32);
    /// Copy the vertex normal into `out` (3 floats).
    pub fn bms_vert_no(v: *const BMVert, out: *mut f32);
    /// Read an edge's two endpoint verts into `out_v1` / `out_v2`. Both
    /// out-pointers must be non-null and writable; each receives the
    /// corresponding endpoint (`e->v1`, `e->v2`).
    pub fn bms_edge_verts(e: *mut BMEdge, out_v1: *mut *mut BMVert, out_v2: *mut *mut BMVert);
    /// `head.htype` — element geometric type (`BM_VERT=1`, `BM_EDGE=2`, …).
    pub fn bms_elem_htype(elem: *const std::ffi::c_void) -> c_int;
    /// `head.hflag` — public element flags (e.g. `BM_ELEM_SELECT = 1<<0`).
    pub fn bms_elem_hflag(elem: *const std::ffi::c_void) -> c_int;
    /// `head.api_flag` — transient operator-scratch flags.
    pub fn bms_elem_api_flag(elem: *const std::ffi::c_void) -> c_int;

    /// OR `hflag_bit` into `head.hflag` on any BM element.
    pub fn bms_elem_set_hflag(elem: *mut std::ffi::c_void, hflag_bit: c_int);
    /// AND-out `hflag_bit` from `head.hflag` on any BM element.
    pub fn bms_elem_clear_hflag(elem: *mut std::ffi::c_void, hflag_bit: c_int);
    /// XOR `hflag_bit` into `head.hflag` on any BM element.
    pub fn bms_elem_toggle_hflag(elem: *mut std::ffi::c_void, hflag_bit: c_int);
}

/// `head.htype` value for a vertex (mirrors BMesh's `BM_VERT`).
pub const BM_VERT: c_int = 1;
/// `head.hflag` select bit (mirrors BMesh's `BM_ELEM_SELECT`).
pub const BM_ELEM_SELECT: c_int = 1 << 0;

// ---- Element hflag bits ----
//
// These mirror BMesh's `BMHeader.hflag` bits. Combine with `|`. Note the
// `HIDE` name matches Blender's `BM_ELEM_HIDDEN` bit (the name `HIDE` is
// the conventional shorthand and what the FFI exposes).

/// `BM_ELEM_SELECT` — element-is-selected bit.
pub const BMS_ELEM_HFLAG_SELECT: c_int = 1 << 0;
/// `BM_ELEM_HIDDEN` — element-is-hidden bit.
pub const BMS_ELEM_HFLAG_HIDE: c_int = 1 << 1;
/// `BM_ELEM_SEAM` — edge-is-a-UV-seam bit (also reused as
/// `BM_ELEM_SELECT_UV_EDGE` for loops).
pub const BMS_ELEM_HFLAG_SEAM: c_int = 1 << 2;
/// `BM_ELEM_SMOOTH` — face/edge smooth-shaded bit (cleared = sharp).
pub const BMS_ELEM_HFLAG_SMOOTH: c_int = 1 << 3;
/// `BM_ELEM_TAG` — general-purpose scratch bit (assume dirty; clear before use).
pub const BMS_ELEM_HFLAG_TAG: c_int = 1 << 4;
/// `BM_ELEM_SELECT_UV` — loop-vertex UV selection / face selection bit.
pub const BMS_ELEM_HFLAG_SELECT_UV: c_int = 1 << 5;
/// `BM_ELEM_TAG_ALT` — secondary scratch bit (assume dirty; clear before use).
pub const BMS_ELEM_HFLAG_TAG_ALT: c_int = 1 << 6;
/// `BM_ELEM_INTERNAL_TAG` — reserved for low-level internal tagging
/// (callers should leave cleared).
pub const BMS_ELEM_HFLAG_INTERNAL_TAG: c_int = 1 << 7;

// ---- Splice / merge ----

unsafe extern "C" {
    pub fn bms_vert_splice(bm: *mut BMesh, v_dst: *mut BMVert, v_src: *mut BMVert) -> bool;
    pub fn bms_edge_splice(bm: *mut BMesh, e_dst: *mut BMEdge, e_src: *mut BMEdge) -> bool;
}

// ---- Kernel Euler operators ----

unsafe extern "C" {
    /// SFME — Split Face, Make Edge.
    /// bmesh: `bmesh_kernel_split_face_make_edge`
    pub fn bms_sfme(
        bm: *mut BMesh,
        f: *mut BMFace,
        l_v1: *mut BMLoop,
        l_v2: *mut BMLoop,
    ) -> *mut BMFace;

    /// SEMV — Split Edge, Make Vert.
    /// bmesh: `bmesh_kernel_split_edge_make_vert`
    pub fn bms_semv(
        bm: *mut BMesh,
        tv: *mut BMVert,
        e: *mut BMEdge,
        r_e: *mut *mut BMEdge,
    ) -> *mut BMVert;

    /// JEKV — Join Edge, Kill Vert (collapse degree-2 vertex).
    /// bmesh: `bmesh_kernel_join_edge_kill_vert`
    pub fn bms_jekv(bm: *mut BMesh, e_kill: *mut BMEdge, v_kill: *mut BMVert) -> *mut BMEdge;

    /// JVKE — Join Vert, Kill Edge (general edge collapse).
    /// bmesh: `bmesh_kernel_join_vert_kill_edge`
    pub fn bms_jvke(bm: *mut BMesh, e_kill: *mut BMEdge, v_kill: *mut BMVert) -> *mut BMVert;

    /// JFKE — Join Face, Kill Edge.
    /// bmesh: `bmesh_kernel_join_face_kill_edge`
    pub fn bms_jfke(
        bm: *mut BMesh,
        f1: *mut BMFace,
        f2: *mut BMFace,
        e: *mut BMEdge,
    ) -> *mut BMFace;

    /// bmesh: `bmesh_kernel_loop_reverse`
    pub fn bms_loop_reverse(bm: *mut BMesh, f: *mut BMFace);

    /// bmesh: `bmesh_kernel_edge_separate`
    pub fn bms_edge_separate(bm: *mut BMesh, e: *mut BMEdge, l_sep: *mut BMLoop) -> *mut BMEdge;

    // Query helpers — for the A/B harness.

    /// bmesh: `BM_edge_exists`. Null if no edge connects the two verts.
    pub fn bms_edge_exists(v1: *mut BMVert, v2: *mut BMVert) -> *mut BMEdge;

    /// bmesh: `BM_face_vert_share_loop`. The loop in `f`'s cycle whose vertex is `v`.
    pub fn bms_face_vert_share_loop(f: *mut BMFace, v: *mut BMVert) -> *mut BMLoop;

    /// bmesh: `BM_face_edge_share_loop`. The loop in `f`'s cycle whose edge is `e`.
    pub fn bms_face_edge_share_loop(f: *mut BMFace, e: *mut BMEdge) -> *mut BMLoop;

    /// bmesh: `f->l_first`. One loop of the face's loop cycle — caller walks
    /// the cycle via `bms_loop_next`. Returns null if the face has no loops.
    pub fn bms_face_first_loop(f: *mut BMFace) -> *mut BMLoop;

    /// bmesh: `l->next`. The next loop around `l`'s face cycle.
    pub fn bms_loop_next(l: *mut BMLoop) -> *mut BMLoop;

    /// bmesh: `l->v`. Returns the loop's vertex.
    pub fn bms_loop_vert(l: *mut BMLoop) -> *mut BMVert;

    /// bmesh: `f->mat_nr`. Returns the face's material-slot index.
    pub fn bms_face_get_mat_nr(f: *const BMFace) -> std::os::raw::c_short;

    /// bmesh: `f->mat_nr = mat_nr`. Writes the face's material-slot index.
    pub fn bms_face_set_mat_nr(f: *mut BMFace, mat_nr: std::os::raw::c_short);

    // High-level wrappers.

    /// bmesh: `BM_edge_rotate` with `check_flag = 0`.
    pub fn bms_edge_rotate(bm: *mut BMesh, e: *mut BMEdge, ccw: bool) -> *mut BMEdge;

    /// bmesh: `BM_faces_join` with `do_del=true`. Joins N faces into one.
    pub fn bms_faces_join(bm: *mut BMesh, faces: *mut *mut BMFace, totface: c_int) -> *mut BMFace;

    /// bmesh: `BM_edge_collapse`. Collapses an edge by killing one of its
    /// vertices; the other endpoint absorbs all of v_kill's other edges.
    pub fn bms_edge_collapse(
        bm: *mut BMesh,
        e_kill: *mut BMEdge,
        v_kill: *mut BMVert,
    ) -> *mut BMVert;

    /// bmesh: `bmesh_kernel_vert_separate`. Returns the number of resulting
    /// vertices (input + newly-created fans). If `out_verts` non-null and
    /// `out_verts_cap >= return-value`, fills `out_verts` with the resulting
    /// vertex pointers (first = input `v`).
    pub fn bms_vert_separate(
        bm: *mut BMesh,
        v: *mut BMVert,
        out_verts: *mut *mut BMVert,
        out_verts_cap: c_int,
    ) -> c_int;

    /// bmesh: `bmesh_kernel_unglue_region_make_vert`. Peels the corner at
    /// `l_sep` away from its vertex's fan, creating a new vertex for it.
    pub fn bms_unglue_region_make_vert(bm: *mut BMesh, l_sep: *mut BMLoop) -> *mut BMVert;

    // ---- Phase M: subdivision basics ----

    /// bmesh: `BM_edge_split`. Parameterized split — new vertex at
    /// `lerp(v.co, other_endpoint.co, fac)`. If `r_e` is non-null, writes
    /// the new edge (the half adjacent to `v`) into it.
    pub fn bms_edge_split(
        bm: *mut BMesh,
        e: *mut BMEdge,
        v: *mut BMVert,
        fac: f32,
        r_e: *mut *mut BMEdge,
    ) -> *mut BMVert;

    /// bmesh: `BM_edge_split_n`. Uniformly subdivides into `numcuts + 1`
    /// segments; writes the `numcuts` new vertex pointers into `r_varr`.
    /// Returns the count actually written.
    pub fn bms_edge_split_n(
        bm: *mut BMesh,
        e: *mut BMEdge,
        numcuts: c_int,
        r_varr: *mut *mut BMVert,
    ) -> c_int;

    /// bmesh: `BM_face_split_n` with loop lookup by vertex. `cos` is a
    /// `n * 3` float buffer (positions for intermediate vertices). Returns
    /// the new face on success, null on failure.
    pub fn bms_face_split_n(
        bm: *mut BMesh,
        face: *mut BMFace,
        v1: *mut BMVert,
        v2: *mut BMVert,
        cos: *mut f32,
        n: c_int,
    ) -> *mut BMFace;

    /// Like `BM_face_poke` with offset=0: insert a centre vertex at the
    /// face's median position and fan-triangulate. Hand-composed in the
    /// shim because the bmesh tools/ tree isn't vendored. Equivalent to
    /// `bms_face_poke_mode(bm, face, 0)`. Returns the new centre vertex, or
    /// null if the face has fewer than 3 corners (or exceeds the internal
    /// ngon capacity).
    pub fn bms_face_poke(bm: *mut BMesh, face: *mut BMFace) -> *mut BMVert;

    /// Like [`bms_face_poke`], but selects the poke centre formula via
    /// `center_mode` (ordering matches the poke BMOP's `eCenterMode`):
    /// `0` = MEAN (uniform centroid), `1` = BOUNDS (axis-aligned bbox
    /// centre), `2` = MEAN_WEIGHTED (edge-length-weighted centroid, falling
    /// back to MEAN when the total weight is non-positive). Any other value
    /// behaves as MEAN. Only the centre position differs between modes; the
    /// fan-triangulation and customdata interpolation are identical. Returns
    /// the new centre vertex, or null if the face has fewer than 3 corners
    /// (or exceeds the internal ngon capacity).
    pub fn bms_face_poke_mode(
        bm: *mut BMesh,
        face: *mut BMFace,
        center_mode: c_int,
    ) -> *mut BMVert;

    /// Like [`bms_face_poke_mode`], but additionally lifts the new centre
    /// vertex along the source face's normal. `center_mode` selects the centre
    /// formula exactly as in [`bms_face_poke_mode`]. The lift applied to the
    /// centre vertex is `face_normal * offset * scale`, where `scale` is the
    /// arithmetic mean of the corner-to-centre distances when
    /// `use_relative_offset` is `true`, and `1.0` otherwise; it is applied
    /// after customdata interpolation, so interpolation sees the un-lifted
    /// position. Returns the new centre vertex, or null if the face has fewer
    /// than 3 corners (or exceeds the internal ngon capacity).
    pub fn bms_face_poke_offset(
        bm: *mut BMesh,
        face: *mut BMFace,
        center_mode: c_int,
        offset: f32,
        use_relative_offset: bool,
    ) -> *mut BMVert;

    /// bmesh: `BM_vert_dissolve`. Returns true on success, false if the
    /// vertex's topology is unsupported (and the mesh is left unchanged).
    pub fn bms_vert_dissolve(bm: *mut BMesh, v: *mut BMVert) -> bool;

    /// Marks each input face with `BM_ELEM_TAG`, then invokes BMesh's
    /// `extrude_face_region` operator.
    ///
    /// Note: Originals are replaced by the duplicates (aka `use_keep_orig=false`)
    ///
    /// `use_normal_flip` is forwarded to the operator; when `true` it reverses
    /// the winding of the side (wall) faces built between the original boundary
    /// and the lifted duplicate.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_extrude_face_region_ex(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        use_normal_flip: bool,
    ) -> bool;

    /// Convenience wrapper for [`bms_extrude_face_region_ex`] with
    /// `use_normal_flip = false`.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_extrude_face_region(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
    ) -> bool;

    /// Extrudes a region of faces while forwarding the operator's
    /// `edges_exclude` set.
    ///
    /// `faces` points to `faces_len` `*mut BMFace`; each is passed as the
    /// operator's `geom` input. `edges_exclude` points to `edges_exclude_len`
    /// `*mut BMEdge` inserted into the operator's `edges_exclude` mapping slot;
    /// it may be null when `edges_exclude_len == 0` to request no exclusions.
    /// `use_keep_orig` and `use_normal_flip` are forwarded to the operator.
    ///
    /// Unlike [`bms_extrude_face_region_ex`], the input faces are not killed
    /// after the op; deletion of selection-interior originals is left to the
    /// operator under `use_keep_orig = false`.
    ///
    /// Returns false if the operator rejected the input.
    ///
    /// # Safety
    ///
    /// `bm` must be a valid mesh. `faces` must be valid for `faces_len`
    /// elements and `edges_exclude` valid for `edges_exclude_len` elements
    /// (or null when the length is zero). All referenced elements must belong
    /// to `bm`.
    pub fn bms_extrude_face_region_exclude(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        edges_exclude: *mut *mut BMEdge,
        edges_exclude_len: c_int,
        use_keep_orig: bool,
        use_normal_flip: bool,
    ) -> bool;

    /// Extrudes over the operator's native mixed `geom` element buffer.
    ///
    /// `geom` is a type-erased element buffer (the `%eb` slot format): a
    /// pointer to `geom_len` [`BMHeader`] pointers that may freely mix
    /// vert / edge / face pointers in a single call (the header is the
    /// first field of every element, so each may be cast to `*mut BMHeader`).
    /// The operator routes each element kind on its own: faces drive a
    /// connected region extrude, edges build edge-only walls, and loose
    /// verts spawn a connecting wire edge to their lifted duplicate.
    ///
    /// `edges_exclude` points to `edges_exclude_len` `*mut BMEdge` inserted
    /// into the operator's `edges_exclude` mapping slot; it may be null when
    /// `edges_exclude_len == 0` to request no exclusions. `use_keep_orig`
    /// and `use_normal_flip` are forwarded verbatim.
    ///
    /// No input elements are killed after the op; deletion of
    /// selection-interior originals is left to BMesh, so loose verts and
    /// wire edges (which have no interior) are preserved.
    ///
    /// Returns false if the operator rejected the input.
    ///
    /// # Safety
    ///
    /// `bm` must be a valid mesh. `geom` must be valid for `geom_len`
    /// elements and `edges_exclude` valid for `edges_exclude_len` elements
    /// (or null when the length is zero). All referenced elements must
    /// belong to `bm`.
    pub fn bms_extrude_face_region_geom(
        bm: *mut BMesh,
        geom: *mut *mut BMHeader,
        geom_len: c_int,
        edges_exclude: *mut *mut BMEdge,
        edges_exclude_len: c_int,
        use_keep_orig: bool,
        use_normal_flip: bool,
    ) -> bool;

    /// Extrudes a region of faces while forwarding the operator's
    /// `use_normal_from_adjacent` slot.
    ///
    /// `faces` points to `faces_len` `*mut BMFace`; each is passed as the
    /// operator's `geom` input. `use_keep_orig`, `use_normal_flip`, and
    /// `use_normal_from_adjacent` are forwarded to the operator. When
    /// `use_normal_from_adjacent` is `true`, the side (wall) faces take their
    /// orientation from geometry adjacent to the extruded region rather than
    /// from the region's own averaged normal.
    ///
    /// Unlike [`bms_extrude_face_region_ex`], the input faces are not killed
    /// after the op; the surrounding geometry is left intact.
    ///
    /// Returns false if the operator rejected the input.
    ///
    /// # Safety
    ///
    /// `bm` must be a valid mesh. `faces` must be valid for `faces_len`
    /// elements and all referenced faces must belong to `bm`.
    pub fn bms_extrude_face_region_normal_from_adjacent(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        use_keep_orig: bool,
        use_normal_flip: bool,
        use_normal_from_adjacent: bool,
    ) -> bool;

    /// Extrude a region of faces, forwarding the operator's
    /// `use_dissolve_ortho_edges` slot. Refreshes face normals, then tags the
    /// full closure of each input face -- the face plus its edges and verts --
    /// with `BM_ELEM_TAG` and passes them as the operator's `geom` input. The
    /// region's edges must be in `geom` for the dissolve pass to run: the
    /// operator only treats a region edge as a dissolve candidate after it has
    /// deleted the original it lifted off, which it does only when the region's
    /// edges are present in `geom`. When `use_dissolve_ortho_edges` is true,
    /// side (wall) faces that end up lying in the plane of the extruded region
    /// are dissolved back into the surround, and the verts left as edge-pairs by
    /// those merges are collapsed. `use_keep_orig` and `use_normal_flip` are
    /// forwarded too; the dissolve pass only has effect when originals are
    /// deleted, i.e. `use_keep_orig == false`. No originals are killed by this
    /// wrapper itself.
    ///
    /// Returns false if the operator rejected the input.
    ///
    /// # Safety
    ///
    /// `bm` must be a valid mesh. `faces` must be valid for `faces_len`
    /// elements and all referenced faces must belong to `bm`.
    pub fn bms_extrude_face_region_dissolve_ortho(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        use_keep_orig: bool,
        use_normal_flip: bool,
        use_dissolve_ortho_edges: bool,
    ) -> bool;

    /// Marks each input face with `BM_ELEM_TAG`, then invokes BMesh's
    /// `extrude_discrete_faces` operator. Each face is extruded individually,
    /// so two formerly-adjacent input faces split apart along their shared
    /// edge rather than lifting as one connected region.
    ///
    /// The operator deletes the original faces internally (keeping their
    /// edges/verts as wall bottoms), so every input face is replaced by its
    /// lifted duplicate.
    ///
    /// `use_normal_flip` is forwarded to the operator; when `true` it reverses
    /// the winding of the side (wall) faces.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_extrude_discrete_faces_ex(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        use_normal_flip: bool,
    ) -> bool;

    /// Convenience wrapper for [`bms_extrude_discrete_faces_ex`] with
    /// `use_normal_flip = false`.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_extrude_discrete_faces(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
    ) -> bool;

    /// Marks each input edge with `BM_ELEM_TAG`, then invokes BMesh's
    /// `extrude_edge_only` operator. Each input edge gains one wall quad
    /// spanning the original edge and its lifted duplicate; a contiguous strip
    /// of input edges produces a continuous ribbon sharing vertical edges
    /// between adjacent walls.
    ///
    /// The originals are kept in place (no post-op kill), so input edges, verts
    /// and faces remain valid after the call.
    ///
    /// `use_normal_flip` is forwarded to the operator; when `true` it reverses
    /// the winding of each wall quad.
    ///
    /// `edges` must point to `edges_len` valid `*mut BMEdge` belonging to `bm`.
    /// Returns false if the operator rejected the input.
    pub fn bms_extrude_edge_only_ex(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        use_normal_flip: bool,
    ) -> bool;

    /// Convenience wrapper for [`bms_extrude_edge_only_ex`] with
    /// `use_normal_flip = false`.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_extrude_edge_only(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
    ) -> bool;

    /// Marks each input vert with `BM_ELEM_TAG`, then invokes BMesh's
    /// `extrude_vert_indiv` operator. For each input vert a duplicate vert is
    /// created at the same position and a fresh wire edge connects the original
    /// to the duplicate.
    ///
    /// The operation is purely additive: the original verts are never deleted
    /// (no post-op kill), so the input verts remain valid after the call. The
    /// caller is responsible for displacing the new verts after the call.
    ///
    /// `verts` must point to `verts_len` valid `*mut BMVert` belonging to `bm`.
    /// Returns false if the operator rejected the input.
    pub fn bms_extrude_vert_indiv(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
    ) -> bool;

    /// Drives BMesh's `spin` operator (rotate-extrude / lathe), sweeping the
    /// mixed-element `geom` buffer through `angle` radians in `steps` slices
    /// about `axis` passing through `cent`. `dvec` adds a per-step screw
    /// translation; pass null for a pure rotation. `use_merge` welds the first
    /// and last rings on a full 360 degree sweep, `use_normal_flip` reverses
    /// the generated face winding, and `use_duplicate` copies the input per
    /// step instead of extruding it.
    ///
    /// `geom` must point to `geom_len` valid `*mut BMHeader` belonging to `bm`.
    /// `cent`, `axis` and `dvec` are each `*const f32` to 3 floats (`dvec` may
    /// be null). `space` is the coordinate-frame matrix in which `cent`, `axis`,
    /// the rotation and `dvec` are interpreted: a `*const f32` to 16 contiguous
    /// floats in Blender's `float[4][4]` column-major order
    /// (`space[col * 4 + row]`). Pass null for world/identity space, which
    /// reproduces a plain world-space spin. The operator's `geom_last.out` slot
    /// is written into `out_geom_last` up to `out_geom_last_cap` entries; the
    /// buffer is overwritten in place. Returns the total `geom_last.out` count
    /// (which may exceed the capacity), or -1 if the operator rejected the input.
    pub fn bms_spin(
        bm: *mut BMesh,
        geom: *mut *mut BMHeader,
        geom_len: c_int,
        cent: *const f32,
        axis: *const f32,
        dvec: *const f32,
        angle: f32,
        steps: c_int,
        use_merge: bool,
        use_normal_flip: bool,
        use_duplicate: bool,
        space: *const f32,
        out_geom_last: *mut *mut BMHeader,
        out_geom_last_cap: c_int,
    ) -> c_int;

    /// Marks each input face with `BM_ELEM_TAG`, then invokes BMesh's
    /// `inset_region` operator. Every parameter the operator exposes is
    /// forwarded explicitly so A/B tests can pin each parameter axis.
    ///
    /// `faces_exclude` may be null with `faces_exclude_len = 0` when no
    /// exclusion list is needed; the operator itself only consults it when
    /// `use_outset` is true.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_inset_region(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        faces_exclude: *mut *mut BMFace,
        faces_exclude_len: c_int,
        use_boundary: bool,
        use_even_offset: bool,
        use_interpolate: bool,
        use_relative_offset: bool,
        use_edge_rail: bool,
        use_outset: bool,
        thickness: f32,
        depth: f32,
    ) -> bool;

    /// Marks each input face with `BM_ELEM_TAG`, then invokes BMesh's
    /// `inset_individual` operator. Each face is inset in its own plane,
    /// independently of its neighbours.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_inset_individual(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        use_even_offset: bool,
        use_interpolate: bool,
        use_relative_offset: bool,
        thickness: f32,
        depth: f32,
    ) -> bool;

    /// Bevel the supplied verts / edges / faces via BMesh's `bevel` BMOP.
    ///
    /// `geom` is a mixed element buffer of `*mut BMVert` / `*mut BMEdge` /
    /// `*mut BMFace` type-erased to `*mut BMHeader`, of length `geom_len`.
    /// The buffer is read but not modified; the operator flushes it into
    /// element tags internally. Only manifold edges (and the verts incident
    /// to them) are beveled — non-manifold edges in `geom` are ignored.
    ///
    /// Integer enum parameters use the operator's 0-based enum values:
    ///
    /// - `offset_type`: OFFSET=0, WIDTH=1, DEPTH=2, PERCENT=3, ABSOLUTE=4
    /// - `profile_type`: SUPERELLIPSE=0, CUSTOM=1
    /// - `affect`: VERTICES=0, EDGES=1
    /// - `face_strength_mode`: NONE=0, NEW=1, AFFECTED=2, ALL=3
    /// - `miter_outer` / `miter_inner`: SHARP=0, PATCH=1, ARC=2
    /// - `vmesh_method`: ADJ=0, CUTOFF=1
    ///
    /// `material` is a material-slot index, or `-1` to inherit from adjacent
    /// faces. The operator is a no-op when `offset <= 0.0`. Output geometry is
    /// mutated in place; re-read the mesh to observe the result.
    ///
    /// Returns false if the operator rejected the input.
    ///
    /// # Safety
    /// `bm` must be a valid mesh and `geom` must point to `geom_len` valid
    /// element pointers belonging to `bm`.
    pub fn bms_bevel(
        bm: *mut BMesh,
        geom: *mut *mut BMHeader,
        geom_len: c_int,
        offset: f32,
        offset_type: c_int,
        segments: c_int,
        profile: f32,
        profile_type: c_int,
        affect: c_int,
        clamp_overlap: bool,
        material: c_int,
        loop_slide: bool,
        mark_seam: bool,
        mark_sharp: bool,
        harden_normals: bool,
        face_strength_mode: c_int,
        miter_outer: c_int,
        miter_inner: c_int,
        spread: f32,
        vmesh_method: c_int,
    ) -> bool;

    /// Invoke BMesh's `dissolve_verts` BMOP on the supplied vertex set.
    /// Both BMOP slot parameters are forwarded explicitly:
    ///
    /// - `use_face_split` — split off face corners around dissolved verts
    ///   so surrounding geometry stays well-formed.
    /// - `use_boundary_tear` — split off face corners on boundary verts
    ///   instead of merging adjacent faces.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_dissolve_verts(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        use_face_split: bool,
        use_boundary_tear: bool,
    ) -> bool;

    /// Invoke BMesh's `unsubdivide` BMOP on the supplied vertex set,
    /// coarsening grid topology by `iterations` passes. The operator emits
    /// no output slot, so this only reports whether the op ran.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_unsubdivide(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        iterations: c_int,
    ) -> bool;

    /// Invoke BMesh's `dissolve_edges` BMOP on the supplied edge set. Every
    /// BMOP slot parameter is forwarded explicitly:
    ///
    /// - `use_verts` — also dissolve any vert left between exactly two
    ///   surviving edges.
    /// - `use_face_split` — split off face corners so surrounding geometry
    ///   stays well-formed.
    /// - `angle_threshold` — when `use_verts` is true, only dissolve a vert
    ///   if the angle between its remaining two edges is at most this value
    ///   (radians). Pass `std::f32::consts::PI` to disable the limit
    ///   (matches the BMOP's own default).
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_dissolve_edges(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        use_verts: bool,
        use_face_split: bool,
        angle_threshold: f32,
    ) -> bool;

    /// Invoke BMesh's `dissolve_faces` BMOP on the supplied face set.
    /// Partitions the set into edge-adjacent connected components and merges
    /// each component into a single face. Both BMOP slot parameters are
    /// forwarded explicitly:
    ///
    /// - `use_verts` — after merging, also dissolve any vert left between
    ///   exactly two surviving edges.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_dissolve_faces(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        use_verts: bool,
    ) -> bool;

    /// Capturing variant of [`bms_dissolve_faces`].
    ///
    /// Runs the same `dissolve_faces` BMOP and additionally copies the
    /// operator's `region.out` slot — one face pointer per successful
    /// per-region merge — into the caller-supplied buffer `out_buf` of
    /// capacity `out_cap` slots.
    ///
    /// Return value:
    /// - `-1` on operator init failure (mirrors the `false` return of
    ///   the non-capturing variant).
    /// - `>= 0` on success: the *total* merged-face count produced by
    ///   the operator. Up to `min(total, out_cap)` pointers are written
    ///   to `out_buf` in the slot's emit order; if `total > out_cap`
    ///   the buffer was undersized.
    ///
    /// `out_buf` may be null only when `out_cap` is zero (size-probing
    /// mode).
    pub fn bms_dissolve_faces_out(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        use_verts: bool,
        out_buf: *mut *mut BMFace,
        out_cap: c_int,
    ) -> c_int;

    /// Invoke BMesh's `triangle_fill` BMOP on the supplied edge loop.
    /// Triangulates the planar region bounded by `edges`, creating fill
    /// faces and their interior edges (no new vertices) and deleting no
    /// input edge. Both BMOP bool slots are forwarded explicitly:
    ///
    /// - `use_beauty` — run the beautify pass that rotates fill edges
    ///   toward a more equilateral triangulation.
    /// - `use_dissolve` — join the fill's interior edges back into n-gons.
    ///
    /// `normal` may be null. When non-null it must point to 3 `f32`
    /// giving the fill plane normal; a null pointer (or an all-zero
    /// vector) lets the operator derive the normal from the edge loop.
    ///
    /// `edges` points to an array of `edges_len` edge pointers belonging
    /// to `bm`. Returns false if the operator rejected the input.
    pub fn bms_triangle_fill(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        use_beauty: bool,
        use_dissolve: bool,
        normal: *const f32,
    ) -> bool;

    /// Capturing variant of [`bms_triangle_fill`].
    ///
    /// Runs the same `triangle_fill` BMOP and additionally copies the
    /// operator's `geom.out` slot — the newly-created fill edges and
    /// faces — into the caller-supplied buffer `out_buf` of capacity
    /// `out_cap` slots. Each written pointer is a `*mut BMHeader`; inspect
    /// the element's `htype` to distinguish edges from faces.
    ///
    /// Return value:
    /// - `-1` on operator init failure (mirrors the `false` return of
    ///   the non-capturing variant).
    /// - `>= 0` on success: the *total* number of new elements produced.
    ///   Up to `min(total, out_cap)` pointers are written to `out_buf`
    ///   in the slot's emit order; if `total > out_cap` the buffer was
    ///   undersized.
    ///
    /// `out_buf` may be null only when `out_cap` is zero (size-probing
    /// mode).
    pub fn bms_triangle_fill_out(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        use_beauty: bool,
        use_dissolve: bool,
        normal: *const f32,
        out_buf: *mut *mut BMHeader,
        out_cap: c_int,
    ) -> c_int;

    /// Maps to BMesh's `reverse_uvs` operator: reverses the active UV
    /// layer's per-loop values around each input face (a pure
    /// loop-customdata permutation, no topology change).
    ///
    /// `faces` points to an array of `faces_len` face pointers belonging
    /// to `bm`. Returns false if the operator rejected the input.
    pub fn bms_reverse_uvs(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
    ) -> bool;

    /// Maps to BMesh's `collapse_uvs` operator: averages and collapses the
    /// per-loop values of interpolatable loop-customdata layers (UVs and
    /// similar) across each input edge, merging the loops on either side.
    /// Operates on an edge buffer rather than a face buffer.
    ///
    /// `edges` points to an array of `edges_len` edge pointers belonging
    /// to `bm`. Returns false if the operator rejected the input.
    pub fn bms_collapse_uvs(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
    ) -> bool;

    /// Maps to BMesh's `average_vert_facedata` operator: averages the
    /// per-loop values of interpolatable loop-customdata layers across the
    /// loops of each input vertex and writes the averaged result back to
    /// those loops. Operates on a vertex buffer.
    ///
    /// `verts` points to an array of `verts_len` vertex pointers belonging
    /// to `bm`. Returns false if the operator rejected the input.
    pub fn bms_average_vert_facedata(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
    ) -> bool;

    /// Maps to BMesh's `pointmerge_facedata` operator: snaps the per-loop
    /// values of interpolatable loop-customdata layers across the loops of
    /// the input vertices to those of a single snap vertex.
    ///
    /// `verts` points to an array of `verts_len` vertex pointers belonging
    /// to `bm`; `vert_snap` is the single snap vertex. Returns false if the
    /// operator rejected the input.
    pub fn bms_pointmerge_facedata(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        vert_snap: *mut BMVert,
    ) -> bool;

    /// Maps to BMesh's `reverse_colors` operator: reverses the per-loop
    /// values of the color layer selected by `color_index` around each
    /// input face (a pure loop-customdata permutation, no topology change).
    ///
    /// `faces` points to an array of `faces_len` face pointers belonging
    /// to `bm`. `color_index` picks the color layer (0 selects the
    /// active/first layer). Returns false if the operator rejected the input.
    pub fn bms_reverse_colors(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        color_index: c_int,
    ) -> bool;

    /// Maps to BMesh's `rotate_uvs` operator: cycles the active UV
    /// layer's per-loop values forward by one corner around each input
    /// face (a pure loop-customdata permutation, no topology change).
    /// `use_ccw` selects the rotation direction (counter-clockwise when
    /// true, clockwise when false).
    ///
    /// `faces` points to an array of `faces_len` face pointers belonging
    /// to `bm`. Returns false if the operator rejected the input.
    pub fn bms_rotate_uvs(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        use_ccw: bool,
    ) -> bool;

    /// Maps to BMesh's `rotate_colors` operator: cycles the per-loop
    /// values of the color layer selected by `color_index` forward by one
    /// corner around each input face (a pure loop-customdata permutation,
    /// no topology change). `use_ccw` selects the rotation direction
    /// (counter-clockwise when true, clockwise when false).
    ///
    /// `faces` points to an array of `faces_len` face pointers belonging
    /// to `bm`. `color_index` picks the color layer (0 selects the
    /// active/first layer). Returns false if the operator rejected the input.
    pub fn bms_rotate_colors(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        use_ccw: bool,
        color_index: c_int,
    ) -> bool;

    /// Invoke BMesh's `recalc_face_normals` BMOP ("Recalculate Outside") on
    /// the supplied face set. Recomputes each face's cached normal from its
    /// corner positions and rewinds each manifold-connected component so it
    /// faces consistently outward. The mesh behind `bm` is mutated in place.
    ///
    /// `faces` / `faces_len` describe a caller-owned array of face pointers;
    /// the array itself is read only. Returns `true` on success, `false` if
    /// the operator rejected the input.
    pub fn bms_recalc_face_normals(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
    ) -> bool;

    /// "Recalculate Inside" companion of [`bms_recalc_face_normals`]: runs
    /// the same BMOP to make each component consistently wound, then reverses
    /// the winding of every face in `faces` so the component points inward.
    /// The mesh behind `bm` is mutated in place.
    ///
    /// Returns `true` on success, `false` if the operator rejected the input.
    pub fn bms_recalc_face_normals_inside(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
    ) -> bool;

    /// Invoke BMesh's `split_edges` BMOP on the supplied edge set, peeling
    /// the edges apart so adjacent faces no longer share them.
    ///
    /// `edges` / `edges_len` are the edges to separate. `verts` /
    /// `verts_len` are optional constraint vertices consulted only when
    /// `use_verts` is true; `verts` may be null with `verts_len` 0 (an
    /// empty set is forwarded to the operator). The mesh behind `bm` is
    /// mutated in place.
    ///
    /// The `edges.out` slot (the original edges that were disconnected)
    /// is copied into `out_edges`:
    ///
    /// - `-1` on operator init failure.
    /// - `>= 0` on success: the *total* edge count the slot produced. Up
    ///   to `min(total, out_cap)` pointers are written to `out_edges` in
    ///   the slot's emit order; if `total > out_cap` the buffer was
    ///   undersized.
    ///
    /// `out_edges` may be null only when `out_cap` is zero (size-probing
    /// mode).
    pub fn bms_split_edges(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        use_verts: bool,
        out_edges: *mut *mut BMEdge,
        out_cap: c_int,
    ) -> c_int;

    /// Invoke BMesh's `join_triangles` BMOP on the supplied face set.
    /// Merges adjacent triangle pairs into quads, subject to the delimit
    /// and angle gates. Every BMOP slot is forwarded verbatim:
    ///
    /// - `cmp_seam` / `cmp_sharp` / `cmp_uvs` / `cmp_vcols` /
    ///   `cmp_materials` — block a merge across a shared edge whose
    ///   attribute differs.
    /// - `angle_face_threshold` — max fold angle (radians) between the
    ///   two triangle normals; `>= PI` disables this gate.
    /// - `angle_shape_threshold` — max deviation (radians) of the merged
    ///   quad from an ideal shape; `>= PI` disables this gate.
    /// - `topology_influence` — 0..2 weighting biasing the merge order.
    /// - `deselect_joined` — clear the select flag on merged faces.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_join_triangles(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        cmp_seam: bool,
        cmp_sharp: bool,
        cmp_uvs: bool,
        cmp_vcols: bool,
        cmp_materials: bool,
        angle_face_threshold: f32,
        angle_shape_threshold: f32,
        topology_influence: f32,
        deselect_joined: bool,
    ) -> bool;

    /// Capturing variant of [`bms_join_triangles`].
    ///
    /// Runs the same `join_triangles` BMOP and additionally copies the
    /// operator's `faces.out` slot — the merged quads plus the triangles
    /// left un-merged — into the caller-supplied buffer `out_buf` of
    /// capacity `out_cap` slots.
    ///
    /// Return value semantics match [`bms_dissolve_faces_out`]: `-1`
    /// on init failure; otherwise the total slot count, with up to
    /// `min(total, out_cap)` pointers written to `out_buf` in emit
    /// order. `out_buf` may be null only when `out_cap` is zero
    /// (size-probing mode).
    pub fn bms_join_triangles_out(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        cmp_seam: bool,
        cmp_sharp: bool,
        cmp_uvs: bool,
        cmp_vcols: bool,
        cmp_materials: bool,
        angle_face_threshold: f32,
        angle_shape_threshold: f32,
        topology_influence: f32,
        deselect_joined: bool,
        out_buf: *mut *mut BMFace,
        out_cap: c_int,
    ) -> c_int;

    /// Invoke BMesh's `dissolve_limit` BMOP ("limited dissolve") on the
    /// supplied edge + vert sets. Greedy heap-driven planar / co-linear
    /// dissolve: every candidate within `angle_limit` (radians) is dissolved,
    /// subject to `delimit`.
    ///
    /// Either input buffer may be null with a zero length. Element pointers
    /// must remain valid for the duration of the call.
    ///
    /// - `angle_limit` — maximum dihedral angle (radians) between adjacent
    ///   face normals (edge dissolve) or between the two remaining edges
    ///   (vert dissolve). BMesh internally clamps to `pi/2`; this binding
    ///   forwards the caller's value verbatim.
    /// - `use_dissolve_boundaries` — also dissolve verts lying on mesh
    ///   boundaries (between exactly two wire / boundary edges).
    /// - `delimit` — bitmask of edge attributes that block dissolve across
    ///   them; use the `BMS_DELIM_*` constants below.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_dissolve_limit(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        angle_limit: f32,
        use_dissolve_boundaries: bool,
        delimit: c_int,
    ) -> bool;

    /// Capturing variant of [`bms_dissolve_limit`].
    ///
    /// Runs the same `dissolve_limit` BMOP and additionally copies the
    /// operator's `region.out` slot — one face pointer per successful
    /// edge-pass merge in heap-pop order — into the caller-supplied
    /// buffer `out_buf` of capacity `out_cap` slots.
    ///
    /// Return value semantics match [`bms_dissolve_faces_out`]: `-1`
    /// on init failure; otherwise the total slot count, with up to
    /// `min(total, out_cap)` pointers written to `out_buf`. `out_buf`
    /// may be null only when `out_cap` is zero (size-probing mode).
    pub fn bms_dissolve_limit_out(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        angle_limit: f32,
        use_dissolve_boundaries: bool,
        delimit: c_int,
        out_buf: *mut *mut BMFace,
        out_cap: c_int,
    ) -> c_int;

    /// Invoke BMesh's `dissolve_degenerate` BMOP on the supplied edge set.
    /// Numeric cleanup pass for collapsing zero-length edges and clipping
    /// near-collinear loop ears within a tolerance.
    ///
    /// - `dist` — distance tolerance (world units). Edges whose squared
    ///   length is below `dist * dist` are collapsed; loop ears whose
    ///   collinearity error is at most `dist` are split and the introduced
    ///   joining edge is also collapsed. At `dist = 0` only exact-zero-length
    ///   edges and exactly collinear ears qualify.
    ///
    /// The `edges` buffer may be null with a zero length; the per-edge
    /// length test is gated on the input-flag set so a zero-length input
    /// set is effectively a no-op even if the mesh contains degeneracies.
    /// Pass every edge for a whole-mesh cleanup.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_dissolve_degenerate(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        dist: f32,
    ) -> bool;

    /// Invoke BMesh's `subdivide_edges` BMOP on the supplied edge set. Each
    /// input edge is split into `cuts + 1` segments and the faces touching
    /// the cut edges are re-filled according to the chosen pattern. The mesh
    /// is mutated in place; no output geometry is captured.
    ///
    /// - `edges` — the edges to subdivide. The pointer may be null only when
    ///   `edges_len == 0`, in which case nothing is subdivided.
    /// - `cuts` — number of cut points per edge; `1` is a single midpoint cut.
    /// - `smooth` — smoothing factor for new vertices; `0.0` is no-op.
    /// - `smooth_falloff` — curve shaping the smoothing offset; one of the
    ///   `BMS_SUBD_FALLOFF_*` constants below.
    /// - `use_smooth_even` — keep the smoothing offset even across the mesh.
    /// - `fractal` — random displacement magnitude; `0.0` is no-op.
    /// - `along_normal` — factor (0..1) restricting fractal displacement
    ///   toward the surface normal.
    /// - `seed` — seed for the fractal random number generator.
    /// - `quad_corner_type` — fill pattern for single-corner quad cuts; one of
    ///   the `BMS_SUBD_CORNER_*` constants below.
    /// - `use_grid_fill` — fill fully-selected faces with a regular grid.
    /// - `use_single_edge` — tessellate the single-edge case in a quad/tri.
    /// - `use_only_quads` — only subdivide quads (loop-cut behaviour).
    /// - `use_sphere` — project the newly-created subdivision geometry onto a
    ///   sphere.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_subdivide_edges(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        cuts: c_int,
        smooth: f32,
        smooth_falloff: c_int,
        use_smooth_even: bool,
        fractal: f32,
        along_normal: f32,
        seed: c_int,
        quad_corner_type: c_int,
        use_grid_fill: bool,
        use_single_edge: bool,
        use_only_quads: bool,
        use_sphere: bool,
    ) -> bool;

    /// Invoke BMesh's `subdivide_edgering` BMOP on the supplied edge set. The
    /// edge-ring is subdivided across its connecting faces, inserting `cuts`
    /// new loops and re-filling the spans with interpolated geometry. The mesh
    /// is mutated in place; no output geometry is captured.
    ///
    /// - `edges` — the edge-ring to subdivide. The pointer may be null only
    ///   when `edges_len == 0`, in which case nothing is subdivided.
    /// - `cuts` — number of new loops inserted across the ring.
    /// - `interp_mode` — interpolation method for the new geometry; one of the
    ///   `BMS_RING_INTERP_*` constants below.
    /// - `smooth` — smoothing factor applied to the new loops.
    /// - `profile_shape` — falloff curve shaping the inserted profile; one of
    ///   the `BMS_SUBD_FALLOFF_*` constants.
    /// - `profile_shape_factor` — how far intermediary new edges are
    ///   shrunk/expanded along the profile.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_subdivide_edgering(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        cuts: c_int,
        interp_mode: c_int,
        smooth: f32,
        profile_shape: c_int,
        profile_shape_factor: f32,
    ) -> bool;

    /// Invoke BMesh's `bisect_edges` BMOP on the supplied edge set. This is
    /// the pure per-edge midpoint-split phase: each input edge is split into
    /// `cuts` evenly-spaced segments, introducing `cuts` two-valence vertices
    /// along it. No per-face dispatch or inner-vertex connection is performed.
    ///
    /// - `edges` — the edges to split. The pointer may be null only when
    ///   `edges_len == 0`, in which case nothing is split.
    /// - `cuts` — number of cuts per edge.
    ///
    /// The mesh is mutated in place; there is no output. Returns false if the
    /// operator rejected the input, true on normal completion.
    pub fn bms_bisect_edges(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        cuts: c_int,
    ) -> bool;

    /// Invoke BMesh's `collapse` BMOP on the supplied edge set. Collapses
    /// each connected group of input edges to a single vertex, welding their
    /// endpoints together and removing the now-degenerate geometry.
    ///
    /// - `edges` — the edges to collapse. The pointer may be null only when
    ///   `edges_len == 0`, in which case nothing is collapsed.
    /// - `uvs` — when true, also blend the per-loop custom data (UVs, vertex
    ///   colours, etc.) of the welded corners.
    ///
    /// The mesh is mutated in place; there is no output. Returns false if the
    /// operator rejected the input, true on normal completion.
    pub fn bms_collapse(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        uvs: bool,
    ) -> bool;

    /// Invoke BMesh's `connect_verts` BMOP on the supplied vertex set. For
    /// each face carrying two or more of the input verts as corners, inserts
    /// edges between selected corner pairs and splits the face. All three
    /// input slots are forwarded explicitly:
    ///
    /// - `verts` — the candidate corner verts to connect.
    /// - `faces_exclude` — faces that must not be split; may be null with a
    ///   zero length.
    /// - `check_degenerate` — when true, reject splits that would produce
    ///   overlapping or self-intersecting geometry.
    ///
    /// Element pointers must remain valid for the duration of the call.
    /// Returns false if the operator rejected the input.
    pub fn bms_connect_verts(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        faces_exclude: *mut *mut BMFace,
        faces_exclude_len: c_int,
        check_degenerate: bool,
    ) -> bool;

    /// Capturing variant of [`bms_connect_verts`].
    ///
    /// Runs the same `connect_verts` BMOP and additionally copies the
    /// operator's `edges.out` slot — the edges created by the face splits —
    /// into the caller-supplied buffer `out_buf` of capacity `out_cap` slots.
    ///
    /// Return value:
    /// - `-1` on operator init failure (mirrors the `false` return of the
    ///   non-capturing variant).
    /// - `>= 0` on success: the *total* created-edge count produced by the
    ///   operator. Up to `min(total, out_cap)` pointers are written to
    ///   `out_buf` in the slot's emit order; if `total > out_cap` the buffer
    ///   was undersized.
    ///
    /// `out_buf` may be null only when `out_cap` is zero (size-probing mode).
    pub fn bms_connect_verts_out(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        faces_exclude: *mut *mut BMFace,
        faces_exclude_len: c_int,
        check_degenerate: bool,
        out_buf: *mut *mut BMEdge,
        out_cap: c_int,
    ) -> c_int;

    /// Invoke BMesh's `connect_verts_concave` BMOP on the supplied face set.
    /// Each concave input face — one with more than three corners and at
    /// least one reflex corner — is cut into convex pieces along newly
    /// inserted divider edges; convex faces and triangles are left
    /// untouched. The single input slot is forwarded:
    ///
    /// - `faces` — the candidate faces to make convex.
    ///
    /// Element pointers must remain valid for the duration of the call.
    /// Returns false if the operator rejected the input.
    pub fn bms_connect_verts_concave(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
    ) -> bool;

    /// Capturing variant of [`bms_connect_verts_concave`].
    ///
    /// Runs the same `connect_verts_concave` BMOP and additionally copies
    /// the operator's `edges.out` slot — the surviving interior divider
    /// edges — into the caller-supplied buffer `out_buf` of capacity
    /// `out_cap` slots.
    ///
    /// Return value:
    /// - `-1` on operator init failure (mirrors the `false` return of the
    ///   non-capturing variant).
    /// - `>= 0` on success: the *total* created-edge count produced by the
    ///   operator. Up to `min(total, out_cap)` pointers are written to
    ///   `out_buf` in the slot's emit order; if `total > out_cap` the buffer
    ///   was undersized.
    ///
    /// `out_buf` may be null only when `out_cap` is zero (size-probing mode).
    pub fn bms_connect_verts_concave_out(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        out_buf: *mut *mut BMEdge,
        out_cap: c_int,
    ) -> c_int;

    /// Invoke BMesh's `connect_verts_nonplanar` BMOP on the supplied face
    /// set. Each input face whose corners deviate from a single plane by
    /// more than `angle_limit` (radians) is split along newly inserted
    /// diagonal edges until its pieces are flatter than the limit;
    /// sufficiently planar faces are left untouched. The two input slots
    /// are forwarded:
    ///
    /// - `faces` — the candidate faces to flatten.
    /// - `angle_limit` — maximum non-planarity (radians) before a split.
    ///
    /// This operator reads face normals; ensure mesh normals are current
    /// before calling. Element pointers must remain valid for the duration
    /// of the call. Returns false if the operator rejected the input.
    pub fn bms_connect_verts_nonplanar(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        angle_limit: f32,
    ) -> bool;

    /// Capturing variant of [`bms_connect_verts_nonplanar`].
    ///
    /// Runs the same `connect_verts_nonplanar` BMOP and additionally copies
    /// the operator's `edges.out` slot — the newly inserted diagonal edges —
    /// into the caller-supplied buffer `out_buf` of capacity `out_cap`
    /// slots.
    ///
    /// Return value:
    /// - `-1` on operator init failure (mirrors the `false` return of the
    ///   non-capturing variant).
    /// - `>= 0` on success: the *total* created-edge count produced by the
    ///   operator. Up to `min(total, out_cap)` pointers are written to
    ///   `out_buf` in the slot's emit order; if `total > out_cap` the buffer
    ///   was undersized.
    ///
    /// `out_buf` may be null only when `out_cap` is zero (size-probing mode).
    pub fn bms_connect_verts_nonplanar_out(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        angle_limit: f32,
        out_buf: *mut *mut BMEdge,
        out_cap: c_int,
    ) -> c_int;

    /// Face-capturing variant of [`bms_connect_verts_nonplanar`].
    ///
    /// Runs the same `connect_verts_nonplanar` BMOP and copies the
    /// operator's `faces.out` slot — every face the operator touched (both
    /// halves of each committed split and every leaf piece, each appearing
    /// once) — into the caller-supplied buffer `out_buf` of capacity
    /// `out_cap` slots.
    ///
    /// Return value:
    /// - `-1` on operator init failure (mirrors the `false` return of the
    ///   non-capturing variant).
    /// - `>= 0` on success: the *total* touched-face count produced by the
    ///   operator. Up to `min(total, out_cap)` pointers are written to
    ///   `out_buf` in the slot's emit order; if `total > out_cap` the buffer
    ///   was undersized.
    ///
    /// `out_buf` may be null only when `out_cap` is zero (size-probing mode).
    pub fn bms_connect_verts_nonplanar_faces_out(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        angle_limit: f32,
        out_buf: *mut *mut BMFace,
        out_cap: c_int,
    ) -> c_int;

    /// Invoke BMesh's `rotate_edges` BMOP on the supplied edge set. Each
    /// eligible edge — one shared by exactly two faces whose union forms a
    /// quad — is rotated (spun) to its other diagonal; ineligible edges are
    /// skipped by the operator's own filtering. The two input slots are
    /// forwarded:
    ///
    /// - `edges` — the candidate edges to rotate.
    /// - `use_ccw` — rotate counter-clockwise when true, clockwise when
    ///   false.
    ///
    /// Element pointers must remain valid for the duration of the call.
    /// Returns false if the operator rejected the input.
    pub fn bms_rotate_edges(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        use_ccw: bool,
    ) -> bool;

    /// Capturing variant of [`bms_rotate_edges`].
    ///
    /// Runs the same `rotate_edges` BMOP and additionally copies the
    /// operator's `edges.out` slot — one edge per successfully rotated edge —
    /// into the caller-supplied buffer `out_buf` of capacity `out_cap` slots.
    ///
    /// Return value:
    /// - `-1` on operator init failure (mirrors the `false` return of the
    ///   non-capturing variant).
    /// - `>= 0` on success: the *total* rotated-edge count produced by the
    ///   operator. Up to `min(total, out_cap)` pointers are written to
    ///   `out_buf` in the slot's emit order; if `total > out_cap` the buffer
    ///   was undersized.
    ///
    /// `out_buf` may be null only when `out_cap` is zero (size-probing mode).
    pub fn bms_rotate_edges_out(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        use_ccw: bool,
        out_buf: *mut *mut BMEdge,
        out_cap: c_int,
    ) -> c_int;

    /// Invoke BMesh's `connect_vert_pair` BMOP on a pair of verts. The
    /// operator connects the two input verts along the shortest path that
    /// stays within the surrounding faces: each edge the path crosses is
    /// split to introduce an intermediate vert, then the `connect_verts`
    /// sub-op inserts the connecting edges and splits the traversed faces.
    /// All three input slots are forwarded explicitly:
    ///
    /// - `verts` — the pair to connect; `verts_len` must be exactly 2.
    /// - `verts_exclude` — verts the path must avoid; may be null with a
    ///   zero length.
    /// - `faces_exclude` — faces the path must not cross; may be null with a
    ///   zero length.
    ///
    /// Element pointers must remain valid for the duration of the call.
    /// Returns false if the operator rejected the input.
    pub fn bms_connect_vert_pair(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        verts_exclude: *mut *mut BMVert,
        verts_exclude_len: c_int,
        faces_exclude: *mut *mut BMFace,
        faces_exclude_len: c_int,
    ) -> bool;

    /// Capturing variant of [`bms_connect_vert_pair`].
    ///
    /// Runs the same `connect_vert_pair` BMOP and additionally copies the
    /// operator's `edges.out` slot — the edges created along the connecting
    /// path — into the caller-supplied buffer `out_buf` of capacity
    /// `out_cap` slots.
    ///
    /// Return value:
    /// - `-1` on operator init failure (mirrors the `false` return of the
    ///   non-capturing variant).
    /// - `>= 0` on success: the *total* created-edge count produced by the
    ///   operator. Up to `min(total, out_cap)` pointers are written to
    ///   `out_buf` in the slot's emit order; if `total > out_cap` the buffer
    ///   was undersized.
    ///
    /// `out_buf` may be null only when `out_cap` is zero (size-probing mode).
    pub fn bms_connect_vert_pair_out(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        verts_exclude: *mut *mut BMVert,
        verts_exclude_len: c_int,
        faces_exclude: *mut *mut BMFace,
        faces_exclude_len: c_int,
        out_buf: *mut *mut BMEdge,
        out_cap: c_int,
    ) -> c_int;

    /// Invoke BMesh's `poke` BMOP on a face set and capture both of its
    /// output slots.
    ///
    /// Each input face is split into a triangle fan around a freshly-created
    /// centre vertex. `center_mode` follows the same convention as
    /// [`bms_face_poke_mode`] (`0` = MEAN, `1` = BOUNDS, `2` = MEAN_WEIGHTED)
    /// and is translated internally to the operator's native enum. `offset`
    /// lifts each centre vertex along its source face normal; when
    /// `use_relative_offset` is `true` the lift is scaled by the mean
    /// corner-to-centre distance of each face.
    ///
    /// The operator's two output slots are copied into caller-allocated
    /// buffers:
    /// - `verts.out` -> `out_verts` (one centre vertex per input face).
    /// - `out_verts` must point to `out_verts_cap` writable `*mut BMVert`.
    /// - `faces.out` -> `out_faces` (the fan-triangle faces).
    /// - `out_faces` must point to `out_faces_cap` writable `*mut BMFace`.
    ///
    /// For each slot the total length is written through the matching
    /// `r_*_len` out-param (which may be null); up to `min(len, cap)` pointers
    /// are written to the buffer. A reported length greater than its cap
    /// signals truncation, so callers can re-run on a fresh fixture with a
    /// larger buffer. Both slots are filled in the mesh's element-iteration
    /// order over the newly-created elements, not grouped per input face.
    /// Either buffer may be null only when its cap is zero (size-probing mode).
    ///
    /// Returns `0` on success, or `-1` if the operator rejected the input.
    pub fn bms_poke_out(
        bm: *mut BMesh,
        faces: *mut *mut BMFace,
        faces_len: c_int,
        center_mode: c_int,
        offset: f32,
        use_relative_offset: bool,
        out_verts: *mut *mut BMVert,
        out_verts_cap: c_int,
        r_verts_len: *mut c_int,
        out_faces: *mut *mut BMFace,
        out_faces_cap: c_int,
        r_faces_len: *mut c_int,
    ) -> c_int;

    /// Invoke BMesh's general-purpose `delete` BMOP on a mixed element
    /// buffer.
    ///
    /// `geom` points to a buffer of `geom_len` type-erased element
    /// pointers ([`BMHeader`]); any mix of vert / edge / face pointers may
    /// be cast to `*mut BMHeader` since the header is the first field of
    /// every element. The buffer may be null only when `geom_len` is zero.
    /// Element pointers must remain valid for the duration of the call.
    ///
    /// `context` is the operator's `context` enum int selecting which
    /// incident geometry is removed:
    /// - `1` VERTS — verts plus all geometry using them.
    /// - `2` EDGES — edges plus all faces using them.
    /// - `3` FACES_ONLY — only the faces themselves.
    /// - `4` EDGES_FACES — edges and their faces.
    /// - `5` FACES — faces plus verts/edges left unused afterwards.
    /// - `6` FACES_KEEP_BOUNDARY — like FACES but keep boundary edges of
    ///   the removed region.
    /// - `7` TAGGED_ONLY — only the elements in the buffer, leaving
    ///   incident geometry.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_delete_geom(
        bm: *mut BMesh,
        geom: *mut *mut BMHeader,
        geom_len: c_int,
        context: c_int,
    ) -> bool;

    /// Invoke BMesh's `weld_verts` BMOP, welding each source vert onto a
    /// target vert.
    ///
    /// `pairs` points to a flat buffer of `2 * pairs_len` [`BMVert`]
    /// pointers laid out as consecutive `(src, tar)` couples: `pairs[2*i]`
    /// is the source vert welded onto target `pairs[2*i+1]`. Each couple
    /// populates one entry of the operator's `targetmap` mapping slot
    /// (source as map key, target as mapped value). `pairs` may be null
    /// only when `pairs_len` is zero. Element pointers must remain valid
    /// for the duration of the call.
    ///
    /// `use_centroid` forwards the operator's `use_centroid` bool slot:
    /// when true each merged group settles at the centroid of its members,
    /// otherwise the group adopts the target vert's position.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_weld_verts(
        bm: *mut BMesh,
        pairs: *mut *mut BMVert,
        pairs_len: c_int,
        use_centroid: bool,
    ) -> bool;

    /// Run BMesh's `find_doubles` operator: detect groups of verts within
    /// `dist` and build a vert -> vert merge map without modifying topology.
    ///
    /// `verts` / `keep_verts` are arrays of `*mut BMVert` of length
    /// `verts_len` / `keep_len`; either may be null only when its length is
    /// zero. Verts outside `keep_verts` can only merge onto a vert inside it.
    /// Element pointers must remain valid for the duration of the call.
    ///
    /// On return, the `targetmap.out` map is written into `out_pairs` as flat
    /// `(src, tar)` couples: `out_pairs[2*i]` is the source vert,
    /// `out_pairs[2*i+1]` its target. `out_pairs` must point to at least
    /// `2 * out_cap` writable `*mut BMVert` slots.
    ///
    /// Returns the total number of map entries. When that exceeds `out_cap`,
    /// only the first `out_cap` couples are written, so a return value greater
    /// than `out_cap` signals truncation. Returns -1 if the operator rejected
    /// the input.
    pub fn bms_find_doubles(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        keep_verts: *mut *mut BMVert,
        keep_len: c_int,
        dist: f32,
        use_connected: bool,
        out_pairs: *mut *mut BMVert,
        out_cap: c_int,
    ) -> c_int;

    /// Run BMesh's `remove_doubles` operator: detect groups of verts within
    /// `dist` and weld each group in place, mutating the mesh's topology.
    /// There is no output map; inspect the mesh after the call.
    ///
    /// `verts` is an array of `*mut BMVert` of length `verts_len`; it may be
    /// null only when `verts_len` is zero. `dist` and `use_connected` control
    /// the merge distance and whether pairing is restricted to connected
    /// geometry. Element pointers must remain valid for the duration of the
    /// call.
    ///
    /// `keep_verts` / `keep_len` are accepted for signature parity with
    /// [`bms_find_doubles`]; the operator has no keep-verts slot, so they are
    /// not forwarded. Pass null with `keep_len` zero.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_remove_doubles(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        keep_verts: *mut *mut BMVert,
        keep_len: c_int,
        dist: f32,
        use_connected: bool,
    ) -> bool;

    /// Run BMesh's `pointmerge` operator: move every input vert onto
    /// `merge_co` and weld the set together onto a single survivor (the first
    /// input vert), mutating the mesh's topology in place. There is no output
    /// map; inspect the mesh after the call.
    ///
    /// `verts` is an array of `*mut BMVert` of length `verts_len`; it may be
    /// null only when `verts_len` is zero. `merge_co` must point to three
    /// readable `f32` (the target position); its contents are copied and left
    /// unmodified. Element pointers must remain valid for the duration of the
    /// call.
    ///
    /// Returns false if the operator rejected the input.
    pub fn bms_pointmerge(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        merge_co: *const f32,
    ) -> bool;

    /// Maps to BMesh's `duplicate` BMOP - clones a selection into disjoint
    /// coincident geometry within the same mesh.
    ///
    /// `geom` is a mixed array of `*mut BMHeader` (verts/edges/faces) of
    /// length `geom_len`; it may be null only when `geom_len` is zero. `use_edge_flip_from_face` forwards the bool
    /// in-slot of the same name. Element pointers must remain valid for the
    /// duration of the call.
    ///
    /// Outputs are written into caller-allocated buffers. Each `*_cap` is the
    /// number of entries (or couples, for the maps) the buffer can hold; any
    /// buffer may be null with its cap `0` to skip reading that slot. Every
    /// buffer is filled up to its cap and the true count is reported, so a
    /// count greater than the cap signals truncation.
    ///
    /// - `out_geom` receives the `geom.out` clone elements (verts, edges,
    ///   faces). It must hold at least `out_geom_cap` `*mut BMHeader`. The
    ///   total count is the function's return value.
    /// - The five `*_map` buffers receive their mapping slots as flat
    ///   `(src, dst)` couples: `buf[2*i]` is the key, `buf[2*i+1]` the mapped
    ///   value, so each must hold at least `2 * cap` pointers. The operator
    ///   inserts each correspondence in both directions, so an N-element map
    ///   yields `2 * N` couples. Per-slot couple counts are written through
    ///   the `out_*_count` out-params (each may be null to ignore):
    ///   `out_boundary_map`/`out_edge_map` are edge->edge,
    ///   `out_isovert_map`/`out_vert_map` are vert->vert, `out_face_map` is
    ///   face->face.
    ///
    /// Returns the total `geom.out` count, or -1 if the operator rejected the
    /// input (in which case no out-params are written).
    pub fn bms_duplicate(
        bm: *mut BMesh,
        geom: *mut *mut BMHeader,
        geom_len: c_int,
        use_edge_flip_from_face: bool,
        out_geom: *mut *mut BMHeader,
        out_geom_cap: c_int,
        out_boundary_map: *mut *mut BMHeader,
        out_boundary_cap: c_int,
        out_boundary_count: *mut c_int,
        out_isovert_map: *mut *mut BMVert,
        out_isovert_cap: c_int,
        out_isovert_count: *mut c_int,
        out_vert_map: *mut *mut BMVert,
        out_vert_cap: c_int,
        out_vert_count: *mut c_int,
        out_edge_map: *mut *mut BMEdge,
        out_edge_cap: c_int,
        out_edge_count: *mut c_int,
        out_face_map: *mut *mut BMFace,
        out_face_cap: c_int,
        out_face_count: *mut c_int,
    ) -> c_int;

    /// Invoke BMesh's `split` operator on the `geom` set, duplicating it
    /// and tearing the copy off as a topologically disjoint set within the
    /// same mesh. `geom` (length `geom_len`) is a mixed vert/edge/face
    /// header buffer fed to the operator's `geom` in-slot; it may be null
    /// with `geom_len` `0`. Element pointers must remain valid for the
    /// duration of the call. `use_only_faces` suppresses duplication of
    /// loose verts/edges.
    ///
    /// Outputs are written into caller-allocated buffers. Each `*_cap` is
    /// the number of entries (or couples, for the maps) the buffer can
    /// hold; any buffer may be null with its cap `0` to skip reading that
    /// slot. Every buffer is filled up to its cap and the true count is
    /// reported, so a count greater than the cap signals truncation.
    ///
    /// - `out_geom` receives the `geom.out` split-off elements (verts,
    ///   edges, faces). It must hold at least `out_geom_cap`
    ///   `*mut BMHeader`. The total count is the function's return value.
    /// - `out_boundary_map` (edge->edge) and `out_isovert_map`
    ///   (vert->vert) receive their mapping slots as flat `(src, dst)`
    ///   couples: `buf[2*i]` is the key, `buf[2*i+1]` the mapped value, so
    ///   each must hold at least `2 * cap` pointers. Per-slot couple counts
    ///   are written through the `out_*_count` out-params (each may be null
    ///   to ignore).
    ///
    /// Returns the total `geom.out` count, or -1 if the operator rejected
    /// the input (in which case no out-params are written).
    pub fn bms_split(
        bm: *mut BMesh,
        geom: *mut *mut BMHeader,
        geom_len: c_int,
        use_only_faces: bool,
        out_geom: *mut *mut BMHeader,
        out_geom_cap: c_int,
        out_boundary_map: *mut *mut BMEdge,
        out_boundary_cap: c_int,
        out_boundary_count: *mut c_int,
        out_isovert_map: *mut *mut BMVert,
        out_isovert_cap: c_int,
        out_isovert_count: *mut c_int,
    ) -> c_int;

    /// Invoke BMesh's `mirror` operator on the `geom` set: duplicate it,
    /// reflect the duplicate across the `axis` plane in `matrix` space,
    /// flip the reflected faces' winding, and weld each reflected vert
    /// back onto its original when the original lies within `merge_dist`
    /// of the mirror plane.
    ///
    /// `geom` (length `geom_len`) is a mixed vert/edge/face header buffer
    /// fed to the operator's `geom` in-slot; it may be null with `geom_len`
    /// `0`. Element pointers must remain valid for the duration of the call.
    ///
    /// `matrix` points to 16 `f32` in Blender's native column-major 4x4
    /// layout (read as `m[i / 4][i % 4]`, so translation occupies indices
    /// 12, 13, 14). It may be null, in which case the identity matrix is
    /// used. `merge_dist` is the maximum distance from the mirror plane
    /// for welding (0 disables welding). `axis` selects the negated
    /// component: 0 = X, 1 = Y, 2 = Z. `mirror_u` / `mirror_v` /
    /// `mirror_udim` control UV mirroring of the reflected faces.
    ///
    /// `out_geom` receives the `geom.out` mirrored elements (verts, edges,
    /// faces) and must hold at least `out_geom_cap` `*mut BMHeader`; the
    /// buffer is filled up to its cap and the true count returned, so a
    /// count greater than `out_geom_cap` signals truncation. `out_geom`
    /// may be null with `out_geom_cap` `0` to skip read-back.
    ///
    /// Returns the total `geom.out` count, or -1 if the operator rejected
    /// the input (in which case `out_geom` is not written).
    pub fn bms_mirror(
        bm: *mut BMesh,
        geom: *mut *mut BMHeader,
        geom_len: c_int,
        matrix: *const f32,
        merge_dist: f32,
        axis: c_int,
        mirror_u: bool,
        mirror_v: bool,
        mirror_udim: bool,
        out_geom: *mut *mut BMHeader,
        out_geom_cap: c_int,
    ) -> c_int;

    /// Invoke BMesh's `transform` operator: apply a 4x4 affine matrix to
    /// the positions of the input vertices, mutating the mesh in place.
    ///
    /// `verts` (length `verts_len`) is a vertex pointer buffer fed to the
    /// operator's `verts` in-slot; it may be null with `verts_len` `0`, in
    /// which case the call is a no-op. Element pointers must remain valid
    /// for the duration of the call.
    ///
    /// `matrix` points to 16 `f32` in Blender's native column-major 4x4
    /// layout (read as `m[i / 4][i % 4]`, so translation occupies indices
    /// 12, 13, 14). It may be null, in which case the identity matrix is
    /// used.
    ///
    /// `space` uses the same 16-float column-major layout. When non-null
    /// and non-zero, `matrix` is re-expressed in that frame before being
    /// applied. A null pointer feeds the all-zeros sentinel, treated as
    /// "no space transform", so `matrix` is applied directly in world
    /// space.
    ///
    /// `use_shapekey`, when `true`, applies the same transform to each
    /// vertex's shape-key coordinates as well.
    ///
    /// The operator has no output; vertex positions are mutated in place.
    pub fn bms_transform(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        matrix: *const f32,
        space: *const f32,
        use_shapekey: bool,
    );

    /// Invoke BMesh's `smooth_vert` operator: relax each input vertex toward
    /// the unweighted average of its connected neighbours (the other endpoint
    /// of every incident edge), blended from its original position by
    /// `factor`. A single double-buffered pass makes the result
    /// order-independent.
    ///
    /// `verts` (length `verts_len`) is the vertex buffer fed to the
    /// operator's `verts` in-slot; it may be null with `verts_len` `0`, in
    /// which case the call is a no-op. Element pointers must remain valid for
    /// the duration of the call.
    ///
    /// `factor` is the lerp amount from each vertex's original position toward
    /// the neighbour average. `mirror_clip_x/y/z`, when `true`, snap the
    /// target coordinate on that axis to 0 for any vertex whose original
    /// coordinate on that axis lies within `clip_dist` of 0. `use_axis_x/y/z`
    /// gate write-back: only enabled-axis coordinates of the target are
    /// applied.
    ///
    /// The operator has no output; vertex positions are mutated in place.
    pub fn bms_smooth_vert(
        bm: *mut BMesh,
        verts: *mut *mut BMVert,
        verts_len: c_int,
        factor: f32,
        mirror_clip_x: bool,
        mirror_clip_y: bool,
        mirror_clip_z: bool,
        clip_dist: f32,
        use_axis_x: bool,
        use_axis_y: bool,
        use_axis_z: bool,
    );

    /// Invoke BMesh's `symmetrize` operator: bisect `geom` along an
    /// axis-aligned plane, clear the half selected by `direction`, then
    /// mirror the surviving half across the plane and weld the duplicated
    /// geometry at the seam within `dist`.
    ///
    /// `geom` (length `geom_len`) is a mixed vert/edge/face header buffer
    /// fed to the operator's `input` in-slot; it may be null with `geom_len`
    /// `0`. Element pointers must remain valid for the duration of the call.
    ///
    /// `direction` selects the signed-axis half that is kept and mirrored:
    /// 0 = -X, 1 = -Y, 2 = -Z, 3 = +X, 4 = +Y, 5 = +Z. `dist` is the
    /// on-plane merge tolerance used when welding the seam. `use_shapekey`
    /// controls whether shape-key coordinates are transformed alongside the
    /// base geometry.
    ///
    /// `out_geom` receives the `geom.out` symmetric elements (verts, edges,
    /// faces) and must hold at least `out_geom_cap` `*mut BMHeader`; the
    /// buffer is filled up to its cap and the true count returned, so a
    /// count greater than `out_geom_cap` signals truncation. `out_geom`
    /// may be null with `out_geom_cap` `0` to skip read-back.
    ///
    /// Returns the total `geom.out` count, or -1 if the operator rejected
    /// the input (in which case `out_geom` is not written).
    pub fn bms_symmetrize(
        bm: *mut BMesh,
        geom: *mut *mut BMHeader,
        geom_len: c_int,
        direction: c_int,
        dist: f32,
        use_shapekey: bool,
        out_geom: *mut *mut BMHeader,
        out_geom_cap: c_int,
    ) -> c_int;

    /// Invoke BMesh's `bisect_plane` operator: slice `geom` by an arbitrary
    /// plane, optionally snapping on-plane verts onto the plane and clearing
    /// the geometry on one or both sides.
    ///
    /// `geom` (length `geom_len`) is a mixed vert/edge/face header buffer fed
    /// to the operator's `geom` in-slot; it may be null with `geom_len` `0`.
    /// Element pointers must remain valid for the duration of the call.
    ///
    /// `plane_co` and `plane_no` each point to 3 floats: a point on the
    /// cutting plane and the plane normal. The normal's sign defines which
    /// side is "positive" (outer) and which is "negative" (inner). `dist` is
    /// the tolerance within which a vert is treated as exactly on the plane.
    /// `use_snap_center` snaps on-plane verts onto the plane; `clear_inner`
    /// removes the negative side; `clear_outer` removes the positive side.
    ///
    /// Two output buffers are filled independently:
    /// - `out_geom` receives the `geom.out` surviving elements (verts, edges,
    ///   faces) and must hold at least `out_geom_cap` `*mut BMHeader`; the
    ///   buffer is filled up to its cap and the true count returned, so a
    ///   count greater than `out_geom_cap` signals truncation.
    /// - `out_cut` receives the `geom_cut.out` on-plane seam elements (verts,
    ///   edges) and must hold at least `out_cut_cap` `*mut BMHeader`; its true
    ///   count is written through `out_cut_len`, which may exceed `out_cut_cap`
    ///   to signal truncation.
    ///
    /// Either output buffer may be null with a cap of `0` to skip its
    /// read-back, and `out_cut_len` may be null.
    ///
    /// Returns the total `geom.out` count, or -1 if the operator rejected the
    /// input (in which case neither output buffer nor `out_cut_len` is
    /// written).
    pub fn bms_bisect_plane(
        bm: *mut BMesh,
        geom: *mut *mut BMHeader,
        geom_len: c_int,
        plane_co: *const f32,
        plane_no: *const f32,
        dist: f32,
        use_snap_center: bool,
        clear_inner: bool,
        clear_outer: bool,
        out_geom: *mut *mut BMHeader,
        out_geom_cap: c_int,
        out_cut: *mut *mut BMHeader,
        out_cut_cap: c_int,
        out_cut_len: *mut c_int,
    ) -> c_int;

    /// Invoke BMesh's `bridge_loops` BMOP on the supplied edge set, building
    /// geometry that spans two or more edge loops. The input slots are
    /// forwarded directly: `use_pairs` bridges consecutive loop pairs (and
    /// requires an even loop count), `use_cyclic` treats the loops as closed,
    /// `use_merge` welds the loops instead of creating bridging faces (and
    /// requires equal loop edge counts), `merge_factor` is the weld
    /// interpolation factor, and `twist_offset` rotates closed-loop matching.
    ///
    /// The operator cancels — leaving the mesh unchanged — on three
    /// validation failures: fewer than two loops, an odd loop count under
    /// `use_pairs`, or unequal loop edge counts under `use_merge`.
    ///
    /// `edges` points to `edges_len` `*mut BMEdge`. Returns `true` on success,
    /// `false` when the operator cancelled (a no-op) or its init was rejected.
    pub fn bms_bridge_loops(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        use_pairs: bool,
        use_cyclic: bool,
        use_merge: bool,
        merge_factor: f32,
        twist_offset: c_int,
    ) -> bool;

    /// Capturing variant of [`bms_bridge_loops`] for the `faces.out` slot.
    ///
    /// Runs the same `bridge_loops` BMOP and copies the operator's `faces.out`
    /// slot — the faces created by the bridge — into the caller-supplied
    /// buffer `faces_out` of capacity `faces_cap` slots.
    ///
    /// Return value:
    /// - `-1` on operator init failure, or when the operator cancelled (one of
    ///   the three validation failures), distinguishing the no-op from a
    ///   zero-face success.
    /// - `>= 0` on success: the *total* produced-face count. Up to
    ///   `min(total, faces_cap)` pointers are written to `faces_out` in the
    ///   slot's emit order; if `total > faces_cap` the buffer was undersized.
    ///
    /// `faces_out` may be null only when `faces_cap` is zero (size-probing).
    pub fn bms_bridge_loops_out(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        use_pairs: bool,
        use_cyclic: bool,
        use_merge: bool,
        merge_factor: f32,
        twist_offset: c_int,
        faces_out: *mut *mut BMFace,
        faces_cap: c_int,
    ) -> c_int;

    /// Capturing variant of [`bms_bridge_loops`] for the `edges.out` slot.
    ///
    /// Runs the same `bridge_loops` BMOP and copies the operator's `edges.out`
    /// slot — the rung edges created across the bridge — into the
    /// caller-supplied buffer `edges_out` of capacity `edges_cap` slots.
    ///
    /// Return value:
    /// - `-1` on operator init failure, or when the operator cancelled.
    /// - `>= 0` on success: the *total* produced-edge count. Up to
    ///   `min(total, edges_cap)` pointers are written to `edges_out` in the
    ///   slot's emit order; if `total > edges_cap` the buffer was undersized.
    ///
    /// `edges_out` may be null only when `edges_cap` is zero (size-probing).
    pub fn bms_bridge_loops_edges_out(
        bm: *mut BMesh,
        edges: *mut *mut BMEdge,
        edges_len: c_int,
        use_pairs: bool,
        use_cyclic: bool,
        use_merge: bool,
        merge_factor: f32,
        twist_offset: c_int,
        edges_out: *mut *mut BMEdge,
        edges_cap: c_int,
    ) -> c_int;
}

// ---- Delimit bits for `bms_dissolve_limit` ----
//
// Bitmask values matching BMesh's `BMO_Delimit` enum. Combine with `|`.
pub const BMS_DELIM_NORMAL: c_int = 1 << 0;
pub const BMS_DELIM_MATERIAL: c_int = 1 << 1;
pub const BMS_DELIM_SEAM: c_int = 1 << 2;
pub const BMS_DELIM_SHARP: c_int = 1 << 3;
pub const BMS_DELIM_UV: c_int = 1 << 4;

// ---- Quad corner types for `bms_subdivide_edges` ----
//
// Values matching BMesh's quad innervert enum, selecting the fill pattern
// applied when a quad is cut on a single corner.
pub const BMS_SUBD_CORNER_INNERVERT: c_int = 0;
pub const BMS_SUBD_CORNER_PATH: c_int = 1;
pub const BMS_SUBD_CORNER_FAN: c_int = 2;
pub const BMS_SUBD_CORNER_STRAIGHT_CUT: c_int = 3;

// ---- Smooth-falloff curves for `bms_subdivide_edges` ----
//
// Values matching BMesh's subdivide falloff enum, shaping the curve applied
// to the smoothing offset of newly created vertices. Note the gap at 5/6:
// the enum reserves 7 for the inverse-square curve.
pub const BMS_SUBD_FALLOFF_SMOOTH: c_int = 0;
pub const BMS_SUBD_FALLOFF_SPHERE: c_int = 1;
pub const BMS_SUBD_FALLOFF_ROOT: c_int = 2;
pub const BMS_SUBD_FALLOFF_SHARP: c_int = 3;
pub const BMS_SUBD_FALLOFF_LIN: c_int = 4;
pub const BMS_SUBD_FALLOFF_INVSQUARE: c_int = 7;

// ---- Interpolation modes for `bms_subdivide_edgering` ----
//
// Values matching BMesh's edge-ring interpolation enum, selecting how the
// geometry inserted across the ring is shaped.
pub const BMS_RING_INTERP_LINEAR: c_int = 0;
pub const BMS_RING_INTERP_PATH: c_int = 1;
pub const BMS_RING_INTERP_SURFACE: c_int = 2;

// ---- Counts ----

unsafe extern "C" {
    pub fn bms_totvert(bm: *mut BMesh) -> c_int;
    pub fn bms_totedge(bm: *mut BMesh) -> c_int;
    pub fn bms_totface(bm: *mut BMesh) -> c_int;
    pub fn bms_totloop(bm: *mut BMesh) -> c_int;
}

// ---- Render-time tessellation ----
//
// `bms_mesh_calc_tessellation` fills a caller-allocated buffer with
// `bms_mesh_calc_looptri_count(bm)` triples of `[*mut BMLoop; 3]`,
// where each triple references three loops of an existing face in the
// mesh. The mesh itself is not modified. Each face contributes
// `face.len - 2` consecutive triples; the per-face order matches BMesh's
// internal iteration order (`BM_ITER_MESH(BM_FACES_OF_MESH)`).

unsafe extern "C" {
    /// Number of looptri triples produced by `bms_mesh_calc_tessellation`
    /// on the current mesh. Equals `totloop - 2 * totface`. Returns 0 for
    /// an empty mesh.
    pub fn bms_mesh_calc_looptri_count(bm: *mut BMesh) -> c_int;

    /// Compute render-time looptri tessellation. `out_tris` must point to
    /// a buffer of at least `bms_mesh_calc_looptri_count(bm)` triples,
    /// each a `[*mut BMLoop; 3]`. The buffer is overwritten in place; the
    /// mesh is not modified.
    pub fn bms_mesh_calc_tessellation(bm: *mut BMesh, out_tris: *mut [*mut BMLoop; 3]);
}

// ---- Destructive per-face triangulation ----
//
// `bms_face_triangulate` triangulates a single face in place. The five
// `BMS_TRIANGULATE_QUAD_*` values select the diagonal for a quad input;
// the two `BMS_TRIANGULATE_NGON_*` values select the algorithm for an
// n-gon input. The integer values are part of the FFI contract and must
// match the BMesh-side `MOD_TRIANGULATE_QUAD_*` / `MOD_TRIANGULATE_NGON_*`
// enums.

/// Quad diagonal: pick the diagonal that maximises a "beauty" metric.
pub const BMS_TRIANGULATE_QUAD_BEAUTY: c_int = 0;
/// Quad diagonal: always use the (0, 2) diagonal.
pub const BMS_TRIANGULATE_QUAD_FIXED: c_int = 1;
/// Quad diagonal: alternate between (0, 2) and (1, 3) per face.
pub const BMS_TRIANGULATE_QUAD_ALTERNATE: c_int = 2;
/// Quad diagonal: pick the shorter of the two diagonals.
pub const BMS_TRIANGULATE_QUAD_SHORTEDGE: c_int = 3;
/// Quad diagonal: pick the longer of the two diagonals.
pub const BMS_TRIANGULATE_QUAD_LONGEDGE: c_int = 4;

/// N-gon algorithm: polyfill followed by edge-flip beautification.
pub const BMS_TRIANGULATE_NGON_BEAUTY: c_int = 0;
/// N-gon algorithm: simple ear-clipping (no beautification pass).
pub const BMS_TRIANGULATE_NGON_EARCLIP: c_int = 1;

unsafe extern "C" {
    /// Triangulate a single face in place. `quad_method` and `ngon_method`
    /// must be one of the `BMS_TRIANGULATE_QUAD_*` / `BMS_TRIANGULATE_NGON_*`
    /// constants.
    ///
    /// When `r_faces_new` is non-null, it must point to a caller-allocated
    /// buffer of at least `f.len - 3` `*mut BMFace` slots and
    /// `*r_faces_new_tot` must be set to `f.len - 3` on entry. On return,
    /// `*r_faces_new_tot` holds the count of new faces actually written
    /// (less than `f.len - 3` if some target triangles coincided with
    /// faces already in the mesh).
    ///
    /// Both `r_faces_new` and `r_faces_new_tot` may be null when the new-face
    /// list is not needed. `use_tag` causes the new faces and edges to be
    /// marked with `BM_ELEM_TAG`.
    pub fn bms_face_triangulate(
        bm: *mut BMesh,
        f: *mut BMFace,
        quad_method: c_int,
        ngon_method: c_int,
        use_tag: bool,
        r_faces_new: *mut *mut BMFace,
        r_faces_new_tot: *mut c_int,
    );
}

// ---- Snapshot: flat-buffer mesh export ----
//
// Caller pre-sizes the buffers via the tot* functions, then calls bms_snapshot
// to fill them in. Returns false if any buffer was too small. Buffer layout:
//
//   out_verts: 3 f32 per vertex
//   out_edges: 2 vertex-index per edge
//   out_face_offsets: 1 i32 per face — index into out_face_verts of that face's first vert
//   out_face_verts: face vertex indices, concatenated
//
// Vertex indices in the returned arrays are dense (assigned 0..tot_vert in
// the order vertices appear in `BM_ITER_MESH(BM_VERTS_OF_MESH)`).

unsafe extern "C" {
    pub fn bms_snapshot(
        bm: *mut BMesh,
        out_verts: *mut f32,
        out_verts_cap: c_int,
        out_edges: *mut c_int,
        out_edges_cap: c_int,
        out_face_offsets: *mut c_int,
        out_face_offsets_cap: c_int,
        out_face_verts: *mut c_int,
        out_face_verts_cap: c_int,
        out_face_lens: *mut c_int,
        out_face_lens_cap: c_int,
        // Optional: pass null + 0 to skip; otherwise must be totface*3 floats.
        out_face_normals: *mut f32,
        out_face_normals_cap: c_int,
    ) -> bool;

    /// Walks all faces and calls `BM_face_normal_update` on each. Does NOT
    /// recompute vertex normals (BMesh's full mesh-normals-update pulls in
    /// task-parallel machinery that is not linked here); use
    /// [`bms_mesh_vert_normals_update`] when vertex normals are needed.
    pub fn bms_mesh_normals_update(bm: *mut BMesh);

    /// Recomputes every vertex's stored normal across the whole mesh, writing
    /// each into the vertex's `no` field. Vertex normals are accumulated
    /// serially from incident faces (whose normals are refreshed in the same
    /// pass), so this does not require any task-scheduler symbols and is
    /// correct even when face normals were stale on entry. `bm` must be a
    /// valid, non-null mesh pointer.
    pub fn bms_mesh_vert_normals_update(bm: *mut BMesh);
}

// ---- Customdata layer access ----

/// CD layer-type constants. These mirror Blender's `eCustomDataType` integer
/// values directly.
#[repr(i32)]
pub enum CdType {
    /// `CD_PROP_FLOAT`
    Float = 10,
    /// `CD_PROP_INT32`
    Int32 = 11,
    /// `CD_PROP_COLOR` — 4× `f32`.
    Color = 47,
    /// `CD_PROP_FLOAT3`
    Float3 = 48,
    /// `CD_PROP_FLOAT2`
    Float2 = 49,
}

#[repr(i32)]
pub enum BmsDomain {
    Vert = 0,
    Edge = 1,
    Loop = 2,
    Face = 3,
}

unsafe extern "C" {
    /// Register a named CD layer on `domain`. Returns the byte offset of
    /// the new layer inside an element's CD block, or `-1` on error.
    pub fn bms_layer_add_named(
        bm: *mut BMesh,
        domain: c_int,
        type_: c_int,
        name: *const std::os::raw::c_char,
    ) -> c_int;

    /// Read/write typed CD values at a given layer offset on any BM
    /// element. `elem` may be any of `*mut BMVert`/`BMEdge`/`BMLoop`/`BMFace`;
    /// the wrappers dereference via the uniform `BMHead.data` prefix.
    pub fn bms_elem_get_float(elem: *mut std::ffi::c_void, offset: c_int, out: *mut f32);
    pub fn bms_elem_set_float(elem: *mut std::ffi::c_void, offset: c_int, value: f32);
    pub fn bms_elem_get_float2(elem: *mut std::ffi::c_void, offset: c_int, out: *mut f32);
    pub fn bms_elem_set_float2(elem: *mut std::ffi::c_void, offset: c_int, value: *const f32);
    pub fn bms_elem_get_float3(elem: *mut std::ffi::c_void, offset: c_int, out: *mut f32);
    pub fn bms_elem_set_float3(elem: *mut std::ffi::c_void, offset: c_int, value: *const f32);
    pub fn bms_elem_get_float4(elem: *mut std::ffi::c_void, offset: c_int, out: *mut f32);
    pub fn bms_elem_set_float4(elem: *mut std::ffi::c_void, offset: c_int, value: *const f32);
    pub fn bms_elem_get_int(elem: *mut std::ffi::c_void, offset: c_int, out: *mut c_int);
    pub fn bms_elem_set_int(elem: *mut std::ffi::c_void, offset: c_int, value: c_int);

    /// Number of elements of `domain` currently in the mesh (matches the
    /// matching `bm->tot*` field). Returns `-1` for an invalid domain.
    pub fn bms_domain_elem_count(bm: *mut BMesh, domain: c_int) -> c_int;

    /// Look up a CD layer's byte offset by `(domain, type, name)` without
    /// adding it. Returns the offset if a matching layer exists, or `-1`
    /// if no such layer is registered. Read-only.
    pub fn bms_layer_find_offset_named(
        bm: *mut BMesh,
        domain: c_int,
        type_: c_int,
        name: *const std::os::raw::c_char,
    ) -> c_int;

    /// Read every element-of-`domain`'s CD value at `offset` into a flat
    /// float buffer, in `BM_ITER_MESH` order. `components` is 1/2/3/4
    /// (matching `bms_elem_get_floatN`); the function writes
    /// `bms_domain_elem_count(bm, domain) * components` floats. For loops
    /// the iteration order is faces in `BM_ITER_MESH(BM_FACES_OF_MESH)`
    /// order, then each face's loop cycle starting at
    /// `BM_FACE_FIRST_LOOP` — the same layout `bms_snapshot` uses for
    /// `face_verts`. Returns `false` on an invalid domain / unsupported
    /// component count / buffer too small.
    pub fn bms_layer_read_floats(
        bm: *mut BMesh,
        domain: c_int,
        offset: c_int,
        components: c_int,
        out_floats: *mut f32,
        out_floats_cap: c_int,
    ) -> bool;

    /// Read every element-of-`domain`'s `CD_PROP_INT32` value at `offset`
    /// into a flat int buffer, in the same iteration order as
    /// [`bms_layer_read_floats`]. Returns `false` on an invalid domain or
    /// buffer too small.
    pub fn bms_layer_read_ints(
        bm: *mut BMesh,
        domain: c_int,
        offset: c_int,
        out_ints: *mut c_int,
        out_ints_cap: c_int,
    ) -> bool;
}

// ---- Region-inset customdata-merge trace ----

/// One per-corner customdata-merge invocation recorded by
/// [`bms_inset_region_merge_trace`].
///
/// Each of the four loops is identified by its owning face index and its
/// corner-vertex index (both `BM_elem_index` values, valid once the trace
/// call has rebuilt the index tables). `*_pre` / `*_post` are the traced
/// layer's value on that loop before and after the merge writes; `comps`
/// is the layer's component count (1..=4) with unused components zeroed.
/// Any index field is `-1` if its loop was not captured.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct BmsMergeInvocation {
    pub vert_index: c_int,
    pub a_inner_face: c_int,
    pub a_inner_corner_vert: c_int,
    pub b_inner_face: c_int,
    pub b_inner_corner_vert: c_int,
    pub a_inner_inset_face: c_int,
    pub a_inner_inset_corner_vert: c_int,
    pub b_inner_inset_face: c_int,
    pub b_inner_inset_corner_vert: c_int,
    pub a_inner_pre: [f32; 4],
    pub a_inner_post: [f32; 4],
    pub b_inner_pre: [f32; 4],
    pub b_inner_post: [f32; 4],
    pub a_inner_inset_pre: [f32; 4],
    pub a_inner_inset_post: [f32; 4],
    pub b_inner_inset_pre: [f32; 4],
    pub b_inner_inset_post: [f32; 4],
    pub comps: c_int,
}

/// Callee-allocated array of merge invocations. Zero-initialise (e.g. via
/// [`Default`]) before passing to [`bms_inset_region_merge_trace`]; the
/// callee fills `invocations` with `len` records and the buffer must be
/// released with [`bms_merge_trace_free`].
#[repr(C)]
#[derive(Debug)]
pub struct BmsMergeTrace {
    pub invocations: *mut BmsMergeInvocation,
    pub len: c_int,
    pub cap: c_int,
}

impl Default for BmsMergeTrace {
    fn default() -> Self {
        Self {
            invocations: std::ptr::null_mut(),
            len: 0,
            cap: 0,
        }
    }
}

unsafe extern "C" {
    /// Run region inset with interpolation enabled and record every
    /// per-corner customdata-merge invocation it performs. The mesh is
    /// mutated exactly as `inset_region` mutates it.
    ///
    /// # Safety
    /// `bm` must be a valid mesh and `out` a valid, zero-initialised
    /// [`BmsMergeTrace`]. `faces` must point to `faces_len` valid
    /// `*mut BMHeader` (faces) belonging to `bm`. `flags` is a bitmask
    /// (bit 0 `use_boundary`, bit 1 `use_even_offset`, bit 2
    /// `use_relative_offset`, bit 3 `use_edge_rail`, bit 4 `use_outset`);
    /// interpolation is always enabled. `layer_name` may be null; otherwise
    /// it names the loop layer to trace (first matching float2 / float3 /
    /// float / color layer). Returns `1` on success, `0` if the operator
    /// rejected the input, `-1` on a null `bm` / `out`. On success `out`
    /// owns a heap allocation that must be released with
    /// [`bms_merge_trace_free`].
    pub fn bms_inset_region_merge_trace(
        bm: *mut BMesh,
        faces: *mut *mut BMHeader,
        faces_len: c_int,
        thickness: f32,
        depth: f32,
        flags: c_int,
        layer_name: *const std::os::raw::c_char,
        out: *mut BmsMergeTrace,
    ) -> c_int;

    /// Release the callee allocation held by a [`BmsMergeTrace`] and reset
    /// it to empty.
    ///
    /// # Safety
    /// `out` must be a [`BmsMergeTrace`] previously filled by
    /// [`bms_inset_region_merge_trace`], or zero-initialised. Safe to call
    /// twice (idempotent).
    pub fn bms_merge_trace_free(out: *mut BmsMergeTrace);
}

// ---- Whole-mesh traversal micro-workloads ----
//
// Each function runs a complete read-only traversal natively inside the
// shim, so a single FFI crossing covers the whole walk and timing it
// measures BMesh's own iteration cost rather than per-element call
// overhead.

unsafe extern "C" {
    /// For every vertex, walk its disk cycle (the per-vertex cycle of
    /// incident edges) and count the edges visited; returns the total
    /// summed over all vertices (`2 * totedge`). The mesh is not modified.
    /// `bm` must be a valid mesh.
    pub fn bms_bench_disk_walk_sum(bm: *mut BMesh) -> u64;

    /// For every edge, walk its radial cycle (the per-edge cycle of
    /// incident face loops) and count the loops visited; returns the total
    /// summed over all edges (`totloop`). The mesh is not modified. `bm`
    /// must be a valid mesh.
    pub fn bms_bench_radial_walk_sum(bm: *mut BMesh) -> u64;

    /// Sum `co[0] + co[1] + co[2]` over every vertex, accumulated in
    /// `f64`. A whole-mesh read checksum whose result depends on every
    /// coordinate, making it a convenient optimisation barrier for timed
    /// reads. The mesh is not modified. `bm` must be a valid mesh.
    pub fn bms_bench_vert_position_sum(bm: *mut BMesh) -> f64;
}

// ---- Guarded-allocator bookkeeping ----
//
// BMesh routes its allocations through the vendored guarded allocator
// (MEM_* / BLI_mempool on top of MEM_*), so reading these counters before
// and after an operation observes its allocation delta. The counters are
// process-global and cover every MEM_* user in the binary; concurrent
// allocator activity on other threads shows up in the deltas.

unsafe extern "C" {
    /// Number of memory blocks currently live in the guarded allocator.
    pub fn bms_mem_blocks_in_use() -> c_uint;

    /// Total bytes currently live in the guarded allocator.
    pub fn bms_mem_in_use() -> usize;
}

pub mod owned;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn edge_verts_reads_endpoints() {
        unsafe {
            let bm = bms_mesh_create();
            assert!(!bm.is_null());

            let co_a = [1.0f32, 2.0, 3.0];
            let co_b = [4.0f32, 5.0, 6.0];
            let v1 = bms_vert_create(bm, co_a.as_ptr());
            let v2 = bms_vert_create(bm, co_b.as_ptr());
            assert!(!v1.is_null());
            assert!(!v2.is_null());

            let e = bms_edge_create(bm, v1, v2, false);
            assert!(!e.is_null());

            let mut out_v1: *mut BMVert = core::ptr::null_mut();
            let mut out_v2: *mut BMVert = core::ptr::null_mut();
            bms_edge_verts(e, &mut out_v1, &mut out_v2);

            assert_eq!(out_v1, v1);
            assert_eq!(out_v2, v2);

            let mut read_a = [0.0f32; 3];
            let mut read_b = [0.0f32; 3];
            bms_vert_co(out_v1, read_a.as_mut_ptr());
            bms_vert_co(out_v2, read_b.as_mut_ptr());
            assert_eq!(read_a, co_a);
            assert_eq!(read_b, co_b);

            bms_mesh_free(bm);
        }
    }
}
