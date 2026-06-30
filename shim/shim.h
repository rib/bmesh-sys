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
#include <stddef.h>
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
    /* Read an edge's two endpoint verts (`e->v1` / `e->v2`) into the
     * caller-supplied out-pointers. */
    void bms_edge_verts(BMEdge *e, BMVert **out_v1, BMVert **out_v2);
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

    /* Recompute every vertex's stored normal (v->no) across the whole mesh.
     * Each vertex normal is accumulated serially from its incident faces, so
     * no task-scheduler symbols are pulled in. The incident face normals are
     * refreshed in the same pass, so the result is correct even if face
     * normals were stale on entry. */
    void bms_mesh_vert_normals_update(BMesh *bm);

    /* Copy each vertex's stored normal (v->no) into out_vert_normals as
     * 3 floats per vert, in vertex iteration order (the same order
     * bms_snapshot writes vertex positions). out_cap is the writable float
     * capacity and must be at least 3 * totvert. This does NOT recompute
     * normals -- run bms_mesh_vert_normals_update first. Returns the true
     * vertex count; when out_cap is too small nothing is written but the
     * true count is still returned so callers can detect truncation. */
    int bms_vert_normals_read(BMesh *bm, float *out_vert_normals, int out_cap);

    /* Copy each face's material-slot index (mat_nr) into out[], one short per
     * face, in face iteration order (the same order bms_snapshot writes face
     * data, so the i-th value aligns with the i-th snapshot face). Up to
     * out_cap values are written; the true face count is always returned, so
     * callers detect truncation when the count exceeds out_cap. out may be
     * null when out_cap is 0 to obtain just the count. */
    int bms_faces_read_mat_nr(BMesh *bm, short *out, int out_cap);

    /* Copy each edge's header flags (hflag) into out[], one int per edge, in
     * edge iteration order (the same order bms_snapshot writes edge data, so
     * the i-th value aligns with the i-th snapshot edge). The flags are a
     * bitfield (BM_ELEM_SELECT, BM_ELEM_SEAM, BM_ELEM_SMOOTH, etc.). Up to
     * out_cap values are written; the true edge count is always returned, so
     * callers detect truncation when the count exceeds out_cap. out may be
     * null when out_cap is 0 to obtain just the count. */
    int bms_edges_read_hflag(BMesh *bm, int *out, int out_cap);

    /* Copy each face's header flags (hflag) into out[], one int per face, in
     * face iteration order (the same order bms_snapshot writes face data, so
     * the i-th value aligns with the i-th snapshot face). The flags are a
     * bitfield (BM_ELEM_SELECT, BM_ELEM_SMOOTH, etc.). Up to out_cap values
     * are written; the true face count is always returned, so callers detect
     * truncation when the count exceeds out_cap. out may be null when out_cap
     * is 0 to obtain just the count. */
    int bms_faces_read_hflag(BMesh *bm, int *out, int out_cap);

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

    /* As `bms_face_triangulate`, but additionally surfaces the set of faces
     * that BMesh's radial-walk doubles-detection flags as coincident: a
     * would-be-new triangle whose three vertices already span an existing
     * triangle in the mesh. Internally a real linked list is always threaded
     * into `BM_face_triangulate`, so coincident-triangle input is handled
     * safely whether or not the caller wants the duplicate set.
     *
     * `r_faces_new` / `r_faces_new_tot` follow the same convention as
     * `bms_face_triangulate`.
     *
     * If `r_faces_double` is non-null it must point to a caller-allocated
     * buffer, and `*r_faces_double_tot` must hold that buffer's capacity (in
     * BMFace* entries) on entry. At most `f->len - 2` duplicates can be
     * reported, so a buffer of that size never truncates. On return,
     * `*r_faces_double_tot` holds the number of coincident faces detected
     * (which may exceed the capacity, in which case only the first
     * `capacity` entries were written). Each written entry is one of the
     * faces involved in a coincidence; its vertices match the pre-existing
     * face it coincided with. Both pointers may be null to discard the
     * duplicate set. */
    void bms_face_triangulate_with_doubles(BMesh *bm,
                                           BMFace *f,
                                           int quad_method,
                                           int ngon_method,
                                           bool use_tag,
                                           BMFace **r_faces_new,
                                           int *r_faces_new_tot,
                                           BMFace **r_faces_double,
                                           int *r_faces_double_tot);

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
    /*  CD_PROP_FLOAT2=49, CD_PROP_FLOAT3=48, CD_PROP_COLOR=47 etc.). */
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

    /* Capturing variant of bms_extrude_face_region_geom. Inputs match that      */
    /* function exactly; additionally reads back the operator's `geom.out`        */
    /* output slot before finishing. `geom.out` is a mixed element buffer of      */
    /* the verts, edges, and faces the extrude produced; each is returned         */
    /* type-erased as a BMHeader*. Up to `out_cap` pointers are copied into the   */
    /* caller-allocated `out_buf`. Returns the total `geom.out` element count     */
    /* (which may exceed `out_cap`), or -1 if the operator rejected the input.    */
    int bms_extrude_face_region_geom_out(BMesh *bm,
                                         BMHeader **geom, int geom_len,
                                         BMEdge **edges_exclude, int edges_exclude_len,
                                         bool use_keep_orig,
                                         bool use_normal_flip,
                                         BMHeader **out_buf, int out_cap);

    /* Solidify (offset) a marked face region. `geom` is a type-erased element    */
    /* buffer (%eb) of `geom_len` BMHeader* that may mix vert / edge / face       */
    /* pointers; the operator offsets the marked face region along smoothed       */
    /* per-vertex normals by `thickness` and stitches a rim of wall faces between */
    /* the original boundary and the offset duplicate. Forwards `geom` to the     */
    /* operator's `geom` input slot and `thickness` to its `thickness` float      */
    /* slot. Returns true on success, false if the operator rejected the input.   */
    bool bms_solidify(BMesh *bm, BMHeader **geom, int geom_len, float thickness);

    /* Capturing variant of bms_solidify. Inputs match that function exactly;     */
    /* additionally reads back the operator's `geom.out` output slot before       */
    /* finishing. `geom.out` is a mixed element buffer of the verts, edges, and   */
    /* faces the operation produced; each is returned type-erased as a BMHeader*. */
    /* Up to `out_cap` pointers are copied into the caller-allocated `out_buf`.   */
    /* Returns the total `geom.out` element count (which may exceed `out_cap`),   */
    /* or -1 if the operator rejected the input.                                  */
    int bms_solidify_out(BMesh *bm, BMHeader **geom, int geom_len, float thickness,
                         BMHeader **out_buf, int out_cap);

    /* Wireframe a marked face region. `faces` is a type-erased element buffer  */
    /* (%eb) of `faces_len` BMHeader* face pointers; the operator replaces each  */
    /* face with a frame of strut faces inset from its edges by `thickness`,     */
    /* shifted by `offset`. `use_replace` removes the original faces;            */
    /* `use_boundary` also wires open boundary edges; `use_even_offset` and      */
    /* `use_relative_offset` adjust how the inset distance scales; `use_crease`  */
    /* applies `crease_weight` to the new edges; `material_offset` shifts the    */
    /* material index of the generated faces. Returns true on success, false if  */
    /* the operator rejected the input.                                          */
    bool bms_wireframe(BMesh *bm,
                       BMHeader **faces, int faces_len,
                       float thickness, float offset,
                       bool use_replace, bool use_boundary,
                       bool use_even_offset, bool use_relative_offset,
                       bool use_crease, float crease_weight,
                       int material_offset);

    /* Capturing variant of bms_wireframe. Inputs match that function exactly;   */
    /* additionally reads back the operator's `faces.out` output slot before     */
    /* finishing. `faces.out` is the buffer of generated strut faces; each is    */
    /* returned type-erased as a BMHeader*. Up to `out_cap` pointers are copied  */
    /* into the caller-allocated `out_buf`. Returns the total `faces.out` count  */
    /* (which may exceed `out_cap`), or -1 if the operator rejected the input.   */
    int bms_wireframe_out(BMesh *bm,
                          BMHeader **faces, int faces_len,
                          float thickness, float offset,
                          bool use_replace, bool use_boundary,
                          bool use_even_offset, bool use_relative_offset,
                          bool use_crease, float crease_weight,
                          int material_offset,
                          BMHeader **out_buf, int out_cap);

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

    /* Extrude a region of faces, forwarding the operator's                       */
    /* `use_dissolve_ortho_edges` slot. Refreshes face normals, then marks the    */
    /* full closure of each input face -- the face and its edges and verts -- with*/
    /* BM_ELEM_TAG and passes them as the operator's `geom` input. The region's   */
    /* edges must be in `geom` for the dissolve pass to run: the operator only    */
    /* treats a region edge as a dissolve candidate once it has deleted the       */
    /* original it lifted off, and that deletion is gated on the region's edges   */
    /* being present in `geom`. When `use_dissolve_ortho_edges` is true, side     */
    /* (wall) faces that end up lying in the plane of the extruded region are     */
    /* dissolved back into the surround (decided from the refreshed face          */
    /* normals), and the verts left as edge-pairs by those merges are collapsed.  */
    /* `use_keep_orig` and `use_normal_flip` are forwarded too; note the dissolve */
    /* pass only has effect when originals are deleted, i.e. use_keep_orig=false. */
    /* Returns true on success, false if the operator rejected the input. */
    bool bms_extrude_face_region_dissolve_ortho(BMesh *bm,
                                                BMFace **faces, int faces_len,
                                                bool use_keep_orig,
                                                bool use_normal_flip,
                                                bool use_dissolve_ortho_edges);

    /* Extrude a region of faces, forwarding the operator's `skip_input_flip`     */
    /* slot alongside `use_keep_orig`. Marks each input face with BM_ELEM_TAG and */
    /* passes them as the operator's `geom` input. `skip_input_flip` only has an  */
    /* effect when `use_keep_orig` is true: the kept-original cleanup may reverse */
    /* the winding of the retained original face, and `skip_input_flip` suppresses*/
    /* that flip so the original keeps its incoming orientation. Both booleans    */
    /* are forwarded verbatim; `use_normal_flip` is left at its operator default. */
    /* No input faces are killed after the op. Returns true on success, false if  */
    /* the operator rejected the input. */
    bool bms_extrude_face_region_skip_input_flip(BMesh *bm,
                                                 BMFace **faces, int faces_len,
                                                 bool use_keep_orig,
                                                 bool skip_input_flip);

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

    /* Spin (rotate-extrude / lathe) a geometry selection around an axis.        */
    /* Invokes BMesh's `spin` operator, sweeping `geom` through `angle` radians   */
    /* in `steps` slices about `axis` passing through `cent`. Each step emits a   */
    /* band of geometry; `dvec` adds a per-step screw translation (pass null for  */
    /* a pure rotation). With `use_merge` and a full 360 degree sweep the first   */
    /* and last rings are welded. `use_normal_flip` reverses the generated face   */
    /* winding; `use_duplicate` copies the input geometry per step instead of     */
    /* extruding it. `cent`, `axis` and `dvec` are 3-float vectors. `space` is a  */
    /* 4x4 coordinate-frame matrix in which `cent`, `axis`, the rotation and      */
    /* `dvec` are interpreted; it is 16 contiguous floats in Blender's            */
    /* `float[4][4]` column-major order (`space[col * 4 + row]`). Pass a null     */
    /* `space` for world/identity space (matching a pure world-space spin). The   */
    /* operator's `geom_last.out` slot (the leading edge of the final step) is    */
    /* walked with a BM_ALL_NOLOOP iterator and written into `out_geom_last` up   */
    /* to `out_geom_last_cap` entries. Returns the total `geom_last.out` count    */
    /* (which may exceed the capacity), or -1 if the operator rejected the input. */
    int bms_spin(BMesh *bm,
                 BMHeader **geom, int geom_len,
                 const float *cent,
                 const float *axis,
                 const float *dvec,
                 float angle,
                 int steps,
                 bool use_merge,
                 bool use_normal_flip,
                 bool use_duplicate,
                 const float *space,
                 BMHeader **out_geom_last, int out_geom_last_cap);

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

    /* Inset a region of faces and capture the operator's `faces.out` slot.
     * Runs the same `inset_region` BMOP as `bms_inset_region` with the same
     * parameter list, but additionally copies the operator's `faces.out`
     * slot (the ring of wall faces created around the inset region) into the
     * caller-supplied buffer `out_buf` of capacity `out_cap` face slots.
     *
     * Return value:
     *   -1  on operator init failure (matches the `false` return of the
     *       non-capturing variant).
     *   >= 0 on success: the *total* number of faces the slot produced.
     *       Up to `min(total, out_cap)` pointers are written to `out_buf`
     *       (in the slot's emit order). If the returned count exceeds
     *       `out_cap`, the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero; in that case the
     * function still runs the operator and returns the face count for sizing
     * purposes. */
    int bms_inset_region_out(BMesh *bm,
                             BMFace **faces, int faces_len,
                             BMFace **faces_exclude, int faces_exclude_len,
                             bool use_boundary,
                             bool use_even_offset,
                             bool use_interpolate,
                             bool use_relative_offset,
                             bool use_edge_rail,
                             bool use_outset,
                             float thickness,
                             float depth,
                             BMFace **out_buf, int out_cap);

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

    /* Capturing variant of `bms_inset_individual`: runs the same operator with
     * the same parameter list, but additionally copies the operator's
     * `faces.out` slot (the newly-created inset wall faces) into the
     * caller-supplied buffer `out_buf` of capacity `out_cap` face slots.
     *
     * Return value:
     *   -1  on operator init failure (matches the `false` return of the
     *       non-capturing variant).
     *   >= 0 on success: the *total* number of faces the slot produced.
     *       Up to `min(total, out_cap)` pointers are written to `out_buf`
     *       (in the slot's emit order). If the returned count exceeds
     *       `out_cap`, the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero; in that case the
     * function still runs the operator and returns the face count for sizing
     * purposes. */
    int bms_inset_individual_out(BMesh *bm,
                                 BMFace **faces, int faces_len,
                                 bool use_even_offset,
                                 bool use_interpolate,
                                 bool use_relative_offset,
                                 float thickness,
                                 float depth,
                                 BMFace **out_buf, int out_cap);

    /* Bevel the supplied vertices / edges / faces via BMesh's `bevel`
     * operator. `geom` is a mixed element buffer of BMVert* / BMEdge* /
     * BMFace* (type-erased to BMHeader*) of length `geom_len`, filled into
     * the operator's `geom` input slot. The operator only bevels manifold
     * edges and the verts incident to them; non-manifold edges in `geom`
     * are ignored.
     *
     * Every operator slot is forwarded explicitly. The integer enum slots
     * take the operator's enum values (all 0-based):
     *
     *   - `offset_type`        — OFFSET=0, WIDTH=1, DEPTH=2, PERCENT=3,
     *                            ABSOLUTE=4
     *   - `profile_type`       — SUPERELLIPSE=0, CUSTOM=1
     *   - `affect`             — VERTICES=0, EDGES=1
     *   - `face_strength_mode` — NONE=0, NEW=1, AFFECTED=2, ALL=3
     *   - `miter_outer` /
     *     `miter_inner`        — SHARP=0, PATCH=1, ARC=2
     *   - `vmesh_method`       — ADJ=0, CUTOFF=1
     *
     * `material` is a material-slot index, or -1 to inherit from adjacent
     * faces. The operator is a no-op when `offset <= 0`.
     *
     * The custom-profile slot is always passed as null (the SUPERELLIPSE
     * profile is parameterised entirely by `profile`).
     *
     * Output geometry is left in place: the operator's verts/edges/faces
     * output slots carry no information not recoverable by re-reading the
     * mesh, so they are not surfaced. Face and vertex normals are refreshed
     * before the op runs, since the offset solver reads them.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_bevel(BMesh *bm,
                   BMHeader **geom, int geom_len,
                   float offset,
                   int offset_type,
                   int segments,
                   float profile,
                   int profile_type,
                   int affect,
                   bool clamp_overlap,
                   int material,
                   bool loop_slide,
                   bool mark_seam,
                   bool mark_sharp,
                   bool harden_normals,
                   int face_strength_mode,
                   int miter_outer,
                   int miter_inner,
                   float spread,
                   int vmesh_method);

    /* Bevel as `bms_bevel`, but with the per-element bevel-weight gate ON.
     * Calls BMesh's `BM_mesh_bevel` entry directly with `use_weights` enabled
     * (the `bevel` BMOP that `bms_bevel` drives always passes this flag off,
     * so it ignores the weight layers). When enabled, the named float layers
     * "bevel_weight_vert" (vertex domain) and "bevel_weight_edge" (edge
     * domain) locally scale the resolved offset, applied as a prescale before
     * placement: effective_offset = resolved_offset * weight.
     *
     * The caller is responsible for creating and populating those layers
     * before calling — e.g. via `bms_layer_add_named(bm, 0, CD_PROP_FLOAT,
     * "bevel_weight_vert")` / `(bm, 1, CD_PROP_FLOAT, "bevel_weight_edge")`
     * and `bms_elem_set_float`. A missing layer is treated as a zero weight
     * for that domain (the affected geometry collapses to no offset).
     *
     * Parameter surface, element-buffer semantics (manifold-edge gating),
     * normal refresh, and the `offset <= 0` no-op all match `bms_bevel`.
     * `clamp_overlap` maps to the solver's offset-limiting pass. Output
     * geometry is mutated in place; the result element slots are not surfaced.
     *
     * Returns true on success (including the `offset <= 0` no-op). */
    bool bms_bevel_weighted(BMesh *bm,
                            BMHeader **geom, int geom_len,
                            float offset,
                            int offset_type,
                            int segments,
                            float profile,
                            int profile_type,
                            int affect,
                            bool clamp_overlap,
                            int material,
                            bool loop_slide,
                            bool mark_seam,
                            bool mark_sharp,
                            bool harden_normals,
                            int face_strength_mode,
                            int miter_outer,
                            int miter_inner,
                            float spread,
                            int vmesh_method);

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

    /* Invoke BMesh's `unsubdivide` operator on the supplied vertex set.
     * Coarsens grid topology by collapsing it `iterations` times. Exposes
     * the BMOP slot parameter explicitly:
     *
     *   - `iterations` — number of unsubdivision passes to apply.
     *
     * The operator emits no output slot, so the shim only reports whether
     * the op ran. Returns true on success, false if BMO_op_initf rejected
     * the input. */
    bool bms_unsubdivide(BMesh *bm,
                         BMVert **verts, int verts_len,
                         int iterations);

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

    /* Invoke BMesh's `offset_edgeloops` operator on the supplied edge set,
     * inserting a parallel edge loop on each flanked side of the input
     * edges. Exposes the BMOP slot parameter explicitly:
     *
     *   - `use_cap_endpoint`  — extend the inserted loop around open
     *                           end-points of the selection.
     *
     * The operator's `edges.out` slot is not surfaced by this binding.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_offset_edgeloops(BMesh *bm,
                              BMEdge **edges, int edges_len,
                              bool use_cap_endpoint);

    /* As bms_offset_edgeloops, but surfaces the operator's `edges.out`
     * slot — the rail/cap edges the operator inserted — into the
     * caller-allocated `out_edges` buffer. Up to `min(slot_len, out_cap)`
     * pointers are written; the full slot count is returned so callers can
     * detect truncation.
     *
     * Returns -1 if BMO_op_initf rejected the input. */
    int bms_offset_edgeloops_out(BMesh *bm,
                                 BMEdge **edges, int edges_len,
                                 bool use_cap_endpoint,
                                 BMEdge **out_edges, int out_cap);

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

    /* Maps to BMesh's `triangle_fill` operator. Triangulates the planar
     * region bounded by the input `edges` loop, creating fill faces (and
     * the interior edges between them) but no vertices, and without
     * deleting any input edge. Exposes both BMOP bool slots explicitly:
     *
     *   - `use_beauty`   — run the beautify pass that rotates fill edges
     *                      toward a better (more equilateral) triangulation.
     *   - `use_dissolve` — join the fill's interior edges, merging the
     *                      result back into n-gons.
     *
     * `normal` is the operator's `normal` VEC slot: a pointer to 3 floats
     * giving the fill plane normal. A null pointer (or an all-zero vector)
     * selects the operator's default behaviour of deriving the normal from
     * the input edge loop.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_triangle_fill(BMesh *bm,
                           BMEdge **edges, int edges_len,
                           bool use_beauty,
                           bool use_dissolve,
                           const float *normal);

    /* Capturing variant of `bms_triangle_fill`.
     *
     * Runs the same `triangle_fill` BMOP but also copies the operator's
     * `geom.out` slot — the newly-created fill edges and faces — into the
     * caller-supplied buffer `out_buf` of capacity `out_cap` element slots.
     * Each written pointer is a `BMHeader *`; the caller distinguishes
     * edges from faces via the element's `htype`.
     *
     * Return value:
     *   -1   on operator init failure (matches the `false` return of the
     *        non-capturing variant).
     *   >= 0 on success: the *total* number of new elements the slot
     *        produced. Up to `min(total, out_cap)` pointers are written to
     *        `out_buf` (in the slot's emit order). If the returned count
     *        exceeds `out_cap`, the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero; in that case the
     * function still runs the operator and returns the new-element count
     * for sizing purposes. */
    int bms_triangle_fill_out(BMesh *bm,
                              BMEdge **edges, int edges_len,
                              bool use_beauty,
                              bool use_dissolve,
                              const float *normal,
                              BMHeader **out_buf, int out_cap);

    /* Maps to BMesh's `beautify_fill` operator. Rotates the interior
     * diagonals of an existing triangle patch toward a better-shaped
     * triangulation under a beauty metric. Creates and deletes no
     * geometry: it only changes which diagonal is present by flipping
     * rotatable edges. Exposes the BMOP's input slots explicitly:
     *
     *   - `faces`            — the triangle patch to operate on.
     *   - `edges`            — the set of interior edges eligible to rotate.
     *   - `use_restrict_tag` — restrict edge rotation to edges spanning
     *                          mixed (tagged/untagged) vertices.
     *   - `method`           — the beauty metric: 0 = AREA (area/perimeter),
     *                          1 = ANGLE (dihedral angle).
     *
     * `faces` points to an array of `faces_len` face pointers and `edges`
     * to an array of `edges_len` edge pointers, all belonging to `bm`.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_beautify_fill(BMesh *bm,
                           BMFace **faces, int faces_len,
                           BMEdge **edges, int edges_len,
                           bool use_restrict_tag, int method);

    /* Capturing variant of `bms_beautify_fill`.
     *
     * Runs the same `beautify_fill` BMOP but also copies the operator's
     * `geom.out` slot — the rotated edges and their flanking faces — into
     * the caller-supplied buffer `out_buf` of capacity `out_cap` element
     * slots. Each written pointer is a `BMHeader *`; the caller
     * distinguishes edges from faces via the element's `htype`.
     *
     * Return value:
     *   -1   on operator init failure (matches the `false` return of the
     *        non-capturing variant).
     *   >= 0 on success: the *total* number of elements the slot produced.
     *        Up to `min(total, out_cap)` pointers are written to `out_buf`
     *        (in the slot's emit order). If the returned count exceeds
     *        `out_cap`, the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero; in that case the
     * function still runs the operator and returns the element count for
     * sizing purposes. */
    int bms_beautify_fill_out(BMesh *bm,
                              BMFace **faces, int faces_len,
                              BMEdge **edges, int edges_len,
                              bool use_restrict_tag, int method,
                              BMHeader **out_buf, int out_cap);

    /* Maps to BMesh's `edgeloop_fill` operator. Caps each closed loop in
     * the supplied `edges` set with a single n-gon face — no triangulation,
     * no new vertices, no new edges. The operator collects the loop's
     * vertices, walks the cycle to order them, and creates one face per
     * loop spanning exactly those vertices (skipping any loop whose face
     * already exists). Multiple disjoint closed loops in one call each
     * receive their own n-gon. Exposes both BMOP slots explicitly:
     *
     *   - `mat_nr`     — material index assigned to each created face.
     *   - `use_smooth` — set the smooth-shading flag on each created face.
     *
     * The input is rejected (no face created) unless every input edge's two
     * endpoints each carry exactly two input edges — i.e. the input is one
     * or more simple closed loops with no branch, no dangling chain, and as
     * many distinct vertices as edges.
     *
     * The operator's `faces.out` slot is not surfaced by this binding.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_edgeloop_fill(BMesh *bm,
                           BMEdge **edges, int edges_len,
                           int mat_nr,
                           bool use_smooth);

    /* Capturing variant of `bms_edgeloop_fill`.
     *
     * Runs the same `edgeloop_fill` BMOP but also copies the operator's
     * `faces.out` slot — the n-gon face(s) the fill created — into the
     * caller-supplied buffer `out_buf` of capacity `out_cap` face slots.
     *
     * Return value:
     *   -1   on operator init failure (matches the `false` return of the
     *        non-capturing variant).
     *   >= 0 on success: the *total* number of faces the slot produced. Up
     *        to `min(total, out_cap)` pointers are written to `out_buf` in
     *        the slot's emit order; if the returned count exceeds `out_cap`,
     *        the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero (size-probing mode). */
    int bms_edgeloop_fill_out(BMesh *bm,
                              BMEdge **edges, int edges_len,
                              int mat_nr,
                              bool use_smooth,
                              BMFace **out_buf, int out_cap);

    /* Maps to BMesh's `holes_fill` operator. Detects closed boundary loops
     * spanned by the supplied `edges` set and caps each with a single n-gon
     * face. Holes whose perimeter exceeds `sides` edges are skipped; pass a
     * large `sides` to fill every detected hole regardless of size. Both
     * BMOP slots are forwarded explicitly:
     *
     *   - `edges` — the candidate boundary edges to consider.
     *   - `sides` — maximum hole perimeter (in edges) eligible for filling.
     *
     * The operator's `faces.out` slot is not surfaced by this binding.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_holes_fill(BMesh *bm,
                        BMEdge **edges, int edges_len,
                        int sides);

    /* Capturing variant of `bms_holes_fill`.
     *
     * Runs the same `holes_fill` BMOP but also copies the operator's
     * `faces.out` slot — the n-gon face(s) the fill created — into the
     * caller-supplied buffer `out_buf` of capacity `out_cap` face slots.
     *
     * Return value:
     *   -1   on operator init failure (matches the `false` return of the
     *        non-capturing variant).
     *   >= 0 on success: the *total* number of faces the slot produced. Up
     *        to `min(total, out_cap)` pointers are written to `out_buf` in
     *        the slot's emit order; if the returned count exceeds `out_cap`,
     *        the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero (size-probing mode). */
    int bms_holes_fill_out(BMesh *bm,
                           BMEdge **edges, int edges_len,
                           int sides,
                           BMFace **out_buf, int out_cap);

    /* Maps to BMesh's `face_attribute_fill` operator. Treats the supplied
     * `faces` set as destination faces that should inherit their attributes
     * (and, optionally, winding) from their adjacent *unselected* faces. The
     * fill is a breadth-first flood that starts at the destination faces
     * bordering an unselected face and propagates inward, so an interior
     * destination face inherits transitively once one of its neighbours has
     * been resolved. Both BMOP slots are forwarded explicitly:
     *
     *   - `use_normals` — when true, flip a destination face whose winding is
     *     incoherent with the unselected neighbour it inherits from.
     *   - `use_data` — when true, copy the neighbour's face-domain custom-data
     *     (including the material index) and interpolate its per-corner loop
     *     data across the shared edge.
     *
     * The operator's `faces_fail.out` slot is not surfaced by this binding.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_face_attribute_fill(BMesh *bm,
                                 BMFace **faces, int faces_len,
                                 bool use_normals, bool use_data);

    /* Capturing variant of `bms_face_attribute_fill`.
     *
     * Runs the same `face_attribute_fill` BMOP but also copies the operator's
     * `faces_fail.out` slot — the destination faces that could *not* be
     * resolved because no unselected face was ever reachable from them (e.g.
     * an entire connected component was selected) — into the caller-supplied
     * buffer `out_buf` of capacity `out_cap` face slots.
     *
     * Return value:
     *   -1   on operator init failure (matches the `false` return of the
     *        non-capturing variant).
     *   >= 0 on success: the *total* number of unresolved faces the slot
     *        produced. Up to `min(total, out_cap)` pointers are written to
     *        `out_buf`; if the returned count exceeds `out_cap`, the buffer
     *        was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero (size-probing mode). */
    int bms_face_attribute_fill_out(BMesh *bm,
                                    BMFace **faces, int faces_len,
                                    bool use_normals, bool use_data,
                                    BMFace **out_buf, int out_cap);

    /* Maps to BMesh's `grid_fill` operator. Fills the rectangular region
     * delimited by two opposing open edge loops with a regular grid of
     * quad faces. The supplied `edges` set must resolve to exactly two
     * open (non-closed) edge loops — the two opposing sides of the grid.
     * The operator discovers the two connecting "rail" sides automatically
     * by walking wire-or-boundary edges between the loop endpoints, so those
     * rails must already exist in the mesh. When the two opposing sides (or
     * the two rails) differ in vertex count, the shorter side is temporarily
     * subdivided so the grid is rectangular, and those splits are collapsed
     * back out afterwards. Interior grid vertices are created by
     * interpolating the four boundary sides; the perimeter vertices are
     * reused. Exposes every BMOP slot explicitly:
     *
     *   - `mat_nr`            — material index assigned to each created quad.
     *   - `use_smooth`        — set the smooth-shading flag on each quad.
     *   - `use_interp_simple` — place interior verts by simple bilinear
     *                           boundary weighting instead of the default
     *                           transform-based interpolation.
     *
     * The input is rejected (no face created) when it does not resolve to
     * exactly two open loops, when either resolved loop is closed, when no
     * wire/boundary rail connects the loops, or when the discovered rails
     * overlap.
     *
     * The operator's `faces.out` slot is not surfaced by this binding.
     *
     * Returns true on success, false if BMO_op_initf rejected the input or
     * the operator cancelled (e.g. the input did not resolve to two open
     * loops). */
    bool bms_grid_fill(BMesh *bm,
                       BMEdge **edges, int edges_len,
                       int mat_nr,
                       bool use_smooth,
                       bool use_interp_simple);

    /* Capturing variant of `bms_grid_fill`.
     *
     * Runs the same `grid_fill` BMOP but also copies the operator's
     * `faces.out` slot — the grid of quad faces the fill created — into the
     * caller-supplied buffer `out_buf` of capacity `out_cap` face slots.
     *
     * Return value:
     *   -1   on operator init failure (matches the `false` return of the
     *        non-capturing variant).
     *   >= 0 on success: the *total* number of faces the slot produced. Up
     *        to `min(total, out_cap)` pointers are written to `out_buf` in
     *        the slot's emit order; if the returned count exceeds `out_cap`,
     *        the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero (size-probing mode). */
    int bms_grid_fill_out(BMesh *bm,
                          BMEdge **edges, int edges_len,
                          int mat_nr,
                          bool use_smooth,
                          bool use_interp_simple,
                          BMFace **out_buf, int out_cap);

    /* Maps to BMesh's `create_grid` operator. Builds a planar
     * (`x_segments` × `y_segments`) grid of quad faces in the local z=0
     * plane, scaled by `size` and transformed by `matrix` (a column-major
     * 4x4 matrix given as 16 floats), then appends the new geometry to
     * `bm`. When `calc_uvs` is set, the faces receive default unit-square
     * UVs on the active UV layer. The operator's `verts.out` slot is not
     * surfaced by this binding. */
    void bms_create_grid(BMesh *bm,
                         int x_segments,
                         int y_segments,
                         float size,
                         const float matrix[16],
                         bool calc_uvs);

    /* Capturing variant of `bms_create_grid`. Runs the same `create_grid`
     * operator and additionally surfaces its `verts.out` element-buffer slot —
     * the vertices the operator created, in slot (operator output) order.
     *
     * Up to `min(total, out_cap)` `BMVert*` handles are written to `out_buf`
     * in slot order; the return value is the *total* created-vertex count.
     * If the returned count exceeds `out_cap`, the buffer was undersized and
     * only the first `out_cap` handles were written. Returns -1 if BMO_op_initf
     * rejected the input. `out_buf` may be null only when `out_cap` is zero
     * (size-probing mode). Vertex positions are read per handle via
     * `bms_vert_co`. */
    int bms_create_grid_out(BMesh *bm,
                            int x_segments,
                            int y_segments,
                            float size,
                            const float matrix[16],
                            bool calc_uvs,
                            BMVert **out_buf, int out_cap);

    /* Maps to BMesh's `create_cube` operator. Builds a unit box of six
     * quad faces, scaled by `size` and transformed by `matrix` (a
     * column-major 4x4 matrix given as 16 floats), then appends the new
     * geometry to `bm`. When `calc_uvs` is set, the faces receive default
     * UVs on the active UV layer. The operator's `verts.out` slot is not
     * surfaced by this binding. */
    void bms_create_cube(BMesh *bm,
                         float size,
                         const float matrix[16],
                         bool calc_uvs);

    /* Capturing variant of `bms_create_cube`. Runs the same `create_cube`
     * operator and additionally surfaces its `verts.out` element-buffer slot —
     * the vertices the operator created, in slot (operator output) order.
     *
     * Up to `min(total, out_cap)` `BMVert*` handles are written to `out_buf`
     * in slot order; the return value is the *total* created-vertex count.
     * If the returned count exceeds `out_cap`, the buffer was undersized and
     * only the first `out_cap` handles were written. Returns -1 if BMO_op_initf
     * rejected the input. `out_buf` may be null only when `out_cap` is zero
     * (size-probing mode). Vertex positions are read per handle via
     * `bms_vert_co`. */
    int bms_create_cube_out(BMesh *bm,
                            float size,
                            const float matrix[16],
                            bool calc_uvs,
                            BMVert **out_buf, int out_cap);

    /* Maps to BMesh's `create_circle` operator. Builds a circle of
     * `segments` vertices of the given `radius` in the local z=0 plane,
     * transformed by `matrix` (a column-major 4x4 matrix given as 16
     * floats), then appends the new geometry to `bm`. When `cap_ends` is
     * set the circle is filled with a face; `cap_tris` then chooses a
     * triangle fan over a single n-gon. When `calc_uvs` is set, the faces
     * receive default UVs on the active UV layer. The operator's
     * `verts.out` slot is not surfaced by this binding. */
    void bms_create_circle(BMesh *bm,
                           bool cap_ends,
                           bool cap_tris,
                           int segments,
                           float radius,
                           const float matrix[16],
                           bool calc_uvs);

    /* Capturing variant of `bms_create_circle`. Runs the same `create_circle`
     * operator and additionally surfaces its `verts.out` element-buffer slot —
     * the vertices the operator created, in slot (operator output) order.
     *
     * Up to `min(total, out_cap)` `BMVert*` handles are written to `out_buf`
     * in slot order; the return value is the *total* created-vertex count.
     * If the returned count exceeds `out_cap`, the buffer was undersized and
     * only the first `out_cap` handles were written. Returns -1 if BMO_op_initf
     * rejected the input. `out_buf` may be null only when `out_cap` is zero
     * (size-probing mode). Vertex positions are read per handle via
     * `bms_vert_co`. */
    int bms_create_circle_out(BMesh *bm,
                              bool cap_ends,
                              bool cap_tris,
                              int segments,
                              float radius,
                              const float matrix[16],
                              bool calc_uvs,
                              BMVert **out_buf, int out_cap);

    /* Maps to BMesh's `create_cone` operator. Builds a cone, cylinder, or
     * truncated cone of `segments` vertices per ring: a bottom ring of
     * radius `radius1` at local z = -depth/2 and a top ring of radius
     * `radius2` at local z = +depth/2, joined by a wall of side faces, all
     * transformed by `matrix` (a column-major 4x4 matrix given as 16
     * floats), then appended to `bm`. When `cap_ends` is set each open ring
     * is filled with a cap; `cap_tris` then chooses triangle fans over
     * single n-gon caps. A ring whose radius is 0 collapses to a single
     * apex vertex (a true cone), with the side faces becoming triangles.
     * When `calc_uvs` is set the new faces receive default UVs on the
     * active UV layer. The operator's `verts.out` slot is not surfaced by
     * this binding. */
    void bms_create_cone(BMesh *bm,
                         bool cap_ends,
                         bool cap_tris,
                         int segments,
                         float radius1,
                         float radius2,
                         float depth,
                         const float matrix[16],
                         bool calc_uvs);

    /* Capturing variant of `bms_create_cone`. Runs the same `create_cone`
     * operator and additionally surfaces its `verts.out` element-buffer slot —
     * the vertices the operator created, in slot (operator output) order.
     *
     * Up to `min(total, out_cap)` `BMVert*` handles are written to `out_buf`
     * in slot order; the return value is the *total* created-vertex count.
     * If the returned count exceeds `out_cap`, the buffer was undersized and
     * only the first `out_cap` handles were written. Returns -1 if BMO_op_initf
     * rejected the input. `out_buf` may be null only when `out_cap` is zero
     * (size-probing mode). Vertex positions are read per handle via
     * `bms_vert_co`. */
    int bms_create_cone_out(BMesh *bm,
                            bool cap_ends,
                            bool cap_tris,
                            int segments,
                            float radius1,
                            float radius2,
                            float depth,
                            const float matrix[16],
                            bool calc_uvs,
                            BMVert **out_buf, int out_cap);

    /* Maps to BMesh's `create_uvsphere` operator. Builds a
     * longitude/latitude sphere of `u_segments` meridian columns and
     * `v_segments` latitude bands at the given `radius`: a stack of
     * constant-latitude rings joined by quad bands, closed off by a
     * triangle fan at each pole. Both segment counts are clamped to a
     * minimum of 1 (a count of 0 builds a degenerate shell, not nothing).
     * Every generated vertex is transformed by `matrix` (a column-major
     * 4x4 matrix given as 16 floats) and the geometry is appended to `bm`.
     * When `calc_uvs` is set the new faces receive a default equirectangular
     * UV unwrap on the active UV layer. The operator's `verts.out` slot is
     * not surfaced by this binding. */
    void bms_create_uvsphere(BMesh *bm,
                             int u_segments,
                             int v_segments,
                             float radius,
                             const float matrix[16],
                             bool calc_uvs);

    /* Maps to BMesh's `create_icosphere` operator. Builds a geodesic
     * sphere by recursively subdividing a regular icosahedron. The base
     * icosahedron (12 vertices, 20 triangular faces) is laid down first,
     * then for `subdivisions` greater than 1 every edge is split into
     * 2^(subdivisions-1) segments with spherical projection, so each base
     * triangle becomes a fan of 4^(subdivisions-1) triangles whose new
     * vertices land on the sphere of the given `radius`. A `subdivisions`
     * of 1 (or less) leaves the bare icosahedron. Every generated vertex
     * is transformed by `matrix` (a column-major 4x4 matrix given as 16
     * floats) and the geometry is appended to `bm`. When `calc_uvs` is set
     * the new faces receive a default UV layout on the active UV layer.
     * The operator's `verts.out` slot is not surfaced by this binding. */
    void bms_create_icosphere(BMesh *bm,
                              int subdivisions,
                              float radius,
                              const float matrix[16],
                              bool calc_uvs);

    /* Maps to BMesh's `reverse_uvs` operator. Reverses the active UV
     * layer's per-loop float2 values around each input face — a pure
     * loop-customdata permutation with no topology change.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_reverse_uvs(BMesh *bm, BMFace **faces, int faces_len);

    /* Maps to BMesh's `collapse_uvs` operator. Averages and collapses the
     * per-loop values of loop-customdata layers that support interpolation
     * (UVs and similar) across each input edge, merging the values of the
     * loops on either side. Operates on an edge buffer, not a face buffer.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_collapse_uvs(BMesh *bm, BMEdge **edges, int edges_len);

    /* Maps to BMesh's `average_vert_facedata` operator. Averages the
     * per-loop values of interpolatable loop-customdata layers across the
     * loops of each input vertex and assigns the averaged result back to
     * those loops. Operates on a vertex buffer.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_average_vert_facedata(BMesh *bm, BMVert **verts, int verts_len);

    /* Maps to BMesh's `pointmerge_facedata` operator. Snaps the per-loop
     * values of interpolatable loop-customdata layers across the loops of
     * the input vertices to those of a single snap vertex. Operates on a
     * vertex buffer plus one snap vertex.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_pointmerge_facedata(BMesh *bm, BMVert **verts, int verts_len, BMVert *vert_snap);

    /* Maps to BMesh's `reverse_colors` operator. Reverses the per-loop values
     * of the color layer selected by `color_index` around each input face — a
     * pure loop-customdata permutation with no topology change. `color_index`
     * picks the color layer (0 selects the active/first layer).
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_reverse_colors(BMesh *bm, BMFace **faces, int faces_len, int color_index);

    /* Maps to BMesh's `rotate_uvs` operator. Cycles the active UV layer's
     * per-loop float2 values forward by one corner around each input face
     * — a pure loop-customdata permutation with no topology change.
     * `use_ccw` selects the rotation direction (counter-clockwise when
     * true, clockwise when false).
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_rotate_uvs(BMesh *bm, BMFace **faces, int faces_len, bool use_ccw);

    /* Maps to BMesh's `rotate_colors` operator. Cycles the per-loop values of
     * the color layer selected by `color_index` forward by one corner around
     * each input face — a pure loop-customdata permutation with no topology
     * change. `use_ccw` selects the rotation direction (counter-clockwise when
     * true, clockwise when false). `color_index` picks the color layer (0
     * selects the active/first layer).
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_rotate_colors(BMesh *bm, BMFace **faces, int faces_len, bool use_ccw, int color_index);

    /* Invoke BMesh's `recalc_face_normals` operator on the supplied face set
     * (a.k.a. "Recalculate Outside"). For each face it recomputes the cached
     * normal from the corner positions, then propagates a consistent winding
     * across each manifold-connected component so the component faces outward.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_recalc_face_normals(BMesh *bm,
                                 BMFace **faces, int faces_len);

    /* "Recalculate Inside" companion of `bms_recalc_face_normals`.
     *
     * Runs the same `recalc_face_normals` BMOP to make each manifold-connected
     * component consistently wound, then reverses the winding of every named
     * face so the component points inward instead of outward.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_recalc_face_normals_inside(BMesh *bm,
                                        BMFace **faces, int faces_len);

    /* Invoke BMesh's `split_edges` operator on the supplied edge set.
     *
     * Peels the selected edges apart so that adjacent faces no longer
     * share them, opening the mesh along those edges (the edge-separate /
     * seam-opening operation). This is distinct from the `edge_split`
     * subdivision kernel exposed by `bms_edge_split`: nothing is
     * subdivided here, the existing topology is disconnected.
     *
     * `verts` / `verts_len` supply optional vertices that constrain where
     * the split propagates; they are only consulted when `use_verts` is
     * true. `verts` may be null with `verts_len` 0 (an empty set is passed
     * to the operator). When `use_verts` is false the operator derives the
     * split vertices from `edges` alone and the `verts` slot is ignored.
     *
     * In addition to performing the surgery, copies the operator's
     * `edges.out` slot (the original edges that were disconnected) into
     * the caller-supplied buffer `out_edges` of capacity `out_cap` edge
     * slots.
     *
     * Return value:
     *   -1  on operator init failure (BMO_op_initf rejected the format).
     *   >= 0 on success: the *total* number of edges the slot produced.
     *       Up to `min(total, out_cap)` pointers are written to
     *       `out_edges` (in the slot's emit order). If the returned count
     *       exceeds `out_cap`, the buffer was undersized.
     *
     * `out_edges` may be null only when `out_cap` is zero; in that case
     * the function still runs the operator and returns the produced count
     * for sizing purposes. */
    int bms_split_edges(BMesh *bm,
                        BMEdge **edges, int edges_len,
                        BMVert **verts, int verts_len,
                        bool use_verts,
                        BMEdge **out_edges, int out_cap);

    /* Invoke BMesh's `join_triangles` operator on the supplied face set.
     * Merges adjacent triangle pairs into quads, subject to the delimit
     * and angle gates below. Every BMOP slot is exposed explicitly:
     *
     *   - `cmp_seam` / `cmp_sharp` / `cmp_uvs` / `cmp_vcols` /
     *     `cmp_materials` — block a merge when the shared edge / its loops
     *     differ across the named attribute.
     *   - `angle_face_threshold`  — max fold angle (radians) between the
     *     two triangle normals; a value >= pi disables this gate.
     *   - `angle_shape_threshold` — max deviation (radians) of the
     *     resulting quad from an ideal shape; a value >= pi disables it.
     *   - `topology_influence`    — 0..2 weighting that biases the merge
     *     order toward regular topology.
     *   - `deselect_joined`       — clear the select flag on faces that
     *     were merged.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_join_triangles(BMesh *bm,
                            BMFace **faces, int faces_len,
                            bool cmp_seam, bool cmp_sharp, bool cmp_uvs,
                            bool cmp_vcols, bool cmp_materials,
                            float angle_face_threshold,
                            float angle_shape_threshold,
                            float topology_influence,
                            bool deselect_joined);

    /* Capturing variant of `bms_join_triangles`.
     *
     * Runs the same `join_triangles` BMOP but, in addition to performing
     * the surgery, copies the operator's `faces.out` slot (the merged
     * quads together with the triangles that were left un-merged) into
     * the caller-supplied buffer `out_buf` of capacity `out_cap` face
     * slots.
     *
     * Return value:
     *   -1  on operator init failure (matches the `false` return of the
     *       non-capturing variant).
     *   >= 0 on success: the *total* number of faces the slot produced.
     *       Up to `min(total, out_cap)` pointers are written to `out_buf`
     *       (in the slot's emit order). If the returned count exceeds
     *       `out_cap`, the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero; in that case
     * the function still runs the operator and returns the face count
     * for sizing purposes. */
    int bms_join_triangles_out(BMesh *bm,
                               BMFace **faces, int faces_len,
                               bool cmp_seam, bool cmp_sharp, bool cmp_uvs,
                               bool cmp_vcols, bool cmp_materials,
                               float angle_face_threshold,
                               float angle_shape_threshold,
                               float topology_influence,
                               bool deselect_joined,
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

    /* Invoke BMesh's `weld_verts` operator, welding each source vert onto a
     * target vert.
     *
     * `pairs` is a flat array of `2 * pairs_len` BMVert* laid out as
     * consecutive (src, tar) couples: `pairs[2*i]` is the source vert welded
     * onto target `pairs[2*i+1]`. Each couple is inserted into the operator's
     * `targetmap` mapping slot (source vert as map key, target vert as mapped
     * value). `pairs` may be null with `pairs_len == 0` for an empty input.
     *
     * `use_centroid` sets the operator's `use_centroid` bool slot: when true
     * each merged group settles at the centroid of its members, otherwise the
     * group adopts the target vert's position.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_weld_verts(BMesh *bm, BMVert **pairs, int pairs_len, bool use_centroid);

    /* Invoke BMesh's `find_doubles` operator, which detects groups of
     * coincident verts within `dist` and produces a vert -> vert merge map
     * *without* modifying topology.
     *
     * Inputs map 1:1 onto the operator's slots:
     *   - `verts` / `verts_len`           — the BMVert* set to search. May
     *                                       be null with `verts_len == 0`.
     *   - `keep_verts` / `keep_len`       — verts that must never be merged
     *                                       away; verts outside this set can
     *                                       only merge onto a vert inside it.
     *                                       May be null with `keep_len == 0`.
     *   - `dist`                          — maximum merge distance.
     *   - `use_connected`                 — restrict pairing to verts joined
     *                                       by existing topology.
     *
     * On success the operator's `targetmap.out` MAP_ELEM slot holds one entry
     * per merged source vert (key = source BMVert*, value = target BMVert*).
     * Each entry is written to `out_pairs` as a flat (src, tar) couple:
     * `out_pairs[2*i]` is the source vert, `out_pairs[2*i+1]` its target.
     * At most `out_cap` couples are written.
     *
     * Returns the total number of map entries, which may exceed `out_cap`
     * (only the first `out_cap` couples are written in that case, so callers
     * can detect truncation), or -1 if BMO_op_initf rejected the input. */
    int bms_find_doubles(BMesh *bm,
                         BMVert **verts, int verts_len,
                         BMVert **keep_verts, int keep_len,
                         float dist, bool use_connected,
                         BMVert **out_pairs, int out_cap);

    /* Invoke BMesh's `remove_doubles` operator, which detects groups of
     * coincident verts within `dist` (reusing the same clustering as
     * `find_doubles`) and then welds each group in place with
     * `use_centroid = false`, collapsing the topology. The mesh is mutated
     * directly; there is no output slot to read back, so callers inspect the
     * mesh afterwards.
     *
     * Inputs map onto the operator's slots:
     *   - `verts` / `verts_len`           — the BMVert* set to search. May
     *                                       be null with `verts_len == 0`.
     *   - `dist`                          — maximum merge distance.
     *   - `use_connected`                 — restrict pairing to verts joined
     *                                       by existing topology.
     *
     * `keep_verts` / `keep_len` describe verts that must survive a merge.
     * The `remove_doubles` operator exposes no `keep_verts` slot of its own,
     * so these are accepted for signature parity with `bms_find_doubles` but
     * are not forwarded; pass null with `keep_len == 0`.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_remove_doubles(BMesh *bm,
                            BMVert **verts, int verts_len,
                            BMVert **keep_verts, int keep_len,
                            float dist, bool use_connected);

    /* Invoke BMesh's `pointmerge` operator, which moves every input vert onto
     * the position `merge_co` and welds the whole set together onto a single
     * survivor (the first vert in the input buffer), collapsing the topology.
     * The mesh is mutated directly; there is no output slot to read back.
     *
     * Inputs map onto the operator's slots:
     *   - `verts` / `verts_len`           — the BMVert* set to merge. May be
     *                                       null with `verts_len == 0`.
     *   - `merge_co`                       — the float[3] target position all
     *                                       input verts are moved to.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_pointmerge(BMesh *bm,
                        BMVert **verts, int verts_len,
                        const float merge_co[3]);

    /* Maps to BMesh's `duplicate` BMOP - clones a selection into disjoint
     * coincident geometry within the same mesh.
     *
     * Input:
     *   - `geom` / `geom_len`  — mixed vert / edge / face selection
     *                            forwarded to the operator's `geom` slot via
     *                            the `%eb` element-buffer specifier
     *                            (BM_VERT | BM_EDGE | BM_FACE). May be null
     *                            with `geom_len == 0`. The destination mesh
     *                            defaults to `bm` (the operator's `dest`
     *                            slot is left unset).
     *   - `use_edge_flip_from_face` — forwards the operator's bool in-slot of
     *                            the same name, copying edge flip state from
     *                            connected faces.
     *
     * Outputs are read back into caller-allocated buffers; each `_cap`
     * argument is the number of writable slots in its buffer, and the
     * corresponding count is returned (see below). Every buffer is written
     * up to its cap, and the true count is reported so callers can detect
     * truncation. Any output buffer may be null with its cap `0` to skip
     * reading that slot.
     *
     *   - `out_geom` / `out_geom_cap` — the `geom.out` element buffer (the
     *                            newly created clone elements, BM_ALL_NOLOOP:
     *                            verts, edges, faces). The total count is the
     *                            function's primary return value.
     *   - the five `*_map.out` mapping slots are emitted as flat (src, dst)
     *     couples: `buf[2*i]` is the map key, `buf[2*i+1]` the mapped value,
     *     so each buffer needs `2 * cap` writable slots. NOTE: the operator
     *     inserts each correspondence in both directions (source->dupe and
     *     dupe->source), so a duplicated set of N elements yields 2*N couples
     *     per element-type map. The per-slot couple counts are returned via
     *     the `out_*_count` out-params (each may be null to ignore):
     *       - `out_boundary_map` (edge->edge) — split-boundary edges that map
     *         original edges to destination edges; count in `out_boundary_count`.
     *       - `out_isovert_map`  (vert->vert) — isolated (wire/loose) verts;
     *         count in `out_isovert_count`.
     *       - `out_vert_map`     (vert->vert) — full vert correspondence;
     *         count in `out_vert_count`.
     *       - `out_edge_map`     (edge->edge) — full edge correspondence;
     *         count in `out_edge_count`.
     *       - `out_face_map`     (face->face) — full face correspondence;
     *         count in `out_face_count`.
     *
     * Element pointers in `geom` must remain valid for the duration of the
     * call. Returns the total `geom.out` count (which may exceed
     * `out_geom_cap`), or -1 if BMO_op_initf rejected the input (in which
     * case no out-params are written). */
    int bms_duplicate(BMesh *bm,
                      BMHeader **geom, int geom_len,
                      bool use_edge_flip_from_face,
                      BMHeader **out_geom, int out_geom_cap,
                      BMHeader **out_boundary_map, int out_boundary_cap,
                      int *out_boundary_count,
                      BMVert **out_isovert_map, int out_isovert_cap,
                      int *out_isovert_count,
                      BMVert **out_vert_map, int out_vert_cap,
                      int *out_vert_count,
                      BMEdge **out_edge_map, int out_edge_cap,
                      int *out_edge_count,
                      BMFace **out_face_map, int out_face_cap,
                      int *out_face_count);

    /* Maps to BMesh's `duplicate` BMOP driven through its `dest` pointer
     * slot: clone the `geom` selection out of the source mesh `bm` and into
     * a *separate* destination mesh `bm_dst`. Unlike `bms_duplicate` (which
     * leaves `dest` unset so clones are born in `bm`), here the clones are
     * created in `bm_dst`, which may be empty or already hold geometry on
     * entry.
     *
     * Inputs mirror `bms_duplicate`:
     *   - `geom` / `geom_len`  — mixed vert / edge / face selection from the
     *                            *source* mesh, forwarded via `%eb`
     *                            (BM_VERT | BM_EDGE | BM_FACE). May be null
     *                            with `geom_len == 0`.
     *   - `use_edge_flip_from_face` — forwards the bool in-slot of the same
     *                            name.
     *   - `bm_dst`             — the destination mesh wired into the
     *                            operator's `dest` slot. Its operator-flag
     *                            pools are ensured before exec so the clones
     *                            can be created and marked there. After the
     *                            call `bm_dst` is an ordinary mesh whose
     *                            verts / edges / faces (the clones) can be
     *                            walked with the normal whole-mesh iteration
     *                            entry points.
     *
     * Outputs mirror `bms_duplicate`: `out_geom` receives the `geom.out`
     * element buffer, and the five `*_map.out` slots are emitted as flat
     * (src, dst) couples (`buf[2*i]` key, `buf[2*i+1]` value) up to each
     * `_cap`, with the full couple count reported through the matching
     * `_count` out-param. In the cross-mesh case the map keys live in `bm`
     * and the mapped values live in `bm_dst`. Any buffer may be null with
     * its cap `0` to skip that slot.
     *
     * Return value: the operator builds its `geom.out` buffer by scanning
     * the *source* mesh for newly created elements, so in the cross-mesh
     * case (clones born in `bm_dst`) `geom.out` comes back empty. When
     * `geom.out` is empty this function instead returns the clone count
     * derived from the correspondence maps — the per-kind maps record each
     * correspondence in both directions, so the clone count is
     * (vert_couples + edge_couples + face_couples) / 2, matching the
     * BM_ALL_NOLOOP element count `bms_duplicate` returns in-place. Returns
     * -1 if BMO_op_initf rejected the input (no out-params written). */
    int bms_duplicate_into_dest(BMesh *bm,
                                BMHeader **geom, int geom_len,
                                bool use_edge_flip_from_face,
                                BMesh *bm_dst,
                                BMHeader **out_geom, int out_geom_cap,
                                BMHeader **out_boundary_map, int out_boundary_cap,
                                int *out_boundary_count,
                                BMVert **out_isovert_map, int out_isovert_cap,
                                int *out_isovert_count,
                                BMVert **out_vert_map, int out_vert_cap,
                                int *out_vert_count,
                                BMEdge **out_edge_map, int out_edge_cap,
                                int *out_edge_count,
                                BMFace **out_face_map, int out_face_cap,
                                int *out_face_count);

    /* Invoke BMesh's `split` operator, which duplicates the supplied
     * geometry and tears the copy off as a topologically disjoint set
     * within the same mesh (the split-off copy replaces the selection's
     * shared topology so the duplicate no longer shares verts/edges with
     * the remainder).
     *
     * `geom` / `geom_len` are forwarded to the operator's `geom`
     * element-buffer slot via the `%eb` specifier (a mixed BM_VERT |
     * BM_EDGE | BM_FACE set); either may be null with a length of 0. The
     * `dest` pointer slot is left unset, so the split copy lands in `bm`.
     * `use_only_faces`, when true, suppresses duplication of loose
     * verts/edges.
     *
     * After exec the output slots are read back into the caller's
     * buffers:
     *
     *   - The `geom.out` element buffer is walked with a BMOIter
     *     restricted to BM_ALL_NOLOOP and written into `out_geom` up to
     *     `out_geom_cap` entries; its full count is the return value.
     *   - The `boundary_map.out` (edge->edge) and `isovert_map.out`
     *     (vert->vert) MAP_ELEM slots are each walked with a BMOIter and
     *     emitted as flat (src, dst) couples (`buf[2*i]` key,
     *     `buf[2*i+1]` value), up to the slot's `_cap` couples; the full
     *     couple count is stored through the matching `_count` out-param
     *     when that pointer is non-null. Each map buffer needs
     *     `2 * cap` writable slots.
     *
     * Element pointers in `geom` must remain valid for the duration of
     * the call. Returns the total `geom.out` count (which may exceed
     * `out_geom_cap`), or -1 if BMO_op_initf rejected the input (in which
     * case no out-params are written). */
    int bms_split(BMesh *bm,
                  BMHeader **geom, int geom_len,
                  bool use_only_faces,
                  BMHeader **out_geom, int out_geom_cap,
                  BMEdge **out_boundary_map, int out_boundary_cap,
                  int *out_boundary_count,
                  BMVert **out_isovert_map, int out_isovert_cap,
                  int *out_isovert_count);

    /* Maps to BMesh's `split` BMOP, setting its `dest` pointer slot to
     * `bm_dst` through the operator format string. This drives the operator's
     * *declared* slot form, but the slot is inert: the operator's exec never
     * reads `dest`, forwarding only the `geom` buffer to its internal
     * duplicate sub-op. The split-off copy is therefore always created in the
     * source mesh `bm`, and `bm_dst` is left untouched even though it is
     * wired in. Behaves identically to `bms_split`.
     *
     * Inputs:
     *   - `geom` / `geom_len`  — mixed vert / edge / face selection from the
     *                            source mesh, forwarded via `%eb`
     *                            (BM_VERT | BM_EDGE | BM_FACE). May be null
     *                            with `geom_len == 0`.
     *   - `use_only_faces`     — when true, suppresses duplication of loose
     *                            verts/edges.
     *   - `bm_dst`             — passed into the operator's `dest` slot. The
     *                            operator ignores it, so it is not modified by
     *                            this call.
     *
     * Outputs mirror `bms_split`: `out_geom` receives the `geom.out` element
     * buffer, and the `boundary_map.out` (edge->edge) and `isovert_map.out`
     * (vert->vert) MAP_ELEM slots are emitted as flat (src, dst) couples
     * (`buf[2*i]` key, `buf[2*i+1]` value) up to each `_cap`, with the full
     * couple count reported through the matching `_count` out-param. Each map
     * buffer needs `2 * cap` writable slots. Any buffer may be null with its
     * cap `0` to skip that slot. The clone is born in `bm`, so every key and
     * value points into `bm`.
     *
     * The operator builds `geom.out` by scanning the source mesh, where the
     * clone lives, so it returns the full same-mesh clone (not an empty
     * buffer). A genuine cross-mesh tear must instead be composed from a
     * cross-mesh duplicate plus a source-side delete. Element pointers in
     * `geom` must remain valid for the duration of the call. Returns the
     * `geom.out` count (which may exceed `out_geom_cap`), or -1 if
     * BMO_op_initf rejected the input (no out-params written). */
    int bms_split_into_dest(BMesh *bm,
                            BMHeader **geom, int geom_len,
                            bool use_only_faces,
                            BMesh *bm_dst,
                            BMHeader **out_geom, int out_geom_cap,
                            BMEdge **out_boundary_map, int out_boundary_cap,
                            int *out_boundary_count,
                            BMVert **out_isovert_map, int out_isovert_cap,
                            int *out_isovert_count);

    /* Invoke BMesh's `mirror` operator, which duplicates the supplied
     * geometry, reflects the duplicate by negating the chosen axis in the
     * `matrix` space, flips the reflected faces' winding, and welds each
     * reflected vert back onto its original when the original lies within
     * `merge_dist` of the mirror plane.
     *
     * Slot mapping:
     *   - `geom` / `geom_len` feed the `geom` element-buffer in-slot via
     *     the `%eb` specifier (a mixed BM_VERT | BM_EDGE | BM_FACE set);
     *     either may be null with a length of 0.
     *   - `matrix` points to 16 floats defining the mirror transform space
     *     fed to the operator's `matrix` (BMO_OP_SLOT_MAT) in-slot. The
     *     layout is Blender's native column-major 4x4: the 16 floats are
     *     read as `m[col][row]` (i.e. `m[i / 4][i % 4]`), so the three
     *     translation components occupy indices 12, 13, 14. May be null,
     *     in which case the identity matrix is used.
     *   - `merge_dist` sets the `merge_dist` float in-slot: the maximum
     *     distance from the mirror plane within which an original vert is
     *     welded to its reflection. 0 disables welding.
     *   - `axis` sets the `axis` int in-slot (0 = X, 1 = Y, 2 = Z); this is
     *     the component negated in `matrix` space to perform the
     *     reflection.
     *   - `mirror_u`, `mirror_v`, `mirror_udim` set the matching bool
     *     in-slots controlling UV mirroring of the reflected faces.
     *
     * After exec the `geom.out` element buffer (the mirrored, post-weld
     * verts/edges/faces) is walked with a BMOIter restricted to
     * BM_ALL_NOLOOP and written into `out_geom` up to `out_geom_cap`
     * entries; its full count is the return value.
     *
     * Element pointers in `geom` must remain valid for the duration of the
     * call. Returns the total `geom.out` count (which may exceed
     * `out_geom_cap`), or -1 if BMO_op_initf rejected the input (in which
     * case `out_geom` is not written). */
    int bms_mirror(BMesh *bm,
                   BMHeader **geom, int geom_len,
                   const float *matrix,
                   float merge_dist,
                   int axis,
                   bool mirror_u, bool mirror_v, bool mirror_udim,
                   BMHeader **out_geom, int out_geom_cap);

    /* Invoke BMesh's `transform` operator: apply a 4x4 affine matrix to the
     * positions of the input vertices, mutating the mesh in place.
     *   - `verts` / `verts_len` fill the `verts` element buffer in-slot
     *     (BM_VERT only). They may be null with `verts_len` 0, in which
     *     case the operator is a no-op.
     *   - `matrix` points to 16 floats in Blender's native column-major 4x4
     *     layout (read as `m[i / 4][i % 4]`, so translation occupies indices
     *     12, 13, 14). It is forwarded to the `matrix` (BMO_OP_SLOT_MAT)
     *     in-slot; a null pointer is treated as the identity matrix.
     *   - `space` uses the same 16-float column-major layout and feeds the
     *     `space` in-slot. When non-zero, `matrix` is re-expressed in that
     *     frame (space^-1 * matrix * space) before being applied. A null
     *     pointer feeds the all-zeros sentinel, which the operator reads as
     *     "no space transform" (the space slot is skipped, not inverted),
     *     so `matrix` is applied directly in world space.
     *   - `use_shapekey` sets the matching bool in-slot; when true the same
     *     transform is also applied to each vertex's shape-key coordinates.
     *
     * The operator has no output slot; vertex positions are mutated in
     * place and there is nothing to read back. Element pointers in `verts`
     * must remain valid for the duration of the call. */
    void bms_transform(BMesh *bm,
                       BMVert **verts, int verts_len,
                       const float *matrix,
                       const float *space,
                       bool use_shapekey);

    /* Invoke BMesh's `smooth_vert` operator: relax each input vertex toward
     * the unweighted average of its connected neighbours (the other endpoint
     * of every incident edge), blended from its original position by
     * `factor`. A single double-buffered pass: every target is computed
     * against the input positions and applied afterwards, so the result is
     * order-independent.
     *   - `verts` / `verts_len` fill the `verts=%eb` element buffer in-slot.
     *   - `factor` sets the `factor` float in-slot (lerp original->average).
     *   - `mirror_clip_x/y/z` set the matching bool in-slots; a vert whose
     *     original coordinate on that axis is within `clip_dist` of 0 has its
     *     target coordinate on that axis snapped to 0.
     *   - `clip_dist` sets the `clip_dist` float in-slot (the clip band).
     *   - `use_axis_x/y/z` set the matching bool in-slots; only enabled-axis
     *     coordinates of the target are written back.
     *
     * The operator has no output slot; vertex positions are mutated in place.
     * Element pointers in `verts` must remain valid for the duration of the
     * call. */
    void bms_smooth_vert(BMesh *bm,
                         BMVert **verts, int verts_len,
                         float factor,
                         bool mirror_clip_x, bool mirror_clip_y, bool mirror_clip_z,
                         float clip_dist,
                         bool use_axis_x, bool use_axis_y, bool use_axis_z);

    /* Invoke BMesh's `smooth_laplacian_vert` operator: relax each input vertex
     * by assembling and solving one cotangent-weighted Laplacian system (a
     * single implicit/backward-Euler smoothing step). Interior verts use the
     * area-normalised cotangent weights at strength `lambda_factor`; rim verts
     * (on a topological boundary or adjacent to a face outside the region) use
     * a uniform edge-length weighting at strength `lambda_border`.
     *
     * The operator's smoothing region is carried by the face SELECT flags, not
     * a slot: this shim selects exactly the faces in `region_faces` (and clears
     * SELECT on every other face and on all edges) before running, so the named
     * faces' corner triangles supply the cotangent weights and their rim
     * behaves as boundary. Pass the whole face set to treat the entire mesh as
     * the region.
     *   - `verts` / `verts_len` fill the `verts=%eb` element buffer in-slot;
     *     these are the vertices free to move (all other verts are pinned as
     *     fixed boundary conditions). May be null with `verts_len` 0 (no-op).
     *   - `region_faces` / `region_faces_len` select the region faces. May be
     *     null with `region_faces_len` 0, in which case no face is selected and
     *     the assembly has no cotangent support (every free vert stays put).
     *   - `lambda_factor` / `lambda_border` set the matching float in-slots.
     *   - `use_x/y/z` set the matching bool in-slots; only enabled-axis
     *     coordinates of the solution are written back.
     *   - `preserve_volume` sets the matching bool in-slot; when true the mesh
     *     volume is measured before/after and the free verts are rescaled about
     *     the origin to restore it.
     *
     * The operator has no output slot; vertex positions are mutated in place,
     * and face/edge SELECT flags are left as this shim set them. Element
     * pointers must remain valid for the duration of the call. A mesh with no
     * faces is a no-op. */
    void bms_smooth_laplacian(BMesh *bm,
                              BMVert **verts, int verts_len,
                              BMFace **region_faces, int region_faces_len,
                              float lambda_factor, float lambda_border,
                              bool use_x, bool use_y, bool use_z,
                              bool preserve_volume);

    /* Invoke BMesh's `symmetrize` operator: bisect `geom` along an
     * axis-aligned plane, clear the named half, then mirror the surviving
     * half across the plane and weld the duplicated geometry onto the
     * seam within `dist`.
     *   - `geom` / `geom_len` fill the `input` element buffer in-slot.
     *   - `direction` sets the `direction` int-enum in-slot selecting the
     *     signed axis half that is kept and mirrored: 0 = -X, 1 = -Y,
     *     2 = -Z, 3 = +X, 4 = +Y, 5 = +Z.
     *   - `dist` sets the `dist` float in-slot, the on-plane merge
     *     tolerance used when welding the seam.
     *   - `use_shapekey` sets the `use_shapekey` bool in-slot, controlling
     *     whether shape-key coordinates are transformed alongside the
     *     base geometry.
     *
     * After exec the `geom.out` element buffer (the symmetric, post-weld
     * verts/edges/faces) is walked with a BMOIter restricted to
     * BM_ALL_NOLOOP and written into `out_geom` up to `out_geom_cap`
     * entries; its full count is the return value.
     *
     * Element pointers in `geom` must remain valid for the duration of the
     * call. Returns the total `geom.out` count (which may exceed
     * `out_geom_cap`), or -1 if BMO_op_initf rejected the input (in which
     * case `out_geom` is not written). */
    int bms_symmetrize(BMesh *bm,
                       BMHeader **geom, int geom_len,
                       int direction,
                       float dist,
                       bool use_shapekey,
                       BMHeader **out_geom, int out_geom_cap);

    /* Invoke BMesh's `bisect_plane` operator: slice `geom` by an arbitrary
     * plane, optionally snapping on-plane verts onto the plane and clearing
     * the geometry on one or both sides.
     *   - `geom` / `geom_len` fill the `geom` element-buffer in-slot (a mixed
     *     BM_VERT | BM_EDGE | BM_FACE set).
     *   - `plane_co` (3 floats) sets the `plane_co` vec in-slot: a point on
     *     the cutting plane.
     *   - `plane_no` (3 floats) sets the `plane_no` vec in-slot: the plane
     *     normal. Its sign defines which side is "positive" (outer) and which
     *     is "negative" (inner).
     *   - `dist` sets the `dist` float in-slot: the tolerance within which a
     *     vert is treated as exactly on the plane.
     *   - `use_snap_center` sets the `use_snap_center` bool in-slot: snap
     *     on-plane verts onto the plane.
     *   - `clear_inner` sets the `clear_inner` bool in-slot: remove geometry
     *     on the negative side of the plane.
     *   - `clear_outer` sets the `clear_outer` bool in-slot: remove geometry
     *     on the positive side of the plane.
     *
     * The operator exposes two output element buffers that are surfaced
     * separately:
     *   - `geom.out` (the full surviving geometry, walked with a BMOIter
     *     restricted to BM_ALL_NOLOOP) is written into `out_geom` up to
     *     `out_geom_cap` entries; its full count is the return value.
     *   - `geom_cut.out` (the on-plane cut seam, restricted to
     *     BM_VERT | BM_EDGE) is written into `out_cut` up to `out_cut_cap`
     *     entries; its full count is written through `out_cut_len`.
     *
     * Either output buffer may be null with a cap of 0 to skip its read-back;
     * `out_cut_len` may be null. Element pointers in `geom` must remain valid
     * for the duration of the call. Returns the total `geom.out` count (which
     * may exceed `out_geom_cap`), or -1 if BMO_op_initf rejected the input (in
     * which case neither output buffer nor `out_cut_len` is written). */
    int bms_bisect_plane(BMesh *bm,
                         BMHeader **geom, int geom_len,
                         const float *plane_co,
                         const float *plane_no,
                         float dist,
                         bool use_snap_center,
                         bool clear_inner,
                         bool clear_outer,
                         BMHeader **out_geom, int out_geom_cap,
                         BMHeader **out_cut, int out_cut_cap, int *out_cut_len);

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

    /* Invoke BMesh's `subdivide_edges` operator on the supplied edge set.
     * Each input edge is split into `cuts + 1` segments and the faces
     * touching the cut edges are re-filled according to the chosen pattern.
     *
     * Parameters map 1:1 onto the operator's slots:
     *
     *   - `edges`            — the edges to subdivide. The buffer may be null
     *                          only when `edges_len == 0`, a no-op.
     *   - `cuts`             — number of cut points per edge (segments minus
     *                          one). `cuts == 1` is a single midpoint cut.
     *   - `smooth`           — Catmull-Clark-style smoothing factor applied to
     *                          new vertices; `0.0` keeps them on the originals.
     *   - `smooth_falloff`    — curve applied to the smoothing offset; one of
     *                          the BMS_SUBD_FALLOFF_* values below.
     *   - `use_smooth_even`   — keep the smoothing offset even by scaling it by
     *                          the inverse of the vertex's edge-angle factor.
     *   - `fractal`          — random displacement magnitude for new vertices;
     *                          `0.0` disables fractal jitter.
     *   - `along_normal`     — factor (0..1) restricting fractal displacement
     *                          toward the surface normal.
     *   - `seed`             — seed for the fractal random number generator.
     *   - `quad_corner_type` — fill pattern for quads cut on one corner; one of
     *                          the BMS_SUBD_CORNER_* values below.
     *   - `use_grid_fill`    — fill fully-selected faces with a regular grid.
     *   - `use_single_edge`  — tessellate the single-edge case in a quad/tri.
     *   - `use_only_quads`   — only subdivide quads (loop-cut behaviour).
     *
     * Corner-type values (matching BMesh's quad innervert enum):
     *   BMS_SUBD_CORNER_INNERVERT    = 0
     *   BMS_SUBD_CORNER_PATH         = 1
     *   BMS_SUBD_CORNER_FAN          = 2
     *   BMS_SUBD_CORNER_STRAIGHT_CUT = 3
     *
     * Smooth-falloff values (matching BMesh's subdivide falloff enum):
     *   BMS_SUBD_FALLOFF_SMOOTH        = 0
     *   BMS_SUBD_FALLOFF_SPHERE        = 1
     *   BMS_SUBD_FALLOFF_ROOT          = 2
     *   BMS_SUBD_FALLOFF_SHARP         = 3
     *   BMS_SUBD_FALLOFF_LIN           = 4
     *   BMS_SUBD_FALLOFF_INVSQUARE     = 7
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    enum BMS_SubdCornerType
    {
        BMS_SUBD_CORNER_INNERVERT = 0,
        BMS_SUBD_CORNER_PATH = 1,
        BMS_SUBD_CORNER_FAN = 2,
        BMS_SUBD_CORNER_STRAIGHT_CUT = 3,
    };
    enum BMS_SubdFalloff
    {
        BMS_SUBD_FALLOFF_SMOOTH = 0,
        BMS_SUBD_FALLOFF_SPHERE = 1,
        BMS_SUBD_FALLOFF_ROOT = 2,
        BMS_SUBD_FALLOFF_SHARP = 3,
        BMS_SUBD_FALLOFF_LIN = 4,
        BMS_SUBD_FALLOFF_INVSQUARE = 7,
    };
    bool bms_subdivide_edges(BMesh *bm,
                             BMEdge **edges, int edges_len,
                             int cuts,
                             float smooth,
                             int smooth_falloff,
                             bool use_smooth_even,
                             float fractal,
                             float along_normal,
                             int seed,
                             int quad_corner_type,
                             bool use_grid_fill,
                             bool use_single_edge,
                             bool use_only_quads,
                             bool use_sphere);

    /* Invoke BMesh's `subdivide_edgering` operator on the supplied edge set.
     * Takes an edge-ring and subdivides it across the connecting faces,
     * inserting `cuts` new loops and re-filling with interpolated geometry.
     *
     * Parameters map 1:1 onto the operator's slots:
     *
     *   - `edges`                — the edge-ring to subdivide. The buffer may
     *                              be null only when `edges_len == 0`, a no-op.
     *   - `cuts`                 — number of new loops inserted across the ring.
     *   - `interp_mode`          — interpolation method for the new geometry;
     *                              one of the BMS_RING_INTERP_* values below.
     *   - `smooth`               — smoothing factor applied to the new loops.
     *   - `profile_shape`        — falloff curve shaping the inserted profile;
     *                              one of the BMS_SUBD_FALLOFF_* values above.
     *   - `profile_shape_factor` — how far intermediary new edges are
     *                              shrunk/expanded along the profile.
     *
     * Interpolation-mode values (matching BMesh's edge-ring interp enum):
     *   BMS_RING_INTERP_LINEAR  = 0
     *   BMS_RING_INTERP_PATH    = 1
     *   BMS_RING_INTERP_SURFACE = 2
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    enum BMS_RingInterpMode
    {
        BMS_RING_INTERP_LINEAR = 0,
        BMS_RING_INTERP_PATH = 1,
        BMS_RING_INTERP_SURFACE = 2,
    };
    bool bms_subdivide_edgering(BMesh *bm,
                                BMEdge **edges, int edges_len,
                                int cuts,
                                int interp_mode,
                                float smooth,
                                int profile_shape,
                                float profile_shape_factor);

    /* Capturing variant of `bms_subdivide_edgering`. Runs the same
     * `subdivide_edgering` BMOP and additionally reads the operator's
     * `faces.out` slot — the newly-created fill faces (each subdivided
     * strip quad becomes a column of `cuts + 1` quads) — into the
     * caller-provided `out_buf`.
     *
     * Up to `out_cap` `BMFace*` pointers are written into `out_buf`;
     * the full slot length is returned regardless of `out_cap`, so a
     * size-probe call may pass a null `out_buf` with `out_cap == 0`.
     * Returns -1 if BMO_op_initf rejected the input. */
    int bms_subdivide_edgering_out(BMesh *bm,
                                   BMEdge **edges, int edges_len,
                                   int cuts,
                                   int interp_mode,
                                   float smooth,
                                   int profile_shape,
                                   float profile_shape_factor,
                                   BMFace **out_buf, int out_cap);

    /* Capturing variant of `bms_subdivide_edges`. Runs the same
     * `subdivide_edges` BMOP with the identical parameter set and, after
     * exec, reads back the operator's three output geometry slots:
     *
     *   - `geom_split.out`  — the new midpoint verts plus the edge-halves
     *                         produced by splitting the input edges.
     *   - `geom_inner.out`  — the inner edges (and, for the 4-cut quad /
     *                         3-cut tri patterns, the inner vert) added by
     *                         the per-face re-split.
     *   - `geom.out`        — the union of every vert, edge and face the
     *                         operator created or replaced.
     *
     * Each slot is a heterogeneous element buffer; its pointers are written
     * type-erased as BMHeader* into the matching `out_*` buffer, and the
     * caller distinguishes verts / edges / faces via `bms_elem_htype`
     * (resolving coordinates with `bms_vert_co`, endpoints with
     * `bms_edge_verts`, and face corners with the face-vertex accessors).
     *
     * For each slot, up to `*_cap` pointers are written and the full slot
     * length is reported through the matching `r_*_len` out-param (which may
     * be null); a reported length greater than its cap signals truncation.
     * Each `out_*` buffer may be null only when its cap is zero
     * (size-probing mode).
     *
     * Returns 0 on success, or -1 if BMO_op_initf rejected the input. */
    int bms_subdivide_core_out(BMesh *bm,
                               BMEdge **edges, int edges_len,
                               int cuts,
                               float smooth,
                               int smooth_falloff,
                               bool use_smooth_even,
                               float fractal,
                               float along_normal,
                               int seed,
                               int quad_corner_type,
                               bool use_grid_fill,
                               bool use_single_edge,
                               bool use_only_quads,
                               bool use_sphere,
                               BMHeader **out_split, int out_split_cap,
                               int *r_split_len,
                               BMHeader **out_inner, int out_inner_cap,
                               int *r_inner_len,
                               BMHeader **out_geom, int out_geom_cap,
                               int *r_geom_len);

    /* Invoke BMesh's `bisect_edges` operator on the supplied edge set. This
     * is the pure per-edge midpoint-split phase: each input edge is split into
     * `cuts` evenly-spaced segments, introducing `cuts` two-valence vertices
     * along it. No per-face dispatch or inner-vertex connection is performed.
     *
     *   - `edges`     — the edges to split. The buffer may be null only when
     *                   `edges_len == 0`, in which case nothing is split.
     *   - `cuts`      — number of cuts per edge.
     *
     * The mesh is mutated in place. Returns true on normal completion, false
     * if BMO_op_initf rejected the input. */
    bool bms_bisect_edges(BMesh *bm,
                          BMEdge **edges, int edges_len,
                          int cuts);

    /* Invoke BMesh's `collapse` operator on the supplied edge set. Collapses
     * each connected group of input edges to a single vertex, welding their
     * endpoints together and removing the now-degenerate geometry.
     *
     * Parameters map 1:1 onto the operator's slots:
     *
     *   - `edges` — the edges to collapse. The buffer may be null only when
     *               `edges_len == 0`, in which case nothing is collapsed.
     *   - `uvs`   — when true, also blend the per-loop custom data (UVs,
     *               vertex colours, etc.) of the welded corners.
     *
     * The mesh is mutated in place; there is no output slot. Returns true on
     * normal completion, false if BMO_op_initf rejected the input. */
    bool bms_collapse(BMesh *bm,
                      BMEdge **edges, int edges_len,
                      bool uvs);

    /* Invoke BMesh's `connect_verts` operator on the supplied vertex set.
     * For each face carrying two or more of the input verts as corners, the
     * operator inserts edges between selected corner pairs and splits the
     * face along them. Exposes all three BMOP input slots explicitly:
     *
     *   - `verts`            — the candidate corner verts to connect.
     *   - `faces_exclude`    — faces that must not be split, even if they
     *                          carry two or more input verts. May be null
     *                          with a zero length.
     *   - `check_degenerate` — when true, reject splits that would produce
     *                          overlapping or self-intersecting geometry.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_connect_verts(BMesh *bm,
                           BMVert **verts, int verts_len,
                           BMFace **faces_exclude, int faces_exclude_len,
                           bool check_degenerate);

    /* Capturing variant of `bms_connect_verts`.
     *
     * Runs the same `connect_verts` BMOP but, in addition to performing the
     * splits, copies the operator's `edges.out` slot (the edges created by
     * the face splits) into the caller-supplied buffer `out_buf` of capacity
     * `out_cap` edge slots.
     *
     * Return value:
     *   -1  on operator init failure (matches the `false` return of the
     *       non-capturing variant).
     *   >= 0 on success: the *total* number of edges the slot produced. Up
     *       to `min(total, out_cap)` pointers are written to `out_buf` (in
     *       the slot's emit order). If the returned count exceeds `out_cap`,
     *       the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero; in that case the
     * function still runs the operator and returns the created-edge count
     * for sizing purposes. */
    int bms_connect_verts_out(BMesh *bm,
                              BMVert **verts, int verts_len,
                              BMFace **faces_exclude, int faces_exclude_len,
                              bool check_degenerate,
                              BMEdge **out_buf, int out_cap);

    /* Invoke BMesh's `connect_verts_concave` operator on the supplied face
     * set. Each concave input face — one with more than three corners and at
     * least one reflex corner — is cut into convex pieces along newly
     * inserted divider edges; convex faces and triangles are left untouched.
     * Exposes the operator's single BMOP input slot:
     *
     *   - `faces` — the candidate faces to make convex.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_connect_verts_concave(BMesh *bm,
                                   BMFace **faces, int faces_len);

    /* Capturing variant of `bms_connect_verts_concave`.
     *
     * Runs the same `connect_verts_concave` BMOP but, in addition to
     * performing the cuts, copies the operator's `edges.out` slot (the
     * surviving interior divider edges) into the caller-supplied buffer
     * `out_buf` of capacity `out_cap` edge slots.
     *
     * Return value:
     *   -1  on operator init failure (matches the `false` return of the
     *       non-capturing variant).
     *   >= 0 on success: the *total* number of edges the slot produced. Up
     *       to `min(total, out_cap)` pointers are written to `out_buf` (in
     *       the slot's emit order). If the returned count exceeds `out_cap`,
     *       the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero; in that case the
     * function still runs the operator and returns the created-edge count
     * for sizing purposes. */
    int bms_connect_verts_concave_out(BMesh *bm,
                                      BMFace **faces, int faces_len,
                                      BMEdge **out_buf, int out_cap);

    /* Invoke BMesh's `connect_verts_nonplanar` operator on the supplied
     * face set. Each input face whose corners deviate from a single plane
     * by more than `angle_limit` (in radians) is split along newly inserted
     * diagonal edges so that the resulting faces are flatter than the limit;
     * sufficiently planar faces are left untouched. Exposes the operator's
     * two BMOP input slots:
     *
     *   - `faces`       — the candidate faces to flatten.
     *   - `angle_limit` — maximum non-planarity (radians) tolerated before
     *                     a face is split.
     *
     * This operator carries the normals-calc op-type flag and reads face
     * normals; callers should ensure mesh normals are up to date (e.g. via
     * a normals update) before invoking.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_connect_verts_nonplanar(BMesh *bm,
                                     BMFace **faces, int faces_len,
                                     float angle_limit);

    /* Capturing variant of `bms_connect_verts_nonplanar`.
     *
     * Runs the same `connect_verts_nonplanar` BMOP but, in addition to
     * performing the splits, copies the operator's `edges.out` slot (the
     * newly inserted diagonal edges) into the caller-supplied buffer
     * `out_buf` of capacity `out_cap` edge slots.
     *
     * Return value:
     *   -1  on operator init failure (matches the `false` return of the
     *       non-capturing variant).
     *   >= 0 on success: the *total* number of edges the slot produced. Up
     *       to `min(total, out_cap)` pointers are written to `out_buf` (in
     *       the slot's emit order). If the returned count exceeds `out_cap`,
     *       the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero; in that case the
     * function still runs the operator and returns the created-edge count
     * for sizing purposes. */
    int bms_connect_verts_nonplanar_out(BMesh *bm,
                                        BMFace **faces, int faces_len,
                                        float angle_limit,
                                        BMEdge **out_buf, int out_cap);

    /* Face-capturing variant of `bms_connect_verts_nonplanar`.
     *
     * Runs the same `connect_verts_nonplanar` BMOP but copies the operator's
     * `faces.out` slot into the caller-supplied buffer `out_buf` of capacity
     * `out_cap` face slots. The `faces.out` slot holds every face the
     * operator touched: both halves of each committed split and every leaf
     * piece. Re-flagging is idempotent, so each face appears once.
     *
     * Return value:
     *   -1  on operator init failure (matches the `false` return of the
     *       non-capturing variant).
     *   >= 0 on success: the *total* number of faces the slot produced. Up
     *       to `min(total, out_cap)` pointers are written to `out_buf` (in
     *       the slot's emit order). If the returned count exceeds `out_cap`,
     *       the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero; in that case the
     * function still runs the operator and returns the touched-face count
     * for sizing purposes. */
    int bms_connect_verts_nonplanar_faces_out(BMesh *bm,
                                              BMFace **faces, int faces_len,
                                              float angle_limit,
                                              BMFace **out_buf, int out_cap);

    /* Invoke BMesh's `planar_faces` operator on the supplied face set. The
     * operator nudges the vertices of the input faces toward each face's
     * average plane, flattening them in place. Exposes the operator's three
     * input slots:
     *
     *   - `faces`      — the faces to flatten.
     *   - `iterations` — number of flattening passes (relevant when the
     *                    input faces share vertices, so each pass propagates).
     *   - `factor`     — per-iteration influence of the move toward planar.
     *
     * The operator mutates input-face vertex positions in place and does not
     * produce any geometry the caller needs read back, so no output buffer is
     * exposed. `faces` may be null when `faces_len` is zero (empty no-op).
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_planar_faces(BMesh *bm,
                          BMFace **faces, int faces_len,
                          int iterations, float factor);

    /* Capturing variant of `bms_planar_faces` that reads back the operator's
     * `geom.out` output slot.
     *
     * Runs the same `planar_faces` BMOP with the same
     * `faces=%eb iterations=%i factor=%f` parameterisation, then copies up to
     * `out_cap` element-header pointers from the `geom.out` slot into
     * `out_geom_buf`. The slot is a mixed-geometry buffer (slot type
     * `BM_VERT | BM_EDGE | BM_FACE`), so its entries are `BMHeader *` pointers
     * that may refer to verts, edges, or faces interchangeably.
     *
     * Returns the slot's true element count (which may exceed `out_cap`, in
     * which case the buffer holds only the first `out_cap` entries), or -1 if
     * BMO_op_initf rejected the input. The operator does not populate
     * `geom.out`, so the returned length is expected to be zero. */
    int bms_planar_faces_geom_out(BMesh *bm,
                                  BMFace **faces, int faces_len,
                                  int iterations, float factor,
                                  BMHeader **out_geom_buf, int out_cap);

    /* Maps to BMesh's `region_extend` operator, which grows or shrinks a
     * region of geometry by one ring of incident elements. The input `geom`
     * is a mixed vert/edge/face buffer, forwarded to the operator's `geom`
     * element-buffer slot (`%eb`); it may be null when `geom_len` is zero.
     * The three bool slots are forwarded explicitly:
     *
     *   - `use_contract`  — find the boundary inside the region (shrink)
     *                       instead of outside it (grow).
     *   - `use_faces`     — extend across faces rather than edges, so the
     *                       output is faces instead of verts and edges.
     *   - `use_face_step` — step over connected faces while walking.
     *
     * The operator's `geom.out` slot — the computed ring of boundary
     * geometry — is a mixed vert/edge/face buffer; each entry is returned
     * type-erased as a BMHeader* (the header is the first field of every
     * element). Up to `out_geom_cap` pointers are written into the
     * caller-allocated `out_geom`; the return value is the slot's true
     * total element count.
     *
     * Return value:
     *   -1  on operator init failure.
     *   >= 0 on success: the total number of output elements. Up to
     *        min(total, out_geom_cap) pointers are written to `out_geom`.
     *        A return greater than `out_geom_cap` signals truncation.
     *
     * `out_geom` may be null only when `out_geom_cap` is zero. */
    int bms_region_extend(BMesh *bm,
                          BMHeader **geom, int geom_len,
                          bool use_contract, bool use_faces, bool use_face_step,
                          BMHeader **out_geom, int out_geom_cap);

    /* Invoke BMesh's `rotate_edges` operator on the supplied edge set. Each
     * eligible edge — one shared by exactly two faces whose union forms a
     * quad — is rotated (spun) to its other diagonal; ineligible edges are
     * skipped by the operator's own filtering. Exposes the operator's two
     * BMOP input slots:
     *
     *   - `edges`   — the candidate edges to rotate.
     *   - `use_ccw` — rotate counter-clockwise when true, clockwise when
     *                 false.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_rotate_edges(BMesh *bm,
                          BMEdge **edges, int edges_len,
                          bool use_ccw);

    /* Capturing variant of `bms_rotate_edges`.
     *
     * Runs the same `rotate_edges` BMOP but, in addition to performing the
     * rotations, copies the operator's `edges.out` slot (one edge per
     * successfully rotated edge) into the caller-supplied buffer `out_buf`
     * of capacity `out_cap` edge slots.
     *
     * Return value:
     *   -1  on operator init failure (matches the `false` return of the
     *       non-capturing variant).
     *   >= 0 on success: the *total* number of edges the slot produced. Up
     *       to `min(total, out_cap)` pointers are written to `out_buf` (in
     *       the slot's emit order). If the returned count exceeds `out_cap`,
     *       the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero; in that case the
     * function still runs the operator and returns the rotated-edge count
     * for sizing purposes. */
    int bms_rotate_edges_out(BMesh *bm,
                             BMEdge **edges, int edges_len,
                             bool use_ccw,
                             BMEdge **out_buf, int out_cap);

    /* Invoke BMesh's `connect_vert_pair` operator. Given exactly two input
     * verts, the operator connects them along the shortest path that stays
     * within the surrounding faces: it splits each edge crossed by that path
     * to introduce intermediate verts, then runs the `connect_verts` sub-op
     * to insert the connecting edges and split the traversed faces. Exposes
     * all three BMOP input slots explicitly:
     *
     *   - `verts`         — the pair of verts to connect; `verts_len` must be
     *                       exactly 2.
     *   - `verts_exclude` — verts the path must not route through. May be
     *                       null with a zero length.
     *   - `faces_exclude` — faces the path must not cross. May be null with a
     *                       zero length.
     *
     * Returns true on success, false if BMO_op_initf rejected the input. */
    bool bms_connect_vert_pair(BMesh *bm,
                               BMVert **verts, int verts_len,
                               BMVert **verts_exclude, int verts_exclude_len,
                               BMFace **faces_exclude, int faces_exclude_len);

    /* Capturing variant of `bms_connect_vert_pair`.
     *
     * Runs the same `connect_vert_pair` BMOP but, in addition to performing
     * the connection, copies the operator's `edges.out` slot (the edges
     * created along the connecting path) into the caller-supplied buffer
     * `out_buf` of capacity `out_cap` edge slots.
     *
     * Return value:
     *   -1  on operator init failure (matches the `false` return of the
     *       non-capturing variant).
     *   >= 0 on success: the *total* number of edges the slot produced. Up
     *       to `min(total, out_cap)` pointers are written to `out_buf` (in
     *       the slot's emit order). If the returned count exceeds `out_cap`,
     *       the buffer was undersized.
     *
     * `out_buf` may be null only when `out_cap` is zero; in that case the
     * function still runs the operator and returns the created-edge count
     * for sizing purposes. */
    int bms_connect_vert_pair_out(BMesh *bm,
                                  BMVert **verts, int verts_len,
                                  BMVert **verts_exclude, int verts_exclude_len,
                                  BMFace **faces_exclude, int faces_exclude_len,
                                  BMEdge **out_buf, int out_cap);

    /* Invoke BMesh's `poke` operator on the supplied face set, splitting each
     * input face into a triangle fan around a freshly-created centre vertex,
     * and capture both of the operator's output slots.
     *
     * Parameters map onto the operator's input slots:
     *   - `faces` / `faces_len`   — the BMFace* set to poke. May be null with
     *                               `faces_len == 0` (a no-op).
     *   - `center_mode`           — centre formula, using the same convention
     *                               as the single-face poke shims:
     *                                 0 = MEAN          (uniform centroid)
     *                                 1 = BOUNDS        (axis-aligned bbox)
     *                                 2 = MEAN_WEIGHTED (edge-length weighted)
     *                               Translated internally to the operator's
     *                               native enum.
     *   - `offset`                — lift of each centre vertex along its source
     *                               face normal.
     *   - `use_relative_offset`   — when true the lift is scaled by the mean
     *                               corner-to-centre distance of each face.
     *
     * The two output slots are copied into caller-allocated buffers:
     *   - `verts.out` -> `out_verts` (one centre vertex per input face).
     *   - `faces.out` -> `out_faces` (the fan-triangle faces; n per input
     *                                 face of n corners).
     * Both slots are populated by filtering the mesh's element lists for the
     * operator's new-element tool flag, so the order is the mesh's element
     * iteration order over the newly-created elements, not a per-input-face
     * grouping.
     *
     * For each slot, up to `min(slot_len, cap)` pointers are written to the
     * buffer and the total `slot_len` is reported through the matching
     * `r_*_len` out-param (which may be null). A returned length greater than
     * its cap signals the buffer was undersized. Either buffer may be null
     * only when its cap is zero (size-probing mode).
     *
     * Returns 0 on success, or -1 if BMO_op_initf rejected the input. */
    int bms_poke_out(BMesh *bm,
                     BMFace **faces, int faces_len,
                     int center_mode, float offset, bool use_relative_offset,
                     BMVert **out_verts, int out_verts_cap, int *r_verts_len,
                     BMFace **out_faces, int out_faces_cap, int *r_faces_len);

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

    /* Invoke BMesh's `bridge_loops` operator on the supplied edge set.
     * Builds geometry spanning two or more edge loops. The input slots map
     * directly onto the operator's BMOP slots:
     *
     *   - `edges`        — the edge loops to bridge.
     *   - `use_pairs`    — bridge consecutive loop pairs rather than chaining
     *                      every loop into one strip; requires an even number
     *                      of loops.
     *   - `use_cyclic`   — treat the loops as closed (cyclic) rather than open.
     *   - `use_merge`    — weld the loops together instead of creating new
     *                      bridging faces; requires the loops to have equal
     *                      edge counts.
     *   - `merge_factor` — interpolation factor used when `use_merge` is set.
     *   - `twist_offset` — rotational offset applied when matching closed
     *                      loops.
     *
     * The operator raises a cancel on three validation failures: fewer than
     * two loops, an odd loop count under `use_pairs`, and unequal loop edge
     * counts under `use_merge`. In every cancel case the mesh is left
     * unchanged.
     *
     * Returns true on success, false if the operator cancelled (a no-op) or
     * if BMO_op_initf rejected the input. */
    bool bms_bridge_loops(BMesh *bm,
                          BMEdge **edges, int edges_len,
                          bool use_pairs,
                          bool use_cyclic,
                          bool use_merge,
                          float merge_factor,
                          int twist_offset);

    /* Capturing variant of `bms_bridge_loops` for the `faces.out` slot.
     *
     * Runs the same `bridge_loops` BMOP and copies the operator's `faces.out`
     * slot (the faces created by the bridge) into the caller-supplied buffer
     * `faces_out` of capacity `faces_cap` face slots.
     *
     * Return value:
     *   -1  on operator init failure, or when the operator cancelled (one of
     *       the three validation failures above) — distinguishing the no-op
     *       case from a zero-face success.
     *   >= 0 on success: the *total* number of faces the slot produced. Up to
     *       `min(total, faces_cap)` pointers are written to `faces_out` (in
     *       the slot's emit order). If the returned count exceeds `faces_cap`,
     *       the buffer was undersized.
     *
     * `faces_out` may be null only when `faces_cap` is zero; in that case the
     * function still runs the operator and returns the produced count for
     * sizing purposes. */
    int bms_bridge_loops_out(BMesh *bm,
                             BMEdge **edges, int edges_len,
                             bool use_pairs,
                             bool use_cyclic,
                             bool use_merge,
                             float merge_factor,
                             int twist_offset,
                             BMFace **faces_out, int faces_cap);

    /* Capturing variant of `bms_bridge_loops` for the `edges.out` slot.
     *
     * Runs the same `bridge_loops` BMOP and copies the operator's `edges.out`
     * slot (the rung edges created across the bridge) into the caller-supplied
     * buffer `edges_out` of capacity `edges_cap` edge slots.
     *
     * Return value:
     *   -1  on operator init failure, or when the operator cancelled (one of
     *       the three validation failures above).
     *   >= 0 on success: the *total* number of edges the slot produced. Up to
     *       `min(total, edges_cap)` pointers are written to `edges_out` (in
     *       the slot's emit order). If the returned count exceeds `edges_cap`,
     *       the buffer was undersized.
     *
     * `edges_out` may be null only when `edges_cap` is zero. */
    int bms_bridge_loops_edges_out(BMesh *bm,
                                   BMEdge **edges, int edges_len,
                                   bool use_pairs,
                                   bool use_cyclic,
                                   bool use_merge,
                                   float merge_factor,
                                   int twist_offset,
                                   BMEdge **edges_out, int edges_cap);

    /* ---- Region-inset customdata-merge trace ---- */

    /* One per-corner customdata-merge invocation recorded by
     * `bms_inset_region_merge_trace`. At each moved fan hub the region-inset
     * interpolation step reconciles two diverged inner loops by averaging the
     * two "inner inset" loops behind them and writing the blend onto all four.
     *
     * Each of the four loops is identified by its owning face's index and its
     * corner vertex's index (both `BM_elem_index` values, valid after the
     * trace call rebuilds the index tables). `*_pre` / `*_post` hold the
     * traced layer's value on that loop immediately before and after the
     * merge writes. `vert_index` is the hub vertex shared by the two inner
     * loops. `comps` is the traced layer's component count (1..4); unused
     * components are zero. Any index is -1 if the corresponding loop was not
     * captured. */
    typedef struct BmsMergeInvocation
    {
        int vert_index;
        int a_inner_face, a_inner_corner_vert;
        int b_inner_face, b_inner_corner_vert;
        int a_inner_inset_face, a_inner_inset_corner_vert;
        int b_inner_inset_face, b_inner_inset_corner_vert;
        float a_inner_pre[4], a_inner_post[4];
        float b_inner_pre[4], b_inner_post[4];
        float a_inner_inset_pre[4], a_inner_inset_post[4];
        float b_inner_inset_pre[4], b_inner_inset_post[4];
        int comps;
    } BmsMergeInvocation;

    /* Callee-allocated array of merge invocations. The caller zero-initialises
     * this struct and passes it to `bms_inset_region_merge_trace`; that fills
     * `invocations` with `len` records (callee-owned) and releases it via
     * `bms_merge_trace_free`. */
    typedef struct BmsMergeTrace
    {
        BmsMergeInvocation *invocations;
        int len;
        int cap;
    } BmsMergeTrace;

    /* Run region inset with `use_interpolate` enabled and record every
     * per-corner customdata-merge invocation it performs.
     *
     * The mesh is mutated exactly as `inset_region` mutates it (the merge is
     * applied). `faces` is the input face set (type-erased BMHeader* buffer of
     * `faces_len`, all faces). `flags` is a bitmask:
     *   bit 0 use_boundary, bit 1 use_even_offset, bit 2 use_relative_offset,
     *   bit 3 use_edge_rail, bit 4 use_outset. (use_interpolate is always on.)
     *
     * `layer_name` names the loop layer to trace; the first of a float2 /
     * float3 / float (color) layer with that name is used. A null or absent
     * layer is not an error — invocations are still recorded with zeroed
     * values.
     *
     * `out` must be a zero-initialised `BmsMergeTrace`; on success it is
     * filled with a callee-owned array. Returns 1 on success, 0 if the
     * operator rejected the input, -1 on a usage error (null `bm` / `out`). */
    int bms_inset_region_merge_trace(BMesh *bm,
                                     BMHeader **faces, int faces_len,
                                     float thickness, float depth,
                                     int flags,
                                     const char *layer_name,
                                     BmsMergeTrace *out);

    /* Release the callee allocation held by a `BmsMergeTrace` and reset it to
     * empty. Safe to call on a zero-initialised or already-freed trace. */
    void bms_merge_trace_free(BmsMergeTrace *out);

    /* ---- Whole-mesh traversal micro-workloads ---- */
    /*
     * Each function runs a complete read-only traversal natively inside the
     * shim, so a single FFI crossing covers the whole walk and timing it
     * measures BMesh's own iteration cost rather than per-element call
     * overhead. None of them modify the mesh.
     */

    /* For every vertex, walk its disk cycle (the per-vertex cycle of
     * incident edges) and count the edges visited; returns the total summed
     * over all vertices. Since every edge appears in both endpoints' disks,
     * this equals 2 * totedge. */
    uint64_t bms_bench_disk_walk_sum(BMesh *bm);

    /* For every edge, walk its radial cycle (the per-edge cycle of incident
     * face loops) and count the loops visited; returns the total summed over
     * all edges. Since every loop is radial to exactly one edge, this equals
     * totloop. */
    uint64_t bms_bench_radial_walk_sum(BMesh *bm);

    /* Sum co[0] + co[1] + co[2] over every vertex, accumulated in double.
     * A whole-mesh read checksum whose result depends on every coordinate,
     * making it a convenient optimisation barrier for timed reads. */
    double bms_bench_vert_position_sum(BMesh *bm);

    /* ---- Guarded-allocator bookkeeping ---- */
    /*
     * BMesh routes its allocations through the vendored guarded allocator
     * (MEM_* / BLI_mempool on top of MEM_*), so reading these counters
     * before and after an operation observes its allocation delta. The
     * counters are process-global and cover every MEM_* user in the binary.
     */

    /* Number of memory blocks currently live in the guarded allocator
     * (MEM_get_memory_blocks_in_use). */
    unsigned int bms_mem_blocks_in_use(void);

    /* Total bytes currently live in the guarded allocator
     * (MEM_get_memory_in_use). */
    size_t bms_mem_in_use(void);

#ifdef __cplusplus
}
#endif

#endif /* BMESH_SYS_SHIM_H */
