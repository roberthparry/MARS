/**
 * @file globe_photo.ts
 * @brief Bounded asynchronous delivery of local photographic tiles to WebGL.
 *
 * Native code selects the geographic patch. The browser decodes four small files
 * and uploads a complete replacement, retaining the previous texture on failure.
 * No network imagery service or full-resolution global allocation is required.
 */
export class GlobePhoto {
    bounds = [-180, -90, 180, 90];
    ready = false;
    private key = '';
    private pendingKey = '';
    private request: AbortController | null = null;
    private texture: WebGLTexture | null = null;
    private readonly cache = new Map<string, ImageBitmap>();
    private closed = false;
    private retryAfter = 0;
    private readonly fallback: WebGLTexture | null;

    constructor(private readonly gl: WebGLRenderingContext, private readonly redraw: () => void) {
        const active = gl.getParameter(gl.ACTIVE_TEXTURE) as number;
        gl.activeTexture(gl.TEXTURE7);
        this.fallback = gl.getParameter(gl.TEXTURE_BINDING_2D) as WebGLTexture | null;
        gl.activeTexture(active);
    }

    /** Load only the native-selected patch; obsolete downloads cannot publish their texture. */
    async load(plan: number[]): Promise<void> {
        const gl = this.gl, key = plan.slice(0, 3).join(':');
        if (this.closed || gl.isContextLost() || gl.getParameter(gl.MAX_TEXTURE_SIZE) < 3600) return;
        if (key === this.pendingKey) return;
        if (key === this.key) {
            this.cancel();
            return;
        }
        if (performance.now() < this.retryAfter) return;
        this.cancel();
        const controller = new AbortController();
        this.request = controller;
        this.pendingKey = key;
        const canvas = document.createElement('canvas');
        canvas.width = canvas.height = 3600;
        const context = canvas.getContext('2d');
        let texture: WebGLTexture | null = null;
        try {
            if (!context) throw new Error('Photo patch canvas unavailable');
            for (let row = 0; row < 2; ++row) {
                for (let column = 0; column < 2; ++column) {
                    const name = `${String(plan[column]).padStart(2, '0')}-${String(plan[2] + row).padStart(2, '0')}`;
                    let bitmap = this.cache.get(name);
                    if (!bitmap) {
                        const response = await fetch(`/earth_tiles/${name}.jpg`, {signal: controller.signal});
                        if (!response.ok) throw new Error('Photo tile unavailable');
                        bitmap = await createImageBitmap(await response.blob());
                        if (controller.signal.aborted || this.closed) { bitmap.close(); return; }
                        if (bitmap.width !== 1800 || bitmap.height !== 1800) {
                            bitmap.close();
                            throw new Error('Invalid photo tile dimensions');
                        }
                        this.cache.set(name, bitmap);
                    } else {
                        this.cache.delete(name);
                        this.cache.set(name, bitmap);
                    }
                    context.drawImage(bitmap, column * 1800, row * 1800);
                    while (this.cache.size > 4) {
                        const oldest = this.cache.keys().next().value!;
                        this.cache.get(oldest)!.close();
                        this.cache.delete(oldest);
                    }
                }
            }
            if (controller.signal.aborted || this.closed || gl.isContextLost()) return;
            texture = gl.createTexture();
            if (!texture) throw new Error('Photo texture unavailable');
            gl.activeTexture(gl.TEXTURE7);
            gl.bindTexture(gl.TEXTURE_2D, texture);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, canvas);
            if (gl.getError() !== gl.NO_ERROR) throw new Error('Photo texture upload failed');
            gl.deleteTexture(this.texture);
            this.texture = texture;
            texture = null;
            this.bounds = plan.slice(3);
            this.key = key;
            this.ready = true;
            this.redraw();
        } catch {
            if (!controller.signal.aborted) this.retryAfter = performance.now() + 30000;
        } finally {
            canvas.width = canvas.height = 1;
            gl.deleteTexture(texture);
            if (!gl.isContextLost()) {
                gl.activeTexture(gl.TEXTURE7);
                gl.bindTexture(gl.TEXTURE_2D, this.texture || this.fallback);
            }
            if (this.request === controller) {
                this.request = null;
                this.pendingKey = '';
            }
        }
    }

    /** Cancel pending downloads while keeping the last complete patch drawable. */
    cancel(): void {
        this.request?.abort();
        this.request = null;
        this.pendingKey = '';
    }

    /** Release decoders and GPU storage when the page closes. */
    dispose(): void {
        this.closed = true;
        this.cancel();
        for (const bitmap of this.cache.values()) bitmap.close();
        this.cache.clear();
        this.gl.deleteTexture(this.texture);
        this.texture = null;
        this.ready = false;
    }
}
