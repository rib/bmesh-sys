/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * C-callable shim around Blender's BMesh API. Functions are prefixed bms_
 * (BMesh Shim) to decouple FFI consumers from Blender's name-mangled C++
 * symbols. All inputs and outputs use plain pointers / primitive types — no
 * C++ stdlib types cross the FFI boundary.
 */
#ifndef BMESH_SYS_SHIM_H
#define BMESH_SYS_SHIM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Opaque types — defined in the vendored bmesh headers. */
    typedef struct BMesh BMesh;
    typedef struct BMVert BMVert;
    typedef struct BMEdge BMEdge;
    typedef struct BMLoop BMLoop;
    typedef struct BMFace BMFace;
    /* Common header shared by every BM element; used as a type-erased
     * element-pointer in mixed vert/edge/face buffers. */
    typedef struct BMHeader BMHeader;

    /* Mesh lifecycle. */
    BMesh *bms_mesh_create(void);
    void bms_mesh_free(BMesh *bm);

    /* Element creation. */
    BMVert *bms_vert_create(BMesh *bm, const float co[3]);
    /* Like bms_vert_create, but copies customdata (not flags) from `example`. */
    BMVert *bms_vert_create_example(BMesh *bm, const float co[3], BMVert *example);
    BMEdge *bms_edge_create(BMesh *bm, BMVert *v1, BMVert *v2, bool no_double);
    BMFace *bms_face_create_verts(BMesh *bm, BMVert **verts, int len, bool no_double);

    /* Selection + read-only element field accessors. */
    void bms_vert_select_set(BMesh *bm, BMVert *v, bool select);
    void bms_vert_co(const BMVert *v, float out[3]);
    void bms_vert_no(const BMVert *v, float out[3]);
    int bms_elem_htype(const void *elem);
    int bms_elem_hflag(const void *elem);
    int bms_elem_api_flag(const void *elem);

    /* OR / AND-NOT / XOR a single hflag bit on `head.hflag`. `hflag_bit`
     * is an `BM_ELEM_*` mask (e.g. BM_ELEM_SELECT). Multiple bits OR'd
     * into a single mask are accepted; the operation applies to all set
     * bits in one shot. `elem` may be any element type (vert / edge /
     * loop / face) since BMHeader is uniformly the first field. */
    void bms_elem_set_hflag(void *elem, int hflag_bit);
    void bms_elem_clear_hflag(void *elem, int hflag_bit);
    void bms_elem_toggle_hflag(void *elem, int hflag_bit);

    /* Get / set a face's `mat_nr` (the material-slot index). */
    short bms_face_get_mat_nr(const BMFace *f);
    void bms_face_set_mat_nr(BMFace *f, short mat_nr);

    /* Element deletion. */
    void bms_vert_kill(BMesh *bm, BMVert *v);
    void bms_edge_kill(BMesh *bm, BMEdge *e);
    void bms_face_kill(BMesh *bm, BMFace *f);

    /* Splice / merge. */
    bool bms_vert_splice(BMesh *bm, BMVert *v_dst, BMVert *v_src);
    bool bms_edge_splice(BMesh *bm, BMEdge *e_dst, BMEdge *e_src);

    /* Kernel Euler operators. */
    BMFace *bms_sfme(BMesh *bm, BMFace *f, BMLoop *l_v1, BMLoop *l_v2);
    BMVert *bms_semv(BMesh *bm, BMVert *tv, BMEdge *e, BMEdge **r_e);
    BMEdge *bms_jekv(BMesh *bm, BMEdge *e_kill, BMVert *v_kill);
    BMVert *bms_jvke(BMesh *bm, BMEdge *e_kill, BMVert *v_kill);
    BMFace *bms_jfke(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e);
    void bms_loop_reverse(BMesh *bm, BMFace *f);
    BMEdge *bms_edge_separate(BMesh *bm, BMEdge *e, BMLoop *l_sep);

    /* Element counts. */
    int bms_totvert(BMesh *bm);
    int bms_totedge(BMesh *bm);
    int bms_totface(BMesh *bm);
    int bms_totloop(BMesh *bm);

    /* Number of looptri triples produced by `bms_mesh_calc_tessellation`
     * for the current mesh. Equals `totloop - 2 * totface` (the standard
     * polygon-to-triangle count). Returns 0 for an empty mesh. */
    int bms_mesh_calc_looptri_count(BMesh *bm);

    /* Compute the per-face render-time tessellation: for each face emit
     * `face.len - 2` triples of loops pointing into the face's existing
     * loop cycle. Writes exactly `bms_mesh_calc_looptri_count(bm)` triples
     * into `out_tris`, which the caller must pre-allocate. The mesh itself
     * is not modified. */
    void bms_mesh_calc_tessellation(BMesh *bm, BMLoop *(*out_tris)[3]);

    /* Destructively triangulate a single face in place. Wraps BMesh's
     * `BM_face_triangulate` along with the temporary MemArena (and, for
     * `MOD_TRIANGULATE_NGON_BEAUTY`, the BLI Heap) that it requires.
     *
     * `quad_method` is one of the BMS_TRIANGULATE_QUAD_* values (matches
     * `MOD_TRIANGULATE_QUAD_*`); `ngon_method` is one of the
     * BMS_TRIANGULATE_NGON_* values (matches `MOD_TRIANGULATE_NGON_*`).
     *
     * If `r_faces_new` is non-null, it must point to a caller-allocated
     * buffer sized for at least `f->len - 3` entries, and
     * `*r_faces_new_tot` must equal `f->len - 3` on entry. After the call,
     * `*r_faces_new_tot` holds the number of newly-created faces actually
     * written (may be less than `f->len - 3` if some target triangles
     * already existed in the mesh). Both pointers may be null when the
     * caller doesn't need the new-face list. */
    void bms_face_triangulate(BMesh *bm,
                              BMFace *f,
                              int quad_method,
                              int ngon_method,
                              bool use_tag,
                              BMFace **r_faces_new,
                              int *r_faces_new_tot);

    /* Snapshot the mesh into flat buffers. The caller pre-sizes the verts/edges/
     * face_offsets/face_lens buffers using the tot* counts (verts: tot*3 floats,
     * edges: tot*2 ints, face_offsets/face_lens: tot ints). face_verts_cap should
     * be the sum of all face lengths (caller can call bms_snapshot once with a
     * generous estimate; returns false if too small). */
    bool bms_snapshot(BMesh *bm,
                      float *out_verts, int out_verts_cap,
                      int *out_edges, int out_edges_cap,
                      int *out_face_offsets, int out_face_offsets_cap,
                      int *out_face_verts, int out_face_verts_cap,
                      int *out_face_lens, int out_face_lens_cap);

    /* ---- Customdata layer access. ---- */
    /* `domain` is one of: 0=vert, 1=edge, 2=loop, 3=face. */
    /* `type` is an eCustomDataType value (CD_PROP_FLOAT=10, CD_PROP_INT32=11, */
    /*  CD_PROP_FLOAT2=49, CD_PROP_FLOAT3=47, CD_PROP_COLOR=37 etc.). */
    /* Returns the layer's byte offset inside a per-element CD block, or -1   */
    /* on error. */
    int bms_layer_add_named(BMesh *bm, int domain, int type, const char *name);

    /* Extrude a region of faces. Marks each input face with BM_ELEM_TAG, then  */
    /* invokes BMesh's `extrude_face_region` operator with use_keep_orig=false.  */
    /* `use_normal_flip` is forwarded to the operator; when true it reverses the */
    /* winding of the side (wall) faces built between the original boundary and  */
    /* the lifted duplicate. Returns true on success, false if the operator     */
    /* rejected the input. */
    bool bms_extrude_face_region_ex(BMesh *bm, BMFace **faces, int faces_len,
                                    bool use_normal_flip);

    /* Convenience wrapper for bms_extrude_face_region_ex with                   */
    /* use_normal_flip=false. */
    bool bms_extrude_face_region(BMesh *bm, BMFace **faces, int faces_len);

    /* Extrude a region of faces, forwarding the operator's `edges_exclude` set. */
    /* Marks each input face with BM_ELEM_TAG and passes them as the operator's  */
    /* `geom` input, then populates the `edges_exclude` mapping slot with each   */
    /* edge in `edges_exclude[0..edges_exclude_len]`; excluded edges are not      */
    /* split off into the extruded region (their adjacent geometry stays joined  */
    /* to the original). `edges_exclude` may be null with edges_exclude_len == 0 */
    /* to request no exclusions. `use_keep_orig` and `use_normal_flip` are        */
    /* forwarded to the operator. Unlike bms_extrude_face_region_ex, the input    */
    /* faces are not killed after the op; deletion of selection-interior          */
    /* originals is left to the operator under use_keep_orig=false. Returns true  */
    /* on success, false if the operator rejected the input. */
    bool bms_extrude_face_region_exclude(BMesh *bm,
                                         BMFace **faces, int faces_len,
                                         BMEdge **edges_exclude, int edges_exclude_len,
                                         bool use_keep_orig,
                                         bool use_normal_flip);

    /* Extrude over the operator's native mixed `geom` element buffer. `geom`     */
    /* is a type-erased element buffer (%eb) of `geom_len` BMHeader* that may     */
    /* freely mix vert / edge / face pointers in one call; the operator routes    */
    /* each kind on its own (faces -> region extrude, edges -> edge-only walls,   */
    /* loose verts -> connecting wire edge). `edges_exclude` populates the        */
    /* operator's `edges_exclude` mapping slot; it may be null with               */
    /* edges_exclude_len == 0 for no exclusions. `use_keep_orig` and              */
    /* `use_normal_flip` are forwarded verbatim. No input elements are killed     */
    /* after the op; deletion of selection-interior originals is left to BMesh,   */
    /* so loose verts and wire edges (which have no interior) are preserved.      */
    /* Returns true on success, false if the operator rejected the input. */
    bool bms_extrude_face_region_geom(BMesh *bm,
                                      BMHeader **geom, int geom_len,
                                      BMEdge **edges_exclude, int edges_exclude_len,
                                      bool use_keep_orig,
                                      bool use_normal_flip);

    /* Extrude a region of faces, forwarding the operator's                      */
    /* `use_normal_from_adjacent` slot. Marks each input face with BM_ELEM_TAG    */
    /* and passes them as the operator's `geom` input. `use_keep_orig`,           */
    /* `use_normal_flip`, and `use_normal_from_adjacent` are forwarded to the     */
    /* operator. When `use_normal_from_adjacent` is true, the side (wall) faces   */
    /* take their orientation from geometry adjacent to the extruded region       */
    /* rather than from the region's own averaged normal. Unlike                  */
    /* bms_extrude_face_region_ex, the input faces are not killed after the op;   */
    /* the surrounding geometry is left intact. Returns true on success, false    */
    /* if the operator rejected the input. */
    bool bms_extrude_face_region_normal_from_adjacent(BMesh *bm,
                                                      BMFace **faces, int faces_len,
                                                      bool use_keep_orig,
                                                      bool use_normal_flip,
                                                      bool use_normal_from_adjacent);

    /* Extrude faces individually (discrete). Marks each input face with         */
    /* BM_ELEM_TAG, then invokes BMesh's `extrude_discrete_faces` operator. Each */
    /* input face is extruded on its own, so two formerly-adjacent input faces   */
    /* split apart along their shared edge rather than lifting as one connected  */
    /* region. The operator itself deletes the original faces (keeping their     */
    /* edges/verts as wall bottoms), so this shim performs no post-op kill.      */
    /* `use_normal_flip` is forwarded to the operator; when true it reverses the */
    /* winding of the side (wall) faces. Returns true on success, false if the   */
    /* operator rejected the input. */
    bool bms_extrude_discrete_faces_ex(BMesh *bm, BMFace **faces, int faces_len,
                                       bool use_normal_flip);

    /* Convenience wrapper for bms_extrude_discrete_faces_ex with                 */
    /* use_normal_flip=false. */
    bool bms_extrude_discrete_faces(BMesh *bm, BMFace **faces, int faces_len);

    /* Extrude edges only. Marks each input edge with BM_ELEM_TAG, then invokes  */
    /* BMesh's `extrude_edge_only` operator. Each input edge gains one wall quad  */
    /* spanning the original edge and its lifted duplicate; a contiguous strip   */
    /* of input edges produces a continuous ribbon sharing vertical edges        */
    /* between adjacent walls. The original edges/verts/faces are kept in place  */
    /* (they may still attach to non-input surrounding faces), so this shim      */
    /* performs no post-op kill. `use_normal_flip` is forwarded to the operator; */
    /* when true it reverses the winding of each wall quad. Returns true on      */
    /* success, false if the operator rejected the input. */
    bool bms_extrude_edge_only_ex(BMesh *bm, BMEdge **edges, int edges_len,
                                  bool use_normal_flip);

    /* Convenience wrapper for bms_extrude_edge_only_ex with                      */
    /* use_normal_flip=false. */
    bool bms_extrude_edge_only(BMesh *bm, BMEdge **edges, int edges_len);

    /* Extrude vertices individually. Marks each input vert with BM_ELEM_TAG,     */
    /* then invokes BMesh's `extrude_vert_indiv` operator. For each input vert a   */
    /* duplicate vert is created at the same position and a fresh wire edge        */
    /* connects the original to the duplicate. The operation is purely additive:   */
    /* the original verts are kept in place, so this shim performs no post-op      */
    /* kill. The caller is responsible for displacing the new verts after the      */
    /* call. Returns true on success, false if the operator rejected the input.   */
    bool bms_extrude_vert_indiv(BMesh *bm, BMVert **verts, int verts_len);

    /* Inset a region of faces. Marks each input face with BM_ELEM_TAG, then
     * invokes BMesh's `inset_region` operator. Every parameter the operator
     * exposes is forwarded explicitly so A/B tests can pin each parameter axis.
     *
     * `faces_exclude` is the same shape as `faces` and may be `nullptr` /
     * `faces_exclude_len == 0` if no exclusion list is needed. It is only
     * meaningful when `use_outset` is true; otherwise the operator ignores it.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_inset_region(BMesh *bm,
                          BMFace **faces, int faces_len,
                          BMFace **faces_exclude, int faces_exclude_len,
                          bool use_boundary,
                          bool use_even_offset,
                          bool use_interpolate,
                          bool use_relative_offset,
                          bool use_edge_rail,
                          bool use_outset,
                          float thickness,
                          float depth);

    /* Inset each input face individually (no shared boundary handling). Marks
     * each input face with BM_ELEM_TAG, then invokes BMesh's `inset_individual`
     * operator.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_inset_individual(BMesh *bm,
                              BMFace **faces, int faces_len,
                              bool use_even_offset,
                              bool use_interpolate,
                              bool use_relative_offset,
                              float thickness,
                              float depth);

    /* Invoke BMesh's `dissolve_verts` operator on the supplied vertex set.
     * Exposes both BMOP slot parameters explicitly:
     *
     *   - `use_face_split`     — split off face corners around dissolved verts
     *                            so surrounding geometry stays well-formed.
     *   - `use_boundary_tear`  — split off face corners on boundary verts
     *                            instead of merging adjacent faces.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_dissolve_verts(BMesh *bm,
                            BMVert **verts, int verts_len,
                            bool use_face_split,
                            bool use_boundary_tear);

    /* Invoke BMesh's `dissolve_edges` operator on the supplied edge set.
     * Exposes every BMOP slot parameter explicitly:
     *
     *   - `use_verts`         — also dissolve any vert left between exactly
     *                           two surviving edges.
     *   - `use_face_split`    — split off face corners to keep surrounding
     *                           geometry well-formed.
     *   - `angle_threshold`   — when `use_verts` is true, only dissolve a
     *                           vert if the angle between its remaining two
     *                           edges is at most this value (radians). Use
     *                           `M_PI` to disable the angle limit (matches
     *                           the BMOP's own default).
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_dissolve_edges(BMesh *bm,
                            BMEdge **edges, int edges_len,
                            bool use_verts,
                            bool use_face_split,
                            float angle_threshold);

    /* Invoke BMesh's `dissolve_faces` operator on the supplied face set.
     * Partitions the set into edge-adjacent connected components and merges
     * each component into a single face. Exposes both BMOP slot parameters
     * explicitly:
     *
     *   - `use_verts`         — after merging, also dissolve any vert left
     *                           between exactly two surviving edges.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_dissolve_faces(BMesh *bm,
                            BMFace **faces, int faces_len,
                            bool use_verts);

    /* Capturing variant of `bms_dissolve_faces`.
     *
     * Runs the same `dissolve_faces` BMOP but, in addition to performing
     * the surgery, copies the operator's `region.out` slot (the merged
     * faces produced by each successful per-region merge) into the
     * caller-supplied buffer `out_buf` of capacity `out_cap` face slots.
     *
     * Return value:
     *   -1  on operator init failure (matches the `false` return of the
     *       non-capturing variant).
     *   >= 0 on success: the *total* number of merged faces the slot
     *       produced. Up to `min(total, out_cap)` pointers are written
     *       to `out_buf` (in the slot's emit order). If the returned
     *       count exceeds `out_cap`, the buffer was undersized — the
     *       caller can detect overflow and re-call with a larger buffer
     *       on a fresh fixture.
     *
     * `out_buf` may be null only when `out_cap` is zero; in that case
     * the function still runs the operator and returns the merged-face
     * count for sizing purposes. */
    int bms_dissolve_faces_out(BMesh *bm,
                               BMFace **faces, int faces_len,
                               bool use_verts,
                               BMFace **out_buf, int out_cap);

    /* Invoke BMesh's general-purpose `delete` operator on a mixed element
     * buffer. `geom` is a type-erased buffer of `geom_len` BMHeader*
     * pointers (any mix of verts / edges / faces, since BMHeader is the
     * first field of every element). `context` selects which incident
     * geometry the operator removes; it is the operator's `context` enum
     * int, with these values:
     *
     *   1 = VERTS                — delete verts and all geometry using them.
     *   2 = EDGES                — delete edges and all faces using them.
     *   3 = FACES_ONLY           — delete only the faces themselves.
     *   4 = EDGES_FACES          — delete edges and their faces.
     *   5 = FACES                — delete faces and any verts/edges left
     *                              unused afterwards.
     *   6 = FACES_KEEP_BOUNDARY  — like FACES, but keep edges on the
     *                              boundary of the removed region.
     *   7 = TAGGED_ONLY          — delete only the tagged elements in the
     *                              buffer, leaving incident geometry.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_delete_geom(BMesh *bm, BMHeader **geom, int geom_len, int context);

    /* Invoke BMesh's `dissolve_limit` operator (a.k.a. "limited dissolve") on
     * the supplied edge + vert sets. Greedy heap-driven planar / co-linear
     * dissolve: every candidate whose dihedral angle stays within
     * `angle_limit` (radians) is dissolved, subject to the `delimit` bitmask
     * which prevents dissolving across selected boundaries.
     *
     * Parameters map 1:1 onto the operator's slots:
     *
     *   - `angle_limit`              — maximum dihedral angle (radians)
     *                                  between adjacent face normals for
     *                                  edge dissolve, and between the two
     *                                  remaining edges for vert dissolve.
     *                                  BMesh internally clamps this to
     *                                  `pi/2`; this shim forwards the
     *                                  caller's value verbatim.
     *   - `use_dissolve_boundaries`  — also dissolve verts that lie on a
     *                                  mesh boundary (i.e. between exactly
     *                                  two wire / boundary edges).
     *   - `delimit`                  — bitmask of edge attributes that
     *                                  block dissolve across them. Bits:
     *                                    BMS_DELIM_NORMAL   = 1 << 0
     *                                    BMS_DELIM_MATERIAL = 1 << 1
     *                                    BMS_DELIM_SEAM     = 1 << 2
     *                                    BMS_DELIM_SHARP    = 1 << 3
     *                                    BMS_DELIM_UV       = 1 << 4
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    enum
    {
        BMS_DELIM_NORMAL = 1 << 0,
        BMS_DELIM_MATERIAL = 1 << 1,
        BMS_DELIM_SEAM = 1 << 2,
        BMS_DELIM_SHARP = 1 << 3,
        BMS_DELIM_UV = 1 << 4,
    };
    bool bms_dissolve_limit(BMesh *bm,
                            BMEdge **edges, int edges_len,
                            BMVert **verts, int verts_len,
                            float angle_limit,
                            bool use_dissolve_boundaries,
                            int delimit);

    /* Capturing variant of `bms_dissolve_limit`.
     *
     * Runs the same `dissolve_limit` BMOP but, in addition to performing
     * the surgery, copies the operator's `region.out` slot (the merged
     * faces produced by each successful edge-pass merge, in heap-pop
     * order) into the caller-supplied buffer `out_buf` of capacity
     * `out_cap` face slots.
     *
     * Return value matches `bms_dissolve_faces_out` (see above): -1 on
     * init failure; otherwise the total slot count, with up to
     * `min(total, out_cap)` pointers written to `out_buf`. `out_buf`
     * may be null only when `out_cap` is zero. */
    int bms_dissolve_limit_out(BMesh *bm,
                               BMEdge **edges, int edges_len,
                               BMVert **verts, int verts_len,
                               float angle_limit,
                               bool use_dissolve_boundaries,
                               int delimit,
                               BMFace **out_buf, int out_cap);

    /* Invoke BMesh's `dissolve_degenerate` operator on the supplied edge set.
     * Numeric cleanup pass: collapses every input edge shorter than `dist`,
     * then clips degenerate "ear" loops on the still-present faces (loops
     * whose two incident edges are collinear within `dist`).
     *
     * Parameters map 1:1 onto the operator's slots:
     *
     *   - `dist`  — distance tolerance (world units). Edges with squared
     *               length below `dist * dist` are collapsed; loop ears
     *               whose collinearity error is at most `dist` are split
     *               and the introduced joining edge is also collapsed.
     *               At `dist = 0` only exact-zero-length edges and exactly
     *               collinear ears qualify; in practice a sanity-check pass.
     *
     * The `edges` buffer may be null with a zero length, in which case the
     * operator iterates the whole mesh but the input-flag check (which gates
     * the per-edge length test) is never satisfied — so passing a zero-length
     * input set is effectively a no-op even if the mesh contains degeneracies.
     * Callers wanting a whole-mesh cleanup should pass every edge.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_dissolve_degenerate(BMesh *bm,
                                 BMEdge **edges, int edges_len,
                                 float dist);

    /* Read / write typed values at a layer offset on any BM element. */
    void bms_elem_get_float(void *elem, int offset, float *out);
    void bms_elem_set_float(void *elem, int offset, float value);
    void bms_elem_get_float2(void *elem, int offset, float out[2]);
    void bms_elem_set_float2(void *elem, int offset, const float in[2]);
    void bms_elem_get_float3(void *elem, int offset, float out[3]);
    void bms_elem_set_float3(void *elem, int offset, const float in[3]);
    void bms_elem_get_float4(void *elem, int offset, float out[4]);
    void bms_elem_set_float4(void *elem, int offset, const float in[4]);
    void bms_elem_get_int(void *elem, int offset, int *out);
    void bms_elem_set_int(void *elem, int offset, int value);

    /* Iterate every element of `domain` in `BM_ITER_MESH` order and write
     * the layer's value at `offset` into a flat output buffer.
     *
     * `components` is 1/2/3/4 for the matching `bms_elem_get_floatN`; the
     * function writes `bm_tot<domain> * components` floats. For loops the
     * iteration order is faces in `BM_ITER_MESH(BM_FACES_OF_MESH)` order,
     * and within each face the face's existing loop cycle starting at
     * `BM_FACE_FIRST_LOOP`. This matches the `face_verts` order
     * `bms_snapshot` writes.
     *
     * `out_floats_cap` is the buffer's capacity in floats; the function
     * returns false if it is smaller than the required total, or if
     * `domain` is invalid.
     */
    bool bms_layer_read_floats(BMesh *bm,
                               int domain,
                               int offset,
                               int components,
                               float *out_floats,
                               int out_floats_cap);

    /* Same shape as `bms_layer_read_floats` for `CD_PROP_INT32` layers
     * (one int per element). Returns false on bad domain / buffer too
     * small. */
    bool bms_layer_read_ints(BMesh *bm,
                             int domain,
                             int offset,
                             int *out_ints,
                             int out_ints_cap);

    /* Element count for `domain` (0=vert, 1=edge, 2=loop, 3=face).
     * Returns -1 for an invalid domain. */
    int bms_domain_elem_count(BMesh *bm, int domain);

    /* Look up a CD layer's byte offset by `(domain, type, name)`. Returns
     * the offset if a matching layer exists, or -1 if no such layer is
     * registered (and `bms_layer_add_named` would have to be called to
     * register one). Read-only — never mutates the CustomData block. */
    int bms_layer_find_offset_named(BMesh *bm, int domain, int type, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* BMESH_SYS_SHIM_H */
