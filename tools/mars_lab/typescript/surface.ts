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

/** Snapshot producer used when preserving a populated worksheet result. */
interface MarsSurfaceCapture {
    /** @brief Capture the current worksheet result. */
    (): WorksheetResultSnapshot;
}

/** Snapshot producer which may have no current worksheet result. */
interface MarsSurfaceOptionalCapture {
    /** @brief Capture the current worksheet result when available. */
    (): WorksheetResultSnapshot | null | undefined;
}

/** @brief Browser drawing resources allocated before assembling and initialising a retained view. */
interface MarsSurfaceResources {
    host: HTMLElement;
    overlay: HTMLDivElement;
    holder: HTMLDivElement;
    canvas: HTMLCanvasElement;
    vertices: Float32Array;
    width: number;
    height: number;
    gpu: MarsSurfaceGPU | null;
    context: CanvasRenderingContext2D | null;
    ink: CanvasRenderingContext2D;
}

/** Owns the active view, overlay, observers, frame scheduling and snapshot lifecycle. */
class MarsSurfacePreview {
    private current: MarsSurfaceView | null = null;
    private resizeFrame = 0;
    private restorationFrame = 0;
    private closed = false;
    private hover: MarsSurfaceHover | undefined;
    private onHover(event: Event): void {
        if (this.closed) return;
        this.hover ??= new MarsSurfaceHover(this.hoverView.bind(this),
            this.host.native, this.events);
        this.hover.inspect(event);
    }

    private hoverView(chart: SVGSVGElement): MarsSurfaceView | null {
        return this.host.volume.interacting ? null : this.prepare(chart);
    }

    /**
     * @brief Register page-lifetime resize and page-hide callbacks for the retained preview.
     * View resources are allocated lazily; dispose() releases a view, not these listeners.
     */
    constructor(private readonly host: MarsSurfacePreviewHost, private readonly events: EventTarget = window) {
        this.onPageHide = this.onPageHide.bind(this);
        this.onResize = this.onResize.bind(this);
        this.onHover = this.onHover.bind(this);
        this.events.addEventListener('pagehide', this.onPageHide);
        this.events.addEventListener('resize', this.onResize);
        this.events.addEventListener('pointermove', this.onHover);
        // Replaced SVG paths can enter a stationary pointer without a pointermove.
        this.events.addEventListener('pointerover', this.onHover);
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
        view.settleTimer = setTimeout(this.settleView.bind(this, view), 150);
    }

    private settleView(view: MarsSurfaceView): void {
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
        // SVG insertion can change the screen transform without another pointer move.
        this.hover?.refresh(view.chart);
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
        // End this overlay cycle; a later draw captures the restored visibility again.
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
        // Identity combines the expression title with the exact domain bounds and coordinate units.
        const parts = [chart.closest('[data-plot-figure]')?.querySelector('title')?.textContent];
        for (const key of ['exactXMin', 'exactXMax', 'exactYMin', 'exactYMax', 'exactZMin', 'exactZMax', 'units']) {
            parts.push(chart.dataset[key]);
        }
        return JSON.stringify(parts);
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
    captureResult(capture: MarsSurfaceCapture): WorksheetResultSnapshot;

    /** @brief Capture an optional result while preserving the retained surface presentation. */
    captureResult(capture: MarsSurfaceOptionalCapture): WorksheetResultSnapshot | null | undefined;

    /** @brief Restore the same connected surface even when snapshot capture throws. */
    captureResult(capture: MarsSurfaceOptionalCapture): WorksheetResultSnapshot | null | undefined {
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
        cancelAnimationFrame(this.restorationFrame);
        this.restorationFrame = 0;
        if (this.closed) return;
        const chart = document.querySelector('#plotRendered [data-surface]');
        if (!marsTransportIsDOM(chart, 'SVGSVGElement')) return;
        // Mode controls and fitted card dimensions settle after DOM restoration.
        this.restorationFrame = requestAnimationFrame(this.restoreView.bind(this, chart, saved));
    }

    private restoreView(chart: SVGSVGElement, saved: MarsSurfaceSnapshot | null): void {
        this.restorationFrame = 0;
        if (this.closed || !chart.isConnected) return;
        const view = this.prepare(chart, saved);
        if (view) this.draw(chart, view.angles);
    }

    private syncZoom(view: MarsSurfaceView): void {
        this.host.native.call('lab_surface_zoom_sync', view);
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
        const mesh = this.host.native.call('lab_surface_mesh', chart) as MarsSurfaceMesh | null;
        if (!mesh) return null;
        const view = this.createView(chart, mesh, saved, software);
        if (!view) return null;
        this.restoreOrientation(view, saved);
        this.host.volume.initialise(view, saved, saved?.key === this.signature(chart));
        this.current = view;
        this.observeView(view);
        this.syncZoom(view);
        return view;
    }

    /** @brief Allocate browser resources and assemble the view before native volume initialisation. */
    private createView(chart: SVGSVGElement, mesh: MarsSurfaceMesh,
                       saved: MarsSurfaceSnapshot | null, software: boolean): MarsSurfaceInitialView | null {
        const resources = this.createDOM(chart, mesh, software);
        if (!resources) return null;
        const view = this.assembleView(chart, mesh, saved, resources);
        resources.canvas.addEventListener('webglcontextlost', this.contextLost.bind(this, chart, resources.overlay));
        return view;
    }

    /** @brief Create the overlay and drawing contexts; release GPU allocations if a required context is unavailable. */
    private createDOM(chart: SVGSVGElement, mesh: MarsSurfaceMesh, software: boolean): MarsSurfaceResources | null {
        const width = chart.viewBox.baseVal.width, height = chart.viewBox.baseVal.height;
        // Keep composited canvases out of SVG foreignObject: mobile engines can
        // position those layers outside the fitted SVG when they first appear.
        const host = chart.closest('.rendered');
        if (!marsTransportIsDOM(host, 'HTMLElement')) return null;
        const overlay = document.createElement('div');
        overlay.dataset.surfaceOverlay = '';
        overlay.style.cssText = `position:absolute;left:0;top:0;width:${width}px;height:${height}px;` +
            'transform-origin:0 0;pointer-events:none;display:none;overflow:hidden';
        const holder = document.createElement('div');
        holder.style.cssText = `position:relative;width:${width}px;height:${height}px;` +
            'transform-origin:0 0;pointer-events:none';
        let canvas = document.createElement('canvas');
        const vertices = new Float32Array(mesh.vertices);
        const gpu = software ? null : MarsSurfaceGPU.create(canvas, vertices, chart.dataset.segments === 'true');
        if (!gpu) canvas = document.createElement('canvas');
        const labels = document.createElement('canvas');
        for (const layer of [canvas, labels]) {
            // Allocate the backing stores from their displayed size on the first draw.
            layer.width = 1;
            layer.height = 1;
            layer.style.cssText = `position:absolute;left:0;top:0;width:${width}px;height:${height}px`;
            holder.appendChild(layer);
        }
        const context = gpu ? null : canvas.getContext('2d');
        const ink = labels.getContext('2d');
        if ((!gpu && !context) || !ink) { gpu?.dispose(); return null; }
        overlay.appendChild(holder);
        host.appendChild(overlay);
        return {host, overlay, holder, canvas, vertices, width, height, gpu, context, ink};
    }

    /** @brief Combine browser resources, native geometry and compatible camera state without starting observers. */
    private assembleView(chart: SVGSVGElement, mesh: MarsSurfaceMesh, saved: MarsSurfaceSnapshot | null,
                         resources: MarsSurfaceResources): MarsSurfaceInitialView {
        const {host, overlay, holder, vertices, width, height, gpu, context, ink} = resources;
        const names: string[] = [];
        for (let axis = 0; axis < 3; ++axis) {
            names.push(chart.querySelector(`[data-axis-name="${axis}"]`)?.textContent || '');
        }
        return {
            frame: 0, settled: null, orientation: [0, 0, 0, 1],
            chart, host, holder, primitives: mesh.primitives, vertices, units: mesh.units,
            width, height, overlay, gpu, context, ink, ticks: this.readTicks(chart), names,
            observer: new MutationObserver(this.chartMutated.bind(this, chart)), hidden: [],
            camera: saved && (saved.key === this.signature(chart) || saved.refining) ?
                {...saved.camera} : {scale: 1, x: 0, y: 0},
            angles: this.readAngles()
        };
    }

    private readTicks(chart: SVGSVGElement): MarsSurfaceTick[] {
        const ticks: MarsSurfaceTick[] = [];
        for (const node of chart.querySelectorAll('[data-tick-fraction]')) {
            if (!marsTransportIsDOM(node, 'SVGElement')) continue;
            ticks.push({axis: Number(node.dataset.plotTick), fraction: Number(node.dataset.tickFraction),
                label: node.nextElementSibling?.tagName === 'text' ? (node.nextElementSibling.textContent ?? '') : ''});
        }
        return ticks;
    }

    private readAngles(): MarsSurfaceAngles {
        const azimuth = document.getElementById('plotAzimuth') as HTMLInputElement | null;
        const elevation = document.getElementById('plotElevation') as HTMLInputElement | null;
        const roll = document.getElementById('plotRoll') as HTMLInputElement | null;
        return [Number(azimuth?.value ?? 0), Number(elevation?.value ?? 0), Number(roll?.value ?? 0)];
    }

    private anglesMatch(left: MarsSurfaceAngles, right: MarsSurfaceAngles, tolerance: number): boolean {
        for (let i = 0; i < left.length; ++i) {
            if (!(Math.abs(left[i] - right[i]) <= tolerance)) return false;
        }
        return true;
    }

    private restoreOrientation(view: MarsSurfaceInitialView, saved: MarsSurfaceSnapshot | null): void {
        const preserveOrientation = saved?.orientation && (saved.refining ||
            (saved.identity === this.identity(view.chart) && this.anglesMatch(view.angles, saved.angles, 0.51)));
        view.orientation = preserveOrientation ?
            [...saved.orientation] : this.host.camera.fromAngles(view.angles);
        if (preserveOrientation) view.angles = [...saved.angles];
    }

    private chartMutated(chart: SVGSVGElement): void {
        if (!chart.isConnected) this.dispose();
    }

    private contextLost(chart: SVGSVGElement, overlay: HTMLDivElement, event: Event): void {
        event.preventDefault();
        if (this.current?.overlay !== overlay) return;
        const savedView = this.snapshot();
        this.dispose();
        const fallback = this.prepare(chart, savedView, true);
        if (fallback) this.draw(chart, fallback.angles);
    }

    private observeView(view: MarsSurfaceView): void {
        // Activate observers only after native initialisation has supplied view.volume.
        const rendered = document.getElementById('plotRendered');
        if (rendered) view.observer.observe(rendered, {childList: true, subtree: true});
        if (window.ResizeObserver) {
            view.resizeObserver = new ResizeObserver(this.viewResized.bind(this, view));
            view.resizeObserver.observe(view.host);
        }
    }

    private viewResized(view: MarsSurfaceView): void {
        if (this.current !== view || view.frame) return;
        view.frame = requestAnimationFrame(this.redrawView.bind(this, view));
    }

    private redrawView(view: MarsSurfaceView): void {
        view.frame = 0;
        if (this.current === view && view.chart.isConnected) this.draw(view.chart, view.angles);
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

    private projectedPolygons(view: MarsSurfaceView, frame: MarsSurfaceFrame): MarsSurfaceDrawing[] {
        if (!view.drawing) {
            const segments = view.chart.dataset.segments === 'true';
            view.drawing = this.host.native.call('lab_surface_polygons', frame,
                                                view.primitives, Number(segments)) as MarsSurfaceDrawing[];
        }
        return view.drawing;
    }

    private positionOverlay(view: MarsSurfaceView): boolean {
        // Map the native chart's coordinate system into the HTML scroll content,
        // including nested SVG headings, mobile fitting, borders and scroll offsets.
        const screen = view.chart.getScreenCTM(), box = view.host.getBoundingClientRect();
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
        return true;
    }

    private projectPoint(frame: MarsSurfaceFrame, point: MarsSurfacePoint3): MarsSurfacePoint2 {
        return this.host.native.call('lab_surface_project', frame, point) as MarsSurfacePoint2;
    }

    /**
     * @brief Render a native frame and synchronise axes, overlays and zoom metadata.
     * Uses device-pixel backing stores and falls back to Canvas after GPU failure.
     * Returns false when the chart, angles or native frame cannot be used.
     */
    draw(chart: SVGSVGElement, angles: MarsSurfaceAngles, bounds: number[] | null = null): boolean {
        // 1. Prepare the view, validate angles and update orientation.
        this.hover?.invalidate(chart);
        const view = this.prepare(chart);
        if (!view) return false;
        for (const angle of angles) {
            if (!Number.isFinite(angle)) return false;
        }
        this.removeSettled(view);
        if (!this.anglesMatch(angles, view.angles, 1e-9)) {
            view.orientation = this.host.camera.fromAngles(angles);
        }
        view.angles = [...angles];
        // 2. Position the overlay and resize its backing stores.
        if (!this.positionOverlay(view)) return false;

        // 3. Obtain the native frame and install its projection callbacks.
        const frame = this.host.native.call('lab_surface_frame', view, bounds) as MarsSurfaceFrame | null;
        if (!frame) return false;
        view.framePlan = frame;
        view.project = this.projectPoint.bind(this, frame);
        view.drawing = undefined;
        view.polygons = this.projectedPolygons.bind(this, view, frame);

        // 4. Render with WebGL, or retry with Canvas if the GPU fails.
        if (view.gpu) {
            if (!view.gpu.draw(new Float32Array(frame.matrix), frame.scaling, frame.offset, frame.drawMesh)) {
                const savedView = this.snapshot();
                this.dispose();
                const fallback = this.prepare(chart, savedView, true);
                return fallback ? this.draw(chart, fallback.angles, bounds) : false;
            }
        } else if (!this.drawCanvas(view)) {
            return false;
        }
        // 5. Paint axes, reveal the overlay, synchronise zoom and schedule settling.
        view.paintAxes = this.paintAxes.bind(this, frame);
        view.paintAxes(view.ink);
        this.showView(view);
        this.syncZoom(view);
        this.scheduleSettled(view);
        this.hover?.refresh(chart);
        return true;
    }

    private drawCanvas(view: MarsSurfaceView): boolean {
        const ctx = view.context;
        if (!ctx || !view.polygons) return false;
        ctx.clearRect(0, 0, view.width, view.height);
        const triangles = view.polygons();
        ctx.strokeStyle = '#193d4659';
        ctx.lineWidth = 0.45;
        for (const triangle of triangles) {
            ctx.beginPath();
            for (let i = 0; i < triangle.points.length; ++i) {
                const point = triangle.points[i];
                if (i) ctx.lineTo(...point);
                else ctx.moveTo(...point);
            }
            if (view.chart.dataset.segments === 'true') {
                ctx.strokeStyle = triangle.colour;
                ctx.lineWidth = 2;
                ctx.stroke();
                continue;
            }
            ctx.closePath();
            ctx.fillStyle = triangle.colour;
            ctx.fill(); ctx.stroke();
        }
        return true;
    }

    private paintAxes(frame: MarsSurfaceFrame, ctx: MarsSurfaceAxisContext): void {
        ctx.clearRect(0, 0, frame.width, frame.height);
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
    }

    private showView(view: MarsSurfaceView): void {
        const chart = view.chart;
        // Save original g/path visibility once per overlay cycle; clear(true) restores it.
        // Native base geometry stays fixed until chart replacement. The separately owned
        // settled SVG layer is removed before drawing, so it never enters this list.
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
        chart.dataset.localCamera = view.angles.join(',');
        chart.dataset.cameraZoom = String(view.camera.scale);
        chart.dataset.volumeBounds = view.volume.bounds.join(',');
        chart.dataset.cameraQuaternion = view.orientation.join(',');
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
        const controls = ['plotAzimuth', 'plotElevation', 'plotRoll'];
        for (let i = 0; i < controls.length; ++i) {
            const id = controls[i];
            const control = document.getElementById(id) as HTMLInputElement | null, value = view.angles[i];
            if (control) control.value = String(value);
            const output = document.getElementById(id + 'Value');
            if (output) output.textContent = String(Math.round(value)) + '°';
        }
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
        if (!view.frame) view.frame = requestAnimationFrame(this.redrawView.bind(this, view));
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

    /** @brief Release page-lifetime subscriptions and all retained view resources. */
    public close(): void {
        this.closed = true;
        this.events.removeEventListener('pointermove', this.onHover);
        this.events.removeEventListener('pointerover', this.onHover);
        this.hover?.dispose();
        this.events.removeEventListener('pagehide', this.onPageHide);
        this.events.removeEventListener('resize', this.onResize);
        cancelAnimationFrame(this.resizeFrame);
        this.resizeFrame = 0;
        cancelAnimationFrame(this.restorationFrame);
        this.restorationFrame = 0;
        this.dispose();
    }

    private onPageHide(): void {
        this.dispose();
    }

    private onResize(): void {
        if (this.closed) return;
        cancelAnimationFrame(this.resizeFrame);
        this.resizeFrame = requestAnimationFrame(this.deferResize.bind(this));
    }

    private deferResize(): void {
        this.resizeFrame = requestAnimationFrame(this.finishResize.bind(this));
    }

    private finishResize(): void {
        this.resizeFrame = 0;
        if (this.current?.chart.isConnected) this.draw(this.current.chart, this.current.angles);
    }
}

/** @brief Shared retained renderer with lazily resolved camera, volume and export capabilities. */
const labSurfacePreview: MarsSurfacePreview = new MarsSurfacePreview({
    native: labDOM,
    get volume() { return labSurfaceVolume; },
    get exporter() { return labSurfaceExport; },
    get camera() { return labSurfaceCamera; }
});
