/**
 * @file test_support.ts
 * @brief Small browser timing and response fixtures shared by the Lab tests.
 *
 * Keeps repeated Promise setup out of individual scenarios. Helpers use the current
 * browser timers so tests can still substitute and restore those resources locally.
 * This script is injected only into the disposable regression page.
 */
namespace LabTestBrowser {
    /** @brief Wait for the requested browser timer interval. */
    export function wait(milliseconds: number): Promise<void> {
        return new Promise(function schedule(resolve) {
            window.setTimeout(resolve, milliseconds);
        });
    }

    /** @brief Wait for one animation frame without exposing its timestamp to the test. */
    export function nextFrame(): Promise<void> {
        return new Promise(function schedule(resolve) {
            function frame(): void { resolve(); }
            window.requestAnimationFrame(frame);
        });
    }

    /** @brief Wait for two animation frames so queued layout work can reach the next paint. */
    export async function paint(): Promise<void> {
        await nextFrame();
        await nextFrame();
    }

    /** @brief Create a checkpoint which a fixture completes when the expected operation starts. */
    export function signal(): LabTestSignal {
        let resolve!: () => void;
        let reject!: (reason?: unknown) => void;
        const promise = new Promise<void>(function capture(accept, fail) {
            resolve = accept;
            reject = fail;
        });
        return {promise, resolve, reject};
    }

    /** @brief Create a manually completed HTTP response for stale and failed request scenarios. */
    export function responseGate(): LabTestResponseGate {
        let resolve!: (response: Response) => void;
        let reject!: (reason?: unknown) => void;
        const promise = new Promise<Response>(function capture(accept, fail) {
            resolve = accept;
            reject = fail;
        });
        return {promise, resolve, reject};
    }
}
