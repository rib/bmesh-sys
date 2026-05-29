/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * C++ shim: thin extern "C" wrappers around Blender's BMesh API.
 *
 * NB: we deliberately do NOT include shim.h here — its global-namespace
 * `typedef struct BMesh BMesh;` typedefs would collide with the
 * `blender::BMesh` types from bmesh.hh. The FFI side only cares about the
 * symbol names and pointer-compatibility, both of which are guaranteed by
 * `extern "C"` linkage.
 */

#include "bmesh.hh"
#include "intern/bmesh_core.hh"
#include "intern/bmesh_iterators.hh"
#include "intern/bmesh_query.hh"
#include "intern/bmesh_mesh.hh"
#include "intern/bmesh_mesh_tessellate.hh"
#include "intern/bmesh_polygon.hh"
#include "intern/bmesh_construct.hh"
#include "intern/bmesh_interp.hh"
#include "intern/bmesh_operator_api.hh"
#include "BKE_customdata.hh"
#include "BLI_heap.h"
#include "BLI_math_geom.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"
#include "BLI_span.hh"
#include "DNA_customdata_types.h"
#include "DNA_modifier_types.h"
#include "MEM_guardedalloc.h"

#include <array>
#include <cstring>

using namespace blender; // NOLINT(google-build-using-namespace)

/* BM_verts_calc_rotate_beauty is now provided by the real
 * bmesh/tools/bmesh_beautify.cc. bpy_bm_generic_invalidate is provided
 * by the real customdata.cc. No stubs needed here. */

extern "C"
{

    BMesh *bms_mesh_create(void)
    {
        BMeshCreateParams params{};
        // Required by every BMO_op_* call (operator-flag storage is allocated
        // lazily when toolflags are enabled). Cheap when the operator
        // framework isn't used.
        params.use_toolflags = true;
        return BM_mesh_create(&bm_mesh_allocsize_default, &params);
    }

    void bms_mesh_free(BMesh *bm)
    {
        if (bm)
            BM_mesh_free(bm);
    }

    BMVert *bms_vert_create(BMesh *bm, const float co[3])
    {
        return BM_vert_create(bm, co, nullptr, BM_CREATE_NOP);
    }

    BMVert *bms_vert_create_example(BMesh *bm, const float co[3], BMVert *example)
    {
        return BM_vert_create(bm, co, example, BM_CREATE_NOP);
    }

    void bms_vert_select_set(BMesh *bm, BMVert *v, bool select)
    {
        BM_vert_select_set(bm, v, select);
    }

    /* ---- Element field accessors (read-only) for tests / harness. ---- */
    void bms_vert_co(const BMVert *v, float out[3])
    {
        out[0] = v->co[0];
        out[1] = v->co[1];
        out[2] = v->co[2];
    }
    void bms_vert_no(const BMVert *v, float out[3])
    {
        out[0] = v->no[0];
        out[1] = v->no[1];
        out[2] = v->no[2];
    }

    /* BMHeader is the first field of every BM element type, so a type-erased
     * element pointer can be read as a BMHeader directly. */
    int bms_elem_htype(const void *elem)
    {
        return static_cast<const BMHeader *>(elem)->htype;
    }
    int bms_elem_hflag(const void *elem)
    {
        return static_cast<const BMHeader *>(elem)->hflag;
    }
    int bms_elem_api_flag(const void *elem)
    {
        return static_cast<const BMHeader *>(elem)->api_flag;
    }

    /* OR / AND-NOT / XOR a single hflag bit on `head.hflag`. Accepts any
     * BM element pointer (BMHeader is the first field of every element). */
    void bms_elem_set_hflag(void *elem, int hflag_bit)
    {
        static_cast<BMHeader *>(elem)->hflag |= static_cast<char>(hflag_bit);
    }
    void bms_elem_clear_hflag(void *elem, int hflag_bit)
    {
        static_cast<BMHeader *>(elem)->hflag &= static_cast<char>(~hflag_bit);
    }
    void bms_elem_toggle_hflag(void *elem, int hflag_bit)
    {
        static_cast<BMHeader *>(elem)->hflag ^= static_cast<char>(hflag_bit);
    }

    /* BMFace.mat_nr is a public field; expose it directly. */
    short bms_face_get_mat_nr(const BMFace *f)
    {
        return f->mat_nr;
    }
    void bms_face_set_mat_nr(BMFace *f, short mat_nr)
    {
        f->mat_nr = mat_nr;
    }

    BMEdge *bms_edge_create(BMesh *bm, BMVert *v1, BMVert *v2, bool no_double)
    {
        eBMCreateFlag flag = no_double ? BM_CREATE_NO_DOUBLE : BM_CREATE_NOP;
        return BM_edge_create(bm, v1, v2, nullptr, flag);
    }

    BMFace *bms_face_create_verts(BMesh *bm, BMVert **verts, int len, bool no_double)
    {
        eBMCreateFlag flag = no_double ? BM_CREATE_NO_DOUBLE : BM_CREATE_NOP;
        return BM_face_create_verts(bm, verts, len, nullptr, flag, true);
    }

    void bms_vert_kill(BMesh *bm, BMVert *v) { BM_vert_kill(bm, v); }
    void bms_edge_kill(BMesh *bm, BMEdge *e) { BM_edge_kill(bm, e); }
    void bms_face_kill(BMesh *bm, BMFace *f) { BM_face_kill(bm, f); }

    bool bms_vert_splice(BMesh *bm, BMVert *v_dst, BMVert *v_src)
    {
        return BM_vert_splice(bm, v_dst, v_src);
    }

    bool bms_edge_splice(BMesh *bm, BMEdge *e_dst, BMEdge *e_src)
    {
        return BM_edge_splice(bm, e_dst, e_src);
    }

    BMFace *bms_sfme(BMesh *bm, BMFace *f, BMLoop *l_v1, BMLoop *l_v2)
    {
        BMLoop *r_l = nullptr;
        return bmesh_kernel_split_face_make_edge(bm, f, l_v1, l_v2, &r_l, nullptr, false);
    }

    BMVert *bms_semv(BMesh *bm, BMVert *tv, BMEdge *e, BMEdge **r_e)
    {
        return bmesh_kernel_split_edge_make_vert(bm, tv, e, r_e);
    }

    BMEdge *bms_jekv(BMesh *bm, BMEdge *e_kill, BMVert *v_kill)
    {
        return bmesh_kernel_join_edge_kill_vert(bm, e_kill, v_kill,
                                                /*do_del=*/true,
                                                /*check_edge_exists=*/false,
                                                /*kill_degenerate_faces=*/false,
                                                /*kill_duplicate_faces=*/false);
    }

    BMVert *bms_jvke(BMesh *bm, BMEdge *e_kill, BMVert *v_kill)
    {
        return bmesh_kernel_join_vert_kill_edge(bm, e_kill, v_kill,
                                                /*do_del=*/true,
                                                /*check_edge_exists=*/false,
                                                /*kill_degenerate_faces=*/false);
    }

    BMFace *bms_jfke(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e)
    {
        return bmesh_kernel_join_face_kill_edge(bm, f1, f2, e);
    }

    void bms_loop_reverse(BMesh *bm, BMFace *f)
    {
        bmesh_kernel_loop_reverse(bm, f, /*cd_loop_mdisp_offset=*/-1, /*use_loop_mdisp_flip=*/false);
    }

    BMEdge *bms_edge_separate(BMesh *bm, BMEdge *e, BMLoop *l_sep)
    {
        bmesh_kernel_edge_separate(bm, e, l_sep, /*copy_select=*/false);
        return l_sep->e;
    }

    /* Query helpers used by the A/B comparison harness to identify the
     * "equivalent" element in bmesh given fixture-level vertex/edge handles. */

    BMEdge *bms_edge_exists(BMVert *v1, BMVert *v2)
    {
        return BM_edge_exists(v1, v2);
    }

    BMLoop *bms_face_vert_share_loop(BMFace *f, BMVert *v)
    {
        return BM_face_vert_share_loop(f, v);
    }

    BMLoop *bms_face_edge_share_loop(BMFace *f, BMEdge *e)
    {
        return BM_face_edge_share_loop(f, e);
    }

    BMLoop *bms_face_first_loop(BMFace *f)
    {
        return f ? f->l_first : nullptr;
    }

    BMLoop *bms_loop_next(BMLoop *l)
    {
        return l ? l->next : nullptr;
    }

    BMVert *bms_loop_vert(BMLoop *l)
    {
        return l ? l->v : nullptr;
    }

    /* High-level wrappers. */

    /* Returns the count of resulting vertices (1 + number of newly-created
     * fans). If `out_count` is non-null and `out_verts_cap` is large enough,
     * fills out_verts with the resulting vertex pointers (first = input v). */
    BMVert *bms_unglue_region_make_vert(BMesh *bm, BMLoop *l_sep)
    {
        return bmesh_kernel_unglue_region_make_vert(bm, l_sep);
    }

    int bms_vert_separate(BMesh *bm, BMVert *v, BMVert **out_verts, int out_verts_cap)
    {
        BMVert **r_vout = nullptr;
        int r_vout_len = 0;
        bmesh_kernel_vert_separate(bm, v, &r_vout, &r_vout_len, /*copy_select=*/false);
        if (out_verts && out_verts_cap >= r_vout_len)
        {
            for (int i = 0; i < r_vout_len; i++)
                out_verts[i] = r_vout[i];
        }
        /* r_vout was allocated via MEM_new_array_uninitialized inside bmesh.
         * For typed pointers, MEM_delete is the matching free. */
        if (r_vout)
            MEM_delete(r_vout);
        return r_vout_len;
    }

    BMVert *bms_edge_collapse(BMesh *bm, BMEdge *e_kill, BMVert *v_kill)
    {
        return BM_edge_collapse(bm, e_kill, v_kill,
                                /*do_del=*/true,
                                /*kill_degenerate_faces=*/false);
    }

    BMFace *bms_faces_join(BMesh *bm, BMFace **faces, int totface)
    {
        BMFace *r_double = nullptr;
        return BM_faces_join(bm, faces, totface, /*do_del=*/true, &r_double);
    }

    BMEdge *bms_edge_rotate(BMesh *bm, BMEdge *e, bool ccw)
    {
        /* check_flag = 0 means do the rotation without optional beauty / exists /
         * degenerate checks. Callers that need those guards should perform them
         * before invoking this shim. */
        return BM_edge_rotate(bm, e, ccw, 0);
    }

    /* --- Phase M: subdivision basics --- */

    BMVert *bms_edge_split(BMesh *bm, BMEdge *e, BMVert *v, float fac, BMEdge **r_e)
    {
        return BM_edge_split(bm, e, v, r_e, fac);
    }

    /* numcuts new verts are written into r_varr[0..numcuts]; the caller is
     * responsible for sizing the buffer. Returns the count actually written
     * (== numcuts on success, 0 if numcuts == 0). */
    int bms_edge_split_n(BMesh *bm, BMEdge *e, int numcuts, BMVert **r_varr)
    {
        if (numcuts <= 0)
            return 0;
        BM_edge_split_n(bm, e, numcuts, r_varr);
        return numcuts;
    }

    /* Dissolve a vertex (or no-op if precondition fails). bmesh's
     * BM_vert_dissolve handles every case (isolated, wire, boundary, manifold). */
    bool bms_vert_dissolve(BMesh *bm, BMVert *v)
    {
        return BM_vert_dissolve(bm, v);
    }

    /* Face poke: insert a centre vertex at the face's median position and
     * fan-triangulate. The bmesh tools/ tree's BM_face_poke isn't vendored, so
     * we hand-compose using BM_face_create_verts after killing the original.
     * Returns the new centre vertex. */
    BMVert *bms_face_poke(BMesh *bm, BMFace *face)
    {
        int n = face->len;
        if (n < 3)
            return nullptr;

        /* Snapshot perimeter vertices in face-cycle order. */
        BMVert *verts[32]; /* matches BM_DEFAULT_NGON_STACK_SIZE patterns */
        if (n > (int)(sizeof(verts) / sizeof(verts[0])))
            return nullptr;
        /* Snapshot the per-corner loop customdata blocks alongside the
         * vertices. BM_face_kill frees the source face's loop (and face)
         * customdata, so the fan triangles built afterwards have to inherit
         * from copies taken now. CustomData_bmesh_copy_block allocates a
         * fresh pool block (dst starts null) and clones every layer into it. */
        void *loop_cd[32];
        for (int i = 0; i < (int)(sizeof(loop_cd) / sizeof(loop_cd[0])); i++)
            loop_cd[i] = nullptr;
        BMLoop *l_first = BM_FACE_FIRST_LOOP(face);
        BMLoop *l_iter = l_first;
        int idx = 0;
        do
        {
            verts[idx] = l_iter->v;
            CustomData_bmesh_copy_block(bm->ldata, l_iter->head.data, &loop_cd[idx]);
            idx++;
            l_iter = l_iter->next;
        } while (l_iter != l_first);

        /* Snapshot the source face's face-level customdata block too. */
        void *face_cd = nullptr;
        CustomData_bmesh_copy_block(bm->pdata, face->head.data, &face_cd);

        /* Median centre. */
        float center[3] = {0.0f, 0.0f, 0.0f};
        for (int i = 0; i < n; i++)
        {
            center[0] += verts[i]->co[0];
            center[1] += verts[i]->co[1];
            center[2] += verts[i]->co[2];
        }
        center[0] /= (float)n;
        center[1] /= (float)n;
        center[2] /= (float)n;

        /* Kill original face. This frees the source loop/face customdata,
         * which is why the snapshots above were taken first. */
        BM_face_kill(bm, face);

        /* Build the centre vertex. */
        BMVert *v_center = BM_vert_create(bm, center, nullptr, BM_CREATE_NOP);

        /* Interpolate vertex customdata onto v_center as the uniform average
         * of every perimeter vertex — matches BMesh's `BM_face_poke` semantics
         * (the higher-level `tools/` helper that performs poking with CD
         * interpolation is not part of the vendored set, so we replicate the
         * weighted interp here). */
        const void *src_blocks[32];
        float weights[32];
        float w = 1.0f / (float)n;
        for (int i = 0; i < n; i++)
        {
            src_blocks[i] = verts[i]->head.data;
            weights[i] = w;
        }
        CustomData_bmesh_interp(&bm->vdata, src_blocks, weights, n, v_center->head.data);

        /* Build N triangle faces. BM_face_create_verts auto-creates the
         * perimeter edges as needed (the v_center to v edges are created
         * lazily here too — face #0 creates the first two; each subsequent
         * face reuses one v_center edge and creates one new one).
         *
         * Each fan triangle inherits the source face's customdata: the new
         * face takes the source face-CD, its two perimeter corner loops take
         * the loop-CD of the matching source corners, and its apex (centre)
         * loop takes the uniform-average interpolation of every source corner
         * loop — consistent with the centre vertex interp above. The faces are
         * created with a null example, so their CD blocks start at the layer
         * defaults and are overwritten in place here. */
        for (int i = 0; i < n; i++)
        {
            int j = (i + 1) % n;
            BMVert *tri[3] = {verts[i], verts[j], v_center};
            BMFace *f_new = BM_face_create_verts(bm, tri, 3, nullptr, BM_CREATE_NO_DOUBLE, true);
            if (!f_new)
                continue;

            CustomData_bmesh_copy_block(bm->pdata, face_cd, &f_new->head.data);

            BMLoop *l_i = BM_face_vert_share_loop(f_new, verts[i]);
            BMLoop *l_j = BM_face_vert_share_loop(f_new, verts[j]);
            BMLoop *l_c = BM_face_vert_share_loop(f_new, v_center);
            if (l_i)
                CustomData_bmesh_copy_block(bm->ldata, loop_cd[i], &l_i->head.data);
            if (l_j)
                CustomData_bmesh_copy_block(bm->ldata, loop_cd[j], &l_j->head.data);
            if (l_c)
            {
                const void *loop_blocks[32];
                for (int k = 0; k < n; k++)
                    loop_blocks[k] = loop_cd[k];
                CustomData_bmesh_interp(&bm->ldata, loop_blocks, weights, n, l_c->head.data);
            }
        }

        /* Release the snapshot blocks taken before the kill. */
        for (int i = 0; i < n; i++)
            CustomData_bmesh_free_block(&bm->ldata, &loop_cd[i]);
        CustomData_bmesh_free_block(&bm->pdata, &face_cd);

        return v_center;
    }

    /* Face-split-N. Finds the loops in `face` at vertices `v1` and `v2`, then
     * delegates to BM_face_split_n which splits the face and inserts `n`
     * intermediate vertices along the new diagonal at the positions in `cos`.
     *
     * Returns the newly-created face on success, or null on failure (v1 == v2,
     * either vert not on the face, or BM_face_split_n itself failed). */
    BMFace *bms_face_split_n(
        BMesh *bm,
        BMFace *face,
        BMVert *v1,
        BMVert *v2,
        float cos[][3],
        int n)
    {
        if (v1 == v2)
            return nullptr;
        BMLoop *l_a = BM_face_vert_share_loop(face, v1);
        BMLoop *l_b = BM_face_vert_share_loop(face, v2);
        if (!l_a || !l_b)
            return nullptr;
        return BM_face_split_n(bm, face, l_a, l_b, cos, n, nullptr, nullptr);
    }

    int bms_totvert(BMesh *bm) { return bm->totvert; }
    int bms_totedge(BMesh *bm) { return bm->totedge; }
    int bms_totface(BMesh *bm) { return bm->totface; }
    int bms_totloop(BMesh *bm) { return bm->totloop; }

    int bms_mesh_calc_looptri_count(BMesh *bm)
    {
        return poly_to_tri_count(bm->totface, bm->totloop);
    }

    void bms_mesh_calc_tessellation(BMesh *bm, BMLoop *(*out_tris)[3])
    {
        const int n = poly_to_tri_count(bm->totface, bm->totloop);
        if (n <= 0) {
            return;
        }
        blender::MutableSpan<std::array<BMLoop *, 3>> span(
            reinterpret_cast<std::array<BMLoop *, 3> *>(out_tris), n);
        BM_mesh_calc_tessellation(bm, span);
    }

    void bms_face_triangulate(BMesh *bm,
                              BMFace *f,
                              int quad_method,
                              int ngon_method,
                              bool use_tag,
                              BMFace **r_faces_new,
                              int *r_faces_new_tot)
    {
        MemArena *pf_arena = BLI_memarena_new(BLI_POLYFILL_ARENA_SIZE, __func__);
        Heap *pf_heap = nullptr;
        if (ngon_method == MOD_TRIANGULATE_NGON_BEAUTY) {
            pf_heap = BLI_heap_new_ex(BLI_POLYFILL_ALLOC_NGON_RESERVE);
        }

        BM_face_triangulate(bm,
                            f,
                            r_faces_new,
                            r_faces_new_tot,
                            /*r_edges_new=*/nullptr,
                            /*r_edges_new_tot=*/nullptr,
                            /*r_faces_double=*/nullptr,
                            quad_method,
                            ngon_method,
                            use_tag,
                            pf_arena,
                            pf_heap);

        BLI_memarena_free(pf_arena);
        if (pf_heap) {
            BLI_heap_free(pf_heap, nullptr);
        }
    }

    bool bms_snapshot(BMesh *bm,
                      float *out_verts, int out_verts_cap,
                      int *out_edges, int out_edges_cap,
                      int *out_face_offsets, int out_face_offsets_cap,
                      int *out_face_verts, int out_face_verts_cap,
                      int *out_face_lens, int out_face_lens_cap,
                      float *out_face_normals /* nullable */,
                      int out_face_normals_cap)
    {
        if (out_verts_cap < bm->totvert * 3)
            return false;
        if (out_edges_cap < bm->totedge * 2)
            return false;
        if (out_face_offsets_cap < bm->totface)
            return false;
        if (out_face_lens_cap < bm->totface)
            return false;
        if (out_face_normals && out_face_normals_cap < bm->totface * 3)
            return false;

        BMVert *v;
        BMIter iter;
        int vi = 0;
        BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH)
        {
            out_verts[vi * 3 + 0] = v->co[0];
            out_verts[vi * 3 + 1] = v->co[1];
            out_verts[vi * 3 + 2] = v->co[2];
            BM_elem_index_set(v, vi);
            vi++;
        }

        BMEdge *e;
        int ei = 0;
        BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH)
        {
            out_edges[ei * 2 + 0] = BM_elem_index_get(e->v1);
            out_edges[ei * 2 + 1] = BM_elem_index_get(e->v2);
            ei++;
        }

        BMFace *f;
        int fi = 0;
        int fv_cursor = 0;
        BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH)
        {
            out_face_offsets[fi] = fv_cursor;
            out_face_lens[fi] = f->len;
            if (fv_cursor + f->len > out_face_verts_cap)
                return false;
            if (out_face_normals)
            {
                out_face_normals[fi * 3 + 0] = f->no[0];
                out_face_normals[fi * 3 + 1] = f->no[1];
                out_face_normals[fi * 3 + 2] = f->no[2];
            }
            BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
            BMLoop *l = l_first;
            do
            {
                out_face_verts[fv_cursor++] = BM_elem_index_get(l->v);
                l = l->next;
            } while (l != l_first);
            fi++;
        }

        return true;
    }

    /* Update every face's stored normal (f->no) using bmesh's own recompute.
     * We avoid BM_mesh_normals_update because it pulls in TBB / loop-normal
     * machinery; instead walk faces and call BM_face_normal_update. */
    void bms_mesh_normals_update(BMesh *bm)
    {
        BMFace *f;
        BMIter iter;
        BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH)
        {
            BM_face_normal_update(f);
        }
        /* Skip vertex normals on the bmesh side — the canonical comparison
         * only checks face normals; bmesh's BM_vert_normal_update would
         * require additional link-time symbols (BLI_task_*) for the parallel
         * driver. */
    }

    /* ---- Customdata layer access ---- */
    /*
     * The bms_*_layer_add_* functions register a per-element CD layer on
     * the matching domain (vdata / edata / ldata / pdata) and return the byte
     * offset of the new layer inside an element's customdata block. The
     * bms_elem_get_* / bms_elem_set_* helpers read/write the typed value
     * at that offset given an element pointer (any of BMVert / BMEdge / BMLoop /
     * BMFace, since BMHeader is uniformly the first field; passed as void*).
     */

    static blender::CustomData *bms_cd_for_domain(BMesh *bm, int domain)
    {
        switch (domain)
        {
        case 0:
            return &bm->vdata;
        case 1:
            return &bm->edata;
        case 2:
            return &bm->ldata;
        case 3:
            return &bm->pdata;
        default:
            return nullptr;
        }
    }

    /* Add a named layer of the given CD type and return its byte offset.
     * `type_int` is an `eCustomDataType` cast to int. */
    int bms_layer_add_named(BMesh *bm, int domain, int type_int, const char *name)
    {
        blender::CustomData *cd = bms_cd_for_domain(bm, domain);
        if (!cd)
            return -1;
        auto type = static_cast<blender::eCustomDataType>(type_int);
        blender::BM_data_layer_add_named(bm, cd, type, name);
        return blender::CustomData_get_offset_named(cd, type, name);
    }

    /* Look up an existing named layer's byte offset without adding. */
    int bms_layer_find_offset_named(BMesh *bm, int domain, int type_int, const char *name)
    {
        blender::CustomData *cd = bms_cd_for_domain(bm, domain);
        if (!cd)
            return -1;
        return blender::CustomData_get_offset_named(
            cd, static_cast<blender::eCustomDataType>(type_int), name);
    }

    /* Element pointer is type-erased. BMHeader is the first field of every
     * BM element type; we only access head.data. */
    static inline void *bms_elem_cd_ptr(void *elem, int offset)
    {
        blender::BMHeader *head = static_cast<blender::BMHeader *>(elem);
        return static_cast<char *>(head->data) + offset;
    }

    void bms_elem_get_float(void *elem, int offset, float *out)
    {
        const float *p = static_cast<const float *>(bms_elem_cd_ptr(elem, offset));
        *out = *p;
    }

    void bms_elem_set_float(void *elem, int offset, float value)
    {
        *static_cast<float *>(bms_elem_cd_ptr(elem, offset)) = value;
    }

    void bms_elem_get_float2(void *elem, int offset, float out[2])
    {
        const float *p = static_cast<const float *>(bms_elem_cd_ptr(elem, offset));
        out[0] = p[0];
        out[1] = p[1];
    }

    void bms_elem_set_float2(void *elem, int offset, const float in[2])
    {
        float *p = static_cast<float *>(bms_elem_cd_ptr(elem, offset));
        p[0] = in[0];
        p[1] = in[1];
    }

    void bms_elem_get_float3(void *elem, int offset, float out[3])
    {
        const float *p = static_cast<const float *>(bms_elem_cd_ptr(elem, offset));
        out[0] = p[0];
        out[1] = p[1];
        out[2] = p[2];
    }

    void bms_elem_set_float3(void *elem, int offset, const float in[3])
    {
        float *p = static_cast<float *>(bms_elem_cd_ptr(elem, offset));
        p[0] = in[0];
        p[1] = in[1];
        p[2] = in[2];
    }

    void bms_elem_get_float4(void *elem, int offset, float out[4])
    {
        const float *p = static_cast<const float *>(bms_elem_cd_ptr(elem, offset));
        out[0] = p[0];
        out[1] = p[1];
        out[2] = p[2];
        out[3] = p[3];
    }

    void bms_elem_set_float4(void *elem, int offset, const float in[4])
    {
        float *p = static_cast<float *>(bms_elem_cd_ptr(elem, offset));
        p[0] = in[0];
        p[1] = in[1];
        p[2] = in[2];
        p[3] = in[3];
    }

    void bms_elem_get_int(void *elem, int offset, int *out)
    {
        *out = *static_cast<const int *>(bms_elem_cd_ptr(elem, offset));
    }

    void bms_elem_set_int(void *elem, int offset, int value)
    {
        *static_cast<int *>(bms_elem_cd_ptr(elem, offset)) = value;
    }

    /* ---- Bulk CD layer read-back ---- */
    /*
     * `bms_layer_read_floats` / `bms_layer_read_ints` walk every element of
     * `domain` in `BM_ITER_MESH` order and write the CD value at the named
     * `offset` into a flat output buffer. The iteration order matches what
     * `bms_snapshot` uses for vertex / edge / face indices, so callers can
     * align the per-element values one-to-one with the snapshot's element
     * lists. For loops the per-face iteration is the face's existing loop
     * cycle starting at `BM_FACE_FIRST_LOOP`, matching `bms_snapshot`'s
     * `face_verts` layout exactly (one loop value per `face_verts` entry).
     */

    int bms_domain_elem_count(BMesh *bm, int domain)
    {
        switch (domain)
        {
        case 0:
            return bm->totvert;
        case 1:
            return bm->totedge;
        case 2:
            return bm->totloop;
        case 3:
            return bm->totface;
        default:
            return -1;
        }
    }

    static void bms_copy_floats_at(const void *elem,
                                   int offset,
                                   int components,
                                   float *dst)
    {
        const blender::BMHeader *head = static_cast<const blender::BMHeader *>(elem);
        const float *src = reinterpret_cast<const float *>(
            static_cast<const char *>(head->data) + offset);
        for (int c = 0; c < components; c++)
        {
            dst[c] = src[c];
        }
    }

    bool bms_layer_read_floats(BMesh *bm,
                               int domain,
                               int offset,
                               int components,
                               float *out_floats,
                               int out_floats_cap)
    {
        if (components < 1 || components > 4)
            return false;

        const int n = bms_domain_elem_count(bm, domain);
        if (n < 0)
            return false;
        const int total_floats = n * components;
        if (out_floats_cap < total_floats)
            return false;

        BMIter iter;
        switch (domain)
        {
        case 0:
        {
            BMVert *v;
            int vi = 0;
            BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH)
            {
                bms_copy_floats_at(v, offset, components, &out_floats[vi * components]);
                vi++;
            }
            return true;
        }
        case 1:
        {
            BMEdge *e;
            int ei = 0;
            BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH)
            {
                bms_copy_floats_at(e, offset, components, &out_floats[ei * components]);
                ei++;
            }
            return true;
        }
        case 2:
        {
            /* Loops: iterate faces, then each face's loop cycle starting at
             * BM_FACE_FIRST_LOOP — matches `bms_snapshot`'s face_verts order. */
            BMFace *f;
            int li = 0;
            BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH)
            {
                BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
                BMLoop *l = l_first;
                do
                {
                    bms_copy_floats_at(l, offset, components, &out_floats[li * components]);
                    li++;
                    l = l->next;
                } while (l != l_first);
            }
            return true;
        }
        case 3:
        {
            BMFace *f;
            int fi = 0;
            BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH)
            {
                bms_copy_floats_at(f, offset, components, &out_floats[fi * components]);
                fi++;
            }
            return true;
        }
        default:
            return false;
        }
    }

    bool bms_layer_read_ints(BMesh *bm,
                             int domain,
                             int offset,
                             int *out_ints,
                             int out_ints_cap)
    {
        const int n = bms_domain_elem_count(bm, domain);
        if (n < 0)
            return false;
        if (out_ints_cap < n)
            return false;

        BMIter iter;
        switch (domain)
        {
        case 0:
        {
            BMVert *v;
            int vi = 0;
            BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH)
            {
                blender::BMHeader *head = reinterpret_cast<blender::BMHeader *>(v);
                out_ints[vi++] = *reinterpret_cast<const int *>(
                    static_cast<const char *>(head->data) + offset);
            }
            return true;
        }
        case 1:
        {
            BMEdge *e;
            int ei = 0;
            BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH)
            {
                blender::BMHeader *head = reinterpret_cast<blender::BMHeader *>(e);
                out_ints[ei++] = *reinterpret_cast<const int *>(
                    static_cast<const char *>(head->data) + offset);
            }
            return true;
        }
        case 2:
        {
            BMFace *f;
            int li = 0;
            BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH)
            {
                BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
                BMLoop *l = l_first;
                do
                {
                    blender::BMHeader *head = reinterpret_cast<blender::BMHeader *>(l);
                    out_ints[li++] = *reinterpret_cast<const int *>(
                        static_cast<const char *>(head->data) + offset);
                    l = l->next;
                } while (l != l_first);
            }
            return true;
        }
        case 3:
        {
            BMFace *f;
            int fi = 0;
            BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH)
            {
                blender::BMHeader *head = reinterpret_cast<blender::BMHeader *>(f);
                out_ints[fi++] = *reinterpret_cast<const int *>(
                    static_cast<const char *>(head->data) + offset);
            }
            return true;
        }
        default:
            return false;
        }
    }

    /* ---- Extrude (BMesh operator: extrude_face_region) ---- */
    /*
     * Marks each input face with BM_ELEM_TAG (after clearing TAG on every other
     * face), then invokes the `extrude_face_region` operator with the tagged
     * face set as input.
     *
     * This shim hardcodes `use_keep_orig=false` and additionally kills each
     * input face after the op completes — see the comment below for the
     * rationale. If you need access to `use_keep_orig=true` or to the
     * "originals preserved alongside duplicates" behaviour BMesh exposes for
     * isolated / boundary inputs, this shim is too opinionated; call
     * `BMO_op_initf` directly or add a wider-surface helper.
     *
     * `use_normal_flip` is forwarded verbatim to the operator. When true it
     * reverses the winding of the side (wall) faces built between the original
     * boundary and the lifted duplicate. It does not affect the post-op
     * normalisation below.
     *
     * Caller is responsible for displacing the new verts after the call.
     *
     * Returns true on success, false if BMO_op_initf rejected the input.
     */
    bool bms_extrude_face_region_ex(BMesh *bm, BMFace **faces, int faces_len,
                                    bool use_normal_flip)
    {
        using namespace blender;
        BMIter it;
        BMFace *f;
        BM_ITER_MESH(f, &it, bm, BM_FACES_OF_MESH)
        {
            BM_elem_flag_disable(f, BM_ELEM_TAG);
        }
        for (int i = 0; i < faces_len; i++)
        {
            BM_elem_flag_enable(faces[i], BM_ELEM_TAG);
        }

        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "extrude_face_region geom=%hf use_keep_orig=%b use_normal_flip=%b",
                          BM_ELEM_TAG,
                          false,
                          use_normal_flip))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);

        // BM_op_extrude_face_region with use_keep_orig=false only deletes
        // selection-interior edges / verts / faces; isolated or boundary
        // input faces are left in place alongside their duplicates. This
        // shim normalises to a single "every input face is replaced by its
        // duplicate" outcome by explicitly killing the original input faces
        // here. BLI_mempool slots are stable across the op (the BMesh
        // operator does not free anything we passed in via the input slot),
        // so the pointers in `faces` are still live and safe to BM_face_kill.
        for (int i = 0; i < faces_len; i++)
        {
            BM_face_kill(bm, faces[i]);
        }
        return true;
    }

    bool bms_extrude_face_region(BMesh *bm, BMFace **faces, int faces_len)
    {
        return bms_extrude_face_region_ex(bm, faces, faces_len, false);
    }

    /* ---- Extrude (BMesh operator: extrude_discrete_faces) ---- */
    /*
     * Marks each input face with BM_ELEM_TAG (after clearing TAG on every other
     * face), then invokes the `extrude_discrete_faces` operator with the tagged
     * face set as input.
     *
     * Each input face is extruded independently: two formerly-adjacent input
     * faces split apart along their shared edge instead of lifting as a single
     * connected region (the perimeter between them becomes a doubled wall).
     *
     * Unlike the region extrude shim, no post-op kill is performed. This
     * operator already deletes the original faces internally (DEL_ONLYFACES),
     * leaving their edges and verts as the bottoms of the new wall quads, so
     * every input face ends up replaced by its lifted duplicate.
     *
     * `use_select_history` is left at its default (false); it only affects
     * editor selection state. `use_normal_flip` is forwarded verbatim; when
     * true it reverses the winding of the side (wall) faces.
     *
     * Caller is responsible for displacing the new verts after the call.
     *
     * Returns true on success, false if BMO_op_initf rejected the input.
     */
    bool bms_extrude_discrete_faces_ex(BMesh *bm, BMFace **faces, int faces_len,
                                       bool use_normal_flip)
    {
        using namespace blender;
        BMIter it;
        BMFace *f;
        BM_ITER_MESH(f, &it, bm, BM_FACES_OF_MESH)
        {
            BM_elem_flag_disable(f, BM_ELEM_TAG);
        }
        for (int i = 0; i < faces_len; i++)
        {
            BM_elem_flag_enable(faces[i], BM_ELEM_TAG);
        }

        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "extrude_discrete_faces faces=%hf use_normal_flip=%b",
                          BM_ELEM_TAG,
                          use_normal_flip))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_extrude_discrete_faces(BMesh *bm, BMFace **faces, int faces_len)
    {
        return bms_extrude_discrete_faces_ex(bm, faces, faces_len, false);
    }

    /* ---- Inset (BMesh operators: inset_region / inset_individual) ---- */
    /*
     * Both inset variants are exposed as one shim each. The `faces` set is
     * passed by clearing BM_ELEM_TAG on every face, enabling it on the input
     * set, and using "%hf" in the format string. inset_region additionally
     * accepts a `faces_exclude` set, filled into its slot directly via
     * BMO_slot_buffer_from_array (so the two face buffers don't have to share
     * a single tag bit).
     *
     * Every operator parameter is forwarded explicitly so A/B tests can pin
     * each parameter axis. The shims do not modify the input face set after
     * the call — unlike extrude, neither inset variant duplicates the input
     * faces in a way that requires the original to be killed.
     */
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
                          float depth)
    {
        BMIter it;
        BMFace *f;
        BM_ITER_MESH(f, &it, bm, BM_FACES_OF_MESH)
        {
            BM_elem_flag_disable(f, BM_ELEM_TAG);
            /* Inset's offset solver reads face normals; freshly-built test
             * meshes have stale (zero) normals until we refresh them. */
            BM_face_normal_update(f);
        }
        for (int i = 0; i < faces_len; i++)
        {
            BM_elem_flag_enable(faces[i], BM_ELEM_TAG);
        }

        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "inset_region faces=%hf use_boundary=%b "
                          "use_even_offset=%b use_interpolate=%b "
                          "use_relative_offset=%b use_edge_rail=%b "
                          "use_outset=%b thickness=%f depth=%f",
                          BM_ELEM_TAG,
                          use_boundary,
                          use_even_offset,
                          use_interpolate,
                          use_relative_offset,
                          use_edge_rail,
                          use_outset,
                          double(thickness),
                          double(depth)))
        {
            return false;
        }

        if (faces_exclude && faces_exclude_len > 0)
        {
            BMOpSlot *slot = BMO_slot_get(op.slots_in, "faces_exclude");
            BMO_slot_buffer_from_array(
                &op, slot, reinterpret_cast<BMHeader **>(faces_exclude), faces_exclude_len);
        }

        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_inset_individual(BMesh *bm,
                              BMFace **faces, int faces_len,
                              bool use_even_offset,
                              bool use_interpolate,
                              bool use_relative_offset,
                              float thickness,
                              float depth)
    {
        BMIter it;
        BMFace *f;
        BM_ITER_MESH(f, &it, bm, BM_FACES_OF_MESH)
        {
            BM_elem_flag_disable(f, BM_ELEM_TAG);
            BM_face_normal_update(f);
        }
        for (int i = 0; i < faces_len; i++)
        {
            BM_elem_flag_enable(faces[i], BM_ELEM_TAG);
        }

        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "inset_individual faces=%hf "
                          "use_even_offset=%b use_interpolate=%b "
                          "use_relative_offset=%b thickness=%f depth=%f",
                          BM_ELEM_TAG,
                          use_even_offset,
                          use_interpolate,
                          use_relative_offset,
                          double(thickness),
                          double(depth)))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    /* ---- Dissolve (BMesh operators: dissolve_verts / dissolve_edges) ---- */
    /*
     * Both dissolve variants are exposed via the BMOP slot-form (using "%eb" /
     * "%vb" with a raw `(BMHeader **, int)` element buffer), since the BMesh
     * public API does not provide a convenience C wrapper that surfaces the
     * full parameter set — in particular, `angle_threshold` on `dissolve_edges`
     * is only reachable through the BMOP. The shims simply translate the
     * caller's element array into the operator's input slot, run the op, and
     * tear it down; the input pointers themselves are not modified.
     */
    bool bms_dissolve_verts(BMesh *bm,
                            BMVert **verts, int verts_len,
                            bool use_face_split,
                            bool use_boundary_tear)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "dissolve_verts verts=%eb "
                          "use_face_split=%b use_boundary_tear=%b",
                          reinterpret_cast<BMHeader **>(verts),
                          verts_len,
                          use_face_split,
                          use_boundary_tear))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_dissolve_edges(BMesh *bm,
                            BMEdge **edges, int edges_len,
                            bool use_verts,
                            bool use_face_split,
                            float angle_threshold)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "dissolve_edges edges=%eb "
                          "use_verts=%b use_face_split=%b angle_threshold=%f",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          use_verts,
                          use_face_split,
                          double(angle_threshold)))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_dissolve_faces(BMesh *bm,
                            BMFace **faces, int faces_len,
                            bool use_verts)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "dissolve_faces faces=%eb use_verts=%b",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len,
                          use_verts))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    int bms_dissolve_faces_out(BMesh *bm,
                               BMFace **faces, int faces_len,
                               bool use_verts,
                               BMFace **out_buf, int out_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "dissolve_faces faces=%eb use_verts=%b",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len,
                          use_verts))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *slot = BMO_slot_get(op.slots_out, "region.out");
        const int n = slot->len;
        const int n_copy = (n < out_cap) ? n : out_cap;
        BMFace **slot_items = reinterpret_cast<BMFace **>(slot->data.buf);
        for (int i = 0; i < n_copy; ++i)
        {
            out_buf[i] = slot_items[i];
        }

        BMO_op_finish(bm, &op);
        return n;
    }

    bool bms_dissolve_limit(BMesh *bm,
                            BMEdge **edges, int edges_len,
                            BMVert **verts, int verts_len,
                            float angle_limit,
                            bool use_dissolve_boundaries,
                            int delimit)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "dissolve_limit edges=%eb verts=%eb "
                          "angle_limit=%f use_dissolve_boundaries=%b delimit=%i",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          reinterpret_cast<BMHeader **>(verts),
                          verts_len,
                          double(angle_limit),
                          use_dissolve_boundaries,
                          delimit))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    int bms_dissolve_limit_out(BMesh *bm,
                               BMEdge **edges, int edges_len,
                               BMVert **verts, int verts_len,
                               float angle_limit,
                               bool use_dissolve_boundaries,
                               int delimit,
                               BMFace **out_buf, int out_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "dissolve_limit edges=%eb verts=%eb "
                          "angle_limit=%f use_dissolve_boundaries=%b delimit=%i",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          reinterpret_cast<BMHeader **>(verts),
                          verts_len,
                          double(angle_limit),
                          use_dissolve_boundaries,
                          delimit))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *slot = BMO_slot_get(op.slots_out, "region.out");
        const int n = slot->len;
        const int n_copy = (n < out_cap) ? n : out_cap;
        BMFace **slot_items = reinterpret_cast<BMFace **>(slot->data.buf);
        for (int i = 0; i < n_copy; ++i)
        {
            out_buf[i] = slot_items[i];
        }

        BMO_op_finish(bm, &op);
        return n;
    }

    bool bms_dissolve_degenerate(BMesh *bm,
                                 BMEdge **edges, int edges_len,
                                 float dist)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "dissolve_degenerate edges=%eb dist=%f",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          double(dist)))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_delete_geom(BMesh *bm,
                         BMHeader **geom, int geom_len,
                         int context)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "delete geom=%eb context=%i",
                          geom,
                          geom_len,
                          context))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

} /* extern "C" */
