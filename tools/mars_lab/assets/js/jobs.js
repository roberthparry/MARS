/** Browser resources for the C-owned queue and worker lifecycle; no UI-thread waits. */
// Numeric event/effect identifiers mirror lab_jobs.h and are covered by the module ABI.
export const worker_events = Object.freeze({pump: 0, ready: 1, result: 2, error: 3, failure: 4, cancel: 5, close: 6, unknown: 7});
export const worker_actions = Object.freeze({wait: 0, start: 1, send: 2, finish: 3, fail: 4, invalid: 5, stop: 6});

export function createJobs(native, codec, workerURL, wasmURL) {
    const jobs = new Map();
    let worker = null, active = 0, phase = 0;
    let started = 0, completed = 0;
    function transition(event, token = 0) {
        const plan = native.lab_jobs_worker_step(phase, event, token, active);
        phase = plan & 255;
        return plan >>> 8;
    }
    function stop() {
        if (worker)
            worker.terminate();
        worker = null;
    }
    function retire(token, error, value) {
        const job = jobs.get(token);
        if (!job || !native.lab_jobs_finish(token))
            return;
        jobs.delete(token);
        job.bytes?.fill(0);
        job.bytes = null;
        clearTimeout(job.timer);
        job.signal?.removeEventListener('abort', job.abort);
        if (active === token)
            active = 0;
        if (error)
            job.reject(error);
        else {
            ++completed;
            job.resolve(value);
        }
    }
    function fail(error, event = worker_events.failure) {
        transition(event);
        stop();
        // Map iteration is bounded by the eight-job native admission limit.
        for (const token of jobs.keys())
            retire(token, error);
    }
    function pump() {
        const action = transition(worker_events.pump);
        if (action === worker_actions.start) {
            try {
                const instance = new Worker(workerURL, {type: 'module'});
                worker = instance;
                ++started;
                instance.onerror = event => {
                    event.preventDefault();
                    if (worker === instance)
                        fail(new Error('Codec worker failed to start or execute'));
                };
                instance.onmessageerror = () => {
                    if (worker === instance)
                        fail(new Error('Invalid codec worker response'));
                };
                instance.onmessage = ({data}) => {
                    if (worker !== instance)
                        return;
                    const message = data && typeof data === 'object' ? data : {};
                    const event = message.type === 'ready' ? worker_events.ready :
                        message.type === 'result' ? worker_events.result :
                        message.type === 'error' ? worker_events.error : worker_events.unknown;
                    const token = typeof message.token === 'number' ? message.token :
                        message.token == null ? 0 : NaN;
                    const effect = transition(event, token);
                    if (effect === worker_actions.fail) {
                        fail(new Error(message.message));
                        return;
                    } else if (effect === worker_actions.finish) {
                        retire(active, event === worker_events.error ? new Error(message.message) : null, message.value);
                    } else if (effect === worker_actions.invalid) {
                        fail(new Error('Unexpected codec worker response'));
                        return;
                    }
                    pump();
                };
                instance.postMessage({type: 'init', url: wasmURL});
            } catch (error) {
                fail(error);
            }
            return;
        }
        if (action === worker_actions.invalid) {
            fail(new Error('Invalid codec worker state'));
            return;
        }
        if (action !== worker_actions.send)
            return;
        const token = native.lab_jobs_take();
        const job = jobs.get(token);
        if (!job)
            return;
        active = token;
        try {
            worker.postMessage({type: 'decode', token, bytes: job.bytes}, [job.bytes.buffer]);
            job.bytes = null;
        } catch (error) {
            fail(error);
        }
    }
    function decode(buffer, signal) {
        return new Promise((resolve, reject) => {
            if (!native.lab_jobs_worker_accepts(phase))
                throw new Error('Codec queue is closed');
            if (signal?.aborted)
                throw signal.reason || new DOMException('Cancelled', 'AbortError');
            const bytes = codec.bytes(buffer);
            if (bytes.length < native.lab_jobs_threshold()) {
                resolve(codec.decode(bytes));
                return;
            }
            const token = native.lab_jobs_submit(bytes.length);
            if (!token)
                throw new Error('Codec queue is full');
            let copy;
            try {
                copy = bytes.slice();
            } catch (error) {
                native.lab_jobs_finish(token);
                throw error;
            }
            const abort = error => {
                if (transition(worker_events.cancel, token) === worker_actions.stop)
                    stop();
                retire(token, error);
                pump();
            };
            const job = {bytes: copy, resolve, reject, signal,
                abort: () => abort(signal.reason || new DOMException('Cancelled', 'AbortError'))};
            jobs.set(token, job);
            job.timer = setTimeout(() => abort(new DOMException('Codec job timed out', 'TimeoutError')),
                native.lab_jobs_timeout());
            signal?.addEventListener('abort', job.abort, {once: true});
            pump();
        });
    }
    return {
        decode,
        status: () => ({count: native.lab_jobs_count(), bytes: native.lab_jobs_bytes(), active, started, completed}),
        close() {
            fail(new DOMException('Codec queue closed', 'AbortError'), worker_events.close);
        }
    };
}
