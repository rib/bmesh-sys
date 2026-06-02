/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Instrumented variant of BMesh's region-inset operator.
 *
 * When the region-inset operator runs with `use_interpolate` enabled and
 * the loop CustomData carries a layer whose type defines a "math"
 * (mixable) interpolation, it performs a per-corner customdata-merge step
 * at every moved fan hub. That step reconciles diverged loop values where
 * the two inner loops meet at a hub by averaging the two "inner inset"
 * loops one step behind them and writing the blended value back onto all
 * four loops.
 *
 * This translation unit exposes that otherwise-internal merge step for
 * inspection. It works in two phases on a single mesh:
 *
 *   1. It runs the vendored region-inset body up to (but not including)
 *      the merge, by re-compiling that body here with its math-detection
 *      gate forced off. This leaves every connecting face's loops carrying
 *      the post-interpolation, pre-merge values.
 *
 *   2. It then walks the connecting faces the operator produced and, for
 *      each fan hub, identifies the four participating loops
 *      (`l_a_inner`, `l_b_inner`, `l_a_inner_inset`, `l_b_inner_inset`),
 *      records a named loop layer's value on each, performs the same merge
 *      the operator would have, and records the value on each loop again.
 *
 * The connecting-face traversal mirrors the operator's own loop stepping,
 * and the merge mirrors the operator's reconciliation logic, calling the
 * same public CustomData primitives in the same order. The pristine
 * operator (compiled separately as its own translation unit) is left
 * untouched; the copy built here lives under a renamed exec symbol and is
 * reached only through `bms_inset_region_merge_trace`.
 */

#include "bmesh.hh"
#include "intern/bmesh_operator_api.hh"
#include "BKE_customdata.hh"
#include "DNA_customdata_types.h"
#include "DNA_modifier_enums.h"

#include <cstdlib>
#include <cstring>

using namespace blender; // NOLINT(google-build-using-namespace)

/* Suppress the operator's customdata-merge by forcing its math gate off
 * for the re-compiled copy below. The operator computes
 * `has_math_ldata = use_interpolate && CustomData_has_math(...)` once and
 * uses it solely to guard the merge; with this returning false the
 * post-interpolation, pre-merge state is preserved for inspection.
 *
 * Declared in `namespace blender` so the macro-redirected call inside the
 * re-compiled operator body (which lives in that namespace) resolves to
 * it. The real `CustomData_has_math` is unaffected in every other
 * translation unit. */
namespace blender {
inline bool bms__inset_has_math_false(const CustomData * /*data*/)
{
  return false;
}
} // namespace blender

#define CustomData_has_math bms__inset_has_math_false
#define bmo_inset_region_exec bms__inset_region_exec_nomerge
#define bmo_inset_individual_exec bms__inset_individual_exec_nomerge
#include "operators/bmo_inset.cc"
#undef CustomData_has_math
#undef bmo_inset_region_exec
#undef bmo_inset_individual_exec

namespace {

/* Read up to four float components of a loop layer at `offset`. */
inline void read_layer(const BMLoop *l, int offset, int comps, float out[4])
{
  out[0] = out[1] = out[2] = out[3] = 0.0f;
  if (offset < 0 || l == nullptr) {
    return;
  }
  const float *p = reinterpret_cast<const float *>(
      reinterpret_cast<const char *>(l->head.data) + offset);
  for (int i = 0; i < comps; i++) {
    out[i] = p[i];
  }
}

/* One recorded merge invocation with loops held as pointers; resolved to
 * mesh-wide indices after the operator finishes and tables are rebuilt. */
struct TraceEntryRaw {
  BMVert *vert;
  BMLoop *l_a_inner;
  BMLoop *l_b_inner;
  BMLoop *l_a_inner_inset;
  BMLoop *l_b_inner_inset;
  float a_inner_pre[4], a_inner_post[4];
  float b_inner_pre[4], b_inner_post[4];
  float a_inner_inset_pre[4], a_inner_inset_post[4];
  float b_inner_inset_pre[4], b_inner_inset_post[4];
  int comps;
};

/* Public per-invocation record (mirrors `BmsMergeInvocation` in shim.h). */
struct MergeInvocation {
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
};

struct MergeTrace {
  MergeInvocation *invocations;
  int len;
  int cap;
};

struct TraceState {
  int offset;
  int comps;
  TraceEntryRaw *entries;
  int len;
  int cap;
};

TraceEntryRaw *trace_push(TraceState &s)
{
  if (s.len == s.cap) {
    int new_cap = s.cap ? s.cap * 2 : 8;
    s.entries = static_cast<TraceEntryRaw *>(
        std::realloc(s.entries, sizeof(TraceEntryRaw) * size_t(new_cap)));
    s.cap = new_cap;
  }
  TraceEntryRaw *e = &s.entries[s.len++];
  std::memset(e, 0, sizeof(*e));
  return e;
}

/* Perform the operator's per-corner customdata merge at one fan hub, on
 * the post-interpolation/pre-merge mesh, recording the four participating
 * loops and the traced layer's value before/after the merge writes.
 *
 *   l_a_outer / l_b_outer  — the two outer loops on the side edge
 *   l_a_inner / l_b_inner  — the two inner loops meeting at the hub
 *   l_a_inner_inset /
 *   l_b_inner_inset        — the loops one step behind each inner loop,
 *                            across the inset side edges `e_a` / `e_b`
 *
 * Mirrors the operator's reconciliation logic; calls the same public
 * CustomData primitives in the same order, adding only pre/post capture. */
void merge_hub(BMesh *bm,
               TraceState &trace,
               BMLoop *l_a_outer,
               BMLoop *l_b_outer,
               BMLoop *l_a_inner,
               BMLoop *l_b_inner)
{
  const bool is_flip = (l_a_inner->next == l_a_outer);
  BMEdge *e_a, *e_b;

  if (is_flip) {
    e_a = l_a_inner->prev->e;
    e_b = l_b_inner->e;
  }
  else {
    e_a = l_a_inner->e;
    e_b = l_b_inner->prev->e;
  }

  BMLoop *l_a_inner_inset = BM_edge_other_loop(e_a, l_a_inner);
  BMLoop *l_b_inner_inset = BM_edge_other_loop(e_b, l_b_inner);

  /* No chance of divergence — the operator early-outs here. */
  if (l_a_inner_inset->f == l_b_inner_inset->f) {
    return;
  }

  TraceEntryRaw *entry = trace_push(trace);
  entry->vert = l_a_inner->v;
  entry->l_a_inner = l_a_inner;
  entry->l_b_inner = l_b_inner;
  entry->l_a_inner_inset = l_a_inner_inset;
  entry->l_b_inner_inset = l_b_inner_inset;
  entry->comps = trace.comps;
  read_layer(l_a_inner, trace.offset, trace.comps, entry->a_inner_pre);
  read_layer(l_b_inner, trace.offset, trace.comps, entry->b_inner_pre);
  read_layer(l_a_inner_inset, trace.offset, trace.comps, entry->a_inner_inset_pre);
  read_layer(l_b_inner_inset, trace.offset, trace.comps, entry->b_inner_inset_pre);

  for (int layer_n = 0; layer_n < bm->ldata.totlayer; layer_n++) {
    const int type = bm->ldata.layers[layer_n].type;
    const int offset = bm->ldata.layers[layer_n].offset;
    if (!CustomData_layer_has_math(&bm->ldata, layer_n)) {
      continue;
    }

    if (CustomData_data_equals(
            eCustomDataType(type),
            reinterpret_cast<char *>(l_a_outer->head.data) + offset,
            reinterpret_cast<char *>(l_b_outer->head.data) + offset) == true)
    {
      CustomData_data_mix_value(
          eCustomDataType(type),
          reinterpret_cast<char *>(l_a_inner_inset->head.data) + offset,
          reinterpret_cast<char *>(l_b_inner_inset->head.data) + offset,
          CDT_MIX_MIX,
          0.5f);
      CustomData_data_copy_value(
          eCustomDataType(type),
          reinterpret_cast<char *>(l_a_inner_inset->head.data) + offset,
          reinterpret_cast<char *>(l_b_inner_inset->head.data) + offset);

      const void *data_src = reinterpret_cast<char *>(l_a_inner_inset->head.data) + offset;

      if (is_flip ? (l_b_inner_inset->e == l_a_inner_inset->prev->e) :
                    (l_a_inner_inset->e == l_b_inner_inset->prev->e))
      {
        /* simple case — all loops already in hand */
      }
      else {
        BMIter iter;
        BMLoop *l_iter;
        const void *data_cmp_a = reinterpret_cast<char *>(l_b_inner->head.data) + offset;
        const void *data_cmp_b = reinterpret_cast<char *>(l_a_inner->head.data) + offset;
        BM_ITER_ELEM (l_iter, &iter, l_a_inner_inset->v, BM_LOOPS_OF_VERT) {
          if (BM_elem_flag_test(l_iter->f, BM_ELEM_TAG)) {
            if (!ELEM(l_iter, l_a_inner, l_b_inner, l_a_inner_inset, l_b_inner_inset)) {
              void *data_dst = reinterpret_cast<char *>(l_iter->head.data) + offset;
              if (CustomData_data_equals(eCustomDataType(type), data_dst, data_cmp_a) ||
                  CustomData_data_equals(eCustomDataType(type), data_dst, data_cmp_b))
              {
                CustomData_data_copy_value(eCustomDataType(type), data_src, data_dst);
              }
            }
          }
        }
      }

      CustomData_data_copy_value(eCustomDataType(type),
                                 data_src,
                                 reinterpret_cast<char *>(l_b_inner->head.data) + offset);
      CustomData_data_copy_value(eCustomDataType(type),
                                 data_src,
                                 reinterpret_cast<char *>(l_a_inner->head.data) + offset);
    }
  }

  read_layer(l_a_inner, trace.offset, trace.comps, entry->a_inner_post);
  read_layer(l_b_inner, trace.offset, trace.comps, entry->b_inner_post);
  read_layer(l_a_inner_inset, trace.offset, trace.comps, entry->a_inner_inset_post);
  read_layer(l_b_inner_inset, trace.offset, trace.comps, entry->b_inner_inset_post);
}

} // namespace

extern "C" {

/* Defined here (not shim.cc) so it shares the renamed exec symbol.
 * Returns 1 on success, 0 if the operator rejected the input, -1 on a
 * usage error (null mesh / output). */
int bms_inset_region_merge_trace(BMesh *bm,
                                 BMHeader **faces,
                                 int faces_len,
                                 float thickness,
                                 float depth,
                                 int flags,
                                 const char *layer_name,
                                 MergeTrace *out)
{
  if (!bm || !out) {
    return -1;
  }

  enum {
    F_USE_BOUNDARY = 1 << 0,
    F_USE_EVEN_OFFSET = 1 << 1,
    F_USE_RELATIVE_OFFSET = 1 << 2,
    F_USE_EDGE_RAIL = 1 << 3,
    F_USE_OUTSET = 1 << 4,
  };

  BMIter it;
  BMFace *f;
  BM_ITER_MESH (f, &it, bm, BM_FACES_OF_MESH) {
    BM_elem_flag_disable(f, BM_ELEM_TAG);
    BM_face_normal_update(f);
  }
  for (int i = 0; i < faces_len; i++) {
    BM_elem_flag_enable(reinterpret_cast<BMFace *>(faces[i]), BM_ELEM_TAG);
  }

  /* Resolve the traced layer (any float/float2/float3/color loop layer).
   * Absent layer is not an error: the trace still records loop identities
   * with zeroed values. */
  int t_offset = -1;
  int t_comps = 0;
  if (layer_name) {
    const struct {
      eCustomDataType type;
      int comps;
    } candidates[] = {
        {CD_PROP_FLOAT2, 2},
        {CD_PROP_FLOAT3, 3},
        {CD_PROP_FLOAT, 1},
        {CD_PROP_COLOR, 4},
    };
    for (const auto &c : candidates) {
      int off = CustomData_get_offset_named(&bm->ldata, c.type, layer_name);
      if (off != -1) {
        t_offset = off;
        t_comps = c.comps;
        break;
      }
    }
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
                    bool(flags & F_USE_BOUNDARY),
                    bool(flags & F_USE_EVEN_OFFSET),
                    true, /* use_interpolate — the whole point of the trace */
                    bool(flags & F_USE_RELATIVE_OFFSET),
                    bool(flags & F_USE_EDGE_RAIL),
                    bool(flags & F_USE_OUTSET),
                    double(thickness),
                    double(depth)))
  {
    return 0;
  }

  /* Route the exec through the copy whose merge gate is forced off, so the
   * mesh is left in its post-interpolation, pre-merge state. */
  op.exec = bms__inset_region_exec_nomerge;
  BMO_op_exec(bm, &op);

  TraceState trace;
  trace.offset = t_offset;
  trace.comps = t_comps;
  trace.entries = nullptr;
  trace.len = 0;
  trace.cap = 0;

  /* Walk the connecting faces the operator produced. For region inset the
   * only faces it creates are these connectors, so the output set is
   * exactly the set of fan-hub carriers. The per-face loop stepping and
   * the two hub derivations (sides 'a' and 'b') mirror the operator. */
  BMOIter oiter;
  BMFace *fc;
  BMO_ITER (fc, &oiter, op.slots_out, "faces.out", BM_FACE) {
    if (fc->len < 2) {
      continue;
    }
    BMLoop *l0 = BM_FACE_FIRST_LOOP(fc);
    BMLoop *l_a = l0->next->next;
    BMLoop *l_b = l_a->next;

    BMEdge *e_connect_a = l_a->prev->e;
    if (BM_edge_is_manifold(e_connect_a)) {
      merge_hub(bm, trace, l_a, BM_edge_other_loop(e_connect_a, l_a), l_a->prev,
                BM_edge_other_loop(e_connect_a, l_a->prev));
    }

    BMEdge *e_connect_b = l_b->e;
    if (BM_edge_is_manifold(e_connect_b)) {
      merge_hub(bm, trace, l_b, BM_edge_other_loop(e_connect_b, l_b), l_b->next,
                BM_edge_other_loop(e_connect_b, l_b->next));
    }
  }

  BMO_op_finish(bm, &op);

  /* Rebuild index tables so the captured pointers resolve to stable
   * mesh-wide indices for the caller. */
  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_FACE | BM_LOOP);

  out->len = trace.len;
  out->cap = trace.len;
  out->invocations = nullptr;
  if (trace.len > 0) {
    out->invocations = static_cast<MergeInvocation *>(
        std::malloc(sizeof(MergeInvocation) * size_t(trace.len)));
    for (int i = 0; i < trace.len; i++) {
      const TraceEntryRaw &r = trace.entries[i];
      MergeInvocation &m = out->invocations[i];
      std::memset(&m, 0, sizeof(m));
      m.vert_index = r.vert ? BM_elem_index_get(r.vert) : -1;
      m.a_inner_face = r.l_a_inner ? BM_elem_index_get(r.l_a_inner->f) : -1;
      m.a_inner_corner_vert = r.l_a_inner ? BM_elem_index_get(r.l_a_inner->v) : -1;
      m.b_inner_face = r.l_b_inner ? BM_elem_index_get(r.l_b_inner->f) : -1;
      m.b_inner_corner_vert = r.l_b_inner ? BM_elem_index_get(r.l_b_inner->v) : -1;
      m.a_inner_inset_face = r.l_a_inner_inset ? BM_elem_index_get(r.l_a_inner_inset->f) : -1;
      m.a_inner_inset_corner_vert =
          r.l_a_inner_inset ? BM_elem_index_get(r.l_a_inner_inset->v) : -1;
      m.b_inner_inset_face = r.l_b_inner_inset ? BM_elem_index_get(r.l_b_inner_inset->f) : -1;
      m.b_inner_inset_corner_vert =
          r.l_b_inner_inset ? BM_elem_index_get(r.l_b_inner_inset->v) : -1;
      std::memcpy(m.a_inner_pre, r.a_inner_pre, sizeof(m.a_inner_pre));
      std::memcpy(m.a_inner_post, r.a_inner_post, sizeof(m.a_inner_post));
      std::memcpy(m.b_inner_pre, r.b_inner_pre, sizeof(m.b_inner_pre));
      std::memcpy(m.b_inner_post, r.b_inner_post, sizeof(m.b_inner_post));
      std::memcpy(m.a_inner_inset_pre, r.a_inner_inset_pre, sizeof(m.a_inner_inset_pre));
      std::memcpy(m.a_inner_inset_post, r.a_inner_inset_post, sizeof(m.a_inner_inset_post));
      std::memcpy(m.b_inner_inset_pre, r.b_inner_inset_pre, sizeof(m.b_inner_inset_pre));
      std::memcpy(m.b_inner_inset_post, r.b_inner_inset_post, sizeof(m.b_inner_inset_post));
      m.comps = r.comps;
    }
  }

  std::free(trace.entries);
  return 1;
}

void bms_merge_trace_free(MergeTrace *out)
{
  if (!out) {
    return;
  }
  std::free(out->invocations);
  out->invocations = nullptr;
  out->len = 0;
  out->cap = 0;
}

} // extern "C"
