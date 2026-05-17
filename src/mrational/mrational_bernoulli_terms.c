#include <stdlib.h>

#include "mrational_internal.h"

#define MR_BERNOULLI_EVEN_TERM_COUNT 260u
#define MR_BERNOULLI_WORK_COUNT (2u * MR_BERNOULLI_EVEN_TERM_COUNT + 1u)

static mrational_t *mrational_bernoulli_terms[MR_BERNOULLI_EVEN_TERM_COUNT];
static mpq_t *mrational_bernoulli_work;
static size_t mrational_bernoulli_next_degree;
static int mrational_bernoulli_work_ready;

static mrational_t *mrational_from_mpq(const mpq_t value)
{
    mrational_t *rational;

    rational = mr_new();
    if (!rational)
        return NULL;

    mpq_set(rational->value, value);
    return rational;
}

static void mrational_bernoulli_clear(void)
{
    size_t i;

    for (i = 0u; i < MR_BERNOULLI_EVEN_TERM_COUNT; ++i) {
        mr_free(mrational_bernoulli_terms[i]);
        mrational_bernoulli_terms[i] = NULL;
    }

    if (mrational_bernoulli_work) {
        for (i = 0u; i < MR_BERNOULLI_WORK_COUNT; ++i)
            mpq_clear(mrational_bernoulli_work[i]);
        free(mrational_bernoulli_work);
        mrational_bernoulli_work = NULL;
    }

    mrational_bernoulli_next_degree = 0u;
    mrational_bernoulli_work_ready = 0;
}

static int mrational_bernoulli_prepare_work(void)
{
    size_t i;

    if (mrational_bernoulli_work_ready)
        return 0;

    mrational_bernoulli_work = calloc(MR_BERNOULLI_WORK_COUNT,
                                      sizeof(*mrational_bernoulli_work));
    if (!mrational_bernoulli_work)
        return -1;

    for (i = 0u; i < MR_BERNOULLI_WORK_COUNT; ++i)
        mpq_init(mrational_bernoulli_work[i]);

    mrational_bernoulli_next_degree = 0u;
    mrational_bernoulli_work_ready = 1;
    return 0;
}

static int mrational_bernoulli_ensure(size_t index)
{
    mpq_t scale;
    size_t target_degree;
    size_t i;

    if (index == 0u || index > MR_BERNOULLI_EVEN_TERM_COUNT)
        return -1;

    if (mrational_bernoulli_terms[index - 1u])
        return 0;

    if (mrational_bernoulli_prepare_work() != 0)
        return -1;

    target_degree = index * 2u;
    mpq_init(scale);

    for (i = mrational_bernoulli_next_degree; i <= target_degree; ++i) {
        size_t j;

        mpq_set_ui(mrational_bernoulli_work[i],
                   1u,
                   (unsigned long)i + 1u);
        for (j = i; j >= 1u; --j) {
            mpq_sub(mrational_bernoulli_work[j - 1u],
                    mrational_bernoulli_work[j - 1u],
                    mrational_bernoulli_work[j]);
            mpq_set_ui(scale, (unsigned long)j, 1u);
            mpq_mul(mrational_bernoulli_work[j - 1u],
                    mrational_bernoulli_work[j - 1u],
                    scale);
        }

        if (i != 0u && (i % 2u) == 0u) {
            size_t term_index = (i / 2u) - 1u;

            mrational_bernoulli_terms[term_index] =
                mrational_from_mpq(mrational_bernoulli_work[0]);
            if (!mrational_bernoulli_terms[term_index]) {
                mpq_clear(scale);
                mrational_bernoulli_clear();
                return -1;
            }
        }

        mrational_bernoulli_next_degree = i + 1u;
    }

    mpq_clear(scale);
    return 0;
}

static void __attribute__((destructor)) mrational_bernoulli_shutdown(void)
{
    mrational_bernoulli_clear();
}

size_t mr_bernoulli_even_term_count(void)
{
    return MR_BERNOULLI_EVEN_TERM_COUNT;
}

const mrational_t *mr_bernoulli_even_term(size_t index)
{
    if (index == 0u || index > MR_BERNOULLI_EVEN_TERM_COUNT)
        return NULL;

    if (mrational_bernoulli_ensure(index) != 0)
        return NULL;

    return mrational_bernoulli_terms[index - 1u];
}
