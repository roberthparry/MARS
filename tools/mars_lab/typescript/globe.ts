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
interface GlobeTown { town: LabTown; jurisdiction: string; country: string; displayName?: string; fullName?: string; }
interface GlobeCountry { name: string; flag: string; code: string; flagCode: string; nauticalZone: string; }

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
    private static readonly labelMeasure = document.createElement('canvas').getContext('2d');
    private static townLabel(place: GlobeTown, zoom: number, available: number): {name: string; full: string; width: number} {
        const name = place.displayName || place.town.name;
        const full = place.fullName || name;
        const expanded = full !== name && zoom >= 12 && available >= 430;
        const text = expanded ? full : name, measure = this.labelMeasure;
        if (measure) measure.font = '600 11px Arial, sans-serif';
        const width = measure ? Math.ceil(measure.measureText(`• ${text}`).width) + 18 : 220;
        return {name: text, full, width: Math.min(Math.max(28, available), expanded ? 430 : 220, width)};
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
    private shoreFrameRevision = 0;
    private shoreBounds: number[] = [];
    private shoreFrameBounds: number[] = [];
    private shoreColourFrame: number[] | null = null;
    private shoreBlendStarted = 0;
    private shoreWasReady = false;
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
    private shoreRetryAfter = 0;
    private shoreClosed = false;
    private movingUntil = 0;
    private settleTimer = 0;
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
    private travelBorders: import('./globe_gpu.js').MarsGlobeBorders | null = null;
    private travelBordersLoading = false;
    private travelBordersRequest: AbortController | null = null;
    private readonly textures: WebGLTexture[] = [];
    private detailBounds = [-180, -90, 180, 90];
    private detailReady = false;
    private grid = false;
    private highResolutionRequested = false;
    private worldLoading = false;
    private highResolutionLoading = false;
    private palettesReady = false;
    private photo: import('./globe_photo.js').GlobePhoto | null = null;
    private photoModule: Promise<typeof import('./globe_photo.js')> | null = null;
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
        promise: Promise<GlobeCountry>;
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
    private riverRequestKey = '';
    private riverRequestBounds: number[] | null = null;
    private riverRequestZoom = 0;
    private riverRequestObserver = '';
    private riverTimer = 0;
    private riverTimerAt = 0;
    private riverRetryDelay = 1000;
    private riverFallbackRetryAt = 0;
    private riverGeneration = 0;
    private riverReady = false;
    private riverComposite = false;
    private riverPreview = false;
    private riverSurface: import('./globe_surface.js').GlobeSurface | null = null;
    private riverTiles: import('./globe_surface_tiles.js').GlobeSurfaceTiles | null = null;
    private riverTilesModule: Promise<typeof import('./globe_surface_tiles.js')> | null = null;
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
    private readonly travelNotice = document.createElement('div');
    private photoLoading = 0;
    private flightSpeed = 0;
    private historyZoom: number | null = null;
    private historyRedirect = false;
    private historyRestoring = false;

    /** Capture only five camera numbers; imagery and catalogues remain outside worksheet history. */
    public static captureHistory(): number[] | undefined {
        const globe = this.instance;
        if (!globe) return undefined;
        if (globe.flightTarget) {
            if (globe.sameCameraObserver(globe.observerSignature)) return globe.flightTarget.slice();
            // A newly selected observer must not inherit the previous journey's camera.
            return (labDOM.call('lab_globe_history_camera', null,
                Number(globe.input('Latitude').value), Number(globe.input('Longitude').value),
                globe.historyZoom ?? globe.view[2]) as number[] | null) ?? undefined;
        }
        globe.rememberHistory();
        // Persist what is displayed even while form fields are changing. Navigation validates
        // destination visibility separately when the history entry is actually revisited.
        return [...globe.orientation, globe.historyZoom ?? globe.view[2]];
    }

    /** Suspend automatic centring while asynchronous history restoration resolves the destination. */
    public static beginHistory(): number | undefined {
        const globe = this.instance;
        if (!globe) return undefined;
        const zoom = globe.historyZoom ?? globe.flightTarget?.[4] ?? globe.view[2];
        const redirect = Boolean(globe.flightTarget) || globe.historyRestoring;
        // Keep submitting the current route while the replacement restores its controls.
        // Generation owns restoration; the target identity separately owns animation.
        ++globe.flightGeneration;
        globe.historyZoom = zoom;
        globe.historyRedirect = redirect;
        globe.historyRestoring = true;
        globe.clickedObserver = null;
        globe.stopRegionRequests();
        // A saved zoom may cross from photographic detail to a land mask or vice versa.
        globe.detailReady = false;
        // Geographic textures remain valid while history resolves the next destination.
        return globe.flightGeneration;
    }

    /** Start only the still-owned restoration; stale completions cannot move the camera. */
    public static finishHistory(token: number | undefined, camera: unknown, accepted: boolean): void {
        const globe = this.instance;
        if (!globe || token !== globe.flightGeneration) return;
        globe.historyRestoring = false;
        if (!accepted) {
            if (!globe.flightTarget) { globe.cancelFlight(); globe.move(0, 0, 1); }
            return;
        }
        cancelAnimationFrame(globe.flightFrame);
        globe.flightFrame = 0;
        const start = [...globe.orientation, globe.view[2]];
        const zoom = globe.historyZoom ?? start[4], redirect = globe.historyRedirect;
        globe.historyZoom = zoom;
        let target = labDOM.call('lab_globe_history_camera', camera,
            Number(globe.input('Latitude').value), Number(globe.input('Longitude').value), zoom) as number[] | null;
        if (!target) { globe.cancelFlight(); return; }
        // Navigation keeps the current viewing scale; saved pan and roll remain useful only
        // if they still show the observer at that scale. Revalidate after applying it.
        target[4] = zoom;
        target = labDOM.call('lab_globe_history_camera', target,
            Number(globe.input('Latitude').value), Number(globe.input('Longitude').value), zoom) as number[];
        globe.observerSignature = globe.observerState();
        globe.flightTarget = target;
        const destination = globe.selectedTown();
        const name = destination ? MarsAlmanacGlobe.townLabel(destination, zoom, 600).full :
            `${MarsAlmanacGlobe.coordinate(Number(globe.input('Latitude').value), 'N', 'S')}, ` +
            MarsAlmanacGlobe.coordinate(Number(globe.input('Longitude').value), 'E', 'W');
        const distance = labDOM.call('lab_globe_flight_distance', start, target) as number | null;
        const distanceLabel = distance !== null && Number.isFinite(distance) && distance >= 0
            ? distance < 1 ? `${Math.round(distance * 1000).toLocaleString()} m`
                : `${Math.round(distance).toLocaleString()} km` : '';
        globe.travelNotice.textContent = `Travelling to ${name}${distanceLabel ? ` · ${distanceLabel}` : ''}`;
        globe.travelNotice.hidden = false;
        // Fetch the destination once, before zooming towards it, rather than chasing intermediate regions.
        void globe.loadShore();
        void globe.loadTravelBorders();
        void globe.loadPhoto();
        void globe.loadRivers();
        globe.hideGridHover();
        const complete = (): void => {
            globe.applyCamera(target);
            if (globe.historyRestoring) {
                globe.flightTarget = null;
                globe.flightFrame = 0;
                globe.draw();
                return;
            }
            globe.cancelFlight();
            globe.move(0, 0, 1);
        };
        if (!globe.started || document.hidden || globe.gl.isContextLost() ||
            matchMedia('(prefers-reduced-motion: reduce)').matches || start.every((value, i) => value === target[i])) {
            complete();
            return;
        }
        void globe.loadHighResolution();
        globe.draw(true);
        let previous = performance.now(), progress = 0, speed = redirect ? globe.flightSpeed : 0;
        const tick = (now: number): void => {
            if (globe.flightTarget !== target) return;
            if (WorksheetWorkspace.mode.currentMode() !== 'almanac') { globe.cancelFlight(); return; }
            globe.flightFrame = 0;
            // A delayed paint or worker result is not elapsed travel time: never jump to catch up.
            const elapsed = Math.max(0, Math.min(50, now - previous));
            previous = now;
            if (progress >= 1 || document.hidden || matchMedia('(prefers-reduced-motion: reduce)').matches) {
                complete();
                return;
            }
            const loading = globe.worldLoading || globe.highResolutionLoading ||
                (progress === 0 && (globe.travelBordersLoading || globe.photoLoading > 0));
            // Warm the photographic surface before departure. Local vectors refine that complete
            // surface asynchronously, without braking and restarting at each coverage boundary.
            speed += Math.max(-elapsed / 600, Math.min(elapsed / 1200, (loading ? 0 : 1) - speed));
            const ready = !loading;
            if (!ready) speed = 0;
            globe.flightSpeed = speed;
            if (ready && speed > 0) {
                const advanced = labDOM.call('lab_globe_flight_advance', start, target, progress,
                    elapsed * speed, Math.max(1, globe.canvas.clientWidth), Number(redirect)) as number | null;
                if (advanced === null) { complete(); return; }
                progress = advanced;
                const next = labDOM.call('lab_globe_flight', start, target, progress, Number(redirect)) as number[];
                globe.applyCamera(next);
            }
            globe.flightFrame = requestAnimationFrame(tick);
        };
        globe.flightFrame = requestAnimationFrame(tick);
    }

    private cancelFlight(): void {
        ++this.flightGeneration;
        cancelAnimationFrame(this.flightFrame);
        this.flightFrame = 0;
        this.flightTarget = null;
        this.travelNotice.hidden = true;
        this.flightSpeed = 0;
        this.historyZoom = null;
        this.historyRedirect = false;
        this.historyRestoring = false;
    }

    private stopRegionRequests(): void {
        window.clearTimeout(this.riverTimer);
        this.riverTimer = 0;
        this.photo?.cancel();
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
        this.draw(Boolean(this.flightTarget));
        this.scheduleCameraSave();
    }

    constructor(private readonly root: HTMLElement) {
        this.canvas = root.querySelector('canvas')!;
        this.markers = root.querySelector('.almanac-globe-markers')!;
        this.status = root.querySelector('[role="status"]')!;
        this.travelNotice.className = 'globe-travel-notice';
        this.travelNotice.setAttribute('role', 'status');
        this.travelNotice.hidden = true;
        this.canvas.parentElement!.append(this.travelNotice);
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
        for (let i = 0; i < 8; ++i) {
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
        gl.uniform1i(this.uniform('rivers'), 3);
        gl.uniform1i(this.uniform('previousRivers'), 2);
        gl.uniform1i(this.uniform('shoreMask'), 4);
        gl.uniform1i(this.uniform('landPalette'), 5);
        gl.uniform1i(this.uniform('waterPalette'), 6);
        gl.uniform1i(this.uniform('photoPatch'), 7);
        void labApplication.restoredReady.then(async () => {
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
            this.photo?.dispose();
            this.riverRequest?.abort();
            window.clearTimeout(this.riverTimer);
            this.riverSurface?.dispose();
            this.riverTiles?.dispose();
            this.travelBordersRequest?.abort();
            this.travelBorders?.dispose();
            void this.maskRenderer?.then(renderer => renderer.dispose()).catch(() => {});
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

    private usesShoreDetail(): boolean {
        return this.view[2] >= 8;
    }

    private resetDetail(): void {
        // Crossing the close-view threshold must not cancel the earlier prefetch.
        if (this.view[2] < 4) {
            this.shoreRequest?.abort();
            this.shoreRequest = null;
        }
        ++this.generation;
        this.request?.abort();
        this.requestBounds = null;
        this.detailReady = false;
        if (this.usesShoreDetail()) void this.loadShore();
    }

    private observerState(): string {
        return JSON.stringify(['Latitude', 'Longitude', 'Town', 'Jurisdiction'].map(
            field => (document.getElementById('almanac' + field) as HTMLInputElement | HTMLSelectElement).value));
    }

    private restoreCamera(): void {
        try {
            const restored = worksheetHistoryPersistence.restoredGlobeCamera;
            const saved = restored ? {camera: restored} : JSON.parse(localStorage.getItem(MarsAlmanacGlobe.cameraStorageKey) || 'null');
            if (!saved) return;
            // Native validation rejects malformed quaternions and out-of-range zooms.
            const camera = labDOM.call('lab_globe_saved_camera', saved.camera) as number[] | null;
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
        if (this.flightTarget && !this.sameCameraObserver(this.observerSignature)) return;
        try {
            localStorage.setItem(MarsAlmanacGlobe.cameraStorageKey, JSON.stringify({
                observer: this.observerState(),
                camera: this.flightTarget ?? [...this.orientation, this.view[2]]
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
        this.borderRequest?.abort();
        this.borderRequest = null;
        const hadShoreDetail = this.usesShoreDetail();
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
        if (dx || dy || roll || factor !== 1) {
            const bounds = this.riverRequestBounds;
            const observerChanged = this.riverRequestObserver !== this.observerSignature;
            const scaleChanged = factor !== 1 && (!bounds || !this.riverCovers(this.view, bounds) ||
                this.view[2] > this.riverRequestZoom * 1.5);
            if (this.riverRequest && (observerChanged || scaleChanged)) {
                ++this.riverGeneration;
                this.riverRequest.abort();
                this.riverRequest = null;
            }
            this.scheduleRivers(100);
        }
        // Retain authored camera changes before a dropdown refresh can change the observer controls.
        // Flight frames use applyCamera instead, so animation cannot replace the saved destination.
        if (dx || dy || roll || factor !== 1) this.rememberHistory();
        this.scheduleCameraSave();
        this.scheduleRiverUnload();
        if (!this.borders) this.unloadBorders();
        if (hadShoreDetail !== this.usesShoreDetail()) this.resetDetail();
        this.draw();
        window.clearTimeout(this.timer);
        this.timer = window.setTimeout(() => {
            if (this.usesShoreDetail() || this.view[2] > 1.5) void this.loadDetail();
            void this.loadRivers();
            void this.loadBorders();
            if (this.view[2] >= 3) void labApplication.loadNearbyTowns(this.view[0], this.view[1]);
        }, 300);
    }

    private async riverImage(bounds: number[], signal?: AbortSignal): Promise<HTMLImageElement> {
        const [west, south, east, north] = bounds;
        const response = await labHttp.fetch('/almanac-rivers', {
            method: 'POST', body: labWire.encode({west, south, east, north, view: 'natural'}), signal
        });
        const result = await response.labData() as {ok: boolean; image?: string; error?: string};
        if (!response.ok || !result.ok || !result.image) throw new Error(result.error || 'River imagery unavailable.');
        const url = URL.createObjectURL(new Blob([result.image], {type: 'image/svg+xml'}));
        try {
            const image = new Image();
            image.src = url;
            await image.decode();
            return image;
        } finally { URL.revokeObjectURL(url); }
    }

    private upload(index: number, image: HTMLImageElement | import('./globe_surface.js').GlobeSurfaceTile): void {
        const gl = this.gl;
        gl.activeTexture(gl.TEXTURE0 + index);
        gl.bindTexture(gl.TEXTURE_2D, this.textures[index]);
        const filter = index === 2 || (index === 1 && this.usesShoreDetail()) ? gl.NEAREST : gl.LINEAR;
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, filter);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, filter);
        if ('pixels' in image)
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, image.width, image.height, 0, gl.RGBA, gl.UNSIGNED_BYTE, image.pixels);
        else gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, image);
        this.draw();
    }

    private async loadWorld(): Promise<void> {
        this.worldLoading = true;
        this.status.textContent = 'Loading Earth imagery…';
        try {
            const image = new Image();
            image.src = '/earth_surface.png';
            await image.decode();
            this.upload(0, image);
            this.root.dataset.surface = 'blue-marble';
            this.root.dataset.resolution = '2048';
            await this.refreshPalettes();
            this.status.textContent = 'Drag to rotate · drag vertically at either side to roll · scroll or pinch to zoom · select a town or point';
            void this.loadHighResolution();
        } catch (error) { this.status.textContent = String(error); }
        finally { this.worldLoading = false; }
    }

    private async loadHighResolution(): Promise<void> {
        if (this.highResolutionRequested || this.view[2] < 2 || !this.root.dataset.surface) return;
        const gl = this.gl, limit = gl.getParameter(gl.MAX_TEXTURE_SIZE) as number;
        let width = 8192;
        while (width > limit) width /= 2;
        if (width <= 2048) return;
        this.highResolutionRequested = true;
        this.highResolutionLoading = true;
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
                        await this.refreshPalettes();
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
            this.status.textContent = `Earth detail unavailable; overview retained. ${String(error)}`;
        } finally { this.highResolutionLoading = false; }
    }

    private async refreshPalettes(): Promise<void> {
        this.palettesReady = false;
        try {
            const module = await import('./globe_gpu.js');
            const gl = this.gl;
            gl.useProgram(this.program);
            gl.bindBuffer(gl.ARRAY_BUFFER, this.quad);
            gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
            const textures = module.buildGlobePalettes(gl, this.program);
            textures.forEach((texture, i) => {
                gl.deleteTexture(this.textures[5 + i]);
                this.textures[5 + i] = texture;
                gl.activeTexture(gl.TEXTURE5 + i);
                gl.bindTexture(gl.TEXTURE_2D, texture);
            });
            this.palettesReady = true;
        } catch {
            // Small graphics budgets retain the direct shader calculation, never incomplete textures.
        }
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
        void this.loadPhoto();
        if (this.view[2] >= 4) await this.loadShore();
    }

    private async loadPhoto(): Promise<void> {
        const target = this.flightTarget;
        if ((target?.[4] ?? this.view[2]) < 6 || this.shoreClosed) return;
        const latitude = target ? Number(this.input('Latitude').value) : this.view[0];
        const longitude = target ? Number(this.input('Longitude').value) : this.view[1];
        ++this.photoLoading;
        try {
            const module = await (this.photoModule ||= import('./globe_photo.js'));
            if (this.shoreClosed || this.flightTarget !== target) return;
            this.photo ||= new module.GlobePhoto(this.gl, () => {
                this.root.dataset.photoResolution = '21600';
                this.draw();
            });
            await this.photo.load(labDOM.call('lab_globe_photo_patch', latitude, longitude) as number[]);
        } catch { /* Retain the existing photographic surface if optional tiles are unavailable. */ }
        finally { --this.photoLoading; }
    }

    private noteMotion(): void {
        this.movingUntil = performance.now() + 150;
        window.clearTimeout(this.settleTimer);
        this.settleTimer = window.setTimeout(() => this.draw(), 160);
    }

    private scheduleRiverUnload(): void {
        if (this.view[5] - this.view[3] <= 30 && this.view[6] - this.view[4] <= 30) {
            window.clearTimeout(this.riverUnloadTimer);
            this.riverUnloadTimer = 0;
        } else if (this.riverReady && !this.riverUnloadTimer) {
            this.riverUnloadTimer = window.setTimeout(() => {
                this.riverUnloadTimer = 0;
                if (this.view[5] - this.view[3] <= 30 && this.view[6] - this.view[4] <= 30) return;
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

    private async loadTravelBorders(): Promise<void> {
        if (!this.borders || this.travelBorders || this.travelBordersLoading) return;
        this.travelBordersLoading = true;
        const request = this.travelBordersRequest = new AbortController();
        const timeout = window.setTimeout(() => request.abort(), 15000);
        try {
            const response = await labHttp.fetch('/almanac-borders', {
                method: 'POST', signal: request.signal,
                body: labWire.encode({west: -180, south: -90, east: 180, north: 90, format: 'segments'})
            });
            const data = await response.labData() as {ok?: boolean; segments?: string};
            if (!response.ok || !data.ok || typeof data.segments !== 'string' || request.signal.aborted)
                throw new Error('Travel borders unavailable');
            const vertices = await this.shoreJob('lab_globe_borders_mesh', [data.segments], request.signal) as Float32Array;
            const module = await import('./globe_gpu.js');
            if (request.signal.aborted || this.shoreClosed) return;
            try {
                this.travelBorders = new module.MarsGlobeBorders(this.gl,
                    labDOM.call('lab_globe_borders_shader', 0) as string,
                    labDOM.call('lab_globe_borders_shader', 1) as string, vertices);
                this.root.dataset.travelBorders = 'ready';
            } finally { this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.quad); }
        } catch {
            this.root.dataset.travelBorders = 'unavailable';
        } finally {
            window.clearTimeout(timeout);
            this.travelBordersLoading = false;
            this.travelBordersRequest = null;
        }
    }

    private riverCovers(view: number[], bounds: number[]): boolean {
        return view[3] >= bounds[0] && view[4] >= bounds[1] && view[5] <= bounds[2] && view[6] <= bounds[3];
    }

    private scheduleRivers(delay: number): void {
        const at = performance.now() + delay;
        if (this.shoreClosed || (this.riverTimer && this.riverTimerAt <= at)) return;
        window.clearTimeout(this.riverTimer);
        this.riverTimerAt = at;
        this.riverTimer = window.setTimeout(() => {
            this.riverTimer = 0;
            void this.loadRivers();
        }, delay);
    }

    private async loadRivers(): Promise<void> {
        const {GlobeSurfaceTiles} = await (this.riverTilesModule ||= import('./globe_surface_tiles.js'));
        const target = this.flightTarget;
        const orientation = target ? target.slice(0, 4) : this.orientation;
        const zoom = target?.[4] ?? this.view[2];
        if (this.shoreClosed || !this.root.getClientRects().length || this.gl.isContextLost()) return;
        this.riverTiles ??= new GlobeSurfaceTiles(this.gl);
        const view = labDOM.call('lab_globe_view', orientation, zoom, 1) as GlobeView;
        const reserve = labDOM.call('lab_globe_view', orientation, zoom / 1.25, 1) as GlobeView;
        let bounds = GlobeSurfaceTiles.bounds(reserve.slice(3));
        if (bounds[2] - bounds[0] > 30 || bounds[3] - bounds[1] > 30)
            bounds = (labDOM.call('lab_globe_view', orientation, zoom / 1.15, 1) as GlobeView).slice(3);
        if (bounds[2] - bounds[0] > 30 || bounds[3] - bounds[1] > 30) return;
        if (this.riverTiles.covering(reserve.slice(3), zoom)?.complete) {
            this.draw();
            return;
        }
        if (!this.riverPreview && this.riverReady && (this.riverComposite || performance.now() < this.riverFallbackRetryAt) &&
            this.riverCovers(reserve, this.riverBounds) &&
            zoom <= this.riverZoom * 1.5) return;
        const key = bounds.join(',');
        if (this.riverRequest && !this.riverRequest.signal.aborted && (this.riverRequestKey === key ||
            (!target && this.riverRequestObserver === this.observerSignature) ||
            (this.riverRequestBounds && this.riverCovers(view, this.riverRequestBounds) &&
             zoom <= this.riverRequestZoom * 1.5))) return;
        const generation = ++this.riverGeneration;
        this.riverRequest?.abort();
        const request = this.riverRequest = new AbortController();
        this.riverRequestKey = key;
        this.riverRequestBounds = bounds;
        this.riverRequestZoom = zoom;
        this.riverRequestObserver = this.observerSignature;
        const timeout = window.setTimeout(() => request.abort(), 15000);
        let bitmap: import('./globe_surface.js').GlobeSurfaceTile | null = null;
        try {
            let image: HTMLImageElement | import('./globe_surface.js').GlobeSurfaceTile;
            try {
                if (!this.riverSurface) {
                    const {GlobeSurface} = await import('./globe_surface.js');
                    this.riverSurface ??= new GlobeSurface();
                }
                request.signal.throwIfAborted();
                image = bitmap = await this.riverSurface.load(bounds, request.signal, (tile, detailed) => {
                    if (generation !== this.riverGeneration || request.signal.aborted) return;
                    // Keep surveyed imagery while the new window only has its NASA overview.
                    if (!detailed && this.riverReady && this.riverComposite) return;
                    this.riverTiles!.put(bounds, zoom, tile, false);
                    this.root.dataset.riverSurface = 'preview';
                    this.draw();
                });
            } catch {
                request.signal.throwIfAborted();
                image = await this.riverImage(bounds, request.signal);
            }
            if (generation !== this.riverGeneration || request.signal.aborted) return;
            if (!bitmap && this.riverComposite && this.riverReady && this.riverCovers(view, this.riverBounds)) {
                this.scheduleRivers(this.riverRetryDelay);
                this.riverRetryDelay = Math.min(8000, this.riverRetryDelay * 2);
                return;
            }
            if (bitmap) {
                this.riverTiles.put(bounds, zoom, bitmap);
                this.riverPreview = false;
            }
            else {
                this.riverBounds = bounds;
                this.riverZoom = zoom;
                this.riverReady = true;
                this.riverComposite = false;
                this.riverPreview = false;
                this.riverFallbackRetryAt = performance.now() + 8000;
                this.upload(3, image);
            }
            this.root.dataset.rivers = 'ready';
            this.root.dataset.riverSurface = bitmap ? 'composite' : 'overlay';
            this.riverRetryDelay = 1000;
            this.draw();
            if (!bitmap) this.scheduleRivers(8000);
            else if (!this.riverCovers(this.view, bounds)) this.scheduleRivers(100);
        } catch {
            // An unavailable optional layer must not delay or disable observer selection.
            if (generation === this.riverGeneration) {
                this.root.dataset.rivers = 'unavailable';
                this.scheduleRivers(this.riverRetryDelay);
                this.riverRetryDelay = Math.min(8000, this.riverRetryDelay * 2);
            }
        } finally {
            bitmap?.close();
            window.clearTimeout(timeout);
            if (generation === this.riverGeneration) this.riverRequest = null;
        }
    }

    private async loadShore(): Promise<void> {
        if (this.historyRestoring || (this.flightTarget && performance.now() < this.shoreRetryAfter)) return;
        const orientation = this.flightTarget?.slice(0, 4) || this.orientation;
        const zoom = this.flightTarget?.[4] || this.view[2];
        if (this.shoreRequest || zoom < 4) return;
        const view = labDOM.call('lab_globe_view', orientation, zoom, 1) as GlobeView;
        const reserve = labDOM.call('lab_globe_view', orientation, zoom / 1.5, 1) as GlobeView;
        const prediction = !this.flightTarget && performance.now() - this.dragSampleTime < 200 ? this.dragPrediction : null;
        const ahead = prediction ? labDOM.call('lab_globe_view', prediction, zoom / 1.5, 1) as GlobeView : reserve;
        if (labDOM.call('lab_globe_shore_covers', reserve) && labDOM.call('lab_globe_shore_covers', ahead) &&
            zoom <= this.shoreZoom * 1.5) return;
        let bounds = (labDOM.call('lab_globe_view', prediction || orientation, zoom / 2, 1) as GlobeView).slice(3);
        if (!labDOM.call('lab_globe_shore_frame_covers', view, bounds))
            bounds = (labDOM.call('lab_globe_view', orientation, zoom / 2, 1) as GlobeView).slice(3);
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
            if (!request.signal.aborted) this.shoreRetryAfter = performance.now() + 30000;
            if (!request.signal.aborted) this.root.dataset.shoreline = 'unavailable';
            if (!request.signal.aborted) this.status.textContent = `Vector shoreline pending: ${String(error)}`;
        } finally {
            if (this.shoreRequest === request) this.shoreRequest = null;
            if (loaded && !this.flightTarget && this.usesShoreDetail() && !labDOM.call('lab_globe_shore_covers', this.view))
                void this.loadShore();
        }
    }

    private drawShore(size: number): boolean {
        if (this.flightTarget && !this.historyRestoring) {
            void this.loadShore();
            // Fetch geometry off-thread, but defer large stencil textures until arrival.
            // Uploading them during travel can stall the GPU despite a fast JavaScript frame.
        }
        const visible = this.usesShoreDetail() && Boolean(labDOM.call('lab_globe_shore_covers', this.view));
        this.shoreOverlay.style.display = 'none';
        if (!this.usesShoreDetail()) return false;
        // The global photograph is already projected by the GPU. At overview scale it avoids
        // serialising a new world-sized vector frame for every animation step.
        if (this.flightTarget && this.view[2] < 8) return false;
        const frame = [...this.orientation, this.view[2], size];
        let retained = false;
        if (this.shoreFrame && this.shoreFrame[4] >= 8 && this.view[2] >= 8) {
            const transform = labDOM.call('lab_globe_frame_transform', this.shoreFrame, frame) as number[] | null;
            if (transform) {
                this.shoreOverlay.setAttribute('viewBox', `0 0 ${size} ${size}`);
                this.shoreGroup.setAttribute('transform', `matrix(${transform.join(' ')})`);
                this.shoreOverlay.style.display = '';
                retained = true;
            }
        }
        if (this.historyRestoring || this.flightTarget) return retained;
        // One bounded request follows the camera even throughout a continuous drag.
        void this.loadShore();
        if (!visible) return retained;
        // Pointer motion only transforms existing paths. Rebuilding thousands of SVG
        // commands on each worker completion competes with the next input frame.
        // Publish replacement geometry before leaving the old coverage, not after it disappears.
        if (retained && performance.now() < this.movingUntil) return true;
        this.projectShore([...this.orientation, this.view[2]], size);
        return retained;
    }

    private projectShore(camera: number[], size: number): void {
        const frame = [...camera, size], orientation = camera.slice(0, 4);
        const signature = `${size},${camera[4]},${orientation.join(',')}`;
        if (this.shoreClosed || this.shoreDrawing || signature === this.shoreSignature ||
            signature === this.shoreFailed || (this.flightTarget && performance.now() < this.shoreRetryAfter)) return;
        this.shoreDrawing = true;
        const revision = this.shoreRevision, frameBounds = this.shoreBounds, flight = this.flightTarget;
        void this.shoreJob('lab_globe_shore_frame', [orientation, camera[4], size]).then(async value => {
            if (value === null || this.historyRestoring || this.flightTarget) return;
            const result = value as import('./globe_gpu.js').GlobeMaskFrame;
            if (!result || typeof result.svg !== 'string') throw new Error('Shoreline projection unavailable');
            if (revision !== this.shoreRevision) return;
            this.maskRenderer ??= import('./globe_gpu.js').then(module => new module.MarsGlobeMask(this.gl,
                labDOM.call('lab_globe_mask_shader', 0) as string, labDOM.call('lab_globe_mask_shader', 1) as string));
            const renderer = await this.maskRenderer;
            if (this.shoreClosed || revision !== this.shoreRevision || this.historyRestoring || this.flightTarget) return;
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
            // The opaque GPU surface owns flight rendering; do not parse thousands of invisible SVG commands.
            this.shoreGroup.innerHTML = flight ? '' : result.svg;
            this.shoreFrame = frame;
            this.shoreFrameRevision = revision;
            this.shoreFrameBounds = frameBounds;
            this.shoreGroup.removeAttribute('transform');
            this.shoreSignature = signature;
        }).catch(error => {
            this.shoreFailed = signature;
            this.shoreRetryAfter = performance.now() + 30000;
            this.status.textContent = String(error);
        }).finally(() => {
            this.shoreDrawing = false;
            if (!this.shoreClosed) this.draw();
        });
    }

    private uniform(name: string): WebGLUniformLocation | null {
        if (!this.uniforms.has(name)) this.uniforms.set(name, this.gl.getUniformLocation(this.program, name));
        return this.uniforms.get(name)!;
    }

    private draw(immediate = false): void {
        if (immediate) cancelAnimationFrame(this.frame);
        else if (this.frame) return;
        const render = (): void => {
            this.frame = 0;
            const size = this.canvas.clientWidth;
            if (!size || !this.started || !this.cameraRestored || this.gl.isContextLost()) return;
            const mapReady = this.drawShore(size);
            this.surfacePending = this.usesShoreDetail() && !mapReady;
            this.root.dataset.surfacePending = String(this.surfacePending);
            const pixels = Math.min(1536, Math.round(size * Math.min(devicePixelRatio, this.flightTarget ? 1 : 1.5)));
            if (this.canvas.width !== pixels || this.canvas.height !== pixels)
                this.canvas.width = this.canvas.height = pixels;
            const gl = this.gl;
            gl.activeTexture(gl.TEXTURE2);
            gl.bindTexture(gl.TEXTURE_2D, this.textures[2]);
            const regional = this.view[5] - this.view[3] <= 30 && this.view[6] - this.view[4] <= 30;
            const resident = regional ? this.riverTiles?.bind(this.view.slice(3), this.view[2]) : null;
            if (resident) {
                this.riverBounds = resident.bounds;
                this.riverZoom = resident.zoom;
                this.riverReady = this.riverComposite = true;
                this.riverPreview = !resident.complete;
                this.riverFallbackRetryAt = 0;
            }
            const colourTransform = mapReady && this.shoreColourFrame === this.shoreFrame
                ? labDOM.call('lab_globe_frame_transform', this.shoreFrame, [...this.orientation, this.view[2], size]) as number[] | null
                : null;
            // WebGL retains opaque photography while the exact coastline mask catches up.
            // Keeping its SVG paint hidden also avoids compositing a second full map during travel.
            this.shoreOverlay.style.display = 'none';
            gl.uniform1i(this.uniform('naturalShore'), Number(Boolean(colourTransform)));
            const now = performance.now();
            if (colourTransform && !this.shoreWasReady) this.shoreBlendStarted = now;
            this.shoreWasReady = Boolean(colourTransform);
            const blend = colourTransform ? Math.min(1, (now - this.shoreBlendStarted) / 350) : 0;
            gl.uniform1f(this.uniform('shoreBlend'), blend);
            gl.uniform1i(this.uniform('naturalStyle'), 1);
            gl.uniform1i(this.uniform('palettesReady'), Number(this.palettesReady));
            gl.uniform1f(this.uniform('photoPixels'), Number(this.root.dataset.resolution) || 0);
            gl.uniform1i(this.uniform('photoReady'), Number(Boolean(this.photo?.ready)));
            if (this.photo?.ready) gl.uniform4fv(this.uniform('photoBounds'), this.photo.bounds);
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
                         Number(this.detailReady && (!this.usesShoreDetail() || mapReady)));
            gl.uniform4fv(this.uniform('riverBounds'), this.riverBounds);
            gl.uniform1i(this.uniform('riverReady'), Number(this.riverReady));
            gl.uniform1i(this.uniform('riverComposite'), Number(this.riverComposite));
            gl.uniform1f(this.uniform('riverBlend'), resident?.blend ?? 1);
            gl.uniform1i(this.uniform('previousRiverReady'), Number(Boolean(resident?.previous)));
            if (resident?.previous) {
                gl.uniform4fv(this.uniform('previousRiverBounds'), resident.previous.bounds);
                gl.uniform1f(this.uniform('previousRiverInk'), resident.previous.complete ?
                    Math.max(0, Math.min(1, (2 - this.view[2] / Math.max(1, resident.previous.zoom)) * 2)) : 0);
            }
            // Retained photography remains useful at a new scale. Its rasterised minimum-width
            // river ink must not expand into thick artificial channels while refinement loads.
            gl.uniform1f(this.uniform('riverInk'), this.riverPreview ? 0 :
                Math.max(0, Math.min(1, (2 - this.view[2] / Math.max(1, this.riverZoom)) * 2)));
            gl.uniform1i(this.uniform('grid'), Number(this.grid));
            gl.uniform1i(this.uniform('mapMode'), Number(mapReady));
            gl.uniform1i(this.uniform('interactionPreview'), 0);
            gl.uniform1f(this.uniform('viewport'), size);
            gl.drawArrays(gl.TRIANGLES, 0, 6);
            this.drawBorders(size);
            this.drawMarkers(size);
            if (!this.flightTarget && ((colourTransform && blend < 1) || (resident && resident.blend < 1)))
                this.frame = requestAnimationFrame(render);
        };
        // Flight ticks already run on the animation clock; submit their camera in the same frame.
        if (immediate) render();
        else this.frame = requestAnimationFrame(render);
    }

    private drawBorders(size: number): void {
        this.borderOverlay.style.display = this.flightTarget ? 'none' : '';
        if (this.flightTarget) {
            if (this.borders && this.travelBorders) {
                const gl = this.gl;
                this.travelBorders.draw(this.rotation, this.view[2], this.canvas.width / size);
                gl.useProgram(this.program);
                gl.bindBuffer(gl.ARRAY_BUFFER, this.quad);
                gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
            }
            return;
        }
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

    private cachedCountry(key: string): GlobeCountry | undefined {
        const place = labDOM.call('lab_country_cache_get', key) as Omit<GlobeCountry, 'flag'> | null;
        if (!place) return undefined;
        const metadata = place.flagCode ? WorksheetResults.tooltip.cachedCountry(place.flagCode) : {flag: ''};
        return metadata ? {...place, flag: metadata.flag} : undefined;
    }

    private async lookupCountry(lat: number, lon: number, signal: AbortSignal, countryCode = ''): Promise<GlobeCountry> {
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
                    ok?: boolean; country?: string; country_flag?: string; country_code?: string;
                    flag_code?: string; nautical_zone?: string
                };
                if (!response.ok || !result.ok || typeof result.country !== 'string' || controller.signal.aborted)
                    throw new Error('Country unavailable');
                const flag = typeof result.country_flag === 'string' &&
                    (result.country_flag.startsWith('data:image/svg+xml;base64,') ||
                     result.country_flag === '/offshore_flag.svg') ? result.country_flag : '';
                const code = typeof result.country_code === 'string' && /^[A-Z]{2}(?:-[A-Z0-9]{1,3})?$/.test(result.country_code)
                    ? result.country_code : '';
                const nauticalZone = !code && typeof result.nautical_zone === 'string' &&
                    /^-?(?:[0-9]|1[0-2])$/.test(result.nautical_zone) ? result.nautical_zone : '';
                const flagCode = typeof result.flag_code === 'string' && /^[A-Z]{2}(?:-[A-Z0-9]{1,3})?$/.test(result.flag_code)
                    ? result.flag_code : code;
                const value = {name: result.country, flag, code, flagCode, nauticalZone};
                WorksheetResults.tooltip.rememberCountry(flagCode, value.name, flag);
                labDOM.call('lab_country_cache_put', key, value.name, code, nauticalZone, flagCode);
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
        return place ? `town:${place.town.country_code || place.jurisdiction}` :
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
            WorksheetResults.tooltip.selectedObserverFlag(cached?.flagCode || '');
        }
        if (cached || !Number.isFinite(lat) || !Number.isFinite(lon) ||
            Math.abs(lat) > 90 || Math.abs(lon) > 180) return;
        const request = new AbortController();
        if (hoverPoint) this.hoverCountryRequest = request;
        else this.countryRequest = request;
        const timeout = window.setTimeout(() => request.abort(), 5000);
        void (async () => {
            try {
                const result = await this.lookupCountry(lat, lon, request.signal, town?.town.country_code || town?.jurisdiction);
                if ((hoverPoint ? this.hoverCountryKey : this.countryKey) !== key || request.signal.aborted) return;
                if (hoverPoint) {
                    this.hoverCountry = result.name;
                    this.hoverCountryFlag = result.flag;
                } else {
                    this.country = result.name;
                    this.countryFlag = result.flag;
                    WorksheetResults.tooltip.selectedObserverFlag(result.flagCode);
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
            const available = active ? size - 8 : Math.max(point[0], size - point[0]) - 8;
            const label = place ? MarsAlmanacGlobe.townLabel(place, this.view[2], available) : null;
            const width = label ? label.width : 28;
            const positioned = labDOM.call('lab_globe_markers_place', point[0], point[1], width,
                Number(Boolean(place)), Number(active)) as [number, number, number?] | null;
            point = positioned ? [positioned[0], positioned[1]] : null;
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
            const left = positioned?.[2] === -1;
            const text = label ? left ? `${label.name} •` : `• ${label.name}` : '⊕';
            marker.classList.toggle('left-label', left);
            if (marker.textContent !== text) marker.textContent = text;
            const name = label && place ? `${label.full}, ${place.country}` : this.country ? `Observer, ${this.country}` : 'Observer';
            marker.dataset.marsTooltipTitle = `${name} · Latitude ${MarsAlmanacGlobe.coordinate(lat, 'N', 'S')} · ` +
                `Longitude ${MarsAlmanacGlobe.coordinate(lon, 'E', 'W')}`;
            marker.removeAttribute('title');
            marker.setAttribute('aria-label', marker.dataset.marsTooltipTitle);
            if (place && label) {
                marker.dataset.countryCode = place.town.country_code || place.jurisdiction;
                marker.dataset.townTooltipName = label.full;
                marker.dataset.townTooltipCoordinates = ` · Latitude ${MarsAlmanacGlobe.coordinate(lat, 'N', 'S')} · ` +
                    `Longitude ${MarsAlmanacGlobe.coordinate(lon, 'E', 'W')}`;
            }
            marker.style.maxWidth = `${width}px`;
            marker.style.width = `${width}px`;
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
                if (place.town.default) append(Number(place.town.latitude), Number(place.town.longitude), this.placeCountry(place));
            for (const place of places)
                if (!place.town.default)
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
        const zone = this.input('Zone'), previousZone = zone.value;
        let zoneEdited = false;
        const changedZone = () => { zoneEdited = true; };
        zone.addEventListener('input', changedZone);
        zone.addEventListener('change', changedZone);
        try {
            if (place) {
                // The catalogue is already resident: commit the selection before yielding to evaluation.
                jurisdiction.value = place.jurisdiction;
                labDOM.call('lab_location_populate', select,
                    WorksheetLocations.townSelection.townsForJurisdiction(place.jurisdiction), false);
                select.value = place.town.value;
                WorksheetLocations.selects.syncSelection(select);
                WorksheetLocations.selects.syncSelection(document.getElementById('almanacJurisdiction') as WorksheetSelect);
                WorksheetLocations.townSelection.applySelectedTown(WorksheetLocations.calendar.calendarTownControls('almanac'));
            } else {
                this.input('Elevation').dataset.elevationMode = 'retained';
                jurisdiction.value = '';
                labDOM.call('lab_location_populate', select, [], false);
                WorksheetLocations.selects.syncSelection(jurisdiction);
                select.value = '';
                WorksheetLocations.selects.syncSelection(select);
            }
            this.input('Latitude').value = place ? place.town.latitude : String(latitude);
            this.input('Longitude').value = place ? place.town.longitude : String(longitude);
            this.clickedObserver = [Number(this.input('Latitude').value), Number(this.input('Longitude').value)];
            // Named towns supply their reference elevation; custom points retain the authored altitude.
            this.status.textContent = place ? `Selected ${place.displayName || place.town.name}` : 'Selected custom observer location';
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
                    if (!request.signal.aborted && !zoneEdited && zone.value === previousZone &&
                        data.nauticalZone !== '' && !code) {
                        zone.value = data.nauticalZone;
                        zone.dataset.automaticZone = data.nauticalZone;
                    }
                    if (code && jurisdiction.querySelector(`option[value="${CSS.escape(code)}"]`)) {
                        jurisdiction.value = code;
                        labDOM.call('lab_location_populate', select,
                            WorksheetLocations.townSelection.townsForJurisdiction(code), false);
                        select.value = '';
                        WorksheetLocations.selects.syncSelection(select);
                        WorksheetLocations.selects.syncSelection(jurisdiction);
                        WorksheetResults.tooltip.selectedObserverFlag(data.flagCode);
                        if (!zoneEdited && zone.value === previousZone) {
                            const date = this.input('Date').value;
                            const response = await labHttp.fetch('/datetime-jurisdiction-location', {
                                method: 'POST', body: labWire.encode({jurisdiction: code, date}), signal: request.signal
                            });
                            const location = await response.labData() as WorksheetEvaluationData;
                            if (request.signal.aborted || generation !== this.selection ||
                                this.input('Latitude').value !== String(latitude) ||
                                this.input('Longitude').value !== String(longitude) ||
                                jurisdiction.value !== code || select.value || this.input('Date').value !== date) return;
                            if (response.ok && location.ok && !zoneEdited && zone.value === previousZone) {
                                // Native civil-time rules supply the date-specific offset, not town coordinates.
                                labDOM.call('lab_location_response', 6, location,
                                    WorksheetLocations.calendar.calendarLocationContext(), 0, 0);
                            }
                        }
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
        finally {
            zone.removeEventListener('input', changedZone);
            zone.removeEventListener('change', changedZone);
        }
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
                if (button.dataset.globeAction === 'grid') {
                    this.grid = !this.grid;
                    this.hideGridHover();
                    button.setAttribute('aria-pressed', String(this.grid));
                    button.textContent = `Lat/long: ${this.grid ? 'on' : 'off'}`;
                    this.draw();
                } else if (button.dataset.globeAction === 'borders') {
                    this.borders = !this.borders;
                    button.setAttribute('aria-pressed', String(this.borders));
                    button.textContent = `Borders: ${this.borders ? 'on' : 'off'}`;
                    if (this.borders) {
                        void this.loadBorders();
                        if (this.flightTarget) void this.loadTravelBorders();
                    }
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
            // During travel, catalogue hydration is not a new destination. Idle commits
            // still honour explicit location actions which request centring on the observer.
            if (this.flightTarget && this.sameCameraObserver(this.observerSignature)) {
                this.observerSignature = this.observerState();
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
