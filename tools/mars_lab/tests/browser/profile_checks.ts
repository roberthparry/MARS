/**
 * @file profile_checks.ts
 * @brief Opt-in browser profiling for requests and native calls.
 *
 * Collects timing and call-count observations only when profiling is requested; observations are not correctness thresholds.
 */

/** Opt-in real-browser profiling. Timings are observations, never correctness thresholds. */
const labProfile = (() => {
    const enabled = new URLSearchParams(location.search).has('profile');
    const requests: Record<string, number> = Object.create(null);
    const calls: Record<string, number> = Object.create(null);
    let countCalls = false;
    const started = performance.now();
    if (enabled) {
        const originalFetch = window.fetch;
        window.fetch = (url, options) => {
            const path = new URL(String(url), location.href).pathname;
            requests[path] = (requests[path] || 0) + 1;
            return originalFetch(url, options);
        };
        const instantiate = WebAssembly.instantiate;
        function profiledInstantiate(bytes: BufferSource, imports?: WebAssembly.Imports, options?: WebAssembly.WebAssemblyCompileOptions): Promise<WebAssembly.WebAssemblyInstantiatedSource>;
        function profiledInstantiate(bytes: WebAssembly.Module, imports?: WebAssembly.Imports): Promise<WebAssembly.Instance>;
        function profiledInstantiate(bytes: BufferSource | WebAssembly.Module, imports?: WebAssembly.Imports, options?: WebAssembly.WebAssemblyCompileOptions): Promise<WebAssembly.Instance | WebAssembly.WebAssemblyInstantiatedSource> {
            const env: WebAssembly.ModuleImports = {};
            for (const [name, fn] of Object.entries(imports?.env || {}))
                env[name] = typeof fn === 'function' ? (...args: (number | bigint)[]) => {
                    if (countCalls)
                        calls[name] = (calls[name] || 0) + 1;
                    return Reflect.apply(fn, undefined, args);
                } : fn;
            return bytes instanceof WebAssembly.Module ? instantiate(bytes, {...imports, env}) : instantiate(bytes, {...imports, env}, options);
        }
        WebAssembly.instantiate = profiledInstantiate;
    }
    async function measure(name: string, value: unknown) {
        const bytes = labWire.encode(value);
        for (let i = 0; i < 5; ++i) {
            labWire.encode(value);
            labWire.decode(bytes);
        }
        const times = [];
        for (let sample = 0; sample < 7; ++sample) {
            await new Promise(resolve => setTimeout(resolve, 0));
            const begin = performance.now();
            for (let i = 0; i < 20; ++i) {
                labWire.encode(value);
                labWire.decode(bytes);
            }
            times.push((performance.now() - begin) / 20);
        }
        times.sort((a, b) => a - b);
        for (const key of Object.keys(calls)) delete calls[key];
        countCalls = true;
        try {
            labWire.encode(value);
            labWire.decode(bytes);
        } finally {
            countCalls = false;
        }
        const hostCalls = {...calls};
        const decodeTimes = [], backgroundTimes = [], submissionTimes = [];
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
        const median = (values: number[]) => +values.sort((a, b) => a - b)[3].toFixed(3);
        return {name, bytes: bytes.length, median_roundtrip_ms: +times[3].toFixed(3), host_calls: hostCalls,
            ui_decode_ms: median(decodeTimes), async_submit_ms: median(submissionTimes),
            async_complete_ms: median(backgroundTimes)};
    }
    return {
        enabled,
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
            const workloads: [string, unknown][] = [
                ['scalar_request', {expression: 'sin(x)', precision: 78, persist_expression: false}],
                ['native_result', evaluated],
                [
                    'table', {
                        rows: Array.from({length: 256}, (_, i) => ({
                                                            name: `Town ${i}`,
                                                            latitude: i / 10,
                                                            longitude: -i / 20,
                                                            timezone: 'Europe/London',
                                                            description: 'μσ — exact text'
                                                        }))
                    }
                ],
                ['long_text', {expression: '1234567890'.repeat(6553)}],
                ['large_table', {rows: Array.from({length: 4096}, (_, i) => ({
                    name: `Item ${i}`, value: i / 10, description: 'μσ — exact text'.repeat(4)
                }))}],
                ['large_text', {text: '1234567890'.repeat(209715)}]
            ];
            const results = [];
            for (const [name, value] of workloads) results.push(await measure(name, value));
            const before = {...requests};
            await WorksheetRequests.evaluation.evaluateExpression();
            const evaluation_requests = Object.fromEntries(Object.entries(requests)
                                                               .map(([key, value]): [string, number] => [key, value - (before[key] || 0)])
                                                               .filter(([, value]) => value));
            return {startup, results, evaluation_requests};
        }
    };
})();
