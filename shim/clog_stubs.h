/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Stub out Blender's logging / translation systems so we can compile bmesh
 * without pulling in the clog or blentranslation subsystems.
 *
 * Force-included into every .cc translation unit via cc::Build flags
 * (`-include shim/clog_stubs.h`).
 */
#ifndef BMESH_SYS_CLOG_STUBS_H
#define BMESH_SYS_CLOG_STUBS_H

/* ---- CLOG (Blender's logging framework) ---- */
#define CLG_LOG_LEVEL 0
/* CLG_LogRef in real Blender is a struct with at least a `category` field
 * initialized from a string literal: `static CLG_LogRef LOG = {"foo"};`.
 * Provide the same shape so customdata.cc compiles without modification. */
typedef struct CLG_LogRef_stub_t
{
    const char *category;
} CLG_LogRef;
#define CLOG_INFO(...) ((void)0)
#define CLOG_VERBOSE(...) ((void)0)
#define CLOG_DEBUG(...) ((void)0)
#define CLOG_STR_INFO(...) ((void)0)
#define CLOG_STR_VERBOSE(...) ((void)0)
#define CLOG_STR_DEBUG(...) ((void)0)
#define CLOG_WARN(...) ((void)0)
#define CLOG_ERROR(...) ((void)0)
#define CLOG_FATAL(...) ((void)0)
#define CLOG_STR_WARN(...) ((void)0)
#define CLOG_STR_ERROR(...) ((void)0)
#define CLOG_STR_FATAL(...) ((void)0)
#define CLG_LOG_REF_DECLARE_GLOBAL(name, id) static int name##_dummy = 0
#define CLG_LOG_REF_DECLARE(name, id) static int name##_dummy = 0

/* ---- Stubs for bmesh tools/ symbols (tools/ tree not vendored) ----
 *
 * bmesh_polygon.cc calls BM_verts_calc_rotate_beauty inside
 * BM_face_triangulate. Declaration here, definition in shim.cc.
 */
#ifdef __cplusplus
namespace blender
{
    struct BMVert;
    float BM_verts_calc_rotate_beauty(const BMVert *v1,
                                      const BMVert *v2,
                                      const BMVert *v3,
                                      const BMVert *v4,
                                      short flag,
                                      short method);
} // namespace blender
#endif

/* ---- blentranslation ---- */
#define DATA_(msg) msg
#define IFACE_(msg) msg
#define TIP_(msg) msg
#define N_(msg) msg
#define BLT_translate_do(ctx, msg) msg
#define BLT_translate_do_iface(ctx, msg) msg
#define BLT_translate_do_tooltip(ctx, msg) msg

#endif /* BMESH_SYS_CLOG_STUBS_H */
