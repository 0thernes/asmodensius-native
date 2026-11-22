/* diffgeo_qgt_fragment.inc - UPSTREAM CODE, used only as a test oracle.
 *
 * Source:  quantum_geometric_tensor (QGTL), HEAD 770994a (2026-01-09)
 * Files:   include/quantum_geometric/distributed/differential_geometry.h:25-28
 *            (the ComplexDouble type)
 *          src/quantum_geometric/distributed/differential_geometry.c:3504-3521
 *            (complex_conj, complex_mult, complex_add, complex_sub)
 *          src/quantum_geometric/distributed/differential_geometry.c:2819-2906
 *            (diffgeo_compute_fubini_study, diffgeo_compute_berry_curvature)
 * Changes: carriage returns removed; the three line ranges are placed in the
 *          order type, helpers, kernels so that the fragment compiles alone.
 *          No other byte was changed (the comments keep their UTF-8 symbols).
 * Needs:   a type diffgeo_engine_t with counters stats.metric_evaluations
 *          and stats.curvature_evaluations, supplied by the including test.
 * Used by: tests/test_tk_qgt.c. Never compiled into the core.
 *
 * The upstream licence follows, unchanged.
 *
 * MIT License
 *
 * Copyright (c) 2024 Tsotchke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* ---- differential_geometry.h:25-28 ---- */
typedef struct {
    double real;
    double imag;
} ComplexDouble;

/* ---- differential_geometry.c:3504-3521 ---- */
static ComplexDouble complex_conj(ComplexDouble z) {
    return (ComplexDouble){z.real, -z.imag};
}

static ComplexDouble complex_mult(ComplexDouble a, ComplexDouble b) {
    return (ComplexDouble){
        a.real * b.real - a.imag * b.imag,
        a.real * b.imag + a.imag * b.real
    };
}

static ComplexDouble complex_add(ComplexDouble a, ComplexDouble b) {
    return (ComplexDouble){a.real + b.real, a.imag + b.imag};
}

static ComplexDouble complex_sub(ComplexDouble a, ComplexDouble b) {
    return (ComplexDouble){a.real - b.real, a.imag - b.imag};
}

/* ---- differential_geometry.c:2819-2906 ---- */
bool diffgeo_compute_fubini_study(
    diffgeo_engine_t* engine,
    const ComplexDouble* state,
    size_t dim,
    const ComplexDouble* param_derivatives,
    size_t num_params,
    double* metric_out) {

    if (!engine || !state || !param_derivatives || !metric_out ||
        dim == 0 || num_params == 0) return false;

    // g_ij = Re(<∂_i ψ|∂_j ψ> - <∂_i ψ|ψ><ψ|∂_j ψ>)

    for (size_t i = 0; i < num_params; i++) {
        for (size_t j = 0; j < num_params; j++) {
            ComplexDouble inner_deriv = {0.0, 0.0};
            ComplexDouble inner_i = {0.0, 0.0};
            ComplexDouble inner_j = {0.0, 0.0};

            for (size_t k = 0; k < dim; k++) {
                // <∂_i ψ|∂_j ψ>
                ComplexDouble di_conj = complex_conj(param_derivatives[i * dim + k]);
                ComplexDouble dj = param_derivatives[j * dim + k];
                inner_deriv = complex_add(inner_deriv, complex_mult(di_conj, dj));

                // <∂_i ψ|ψ>
                ComplexDouble psi = state[k];
                inner_i = complex_add(inner_i, complex_mult(di_conj, psi));

                // <ψ|∂_j ψ>
                ComplexDouble psi_conj = complex_conj(state[k]);
                inner_j = complex_add(inner_j, complex_mult(psi_conj, dj));
            }

            ComplexDouble proj = complex_mult(inner_i, inner_j);
            ComplexDouble g = complex_sub(inner_deriv, proj);

            metric_out[i * num_params + j] = g.real;
        }
    }

    engine->stats.metric_evaluations++;
    return true;
}

bool diffgeo_compute_berry_curvature(
    diffgeo_engine_t* engine,
    const ComplexDouble* state,
    size_t dim,
    const ComplexDouble* param_derivatives,
    size_t num_params,
    double* curvature_out) {

    if (!engine || !state || !param_derivatives || !curvature_out ||
        dim == 0 || num_params == 0) return false;

    // F_ij = -2 * Im(<∂_i ψ|∂_j ψ> - <∂_i ψ|ψ><ψ|∂_j ψ>)
    // Berry curvature is the antisymmetric part

    for (size_t i = 0; i < num_params; i++) {
        for (size_t j = 0; j < num_params; j++) {
            ComplexDouble inner_deriv = {0.0, 0.0};
            ComplexDouble inner_i = {0.0, 0.0};
            ComplexDouble inner_j = {0.0, 0.0};

            for (size_t k = 0; k < dim; k++) {
                ComplexDouble di_conj = complex_conj(param_derivatives[i * dim + k]);
                ComplexDouble dj = param_derivatives[j * dim + k];
                inner_deriv = complex_add(inner_deriv, complex_mult(di_conj, dj));

                ComplexDouble psi = state[k];
                inner_i = complex_add(inner_i, complex_mult(di_conj, psi));

                ComplexDouble psi_conj = complex_conj(state[k]);
                inner_j = complex_add(inner_j, complex_mult(psi_conj, dj));
            }

            ComplexDouble proj = complex_mult(inner_i, inner_j);
            ComplexDouble qgt_ij = complex_sub(inner_deriv, proj);

            // F_ij = -2 * Im(Q_ij) to get standard Berry curvature normalization
            curvature_out[i * num_params + j] = -2.0 * qgt_ij.imag;
        }
    }

    engine->stats.curvature_evaluations++;
    return true;
}
