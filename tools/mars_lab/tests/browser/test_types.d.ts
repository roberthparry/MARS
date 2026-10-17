/**
 * @file test_types.d.ts
 * @brief Shared assertions, deferred operations and calendar fixture contracts.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Shared assertions and deferred operations used by the browser regressions. */
type LabTestCheck = (condition: unknown, message: string) => void;
interface LabTestDeferred<T> {
    promise: Promise<T>;
    resolve(value: T | PromiseLike<T>): void;
    reject(reason?: unknown): void;
}

interface LabConfiguration { DEFAULT_EQUATION_VARIABLE: string; }
type LabTestCalendarSchema = {key: string; element: string; [field: string]: unknown}[];
interface LabTestAlmanacVariant { html: string; copy_text: string; [field: string]: unknown; }
