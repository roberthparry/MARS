/**
 * @file globe.ts
 * @brief Browser canvas, texture loading and input adapter for the native observer globe.
 *
 * A bundled NASA surface composite supplies natural colour, clipped to native SVG coastlines at deep zoom.
 * WASM owns picking, movement, history-flight interpolation and marker projection.
 * A dedicated WebAssembly worker projects native shoreline polygons into live SVG paths.
 * Gestures use a GPU preview; JavaScript transports results and discards stale camera frames.
 */
type GlobeView = [number, number, number, number, number, number, number];
interface GlobeTown { town: LabTown; jurisdiction: string; country: string; fullName?: string; }

class MarsAlmanacGlobe {
    /** @brief Copy a snapshot of the displayed camera, without requesting new map data. */
    static async copyImage(): Promise<void> {
        const globe = MarsAlmanacGlobe.instance;
        if (!globe || !navigator.clipboard?.write || typeof ClipboardItem === 'undefined')
            throw new Error('Image copying requires a browser with clipboard image support.');
        // Start the clipboard write during the click gesture; encoding may finish asynchronously.
        await navigator.clipboard.write([new ClipboardItem({'image/png': globe.imageBlob()})]);
    }

    /** @brief Composite the current GPU surface, vector overlays and visible labels into a PNG. */
    private async imageBlob(): Promise<Blob> {
        const box = this.canvas.getBoundingClientRect();
        if (!box.width || this.gl.isContextLost()) throw new Error('The globe is not ready to copy.');
        const output = document.createElement('canvas');
        output.width = this.canvas.width;
        output.height = this.canvas.height;
        const context = output.getContext('2d')!;
        context.scale(output.width / box.width, output.height / box.height);
        context.fillStyle = '#000';
        context.fillRect(0, 0, box.width, box.height);
        // WebGL's default framebuffer is disposable. Reissue only its existing draw,
        // with the current uniforms and textures: no camera update or shoreline work.
        this.gl.drawArrays(this.gl.TRIANGLES, 0, 6);
        const surface = document.createElement('canvas');
        surface.width = output.width;
        surface.height = output.height;
        surface.getContext('2d')!.drawImage(this.canvas, 0, 0);
        const overlays = [this.shoreOverlay, this.borderOverlay].map(node => {
            if (getComputedStyle(node).display === 'none') return null;
            const copy = node.cloneNode(true) as SVGSVGElement;
            copy.setAttribute('xmlns', 'http://www.w3.org/2000/svg');
            copy.setAttribute('width', String(box.width));
            copy.setAttribute('height', String(box.height));
            return new XMLSerializer().serializeToString(copy);
        });
        const labels = [...this.markers.children].map(node => {
            const rect = node.getBoundingClientRect();
            const style = getComputedStyle(node);
            return {text: node.textContent || '', x: rect.left - box.left, y: rect.top - box.top,
                width: rect.width, height: rect.height, visible: rect.width > 0 && style.visibility !== 'hidden',
                colour: style.color, background: style.backgroundColor, border: style.borderTopColor,
                borderWidth: parseFloat(style.borderTopWidth), radius: parseFloat(style.borderTopLeftRadius),
                font: style.font, padding: parseFloat(style.paddingLeft)};
        });
        async function paintSvg(source: string | null): Promise<void> {
            if (!source) return;
            const url = URL.createObjectURL(new Blob([source], {type: 'image/svg+xml'}));
            try {
                const image = new Image();
                image.src = url;
                await image.decode();
                context.drawImage(image, 0, 0, box.width, box.height);
            } finally { URL.revokeObjectURL(url); }
        }
        await paintSvg(overlays[0]);
        context.drawImage(surface, 0, 0, box.width, box.height);
        await paintSvg(overlays[1]);
        for (const label of labels) {
            if (!label.visible) continue;
            context.save();
            context.beginPath();
            context.roundRect(label.x, label.y, label.width, label.height, label.radius || 0);
            context.fillStyle = label.background;
            context.fill();
            if (label.borderWidth) {
                context.strokeStyle = label.border;
                context.lineWidth = label.borderWidth;
                context.stroke();
            }
            context.clip();
            context.font = label.font;
            context.textBaseline = 'middle';
            context.fillStyle = label.colour;
            context.fillText(label.text, label.x + label.padding, label.y + label.height / 2);
            context.restore();
        }
        return new Promise((resolve, reject) => output.toBlob(blob => {
            if (blob) resolve(blob);
            else reject(new Error('Could not encode the globe image.'));
        }, 'image/png'));
    }

    private static instance: MarsAlmanacGlobe | null = null;
    private static readonly cameraStorageKey = 'mars.exprLab.globeCamera.v1';
    private cameraSaveTimer = 0;
    private cameraRestored = false;
    private cameraBookmarkRestored = false;
    private observerStartupReady = false;
    private static townLabel(place: GlobeTown, zoom: number, available: number): {name: string; full: string; width: number} {
        const full = place.fullName || place.town.name;
        const expanded = full !== place.town.name && zoom >= 12 && available >= 430;
        return {name: expanded ? full : place.town.name, full, width: expanded ? 430 : 128};
    }

    private view: GlobeView = [0, 0, 1, -180, -90, 180, 90];
    private orientation = [0, 0, 0, 1];
    private rotation = [1, 0, 0, 0, 1, 0, 0, 0, 1];
    private readonly canvas: HTMLCanvasElement;
    private readonly markers: HTMLElement;
    private readonly borderOverlay = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    private readonly shoreOverlay = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    private readonly shoreGroup = document.createElementNS('http://www.w3.org/2000/svg', 'g');
    private shoreFrame: number[] | null = null;
    private dragPrediction: number[] | null = null;
    private dragSampleTime = 0;
    private shoreRevision = 0;
    private shoreBounds: number[] = [];
    private shoreFrameBounds: number[] = [];
    private shoreColourFrame: number[] | null = null;
    private shoreRequest: AbortController | null = null;
    private shoreZoom = 0;
    private shoreSignature = '';
    private surfacePending = false;
    private shoreWorker: Worker | null = null;
    private shoreNextJob = 0;
    private shoreGeometry = 0;
    private readonly shoreJobs = new Map<number, (value: unknown, error?: Error) => void>();
    private shoreDrawing = false;
    private shoreFailed = '';
    private shoreClosed = false;
    private movingUntil = 0;
    private settleTimer = 0;
    private mapPreviewReady = false;
    private mapPreviewRequest: Promise<void> | null = null;
    private readonly previewLifetime = new AbortController();
    private readonly borderPath = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    private borderSignature = '';
    private readonly status: HTMLElement;
    private readonly gridTooltip = document.createElement('div');
    private readonly countryFlagIcon = document.createElement('img');
    private hoverPointer: PointerEvent | null = null;
    private readonly gridMarker = document.createElement('span');
    private readonly gl: WebGLRenderingContext;
    private readonly program: WebGLProgram;
    private readonly quad: WebGLBuffer;
    private readonly uniforms = new Map<string, WebGLUniformLocation | null>();
    private maskRenderer: Promise<import('./globe_gpu.js').MarsGlobeMask> | null = null;
    private readonly textures: WebGLTexture[] = [];
    private detailBounds = [-180, -90, 180, 90];
    private detailReady = false;
    private grid = false;
    private map = false;
    private highResolutionRequested = false;
    private detailZoom = 0;
    private requestBounds: number[] | null = null;
    private readonly markerNodes = new Map<string | null, HTMLElement>();
    private markerSignature = '';
    private observerSignature = '';
    private clickedObserver: [number, number] | null = null;
    private countryKey = '';
    private country = '';
    private countryFlag = '';
    private countryRequest: AbortController | null = null;
    private readonly countryPending = new Map<string, {
        controller: AbortController;
        users: number;
        promise: Promise<{name: string; flag: string; code: string}>;
    }>();
    private hoverCountryKey = '';
    private hoverCountry = '';
    private hoverCountryFlag = '';
    private hoverCountryRequest: AbortController | null = null;
    private hoverCountryTimer = 0;
    private hoverShift = false;
    private readonly countryNames = new Intl.DisplayNames(['en-GB'], {type: 'region'});
    private readonly pointers = new Map<number, {x: number; y: number}>();
    private started = false;
    private dragged = false;
    private suppressClick = false;
    private start = {x: 0, y: 0};
    private timer = 0;
    private detailTimer = 0;
    private frame = 0;
    private generation = 0;
    private selection = 0;
    private selectionCountryRequest: AbortController | null = null;
    private request: AbortController | null = null;
    private riverRequest: AbortController | null = null;
    private riverGeneration = 0;
    private riverReady = false;
    private riverBounds = [-180, -90, 180, 90];
    private riverZoom = 0;
    private riverUnloadTimer = 0;
    private borders = false;
    private borderReady = false;
    private borderZoom = 0;
    private borderRequest: AbortController | null = null;
    private borderRequestKey = '';
    private flightFrame = 0;
    private flightGeneration = 0;
    private flightTarget: number[] | null = null;
    private historyRestoring = false;

    /** Capture only five camera numbers; imagery and catalogues remain outside worksheet history. */
    public static captureHistory(): number[] | undefined {
        const globe = this.instance;
        if (!globe) return undefined;
        if (globe.flightTarget) return globe.flightTarget.slice();
        globe.rememberHistory();
        const orientation = globe.sameCameraObserver(globe.observerSignature) || globe.retainsClickedView() ? globe.orientation :
            labDOM.call('lab_globe_centre', Number(globe.input('Latitude').value),
                Number(globe.input('Longitude').value)) as number[] | null;
        return orientation ? [...orientation, globe.view[2]] : undefined;
    }

    /** Suspend automatic centring while asynchronous history restoration resolves the destination. */
    public static beginHistory(): number | undefined {
        const globe = this.instance;
        if (!globe) return undefined;
        globe.cancelFlight();
        globe.historyRestoring = true;
        globe.clickedObserver = null;
        globe.stopRegionRequests();
        // A saved zoom may cross from photographic detail to a land mask or vice versa.
        globe.detailReady = false;
        globe.riverReady = false;
        return globe.flightGeneration;
    }

    /** Start only the still-owned restoration; stale completions cannot move the camera. */
    public static finishHistory(token: number | undefined, camera: unknown, accepted: boolean): void {
        const globe = this.instance;
        if (!globe || token !== globe.flightGeneration) return;
        globe.historyRestoring = false;
        if (!accepted) { globe.move(0, 0, 1); return; }
        const start = [...globe.orientation, globe.view[2]];
        const saved = labDOM.call('lab_globe_flight', start, camera, 1) as number[] | null;
        const target = saved || MarsAlmanacGlobe.captureHistory();
        if (!target) return;
        globe.observerSignature = globe.observerState();
        globe.flightTarget = target;
        globe.hideGridHover();
        const complete = (): void => {
            globe.applyCamera(target);
            globe.cancelFlight();
            globe.move(0, 0, 1);
        };
        if (!globe.started || document.hidden || globe.gl.isContextLost() ||
            matchMedia('(prefers-reduced-motion: reduce)').matches || start.every((value, i) => value === target[i])) {
            complete();
            return;
        }
        const began = performance.now();
        const tick = (now: number): void => {
            if (token !== globe.flightGeneration) return;
            if (WorksheetWorkspace.mode.currentMode() !== 'almanac') { globe.cancelFlight(); return; }
            globe.flightFrame = 0;
            const progress = (now - began) / 3600;
            if (progress >= 1 || document.hidden || matchMedia('(prefers-reduced-motion: reduce)').matches) {
                complete();
                return;
            }
            const next = labDOM.call('lab_globe_flight', start, target, progress) as number[];
            globe.applyCamera(next);
            globe.flightFrame = requestAnimationFrame(tick);
        };
        globe.flightFrame = requestAnimationFrame(tick);
    }

    private cancelFlight(): void {
        ++this.flightGeneration;
        cancelAnimationFrame(this.flightFrame);
        this.flightFrame = 0;
        this.flightTarget = null;
        this.historyRestoring = false;
    }

    private stopRegionRequests(): void {
        this.shoreRequest?.abort();
        this.shoreRequest = null;
        window.clearTimeout(this.timer);
        window.clearTimeout(this.detailTimer);
        ++this.generation;
        ++this.riverGeneration;
        this.request?.abort();
        this.request = null;
        this.requestBounds = null;
        this.riverRequest?.abort();
        this.riverRequest = null;
        this.borderRequest?.abort();
        this.borderRequest = null;
    }

    private applyCamera(camera: number[]): void {
        this.noteMotion();
        this.orientation = camera.slice(0, 4);
        this.view = labDOM.call('lab_globe_view', this.orientation, camera[4], 1) as GlobeView;
        this.rotation = (labDOM.call('lab_surface_camera', 1, this.orientation, null, null, null, 0) as number[][]).flat();
        this.draw();
        this.scheduleCameraSave();
    }

    constructor(private readonly root: HTMLElement) {
        this.canvas = root.querySelector('canvas')!;
        this.markers = root.querySelector('.almanac-globe-markers')!;
        this.status = root.querySelector('[role="status"]')!;
        this.shoreOverlay.classList.add('globe-shore-overlay');
        this.shoreOverlay.setAttribute('aria-hidden', 'true');
        this.shoreOverlay.append(this.shoreGroup);
        this.shoreOverlay.style.cssText = 'position:absolute;inset:0;width:100%;height:100%;pointer-events:none;overflow:hidden';
        this.canvas.style.position = 'relative';
        this.canvas.before(this.shoreOverlay);
        this.borderOverlay.classList.add('globe-border-overlay');
        this.borderOverlay.setAttribute('aria-hidden', 'true');
        this.borderOverlay.style.cssText = 'position:absolute;inset:0;width:100%;height:100%;pointer-events:none;overflow:hidden';
        this.borderPath.setAttribute('fill', 'none');
        this.borderPath.setAttribute('stroke', '#dcc7a0');
        this.borderPath.setAttribute('stroke-width', '1');
        this.borderPath.setAttribute('vector-effect', 'non-scaling-stroke');
        this.borderPath.setAttribute('stroke-linecap', 'round');
        this.borderOverlay.append(this.borderPath);
        this.canvas.after(this.borderOverlay);
        this.gridTooltip.className = 'globe-grid-tooltip';
        this.gridTooltip.setAttribute('role', 'tooltip');
        this.gridMarker.className = 'globe-grid-marker';
        this.gridMarker.setAttribute('aria-hidden', 'true');
        this.hideGridHover();
        this.canvas.parentElement!.append(this.gridTooltip, this.gridMarker);
        const gl = this.canvas.getContext('webgl', {alpha: true, antialias: true});
        if (!gl) throw new Error('The globe needs WebGL; the observer fields remain available.');
        this.gl = gl;
        this.program = this.createProgram();
        gl.useProgram(this.program);
        const buffer = gl.createBuffer();
        if (!buffer) throw new Error('Could not allocate globe quad.');
        this.quad = buffer;
        gl.bindBuffer(gl.ARRAY_BUFFER, this.quad);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]), gl.STATIC_DRAW);
        const position = gl.getAttribLocation(this.program, 'position');
        gl.enableVertexAttribArray(position);
        gl.vertexAttribPointer(position, 2, gl.FLOAT, false, 0, 0);
        for (let i = 0; i < 5; ++i) {
            const texture = gl.createTexture();
            if (!texture) throw new Error('Could not allocate globe imagery.');
            this.textures.push(texture);
            gl.activeTexture(gl.TEXTURE0 + i);
            gl.bindTexture(gl.TEXTURE_2D, texture);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array([7, 20, 64, 255]));
        }
        gl.uniform1i(this.uniform('world'), 0);
        gl.uniform1i(this.uniform('detail'), 1);
        gl.uniform1i(this.uniform('mapWorld'), 2);
        gl.uniform1i(this.uniform('rivers'), 3);
        gl.uniform1i(this.uniform('shoreMask'), 4);
        void window.labReady.then(async () => {
            this.restoreCamera();
            this.cameraRestored = true;
            if (this.started) this.move(0, 0, 1);
            await labApplication.townsReady;
            // Catalogue hydration is not a request to move the user's camera.
            if (this.observerSignature) this.observerSignature = this.observerState();
            this.observerStartupReady = true;
        });
        this.events();
        window.addEventListener('pagehide', event => {
            this.saveCamera();
            if (event.persisted) return;
            this.shoreClosed = true;
            void this.maskRenderer?.then(renderer => renderer.dispose()).catch(() => {});
            this.previewLifetime.abort();
            this.stopShoreWorker(new Error('Globe closed'));
            window.clearTimeout(this.settleTimer);
        });
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) this.saveCamera();
        });
        document.addEventListener('lab-town-catalogue-ready', () => {
            if (!this.started) return;
            this.markerSignature = '';
            this.draw();
        });
        new ResizeObserver(() => this.draw()).observe(this.canvas);
        const visible = new IntersectionObserver(entries => {
            if (!this.started && entries.some(entry => entry.isIntersecting)) {
                this.started = true;
                if (!this.cameraBookmarkRestored && !this.sameCameraObserver(this.observerSignature)) this.centre();
                else {
                    this.observerSignature = this.observerState();
                    this.move(0, 0, 1);
                }
                void this.loadWorld();
            }
        });
        visible.observe(root);
        MarsAlmanacGlobe.instance = this;
    }

    private createProgram(): WebGLProgram {
        const gl = this.gl;
        const program = gl.createProgram();
        if (!program) throw new Error('Could not create globe renderer.');
        const derivatives = Boolean(gl.getExtension('OES_standard_derivatives'));
        const sources: [number, string][] = [
            [gl.VERTEX_SHADER, labDOM.call('lab_globe_shader', 0, Number(derivatives)) as string],
            [gl.FRAGMENT_SHADER, labDOM.call('lab_globe_shader', 1, Number(derivatives)) as string]
        ];
        for (const [type, source] of sources) {
            const shader = gl.createShader(type)!;
            gl.shaderSource(shader, source);
            gl.compileShader(shader);
            if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error('Globe shaders are unavailable.');
            gl.attachShader(program, shader);
            gl.deleteShader(shader);
        }
        gl.bindAttribLocation(program, 0, 'position');
        gl.linkProgram(program);
        if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error('Could not link globe renderer.');
        return program;
    }

    private placeCountry(place: GlobeTown): GlobeTown {
        place.country = this.countryNames.of(place.jurisdiction.split('-')[0]) || place.jurisdiction;
        return place;
    }

    private selectedTown(): GlobeTown | undefined {
        const place = labDOM.call('lab_town_store_get', this.input('Jurisdiction').value,
            this.input('Town').value) as GlobeTown | null;
        return place ? this.placeCountry(place) : undefined;
    }

    private townKey(place: GlobeTown): string { return `${place.jurisdiction}:${place.town.value}`; }

    private input(name: string): HTMLInputElement { return document.getElementById(`almanac${name}`) as HTMLInputElement; }

    private usesMap(): boolean {
        return this.map || this.view[2] >= 8;
    }

    private resetDetail(): void {
        // Geometry is shared by both styles: crossing the close-view threshold
        // must not cancel the prefetch that was started before reaching it.
        if (!this.map && this.view[2] < 4) {
            this.shoreRequest?.abort();
            this.shoreRequest = null;
        }
        ++this.generation;
        this.request?.abort();
        this.requestBounds = null;
        this.detailReady = false;
        if (this.usesMap()) void this.loadMap();
    }

    private observerState(): string {
        return JSON.stringify(['Latitude', 'Longitude', 'Town', 'Jurisdiction'].map(
            field => (document.getElementById('almanac' + field) as HTMLInputElement | HTMLSelectElement).value));
    }

    private restoreCamera(): void {
        try {
            const saved = JSON.parse(localStorage.getItem(MarsAlmanacGlobe.cameraStorageKey) || 'null');
            if (!saved) return;
            // Native validation rejects malformed quaternions and out-of-range zooms.
            const camera = labDOM.call('lab_globe_flight', [0, 0, 0, 1, 1], saved.camera, 1) as number[] | null;
            if (!camera) return;
            this.orientation = camera.slice(0, 4);
            this.view = labDOM.call('lab_globe_view', this.orientation, camera[4], 1) as GlobeView;
            this.rotation = (labDOM.call('lab_surface_camera', 1, this.orientation, null, null, null, 0) as number[][]).flat();
            this.observerSignature = this.observerState();
            this.cameraBookmarkRestored = true;
        } catch { /* Missing, corrupt or blocked local storage leaves the default camera available. */ }
    }

    private sameCameraObserver(record: unknown): boolean {
        try {
            if (typeof record !== 'string') return false;
            const previous = JSON.parse(record), current = JSON.parse(this.observerState());
            // Town options hydrate independently of the geographic observer.
            return Array.isArray(previous) && [0, 1, 3].every(index => previous[index] === current[index]);
        } catch { return false; }
    }

    private saveCamera(): void {
        window.clearTimeout(this.cameraSaveTimer);
        if (!this.cameraRestored || !this.started || this.historyRestoring) return;
        try {
            localStorage.setItem(MarsAlmanacGlobe.cameraStorageKey, JSON.stringify({
                observer: this.observerState(),
                camera: [...this.orientation, this.view[2]]
            }));
        } catch { /* Storage restrictions must not interrupt globe interaction. */ }
    }

    private scheduleCameraSave(): void {
        window.clearTimeout(this.cameraSaveTimer);
        this.cameraSaveTimer = window.setTimeout(() => this.saveCamera(), 150);
    }

    private retainsClickedView(): boolean {
        return this.clickedObserver !== null &&
            Number(this.input('Latitude').value) === this.clickedObserver[0] &&
            Number(this.input('Longitude').value) === this.clickedObserver[1];
    }

    private rememberHistory(): void {
        if (!this.historyRestoring && !this.flightTarget && this.observerSignature)
            WorksheetPersistence.history.rememberGlobeCamera(
                this.observerSignature, [...this.orientation, this.view[2]]);
    }

    private centre(): void {
        if (this.historyRestoring) return;
        const lat = this.input('Latitude').value, lon = this.input('Longitude').value;
        if (!lat.trim() || !lon.trim() || !Number.isFinite(Number(lat)) || !Number.isFinite(Number(lon)) ||
            Math.abs(Number(lat)) > 90 || Math.abs(Number(lon)) > 180) {
            this.status.textContent = 'Enter an observer location to centre the globe.';
            return;
        }
        this.rememberHistory();
        this.orientation = labDOM.call('lab_globe_centre', Number(lat), Number(lon)) as number[];
        this.observerSignature = this.observerState();
        this.move(0, 0, 1);
    }

    private move(dx: number, dy: number, factor: number, roll = 0): void {
        this.cancelFlight();
        this.noteMotion();
        this.hideGridHover();
        window.clearTimeout(this.detailTimer);
        ++this.riverGeneration;
        this.riverRequest?.abort();
        this.riverRequest = null;
        this.borderRequest?.abort();
        this.borderRequest = null;
        const wasMap = this.usesMap();
        if (dx || dy || roll)
            this.orientation = labDOM.call('lab_globe_rotate', this.orientation, this.view[2], dx, dy, roll) as number[];
        if (dx || dy) {
            const now = performance.now();
            const ahead = Math.min(12, 400 / Math.max(16, now - this.dragSampleTime));
            this.dragSampleTime = now;
            const leadX = Math.max(-.25, Math.min(.25, dx * ahead));
            const leadY = Math.max(-.25, Math.min(.25, dy * ahead));
            this.dragPrediction = labDOM.call('lab_globe_rotate', this.orientation, this.view[2], leadX, leadY, 0) as number[];
        } else this.dragPrediction = null;
        this.rotation = (labDOM.call('lab_surface_camera', 1, this.orientation, null, null, null, 0) as number[][]).flat();
        this.view = labDOM.call('lab_globe_view', this.orientation, this.view[2], factor) as GlobeView;
        this.scheduleCameraSave();
        this.scheduleRiverUnload();
        if (!this.borders) this.unloadBorders();
        if (wasMap !== this.usesMap()) this.resetDetail();
        this.draw();
        window.clearTimeout(this.timer);
        this.timer = window.setTimeout(() => {
            if (this.usesMap() || this.view[2] > 1.5) void this.loadDetail();
            void this.loadRivers();
            void this.loadBorders();
            if (this.view[2] >= 3) void labApplication.loadNearbyTowns(this.view[0], this.view[1]);
        }, 300);
    }

    private async image(bounds: number[], signal?: AbortSignal, map = this.usesMap(),
                        route = '/almanac-globe'): Promise<HTMLImageElement> {
        const [west, south, east, north] = bounds;
        const response = await labHttp.fetch(route, {
            method: 'POST', body: labWire.encode({west, south, east, north, view: map ? 'map' : 'natural'}), signal
        });
        const result = await response.labData() as {ok: boolean; image?: string; error?: string};
        if (!response.ok || !result.ok || !result.image) throw new Error(result.error || 'Shoreline imagery unavailable.');
        const url = URL.createObjectURL(new Blob([result.image], {type: 'image/svg+xml'}));
        try {
            const image = new Image();
            image.src = url;
            await image.decode();
            return image;
        } finally { URL.revokeObjectURL(url); }
    }

    private upload(index: number, image: HTMLImageElement): void {
        const gl = this.gl;
        gl.activeTexture(gl.TEXTURE0 + index);
        gl.bindTexture(gl.TEXTURE_2D, this.textures[index]);
        const filter = index === 2 || (index === 1 && this.usesMap()) ? gl.NEAREST : gl.LINEAR;
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, filter);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, filter);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, image);
        this.draw();
    }

    private async loadWorld(): Promise<void> {
        this.status.textContent = 'Loading Earth imagery…';
        try {
            const image = new Image();
            image.src = '/earth_surface.jpg';
            await image.decode();
            this.upload(0, image);
            this.root.dataset.surface = 'blue-marble';
            this.root.dataset.resolution = '2048';
            this.status.textContent = 'Drag to rotate · drag vertically at either side to roll · scroll or pinch to zoom · select a town or point';
            void this.loadHighResolution();
        } catch (error) { this.status.textContent = String(error); }
    }

    private async loadHighResolution(): Promise<void> {
        if (this.highResolutionRequested || this.map || this.view[2] < 2 || !this.root.dataset.surface) return;
        const gl = this.gl, limit = gl.getParameter(gl.MAX_TEXTURE_SIZE) as number;
        let width = 8192;
        while (width > limit) width /= 2;
        if (width <= 2048) return;
        this.highResolutionRequested = true;
        try {
            const image = new Image();
            image.src = '/earth_surface_detail.png';
            await image.decode();
            if (image.naturalWidth !== 8192 || image.naturalHeight !== 4096) throw new Error('Invalid Earth detail image');
            while (width > 2048 && !gl.isContextLost()) {
                let bitmap: ImageBitmap | null = null;
                const texture = gl.createTexture();
                if (!texture) break;
                try {
                    if (width !== 8192) bitmap = await createImageBitmap(image, {resizeWidth: width, resizeHeight: width / 2});
                    gl.activeTexture(gl.TEXTURE0);
                    gl.bindTexture(gl.TEXTURE_2D, texture);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
                    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, bitmap || image);
                    if (gl.getError() === gl.NO_ERROR) {
                        gl.deleteTexture(this.textures[0]);
                        this.textures[0] = texture;
                        this.root.dataset.resolution = String(width);
                        this.draw();
                        return;
                    }
                } catch { /* Keep the overview and retry at the next smaller supported size. */ }
                finally { bitmap?.close(); }
                gl.deleteTexture(texture);
                gl.activeTexture(gl.TEXTURE0);
                gl.bindTexture(gl.TEXTURE_2D, this.textures[0]);
                width /= 2;
            }
            throw new Error('Higher-resolution imagery exceeds graphics capacity; retaining overview.');
        } catch (error) {
            if (!this.map) this.status.textContent = `Earth detail unavailable; overview retained. ${String(error)}`;
        }
    }

    private async loadMap(): Promise<void> {
        await this.loadShore();
        if (this.map && !this.shoreRequest) void this.loadMapPreview();
    }

    private async loadMapPreview(): Promise<void> {
        if (this.shoreClosed || this.mapPreviewReady || this.mapPreviewRequest) return;
        this.mapPreviewRequest = (async () => {
            try {
                const image = await this.image([-180, -90, 180, 90], this.previewLifetime.signal, true);
                if (this.shoreClosed) return;
                this.upload(2, image);
                this.mapPreviewReady = true;
                this.draw();
            } catch { /* Detailed SVG remains available if the optional movement preview fails. */ }
            finally { this.mapPreviewRequest = null; }
        })();
        await this.mapPreviewRequest;
    }

    private stopShoreWorker(error: Error): void {
        this.shoreWorker?.terminate();
        this.shoreWorker = null;
        this.shoreGeometry = 0;
        for (const finish of this.shoreJobs.values()) finish(null, error);
        labDOM.call('lab_globe_shore_clear');
    }

    private shoreJob(entry: GlobeWorkerRequest['entry'], args: GlobeWorkerRequest['args'],
                     signal?: AbortSignal): Promise<unknown> {
        return new Promise<unknown>((resolve, reject) => {
            if (this.shoreClosed) { reject(new Error('Globe closed')); return; }
            if (signal?.aborted) { reject(new DOMException('Aborted', 'AbortError')); return; }
            if (!this.shoreWorker) {
                this.shoreWorker = new Worker('/js/globe_worker.js');
                this.shoreWorker.onerror = event => this.stopShoreWorker(new Error(event.message));
                this.shoreWorker.onmessage = event => {
                    const {id, value, error} = event.data;
                    this.shoreJobs.get(id)?.(value, error ? new Error(error) : undefined);
                };
            }
            const worker = this.shoreWorker;
            const id = ++this.shoreNextJob;
            const timer = window.setTimeout(() => this.stopShoreWorker(new Error('Shoreline worker timed out')), 30000);
            const finish = (value: unknown, error?: Error): void => {
                window.clearTimeout(timer);
                signal?.removeEventListener('abort', abort);
                this.shoreJobs.delete(id);
                if (error) reject(error);
                else resolve(value);
            };
            const abort = (): void => {
                worker.postMessage({id, entry: 'cancel', args: []});
                // Completion may already be queued in the worker. Require fresh coverage before reusing
                // geometry whose acknowledgement the aborted caller will no longer accept.
                if (entry === 'fetch') labDOM.call('lab_globe_shore_clear');
                finish(null, new DOMException('Aborted', 'AbortError'));
            };
            this.shoreJobs.set(id, finish);
            signal?.addEventListener('abort', abort, {once: true});
            try { worker.postMessage({id, entry, args, geometry: this.shoreGeometry}); }
            catch (error) { finish(null, error instanceof Error ? error : new Error(String(error))); }
        });
    }

    private async loadDetail(): Promise<void> {
        if (!this.root.getClientRects().length || this.gl.isContextLost()) return;
        void this.loadHighResolution();
        if (this.usesMap()) {
            await this.loadShore();
            if (this.map && !this.shoreRequest) void this.loadMapPreview();
        }
        else if (this.view[2] >= 4) await this.loadShore();
    }

    private noteMotion(): void {
        this.movingUntil = performance.now() + 150;
        window.clearTimeout(this.settleTimer);
        this.settleTimer = window.setTimeout(() => this.draw(), 160);
    }

    private scheduleRiverUnload(): void {
        if (this.view[2] >= 12) {
            window.clearTimeout(this.riverUnloadTimer);
            this.riverUnloadTimer = 0;
        } else if (this.riverReady && !this.riverUnloadTimer) {
            this.riverUnloadTimer = window.setTimeout(() => {
                this.riverUnloadTimer = 0;
                if (this.view[2] >= 12) return;
                const gl = this.gl;
                // Keep a complete sampler, but replace the regional allocation with one transparent pixel.
                gl.activeTexture(gl.TEXTURE3);
                gl.bindTexture(gl.TEXTURE_2D, this.textures[3]);
                gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                              new Uint8Array([0, 0, 0, 0]));
                this.riverReady = false;
                this.riverZoom = 0;
                this.root.dataset.rivers = 'unloaded';
                this.draw();
            }, 2000);
        }
    }

    private unloadBorders(): void {
        this.borderRequest?.abort();
        this.borderRequest = null;
        this.borderPath.setAttribute('d', '');
        this.borderSignature = '';
        this.root.dataset.borders = 'off';
    }

    private async loadBorders(): Promise<void> {
        if (!this.borders || !this.root.getClientRects().length || this.gl.isContextLost()) return;
        const zoom = this.view[2];
        const bounds = zoom <= 2 ? [-180, -90, 180, 90] :
            (labDOM.call('lab_globe_view', this.orientation, zoom / 1.15, 1) as GlobeView).slice(3);
        // Replace broad coverage with a local selection at close zoom, rather than repeatedly projecting a world cache.
        if (zoom <= this.borderZoom * 1.5 && labDOM.call('lab_globe_borders_covers', this.view)) {
            this.root.dataset.borders = 'ready';
            this.draw();
            return;
        }
        const requestKey = bounds.join(',');
        if (this.borderRequest && !this.borderRequest.signal.aborted && this.borderRequestKey === requestKey) return;
        this.borderRequest?.abort();
        const request = this.borderRequest = new AbortController();
        this.borderRequestKey = requestKey;
        const timeout = window.setTimeout(() => request.abort(), 15000);
        this.root.dataset.borders = 'loading';
        try {
            const [west, south, east, north] = bounds;
            const response = await labHttp.fetch('/almanac-borders', {
                method: 'POST', signal: request.signal,
                body: labWire.encode({west, south, east, north, format: 'segments'})
            });
            const result = await response.labData() as {ok?: boolean; segments?: string};
            if (request !== this.borderRequest || request.signal.aborted || !this.borders) return;
            if (!response.ok || !result.ok || typeof result.segments !== 'string' ||
                !labDOM.call('lab_globe_borders_load_region', result.segments, bounds)) throw new Error('Border vectors unavailable');
            this.borderReady = true;
            this.borderZoom = zoom;
            this.borderSignature = '';
            this.draw();
            this.root.dataset.borders = 'ready';
        } catch {
            if (request === this.borderRequest) this.root.dataset.borders = 'unavailable';
        } finally {
            window.clearTimeout(timeout);
            if (request === this.borderRequest) this.borderRequest = null;
        }
    }

    private async loadRivers(): Promise<void> {
        if (this.view[2] < 12 || !this.root.getClientRects().length || this.gl.isContextLost()) return;
        const bounds = (labDOM.call('lab_globe_view', this.orientation, this.view[2] / 1.15, 1) as GlobeView).slice(3);
        if (bounds[2] - bounds[0] > 30 || bounds[3] - bounds[1] > 30) return;
        if (this.riverReady && this.view[3] >= this.riverBounds[0] && this.view[4] >= this.riverBounds[1] &&
            this.view[5] <= this.riverBounds[2] && this.view[6] <= this.riverBounds[3] &&
            this.view[2] <= this.riverZoom * 1.5) return;
        const generation = ++this.riverGeneration, zoom = this.view[2];
        this.riverRequest?.abort();
        const request = this.riverRequest = new AbortController();
        const timeout = window.setTimeout(() => request.abort(), 15000);
        try {
            const image = await this.image(bounds, request.signal, false, '/almanac-rivers');
            if (generation !== this.riverGeneration || request.signal.aborted) return;
            this.riverBounds = bounds;
            this.riverZoom = zoom;
            this.riverReady = true;
            this.upload(3, image);
            this.root.dataset.rivers = 'ready';
        } catch {
            // An unavailable optional layer must not delay or disable observer selection.
            if (generation === this.riverGeneration) this.root.dataset.rivers = 'unavailable';
        } finally {
            window.clearTimeout(timeout);
            if (generation === this.riverGeneration) this.riverRequest = null;
        }
    }

    private async loadShore(): Promise<void> {
        if (this.shoreRequest || (!this.map && this.view[2] < 4)) return;
        const reserve = labDOM.call('lab_globe_view', this.orientation, this.view[2] / 1.5, 1) as GlobeView;
        const prediction = performance.now() - this.dragSampleTime < 200 ? this.dragPrediction : null;
        const ahead = prediction ? labDOM.call('lab_globe_view', prediction, this.view[2] / 1.5, 1) as GlobeView : reserve;
        if (labDOM.call('lab_globe_shore_covers', reserve) && labDOM.call('lab_globe_shore_covers', ahead) &&
            this.view[2] <= this.shoreZoom * 1.5) return;
        const zoom = this.view[2];
        let bounds = (labDOM.call('lab_globe_view', prediction || this.orientation, zoom / 2, 1) as GlobeView).slice(3);
        if (!labDOM.call('lab_globe_shore_frame_covers', this.view, bounds))
            bounds = (labDOM.call('lab_globe_view', this.orientation, zoom / 2, 1) as GlobeView).slice(3);
        const request = this.shoreRequest = new AbortController();
        let loaded = false;
        this.root.dataset.shoreline = 'loading';
        this.status.textContent = 'Updating vector shoreline; keeping the previous view until it is ready.';
        try {
            const geometry = await this.shoreJob('fetch', bounds, request.signal);
            if (request.signal.aborted) return;
            if (typeof geometry !== 'number' || !geometry) throw new Error('Shoreline vectors unavailable');
            this.shoreGeometry = geometry;
            // Retain only the native coverage metadata here; vertex parsing belongs to the worker.
            labDOM.call('lab_globe_shore_load', '', bounds);
            this.shoreBounds = bounds;
            ++this.shoreRevision;
            this.shoreZoom = zoom;
            loaded = true;
            this.root.dataset.shoreline = 'svg';
            this.status.textContent = 'Drag to rotate · scroll or pinch to zoom · select a town or point';
            this.shoreSignature = '';
            this.shoreFailed = '';
            this.draw();
        } catch (error) {
            if (!request.signal.aborted) this.root.dataset.shoreline = 'unavailable';
            if (!request.signal.aborted) this.status.textContent = `Vector shoreline pending: ${String(error)}`;
        } finally {
            if (this.shoreRequest === request) this.shoreRequest = null;
            if (loaded && this.usesMap() && !labDOM.call('lab_globe_shore_covers', this.view)) void this.loadShore();
        }
    }

    private drawShore(size: number): boolean {
        const visible = this.usesMap() && Boolean(labDOM.call('lab_globe_shore_covers', this.view));
        this.shoreOverlay.style.display = 'none';
        if (!this.usesMap()) return false;
        const frame = [...this.orientation, this.view[2], size];
        let retained = false;
        if ((!this.map || Boolean(labDOM.call('lab_globe_shore_frame_covers', this.view, this.shoreFrameBounds))) &&
            this.shoreFrame && this.shoreFrame[4] >= 8 && this.view[2] >= 8) {
            const transform = labDOM.call('lab_globe_frame_transform', this.shoreFrame, frame) as number[] | null;
            if (transform) {
                this.shoreOverlay.setAttribute('viewBox', `0 0 ${size} ${size}`);
                this.shoreGroup.setAttribute('transform', `matrix(${transform.join(' ')})`);
                this.shoreOverlay.style.display = '';
                retained = true;
            }
        }
        // One bounded request follows the camera even throughout a continuous drag.
        void this.loadShore();
        if (!visible) return retained;
        // Pointer motion only transforms existing paths. Rebuilding thousands of SVG
        // commands on each worker completion competes with the next input frame.
        if (retained && performance.now() < this.movingUntil) return true;
        const signature = `${size},${this.view[2]},${this.orientation.join(',')}`;
        if (this.shoreClosed || signature === this.shoreFailed) return retained;
        if (signature !== this.shoreSignature) {
            if (!this.shoreDrawing) {
                this.shoreDrawing = true;
                const revision = this.shoreRevision;
                const frameBounds = this.shoreBounds;
                void this.shoreJob('lab_globe_shore_frame', [[...this.orientation], this.view[2], size]).then(async value => {
                    const current = `${this.canvas.clientWidth},${this.view[2]},${this.orientation.join(',')}`;
                    if (value === null) return; // A newer geometry revision won the race with this projection.
                    const result = value as import('./globe_gpu.js').GlobeMaskFrame;
                    if (!result || typeof result.svg !== 'string') throw new Error('Shoreline projection unavailable');
                    if (revision !== this.shoreRevision) return;
                    // Close-view results remain useful: the native affine preview advances them
                    // to the latest camera until the worker's next exact projection arrives.
                    if (current !== signature && (frame[4] < 8 || this.view[2] < 8)) return;
                    this.maskRenderer ??= import('./globe_gpu.js').then(module => new module.MarsGlobeMask(this.gl,
                        labDOM.call('lab_globe_mask_shader', 0) as string, labDOM.call('lab_globe_mask_shader', 1) as string));
                    const renderer = await this.maskRenderer;
                    if (this.shoreClosed || revision !== this.shoreRevision) return;
                    if ((frame[4] < 8 || this.view[2] < 8) &&
                        signature !== `${this.canvas.clientWidth},${this.view[2]},${this.orientation.join(',')}`) return;
                    const pixels = Math.min(2048, Math.round(size * 2 * Math.min(devicePixelRatio, 1.5)));
                    const started = performance.now(), gl = this.gl;
                    try { this.textures[4] = renderer.render(result, pixels, this.quad, this.textures[4]); }
                    finally {
                        gl.useProgram(this.program);
                        gl.bindBuffer(gl.ARRAY_BUFFER, this.quad);
                        gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
                        gl.activeTexture(gl.TEXTURE4); gl.bindTexture(gl.TEXTURE_2D, this.textures[4]);
                        gl.viewport(0, 0, this.canvas.width, this.canvas.height);
                    }
                    this.root.dataset.maskSubmitMs = (performance.now() - started).toFixed(2);
                    this.root.dataset.shoreProjectMs = String(result.projectMs);
                    this.shoreColourFrame = frame;
                    this.shoreOverlay.setAttribute('viewBox', `0 0 ${size} ${size}`);
                    this.shoreGroup.innerHTML = result.svg;
                    this.shoreFrame = frame;
                    this.shoreFrameBounds = frameBounds;
                    this.shoreGroup.removeAttribute('transform');
                    this.shoreSignature = signature;
                }).catch(error => {
                    this.shoreFailed = signature;
                    this.status.textContent = String(error);
                }).finally(() => {
                    this.shoreDrawing = false;
                    if (!this.shoreClosed) this.draw();
                });
            }
            return retained;
        }
        this.shoreGroup.removeAttribute('transform');
        this.shoreOverlay.style.display = '';
        return true;
    }

    private uniform(name: string): WebGLUniformLocation | null {
        if (!this.uniforms.has(name)) this.uniforms.set(name, this.gl.getUniformLocation(this.program, name));
        return this.uniforms.get(name)!;
    }

    private draw(): void {
        if (this.frame) return;
        this.frame = requestAnimationFrame(() => {
            this.frame = 0;
            const size = this.canvas.clientWidth;
            if (!size || !this.started || !this.cameraRestored || this.gl.isContextLost()) return;
            const mapReady = this.drawShore(size);
            this.surfacePending = this.usesMap() && !mapReady;
            this.root.dataset.surfacePending = String(this.surfacePending);
            const pixels = Math.min(1536, Math.round(size * Math.min(devicePixelRatio, 1.5)));
            if (this.canvas.width !== pixels || this.canvas.height !== pixels)
                this.canvas.width = this.canvas.height = pixels;
            const gl = this.gl;
            const colourTransform = mapReady && !this.map && this.shoreColourFrame === this.shoreFrame
                ? labDOM.call('lab_globe_frame_transform', this.shoreFrame, [...this.orientation, this.view[2], size]) as number[] | null
                : null;
            gl.uniform1i(this.uniform('naturalShore'), Number(Boolean(colourTransform)));
            gl.uniform1i(this.uniform('naturalStyle'), Number(!this.map));
            gl.uniform1f(this.uniform('photoPixels'), Number(this.root.dataset.resolution) || 0);
            if (colourTransform) {
                gl.uniform4fv(this.uniform('shoreBounds'), this.shoreFrameBounds);
                gl.uniform4fv(this.uniform('shoreTransform'), colourTransform.slice(0, 4));
                gl.uniform3f(this.uniform('shoreOffset'),
                             colourTransform[4], colourTransform[5], this.shoreColourFrame![5]);
            }
            gl.viewport(0, 0, pixels, pixels);
            gl.uniform3f(this.uniform('camera'), this.view[0], this.view[1], this.view[2]);
            // Row-major world-to-camera values are the column-major inverse rotation required by GLSL.
            gl.uniformMatrix3fv(this.uniform('cameraToWorld'), false, this.rotation);
            gl.uniform4fv(this.uniform('bounds'), this.detailBounds);
            // A regional mask must not be composited as photographic detail while the map base is pending.
            gl.uniform1i(this.uniform('vectorReady'), Number(mapReady));
            gl.uniform1i(this.uniform('detailReady'),
                         Number(this.detailReady && (!this.usesMap() || mapReady)));
            gl.uniform4fv(this.uniform('riverBounds'), this.riverBounds);
            gl.uniform1i(this.uniform('riverReady'), Number(this.riverReady));
            gl.uniform1i(this.uniform('grid'), Number(this.grid));
            gl.uniform1i(this.uniform('mapMode'),
                         Number(mapReady || (this.usesMap() && this.mapPreviewReady)));
            gl.uniform1i(this.uniform('interactionPreview'),
                         Number(!mapReady && this.usesMap() && this.mapPreviewReady));
            gl.uniform1f(this.uniform('viewport'), size);
            gl.drawArrays(gl.TRIANGLES, 0, 6);
            this.drawBorders(size);
            this.drawMarkers(size);
        });
    }

    private drawBorders(size: number): void {
        if (!this.borders || !this.borderReady) return;
        const signature = `${size},${this.view[2]},${this.orientation.join(',')}`;
        if (signature === this.borderSignature) return;
        this.borderSignature = signature;
        const path = labDOM.call('lab_globe_borders_path', this.orientation, this.view[2], size) as string | null;
        this.borderOverlay.setAttribute('viewBox', `0 0 ${size} ${size}`);
        this.borderPath.setAttribute('opacity', '0.7');
        this.borderPath.setAttribute('d', path || '');
        if (path === null) this.root.dataset.borders = 'unavailable';
    }

    private project(lat: number, lon: number, size: number): [number, number] | null {
        const point = labDOM.call('lab_globe_project', lat, lon, this.orientation);
        return labDOM.call('lab_globe_marker_pixels', point, this.view[2], size) as [number, number] | null;
    }

    private cachedCountry(key: string): {name: string; flag: string; code: string} | undefined {
        const place = labDOM.call('lab_country_cache_get', key) as {name: string; code: string} | null;
        if (!place) return undefined;
        const metadata = place.code ? WorksheetResults.tooltip.cachedCountry(place.code) : {flag: ''};
        return metadata ? {...place, flag: metadata.flag} : undefined;
    }

    private async lookupCountry(lat: number, lon: number, signal: AbortSignal, countryCode = ''): Promise<{name: string; flag: string; code: string}> {
        if (signal.aborted) throw new DOMException('Cancelled', 'AbortError');
        const key = countryCode ? `town:${countryCode}` : `${lat},${lon}`, cached = this.cachedCountry(key);
        if (cached) return cached;
        let pending = this.countryPending.get(key);
        if (!pending) {
            const controller = new AbortController();
            const promise = (async () => {
                const response = await labHttp.fetch('/almanac-country', {
                    method: 'POST', body: labWire.encode(countryCode ? {country_code: countryCode} : {lat, lon}),
                    signal: controller.signal
                });
                const result = await response.labData() as {
                    ok?: boolean; country?: string; country_flag?: string; country_code?: string
                };
                if (!response.ok || !result.ok || typeof result.country !== 'string' || controller.signal.aborted)
                    throw new Error('Country unavailable');
                const flag = typeof result.country_flag === 'string' &&
                    result.country_flag.startsWith('data:image/svg+xml;base64,') ? result.country_flag : '';
                const code = typeof result.country_code === 'string' && /^[A-Z]{2}(?:-[A-Z0-9]{1,3})?$/.test(result.country_code)
                    ? result.country_code : '';
                const value = {name: result.country, flag, code};
                WorksheetResults.tooltip.rememberCountry(code, value.name, flag);
                labDOM.call('lab_country_cache_put', key, value.name, code);
                return value;
            })();
            pending = {controller, users: 0, promise};
            this.countryPending.set(key, pending);
        }
        const shared = pending;
        ++shared.users;
        return new Promise((resolve, reject) => {
            let released = false;
            const release = () => {
                if (released) return false;
                released = true;
                signal.removeEventListener('abort', cancel);
                if (--shared.users === 0) {
                    shared.controller.abort();
                    if (this.countryPending.get(key) === shared) this.countryPending.delete(key);
                }
                return true;
            };
            const cancel = () => { if (release()) reject(new DOMException('Cancelled', 'AbortError')); };
            signal.addEventListener('abort', cancel, {once: true});
            shared.promise.then(value => { if (release()) resolve(value); }, error => { if (release()) reject(error); });
        });
    }

    private observerTown(): GlobeTown | undefined {
        const place = this.selectedTown();
        return place && Number(place.town.latitude) === Number(this.input('Latitude').value) &&
            Number(place.town.longitude) === Number(this.input('Longitude').value) ? place : undefined;
    }

    private observerCountryKey(): string {
        const place = this.observerTown();
        return place ? `town:${place.jurisdiction}` :
            `${Number(this.input('Latitude').value)},${Number(this.input('Longitude').value)}`;
    }

    private loadCountry(hoverPoint?: number[]): void {
        const lat = hoverPoint ? hoverPoint[0] : Number(this.input('Latitude').value);
        const lon = hoverPoint ? hoverPoint[1] : Number(this.input('Longitude').value);
        const town = hoverPoint ? undefined : this.observerTown();
        const key = hoverPoint ? `${lat},${lon}` : this.observerCountryKey();
        if (!hoverPoint && key === this.countryKey) return;
        if (!hoverPoint) {
            this.countryKey = key;
            this.countryRequest?.abort();
        }
        const cached = this.cachedCountry(key);
        if (hoverPoint) {
            this.hoverCountry = cached?.name || '';
            this.hoverCountryFlag = cached?.flag || '';
        } else {
            this.country = cached?.name || town?.country || '';
            this.countryFlag = cached?.flag || '';
        }
        if (cached || !Number.isFinite(lat) || !Number.isFinite(lon) ||
            Math.abs(lat) > 90 || Math.abs(lon) > 180) return;
        const request = new AbortController();
        if (hoverPoint) this.hoverCountryRequest = request;
        else this.countryRequest = request;
        const timeout = window.setTimeout(() => request.abort(), 5000);
        void (async () => {
            try {
                const result = await this.lookupCountry(lat, lon, request.signal, town?.jurisdiction);
                if ((hoverPoint ? this.hoverCountryKey : this.countryKey) !== key || request.signal.aborted) return;
                if (hoverPoint) {
                    this.hoverCountry = result.name;
                    this.hoverCountryFlag = result.flag;
                } else {
                    this.country = result.name;
                    this.countryFlag = result.flag;
                    this.markerSignature = '';
                    this.draw();
                }
                if (this.hoverPointer) this.hoverGrid(this.hoverPointer, this.hoverShift);
            } catch {
                // An optional country label must never delay selection or replace a coordinate tooltip.
            } finally {
                window.clearTimeout(timeout);
                if (this.countryRequest === request) this.countryRequest = null;
                if (this.hoverCountryRequest === request) this.hoverCountryRequest = null;
            }
        })();
    }

    private drawMarkers(size: number): void {
        this.loadCountry();
        const townId = JSON.stringify([this.input('Jurisdiction').value, this.input('Town').value]);
        const selected = this.selectedTown();
        const showTowns = this.view[2] >= 3 && !this.flightTarget;
        // Arrival can change label visibility without changing the final camera frame.
        const signature = `${this.orientation},${this.view[2]},${size},${this.input('Latitude').value},${this.input('Longitude').value},${townId},${showTowns}`;
        if (signature === this.markerSignature) return;
        this.markerSignature = signature;
        const visible = new Set<string | null>();
        labDOM.call('lab_globe_markers_begin', size);
        const append = (lat: number, lon: number, place?: GlobeTown): void => {
            if (place && visible.has(this.townKey(place))) return;
            let point = this.project(lat, lon, size);
            if (!point) return;
            const active = Boolean(place && selected && this.townKey(place) === this.townKey(selected));
            const label = place ? MarsAlmanacGlobe.townLabel(place, this.view[2], active ? size - 8 : size - point[0]) : null;
            const width = label ? label.width : 28;
            point = labDOM.call('lab_globe_markers_place', point[0], point[1], width, Number(Boolean(place)), Number(active)) as [number, number] | null;
            if (!point) return;
            const key = place ? this.townKey(place) : null;
            visible.add(key);
            let marker = this.markerNodes.get(key);
            if (!marker) {
                marker = document.createElement(place ? 'button' : 'span');
                marker.className = place ? 'globe-town' : 'globe-observer';
                if (marker instanceof HTMLButtonElement) {
                    marker.type = 'button';
                    marker.addEventListener('click', event => {
                        event.preventDefault();
                        event.stopPropagation();
                        void this.select(lat, lon, place);
                    });
                }
                this.markerNodes.set(key, marker);
                this.markers.append(marker);
            }
            const text = label ? `• ${label.name}` : '⊕';
            if (marker.textContent !== text) marker.textContent = text;
            const name = label && place ? `${label.full}, ${place.country}` : this.country ? `Observer, ${this.country}` : 'Observer';
            marker.dataset.marsTooltipTitle = `${name} · Latitude ${MarsAlmanacGlobe.coordinate(lat, 'N', 'S')} · ` +
                `Longitude ${MarsAlmanacGlobe.coordinate(lon, 'E', 'W')}`;
            marker.removeAttribute('title');
            marker.setAttribute('aria-label', marker.dataset.marsTooltipTitle);
            if (place && label) {
                marker.dataset.countryCode = place.jurisdiction;
                marker.dataset.townTooltipName = label.full;
                marker.dataset.townTooltipCoordinates = ` · Latitude ${MarsAlmanacGlobe.coordinate(lat, 'N', 'S')} · ` +
                    `Longitude ${MarsAlmanacGlobe.coordinate(lon, 'E', 'W')}`;
            }
            marker.style.maxWidth = `${width}px`;
            marker.style.left = `${point[0]}px`;
            marker.style.top = `${point[1]}px`;
            marker.classList.toggle('selected', active);
            if (active) marker.setAttribute('aria-current', 'location');
            else marker.removeAttribute('aria-current');
        };
        // Reserve the selected label first, even at overview zoom, so decluttering cannot remove it.
        if (selected) append(Number(selected.town.latitude), Number(selected.town.longitude), selected);
        const latitude = this.input('Latitude').value, longitude = this.input('Longitude').value;
        if (latitude.trim() && longitude.trim()) append(Number(latitude), Number(longitude));
        if (showTowns) {
            const [, , , west, south, east, north] = this.view;
            const places = labDOM.call('lab_town_store_region', west, south, east, north) as GlobeTown[];
            for (const place of places)
                append(Number(place.town.latitude), Number(place.town.longitude), this.placeCountry(place));
        }
        for (const [key, marker] of this.markerNodes) {
            if (!visible.has(key)) { marker.remove(); this.markerNodes.delete(key); }
        }
    }

    private async select(latitude: number, longitude: number, place?: GlobeTown): Promise<void> {
        this.rememberHistory();
        this.cancelFlight();
        const generation = ++this.selection;
        this.selectionCountryRequest?.abort();
        this.selectionCountryRequest = null;
        const select = document.getElementById('almanacTown') as WorksheetSelect;
        const jurisdiction = document.getElementById('almanacJurisdiction') as WorksheetSelect;
        const elevation = this.input('Elevation').value;
        try {
            if (place) {
                // The catalogue is already resident: commit the selection before yielding to evaluation.
                jurisdiction.value = place.jurisdiction;
                labDOM.call('lab_location_populate', select,
                    WorksheetLocations.townSelection.townsForJurisdiction(place.jurisdiction), false);
                select.value = place.town.value;
                WorksheetLocations.selects.syncSelection(select);
                WorksheetLocations.selects.syncSelection(document.getElementById('almanacJurisdiction') as WorksheetSelect);
                WorksheetLocations.townSelection.applySelectedTown(WorksheetLocations.calendar.calendarTownControls('almanac', false));
                this.input('Elevation').value = elevation;
            } else {
                jurisdiction.value = '';
                labDOM.call('lab_location_populate', select, [], false);
                WorksheetLocations.selects.syncSelection(jurisdiction);
                select.value = '';
                WorksheetLocations.selects.syncSelection(select);
            }
            this.input('Latitude').value = place ? place.town.latitude : String(latitude);
            this.input('Longitude').value = place ? place.town.longitude : String(longitude);
            this.clickedObserver = [Number(this.input('Latitude').value), Number(this.input('Longitude').value)];
            // Deliberately leave the authored observer altitude untouched.
            this.status.textContent = place ? `Selected ${place.town.name}` : 'Selected custom observer location';
            this.observerSignature = this.observerState();
            this.draw();
            if (!place) {
                const request = new AbortController();
                this.selectionCountryRequest = request;
                const timer = window.setTimeout(() => request.abort(), 5000);
                try {
                    const data = await this.lookupCountry(latitude, longitude, request.signal);
                    if (generation !== this.selection || this.input('Latitude').value !== String(latitude) ||
                        this.input('Longitude').value !== String(longitude) || jurisdiction.value || select.value) return;
                    const code = !request.signal.aborted ? data.code : '';
                    if (code && jurisdiction.querySelector(`option[value="${CSS.escape(code)}"]`)) {
                        jurisdiction.value = code;
                        labDOM.call('lab_location_populate', select,
                            WorksheetLocations.townSelection.townsForJurisdiction(code), false);
                        select.value = '';
                        WorksheetLocations.selects.syncSelection(select);
                        WorksheetLocations.selects.syncSelection(jurisdiction);
                    }
                } catch {
                    // Unavailable geography must not restore the previous country or change the chosen point.
                } finally {
                    window.clearTimeout(timer);
                    if (this.selectionCountryRequest === request) this.selectionCountryRequest = null;
                }
            }
            if (generation !== this.selection) return;
            if (!place && (this.input('Latitude').value !== String(latitude) ||
                           this.input('Longitude').value !== String(longitude))) return;
            this.observerSignature = this.observerState();
            await WorksheetLocations.calendarActions.triggerAlmanacAutoEvaluation();
        } catch (error) { if (generation === this.selection) this.status.textContent = String(error); }
    }

    private pick(x: number, y: number): void {
        const box = this.canvas.getBoundingClientRect(), radius = box.width * this.view[2] / 2.24;
        const point = labDOM.call('lab_globe_pick', (x - box.left - box.width / 2) / radius,
            (y - box.top - box.height / 2) / radius, this.orientation) as number[] | null;
        if (point) void this.select(point[0], point[1]);
    }

    private clearHoverCountry(): void {
        window.clearTimeout(this.hoverCountryTimer);
        this.hoverCountryTimer = 0;
        this.hoverCountryRequest?.abort();
        this.hoverCountryRequest = null;
        this.hoverCountryKey = this.hoverCountry = this.hoverCountryFlag = '';
    }

    private queueHoverCountry(point: number[]): void {
        const key = `${point[0]},${point[1]}`;
        if (key === this.hoverCountryKey) return;
        this.clearHoverCountry();
        this.hoverCountryKey = key;
        const cached = this.cachedCountry(key);
        this.hoverCountry = cached?.name || '';
        this.hoverCountryFlag = cached?.flag || '';
        if (!cached) this.hoverCountryTimer = window.setTimeout(() => {
            this.hoverCountryTimer = 0;
            this.loadCountry(point);
        }, 220);
    }

    private hideGridHover(preserveCountry = false): void {
        if (!preserveCountry) this.clearHoverCountry();
        this.gridTooltip.hidden = this.gridMarker.hidden = true;
        this.canvas.classList.remove('grid-hover');
    }

    private hoverGrid(event: PointerEvent, shift = event.shiftKey): void {
        this.hoverShift = shift;
        this.hideGridHover(true);
        if (!shift) this.clearHoverCountry();
        if (this.pointers.size || event.target !== this.canvas) { this.clearHoverCountry(); return; }
        // The observer remains pointer-transparent so dragging can start on its crosshair.
        const observer = this.markerNodes.get(null), observerBox = observer?.getBoundingClientRect();
        const overObserver = !shift && observerBox && event.clientX >= observerBox.left && event.clientX <= observerBox.right &&
            event.clientY >= observerBox.top && event.clientY <= observerBox.bottom;
        if (!this.grid && !shift && !overObserver) return;
        const box = this.canvas.getBoundingClientRect(), radius = box.width * this.view[2] / 2.24;
        const point = overObserver ? [Number(this.input('Latitude').value), Number(this.input('Longitude').value)] :
            labDOM.call('lab_globe_pick', (event.clientX - box.left - box.width / 2) / radius,
            (event.clientY - box.top - box.height / 2) / radius, this.orientation) as number[] | null;
        if (!point) { this.clearHoverCountry(); return; }
        if (shift) this.queueHoverCountry(point);
        const [latitude, longitude, flags] = shift || overObserver ? [point[0], point[1], 3] :
            labDOM.call('lab_globe_grid', point[0], point[1], this.view[2], box.width) as number[];
        if (!flags) return;
        const marker = this.project(flags & 1 ? latitude : point[0], flags & 2 ? longitude : point[1], box.width);
        if (!marker) return;
        const labels: string[] = [];
        if (flags & 1) labels.push(`Latitude ${MarsAlmanacGlobe.coordinate(latitude, 'N', 'S')}`);
        if (flags & 2) labels.push(`Longitude ${MarsAlmanacGlobe.coordinate(longitude, 'E', 'W')}`);
        const observerCountry = overObserver && this.countryKey === this.observerCountryKey();
        const country = shift ? this.hoverCountry : observerCountry ? this.country : '';
        const countryFlag = shift ? this.hoverCountryFlag : observerCountry ? this.countryFlag : '';
        if (country) labels.push(country);
        this.gridTooltip.textContent = labels.join(' · ');
        if (country && countryFlag) {
            const flag = this.countryFlagIcon;
            flag.className = 'country-flag'; flag.alt = '';
            if (flag.getAttribute('src') !== countryFlag) flag.src = countryFlag;
            this.gridTooltip.prepend(flag);
        }
        this.gridTooltip.hidden = false;
        this.gridMarker.style.left = `${marker[0]}px`;
        this.gridMarker.style.top = `${marker[1]}px`;
        this.gridMarker.hidden = false;
        this.canvas.classList.add('grid-hover');
        this.gridTooltip.style.left = `${Math.max(4, Math.min(box.width - this.gridTooltip.offsetWidth - 4, event.clientX - box.left + 12))}px`;
        this.gridTooltip.style.top = `${Math.max(4, Math.min(box.height - this.gridTooltip.offsetHeight - 4, event.clientY - box.top + 16))}px`;
    }

    public static coordinate(value: number, positive: string, negative: string): string {
        return labDOM.call('lab_globe_coordinate', value, positive, negative) as string;
    }

    private events(): void {
        const viewport = this.canvas.parentElement!;
        viewport.addEventListener('pointermove', event => {
            this.hoverPointer = event;
            this.hoverGrid(event);
        });
        const clearHover = (): void => {
            this.hoverPointer = null;
            this.hideGridHover();
        };
        viewport.addEventListener('pointerleave', clearHover);
        window.addEventListener('blur', clearHover);
        for (const name of ['keydown', 'keyup'] as const) window.addEventListener(name, event => {
            if (event.key === 'Shift' && this.hoverPointer)
                this.hoverGrid(this.hoverPointer, event.shiftKey);
        });
        this.root.querySelectorAll<HTMLButtonElement>('[data-globe-action]').forEach(button => {
            button.addEventListener('click', () => {
                this.cancelFlight();
                if (button.dataset.globeAction === 'map') {
                    this.map = !this.map;
                    button.setAttribute('aria-pressed', String(this.map));
                    button.textContent = this.map ? 'Map' : 'Natural Earth';
                    this.resetDetail();
                    if (this.usesMap() || this.view[2] > 1.5) void this.loadDetail();
                    this.draw();
                } else if (button.dataset.globeAction === 'grid') {
                    this.grid = !this.grid;
                    this.hideGridHover();
                    button.setAttribute('aria-pressed', String(this.grid));
                    button.textContent = `Lat/long: ${this.grid ? 'on' : 'off'}`;
                    this.draw();
                } else if (button.dataset.globeAction === 'borders') {
                    this.borders = !this.borders;
                    button.setAttribute('aria-pressed', String(this.borders));
                    button.textContent = `Borders: ${this.borders ? 'on' : 'off'}`;
                    if (this.borders) void this.loadBorders();
                    else this.unloadBorders();
                    this.draw();
                } else if (button.dataset.globeAction === 'centre') this.centre();
                else this.move(0, 0, button.dataset.globeAction === 'in' ? 1.6 : 1 / 1.6);
            });
        });
        this.root.querySelector<HTMLElement>('.almanac-globe-view')!.addEventListener('wheel', event => {
            event.preventDefault();
            event.stopPropagation();
            if (!event.deltaY) return;
            const pixels = event.deltaY * (event.deltaMode === 1 ? 16 : event.deltaMode === 2 ? this.canvas.clientHeight : 1);
            this.move(0, 0, Math.exp(-Math.max(-240, Math.min(240, pixels)) * .0015));
        }, {passive: false});
        this.canvas.addEventListener('pointerdown', event => {
            this.hideGridHover();
            if (event.button !== 0) return;
            this.cancelFlight();
            this.canvas.setPointerCapture(event.pointerId);
            this.pointers.set(event.pointerId, {x: event.clientX, y: event.clientY});
            this.start = {x: event.clientX, y: event.clientY};
            this.dragged = this.pointers.size > 1;
            this.suppressClick = this.dragged;
            this.canvas.classList.add('dragging');
        });
        this.canvas.addEventListener('pointermove', event => {
            const previous = this.pointers.get(event.pointerId);
            if (!previous) return;
            const other = [...this.pointers.entries()].find(([id]) => id !== event.pointerId)?.[1];
            const oldDistance = other ? Math.hypot(previous.x - other.x, previous.y - other.y) : 0;
            const factor = other && oldDistance > 0 ? Math.hypot(event.clientX - other.x, event.clientY - other.y) / oldDistance : 1;
            this.dragged ||= Math.hypot(event.clientX - this.start.x, event.clientY - this.start.y) > 8;
            this.suppressClick ||= this.dragged;
            const radius = this.canvas.clientWidth / 2.24;
            if (this.dragged) {
                const rect = this.canvas.getBoundingClientRect();
                const side = (this.start.x - rect.left - rect.width / 2) / (rect.width / 2);
                const roll = !other && Math.abs(side) >= .6;
                const dx = (event.clientX - previous.x) / radius;
                const dy = (event.clientY - previous.y) / radius;
                this.move(dx, roll ? 0 : dy, Math.max(0.1, factor), roll ? -Math.sign(side) * dy : 0);
            }
            this.pointers.set(event.pointerId, {x: event.clientX, y: event.clientY});
        });
        const release = (event: PointerEvent): void => {
            if (event.type !== 'pointerup' && this.pointers.has(event.pointerId)) this.suppressClick = true;
            this.pointers.delete(event.pointerId);
            if (!this.pointers.size) {
                this.canvas.classList.remove('dragging');
                this.draw();
            }
        };
        for (const name of ['pointerup', 'pointercancel', 'lostpointercapture']) this.canvas.addEventListener(name, event => release(event as PointerEvent));
        this.canvas.addEventListener('click', event => {
            if (event.button !== 0 || this.suppressClick) return;
            event.preventDefault();
            event.stopPropagation();
            this.pick(event.clientX, event.clientY);
        });
        this.canvas.addEventListener('keydown', event => {
            const moves: Record<string, [number, number, number]> = {
                ArrowLeft: [0.15, 0, 1], ArrowRight: [-0.15, 0, 1], ArrowUp: [0, 0.15, 1], ArrowDown: [0, -0.15, 1],
                '+': [0, 0, 1.6], '=': [0, 0, 1.6], '-': [0, 0, 1 / 1.6]
            };
            if (moves[event.key]) { event.preventDefault(); event.stopPropagation(); this.move(...moves[event.key]); }
            if (event.key === 'Enter') {
                event.preventDefault(); event.stopPropagation();
                void this.select(this.view[0], this.view[1]);
            }
        });
        this.canvas.addEventListener('webglcontextlost', event => {
            this.cancelFlight();
            event.preventDefault(); ++this.generation; this.request?.abort();
            window.clearTimeout(this.timer);
            this.status.textContent = 'Globe graphics interrupted. Reload to restore; observer fields remain available.';
        });
        document.addEventListener('mars-almanac-observer-committed', () => {
            if (this.historyRestoring || !this.observerStartupReady) return;
            if (this.retainsClickedView()) {
                this.observerSignature = this.observerState();
                this.scheduleCameraSave();
                this.draw();
                return;
            }
            if (this.observerSignature !== this.observerState()) {
                ++this.selection;
                this.centre();
            }
        });
        for (const id of ['almanacLatitude', 'almanacLongitude', 'almanacTown', 'almanacJurisdiction'])
            document.getElementById(id)?.addEventListener('change', () => {
                this.clickedObserver = null;
                this.cancelFlight();
                ++this.selection;
                requestAnimationFrame(() => {
                    if (!this.observerStartupReady) return;
                    if (this.observerSignature !== this.observerState()) this.centre();
                });
            });
    }
}

/** @brief Install after the worksheet's native state and catalogue have been initialised. */
function installAlmanacGlobe(): void {
    const root = document.getElementById('almanacGlobe');
    if (!root) return;
    try { new MarsAlmanacGlobe(root); }
    catch (error) { root.querySelector('[role="status"]')!.textContent = String(error); }
}
