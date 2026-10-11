/** Isolated workers, bounded C queue policy and asynchronous lifetime regressions. */
async function labCheckWorkers(check) {
    const native = labWire.exports();
    check(native.lab_jobs_count() === 0, 'worker queue starts idle');
    const tokens = Array.from({length: native.lab_jobs_capacity()}, () => native.lab_jobs_submit(1));
    check(tokens.every(Boolean) && !native.lab_jobs_submit(1), 'job count is bounded');
    check(native.lab_jobs_take() === tokens[0], 'native admission order is FIFO');
    check(native.lab_jobs_finish(tokens[0]) && !native.lab_jobs_finish(tokens[0]), 'stale completion rejected');
    const replacement = native.lab_jobs_submit(1);
    check(replacement !== tokens[0] && !native.lab_jobs_finish(tokens[0]), 'slot reuse has a fresh generation');
    check(native.lab_jobs_take() === tokens[1], 'replacement does not jump the queue');
    for (const token of [...tokens, replacement]) native.lab_jobs_finish(token);
    const full = [native.lab_jobs_submit(4194304), native.lab_jobs_submit(4194304)];
    check(full.every(Boolean) && !native.lab_jobs_submit(1), 'retained input budget is bounded');
    for (const token of full) native.lab_jobs_finish(token);
    check(!native.lab_jobs_submit(0) && !native.lab_jobs_submit(4194305), 'invalid message sizes rejected');

    const {createCodec} = await import('/js/codec.js');
    const {createJobs} = await import('/js/jobs.js');
    const codec = createCodec();
    await codec.start(await (await fetch('/wasm/lab_codec.wasm')).arrayBuffer());
    const policy = codec.exports();
    check(policy.memory.buffer instanceof ArrayBuffer, 'codec worker uses private memory');
    const url = path => new URL(path, location.href).href;
    const workerURL = url('/js/codec_worker.js'), wasmURL = url('/wasm/lab_codec.wasm');
    const pool = createJobs(policy, codec, workerURL, wasmURL);
    const rejects = async (promise, name) => {
        try {
            await promise;
            return false;
        } catch (error) {
            return !name || error.name === name;
        }
    };
    const value = {text: 'μ\u0000'.repeat(100000), values: [null, false, 0, '1/3']};
    const bytes = codec.encode(value);
    try {
        check((await pool.decode(codec.encode({small: true}))).small && pool.status().started === 0,
            'small messages need no worker');
        const cancelled = new AbortController();
        cancelled.abort();
        check(await rejects(pool.decode(bytes, cancelled.signal), 'AbortError'), 'pre-cancelled work rejected');
        const queued = new AbortController();
        let timerRan = false;
        const timer = new Promise(resolve => setTimeout(() => { timerRan = true; resolve(); }, 0));
        const first = pool.decode(bytes);
        const dropped = rejects(pool.decode(bytes, queued.signal), 'AbortError');
        queued.abort();
        const copy = await first;
        await timer;
        check(timerRan && await dropped && copy.text === value.text, 'UI timer and queued cancellation stay live');
        check(bytes.byteLength > 0 && copy.values[3] === '1/3', 'input remains owned and exact text survives');
        check(pool.status().started === 1 && pool.status().completed === 1, 'a dedicated worker handled the job');
        const responses = await Promise.all([pool.decode(bytes), pool.decode(bytes), pool.decode(bytes)]);
        check(responses.every(reply => reply.text === value.text), 'queued jobs retain separate results');
        check(pool.status().count === 0 && pool.status().bytes === 0, 'completed jobs release admission budgets');
        const malformed = bytes.slice();
        malformed[1] = 99;
        check(await rejects(pool.decode(malformed)), 'worker rejects malformed Protobuf');
        check((await pool.decode(bytes)).text === value.text, 'worker recovers after codec rejection');
        const polluted = Object.create(null);
        polluted.__proto__ = {safe: true};
        polluted.text = value.text;
        const safe = await pool.decode(codec.encode(polluted));
        check(Object.hasOwn(safe, '__proto__') && !Object.prototype.safe, 'worker cloning preserves safe own keys');
        const tasks = Array.from({length: 9}, () => pool.decode(bytes).then(() => true, () => false));
        const outcomes = await Promise.all(tasks);
        check(outcomes.slice(0, 8).every(Boolean) && !outcomes[8], 'host applies native queue backpressure');
    } finally {
        pool.close();
    }
    check(await rejects(pool.decode(bytes)), 'closed queue rejects');

    const hungURL = URL.createObjectURL(new Blob([
        'self.onmessage = ({data}) => { if (data.type === "init") self.postMessage({type:"ready"}); };'
    ], {type: 'text/javascript'}));
    const hung = createJobs(policy, codec, hungURL, wasmURL);
    try {
        const controller = new AbortController();
        const pending = rejects(hung.decode(bytes, controller.signal), 'AbortError');
        for (let i = 0; i < 1000 && !hung.status().active; ++i)
            await new Promise(resolve => setTimeout(resolve, 10));
        check(hung.status().active !== 0, 'controlled worker started a job');
        controller.abort();
        check(await pending && !hung.status().count && !hung.status().bytes, 'active cancellation releases the job');
        const waiting = rejects(hung.decode(bytes), 'AbortError');
        hung.close();
        check(await waiting && !hung.status().count, 'shutdown rejects pending work');
        const timed = createJobs({...policy, lab_jobs_timeout: () => 100}, codec, hungURL, wasmURL);
        try {
            check(await rejects(timed.decode(bytes), 'TimeoutError'), 'deadline includes worker startup');
            check(!timed.status().count && !timed.status().bytes, 'timeout releases budget');
        } finally {
            timed.close();
        }
    } finally {
        hung.close();
        URL.revokeObjectURL(hungURL);
    }
    const failed = createJobs(policy, codec, workerURL, url('/missing-codec.wasm'));
    try {
        check(await rejects(failed.decode(bytes)), 'worker startup failure rejects');
        check(!failed.status().count && !failed.status().bytes, 'startup failure releases budget');
    } finally {
        failed.close();
    }
    const unavailable = createJobs(policy, codec, url('/js/missing-worker.js'), wasmURL);
    try {
        check(await rejects(unavailable.decode(bytes)), 'worker script failure rejects');
        check(!unavailable.status().count && !unavailable.status().bytes, 'script failure releases budget');
    } finally {
        unavailable.close();
    }
    const originalFetch = window.fetch;
    try {
        window.fetch = async () => new Response(bytes.slice(), {headers: {'Content-Type': 'application/x-protobuf'}});
        const before = labWire.background().status().completed;
        const response = await labFetch('/worker-fixture');
        check((await response.labData()).text === value.text, 'production transport uses background decoding');
        check(labWire.background().status().completed === before + 1, 'production path reached a worker');
    } finally {
        window.fetch = originalFetch;
    }
}
