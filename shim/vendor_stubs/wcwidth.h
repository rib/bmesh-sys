/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Stub for wcwidth.h. customdata.cc reaches BLI_strncpy_utf8 via the layer
 * name-uniquing path; string_utf8.cc includes wcwidth.h for mk_wcwidth.
 * The function is only used by BLI_wcwidth_*, which customdata.cc never
 * exercises. Provide a trivial stub returning 1 (one column per codepoint).
 */
#pragma once

#include <cstddef>

static inline int mk_wcwidth(int /*ucs*/)
{
    return 1;
}

static inline int mk_wcswidth(const char32_t * /*pwcs*/, size_t /*n*/)
{
    return 0;
}
