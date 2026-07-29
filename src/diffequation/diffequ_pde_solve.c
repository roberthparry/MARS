#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

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
