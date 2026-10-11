/** Browser capabilities for the C-owned bounded queue; one isolated worker, no UI-thread waits. */
export function createJobs(native, codec, workerURL, wasmURL) {
    const jobs = new Map();
    let worker = null, ready = false, active = 0, disposed = false;
    let started = 0, completed = 0;
    function stop() {
        if (worker)
            worker.terminate();
        worker = null;
        ready = false;
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
    function fail(error) {
        stop();
        // Map iteration is bounded by the eight-job native admission limit.
        for (const token of jobs.keys())
            retire(token, error);
    }
    function pump() {
        if (disposed || !jobs.size || active)
            return;
        if (!worker) {
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
                    if (data.type === 'ready') {
                        ready = true;
                    } else if (data.type === 'error' && !data.token) {
                        fail(new Error(data.message));
                        return;
                    } else if (data.token === active && (data.type === 'result' || data.type === 'error')) {
                        retire(data.token, data.type === 'error' ? new Error(data.message) : null, data.value);
                    } else {
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
        if (!ready)
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
            if (disposed)
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
                if (active === token)
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
            disposed = true;
            fail(new DOMException('Codec queue closed', 'AbortError'));
        }
    };
}
