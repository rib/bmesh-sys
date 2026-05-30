//! Low-level FFI bindings to Blender's BMesh.
//!
//! All symbols here are `extern "C"` wrappers exported from `shim/shim.cc`.
//! The wrappers are deliberately prefixed `bms_*` (BMesh-Shim) so the FFI
//! surface is decoupled from Blender's own (C++ name-mangled) symbol names.

#![allow(non_camel_case_types, non_snake_case)]

use std::os::raw::c_int;

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
    /// `use_dissolve_ortho_edges` slot. Refreshes face normals, then tags each
    /// input face with `BM_ELEM_TAG` and passes the faces as the operator's
    /// `geom` input (a face-set region extrude, where naming a face implies its
    /// closure of edges and verts). When `use_dissolve_ortho_edges` is true,
    /// side (wall) faces that end up lying in the plane of the extruded region
    /// are dissolved back into the surround, and the verts left as edge-pairs
    /// by those merges are collapsed. `use_keep_orig` and `use_normal_flip` are
    /// forwarded too. No originals are killed by this wrapper.
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
}

// ---- Delimit bits for `bms_dissolve_limit` ----
//
// Bitmask values matching BMesh's `BMO_Delimit` enum. Combine with `|`.
pub const BMS_DELIM_NORMAL: c_int = 1 << 0;
pub const BMS_DELIM_MATERIAL: c_int = 1 << 1;
pub const BMS_DELIM_SEAM: c_int = 1 << 2;
pub const BMS_DELIM_SHARP: c_int = 1 << 3;
pub const BMS_DELIM_UV: c_int = 1 << 4;

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
    /// recompute vertex normals (bmesh's full mesh-normals-update pulls in
    /// task-parallel machinery we don't stub); the comparison harness only
    /// inspects face normals.
    pub fn bms_mesh_normals_update(bm: *mut BMesh);
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

pub mod owned;
