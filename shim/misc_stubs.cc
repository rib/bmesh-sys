/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * No-op stubs for various Blender subsystems we don't vendor
 */

#include <cstdio>

#include "DNA_customdata_types.h"

extern "C"
{
    /* leak_detector internals referenced by mallocn_lockfree_impl.cc. */
    bool leak_detector_has_run = false;
    const char *free_after_leak_detection_message =
        "Free called after leak detection (stub)";
} // extern "C"

namespace blender
{

    struct MLoopNorSpaceArray; // forward decl — bmesh_mesh.cc only takes a pointer

    void BKE_lnor_spacearr_free(MLoopNorSpaceArray * /*lnors_spacearr*/) {}

    /* BLI_system.h: prints a backtrace. Just no-op for our standalone build. */
    void BLI_system_backtrace(FILE * /*fp*/) {}

} // namespace blender

namespace blender
{
    /* bmesh_kernel_loop_reverse references this when cd_loop_mdisp_offset != -1.
     * We always pass -1, so the call is unreachable in practice. */
    struct MDisps;
    void BKE_mesh_mdisp_flip(MDisps * /*md*/, bool /*use_loop_mdisp_flip*/) {}

    /* Multires interp helpers referenced from bmesh_interp.cc when an MDISPS
     * customdata layer exists. We never set one up, so these are unreachable. */
    void old_mdisps_bilinear(float * /*out*/,
                             float (* /*disps*/)[3],
                             int /*st*/,
                             float /*u*/,
                             float /*v*/) {}

    struct TaskParallelTLS;
    struct TaskParallelSettings;
    void BLI_task_parallel_range(int /*start*/,
                                 int /*stop*/,
                                 void * /*userdata*/,
                                 void (* /*func*/)(void *, int, const TaskParallelTLS *),
                                 const TaskParallelSettings * /*settings*/) {}

    /* ---- Subsystems referenced by customdata.cc but never reached on our
     *      in-memory, no-multires, no-CD-file path. ---- */

    /* index_mask helper. customdata.cc references it from one of the layer-type
     * registry templates; we never exercise that code. Returning a reference
     * to a static aligned-byte-buffer cast to IndexMask is safe because the
     * caller never dereferences it on our test path. */
    namespace index_mask
    {
        class IndexMask;
        const IndexMask &get_static_index_mask_for_min_size(int64_t /*min_size*/)
        {
            alignas(16) static unsigned char storage[256];
            return *reinterpret_cast<const IndexMask *>(storage);
        }
    } // namespace index_mask

    /* Multires sculpt helpers. customdata.cc only calls these when an MDISPS
     * layer participates — never in our tests. */
    struct MDisps;
    int multires_mdisp_corners(const MDisps * /*md*/)
    {
        return 0;
    }

    /* BKE_defvert_find_weight — vertex-group weight lookup from BKE_deform.cc.
     * Used by tools/bmesh_wireframe.cc inside a `if (cd_dvert_offset != -1)`
     * branch that's only reached when a CD_MDEFORMVERT customdata layer is
     * present. The operator-level entry point (bmo_wireframe_exec) passes
     * `defgrp_index = -1`, which the algorithm uses to skip vertex-group
     * weighting entirely; we never register a CD_MDEFORMVERT layer in any
     * A/B test. Stub returns 1.0 (the "no weighting" default). */
    struct MDeformVert;
    float BKE_defvert_find_weight(const MDeformVert * /*dvert*/, int /*defgroup*/)
    {
        return 1.0f;
    }

    /* BKE_curve_forward_diff_bezier — bezier-curve forward-difference helper
     * from BKE_curve.cc. Referenced by bmo_subdivide_edgering.cc for smooth
     * subdivide. Stub to no-op (the smooth-subdivide path doesn't need to be
     * exact for our A/B tests; linear is fine). */
    void BKE_curve_forward_diff_bezier(float /*q0*/,
                                       float /*q1*/,
                                       float /*q2*/,
                                       float /*q3*/,
                                       float * /*p*/,
                                       int /*it*/,
                                       int /*stride*/) {}

    /* BKE_curveprofile_init — initialises a custom CurveProfile (CD_FLAG-style
     * lookup table for the bevel custom-profile path). Referenced from
     * tools/bmesh_bevel.cc inside set_profile_spacing, but only when
     * `bp->custom_profile != nullptr`. Our shim never passes a custom_profile
     * (default is BEVEL_PROFILE_SUPERELLIPSE), so the call is unreachable. */
    struct CurveProfile;
    void BKE_curveprofile_init(CurveProfile * /*profile*/, short /*segments_len*/) {}

    /* External customdata-file I/O. Only used by the file-backed CD path
     * (`CD_FLAG_EXTERNAL`), which we don't exercise. */
    struct CDataFile;
    bool cdf_read_data(CDataFile * /*cdf*/, unsigned int /*size*/, void * /*buffer*/)
    {
        return false;
    }
    bool cdf_write_data(CDataFile * /*cdf*/, unsigned int /*size*/, const void * /*buffer*/)
    {
        return false;
    }

    /* ---- Symbols pulled in by the vendored operators framework that live on
     *      unreached code paths in our test workflow. ---- */

    /* BLI_task_parallel_mempool is the mempool variant of the parallel task
     * dispatch — bmesh_operators.cc uses it when an operator's exec is
     * parallelisable. Our test path runs the extrude exec single-threaded. */
    struct BLI_mempool;
    struct MempoolIterData;
    struct TaskParallelTLS;
    struct TaskParallelSettings;
    void BLI_task_parallel_mempool(BLI_mempool * /*mempool*/,
                                   void * /*userdata*/,
                                   void (* /*func*/)(void *,
                                                     MempoolIterData *,
                                                     const TaskParallelTLS *),
                                   const TaskParallelSettings * /*settings*/) {}

    /* BM_mesh_normals_update is provided by our own bms_mesh_normals_update
     * helper (see shim.cc); the vendored bmesh subsystem doesn't include the
     * intern/bmesh_mesh_normals.cc that contains the real implementation, and
     * neither the extrude operator nor our test path calls it. Stub. */
    struct BMesh;
    void BM_mesh_normals_update(BMesh * /*bm*/) {}

    /* BM_lnorspace_update / BKE_lnor_space_custom_normal_to_data — loop-normal
     * "space" (mlnors) helpers used by bevel's `harden_normals` path. The shim
     * never sets harden_normals=true, so `bevel_harden_normals` is unreachable;
     * stubs are link-time only. */
    struct MLoopNorSpace;
    void BM_lnorspace_update(BMesh * /*bm*/) {}
    void BKE_lnor_space_custom_normal_to_data(const MLoopNorSpace * /*lnor_space*/,
                                              const float * /*custom_lnor*/,
                                              short * /*r_clnor_data*/) {}

    /* ---- Exec functions for the architecturally-excluded operators. ----
     *
     * vendor/bmesh/intern/bmesh_opdefines.cc is kept byte-for-byte pristine, so
     * its operator table still references the exec functions for operator
     * files we deliberately do NOT compile (see VENDOR.md "Excluded operators"):
     *
     *   - bmo_mesh_convert.cc  mesh_to_bmesh / bmesh_to_mesh / object_load_bmesh
     *
     * Rather than surgically editing the pristine opdefines table, we satisfy
     * the link by defining the remaining excluded exec symbols here as no-ops.
     * The operators stay present in the table (callable as no-ops) but no shim
     * path invokes them.
     *
     * Keeping opdefines.cc pristine should make it easier to update the version
     * of BMesh that we vendor.
     */
    struct BMOperator;
    void bmo_mesh_to_bmesh_exec(BMesh * /*bm*/, BMOperator * /*op*/) {}
    void bmo_bmesh_to_mesh_exec(BMesh * /*bm*/, BMOperator * /*op*/) {}
    void bmo_object_load_bmesh_exec(BMesh * /*bm*/, BMOperator * /*op*/) {}

} // namespace blender
