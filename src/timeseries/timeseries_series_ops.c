#include "timeseries_internal.h"

size_t ts_length(const timeseries_t *series) { return series ? series->length : 0u; }
ts_index_info_t ts_index_info(const timeseries_t *series)
{
    ts_index_info_t info = { TS_FREQ_UNKNOWN, TS_YEAR_CALENDAR, false, false, 0u };

    if (!series)
        return info;
    info.frequency = series->frequency;
    info.year_type = series->year_type;
    info.is_regular = series->is_regular;
    info.has_datetimes = series->has_index;
    info.season_period = series->season_period;
    return info;
}
ts_frequency_t ts_frequency(const timeseries_t *series) { return series ? series->frequency : TS_FREQ_UNKNOWN; }
ts_year_type_t ts_year_type(const timeseries_t *series) { return series ? series->year_type : TS_YEAR_CALENDAR; }
bool ts_is_regular(const timeseries_t *series) { return series && series->is_regular; }
bool ts_has_missing(const timeseries_t *series)
{
    size_t i;
    if (!series)
        return false;
    for (i = 0u; i < series->length; ++i)
        if (series->missing[i])
            return true;
    return false;
}

int ts_get_value(const timeseries_t *series, size_t index, number_t *out)
{
    if (!series || !out || index >= series->length)
        return -1;
    *out = num_clone(series->values[index]);
    return 0;
}

int ts_get_datetime(const timeseries_t *series, size_t index, datetime_t *out)
{
    if (!series || !out || !series->has_index || index >= series->length)
        return -1;
    if (!series->index[index])
        return -1;
    return datetime_init_copy(out, series->index[index]) ? 0 : -1;
}

datetime_t *ts_start_datetime(const timeseries_t *series)
{
    size_t i;

    if (!series || !series->has_index || series->length == 0u)
        return NULL;
    for (i = 0u; i < series->length; ++i) {
        if (series->index[i])
            return ts_datetime_clone(series->index[i]);
    }
    return NULL;
}

datetime_t *ts_end_datetime(const timeseries_t *series)
{
    size_t i;

    if (!series || !series->has_index || series->length == 0u)
        return NULL;
    for (i = series->length; i > 0u; --i) {
        if (series->index[i - 1u])
            return ts_datetime_clone(series->index[i - 1u]);
    }
    return NULL;
}

timeseries_t *ts_slice(const timeseries_t *series, size_t start, size_t length)
{
    timeseries_t *out;
    size_t i;

    if (!series || start > series->length || start + length > series->length)
        return NULL;
    out = ts_alloc_empty(length);
    if (!out)
        return NULL;
    out->frequency = series->frequency;
    out->year_type = series->year_type;
    out->is_regular = series->is_regular;
    out->season_period = series->season_period;
    if (series->has_index) {
        out->index = calloc(length ? length : 1u, sizeof(*out->index));
        if (!out->index) {
            ts_free(out);
            return NULL;
        }
        out->has_index = true;
    }
    for (i = 0u; i < length; ++i) {
        out->values[i] = num_clone(series->values[start + i]);
        out->missing[i] = series->missing[start + i];
        if (series->has_index)
            out->index[i] = ts_datetime_clone(series->index[start + i]);
    }
    return out;
}

timeseries_t *ts_slice_date(const timeseries_t *series,
                            const datetime_t *start,
                            const datetime_t *end)
{
    size_t i, first = 0u, last = 0u;
    bool found = false;

    if (!series || !series->has_index || !start || !end)
        return NULL;
    for (i = 0u; i < series->length; ++i) {
        if (!series->index[i])
            continue;
        if (datetime_compare(series->index[i], start) >= 0 &&
            datetime_compare(series->index[i], end) <= 0) {
            if (!found)
                first = i;
            last = i;
            found = true;
        }
    }
    return found ? ts_slice(series, first, last - first + 1u) : NULL;
}

timeseries_t *ts_head(const timeseries_t *series, size_t n)
{
    return ts_slice(series, 0u, n < ts_length(series) ? n : ts_length(series));
}

timeseries_t *ts_tail(const timeseries_t *series, size_t n)
{
    size_t len = ts_length(series);
    size_t use = n < len ? n : len;

    return ts_slice(series, len - use, use);
}

timeseries_t *ts_fill_missing(const timeseries_t *series, ts_missing_policy_t policy)
{
    timeseries_t *out = ts_clone(series);
    size_t i;

    if (!out)
        return NULL;
    if (policy == TS_MISSING_KEEP || policy == TS_MISSING_ERROR)
        return out;
    for (i = 0u; i < out->length; ++i) {
        if (!out->missing[i])
            continue;
        if (policy == TS_MISSING_FORWARD_FILL && i > 0u && !out->missing[i - 1u]) {
            num_destroy(&out->values[i]);
            out->values[i] = num_clone(out->values[i - 1u]);
            out->missing[i] = false;
        } else if (policy == TS_MISSING_BACKWARD_FILL && i + 1u < out->length &&
                   !out->missing[i + 1u]) {
            num_destroy(&out->values[i]);
            out->values[i] = num_clone(out->values[i + 1u]);
            out->missing[i] = false;
        } else if (policy == TS_MISSING_INTERPOLATE_LINEAR &&
                   i > 0u && i + 1u < out->length &&
                   !out->missing[i - 1u] && !out->missing[i + 1u]) {
            number_t sum = num_add(out->values[i - 1u], out->values[i + 1u]);
            number_t avg = num_div(sum, NUM_TWO);

            num_destroy(&out->values[i]);
            out->values[i] = avg;
            out->missing[i] = false;
            num_destroy(&sum);
        }
    }
    return out;
}

timeseries_t *ts_drop_missing(const timeseries_t *series)
{
    size_t i, keep = 0u, idx = 0u;
    timeseries_t *out;

    if (!series)
        return NULL;
    for (i = 0u; i < series->length; ++i)
        if (!series->missing[i])
            keep++;
    out = ts_alloc_empty(keep);
    if (!out)
        return NULL;
    out->frequency = series->frequency;
    out->year_type = series->year_type;
    out->is_regular = series->is_regular;
    out->season_period = series->season_period;
    if (series->has_index) {
        out->index = calloc(keep ? keep : 1u, sizeof(*out->index));
        if (!out->index) {
            ts_free(out);
            return NULL;
        }
        out->has_index = true;
    }
    for (i = 0u; i < series->length; ++i) {
        if (series->missing[i])
            continue;
        out->values[idx] = num_clone(series->values[i]);
        if (series->has_index)
            out->index[idx] = ts_datetime_clone(series->index[i]);
        idx++;
    }
    return out;
}

int ts_align_pair(const timeseries_t *left, const timeseries_t *right,
                  ts_join_type_t join_type,
                  timeseries_t **left_out, timeseries_t **right_out)
{
    size_t i = 0u, j = 0u, cap = 16u, len = 0u;
    number_t *lvals = calloc(cap, sizeof(*lvals));
    number_t *rvals = calloc(cap, sizeof(*rvals));
    datetime_t **index = calloc(cap, sizeof(*index));
    bool *lmiss = calloc(cap, sizeof(*lmiss));
    bool *rmiss = calloc(cap, sizeof(*rmiss));
    timeseries_t *L = NULL, *R = NULL;

    if (!left || !right || !left_out || !right_out ||
        !left->has_index || !right->has_index ||
        !lvals || !rvals || !index || !lmiss || !rmiss)
        goto fail;
    while (i < left->length || j < right->length) {
        int cmp;
        bool take_left_only = false, take_right_only = false, take_both = false;
        const datetime_t *left_dt = i < left->length ? left->index[i] : NULL;
        const datetime_t *right_dt = j < right->length ? right->index[j] : NULL;

        if (i < left->length && !left_dt) {
            ++i;
            continue;
        }
        if (j < right->length && !right_dt) {
            ++j;
            continue;
        }

        if (i < left->length && j < right->length)
            cmp = datetime_compare(left_dt, right_dt);
        else if (i < left->length)
            cmp = -1;
        else
            cmp = 1;

        if (cmp == 0)
            take_both = true;
        else if (cmp < 0)
            take_left_only = (join_type == TS_JOIN_LEFT || join_type == TS_JOIN_OUTER);
        else
            take_right_only = (join_type == TS_JOIN_RIGHT || join_type == TS_JOIN_OUTER);

        if (take_both || take_left_only || take_right_only) {
            if (len == cap) {
                size_t new_cap = cap * 2u;
                number_t *nl = realloc(lvals, new_cap * sizeof(*nl));
                number_t *nr = realloc(rvals, new_cap * sizeof(*nr));
                datetime_t **ni = realloc(index, new_cap * sizeof(*ni));
                bool *nlm = realloc(lmiss, new_cap * sizeof(*nlm));
                bool *nrm = realloc(rmiss, new_cap * sizeof(*nrm));

                if (!nl || !nr || !ni || !nlm || !nrm) {
                    free(nl); free(nr); free(ni); free(nlm); free(nrm);
                    goto fail;
                }
                lvals = nl; rvals = nr; index = ni; lmiss = nlm; rmiss = nrm;
                cap = new_cap;
            }
            if (take_both) {
                lvals[len] = num_clone(left->values[i]);
                rvals[len] = num_clone(right->values[j]);
                lmiss[len] = left->missing[i];
                rmiss[len] = right->missing[j];
                index[len] = ts_datetime_clone(left_dt);
                ++i; ++j;
            } else if (take_left_only) {
                lvals[len] = num_clone(left->values[i]);
                rvals[len] = num_clone(NUM_NAN);
                lmiss[len] = left->missing[i];
                rmiss[len] = true;
                index[len] = ts_datetime_clone(left_dt);
                ++i;
            } else {
                lvals[len] = num_clone(NUM_NAN);
                rvals[len] = num_clone(right->values[j]);
                lmiss[len] = true;
                rmiss[len] = right->missing[j];
                index[len] = ts_datetime_clone(right_dt);
                ++j;
            }
            if (!index[len])
                goto fail;
            ++len;
        } else {
            if (cmp < 0) ++i;
            else if (cmp > 0) ++j;
        }
    }

    L = ts_alloc_empty(len);
    R = ts_alloc_empty(len);
    if (!L || !R)
        goto fail;
    free(L->values); L->values = lvals; lvals = NULL;
    free(R->values); R->values = rvals; rvals = NULL;
    free(L->missing); L->missing = lmiss; lmiss = NULL;
    free(R->missing); R->missing = rmiss; rmiss = NULL;
    L->index = index; R->index = calloc(len ? len : 1u, sizeof(*R->index));
    if (!R->index)
        goto fail;
    for (i = 0u; i < len; ++i)
        R->index[i] = ts_datetime_clone(index[i]);
    L->has_index = R->has_index = true;
    L->frequency = left->frequency; R->frequency = right->frequency;
    L->year_type = left->year_type; R->year_type = right->year_type;
    *left_out = L;
    *right_out = R;
    free(lvals); free(rvals); free(lmiss); free(rmiss);
    return 0;

fail:
    if (lvals) {
        for (i = 0u; i < len; ++i) num_destroy(&lvals[i]);
        free(lvals);
    }
    if (rvals) {
        for (i = 0u; i < len; ++i) num_destroy(&rvals[i]);
        free(rvals);
    }
    if (index) {
        for (i = 0u; i < len; ++i) datetime_dealloc(index[i]);
        free(index);
    }
    free(lmiss);
    free(rmiss);
    ts_free(L);
    ts_free(R);
    return -1;
}
