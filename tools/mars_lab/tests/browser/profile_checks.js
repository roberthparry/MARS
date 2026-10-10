/** Opt-in real-browser profiling. Timings are observations, never correctness thresholds. */
const labProfile = (() => {
    const enabled = new URLSearchParams(location.search).has('profile');
    const requests = Object.create(null);
    const calls = Object.create(null);
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
        WebAssembly.instantiate = (bytes, imports) => {
            const env = {};
            for (const [name, fn] of Object.entries(imports.env || {}))
                env[name] = (...args) => {
                    if (countCalls)
                        calls[name] = (calls[name] || 0) + 1;
                    return fn(...args);
                };
            return instantiate(bytes, {...imports, env});
        };
    }
    async function measure(name, value) {
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
        return {name, bytes: bytes.length, median_roundtrip_ms: +times[3].toFixed(3), host_calls: {...calls}};
    }
    return {
        enabled,
        async run() {
            const startup = {milliseconds: +(performance.now() - started).toFixed(1), requests: {...requests}};
            const response = await labFetch('/eval', {
                method: 'POST',
                body: labWire.encode(
                    {expression: '{ sin(x)^2 + cos(x)^2 | x = 1/3 }', precision: 17, persist_expression: false})
            });
            const evaluated = await response.labData();
            if (!response.ok || !evaluated.ok)
                throw new Error('Profile evaluation failed');
            const workloads = [
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
                ['long_text', {expression: '1234567890'.repeat(6553)}]
            ];
            const results = [];
            for (const [name, value] of workloads) results.push(await measure(name, value));
            const before = {...requests};
            await evaluateExpression();
            const evaluation_requests = Object.fromEntries(Object.entries(requests)
                                                               .map(([key, value]) => [key, value - (before[key] || 0)])
                                                               .filter(([, value]) => value));
            return {startup, results, evaluation_requests};
        }
    };
})();
