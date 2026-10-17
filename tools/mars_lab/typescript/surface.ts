/**
 * @file surface.ts
 * @brief Retained plot geometry and browser rendering resources.
 *
 * Owns GPU and fallback rendering for native geometry, plus SVG refinement for curves.
 * Camera arithmetic and mathematical evaluation remain native; retained views manage
 * browser gestures and resource cleanup independently.
 */
/**
 * Retained native surface geometry for local camera interaction. No expressions
 * are evaluated here: normalised vertices, colours and labels come from MARS.
 * WebGL depth-tests triangles or line segments; a painter-ordered canvas is the fallback.
 * Surfaces retain WebGL at rest; line curves settle to batched SVG. Vector export
 * uses the same native projection as the interactive view.
 */
/** Owns a WebGL context and every shader/program/buffer allocated for one retained view. */
class MarsSurfaceGPU {
    private disposed = false;
    private constructor(
        private readonly gl: WebGLRenderingContext,
        private readonly canvas: HTMLCanvasElement,
        private readonly vertexCount: number,
        private readonly segments: boolean,
        private readonly program: WebGLProgram,
        private readonly buffer: WebGLBuffer,
        private readonly shaders: WebGLShader[],
        private readonly camera: WebGLUniformLocation | null,
        private readonly domainScale: WebGLUniformLocation | null,
        private readonly domainOffset: WebGLUniformLocation | null,
        readonly maxSize: number
    ) {}

    /**
     * @brief Allocate a renderer for native interleaved position, colour and corner attributes.
     * Returns null when WebGL is unavailable or setup fails, releasing partial allocations.
     * The successful renderer owns its context and resources until dispose() is called.
     */
    static create(canvas: HTMLCanvasElement, vertices: Float32Array, segments: boolean): MarsSurfaceGPU | null {
        let gl;
        try { gl = canvas.getContext('webgl', {alpha: true, antialias: true, preserveDrawingBuffer: false}); }
        catch (_) { return null; }
        if (!gl) return null;
        const shaders: WebGLShader[] = [];
        let program: WebGLProgram | null = null, buffer: WebGLBuffer | null = null;
        try {
            const precision = (gl.getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.HIGH_FLOAT)?.precision ?? 0) > 0 ?
                'highp' : 'mediump';
            const sources: [number, string][] = [[gl.VERTEX_SHADER, `
                attribute vec3 position; attribute vec3 colour; attribute vec3 corner;
                uniform mat4 camera; uniform vec3 domainScale; uniform vec3 domainOffset;
                varying mediump vec3 shade; varying mediump vec3 edge; varying ${precision} vec3 coordinate;
                void main() {
                    coordinate = (position - vec3(0.5)) * domainScale + domainOffset;
                    gl_Position = camera * vec4(coordinate, 1.0); shade = colour; edge = corner;
                }
            `], [gl.FRAGMENT_SHADER, `
                precision ${precision} float;
                varying mediump vec3 shade; varying mediump vec3 edge; varying ${precision} vec3 coordinate;
                void main() {
                    if (any(greaterThan(abs(coordinate), vec3(0.5000001)))) discard;
                    float wire = smoothstep(0.0, 0.025, min(edge.x, min(edge.y, edge.z)));
                    gl_FragColor = vec4(mix(shade * 0.65, shade, wire), 1.0);
                }
            `]];
            for (const [type, source] of sources) {
                const shader = gl.createShader(type);
                if (!shader) throw new Error('Surface shader allocation failed');
                shaders.push(shader);
                gl.shaderSource(shader, source);
                gl.compileShader(shader);
                if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error('Surface shader unavailable');
            }
            program = gl.createProgram();
            if (!program) throw new Error('Surface program allocation failed');
            for (const shader of shaders) gl.attachShader(program, shader);
            gl.linkProgram(program);
            if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error('Surface program unavailable');
            gl.useProgram(program);
            buffer = gl.createBuffer();
            if (!buffer) throw new Error('Surface buffer allocation failed');
            gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
            gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
            for (const [i, name] of ['position', 'colour', 'corner'].entries()) {
                const location = gl.getAttribLocation(program, name);
                gl.enableVertexAttribArray(location);
                gl.vertexAttribPointer(location, 3, gl.FLOAT, false, 36, i * 12);
            }
            const camera = gl.getUniformLocation(program, 'camera');
            const domainScale = gl.getUniformLocation(program, 'domainScale');
            const domainOffset = gl.getUniformLocation(program, 'domainOffset');
            gl.enable(gl.DEPTH_TEST);
            gl.disable(gl.CULL_FACE);
            const viewport = gl.getParameter(gl.MAX_VIEWPORT_DIMS) as Int32Array;
            const maximum = gl.getParameter(gl.MAX_RENDERBUFFER_SIZE) as number;
            return new MarsSurfaceGPU(gl, canvas, vertices.length / 9, segments, program, buffer,
                shaders, camera, domainScale, domainOffset, Math.min(4096, maximum, ...viewport));
        } catch (_) {
            if (buffer) gl.deleteBuffer(buffer);
            if (program) gl.deleteProgram(program);
            for (const shader of shaders) gl.deleteShader(shader);
            gl.getExtension('WEBGL_lose_context')?.loseContext();
            return null;
        }
    }

    /**
     * @brief Clear the backing canvas and draw retained geometry using native projection parameters.
     * A false visibility flag clears without drawing; a lost or disposed context returns false
     * so the caller can switch to software rendering.
     */
    draw(matrix: Float32Array, scaling: number[], offset: number[], visible: boolean): boolean {
        const gl = this.gl;
        if (this.disposed || gl.isContextLost()) return false;
        gl.viewport(0, 0, this.canvas.width, this.canvas.height);
        gl.clearColor(0, 0, 0, 0);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        gl.uniformMatrix4fv(this.camera, false, matrix);
        gl.uniform3fv(this.domainScale, scaling);
        gl.uniform3fv(this.domainOffset, offset);
        gl.drawArrays(this.segments ? gl.LINES : gl.TRIANGLES, 0, visible ? this.vertexCount : 0);
        return true;
    }

    /** @brief Release the GPU allocations and context once; repeated disposal is harmless. */
    dispose(): void {
        if (this.disposed) return;
        this.disposed = true;
        this.gl.deleteBuffer(this.buffer);
        this.gl.deleteProgram(this.program);
        for (const shader of this.shaders) this.gl.deleteShader(shader);
        this.gl.getExtension('WEBGL_lose_context')?.loseContext();
    }
}

/** Owns the active view, overlay, observers, frame scheduling and snapshot lifecycle. */
class MarsSurfacePreview {
    private current: MarsSurfaceView | null = null;
    private resize_frame = 0;
    private restoration_frame = 0;
    private closed = false;
    private hover: MarsSurfaceHover | undefined;
    private readonly onHover = (event: Event): void => {
        if (this.closed) return;
        this.hover ??= new MarsSurfaceHover(chart => this.host.volume.interacting ? null : this.prepare(chart),
            this.host.native, this.events);
        this.hover.inspect(event);
    };

    /**
     * @brief Register page-lifetime resize and page-hide callbacks for the retained preview.
     * View resources are allocated lazily; dispose() releases a view, not these listeners.
     */
    constructor(private readonly host: MarsSurfacePreviewHost, private readonly events: EventTarget = window) {
        this.events.addEventListener('pagehide', this.onPageHide);
        this.events.addEventListener('resize', this.onResize);
        this.events.addEventListener('pointermove', this.onHover);
    }

    private removeSettled(view: MarsSurfaceView): void {
        clearTimeout(view.settleTimer);
        view.settled?.remove();
        view.settled = null;
    }

    // Line plots use the retained native projection for a crisp, vector idle view.
    // Keep the original mesh and GPU alive so a new gesture resumes immediately.
    private scheduleSettled(view: MarsSurfaceView): void {
        if (view.chart.dataset.segments !== 'true') return;
        clearTimeout(view.settleTimer);
        view.settleTimer = setTimeout(() => {
            if (this.current !== view || !view.chart.isConnected) return;
            const drag = (document.getElementById('plotRendered') as
                (HTMLElement & {marsPlotDrag?: MarsSurfaceGesture}) | null)?.marsPlotDrag;
            if (drag?.active || drag?.wheel || drag?.busy || this.host.volume.interacting) {
                this.scheduleSettled(view);
                return;
            }
            const drawing = this.host.exporter.layer(view);
            if (!drawing) return;
            view.settled?.remove();
            drawing.dataset.surfaceSettled = 'true';
            view.chart.appendChild(drawing);
            view.settled = drawing;
            view.overlay.style.display = 'none';
            view.chart.dataset.localRenderer = 'svg';
        }, 150);
    }

    /**
     * @brief Restore native SVG visibility, or redraw a connected view unless forced.
     * Hides transient overlays and cancels settled-curve work without destroying retained
     * geometry or observers, allowing captureResult() to restore the interactive view.
     */
    clear(force = false): void {
        if (!this.current) return;
        if (!force && this.current.chart.isConnected) {
            this.draw(this.current.chart, this.current.angles);
            return;
        }
        this.removeSettled(this.current);
        for (const [node, visibility] of this.current.hidden) node.style.visibility = visibility;
        this.current.hidden = [];
        this.current.overlay.style.display = 'none';
        delete this.current.chart.dataset.localRenderer;
        delete this.current.chart.dataset.localCamera;
    }

    /** @brief Cancel the active view's frame, disconnect observers and release its overlay and GPU. */
    dispose(): void {
        this.hover?.hide();
        this.clear(true);
        if (!this.current) return;
        cancelAnimationFrame(this.current.frame);
        this.current.gpu?.dispose();
        this.current.observer.disconnect();
        this.current.resizeObserver?.disconnect();
        this.current.overlay.remove();
        this.current = null;
    }

    private signature(chart: SVGSVGElement): string {
        return JSON.stringify([chart.closest('[data-plot-figure]')?.querySelector('title')?.textContent,
            ...['exactXMin', 'exactXMax', 'exactYMin', 'exactYMax', 'exactZMin', 'exactZMax', 'units']
                .map(key => chart.dataset[key])]);
    }

    private identity(chart: SVGSVGElement): string {
        return chart.closest('[data-plot-figure]')?.querySelector('title')?.textContent || '';
    }

    /**
     * @brief Copy the active camera, orientation and volume into a serialisable snapshot.
     * Returns null when no view is retained; no canvas, observer or GPU resource is copied.
     */
    snapshot(): MarsSurfaceSnapshot | null {
        const view = this.current;
        return view ? {key: this.signature(view.chart), identity: this.identity(view.chart), camera: {...view.camera},
            orientation: [...view.orientation], angles: [...view.angles],
            volume: this.host.volume.clone(view.volume)} : null;
    }

    /**
     * @brief Capture result state with transient renderers removed from the saved DOM.
     * Adds a surface snapshot to a returned object and restores the same connected view in
     * a finally block, including when the supplied capture callback throws.
     */
    captureResult<T extends object | null | undefined>(capture: () => T): T {
        const view = this.current, saved = this.snapshot();
        if (!view) return capture();
        this.clear(true);
        view.overlay.remove();
        try {
            const state = capture();
            if (state) Object.assign(state, {surfaceView: saved});
            return state;
        } finally {
            if (this.current === view && view.chart.isConnected) {
                view.host.appendChild(view.overlay);
                this.draw(view.chart, view.angles);
            }
        }
    }

    /** @brief Restore a saved camera on the next frame, provided the restored chart remains connected. */
    restoreResult(saved: MarsSurfaceSnapshot | null): void {
        cancelAnimationFrame(this.restoration_frame);
        this.restoration_frame = 0;
        if (this.closed) return;
        const chart = document.querySelector<SVGSVGElement>('#plotRendered [data-surface]');
        if (!chart) return;
        // Mode controls and fitted card dimensions settle after DOM restoration.
        this.restoration_frame = requestAnimationFrame(() => {
            this.restoration_frame = 0;
            if (this.closed || !chart.isConnected) return;
            const view = this.prepare(chart, saved);
            if (view) this.draw(chart, view.angles);
        });
    }

    private syncZoom(view: MarsSurfaceView): void {
        this.host.native.call<void>('lab_surface_zoom_sync', view);
    }

    /**
     * @brief Reuse the active chart or allocate a retained view from native SVG geometry.
     * Replacing a chart disposes its predecessor. Invalid geometry or unavailable drawing
     * contexts return null; software forces the canvas fallback. Compatible snapshots retain
     * camera state, while observers release detached views and resize their backing stores.
     */
    prepare(chart: SVGSVGElement | null, saved: MarsSurfaceSnapshot | null = null, software = false): MarsSurfaceView | null {
        if (this.closed) return null;
        if (this.current?.chart === chart) return this.current;
        this.dispose();
        if (!chart?.isConnected || !chart.dataset.units) return null;
        const primitives: MarsSurfacePrimitive[] = [], data: number[] = [];
        const segments = chart.dataset.segments === 'true';
        for (const path of chart.querySelectorAll<SVGElement>('[data-segment]')) {
            const points = (path.dataset.segment ?? '').split(' ').map(Number);
            if (points.length !== 6 || !points.every(Number.isFinite)) return null;
            const rgb = Number.parseInt((path.getAttribute('stroke') ?? '#000000').slice(1), 16);
            const colour = [(rgb >> 16 & 255) / 255, (rgb >> 8 & 255) / 255, (rgb & 255) / 255];
            const ends: MarsSurfacePoint3[] = [[points[0], points[1], points[2]], [points[3], points[4], points[5]]];
            primitives.push({points: ends, colour: (path.getAttribute('stroke') ?? '#000000'), depth: 0});
            ends.forEach(point => data.push(...point, ...colour, 1, 1, 1));
        }
        for (const path of chart.querySelectorAll<SVGElement>('[data-vertices]')) {
            const points = (path.dataset.vertices ?? '').split(' ').map(Number);
            if (points.length < 9 || points.length > 15 || points.length % 3 || !points.every(Number.isFinite))
                return null;
            const rgb = Number.parseInt((path.getAttribute('fill') ?? '#000000').slice(1), 16);
            const colour = [(rgb >> 16 & 255) / 255, (rgb >> 8 & 255) / 255, (rgb & 255) / 255];
            for (let i = 3; i + 3 < points.length; i += 3) {
                const triangle: MarsSurfacePoint3[] = [[points[0], points[1], points[2]],
                    [points[i], points[i + 1], points[i + 2]], [points[i + 3], points[i + 4], points[i + 5]]];
                primitives.push({points: triangle, colour: (path.getAttribute('fill') ?? '#000000'), depth: 0});
                triangle.forEach((point, j) => data.push(...point, ...colour,
                                                       Number(j === 0), Number(j === 1), Number(j === 2)));
            }
        }
        if (!primitives.length && chart.dataset.empty !== 'true') return null;
        const units = chart.dataset.units.split(' ').map(Number);
        if (units.length !== 3 || !units.every(x => Number.isFinite(x) && x > 0)) return null;
        const width = chart.viewBox.baseVal.width, height = chart.viewBox.baseVal.height;
        // Keep composited canvases out of SVG foreignObject: mobile engines can
        // position those layers outside the fitted SVG when they first appear.
        const host = chart.closest<HTMLElement>('.rendered');
        if (!host) return null;
        const overlay = document.createElement('div');
        overlay.dataset.surfaceOverlay = '';
        overlay.style.cssText = `position:absolute;left:0;top:0;width:${width}px;height:${height}px;` +
            'transform-origin:0 0;pointer-events:none;display:none;overflow:hidden';
        const holder = document.createElement('div');
        holder.style.cssText = `position:relative;width:${width}px;height:${height}px;` +
            'transform-origin:0 0;pointer-events:none';
        let canvas = document.createElement('canvas');
        const vertices = new Float32Array(data);
        const renderer = software ? null : MarsSurfaceGPU.create(canvas, vertices, segments);
        if (!renderer) canvas = document.createElement('canvas');
        const labels = document.createElement('canvas');
        for (const layer of [canvas, labels]) {
            // Allocate the backing stores from their displayed size on the first draw.
            layer.width = 1;
            layer.height = 1;
            layer.style.cssText = `position:absolute;left:0;top:0;width:${width}px;height:${height}px`;
            holder.appendChild(layer);
        }
        const context = renderer ? null : canvas.getContext('2d');
        const ink = labels.getContext('2d');
        if ((!renderer && !context) || !ink) { renderer?.dispose(); return null; }
        overlay.appendChild(holder);
        host.appendChild(overlay);
        const ticks = [...chart.querySelectorAll<SVGElement>('[data-tick-fraction]')].map(node => ({
            axis: Number(node.dataset.plotTick), fraction: Number(node.dataset.tickFraction),
            label: node.nextElementSibling?.tagName === 'text' ? (node.nextElementSibling.textContent ?? '') : ''
        }));
        const names = [0, 1, 2].map(axis => chart.querySelector(`[data-axis-name="${axis}"]`)?.textContent || '');
        const observer = new MutationObserver(() => { if (!chart.isConnected) this.dispose(); });
        const rendered = document.getElementById('plotRendered');
        if (rendered) observer.observe(rendered, {childList: true, subtree: true});
        const view: MarsSurfaceInitialView = {
            frame: 0, settled: null, orientation: [0, 0, 0, 1],
            chart, host, holder, primitives, vertices, units, width, height, overlay, gpu: renderer, context, ink,
            ticks, names, observer, hidden: [],
            camera: saved && (saved.key === this.signature(chart) || saved.refining) ?
                {...saved.camera} : {scale: 1, x: 0, y: 0},
            angles: ['Azimuth', 'Elevation', 'Roll'].map(key =>
                Number((document.getElementById('plot' + key) as HTMLInputElement | null)?.value ?? 0))
        };
        const preserveOrientation = saved?.orientation && (saved.refining ||
            (saved.identity === this.identity(chart) &&
                view.angles.every((value, i) => Math.abs(value - saved.angles[i]) <= 0.51)));
        view.orientation = preserveOrientation ?
            [...saved.orientation] : this.host.camera.fromAngles(view.angles);
        if (preserveOrientation) view.angles = [...saved.angles];
        this.host.volume.initialise(view, saved, saved?.key === this.signature(chart));
        this.current = view;
        canvas.addEventListener('webglcontextlost', event => {
            event.preventDefault();
            if (this.current?.overlay !== overlay) return;
            const savedView = this.snapshot();
            this.dispose();
            const fallback = this.prepare(chart, savedView, true);
            if (fallback) this.draw(chart, fallback.angles);
        });
        if (window.ResizeObserver) {
            view.resizeObserver = new ResizeObserver(() => {
                if (this.current !== view || view.frame) return;
                view.frame = requestAnimationFrame(() => {
                    view.frame = 0;
                    if (this.current === view && chart.isConnected) this.draw(chart, view.angles);
                });
            });
            view.resizeObserver.observe(host);
        }
        this.syncZoom(this.current);
        return this.current;
    }

    // Browser pixel sizing only: geometry and camera calculations remain native.
    private resizeBacking(view: MarsSurfaceView, screen: DOMMatrix): void {
        const pixels = window.devicePixelRatio || 1;
        const xScale = Math.hypot(screen.a, screen.b) * pixels;
        const yScale = Math.hypot(screen.c, screen.d) * pixels;
        const limit = view.gpu?.maxSize || 4096;
        const requestedWidth = view.width * xScale, requestedHeight = view.height * yScale;
        const fit = Math.min(1, limit / requestedWidth, limit / requestedHeight);
        const width = Math.max(1, Math.ceil(requestedWidth * fit));
        const height = Math.max(1, Math.ceil(requestedHeight * fit));
        for (const layer of view.holder.children) {
            if (!marsTransportIsDOM(layer, 'HTMLCanvasElement')) continue;
            if (layer.width !== width) layer.width = width;
            if (layer.height !== height) layer.height = height;
        }
        view.context?.setTransform(width / view.width, 0, 0, height / view.height, 0, 0);
        view.ink.setTransform(width / view.width, 0, 0, height / view.height, 0, 0);
    }

    /**
     * @brief Render a native-projected frame and synchronise its axes, overlay and zoom metadata.
     * Uses device-pixel-sized backing stores and falls back to canvas after GPU failure.
     * Returns false when the chart, angles or native frame cannot be used; line views settle
     * to SVG after interaction ends without discarding their retained geometry.
     */
    draw(chart: SVGSVGElement, angles: MarsSurfaceAngles, bounds: number[] | null = null): boolean {
        this.hover?.hide();
        const view = this.prepare(chart);
        if (!view || !angles.every(Number.isFinite)) return false;
        this.removeSettled(view);
        if (angles.some((value, i) => Math.abs(value - view.angles[i]) > 1e-9))
            view.orientation = this.host.camera.fromAngles(angles);
        view.angles = [...angles];
        // Map the native chart's coordinate system into the HTML scroll content,
        // including nested SVG headings, mobile fitting, borders and scroll offsets.
        const screen = chart.getScreenCTM(), box = view.host.getBoundingClientRect();
        if (!screen) return false;
        this.resizeBacking(view, screen);
        const tx = screen.e - box.left - view.host.clientLeft + view.host.scrollLeft;
        const ty = screen.f - box.top - view.host.clientTop + view.host.scrollTop;
        const xs = [0, screen.a * view.width, screen.c * view.height,
                    screen.a * view.width + screen.c * view.height];
        const ys = [0, screen.b * view.width, screen.d * view.height,
                    screen.b * view.width + screen.d * view.height];
        const overlayLeft = Math.min(...xs), overlayTop = Math.min(...ys);
        // Size the clipping layer in screen pixels: transforming an 800px-wide
        // outer box alone can still enlarge the mobile scrollable overflow area.
        Object.assign(view.overlay.style, {
            left: `${tx + overlayLeft}px`, top: `${ty + overlayTop}px`,
            width: `${Math.max(...xs) - overlayLeft}px`, height: `${Math.max(...ys) - overlayTop}px`
        });
        view.holder.style.transform =
            `matrix(${screen.a},${screen.b},${screen.c},${screen.d},${-overlayLeft},${-overlayTop})`;
        const {width, height, vertices} = view;
        const frame = this.host.native.call<MarsSurfaceFrame | null>('lab_surface_frame', view, bounds);
        if (!frame) return false;
        view.framePlan = frame;
        view.project = point => this.host.native.call<MarsSurfacePoint2>('lab_surface_project', frame, point);
        view.polygon = points => this.host.native.call<MarsSurfacePolygon>('lab_surface_polygon', frame, points);
        const {scaling, offset} = frame;
        const matrix = new Float32Array(frame.matrix);
        if (view.gpu) {
            if (!view.gpu.draw(matrix, scaling, offset, frame.drawMesh)) {
                const savedView = this.snapshot();
                this.dispose();
                const fallback = this.prepare(chart, savedView, true);
                return fallback ? this.draw(chart, fallback.angles, bounds) : false;
            }
        } else {
            const ctx = view.context;
            if (!ctx) return false;
            ctx.clearRect(0, 0, width, height);
            const triangles = (frame.drawMesh ? view.primitives : []).map(triangle => ({
                ...this.host.native.call<MarsSurfacePolygon>('lab_surface_polygon', frame, triangle.points), colour: triangle.colour
            })).filter(triangle => triangle.points.length >= (chart.dataset.segments === 'true' ? 2 : 3));
            triangles.sort((a, b) => b.depth - a.depth);
            ctx.strokeStyle = '#193d4659';
            ctx.lineWidth = 0.45;
            for (const triangle of triangles) {
                ctx.beginPath();
                triangle.points.forEach((p, i) => { i ? ctx.lineTo(...p) : ctx.moveTo(...p); });
                if (chart.dataset.segments === 'true') {
                    ctx.strokeStyle = triangle.colour;
                    ctx.lineWidth = 2;
                    ctx.stroke();
                    continue;
                }
                ctx.closePath();
                ctx.fillStyle = triangle.colour;
                ctx.fill(); ctx.stroke();
            }
        }
        view.paintAxes = ctx => {
            ctx.clearRect(0, 0, width, height);
            ctx.strokeStyle = '#94b8b1'; ctx.fillStyle = '#b8ccc5'; ctx.lineWidth = 1.2;
            ctx.font = '11px sans-serif'; ctx.textAlign = 'center';
            for (const path of frame.paths) {
                ctx.beginPath();
                ctx.moveTo(path[0], path[1]);
                for (let i = 2; i < path.length; i += 2) ctx.lineTo(path[i], path[i + 1]);
                ctx.stroke();
            }
            for (const text of frame.texts) {
                ctx.textAlign = text.align;
                ctx.fillText(text.text, text.x, text.y);
            }
        };
        view.paintAxes(view.ink);
        if (!view.hidden.length) {
            for (const child of chart.children) {
                if (marsTransportIsDOM(child, 'SVGElement') && (child.tagName === 'g' || child.tagName === 'path')) {
                    view.hidden.push([child, child.style.visibility]);
                    child.style.visibility = 'hidden';
                }
            }
        }
        view.overlay.style.display = '';
        chart.dataset.localRenderer = view.gpu ? 'webgl' : 'canvas';
        chart.dataset.localCamera = angles.join(',');
        chart.dataset.cameraZoom = String(view.camera.scale);
        chart.dataset.volumeBounds = view.volume.bounds.join(',');
        chart.dataset.cameraQuaternion = view.orientation.join(',');
        this.syncZoom(view);
        this.scheduleSettled(view);
        return true;
    }

    /**
     * @brief Apply native trackball rotation and synchronise the camera controls before drawing.
     * Records the gesture's starting orientation for cancellation; slider and wheel actions
     * use their supplied angles instead of starting a trackball gesture.
     */
    rotate(chart: SVGSVGElement, state: MarsSurfaceGesture, angles: MarsSurfaceAngles): boolean {
        const view = this.prepare(chart);
        if (!view) return false;
        if (state.slider || state.wheel || !state.trackball) return this.draw(chart, angles);
        if (!state.rotationStart) {
            state.rotationStart = [...view.orientation];
            state.rotationBox = chart.getBoundingClientRect();
        }
        view.orientation = state.rotationCancelled ? [...state.rotationStart] :
            this.host.camera.drag(state.rotationStart, [state.x, state.y],
                [state.touchX, state.touchY], state.rotationBox ?? chart.getBoundingClientRect(), state.rollOnly);
        view.angles = this.host.camera.angles(view.orientation, view.angles);
        ['plotAzimuth', 'plotElevation', 'plotRoll'].forEach((id, i) => {
            const control = document.getElementById(id) as HTMLInputElement | null, value = view.angles[i];
            if (control) control.value = String(value);
            const output = document.getElementById(id + 'Value');
            if (output) output.textContent = String(Math.round(value)) + '°';
        });
        return this.draw(chart, view.angles);
    }

    /**
     * @brief Apply native volume zoom and coalesce the redraw into one animation frame.
     * Pointer coordinates are retained in the gesture API but unused here. Returns false
     * when preparation or native zoom fails; reset requests the native default volume.
     */
    zoom(chart: SVGSVGElement, factor: number, _x: number, _y: number, reset = false): boolean {
        const view = this.prepare(chart);
        if (!view || !this.host.volume.set(view, factor, reset)) return false;
        if (!view.frame) view.frame = requestAnimationFrame(() => {
            view.frame = 0;
            if (this.current === view && chart.isConnected) this.draw(chart, view.angles);
        });
        return true;
    }

    /** @brief Apply a native pinch-volume update and redraw using the retained camera angles. */
    pinch(chart: SVGSVGElement, state: MarsSurfaceGesture, _angles: MarsSurfaceAngles): boolean {
        const view = this.prepare(chart);
        if (!view) return false;
        if (!this.host.volume.set(view, 0, false, 3, state)) return false;
        return this.draw(chart, view.angles);
    }

    /** @brief Redraw the retained view and serialise its vector projection, or return null if unavailable. */
    exportSvg(): string | null {
        if (!this.current || !this.draw(this.current.chart, this.current.angles)) return null;
        return this.host.exporter.svg(this.current);
    }

    /** Release page-lifetime subscriptions and all retained view resources. */
    public close(): void {
        this.closed = true;
        this.events.removeEventListener('pointermove', this.onHover);
        this.hover?.dispose();
        this.events.removeEventListener('pagehide', this.onPageHide);
        this.events.removeEventListener('resize', this.onResize);
        cancelAnimationFrame(this.resize_frame);
        this.resize_frame = 0;
        cancelAnimationFrame(this.restoration_frame);
        this.restoration_frame = 0;
        this.dispose();
    }

    private readonly onPageHide = (): void => { this.dispose(); };
    private readonly onResize = (): void => {
        if (this.closed) return;
        cancelAnimationFrame(this.resize_frame);
        this.resize_frame = requestAnimationFrame(() => {
            this.resize_frame = requestAnimationFrame(() => {
                this.resize_frame = 0;
                if (this.current?.chart.isConnected) this.draw(this.current.chart, this.current.angles);
            });
        });
    };
}
const labSurfacePreview: MarsSurfacePreview = new MarsSurfacePreview({
    native: labDOM,
    get volume() { return labSurfaceVolume; },
    get exporter() { return labSurfaceExport; },
    get camera() { return labSurfaceCamera; }
});
