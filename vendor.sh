#!/usr/bin/env bash
# Vendor the subset of Blender (+ Eigen) source that bmesh-sys compiles into
# its standalone static library via cc::Build (see build.rs).
#
# Usage: from anywhere,
#   ./vendor.sh [path/to/blender/checkout] [path/to/eigen/checkout]
#
# Defaults assume a Blender checkout at ../blender and an Eigen checkout at
# ../eigen relative to this crate.
#
# This script is the single source of truth for *which* upstream files are
# vendored. It is idempotent: it copies pristine upstream over vendor/ every
# time.
#
# With the exception of some mechanical rewrites documented in the "Patches &
# stub overlays" section below, the `vendor/` files are verbatim copies from the
# Blender source. This should make it easy to sync to new Blender versions by
# re-running the script.
#
# Upstream source layout this script reads from:
#   $BLENDER/source/blender/bmesh/                bmesh.hh, bmesh_class.hh, bmesh_tools.hh
#   $BLENDER/source/blender/bmesh/intern/         core kernel + operator framework
#   $BLENDER/source/blender/bmesh/operators/      bmo_*.cc operator exec functions
#   $BLENDER/source/blender/bmesh/tools/          bmesh_*.cc algorithmic backends
#   $BLENDER/source/blender/blenlib/              BLI_*.h(h) headers + intern/ impls
#   $BLENDER/source/blender/blenkernel/           BKE_*.hh headers + intern/customdata.cc
#   $BLENDER/source/blender/blenloader/           BLO_read_write.hh
#   $BLENDER/source/blender/makesdna/             DNA_*_types.h headers
#   $BLENDER/extern/wcwidth/                      wcwidth.h (BLI_string_utf8 dep)
#   $BLENDER/intern/guardedalloc/                 MEM_guardedalloc.h + intern/mallocn*
#   $BLENDER/intern/atomic/                       atomic_ops.h
#   $BLENDER/intern/eigen/                        Blender's thin C wrapper around Eigen
#   $EIGEN/Eigen/                                 Eigen header-only library (MPL-2.0)

set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
BLENDER=${1:-$HERE/../blender}
EIGEN=${2:-$HERE/../eigen}
DST=$HERE/vendor

if [[ ! -d "$BLENDER/source/blender/bmesh" ]]; then
  echo "error: $BLENDER doesn't look like a Blender source checkout" >&2
  exit 1
fi
if [[ ! -d "$EIGEN/Eigen" ]]; then
  echo "error: $EIGEN doesn't look like an Eigen checkout (no Eigen/ dir)" >&2
  exit 1
fi

SRC="$BLENDER/source/blender"
INTERN="$BLENDER/intern"

echo "vendoring blender from $BLENDER"
echo "vendoring eigen   from $EIGEN"
echo "            into  $DST"

# Start from a clean tree. The directories below are populated wholesale by
# globbed copies, which never *delete* files — so without this, syncing to a
# different Blender version (especially an older one) would leave orphaned
# files behind: e.g. an operator added in 5.2 (bmo_circularize.cc) lingering
# after vendoring 5.1, which build.rs would then try to compile against 5.1
# headers. Wiping first guarantees `vendor/` is exactly what this run produces.
# Safe: the BLENDER/EIGEN existence checks above have already passed.
rm -rf "$DST"

mkdir -p \
  "$DST/bmesh/intern" "$DST/bmesh/operators" "$DST/bmesh/tools" \
  "$DST/blenlib" "$DST/blenlib/intern" \
  "$DST/blenkernel" "$DST/blenkernel/intern" \
  "$DST/blenloader" \
  "$DST/dna" \
  "$DST/guardedalloc" "$DST/guardedalloc/intern" \
  "$DST/atomic" "$DST/atomic/intern" \
  "$DST/eigen" "$DST/eigen_capi/intern"

copy_if() {
  local src=$1 dst=$2
  if [[ -f $src ]]; then
    cp "$src" "$dst"
  else
    echo "warn: missing $src" >&2
  fi
}

# Files compiled as their own translation units are listed explicitly here AND
# in build.rs. The two lists must agree; build.rs warns on any missing file.
# Headers are copied wholesale per directory because they cross-include each
# other freely and pruning them tends to break compilation for no real gain.

# ---- bmesh public headers ----------------------------------------------------
# bmesh_tools.hh is the real upstream header now that the tools/ tree is
# vendored (it #includes tools/bmesh_*.hh). It used to be stubbed.
copy_if "$SRC/bmesh/bmesh.hh"        "$DST/bmesh/bmesh.hh"
copy_if "$SRC/bmesh/bmesh_class.hh"  "$DST/bmesh/bmesh_class.hh"
copy_if "$SRC/bmesh/bmesh_tools.hh"  "$DST/bmesh/bmesh_tools.hh"

# ---- bmesh/intern: ALL headers, curated .cc ---------------------------------
cp "$SRC"/bmesh/intern/*.hh "$DST/bmesh/intern/" 2>/dev/null || true
cp "$SRC"/bmesh/intern/*.h  "$DST/bmesh/intern/" 2>/dev/null || true
BMESH_INTERN_CC=(
  bmesh_core.cc bmesh_construct.cc bmesh_delete.cc bmesh_structure.cc
  bmesh_iterators.cc bmesh_query.cc bmesh_marking.cc bmesh_polygon.cc
  bmesh_mesh.cc bmesh_interp.cc bmesh_mods.cc
  # operator framework + supporting infrastructure
  bmesh_operators.cc bmesh_opdefines.cc
  bmesh_walkers.cc bmesh_walkers_impl.cc bmesh_edgeloop.cc
  bmesh_callback_generic.cc bmesh_polygon_edgenet.cc bmesh_mesh_tessellate.cc
)
for f in "${BMESH_INTERN_CC[@]}"; do
  copy_if "$SRC/bmesh/intern/$f" "$DST/bmesh/intern/$f"
done

# ---- bmesh/operators: ALL bmo_*.cc except the architectural excludes --------
# Excluded (see VENDOR.md "Excluded operators"):
#   bmo_mesh_convert.cc  — pulls in the non-BMesh Mesh data subsystem
for f in "$SRC"/bmesh/operators/bmo_*.cc; do
  base=$(basename "$f")
  case "$base" in
    bmo_mesh_convert.cc) continue ;;
  esac
  cp "$f" "$DST/bmesh/operators/$base"
done

# ---- bmesh/tools: ALL headers, ALL .cc except the one Mesh-BVH dependent ----
# Excluded (see VENDOR.md "Excluded tools/ backends"):
#   bmesh_intersect_edges.cc — needs BKE_bvhutils.hh (indirect Mesh dep).
#   Its header bmesh_intersect_edges.hh is still copied (referenced by others).
cp "$SRC"/bmesh/tools/*.hh "$DST/bmesh/tools/" 2>/dev/null || true
for f in "$SRC"/bmesh/tools/bmesh_*.cc; do
  base=$(basename "$f")
  case "$base" in
    bmesh_intersect_edges.cc) continue ;;
  esac
  cp "$f" "$DST/bmesh/tools/$base"
done

# ---- blenlib: ALL headers + ALL intern .cc/.hh/.h ---------------------------
# The full set is small and several BLI headers #include their intern/*_inline.cc
# or intern/*.cc directly, so copying the whole intern/ avoids include surprises.
# build.rs compiles only a curated subset as standalone TUs.
cp "$SRC"/blenlib/*.h  "$DST/blenlib/" 2>/dev/null || true
cp "$SRC"/blenlib/*.hh "$DST/blenlib/" 2>/dev/null || true
cp "$SRC"/blenlib/intern/*.cc "$DST/blenlib/intern/" 2>/dev/null || true
cp "$SRC"/blenlib/intern/*.hh "$DST/blenlib/intern/" 2>/dev/null || true
cp "$SRC"/blenlib/intern/*.h  "$DST/blenlib/intern/" 2>/dev/null || true
# wcwidth.h (BLI_string_utf8.cc: `#include <wcwidth.h>`) is provided as a stub,
# not copied from extern/wcwidth — see the "Stub overlays" section below.

# ---- atomic (header-only) ----------------------------------------------------
cp "$INTERN"/atomic/*.h        "$DST/atomic/" 2>/dev/null || true
cp "$INTERN"/atomic/intern/*.h "$DST/atomic/intern/" 2>/dev/null || true

# ---- guardedalloc: ALL headers, curated .cc ---------------------------------
cp "$INTERN"/guardedalloc/*.h         "$DST/guardedalloc/" 2>/dev/null || true
cp "$INTERN"/guardedalloc/intern/*.hh "$DST/guardedalloc/intern/" 2>/dev/null || true
cp "$INTERN"/guardedalloc/intern/*.h  "$DST/guardedalloc/intern/" 2>/dev/null || true
for f in mallocn.cc mallocn_lockfree_impl.cc memory_usage.cc; do
  copy_if "$INTERN/guardedalloc/intern/$f" "$DST/guardedalloc/intern/$f"
done

# ---- DNA headers (entire set is small) --------------------------------------
cp "$SRC"/makesdna/*.h  "$DST/dna/" 2>/dev/null || true
cp "$SRC"/makesdna/*.hh "$DST/dna/" 2>/dev/null || true

# ---- BKE: ALL headers, curated .cc ------------------------------------------
# Top-level BKE_*.hh/.h plus the few private headers in intern/.
cp "$SRC"/blenkernel/*.h         "$DST/blenkernel/" 2>/dev/null || true
cp "$SRC"/blenkernel/*.hh        "$DST/blenkernel/" 2>/dev/null || true
cp "$SRC"/blenkernel/intern/*.hh "$DST/blenkernel/intern/" 2>/dev/null || true
cp "$SRC"/blenkernel/intern/*.h  "$DST/blenkernel/intern/" 2>/dev/null || true
copy_if "$SRC/blenkernel/intern/customdata.cc" "$DST/blenkernel/intern/customdata.cc"

# ---- blenloader: BLO_read_write.hh is reached by customdata.cc's blend
#      read/write hooks. The real header pulls in the whole .blend
#      serialisation machinery, so it is provided as a stub instead — see the
#      "Stub overlays" section below.

# ---- Eigen (MPL-2.0): header-only library + license files -------------------
rm -rf "$DST/eigen/Eigen"
cp -R "$EIGEN/Eigen" "$DST/eigen/Eigen"
cp "$EIGEN"/COPYING.* "$DST/eigen/" 2>/dev/null || true

# ---- eigen_capi: Blender's thin C wrapper around Eigen ----------------------
# Used by bevel / smooth_laplacian (linear_solver) and math_solvers.cc.
copy_if "$INTERN/eigen/eigen_capi.h" "$DST/eigen_capi/eigen_capi.h"
cp "$INTERN"/eigen/intern/*.cc "$DST/eigen_capi/intern/" 2>/dev/null || true
cp "$INTERN"/eigen/intern/*.h  "$DST/eigen_capi/intern/" 2>/dev/null || true

# ==== Patches & stub overlays applied by this script (NOT manual edits) ======
#
# Everything below is deterministic and re-applied on every run, so re-running
# the script and diffing vendor/ stays clean. The intent is that vendored
# upstream .cc/.hh files are byte-for-byte pristine; anything we'd otherwise
# have to edit lives instead in a separate stub under shim/ (preferred) or, as
# a last resort, an in-place sed rewrite documented here. Document every entry
# here AND in VENDOR.md so a future Blender version bump stays a clean re-run.

# (1) MEM_guardedalloc.h + guardedalloc .cc hard-code the original tree layout
#     via `#include "../../source/blender/blenlib/BLI_*.h"`. Flatten those to
#     plain "BLI_*.h" — blenlib is already on the cc::Build include path.
sed -i -E 's@"\.\./\.\./source/blender/blenlib/@"@g' \
  "$DST/guardedalloc/MEM_guardedalloc.h" \
  "$DST/guardedalloc/intern/"*.cc \
  "$DST/guardedalloc/intern/"*.hh 2>/dev/null || true

# (2) Stub headers that fully replace an upstream header. The replacements live
#     under shim/vendor_stubs/ (version controlled + reviewable); we overlay
#     them so the vendored copy never carries hand edits of upstream content.
#
#       wcwidth.h          — extern/wcwidth declares mk_wcwidth() but its impl
#                            (wcwidth.c) is not compiled. BLI_string_utf8.cc
#                            includes it; the symbol is never exercised on any
#                            A/B path, so the stub provides an inline mk_wcwidth
#                            returning 1 (one column / codepoint).
#       BLO_read_write.hh  — customdata.cc references the .blend read/write API
#                            but bmesh-sys never serialises a mesh; the real
#                            header drags in the whole loader. The stub provides
#                            no-op BlendWriter / BlendDataReader + free fns.
copy_if "$HERE/shim/vendor_stubs/wcwidth.h"         "$DST/blenlib/wcwidth.h"
copy_if "$HERE/shim/vendor_stubs/BLO_read_write.hh" "$DST/blenloader/BLO_read_write.hh"

# (3) BLT_translation.hh — the blentranslation system is not vendored. The
#     translation macros it would provide are stubbed in shim/clog_stubs.h, so
#     this header only has to exist and be empty to satisfy #include lines.
cat > "$DST/blenlib/BLT_translation.hh" <<'EOF'
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Stub: blentranslation not vendored. Macros stubbed in clog_stubs.h. */
#pragma once
EOF

echo
echo "Vendoring done. File counts:"
echo "  bmesh/intern:      $(ls -1 "$DST"/bmesh/intern | wc -l)"
echo "  bmesh/operators:   $(ls -1 "$DST"/bmesh/operators | wc -l)"
echo "  bmesh/tools:       $(ls -1 "$DST"/bmesh/tools | wc -l)"
echo "  blenlib:           $(ls -1 "$DST"/blenlib | grep -E '\.h$|\.hh$' | wc -l) headers, $(ls -1 "$DST"/blenlib/intern | wc -l) intern"
echo "  blenkernel:        $(ls -1 "$DST"/blenkernel | grep -E '\.h$|\.hh$' | wc -l) headers, $(ls -1 "$DST"/blenkernel/intern | wc -l) intern"
echo "  dna:               $(ls -1 "$DST"/dna | wc -l) headers"
echo "  guardedalloc:      $(ls -1 "$DST"/guardedalloc/intern | wc -l) intern"
echo "  eigen_capi:        $(ls -1 "$DST"/eigen_capi/intern | wc -l) intern"
echo
echo "Audit local deltas vs pristine upstream with:"
echo "  git diff --stat -- vendor/"
