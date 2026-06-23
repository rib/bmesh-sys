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
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"
#include "BLI_span.hh"
#include "DNA_customdata_types.h"
#include "DNA_modifier_types.h"
#include "MEM_guardedalloc.h"

#include <array>
#include <cstdint>
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

    /* BMEdge.v1 / BMEdge.v2 are public fields; expose them directly. */
    void bms_edge_verts(BMEdge *e, BMVert **out_v1, BMVert **out_v2)
    {
        *out_v1 = e->v1;
        *out_v2 = e->v2;
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

    /* Compute a poke centre over `n` perimeter vertices in face-cycle order.
     * center_mode selects the formula:
     *   0 = MEAN          arithmetic mean of corner positions.
     *   1 = BOUNDS        per-axis (min + max) * 0.5 over corner positions.
     *   2 = MEAN_WEIGHTED corner positions weighted by the sum of the two
     *                     incident face-edge lengths; falls back to the
     *                     arithmetic mean when the total weight is non-positive.
     * Any other value behaves as MEAN. Ordering matches the poke BMOP's
     * eCenterMode. */
    static void bms_face_poke_center(BMVert *const *verts, int n, int center_mode,
                                     float center[3])
    {
        if (center_mode == 1)
        {
            float vmin[3] = {verts[0]->co[0], verts[0]->co[1], verts[0]->co[2]};
            float vmax[3] = {verts[0]->co[0], verts[0]->co[1], verts[0]->co[2]};
            for (int i = 1; i < n; i++)
            {
                for (int a = 0; a < 3; a++)
                {
                    float c = verts[i]->co[a];
                    if (c < vmin[a])
                        vmin[a] = c;
                    if (c > vmax[a])
                        vmax[a] = c;
                }
            }
            for (int a = 0; a < 3; a++)
                center[a] = (vmin[a] + vmax[a]) * 0.5f;
            return;
        }

        if (center_mode == 2)
        {
            float total = 0.0f;
            float acc[3] = {0.0f, 0.0f, 0.0f};
            for (int i = 0; i < n; i++)
            {
                int next = (i + 1) % n;
                int prev = (i - 1 + n) % n;
                float w = len_v3v3(verts[i]->co, verts[next]->co) +
                          len_v3v3(verts[prev]->co, verts[i]->co);
                acc[0] += verts[i]->co[0] * w;
                acc[1] += verts[i]->co[1] * w;
                acc[2] += verts[i]->co[2] * w;
                total += w;
            }
            if (total > 0.0f)
            {
                center[0] = acc[0] / total;
                center[1] = acc[1] / total;
                center[2] = acc[2] / total;
                return;
            }
            /* Fall through to the arithmetic mean below. */
        }

        center[0] = 0.0f;
        center[1] = 0.0f;
        center[2] = 0.0f;
        for (int i = 0; i < n; i++)
        {
            center[0] += verts[i]->co[0];
            center[1] += verts[i]->co[1];
            center[2] += verts[i]->co[2];
        }
        center[0] /= (float)n;
        center[1] /= (float)n;
        center[2] /= (float)n;
    }

    /* Face poke with selectable centre formula. center_mode picks the poke
     * centre (see bms_face_poke_center); everything else (perimeter/CD
     * snapshot, face kill, centre vertex create, CD interp, fan-triangle
     * build) is identical regardless of mode. The bmesh tools/ tree's
     * BM_face_poke isn't vendored, so we hand-compose using
     * BM_face_create_verts after killing the original. Returns the new centre
     * vertex. */
    BMVert *bms_face_poke_mode(BMesh *bm, BMFace *face, int center_mode)
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

        /* Poke centre, per the selected mode. */
        float center[3];
        bms_face_poke_center(verts, n, center_mode, center);

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

    /* Face poke at the uniform-mean centre (MEAN mode); equivalent to
     * bms_face_poke_mode(bm, face, 0). Returns the new centre vertex. */
    BMVert *bms_face_poke(BMesh *bm, BMFace *face)
    {
        return bms_face_poke_mode(bm, face, 0);
    }

    /* Face poke (see bms_face_poke_mode), but additionally lifts the new
     * centre vertex along the source face's normal.
     *   - center_mode selects the centre formula as in bms_face_poke_mode.
     *   - The source face's normal is recomputed and captured before the
     *     face is killed, then read into a local.
     *   - base_center (the un-lifted centre) is stashed before lifting so the
     *     relative scale uses corner-to-base-centre distances.
     *   - scale = use_relative_offset
     *           ? arithmetic mean over the corners of distance(base_center, co)
     *           : 1.0f
     *   - The lift is applied after the centre vertex's customdata interp
     *     (so interpolation sees the un-lifted position):
     *       v_center->co += face_normal * offset * scale
     * Returns the new centre vertex, or null on failure. */
    BMVert *bms_face_poke_offset(BMesh *bm, BMFace *face, int center_mode,
                                 float offset, bool use_relative_offset)
    {
        int n = face->len;
        if (n < 3)
            return nullptr;

        BMVert *verts[32];
        if (n > (int)(sizeof(verts) / sizeof(verts[0])))
            return nullptr;
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

        void *face_cd = nullptr;
        CustomData_bmesh_copy_block(bm->pdata, face->head.data, &face_cd);

        /* Poke centre, per the selected mode. */
        float center[3];
        bms_face_poke_center(verts, n, center_mode, center);

        /* Stash the un-lifted centre for the relative-scale computation. */
        float base_center[3] = {center[0], center[1], center[2]};

        /* Capture the source face's normal before the face is killed. */
        BM_face_normal_update(face);
        float face_normal[3] = {face->no[0], face->no[1], face->no[2]};

        /* Relative scale: arithmetic mean of corner-to-base-centre distances. */
        float scale = 1.0f;
        if (use_relative_offset)
        {
            float total = 0.0f;
            for (int i = 0; i < n; i++)
                total += len_v3v3(base_center, verts[i]->co);
            scale = total / (float)n;
        }

        BM_face_kill(bm, face);

        BMVert *v_center = BM_vert_create(bm, center, nullptr, BM_CREATE_NOP);

        const void *src_blocks[32];
        float weights[32];
        float w = 1.0f / (float)n;
        for (int i = 0; i < n; i++)
        {
            src_blocks[i] = verts[i]->head.data;
            weights[i] = w;
        }
        CustomData_bmesh_interp(&bm->vdata, src_blocks, weights, n, v_center->head.data);

        /* Apply the lift after customdata interp so interpolation sees the
         * un-lifted centre position. */
        madd_v3_v3fl(v_center->co, face_normal, offset * scale);

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
        /* Vertex normals are computed separately via
         * bms_mesh_vert_normals_update; the whole-mesh BM_mesh_normals_update
         * is avoided because its parallel driver pulls in BLI_task_* symbols. */
    }

    /* Recompute and store every vertex normal (v->no) for the whole mesh.
     * Each vertex normal is built serially from its incident faces (whose
     * normals are refreshed in the same pass), so no task-scheduler symbols
     * are required. */
    void bms_mesh_vert_normals_update(BMesh *bm)
    {
        BMVert *v;
        BMIter iter;
        BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH)
        {
            BM_vert_normal_update_all(v);
        }
    }

    /* Copy each vertex's stored normal (v->no) into out_vert_normals as 3
     * floats per vert, in BM_VERTS_OF_MESH iteration order (the same order
     * bms_snapshot writes vertex positions). Does not recompute normals;
     * the caller runs bms_mesh_vert_normals_update first. Returns the true
     * vertex count. When out_cap is too small (< 3 * totvert) nothing is
     * written and the true count is still returned so callers can detect
     * truncation. */
    int bms_vert_normals_read(BMesh *bm, float *out_vert_normals, int out_cap)
    {
        int totvert = bm->totvert;
        if (out_cap < totvert * 3)
            return totvert;

        BMVert *v;
        BMIter iter;
        int vi = 0;
        BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH)
        {
            out_vert_normals[vi * 3 + 0] = v->no[0];
            out_vert_normals[vi * 3 + 1] = v->no[1];
            out_vert_normals[vi * 3 + 2] = v->no[2];
            vi++;
        }
        return vi;
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

    bool bms_extrude_face_region_exclude(BMesh *bm,
                                         BMFace **faces, int faces_len,
                                         BMEdge **edges_exclude, int edges_exclude_len,
                                         bool use_keep_orig,
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
                          use_keep_orig,
                          use_normal_flip))
        {
            return false;
        }

        if (edges_exclude && edges_exclude_len > 0)
        {
            BMOpSlot *slot = BMO_slot_get(op.slots_in, "edges_exclude");
            for (int i = 0; i < edges_exclude_len; i++)
            {
                BMO_slot_map_insert(&op, slot, edges_exclude[i], nullptr);
            }
        }

        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    /*
     * Extrude over the operator's native mixed `geom` element buffer.
     *
     * Unlike the `%hf` (faces-by-hflag) variants above, this seeds the
     * `geom` slot with a type-erased element buffer (`%eb`), so the input
     * may freely mix verts, edges, and faces in a single call. The operator
     * routes each element kind on its own: faces drive a connected region
     * extrude, edges build edge-only walls, and loose verts spawn a
     * connecting wire edge to their lifted duplicate.
     *
     * `edges_exclude` populates the operator's `edges_exclude` mapping slot;
     * excluded edges are not split off into the extruded region. It may be
     * null with edges_exclude_len == 0 to request no exclusions.
     *
     * `use_keep_orig` and `use_normal_flip` are forwarded verbatim. No input
     * elements are killed after the op: deletion of selection-interior
     * originals is left entirely to BMesh, which correctly leaves loose verts
     * and wire edges (which have no interior) in place.
     *
     * Returns true on success, false if BMO_op_initf rejected the input.
     */
    bool bms_extrude_face_region_geom(BMesh *bm,
                                      BMHeader **geom, int geom_len,
                                      BMEdge **edges_exclude, int edges_exclude_len,
                                      bool use_keep_orig,
                                      bool use_normal_flip)
    {
        using namespace blender;
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "extrude_face_region geom=%eb use_keep_orig=%b use_normal_flip=%b",
                          reinterpret_cast<BMHeader **>(geom),
                          geom_len,
                          use_keep_orig,
                          use_normal_flip))
        {
            return false;
        }

        if (edges_exclude && edges_exclude_len > 0)
        {
            BMOpSlot *slot = BMO_slot_get(op.slots_in, "edges_exclude");
            for (int i = 0; i < edges_exclude_len; i++)
            {
                BMO_slot_map_insert(&op, slot, edges_exclude[i], nullptr);
            }
        }

        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    /*
     * Capturing variant of bms_extrude_face_region_geom: runs the operator and
     * reads back its `geom.out` output slot before finishing.
     *
     * The inputs match bms_extrude_face_region_geom exactly. After execution
     * the operator's `geom.out` slot holds the full set of geometry the extrude
     * produced -- a mixed element buffer of verts, edges, and faces. Each
     * element is returned type-erased as a BMHeader* (the header is the first
     * field of every element). Up to `out_cap` pointers are copied into the
     * caller-allocated `out_buf`; the return value is the total `geom.out`
     * element count, which may exceed `out_cap` (the caller can re-query with a
     * larger buffer). The slot is read before BMO_op_finish frees it.
     *
     * Returns -1 if BMO_op_initf rejected the input.
     */
    int bms_extrude_face_region_geom_out(BMesh *bm,
                                         BMHeader **geom, int geom_len,
                                         BMEdge **edges_exclude, int edges_exclude_len,
                                         bool use_keep_orig,
                                         bool use_normal_flip,
                                         BMHeader **out_buf, int out_cap)
    {
        using namespace blender;
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "extrude_face_region geom=%eb use_keep_orig=%b use_normal_flip=%b",
                          reinterpret_cast<BMHeader **>(geom),
                          geom_len,
                          use_keep_orig,
                          use_normal_flip))
        {
            return -1;
        }

        if (edges_exclude && edges_exclude_len > 0)
        {
            BMOpSlot *slot = BMO_slot_get(op.slots_in, "edges_exclude");
            for (int i = 0; i < edges_exclude_len; i++)
            {
                BMO_slot_map_insert(&op, slot, edges_exclude[i], nullptr);
            }
        }

        BMO_op_exec(bm, &op);

        /* Walk the `geom.out` element buffer (verts + edges + faces). */
        int out_count = 0;
        {
            BMOIter oiter;
            BMHeader *ele = static_cast<BMHeader *>(
                BMO_iter_new(&oiter, op.slots_out, "geom.out", BM_ALL_NOLOOP));
            for (; ele; ele = static_cast<BMHeader *>(BMO_iter_step(&oiter)))
            {
                if (out_count < out_cap)
                {
                    out_buf[out_count] = ele;
                }
                out_count++;
            }
        }

        BMO_op_finish(bm, &op);
        return out_count;
    }

    bool bms_solidify(BMesh *bm, BMHeader **geom, int geom_len, float thickness)
    {
        using namespace blender;
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "solidify geom=%eb thickness=%f",
                          reinterpret_cast<BMHeader **>(geom),
                          geom_len,
                          thickness))
        {
            return false;
        }

        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    /*
     * Capturing variant of bms_solidify: runs the operator and reads back its
     * `geom.out` output slot before finishing. The inputs match bms_solidify
     * exactly. After execution the operator's `geom.out` slot holds the full
     * set of geometry the operation produced -- a mixed element buffer of
     * verts, edges, and faces. Each element is returned type-erased as a
     * BMHeader* (the header is the first field of every element). Up to
     * `out_cap` pointers are copied into the caller-allocated `out_buf`; the
     * return value is the total `geom.out` element count, which may exceed
     * `out_cap` (the caller can re-query with a larger buffer). The slot is
     * read before BMO_op_finish frees it.
     *
     * Returns -1 if BMO_op_initf rejected the input.
     */
    int bms_solidify_out(BMesh *bm, BMHeader **geom, int geom_len, float thickness,
                         BMHeader **out_buf, int out_cap)
    {
        using namespace blender;
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "solidify geom=%eb thickness=%f",
                          reinterpret_cast<BMHeader **>(geom),
                          geom_len,
                          thickness))
        {
            return -1;
        }

        BMO_op_exec(bm, &op);

        /* Walk the `geom.out` element buffer (verts + edges + faces). */
        int out_count = 0;
        {
            BMOIter oiter;
            BMHeader *ele = static_cast<BMHeader *>(
                BMO_iter_new(&oiter, op.slots_out, "geom.out", BM_ALL_NOLOOP));
            for (; ele; ele = static_cast<BMHeader *>(BMO_iter_step(&oiter)))
            {
                if (out_count < out_cap)
                {
                    out_buf[out_count] = ele;
                }
                out_count++;
            }
        }

        BMO_op_finish(bm, &op);
        return out_count;
    }

    bool bms_extrude_face_region_normal_from_adjacent(BMesh *bm,
                                                      BMFace **faces, int faces_len,
                                                      bool use_keep_orig,
                                                      bool use_normal_flip,
                                                      bool use_normal_from_adjacent)
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
                          "extrude_face_region geom=%hf use_keep_orig=%b use_normal_flip=%b use_normal_from_adjacent=%b",
                          BM_ELEM_TAG,
                          use_keep_orig,
                          use_normal_flip,
                          use_normal_from_adjacent))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_extrude_face_region_dissolve_ortho(BMesh *bm,
                                                BMFace **faces, int faces_len,
                                                bool use_keep_orig,
                                                bool use_normal_flip,
                                                bool use_dissolve_ortho_edges)
    {
        using namespace blender;

        /* The use_dissolve_ortho_edges post-pass reads cached face normals
         * (f->no) to decide which boundary walls lie in the cap plane, so
         * those normals must be current before the operator runs. Refresh
         * per-face (mirrors bms_mesh_normals_update; avoids the TBB /
         * loop-normal machinery BM_mesh_normals_update would pull in). */
        {
            BMFace *fn;
            BMIter nit;
            BM_ITER_MESH(fn, &nit, bm, BM_FACES_OF_MESH)
            {
                BM_face_normal_update(fn);
            }
        }

        /* The dissolve-orthogonal-edges pass only runs over boundary edges of
         * the extruded region, and a region edge is only treated as boundary
         * once the operator has deleted the originals it lifted off. That
         * deletion is gated on the region's *edges* being present in `geom`
         * (the operator flags geom edges as EXT_INPUT and keys "delete
         * originals" on them); a faces-only geom leaves the originals in place,
         * so no region edge ever becomes boundary and the pass is a no-op.
         *
         * Tag the region's full closure -- its faces and their edges and verts
         * -- and hand the operator geom=%hfev so the region's edges reach the
         * EXT_INPUT pass. This matches the operator's documented expectation
         * that `geom` include edges. */
        {
            BMIter it;
            BMFace *f;
            BM_ITER_MESH(f, &it, bm, BM_FACES_OF_MESH)
            {
                BM_elem_flag_disable(f, BM_ELEM_TAG);
            }
            BMEdge *e;
            BM_ITER_MESH(e, &it, bm, BM_EDGES_OF_MESH)
            {
                BM_elem_flag_disable(e, BM_ELEM_TAG);
            }
            BMVert *v;
            BM_ITER_MESH(v, &it, bm, BM_VERTS_OF_MESH)
            {
                BM_elem_flag_disable(v, BM_ELEM_TAG);
            }
        }
        for (int i = 0; i < faces_len; i++)
        {
            BMFace *f = faces[i];
            BM_elem_flag_enable(f, BM_ELEM_TAG);
            BMLoop *l_iter, *l_first;
            l_iter = l_first = BM_FACE_FIRST_LOOP(f);
            do
            {
                BM_elem_flag_enable(l_iter->e, BM_ELEM_TAG);
                BM_elem_flag_enable(l_iter->v, BM_ELEM_TAG);
            } while ((l_iter = l_iter->next) != l_first);
        }

        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "extrude_face_region geom=%hfev use_keep_orig=%b use_normal_flip=%b use_dissolve_ortho_edges=%b",
                          BM_ELEM_TAG,
                          use_keep_orig,
                          use_normal_flip,
                          use_dissolve_ortho_edges))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    /*
     * Extrude a region of faces, forwarding the operator's `skip_input_flip`
     * slot alongside `use_keep_orig`.
     *
     * `skip_input_flip` only has an effect when `use_keep_orig` is true. With
     * `use_keep_orig`, the operator's kept-original cleanup may reverse the
     * winding of the retained original face; setting `skip_input_flip`
     * suppresses that flip so the original keeps its incoming orientation.
     * Both booleans are forwarded verbatim. `use_normal_flip` is left at its
     * operator default.
     *
     * Marks each input face with BM_ELEM_TAG and passes them as the operator's
     * `geom` input. No input faces are killed after the op; deletion of
     * selection-interior originals is left to the operator under
     * use_keep_orig=false.
     *
     * Returns true on success, false if the operator rejected the input.
     */
    bool bms_extrude_face_region_skip_input_flip(BMesh *bm,
                                                 BMFace **faces, int faces_len,
                                                 bool use_keep_orig,
                                                 bool skip_input_flip)
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
                          "extrude_face_region geom=%hf use_keep_orig=%b skip_input_flip=%b",
                          BM_ELEM_TAG,
                          use_keep_orig,
                          skip_input_flip))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
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

    /* ---- Extrude (BMesh operator: extrude_edge_only) ---- */
    /*
     * Marks each input edge with BM_ELEM_TAG (after clearing TAG on every other
     * edge), then invokes the `extrude_edge_only` operator with the tagged edge
     * set as input.
     *
     * Each input edge gains one wall quad spanning the original edge and its
     * lifted duplicate; a contiguous strip of input edges produces a continuous
     * ribbon sharing the vertical edges between adjacent walls.
     *
     * Unlike the region extrude shim, no post-op kill is performed: the original
     * edges, verts and faces are kept in place (they may still attach to
     * non-input surrounding faces). The body is simply init/exec/finish.
     *
     * `use_select_history` is left at false; it only affects editor selection
     * state. `use_normal_flip` is forwarded verbatim; when true it reverses the
     * winding of each wall quad.
     *
     * Caller is responsible for displacing the new verts after the call.
     *
     * Returns true on success, false if BMO_op_initf rejected the input.
     */
    bool bms_extrude_edge_only_ex(BMesh *bm, BMEdge **edges, int edges_len,
                                  bool use_normal_flip)
    {
        using namespace blender;
        BMIter it;
        BMEdge *e;
        BM_ITER_MESH(e, &it, bm, BM_EDGES_OF_MESH)
        {
            BM_elem_flag_disable(e, BM_ELEM_TAG);
        }
        for (int i = 0; i < edges_len; i++)
        {
            BM_elem_flag_enable(edges[i], BM_ELEM_TAG);
        }

        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "extrude_edge_only edges=%he use_normal_flip=%b use_select_history=%b",
                          BM_ELEM_TAG,
                          use_normal_flip,
                          false))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_extrude_edge_only(BMesh *bm, BMEdge **edges, int edges_len)
    {
        return bms_extrude_edge_only_ex(bm, edges, edges_len, false);
    }

    /* ---- Extrude (BMesh operator: extrude_vert_indiv) ---- */
    /*
     * Marks each input vert with BM_ELEM_TAG (after clearing TAG on every other
     * vert), then invokes the `extrude_vert_indiv` operator with the tagged vert
     * set as input.
     *
     * For each input vert the operator creates a duplicate vert at the same
     * position and a fresh wire edge connecting the original to the duplicate.
     * The operation is purely additive: the original verts are never deleted, so
     * unlike the region extrude shim no post-op kill is performed. The body is
     * simply init/exec/finish.
     *
     * `use_select_history` is left at false; it only affects editor selection
     * state.
     *
     * Caller is responsible for displacing the new verts after the call.
     *
     * Returns true on success, false if BMO_op_initf rejected the input.
     */
    bool bms_extrude_vert_indiv(BMesh *bm, BMVert **verts, int verts_len)
    {
        using namespace blender;
        BMIter it;
        BMVert *v;
        BM_ITER_MESH(v, &it, bm, BM_VERTS_OF_MESH)
        {
            BM_elem_flag_disable(v, BM_ELEM_TAG);
        }
        for (int i = 0; i < verts_len; i++)
        {
            BM_elem_flag_enable(verts[i], BM_ELEM_TAG);
        }

        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "extrude_vert_indiv verts=%hv use_select_history=%b",
                          BM_ELEM_TAG,
                          false))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    /* ---- Spin (BMesh operator: spin) ---- */
    /*
     * Drives BMesh's `spin` operator end-to-end. The input geometry is passed
     * via the `geom` element buffer (%eb); `cent`, `axis` and `dvec` are
     * forwarded as %v vectors (a null `dvec` becomes a zero translation). The
     * `space` argument is the operator's MAT input slot: a 4x4 coordinate-frame
     * matrix in which `cent`, `axis`, the rotation and `dvec` are all
     * interpreted. It is laid out as Blender's `float[4][4]`, i.e. 16 contiguous
     * floats in column-major order (`space[col * 4 + row]`). A null pointer
     * leaves the slot at its identity default, reproducing world-space spin. The
     * `geom_last.out` element buffer is walked with a BMOIter restricted to
     * BM_ALL_NOLOOP and written into `out_geom_last` up to `out_geom_last_cap`
     * entries; its full count is the return value (which may exceed the cap),
     * or -1 if BMO_op_initf rejected the input.
     */
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
                 BMHeader **out_geom_last, int out_geom_last_cap)
    {
        using namespace blender;

        const float zero_vec[3] = {0.0f, 0.0f, 0.0f};
        const float *dvec_arg = dvec ? dvec : zero_vec;

        float identity_mat[4][4];
        unit_m4(identity_mat);
        const float *space_arg = space ? space : &identity_mat[0][0];

        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "spin geom=%eb cent=%v axis=%v dvec=%v angle=%f space=%m4 "
                          "steps=%i use_merge=%b use_normal_flip=%b use_duplicate=%b",
                          geom,
                          geom_len,
                          cent,
                          axis,
                          dvec_arg,
                          angle,
                          space_arg,
                          steps,
                          use_merge,
                          use_normal_flip,
                          use_duplicate))
        {
            return -1;
        }

        BMO_op_exec(bm, &op);

        /* Walk the `geom_last.out` element buffer (leading edge of last step). */
        int geom_last_count = 0;
        {
            BMOIter oiter;
            BMHeader *ele = static_cast<BMHeader *>(
                BMO_iter_new(&oiter, op.slots_out, "geom_last.out", BM_ALL_NOLOOP));
            for (; ele; ele = static_cast<BMHeader *>(BMO_iter_step(&oiter)))
            {
                if (geom_last_count < out_geom_last_cap)
                {
                    out_geom_last[geom_last_count] = ele;
                }
                geom_last_count++;
            }
        }

        BMO_op_finish(bm, &op);
        return geom_last_count;
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

    /*
     * Capturing variant of bms_inset_region: runs the same operator with the
     * same parameter handling, then copies the `faces.out` slot (the ring of
     * wall faces the inset creates around the region) into the caller-supplied
     * `out_buf` of `out_cap` face slots. Returns the total slot count (which
     * may exceed `out_cap`), or -1 on operator init failure.
     */
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
                             BMFace **out_buf, int out_cap)
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
            return -1;
        }

        if (faces_exclude && faces_exclude_len > 0)
        {
            BMOpSlot *slot = BMO_slot_get(op.slots_in, "faces_exclude");
            BMO_slot_buffer_from_array(
                &op, slot, reinterpret_cast<BMHeader **>(faces_exclude), faces_exclude_len);
        }

        BMO_op_exec(bm, &op);

        BMOpSlot *out_slot = BMO_slot_get(op.slots_out, "faces.out");
        const int n = out_slot->len;
        const int n_copy = (n < out_cap) ? n : out_cap;
        BMFace **slot_items = reinterpret_cast<BMFace **>(out_slot->data.buf);
        for (int i = 0; i < n_copy; ++i)
        {
            out_buf[i] = slot_items[i];
        }

        BMO_op_finish(bm, &op);
        return n;
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

    int bms_inset_individual_out(BMesh *bm,
                                 BMFace **faces, int faces_len,
                                 bool use_even_offset,
                                 bool use_interpolate,
                                 bool use_relative_offset,
                                 float thickness,
                                 float depth,
                                 BMFace **out_buf, int out_cap)
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
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *out_slot = BMO_slot_get(op.slots_out, "faces.out");
        const int n = out_slot->len;
        const int n_copy = (n < out_cap) ? n : out_cap;
        BMFace **slot_items = reinterpret_cast<BMFace **>(out_slot->data.buf);
        for (int i = 0; i < n_copy; ++i)
        {
            out_buf[i] = slot_items[i];
        }

        BMO_op_finish(bm, &op);
        return n;
    }

    /* ---- Bevel (BMesh operator: bevel) ---- */
    /*
     * The bevel operator is driven through its BMOP slot form (no public
     * convenience C wrapper surfaces its full parameter set). The mixed
     * vert/edge/face element buffer is passed via "%eb"; the operator flushes
     * it into BM_ELEM_TAG internally, so the input pointers are not touched.
     *
     * Every slot is forwarded explicitly. The integer enum slots are passed
     * as raw "%i" values (the operator reads them with BMO_slot_int_get). The
     * custom-profile pointer slot is left at its default (null).
     *
     * Output geometry is mutated in place; the operator's element-buffer
     * output slots are not harvested.
     */
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
                   int vmesh_method)
    {
        /* The offset solver reads face / vertex normals; freshly-built meshes
         * carry stale (zero) normals until refreshed. Refresh serially (face
         * normals first, then vertex normals from incident faces) to avoid the
         * task-scheduler / loop-normal machinery the whole-mesh updater pulls
         * in. */
        {
            BMFace *f;
            BMIter face_iter;
            BM_ITER_MESH(f, &face_iter, bm, BM_FACES_OF_MESH)
            {
                BM_face_normal_update(f);
            }
            BMVert *v;
            BMIter vert_iter;
            BM_ITER_MESH(v, &vert_iter, bm, BM_VERTS_OF_MESH)
            {
                BM_vert_normal_update_all(v);
            }
        }

        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "bevel geom=%eb offset=%f offset_type=%i segments=%i "
                          "profile=%f profile_type=%i affect=%i clamp_overlap=%b "
                          "material=%i loop_slide=%b mark_seam=%b mark_sharp=%b "
                          "harden_normals=%b face_strength_mode=%i "
                          "miter_outer=%i miter_inner=%i spread=%f vmesh_method=%i",
                          geom,
                          geom_len,
                          double(offset),
                          offset_type,
                          segments,
                          double(profile),
                          profile_type,
                          affect,
                          clamp_overlap,
                          material,
                          loop_slide,
                          mark_seam,
                          mark_sharp,
                          harden_normals,
                          face_strength_mode,
                          miter_outer,
                          miter_inner,
                          double(spread),
                          vmesh_method))
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

    bool bms_unsubdivide(BMesh *bm,
                         BMVert **verts, int verts_len,
                         int iterations)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "unsubdivide verts=%eb iterations=%i",
                          reinterpret_cast<BMHeader **>(verts),
                          verts_len,
                          iterations))
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

    bool bms_triangle_fill(BMesh *bm,
                           BMEdge **edges, int edges_len,
                           bool use_beauty,
                           bool use_dissolve,
                           const float *normal)
    {
        const float zero_vec[3] = {0.0f, 0.0f, 0.0f};
        const float *normal_arg = normal ? normal : zero_vec;

        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "triangle_fill edges=%eb "
                          "use_beauty=%b use_dissolve=%b normal=%v",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          use_beauty,
                          use_dissolve,
                          normal_arg))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    int bms_triangle_fill_out(BMesh *bm,
                              BMEdge **edges, int edges_len,
                              bool use_beauty,
                              bool use_dissolve,
                              const float *normal,
                              BMHeader **out_buf, int out_cap)
    {
        const float zero_vec[3] = {0.0f, 0.0f, 0.0f};
        const float *normal_arg = normal ? normal : zero_vec;

        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "triangle_fill edges=%eb "
                          "use_beauty=%b use_dissolve=%b normal=%v",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          use_beauty,
                          use_dissolve,
                          normal_arg))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        int geom_count = 0;
        {
            BMOIter oiter;
            BMHeader *ele = static_cast<BMHeader *>(
                BMO_iter_new(&oiter, op.slots_out, "geom.out", BM_ALL_NOLOOP));
            for (; ele; ele = static_cast<BMHeader *>(BMO_iter_step(&oiter)))
            {
                if (geom_count < out_cap)
                {
                    out_buf[geom_count] = ele;
                }
                geom_count++;
            }
        }

        BMO_op_finish(bm, &op);
        return geom_count;
    }

    bool bms_reverse_uvs(BMesh *bm, BMFace **faces, int faces_len)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "reverse_uvs faces=%eb",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_collapse_uvs(BMesh *bm, BMEdge **edges, int edges_len)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "collapse_uvs edges=%eb",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_average_vert_facedata(BMesh *bm, BMVert **verts, int verts_len)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "average_vert_facedata verts=%eb",
                          reinterpret_cast<BMHeader **>(verts),
                          verts_len))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_pointmerge_facedata(BMesh *bm, BMVert **verts, int verts_len, BMVert *vert_snap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "pointmerge_facedata verts=%eb vert_snap=%e",
                          reinterpret_cast<BMHeader **>(verts),
                          verts_len,
                          reinterpret_cast<BMHeader *>(vert_snap)))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_reverse_colors(BMesh *bm, BMFace **faces, int faces_len, int color_index)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "reverse_colors faces=%eb color_index=%i",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len,
                          color_index))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_rotate_uvs(BMesh *bm, BMFace **faces, int faces_len, bool use_ccw)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "rotate_uvs faces=%eb use_ccw=%b",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len,
                          use_ccw))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_rotate_colors(BMesh *bm, BMFace **faces, int faces_len, bool use_ccw, int color_index)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "rotate_colors faces=%eb use_ccw=%b color_index=%i",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len,
                          use_ccw,
                          color_index))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    /* ---- Recalculate face normals (BMesh operator: recalc_face_normals) ---- */
    /*
     * The "Recalculate Outside" / "Recalculate Inside" pair. Both run the
     * `recalc_face_normals` BMOP, which recomputes each named face's cached
     * normal from its corner geometry and then walks the manifold adjacency
     * of the input set, flipping faces so every connected component is
     * consistently wound with an outward-facing normal. The input pointers
     * are translated into the operator's `faces=%eb` element-buffer slot and
     * are not themselves modified.
     */
    bool bms_recalc_face_normals(BMesh *bm,
                                 BMFace **faces, int faces_len)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "recalc_face_normals faces=%eb",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    bool bms_recalc_face_normals_inside(BMesh *bm,
                                        BMFace **faces, int faces_len)
    {
        /* First make the component consistently outward-wound, then reverse
         * each named face's winding (which also negates its cached normal) so
         * the component ends up consistently inward-facing. */
        if (!bms_recalc_face_normals(bm, faces, faces_len))
        {
            return false;
        }
        for (int i = 0; i < faces_len; ++i)
        {
            BM_face_normal_flip(bm, faces[i]);
        }
        return true;
    }

    int bms_split_edges(BMesh *bm,
                        BMEdge **edges, int edges_len,
                        BMVert **verts, int verts_len,
                        bool use_verts,
                        BMEdge **out_edges, int out_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "split_edges edges=%eb verts=%eb use_verts=%b",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          reinterpret_cast<BMHeader **>(verts),
                          (verts != nullptr) ? verts_len : 0,
                          use_verts))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *slot = BMO_slot_get(op.slots_out, "edges.out");
        const int n = slot->len;
        const int n_copy = (n < out_cap) ? n : out_cap;
        BMEdge **slot_items = reinterpret_cast<BMEdge **>(slot->data.buf);
        for (int i = 0; i < n_copy; ++i)
        {
            out_edges[i] = slot_items[i];
        }

        BMO_op_finish(bm, &op);
        return n;
    }

    bool bms_join_triangles(BMesh *bm,
                            BMFace **faces, int faces_len,
                            bool cmp_seam, bool cmp_sharp, bool cmp_uvs,
                            bool cmp_vcols, bool cmp_materials,
                            float angle_face_threshold,
                            float angle_shape_threshold,
                            float topology_influence,
                            bool deselect_joined)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "join_triangles faces=%eb "
                          "cmp_seam=%b cmp_sharp=%b cmp_uvs=%b cmp_vcols=%b cmp_materials=%b "
                          "angle_face_threshold=%f angle_shape_threshold=%f "
                          "topology_influence=%f deselect_joined=%b",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len,
                          cmp_seam,
                          cmp_sharp,
                          cmp_uvs,
                          cmp_vcols,
                          cmp_materials,
                          double(angle_face_threshold),
                          double(angle_shape_threshold),
                          double(topology_influence),
                          deselect_joined))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    int bms_join_triangles_out(BMesh *bm,
                               BMFace **faces, int faces_len,
                               bool cmp_seam, bool cmp_sharp, bool cmp_uvs,
                               bool cmp_vcols, bool cmp_materials,
                               float angle_face_threshold,
                               float angle_shape_threshold,
                               float topology_influence,
                               bool deselect_joined,
                               BMFace **out_buf, int out_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "join_triangles faces=%eb "
                          "cmp_seam=%b cmp_sharp=%b cmp_uvs=%b cmp_vcols=%b cmp_materials=%b "
                          "angle_face_threshold=%f angle_shape_threshold=%f "
                          "topology_influence=%f deselect_joined=%b",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len,
                          cmp_seam,
                          cmp_sharp,
                          cmp_uvs,
                          cmp_vcols,
                          cmp_materials,
                          double(angle_face_threshold),
                          double(angle_shape_threshold),
                          double(topology_influence),
                          deselect_joined))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *slot = BMO_slot_get(op.slots_out, "faces.out");
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
                             bool use_sphere)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "subdivide_edges edges=%eb cuts=%i "
                          "smooth=%f smooth_falloff=%i use_smooth_even=%b "
                          "fractal=%f along_normal=%f seed=%i "
                          "quad_corner_type=%i use_grid_fill=%b "
                          "use_single_edge=%b use_only_quads=%b "
                          "use_sphere=%b",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          cuts,
                          double(smooth),
                          smooth_falloff,
                          use_smooth_even,
                          double(fractal),
                          double(along_normal),
                          seed,
                          quad_corner_type,
                          use_grid_fill,
                          use_single_edge,
                          use_only_quads,
                          use_sphere))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

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
                               int *r_geom_len)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "subdivide_edges edges=%eb cuts=%i "
                          "smooth=%f smooth_falloff=%i use_smooth_even=%b "
                          "fractal=%f along_normal=%f seed=%i "
                          "quad_corner_type=%i use_grid_fill=%b "
                          "use_single_edge=%b use_only_quads=%b "
                          "use_sphere=%b",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          cuts,
                          double(smooth),
                          smooth_falloff,
                          use_smooth_even,
                          double(fractal),
                          double(along_normal),
                          seed,
                          quad_corner_type,
                          use_grid_fill,
                          use_single_edge,
                          use_only_quads,
                          use_sphere))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *split_slot = BMO_slot_get(op.slots_out, "geom_split.out");
        const int split_n = split_slot->len;
        const int split_copy = (split_n < out_split_cap) ? split_n : out_split_cap;
        BMHeader **split_items = reinterpret_cast<BMHeader **>(split_slot->data.buf);
        for (int i = 0; i < split_copy; ++i)
        {
            out_split[i] = split_items[i];
        }
        if (r_split_len)
        {
            *r_split_len = split_n;
        }

        BMOpSlot *inner_slot = BMO_slot_get(op.slots_out, "geom_inner.out");
        const int inner_n = inner_slot->len;
        const int inner_copy = (inner_n < out_inner_cap) ? inner_n : out_inner_cap;
        BMHeader **inner_items = reinterpret_cast<BMHeader **>(inner_slot->data.buf);
        for (int i = 0; i < inner_copy; ++i)
        {
            out_inner[i] = inner_items[i];
        }
        if (r_inner_len)
        {
            *r_inner_len = inner_n;
        }

        BMOpSlot *geom_slot = BMO_slot_get(op.slots_out, "geom.out");
        const int geom_n = geom_slot->len;
        const int geom_copy = (geom_n < out_geom_cap) ? geom_n : out_geom_cap;
        BMHeader **geom_items = reinterpret_cast<BMHeader **>(geom_slot->data.buf);
        for (int i = 0; i < geom_copy; ++i)
        {
            out_geom[i] = geom_items[i];
        }
        if (r_geom_len)
        {
            *r_geom_len = geom_n;
        }

        BMO_op_finish(bm, &op);
        return 0;
    }

    /* ---- Subdivide edge-ring (BMesh operator: subdivide_edgering) ---- */

    bool bms_subdivide_edgering(BMesh *bm,
                                BMEdge **edges, int edges_len,
                                int cuts,
                                int interp_mode,
                                float smooth,
                                int profile_shape,
                                float profile_shape_factor)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "subdivide_edgering edges=%eb interp_mode=%i "
                          "smooth=%f cuts=%i profile_shape=%i "
                          "profile_shape_factor=%f",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          interp_mode,
                          double(smooth),
                          cuts,
                          profile_shape,
                          double(profile_shape_factor)))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    int bms_subdivide_edgering_out(BMesh *bm,
                                   BMEdge **edges, int edges_len,
                                   int cuts,
                                   int interp_mode,
                                   float smooth,
                                   int profile_shape,
                                   float profile_shape_factor,
                                   BMFace **out_buf, int out_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "subdivide_edgering edges=%eb interp_mode=%i "
                          "smooth=%f cuts=%i profile_shape=%i "
                          "profile_shape_factor=%f",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          interp_mode,
                          double(smooth),
                          cuts,
                          profile_shape,
                          double(profile_shape_factor)))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *slot = BMO_slot_get(op.slots_out, "faces.out");
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

    /* ---- Bisect edges (BMesh operator: bisect_edges) ---- */

    bool bms_bisect_edges(BMesh *bm,
                          BMEdge **edges, int edges_len,
                          int cuts)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "bisect_edges edges=%eb cuts=%i",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          cuts))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    /* ---- Collapse (BMesh operator: collapse) ---- */

    bool bms_collapse(BMesh *bm,
                      BMEdge **edges, int edges_len,
                      bool uvs)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "collapse edges=%eb uvs=%b",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          uvs))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    /* ---- Connect verts (BMesh operator: connect_verts) ---- */

    bool bms_connect_verts(BMesh *bm,
                           BMVert **verts, int verts_len,
                           BMFace **faces_exclude, int faces_exclude_len,
                           bool check_degenerate)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "connect_verts verts=%eb faces_exclude=%eb "
                          "check_degenerate=%b",
                          reinterpret_cast<BMHeader **>(verts),
                          verts_len,
                          reinterpret_cast<BMHeader **>(faces_exclude),
                          faces_exclude_len,
                          check_degenerate))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    int bms_connect_verts_out(BMesh *bm,
                              BMVert **verts, int verts_len,
                              BMFace **faces_exclude, int faces_exclude_len,
                              bool check_degenerate,
                              BMEdge **out_buf, int out_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "connect_verts verts=%eb faces_exclude=%eb "
                          "check_degenerate=%b",
                          reinterpret_cast<BMHeader **>(verts),
                          verts_len,
                          reinterpret_cast<BMHeader **>(faces_exclude),
                          faces_exclude_len,
                          check_degenerate))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *slot = BMO_slot_get(op.slots_out, "edges.out");
        const int n = slot->len;
        const int n_copy = (n < out_cap) ? n : out_cap;
        BMEdge **slot_items = reinterpret_cast<BMEdge **>(slot->data.buf);
        for (int i = 0; i < n_copy; ++i)
        {
            out_buf[i] = slot_items[i];
        }

        BMO_op_finish(bm, &op);
        return n;
    }

    /* ---- Connect verts concave (BMesh operator: connect_verts_concave) ---- */

    bool bms_connect_verts_concave(BMesh *bm,
                                   BMFace **faces, int faces_len)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "connect_verts_concave faces=%eb",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    int bms_connect_verts_concave_out(BMesh *bm,
                                      BMFace **faces, int faces_len,
                                      BMEdge **out_buf, int out_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "connect_verts_concave faces=%eb",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *slot = BMO_slot_get(op.slots_out, "edges.out");
        const int n = slot->len;
        const int n_copy = (n < out_cap) ? n : out_cap;
        BMEdge **slot_items = reinterpret_cast<BMEdge **>(slot->data.buf);
        for (int i = 0; i < n_copy; ++i)
        {
            out_buf[i] = slot_items[i];
        }

        BMO_op_finish(bm, &op);
        return n;
    }

    /* ---- Connect verts nonplanar (BMesh operator: connect_verts_nonplanar) ---- */

    bool bms_connect_verts_nonplanar(BMesh *bm,
                                     BMFace **faces, int faces_len,
                                     float angle_limit)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "connect_verts_nonplanar faces=%eb angle_limit=%f",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len,
                          angle_limit))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    int bms_connect_verts_nonplanar_out(BMesh *bm,
                                        BMFace **faces, int faces_len,
                                        float angle_limit,
                                        BMEdge **out_buf, int out_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "connect_verts_nonplanar faces=%eb angle_limit=%f",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len,
                          angle_limit))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *slot = BMO_slot_get(op.slots_out, "edges.out");
        const int n = slot->len;
        const int n_copy = (n < out_cap) ? n : out_cap;
        BMEdge **slot_items = reinterpret_cast<BMEdge **>(slot->data.buf);
        for (int i = 0; i < n_copy; ++i)
        {
            out_buf[i] = slot_items[i];
        }

        BMO_op_finish(bm, &op);
        return n;
    }

    int bms_connect_verts_nonplanar_faces_out(BMesh *bm,
                                              BMFace **faces, int faces_len,
                                              float angle_limit,
                                              BMFace **out_buf, int out_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "connect_verts_nonplanar faces=%eb angle_limit=%f",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len,
                          angle_limit))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *slot = BMO_slot_get(op.slots_out, "faces.out");
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

    /* ---- Planar faces (BMesh operator: planar_faces) ---- */

    bool bms_planar_faces(BMesh *bm,
                          BMFace **faces, int faces_len,
                          int iterations, float factor)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "planar_faces faces=%eb iterations=%i factor=%f",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len,
                          iterations,
                          factor))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    /* ---- Rotate edges (BMesh operator: rotate_edges) ---- */

    bool bms_rotate_edges(BMesh *bm,
                          BMEdge **edges, int edges_len,
                          bool use_ccw)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "rotate_edges edges=%eb use_ccw=%b",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          use_ccw))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    int bms_rotate_edges_out(BMesh *bm,
                             BMEdge **edges, int edges_len,
                             bool use_ccw,
                             BMEdge **out_buf, int out_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "rotate_edges edges=%eb use_ccw=%b",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          use_ccw))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *slot = BMO_slot_get(op.slots_out, "edges.out");
        const int n = slot->len;
        const int n_copy = (n < out_cap) ? n : out_cap;
        BMEdge **slot_items = reinterpret_cast<BMEdge **>(slot->data.buf);
        for (int i = 0; i < n_copy; ++i)
        {
            out_buf[i] = slot_items[i];
        }

        BMO_op_finish(bm, &op);
        return n;
    }

    /* ---- Connect vert pair (BMesh operator: connect_vert_pair) ---- */

    bool bms_connect_vert_pair(BMesh *bm,
                               BMVert **verts, int verts_len,
                               BMVert **verts_exclude, int verts_exclude_len,
                               BMFace **faces_exclude, int faces_exclude_len)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "connect_vert_pair verts=%eb verts_exclude=%eb "
                          "faces_exclude=%eb",
                          reinterpret_cast<BMHeader **>(verts),
                          verts_len,
                          reinterpret_cast<BMHeader **>(verts_exclude),
                          verts_exclude_len,
                          reinterpret_cast<BMHeader **>(faces_exclude),
                          faces_exclude_len))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    int bms_connect_vert_pair_out(BMesh *bm,
                                  BMVert **verts, int verts_len,
                                  BMVert **verts_exclude, int verts_exclude_len,
                                  BMFace **faces_exclude, int faces_exclude_len,
                                  BMEdge **out_buf, int out_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "connect_vert_pair verts=%eb verts_exclude=%eb "
                          "faces_exclude=%eb",
                          reinterpret_cast<BMHeader **>(verts),
                          verts_len,
                          reinterpret_cast<BMHeader **>(verts_exclude),
                          verts_exclude_len,
                          reinterpret_cast<BMHeader **>(faces_exclude),
                          faces_exclude_len))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *slot = BMO_slot_get(op.slots_out, "edges.out");
        const int n = slot->len;
        const int n_copy = (n < out_cap) ? n : out_cap;
        BMEdge **slot_items = reinterpret_cast<BMEdge **>(slot->data.buf);
        for (int i = 0; i < n_copy; ++i)
        {
            out_buf[i] = slot_items[i];
        }

        BMO_op_finish(bm, &op);
        return n;
    }

    /* Run the `poke` BMOP over a face set and capture both output slots.
     *
     * The shim's `center_mode` follows the same convention as the
     * single-face poke shims (0 = MEAN, 1 = BOUNDS, 2 = MEAN_WEIGHTED);
     * it is translated here into the operator's native eCenterMode
     * (MEAN_WEIGHTED = 0, MEAN = 1, BOUNDS = 2) before being forwarded. */
    int bms_poke_out(BMesh *bm,
                     BMFace **faces, int faces_len,
                     int center_mode, float offset, bool use_relative_offset,
                     BMVert **out_verts, int out_verts_cap, int *r_verts_len,
                     BMFace **out_faces, int out_faces_cap, int *r_faces_len)
    {
        /* MEAN_WEIGHTED = 0, MEAN = 1, BOUNDS = 2 in the operator's enum. */
        int native_center_mode;
        switch (center_mode)
        {
        case 1:
            native_center_mode = 2; /* BOUNDS */
            break;
        case 2:
            native_center_mode = 0; /* MEAN_WEIGHTED */
            break;
        default:
            native_center_mode = 1; /* MEAN */
            break;
        }

        /* The poke operator lifts the centre vertex along each input face's
         * stored normal (f->no). That cached normal is only refreshed inside
         * the operator's post-exec normals pass, so a freshly-built face whose
         * normal was never computed would lift along a zero vector. Refresh the
         * input faces' normals up front so a non-zero offset takes effect. We
         * use per-face updates to avoid the loop-normal machinery that
         * BM_mesh_normals_update would pull in. */
        for (int i = 0; i < faces_len; ++i)
        {
            BM_face_normal_update(faces[i]);
        }

        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "poke faces=%eb offset=%f center_mode=%i "
                          "use_relative_offset=%b",
                          reinterpret_cast<BMHeader **>(faces),
                          faces_len,
                          double(offset),
                          native_center_mode,
                          use_relative_offset))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);

        BMOpSlot *verts_slot = BMO_slot_get(op.slots_out, "verts.out");
        const int verts_n = verts_slot->len;
        const int verts_copy = (verts_n < out_verts_cap) ? verts_n : out_verts_cap;
        BMVert **verts_items = reinterpret_cast<BMVert **>(verts_slot->data.buf);
        for (int i = 0; i < verts_copy; ++i)
        {
            out_verts[i] = verts_items[i];
        }
        if (r_verts_len)
        {
            *r_verts_len = verts_n;
        }

        BMOpSlot *faces_slot = BMO_slot_get(op.slots_out, "faces.out");
        const int faces_n = faces_slot->len;
        const int faces_copy = (faces_n < out_faces_cap) ? faces_n : out_faces_cap;
        BMFace **faces_items = reinterpret_cast<BMFace **>(faces_slot->data.buf);
        for (int i = 0; i < faces_copy; ++i)
        {
            out_faces[i] = faces_items[i];
        }
        if (r_faces_len)
        {
            *r_faces_len = faces_n;
        }

        BMO_op_finish(bm, &op);
        return 0;
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

    /*
     * Invoke BMesh's `weld_verts` BMOP, welding each source vert onto its
     * target vert.
     *
     * `pairs` is a flat array of 2 * pairs_len BMVert* laid out as
     * consecutive (src, tar) couples: pairs[2*i] is the source vert welded
     * onto target pairs[2*i+1]. Each couple becomes one entry in the
     * operator's `targetmap` mapping slot via BMO_slot_map_insert, with the
     * source vert as the map key and the target vert as the mapped value.
     *
     * `use_centroid` sets the operator's `use_centroid` bool slot: when true
     * each merged group settles at the centroid of its members, otherwise the
     * group adopts the target vert's position.
     *
     * Returns true on success, false if BMO_op_initf rejected the input.
     */
    bool bms_weld_verts(BMesh *bm,
                        BMVert **pairs, int pairs_len,
                        bool use_centroid)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "weld_verts use_centroid=%b",
                          use_centroid))
        {
            return false;
        }

        if (pairs && pairs_len > 0)
        {
            BMOpSlot *slot = BMO_slot_get(op.slots_in, "targetmap");
            for (int i = 0; i < pairs_len; i++)
            {
                BMVert *v_src = pairs[2 * i];
                BMVert *v_tar = pairs[2 * i + 1];
                BMO_slot_map_insert(&op, slot, v_src, v_tar);
            }
        }

        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    /*
     * Invoke BMesh's `find_doubles` operator, which detects coincident verts
     * within `dist` and builds a vert -> vert merge map without altering
     * topology.
     *
     * `verts` / `keep_verts` are forwarded to the operator's `verts` and
     * `keep_verts` element-buffer slots via the `%eb` format specifier; either
     * may be null with a length of 0. `dist` and `use_connected` set the
     * matching float / bool input slots.
     *
     * After exec the `targetmap.out` MAP_ELEM slot is walked with a BMOIter:
     * each iteration yields a source vert (the map key) and a target vert (the
     * mapped pointer value). Couples are emitted into `out_pairs` flat as
     * out_pairs[2*i] = src, out_pairs[2*i+1] = tar, up to `out_cap` couples.
     *
     * Returns the full entry count (which may exceed `out_cap`, leaving the
     * map truncated in the output buffer), or -1 if BMO_op_initf rejected the
     * input.
     */
    int bms_find_doubles(BMesh *bm,
                         BMVert **verts, int verts_len,
                         BMVert **keep_verts, int keep_len,
                         float dist, bool use_connected,
                         BMVert **out_pairs, int out_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "find_doubles verts=%eb keep_verts=%eb "
                          "dist=%f use_connected=%b",
                          reinterpret_cast<BMHeader **>(verts),
                          verts_len,
                          reinterpret_cast<BMHeader **>(keep_verts),
                          keep_len,
                          dist,
                          use_connected))
        {
            return -1;
        }

        BMO_op_exec(bm, &op);

        int count = 0;
        BMOIter oiter;
        BMVert *v_src = static_cast<BMVert *>(
            BMO_iter_new(&oiter, op.slots_out, "targetmap.out", BM_VERT));
        for (; v_src; v_src = static_cast<BMVert *>(BMO_iter_step(&oiter)))
        {
            BMVert *v_tar = static_cast<BMVert *>(BMO_iter_map_value_ptr(&oiter));
            if (count < out_cap)
            {
                out_pairs[2 * count] = v_src;
                out_pairs[2 * count + 1] = v_tar;
            }
            count++;
        }

        BMO_op_finish(bm, &op);
        return count;
    }

    /*
     * Invoke BMesh's `remove_doubles` operator: detect coincident verts within
     * `dist` (using the same clustering as `find_doubles`) and weld each group
     * in place. The operator mutates the mesh directly and has no output slot.
     *
     * `verts` is forwarded to the operator's `verts` element-buffer slot via
     * the `%eb` format specifier; it may be null with a length of 0. `dist`
     * and `use_connected` set the matching float / bool input slots.
     *
     * `keep_verts` / `keep_len` are accepted for signature parity with
     * `bms_find_doubles`; the `remove_doubles` operator has no `keep_verts`
     * slot, so they are intentionally unused.
     *
     * Returns true on success, false if BMO_op_initf rejected the input.
     */
    bool bms_remove_doubles(BMesh *bm,
                            BMVert **verts, int verts_len,
                            BMVert **keep_verts, int keep_len,
                            float dist, bool use_connected)
    {
        (void)keep_verts;
        (void)keep_len;

        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "remove_doubles verts=%eb dist=%f use_connected=%b",
                          reinterpret_cast<BMHeader **>(verts),
                          verts_len,
                          dist,
                          use_connected))
        {
            return false;
        }

        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    /*
     * Invoke BMesh's `pointmerge` operator: move every input vert onto
     * `merge_co` and weld the set together onto a single survivor (the first
     * vert in the input buffer). The operator mutates the mesh directly and
     * has no output slot.
     *
     * `verts` is forwarded to the operator's `verts` element-buffer slot via
     * the `%eb` format specifier; it may be null with a length of 0.
     * `merge_co` sets the `merge_co` vec slot via `%v` (the operator copies
     * the three floats, leaving the caller's buffer untouched).
     *
     * Returns true on success, false if BMO_op_initf rejected the input.
     */
    bool bms_pointmerge(BMesh *bm,
                        BMVert **verts, int verts_len,
                        const float merge_co[3])
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "pointmerge verts=%eb merge_co=%v",
                          reinterpret_cast<BMHeader **>(verts),
                          verts_len,
                          const_cast<float *>(merge_co)))
        {
            return false;
        }

        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
        return true;
    }

    /*
     * Maps to BMesh's `duplicate` BMOP: clone the input selection into
     * disjoint, coincident new geometry within the same mesh.
     *
     * `geom` / `geom_len` are forwarded to the operator's `geom`
     * element-buffer slot via the `%eb` specifier (a mixed BM_VERT |
     * BM_EDGE | BM_FACE set); either may be null with a length of 0. The
     * `dest` pointer slot is left unset, so the clones land in `bm`.
     * `use_edge_flip_from_face` sets the matching bool input slot.
     *
     * After exec the output slots are read back into the caller's buffers:
     *
     *   - The `geom.out` element buffer is walked with a BMOIter restricted
     *     to BM_ALL_NOLOOP and written into `out_geom` up to `out_geom_cap`
     *     entries; its full count is the return value.
     *   - Each `*_map.out` MAP_ELEM slot is walked with a BMOIter and emitted
     *     as flat (src, dst) couples (`buf[2*i]` key, `buf[2*i+1]` value), up
     *     to the slot's `_cap` couples; the full couple count is stored
     *     through the matching `_count` out-param when that pointer is
     *     non-null.
     *
     * Returns the total `geom.out` count (which may exceed `out_geom_cap`),
     * or -1 if BMO_op_initf rejected the input.
     */
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
                      int *out_face_count)
    {
        using namespace blender;
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "duplicate geom=%eb use_edge_flip_from_face=%b",
                          geom,
                          geom_len,
                          use_edge_flip_from_face))
        {
            return -1;
        }

        BMO_op_exec(bm, &op);

        /* Walk the `geom.out` element buffer (clone verts/edges/faces). */
        int geom_count = 0;
        {
            BMOIter oiter;
            BMHeader *ele = static_cast<BMHeader *>(
                BMO_iter_new(&oiter, op.slots_out, "geom.out", BM_ALL_NOLOOP));
            for (; ele; ele = static_cast<BMHeader *>(BMO_iter_step(&oiter)))
            {
                if (geom_count < out_geom_cap)
                {
                    out_geom[geom_count] = ele;
                }
                geom_count++;
            }
        }

        /* Read a MAP_ELEM slot as flat (key, value) couples. The slot stores
         * element pointers as both keys and values; `restrict_flag` selects
         * the key element type the iterator yields. */
        auto read_map = [&](const char *slot_name, int restrict_flag,
                            void **out_buf, int cap, int *out_count) {
            int count = 0;
            BMOIter oiter;
            void *key = BMO_iter_new(&oiter, op.slots_out, slot_name, restrict_flag);
            for (; key; key = BMO_iter_step(&oiter))
            {
                void *val = BMO_iter_map_value_ptr(&oiter);
                if (count < cap && out_buf)
                {
                    out_buf[2 * count] = key;
                    out_buf[2 * count + 1] = val;
                }
                count++;
            }
            if (out_count)
            {
                *out_count = count;
            }
        };

        read_map("boundary_map.out", BM_EDGE,
                 reinterpret_cast<void **>(out_boundary_map),
                 out_boundary_cap, out_boundary_count);
        read_map("isovert_map.out", BM_VERT,
                 reinterpret_cast<void **>(out_isovert_map),
                 out_isovert_cap, out_isovert_count);
        read_map("vert_map.out", BM_VERT,
                 reinterpret_cast<void **>(out_vert_map),
                 out_vert_cap, out_vert_count);
        read_map("edge_map.out", BM_EDGE,
                 reinterpret_cast<void **>(out_edge_map),
                 out_edge_cap, out_edge_count);
        read_map("face_map.out", BM_FACE,
                 reinterpret_cast<void **>(out_face_map),
                 out_face_cap, out_face_count);

        BMO_op_finish(bm, &op);
        return geom_count;
    }

    /* Invoke BMesh's `split` operator: duplicate `geom` and tear the copy
     * off as a topologically disjoint set within `bm`. See shim.h for the
     * slot mapping and read-back convention. */
    int bms_split(BMesh *bm,
                  BMHeader **geom, int geom_len,
                  bool use_only_faces,
                  BMHeader **out_geom, int out_geom_cap,
                  BMEdge **out_boundary_map, int out_boundary_cap,
                  int *out_boundary_count,
                  BMVert **out_isovert_map, int out_isovert_cap,
                  int *out_isovert_count)
    {
        using namespace blender;
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "split geom=%eb use_only_faces=%b",
                          geom,
                          geom_len,
                          use_only_faces))
        {
            return -1;
        }

        BMO_op_exec(bm, &op);

        /* Walk the `geom.out` element buffer (split-off verts/edges/faces). */
        int geom_count = 0;
        {
            BMOIter oiter;
            BMHeader *ele = static_cast<BMHeader *>(
                BMO_iter_new(&oiter, op.slots_out, "geom.out", BM_ALL_NOLOOP));
            for (; ele; ele = static_cast<BMHeader *>(BMO_iter_step(&oiter)))
            {
                if (geom_count < out_geom_cap)
                {
                    out_geom[geom_count] = ele;
                }
                geom_count++;
            }
        }

        /* Read a MAP_ELEM slot as flat (key, value) couples. */
        auto read_map = [&](const char *slot_name, int restrict_flag,
                            void **out_buf, int cap, int *out_count) {
            int count = 0;
            BMOIter oiter;
            void *key = BMO_iter_new(&oiter, op.slots_out, slot_name, restrict_flag);
            for (; key; key = BMO_iter_step(&oiter))
            {
                void *val = BMO_iter_map_value_ptr(&oiter);
                if (count < cap && out_buf)
                {
                    out_buf[2 * count] = key;
                    out_buf[2 * count + 1] = val;
                }
                count++;
            }
            if (out_count)
            {
                *out_count = count;
            }
        };

        read_map("boundary_map.out", BM_EDGE,
                 reinterpret_cast<void **>(out_boundary_map),
                 out_boundary_cap, out_boundary_count);
        read_map("isovert_map.out", BM_VERT,
                 reinterpret_cast<void **>(out_isovert_map),
                 out_isovert_cap, out_isovert_count);

        BMO_op_finish(bm, &op);
        return geom_count;
    }

    /* Invoke BMesh's `mirror` operator: duplicate `geom`, reflect the copy
     * across the `axis` plane in `matrix` space, flip the reflected
     * winding, and weld reflected verts onto their originals within
     * `merge_dist`. See shim.h for the slot mapping, the column-major
     * matrix layout, and the read-back convention. */
    int bms_mirror(BMesh *bm,
                   BMHeader **geom, int geom_len,
                   const float *matrix,
                   float merge_dist,
                   int axis,
                   bool mirror_u, bool mirror_v, bool mirror_udim,
                   BMHeader **out_geom, int out_geom_cap)
    {
        using namespace blender;
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "mirror geom=%eb merge_dist=%f axis=%i "
                          "mirror_u=%b mirror_v=%b mirror_udim=%b",
                          geom,
                          geom_len,
                          merge_dist,
                          axis,
                          mirror_u,
                          mirror_v,
                          mirror_udim))
        {
            return -1;
        }

        /* The `matrix` (BMO_OP_SLOT_MAT) slot is the transform space the
         * reflection is applied in. Set it explicitly so the 16-float
         * column-major buffer is forwarded verbatim; fall back to identity
         * when the caller passes null. */
        float mat[4][4];
        if (matrix)
        {
            for (int i = 0; i < 16; i++)
            {
                mat[i / 4][i % 4] = matrix[i];
            }
        }
        else
        {
            for (int i = 0; i < 16; i++)
            {
                mat[i / 4][i % 4] = (i / 4 == i % 4) ? 1.0f : 0.0f;
            }
        }
        BMO_slot_mat_set(&op, op.slots_in, "matrix", &mat[0][0], 4);

        BMO_op_exec(bm, &op);

        /* Walk the `geom.out` element buffer (mirrored, post-weld geometry). */
        int geom_count = 0;
        {
            BMOIter oiter;
            BMHeader *ele = static_cast<BMHeader *>(
                BMO_iter_new(&oiter, op.slots_out, "geom.out", BM_ALL_NOLOOP));
            for (; ele; ele = static_cast<BMHeader *>(BMO_iter_step(&oiter)))
            {
                if (geom_count < out_geom_cap)
                {
                    out_geom[geom_count] = ele;
                }
                geom_count++;
            }
        }

        BMO_op_finish(bm, &op);
        return geom_count;
    }

    /* Invoke BMesh's `transform` operator: apply the 4x4 `matrix` to the
     * positions of every vertex in `verts`, optionally inside the local
     * frame given by `space`. See shim.h for the slot mapping, the
     * column-major matrix layout, and the null/empty handling. */
    void bms_transform(BMesh *bm,
                       BMVert **verts, int verts_len,
                       const float *matrix,
                       const float *space,
                       bool use_shapekey)
    {
        using namespace blender;
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "transform verts=%eb use_shapekey=%b",
                          verts,
                          verts_len,
                          use_shapekey))
        {
            return;
        }

        /* Both `matrix` (BMO_OP_SLOT_MAT) and `space` are forwarded as
         * 16-float column-major buffers. A null `matrix` falls back to
         * identity; a null `space` becomes the all-zeros sentinel the
         * operator reads as "no space transform" (it is skipped, not
         * inverted). */
        float mat[4][4];
        if (matrix)
        {
            for (int i = 0; i < 16; i++)
            {
                mat[i / 4][i % 4] = matrix[i];
            }
        }
        else
        {
            for (int i = 0; i < 16; i++)
            {
                mat[i / 4][i % 4] = (i / 4 == i % 4) ? 1.0f : 0.0f;
            }
        }
        BMO_slot_mat_set(&op, op.slots_in, "matrix", &mat[0][0], 4);

        float mat_space[4][4];
        for (int i = 0; i < 16; i++)
        {
            mat_space[i / 4][i % 4] = space ? space[i] : 0.0f;
        }
        BMO_slot_mat_set(&op, op.slots_in, "space", &mat_space[0][0], 4);

        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
    }

    /* Invoke BMesh's `smooth_vert` operator: relax each input vertex toward
     * the unweighted average of its connected neighbours. See shim.h for the
     * slot mapping, the clipping behaviour, and the per-axis masking. All
     * slots are settable directly in the BMO_op_initf format string, so no
     * separate BMO_slot_*_set calls are needed. */
    void bms_smooth_vert(BMesh *bm,
                         BMVert **verts, int verts_len,
                         float factor,
                         bool mirror_clip_x, bool mirror_clip_y, bool mirror_clip_z,
                         float clip_dist,
                         bool use_axis_x, bool use_axis_y, bool use_axis_z)
    {
        using namespace blender;
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "smooth_vert verts=%eb factor=%f "
                          "mirror_clip_x=%b mirror_clip_y=%b mirror_clip_z=%b "
                          "clip_dist=%f "
                          "use_axis_x=%b use_axis_y=%b use_axis_z=%b",
                          verts,
                          verts_len,
                          factor,
                          mirror_clip_x,
                          mirror_clip_y,
                          mirror_clip_z,
                          clip_dist,
                          use_axis_x,
                          use_axis_y,
                          use_axis_z))
        {
            return;
        }

        BMO_op_exec(bm, &op);
        BMO_op_finish(bm, &op);
    }

    /* Invoke BMesh's `symmetrize` operator: bisect `geom` along the plane
     * selected by `direction`, keep the named half, mirror it across the
     * plane, and weld at the seam within `dist`. See shim.h for the slot
     * mapping, the direction enum values, and the read-back convention. */
    int bms_symmetrize(BMesh *bm,
                       BMHeader **geom, int geom_len,
                       int direction,
                       float dist,
                       bool use_shapekey,
                       BMHeader **out_geom, int out_geom_cap)
    {
        using namespace blender;
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "symmetrize input=%eb direction=%i dist=%f "
                          "use_shapekey=%b",
                          geom,
                          geom_len,
                          direction,
                          dist,
                          use_shapekey))
        {
            return -1;
        }

        BMO_op_exec(bm, &op);

        /* Walk the `geom.out` element buffer (the symmetric, post-weld
         * geometry). */
        int geom_count = 0;
        {
            BMOIter oiter;
            BMHeader *ele = static_cast<BMHeader *>(
                BMO_iter_new(&oiter, op.slots_out, "geom.out", BM_ALL_NOLOOP));
            for (; ele; ele = static_cast<BMHeader *>(BMO_iter_step(&oiter)))
            {
                if (geom_count < out_geom_cap)
                {
                    out_geom[geom_count] = ele;
                }
                geom_count++;
            }
        }

        BMO_op_finish(bm, &op);
        return geom_count;
    }

    /* Invoke BMesh's `bisect_plane` operator: slice `geom` by the plane
     * through `plane_co` with normal `plane_no`, optionally snapping on-plane
     * verts and clearing the inner / outer half. See shim.h for the slot
     * mapping and the two-output read-back convention. */
    int bms_bisect_plane(BMesh *bm,
                         BMHeader **geom, int geom_len,
                         const float *plane_co,
                         const float *plane_no,
                         float dist,
                         bool use_snap_center,
                         bool clear_inner,
                         bool clear_outer,
                         BMHeader **out_geom, int out_geom_cap,
                         BMHeader **out_cut, int out_cut_cap, int *out_cut_len)
    {
        using namespace blender;
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "bisect_plane geom=%eb plane_co=%v plane_no=%v "
                          "dist=%f use_snap_center=%b clear_inner=%b "
                          "clear_outer=%b",
                          geom,
                          geom_len,
                          const_cast<float *>(plane_co),
                          const_cast<float *>(plane_no),
                          dist,
                          use_snap_center,
                          clear_inner,
                          clear_outer))
        {
            return -1;
        }

        BMO_op_exec(bm, &op);

        /* Walk the `geom.out` element buffer (the full surviving geometry). */
        int geom_count = 0;
        {
            BMOIter oiter;
            BMHeader *ele = static_cast<BMHeader *>(
                BMO_iter_new(&oiter, op.slots_out, "geom.out", BM_ALL_NOLOOP));
            for (; ele; ele = static_cast<BMHeader *>(BMO_iter_step(&oiter)))
            {
                if (geom_count < out_geom_cap)
                {
                    out_geom[geom_count] = ele;
                }
                geom_count++;
            }
        }

        /* Walk the `geom_cut.out` element buffer (the on-plane cut seam). */
        int cut_count = 0;
        {
            BMOIter oiter;
            BMHeader *ele = static_cast<BMHeader *>(BMO_iter_new(
                &oiter, op.slots_out, "geom_cut.out", BM_VERT | BM_EDGE));
            for (; ele; ele = static_cast<BMHeader *>(BMO_iter_step(&oiter)))
            {
                if (cut_count < out_cut_cap)
                {
                    out_cut[cut_count] = ele;
                }
                cut_count++;
            }
        }
        if (out_cut_len)
        {
            *out_cut_len = cut_count;
        }

        BMO_op_finish(bm, &op);
        return geom_count;
    }

    /* ---- Bridge loops (BMesh operator: bridge_loops) ---- */

    bool bms_bridge_loops(BMesh *bm,
                          BMEdge **edges, int edges_len,
                          bool use_pairs,
                          bool use_cyclic,
                          bool use_merge,
                          float merge_factor,
                          int twist_offset)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "bridge_loops edges=%eb use_pairs=%b use_cyclic=%b "
                          "use_merge=%b merge_factor=%f twist_offset=%i",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          use_pairs,
                          use_cyclic,
                          use_merge,
                          double(merge_factor),
                          twist_offset))
        {
            return false;
        }
        BMO_op_exec(bm, &op);
        const bool cancelled =
            BMO_error_occurred_at_level(bm, BMO_ERROR_CANCEL);
        if (cancelled)
        {
            BMO_error_clear(bm);
        }
        BMO_op_finish(bm, &op);
        return !cancelled;
    }

    int bms_bridge_loops_out(BMesh *bm,
                             BMEdge **edges, int edges_len,
                             bool use_pairs,
                             bool use_cyclic,
                             bool use_merge,
                             float merge_factor,
                             int twist_offset,
                             BMFace **faces_out, int faces_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "bridge_loops edges=%eb use_pairs=%b use_cyclic=%b "
                          "use_merge=%b merge_factor=%f twist_offset=%i",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          use_pairs,
                          use_cyclic,
                          use_merge,
                          double(merge_factor),
                          twist_offset))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);
        if (BMO_error_occurred_at_level(bm, BMO_ERROR_CANCEL))
        {
            BMO_error_clear(bm);
            BMO_op_finish(bm, &op);
            return -1;
        }

        BMOpSlot *slot = BMO_slot_get(op.slots_out, "faces.out");
        const int n = slot->len;
        const int n_copy = (n < faces_cap) ? n : faces_cap;
        BMFace **slot_items = reinterpret_cast<BMFace **>(slot->data.buf);
        for (int i = 0; i < n_copy; ++i)
        {
            faces_out[i] = slot_items[i];
        }

        BMO_op_finish(bm, &op);
        return n;
    }

    int bms_bridge_loops_edges_out(BMesh *bm,
                                   BMEdge **edges, int edges_len,
                                   bool use_pairs,
                                   bool use_cyclic,
                                   bool use_merge,
                                   float merge_factor,
                                   int twist_offset,
                                   BMEdge **edges_out, int edges_cap)
    {
        BMOperator op;
        if (!BMO_op_initf(bm,
                          &op,
                          BMO_FLAG_DEFAULTS,
                          "bridge_loops edges=%eb use_pairs=%b use_cyclic=%b "
                          "use_merge=%b merge_factor=%f twist_offset=%i",
                          reinterpret_cast<BMHeader **>(edges),
                          edges_len,
                          use_pairs,
                          use_cyclic,
                          use_merge,
                          double(merge_factor),
                          twist_offset))
        {
            return -1;
        }
        BMO_op_exec(bm, &op);
        if (BMO_error_occurred_at_level(bm, BMO_ERROR_CANCEL))
        {
            BMO_error_clear(bm);
            BMO_op_finish(bm, &op);
            return -1;
        }

        BMOpSlot *slot = BMO_slot_get(op.slots_out, "edges.out");
        const int n = slot->len;
        const int n_copy = (n < edges_cap) ? n : edges_cap;
        BMEdge **slot_items = reinterpret_cast<BMEdge **>(slot->data.buf);
        for (int i = 0; i < n_copy; ++i)
        {
            edges_out[i] = slot_items[i];
        }

        BMO_op_finish(bm, &op);
        return n;
    }

    /* ---- Whole-mesh traversal micro-workloads ---- */

    /* Count the edges in every vertex's disk cycle; total = 2 * totedge. */
    uint64_t bms_bench_disk_walk_sum(BMesh *bm)
    {
        uint64_t total = 0;
        BMVert *v;
        BMIter v_iter;
        BM_ITER_MESH(v, &v_iter, bm, BM_VERTS_OF_MESH)
        {
            BMEdge *e;
            BMIter e_iter;
            BM_ITER_ELEM(e, &e_iter, v, BM_EDGES_OF_VERT)
            {
                (void)e;
                total++;
            }
        }
        return total;
    }

    /* Count the loops in every edge's radial cycle; total = totloop. */
    uint64_t bms_bench_radial_walk_sum(BMesh *bm)
    {
        uint64_t total = 0;
        BMEdge *e;
        BMIter e_iter;
        BM_ITER_MESH(e, &e_iter, bm, BM_EDGES_OF_MESH)
        {
            BMLoop *l;
            BMIter l_iter;
            BM_ITER_ELEM(l, &l_iter, e, BM_LOOPS_OF_EDGE)
            {
                (void)l;
                total++;
            }
        }
        return total;
    }

    /* Whole-mesh coordinate checksum, accumulated in double. */
    double bms_bench_vert_position_sum(BMesh *bm)
    {
        double sum = 0.0;
        BMVert *v;
        BMIter iter;
        BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH)
        {
            sum += double(v->co[0]) + double(v->co[1]) + double(v->co[2]);
        }
        return sum;
    }

    /* ---- Guarded-allocator bookkeeping ---- */
    /* MEM_get_memory_* are function pointers (the guarded allocator's
     * dispatch table), hence the explicit call-through wrappers. */

    unsigned int bms_mem_blocks_in_use(void)
    {
        return MEM_get_memory_blocks_in_use();
    }

    size_t bms_mem_in_use(void)
    {
        return MEM_get_memory_in_use();
    }

} /* extern "C" */
