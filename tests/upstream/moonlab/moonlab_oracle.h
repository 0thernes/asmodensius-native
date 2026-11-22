/* moonlab_oracle.h - builds the trimmed upstream Moonlab dense state-vector
 * kernel into one translation unit, as a TEST ORACLE only.
 *
 * Upstream: tsotchke/moonlab, version 1.2.1, HEAD cd3b234 (2026-09-11).
 * MIT License, Copyright (c) 2024-2026 tsotchke. The full licence text is in
 * the file LICENSE next to this header and applies to everything under src/.
 *
 * The files under src/ are byte-identical copies of the upstream files with
 * the same relative paths (checked with cmp against the read-only mirror):
 *   src/quantum/state.c  gates.c  measurement.c  gpu_route_cpu_unavailable.c
 *   src/optimization/simd_ops.c
 *   src/utils/matrix_math.c  config.c  moonlab_status.c
 *   and the 14 headers they include.
 * Nothing under src/ is edited. This wrapper is not upstream code; it only
 * includes the eight sources in the order the upstream build links them.
 *
 * This code is hosted double-precision C with heap and libm. It must never
 * be linked into core/. It is a classical simulator used to check another
 * classical simulator.
 *
 * Include this header FIRST in the test file, before any project header.
 */
#ifndef MOONLAB_ORACLE_H
#define MOONLAB_ORACLE_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include "src/quantum/state.c"
/* state.c pulls in accelerate_ops.h, which defines HAS_ACCELERATE as 0;
 * measurement.c tests it with #ifdef and is compiled without that header
 * upstream, so the definition must not leak into it. */
#undef HAS_ACCELERATE
#include "src/quantum/gates.c"
#include "src/quantum/measurement.c"
#include "src/quantum/gpu_route_cpu_unavailable.c"
#include "src/optimization/simd_ops.c"
#include "src/utils/matrix_math.c"
#include "src/utils/config.c"
#include "src/utils/moonlab_status.c"

#pragma GCC diagnostic pop

/* read and write one amplitude without leaving complex types in the test's way */
static inline double moonlab_oracle_re(const quantum_state_t *st, int i) { return creal(st->amplitudes[i]); }
static inline double moonlab_oracle_im(const quantum_state_t *st, int i) { return cimag(st->amplitudes[i]); }
static inline void moonlab_oracle_set(quantum_state_t *st, int i, double re, double im)
{
    st->amplitudes[i] = re + im * _Complex_I;
}

/* <complex.h> defines the macro I; the test uses plain doubles */
#undef I

#endif /* MOONLAB_ORACLE_H */
