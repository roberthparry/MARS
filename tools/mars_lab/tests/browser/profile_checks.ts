/**
 * @file profile_checks.ts
 * @brief Opt-in browser profiling for requests and native calls.
 *
 * Collects timing and call-count observations only when profiling is requested; observations are not correctness thresholds.
 */

/** @brief Request or native-call counts indexed by their existing report names. */
interface LabProfileCounts {
    [name: string]: number;
}

/** @brief One row in the ordinary table workload. */
interface LabProfileTableRow {
    name: string;
    latitude: number;
    longitude: number;
    timezone: string;
    description: string;
}

/** @brief One row in the larger table workload. */
interface LabProfileLargeRow {
    name: string;
    value: number;
    description: string;
}

/** @brief Configure opt-in profiling; timings remain observations rather than correctness thresholds. */
function createLabProfile() {
    const enabled = new URLSearchParams(location.search).has('profile');
    const requests: LabProfileCounts = Object.create(null);
    const calls: LabProfileCounts = Object.create(null);
    let countCalls = false;
    const started = performance.now();
    if (enabled) {
        const originalFetch = window.fetch;
        window.fetch = function profiledFetch(url, options) {
            const path = new URL(String(url), location.href).pathname;
            requests[path] = (requests[path] || 0) + 1;
            return originalFetch(url, options);
        };
        const instantiate = WebAssembly.instantiate;
        /** @brief Instantiate bytes with counted native imports. */
        function profiledInstantiate(bytes: BufferSource, imports?: WebAssembly.Imports, options?: WebAssembly.WebAssemblyCompileOptions): Promise<WebAssembly.WebAssemblyInstantiatedSource>;

        /** @brief Instantiate a compiled module with counted native imports. */
        function profiledInstantiate(bytes: WebAssembly.Module, imports?: WebAssembly.Imports): Promise<WebAssembly.Instance>;

        /** @brief Wrap function imports while preserving other imports and both overloads. */
        function profiledInstantiate(bytes: BufferSource | WebAssembly.Module, imports?: WebAssembly.Imports, options?: WebAssembly.WebAssemblyCompileOptions): Promise<WebAssembly.Instance | WebAssembly.WebAssemblyInstantiatedSource> {
            const env: WebAssembly.ModuleImports = {};
            for (const [name, fn] of Object.entries(imports?.env || {})) {
                if (typeof fn === 'function') {
                    env[name] = function profiledImport(...args: (number | bigint)[]) {
                        if (countCalls) {
                            calls[name] = (calls[name] || 0) + 1;
                        }
                        return Reflect.apply(fn, undefined, args);
                    };
                } else {
                    env[name] = fn;
                }
            }
            if (bytes instanceof WebAssembly.Module) {
                return instantiate(bytes, {...imports, env});
            }
            return instantiate(bytes, {...imports, env}, options);
        }
        WebAssembly.instantiate = profiledInstantiate;
    }
    /** @brief Sort timing samples numerically. */
    function compareTimes(first: number, second: number): number {
        return first - second;
    }

    /** @brief Return the rounded middle value of the seven timing samples. */
    function median(values: number[]): number {
        values.sort(compareTimes);
        return Number(values[3].toFixed(3));
    }

    /** @brief Measure the existing warm-up, round-trip and background-decode workloads. */
    async function measure(name: string, value: unknown) {
        const bytes = labWire.encode(value);
        for (let i = 0; i < 5; ++i) {
            labWire.encode(value);
            labWire.decode(bytes);
        }
        const times: number[] = [];
        for (let sample = 0; sample < 7; ++sample) {
            await LabTestBrowser.wait(0);
            const begin = performance.now();
            for (let i = 0; i < 20; ++i) {
                labWire.encode(value);
                labWire.decode(bytes);
            }
            times.push((performance.now() - begin) / 20);
        }
        times.sort(compareTimes);
        for (const key of Object.keys(calls)) {
            delete calls[key];
        }
        countCalls = true;
        try {
            labWire.encode(value);
            labWire.decode(bytes);
        } finally {
            countCalls = false;
        }
        const hostCalls = {...calls};
        const decodeTimes: number[] = [];
        const backgroundTimes: number[] = [];
        const submissionTimes: number[] = [];
        for (let i = 0; i < 7; ++i) {
            let begin = performance.now();
            labWire.decode(bytes);
            decodeTimes.push(performance.now() - begin);
            begin = performance.now();
            const pending = labWire.decodeAsync(bytes.buffer);
            submissionTimes.push(performance.now() - begin);
            await pending;
            backgroundTimes.push(performance.now() - begin);
        }
        return {
            name,
            bytes: bytes.length,
            median_roundtrip_ms: +times[3].toFixed(3),
            host_calls: hostCalls,
            ui_decode_ms: median(decodeTimes),
            async_submit_ms: median(submissionTimes),
            async_complete_ms: median(backgroundTimes)
        };
    }
    return {
        enabled,
        /** @brief Profile the fixed codec workloads and one ordinary evaluation. */
        async run() {
            const startup = {milliseconds: +(performance.now() - started).toFixed(1), requests: {...requests}};
            const response = await labHttp.fetch('/eval', {
                method: 'POST',
                body: labWire.encode(
                    {expression: '{ sin(x)^2 + cos(x)^2 | x = 1/3 }', precision: 17, persist_expression: false})
            });
            const evaluated = await response.labData() as {ok?: boolean};
            if (!response.ok || !evaluated.ok)
                throw new Error('Profile evaluation failed');
            const rows: LabProfileTableRow[] = [];
            for (let index = 0; index < 256; ++index) {
                rows.push({
                    name: `Town ${index}`,
                    latitude: index / 10,
                    longitude: -index / 20,
                    timezone: 'Europe/London',
                    description: 'μσ — exact text'
                });
            }
            const largeRows: LabProfileLargeRow[] = [];
            for (let index = 0; index < 4096; ++index) {
                largeRows.push({
                    name: `Item ${index}`,
                    value: index / 10,
                    description: 'μσ — exact text'.repeat(4)
                });
            }
            const workloads: [string, unknown][] = [
                ['scalar_request', {expression: 'sin(x)', precision: 78, persist_expression: false}],
                ['native_result', evaluated],
                ['table', {rows}],
                ['long_text', {expression: '1234567890'.repeat(6553)}],
                ['large_table', {rows: largeRows}],
                ['large_text', {text: '1234567890'.repeat(209715)}]
            ];
            const results = [];
            for (const [name, value] of workloads) {
                results.push(await measure(name, value));
            }
            const before = {...requests};
            await WorksheetRequests.evaluation.evaluateExpression();
            const evaluationRequests: LabProfileCounts = {};
            for (const [key, value] of Object.entries(requests)) {
                const count = value - (before[key] || 0);
                if (count) {
                    evaluationRequests[key] = count;
                }
            }
            return {startup, results, evaluation_requests: evaluationRequests};
        }
    };
}

const labProfile = createLabProfile();
