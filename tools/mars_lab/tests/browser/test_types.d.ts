/**
 * @file test_types.d.ts
 * @brief Shared assertions, deferred operations and calendar fixture contracts.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Shared assertions and deferred operations used by the browser regressions. */
type LabTestCheck = (condition: unknown, message: string) => void;

/** @brief Open native record used only by boundary and malformed-payload fixtures. */
interface LabTestRecord { [name: string]: unknown; }

/** @brief Native text fields whose names are selected by the fixture. */
interface LabTestTextFields { [name: string]: string; }

/** @brief One manually completed HTTP reply; no generic fixture factory is needed. */
interface LabTestResponseGate {
    promise: Promise<Response>;
    /** @brief Complete the pending reply with a browser response. */
    resolve(response: Response): void;

    /** @brief Reject the pending reply with the chosen failure. */
    reject(reason?: unknown): void;
}

/** @brief A manually completed asynchronous checkpoint with no result payload. */
interface LabTestSignal {
    promise: Promise<void>;
    /** @brief Mark the expected operation as started or completed. */
    resolve(): void;

    /** @brief Fail the pending checkpoint. */
    reject(reason?: unknown): void;
}

type LabTestCalendarSchema = {key: string; element: string; [field: string]: unknown}[];
interface LabTestAlmanacVariant { html: string; copy_text: string; [field: string]: unknown; }

/** @brief Screen-space bounds returned by the native picker policy. */
interface LabTestRectangle {
    left: number;
    right: number;
    top: number;
    bottom: number;
    width: number;
    height: number;
}
