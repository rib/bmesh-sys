/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Stub for BLO_read_write.hh tracking the customdata.cc surface of the
 * currently vendored Blender (see README.md for the exact pin).
 *
 * bmesh-sys never serialises a mesh to a .blend file, so customdata.cc's
 * blend_write / blend_read paths are never invoked. The real header entangles
 * with the DNA SDNA type-id template machinery and the loader internals, so
 * we provide a minimal stand-in: no-op BlendWriter / BlendDataReader plus the
 * exact set of free functions and macros customdata.cc bottoms out in.
 * Declarations here are strictly what the vendored customdata.cc references;
 * if a Blender pin bump introduces a new BLO symbol the build fails with
 * "not declared in this scope" pointing right back at this file.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "BLI_implicit_sharing.h"

namespace blender
{

    /* Writer: customdata.cc only invokes the templated write_struct* methods on
     * it. The typed array writers (write_float_array etc.) are the free-function
     * BLO_write_*_array declarations further down. */
    struct BlendWriter
    {
        template <typename T>
        void write_struct(const T * /*p*/) {}
        template <typename T>
        void write_struct_array(int64_t /*count*/, const T * /*p*/) {}
        template <typename T>
        void write_struct_array_at_address(int64_t /*count*/, const T * /*addr*/, const T * /*p*/) {}
        /* Non-template: customdata.cc passes an untyped `void *` for the data,
         * so there is no T to deduce. */
        void write_struct_array_by_name(const char * /*name*/, int64_t /*count*/, const void * /*p*/) {}
    };

    struct BlendDataReader
    {
    };

    inline bool BLO_write_is_undo(BlendWriter * /*writer*/)
    {
        return false;
    }

    inline void BLO_write_float_array(BlendWriter * /*w*/, int64_t /*num*/, const float * /*p*/) {}
    inline void BLO_write_float3_array(BlendWriter * /*w*/, int64_t /*num*/, const float * /*p*/) {}
    inline void BLO_write_int8_array(BlendWriter * /*w*/, int64_t /*num*/, const int8_t * /*p*/) {}
    inline void BLO_write_uint8_array(BlendWriter * /*w*/, int64_t /*num*/, const uint8_t * /*p*/) {}

    template <typename Fn>
    inline void BLO_write_shared(BlendWriter * /*writer*/,
                                 const void * /*data*/,
                                 size_t /*size_in_bytes*/,
                                 const ImplicitSharingInfo * /*sharing_info*/,
                                 Fn && /*write_fn*/) {}

    /* Stubs leave the output pointer untouched — the read path is never
     * executed in this build. */
    inline void BLO_read_float_array(BlendDataReader * /*r*/, int64_t /*size*/, float ** /*p*/) {}
    inline void BLO_read_float3_array(BlendDataReader * /*r*/, int64_t /*size*/, float ** /*p*/) {}
    inline void BLO_read_int8_array(BlendDataReader * /*r*/, int64_t /*size*/, int8_t ** /*p*/) {}
    inline void BLO_read_uint8_array(BlendDataReader * /*r*/, int64_t /*size*/, uint8_t ** /*p*/) {}

    inline void *BLO_read_struct_by_name_array(BlendDataReader * /*reader*/,
                                               const char * /*name*/,
                                               int /*count*/,
                                               void * /*ptr*/)
    {
        return nullptr;
    }

    template <typename Fn>
    inline const ImplicitSharingInfo *BLO_read_shared(BlendDataReader * /*reader*/,
                                                      void ** /*ptr*/,
                                                      Fn &&fn)
    {
        return fn();
    }

} // namespace blender

/* In real Blender these are function-like macros taking a (bare) DNA struct
 * name. We make them no-ops since the read path is unreachable. */
#define BLO_read_struct(reader, struct_name, ptr) ((void)0)
#define BLO_read_struct_array(reader, struct_name, array_size, ptr) ((void)0)
