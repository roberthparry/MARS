/**
 * @file globe_surface.ts
 * @brief Cancellable delivery of cached regional surface tiles from a dedicated worker.
 *
 * Only completed bitmaps cross to the animation thread. Superseded results are closed immediately; worker
 * failures leave the globe's existing surface and independent river fallback available.
 */
export class GlobeSurface {
    private worker: Worker | null = null;
    private next = 0;
    private current: {id: number; reject(error: Error): void; finish(bitmap: ImageBitmap): void} | null = null;

    load(bounds: number[], signal: AbortSignal): Promise<ImageBitmap> {
        if (signal.aborted) return Promise.reject(new DOMException('Aborted', 'AbortError'));
        this.current?.reject(new DOMException('Superseded', 'AbortError'));
        return new Promise((resolve, reject) => {
            const id = ++this.next;
            const abort = () => {
                if (this.current?.id !== id) return;
                this.worker?.postMessage({id, cancel: true});
                finish(null, new DOMException('Aborted', 'AbortError'));
            };
            const finish = (bitmap: ImageBitmap | null, error?: Error) => {
                signal.removeEventListener('abort', abort);
                if (this.current?.id === id) this.current = null;
                if (bitmap) resolve(bitmap); else reject(error);
            };
            this.current = {id, reject: error => finish(null, error), finish: bitmap => finish(bitmap)};
            signal.addEventListener('abort', abort, {once: true});
            try {
                if (!this.worker) {
                    const worker = this.worker = new Worker('/js/globe_surface_worker.js');
                    worker.onmessage = event => {
                        const {id: response, bitmap, error} = event.data;
                        const current = this.current;
                        if (!current || current.id !== response) { bitmap?.close(); return; }
                        if (error) current.reject(new Error(error));
                        else if (bitmap) current.finish(bitmap);
                        else current.reject(new Error('Missing surface bitmap'));
                    };
                    worker.onerror = event => {
                        event.preventDefault();
                        if (this.worker !== worker) return;
                        worker.terminate(); this.worker = null;
                        this.current?.reject(new Error(event.message));
                    };
                }
                this.worker.postMessage({id, bounds});
            } catch (error) { finish(null, error instanceof Error ? error : new Error(String(error))); }
        });
    }

    dispose(): void {
        this.current?.reject(new Error('Globe closed'));
        this.worker?.terminate();
        this.worker = null;
    }
}
