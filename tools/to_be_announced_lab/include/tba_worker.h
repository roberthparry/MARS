/**
 * @file tba_worker.h
 * @brief Private entry point for server-owned forecasting workers.
 *
 * Only the native server dispatches this mode. Fitting is isolated in a child
 * process and supervised by the existing deadline and cancellation machinery.
 * Results use private Protobuf files, not a scratch executable or JSON stream.
 */
#ifndef TBA_WORKER_H
#define TBA_WORKER_H
#include "tba_support.h"

/**
 * @brief Fit a model and publish its result.
 * @param argc Number of worker arguments, including the synthetic programme name.
 * @param argv Borrowed validated argument vector.
 * @param output_path Destination for the private Protobuf result.
 * @return Zero on successful publication, including a structured fitting error; non-zero on publication failure.
 */
int tba_worker_main(int argc, char **argv, const char *output_path);
#endif
