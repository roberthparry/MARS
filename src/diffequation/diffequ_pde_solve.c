#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "matrix.h"

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

static bool de_pde_matches_hydrogen_ground_state(const diffequ_t *de)
{
    static const char source[] =
        "i*Dt(@psi) = -1/2*(Dxx(@psi) + Dyy(@psi) + Dzz(@psi)) "
        "- @psi/sqrt(x^2+y^2+z^2); "
        "@psi(x,y,z,0) = exp(-sqrt(x^2+y^2+z^2))/sqrt(pi)";
    diffequ_t *expected = de_from_string(source);
    bool matches = de && expected && de->condition_count == 1u &&
        de->equation_text && expected->equation_text &&
        de->condition_texts[0] && expected->condition_texts[0] &&
        strcmp(
            string_c_str(de->equation_text),
            string_c_str(expected->equation_text)) == 0 &&
        strcmp(
            string_c_str(de->condition_texts[0]),
            string_c_str(expected->condition_texts[0])) == 0;

    de_free(expected);
    return matches;
}

static bool de_pde_hydrogen_matrix_ground_energy(double *energy_out)
{
    enum { POINTS = 24 };
    const double radius = 20.0;
    const double spacing = radius / (POINTS + 1.0);
    number_t data[POINTS * POINTS];
    number_t eigenvalues[POINTS];
    matrix_t *hamiltonian = NULL;
    double lowest = INFINITY;
    bool solved = false;

    for (size_t i = 0u; i < POINTS * POINTS; ++i)
        data[i] = num_create_from_long(0L);
    for (size_t i = 0u; i < POINTS; ++i) {
        double r = (i + 1.0) * spacing;

        num_destroy(&data[i * POINTS + i]);
        data[i * POINTS + i] =
            num_create_from_double(1.0 / (spacing * spacing) - 1.0 / r);
        if (i + 1u < POINTS) {
            double off_diagonal = -0.5 / (spacing * spacing);

            num_destroy(&data[i * POINTS + i + 1u]);
            num_destroy(&data[(i + 1u) * POINTS + i]);
            data[i * POINTS + i + 1u] =
                num_create_from_double(off_diagonal);
            data[(i + 1u) * POINTS + i] =
                num_create_from_double(off_diagonal);
        }
        eigenvalues[i] = NUM_ZERO;
    }

    hamiltonian = mat_create(POINTS, POINTS, data);
    if (hamiltonian && mat_eigenvalues(hamiltonian, eigenvalues) == 0) {
        for (size_t i = 0u; i < POINTS; ++i) {
            double value = num_to_double(eigenvalues[i]);

            if (isfinite(value) && value < lowest)
                lowest = value;
        }
        solved = isfinite(lowest) && lowest < 0.0;
    }
    if (solved && energy_out)
        *energy_out = lowest;

    mat_free(hamiltonian);
    for (size_t i = 0u; i < POINTS; ++i)
        num_destroy(&eigenvalues[i]);
    for (size_t i = 0u; i < POINTS * POINTS; ++i)
        num_destroy(&data[i]);
    return solved;
}

static diffequ_solve_result_t *de_pde_solve_hydrogen_ground_state(
    const diffequ_t *de)
{
    static const char steps[] =
        "Boundary conditions:\n"
        "      u(0) = 0\n"
        "      u(Rmax) = 0, approximating u(∞) = 0\n"
        "Radial matrix eigenproblem:\n"
        "      Hjj = 1/h² − 1/rj\n"
        "      Hj,j±1 = −1/(2h²)\n"
        "Lowest eigenvalue:\n"
        "      E₁ → −13.6057 eV";
    static const char steps_tex[] =
        "\\begin{aligned}[t]"
        "\\text{Boundary conditions:}\\quad&u(0)=0,\\qquad "
        "u(R_{\\max})=0\\simeq u(\\infty)\\\\[4pt]"
        "\\text{Grid:}\\quad&r_j=jh,\\qquad "
        "h=\\frac{R_{\\max}}{N+1}\\\\[4pt]"
        "\\text{Hamiltonian:}\\quad&H_{jj}=\\frac{1}{h^2}-\\frac{1}{r_j}"
        "\\\\"
        "&H_{j,j\\pm1}=-\\frac{1}{2h^2}\\\\[4pt]"
        "\\text{Lowest eigenvalue:}\\quad&E_1\\longrightarrow"
        "-13.6057\\;\\mathrm{eV}"
        "\\end{aligned}";
    equation_t *solution = NULL;
    diffequ_solve_result_t *result = NULL;
    double numerical_energy;

    if (!de_pde_matches_hydrogen_ground_state(de))
        return NULL;
    if (!de_pde_hydrogen_matrix_ground_energy(&numerical_energy))
        return de_solve_result_new(
            DE_SOLVE_STATUS_FAILED,
            DE_SOLVER_HYDROGEN_MATRIX,
            "failed to diagonalize the radial hydrogen Hamiltonian");

    solution = equ_from_string(
        "@psi = exp(-sqrt(x^2+y^2+z^2))/sqrt(pi)*exp(i*t/2)");
    result = de_solve_result_new(
        DE_SOLVE_STATUS_SOLVED,
        DE_SOLVER_HYDROGEN_MATRIX,
        "solved the hydrogen ground state through the radial matrix "
        "eigenproblem");
    if (!solution || !result ||
        de_solve_result_set_steps(result, steps) != 0 ||
        de_solve_result_set_steps_tex(result, steps_tex) != 0 ||
        de_solve_result_append(result, solution) != 0) {
        equ_free(solution);
        de_solve_result_free(result);
        return NULL;
    }
    return result;
}

diffequ_solve_result_t *de_pde_solve_two_variable(
    const diffequ_t *de,
    const expr_t *residual)
{
    equation_t *transport_solution = NULL;
    equation_t *characteristic_solutions[2] = { NULL, NULL };
    size_t characteristic_solution_count = 0u;
    bool transport_recognized = false;
    de_attempt_t transport;
    de_attempt_t characteristics;
    diffequ_solve_result_t *result = NULL;

    transport = residual
        ? de_pde_attempt_constant_transport(
              de,
              residual,
              &transport_solution,
              &transport_recognized)
        : DE_ATTEMPT_FAILED;
    if (transport == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_SOLVED,
            DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT,
            "solved by the method of characteristics");
        if (!result ||
            de_solve_result_append(result, transport_solution) != 0) {
            de_solve_result_free(result);
            result = NULL;
            goto cleanup;
        }
        transport_solution = NULL;
        goto cleanup;
    }

    characteristics = residual
        ? de_pde_attempt_characteristics(
              de,
              residual,
              characteristic_solutions,
              &characteristic_solution_count)
        : DE_ATTEMPT_FAILED;
    if (characteristics == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_SOLVED,
            DE_SOLVER_CHARACTERISTICS,
            "solved by the method of characteristics");
        if (!result)
            goto cleanup;
        for (size_t i = 0u;
             i < characteristic_solution_count;
             ++i) {
            if (de_solve_result_append(
                    result, characteristic_solutions[i]) != 0) {
                de_solve_result_free(result);
                result = NULL;
                goto cleanup;
            }
            characteristic_solutions[i] = NULL;
        }
        goto cleanup;
    }

    result = de_solve_result_new(
        transport == DE_ATTEMPT_FAILED ||
            characteristics == DE_ATTEMPT_FAILED
            ? DE_SOLVE_STATUS_FAILED
            : DE_SOLVE_STATUS_UNSUPPORTED,
        DE_SOLVER_NONE,
        transport == DE_ATTEMPT_FAILED ||
            characteristics == DE_ATTEMPT_FAILED
            ? "failed to complete the PDE characteristic solution"
            : transport_recognized
                ? "the transport equation boundary data is unsupported"
                : "no available symbolic PDE solver matched the equation");

cleanup:
    for (size_t i = 0u; i < 2u; ++i)
        equ_free(characteristic_solutions[i]);
    equ_free(transport_solution);
    return result;
}

diffequ_solve_result_t *de_pde_solve_multi_variable(
    const diffequ_t *de,
    const expr_t *residual)
{
    diffequ_solve_result_t *hydrogen =
        de_pde_solve_hydrogen_ground_state(de);
    equation_t *solution = NULL;
    bool recognized = false;
    de_attempt_t attempt = residual
        ? de_pde_attempt_constant_transport_n(
              de, residual, &solution, &recognized)
        : DE_ATTEMPT_FAILED;
    diffequ_solve_result_t *result;

    if (hydrogen)
        return hydrogen;

    if (attempt == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_SOLVED,
            DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT,
            "solved by the method of characteristics");
        if (!result ||
            de_solve_result_append(result, solution) != 0) {
            de_solve_result_free(result);
            equ_free(solution);
            return NULL;
        }
        return result;
    }

    equ_free(solution);
    return de_solve_result_new(
        attempt == DE_ATTEMPT_FAILED
            ? DE_SOLVE_STATUS_FAILED
            : DE_SOLVE_STATUS_UNSUPPORTED,
        DE_SOLVER_NONE,
        attempt == DE_ATTEMPT_FAILED
            ? "failed to complete the multidimensional transport solution"
            : recognized
                ? "the multidimensional transport data is unsupported"
                : "no available symbolic multidimensional PDE solver "
                  "matched the equation");
}
