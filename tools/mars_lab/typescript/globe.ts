/**
 * @file globe.ts
 * @brief Browser canvas, texture loading and input adapter for the native observer globe.
 *
 * A bundled NASA surface composite supplies wide-view colour; native filled SVG shorelines replace it at deep zoom.
 * WASM owns picking, movement, history-flight interpolation and marker projection.
 * GPU sampling keeps rotation responsive while a cancellable request obtains finer shoreline imagery.
 */
type GlobeView = [number, number, number, number, number, number, number];
interface GlobeTown { town: LabTown; jurisdiction: string; country: string; fullName?: string; }

class MarsAlmanacGlobe {
    private static instance: MarsAlmanacGlobe | null = null;
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
    private readonly borderPath = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    private borderSignature = '';
    private readonly status: HTMLElement;
    private readonly gridTooltip = document.createElement('div');
    private readonly countryFlagIcon = document.createElement('img');
    private hoverPointer: PointerEvent | null = null;
    private readonly gridMarker = document.createElement('span');
    private readonly gl: WebGLRenderingContext;
    private readonly program: WebGLProgram;
    private readonly textures: WebGLTexture[] = [];
    private detailBounds = [-180, -90, 180, 90];
    private detailReady = false;
    private grid = false;
    private map = false;
    private mapReady = false;
    private mapLoading = false;
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
    private borderBounds = [-180, -90, 180, 90];
    private borderRequest: AbortController | null = null;
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
        const orientation = globe.observerSignature === globe.observerState() || globe.retainsClickedView() ? globe.orientation :
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
        this.orientation = camera.slice(0, 4);
        this.view = labDOM.call('lab_globe_view', this.orientation, camera[4], 1) as GlobeView;
        this.rotation = (labDOM.call('lab_surface_camera', 1, this.orientation, null, null, null, 0) as number[][]).flat();
        this.draw();
    }

    constructor(private readonly root: HTMLElement) {
        this.canvas = root.querySelector('canvas')!;
        this.markers = root.querySelector('.almanac-globe-markers')!;
        this.status = root.querySelector('[role="status"]')!;
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
        gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]), gl.STATIC_DRAW);
        const position = gl.getAttribLocation(this.program, 'position');
        gl.enableVertexAttribArray(position);
        gl.vertexAttribPointer(position, 2, gl.FLOAT, false, 0, 0);
        for (let i = 0; i < 4; ++i) {
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
        gl.uniform1i(gl.getUniformLocation(this.program, 'world'), 0);
        gl.uniform1i(gl.getUniformLocation(this.program, 'detail'), 1);
        gl.uniform1i(gl.getUniformLocation(this.program, 'mapWorld'), 2);
        gl.uniform1i(gl.getUniformLocation(this.program, 'rivers'), 3);
        this.events();
        document.addEventListener('lab-town-catalogue-ready', () => {
            if (!this.started) return;
            this.markerSignature = '';
            this.draw();
        });
        new ResizeObserver(() => this.draw()).observe(this.canvas);
        const visible = new IntersectionObserver(entries => {
            if (!this.started && entries.some(entry => entry.isIntersecting)) {
                this.started = true;
                if (this.observerSignature !== this.observerState()) this.centre();
                else this.move(0, 0, 1);
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
        const sources: [number, string][] = [
            [gl.VERTEX_SHADER, 'attribute vec2 position; varying vec2 screen; void main(){screen=position;gl_Position=vec4(position,0.,1.);}'],
            [gl.FRAGMENT_SHADER, `precision highp float;
                varying vec2 screen;
                uniform sampler2D world, detail, mapWorld;
                uniform sampler2D rivers;
                uniform vec4 riverBounds;
                uniform bool riverReady;
                uniform bool mapMode;
                uniform vec3 camera;
                uniform mat3 cameraToWorld;
                uniform vec4 bounds;
                uniform bool detailReady;
                uniform bool grid;
                uniform float viewport;
                const float rad=0.017453292519943295;
                // Driver inverse-trigonometric approximations can shift close-up geography by kilometres.
                // Reduce atan2 to |t|<=tan(pi/8), then use a bounded odd series in highp arithmetic.
                float geographicAngle(float y,float x){
                    vec2 a=abs(vec2(x,y));
                    float major=max(a.x,a.y);
                    if(major==0.) return 0.;
                    float t=min(a.x,a.y)/major;
                    t=t/(1.+sqrt(1.+t*t));
                    float s=t*t;
                    float series=-1./19.;
                    series=1./17.+s*series;
                    series=-1./15.+s*series;
                    series=1./13.+s*series;
                    series=-1./11.+s*series;
                    series=1./9.+s*series;
                    series=-1./7.+s*series;
                    series=1./5.+s*series;
                    series=-1./3.+s*series;
                    float angle=2.*t*(1.+s*series);
                    if(a.y>a.x) angle=1.5707963267948966-angle;
                    if(x<0.) angle=3.141592653589793-angle;
                    return y<0.?-angle:angle;
                }
                vec3 mapColour(vec3 mask,vec2 location){
                    bool land=mask.g>mask.b || mask.r>.8;
                    // At town scale use one local palette per surface, not magnified photographic variations.
                    // Unwrap around the observer view before blending so the dateline stays continuous.
                    float centreLongitude=location.x+mod(camera.y-location.x+180.,360.)-180.;
                    location=mix(location,vec2(centreLongitude,camera.x),smoothstep(8.,32.,camera.z));
                    // A moving local average retains photographic colour, not magnified raster detail.
                    // Reject opposite-surface samples so water cannot tint a crisp land boundary.
                    vec3 total=vec3(0.);
                    float weights=0.;
                    float available=0.;
                    for(int y=-2;y<=2;y++) for(int x=-2;x<=2;x++) {
                        vec2 offset=vec2(float(x),float(y));
                        vec2 sampleLocation=location+offset*.08;
                        vec2 sampleUV=vec2(fract((sampleLocation.x+180.)/360.),
                                           clamp((90.-sampleLocation.y)/180.,0.,1.));
                        vec3 sampleColour=texture2D(world,sampleUV).rgb;
                        float low=min(sampleColour.r,min(sampleColour.g,sampleColour.b));
                        float high=max(sampleColour.r,max(sampleColour.g,sampleColour.b));
                        float blue=sampleColour.b-max(sampleColour.r,sampleColour.g);
                        float ice=smoothstep(.65,.75,low)*(1.-smoothstep(.15,.22,high-low));
                        // Fade eligibility at colour thresholds: binary rejection creates visible contour patches.
                        // Opposite-surface samples still have zero weight; only land accepts neutral ice.
                        float match=land?max(smoothstep(0.,.08,-blue),ice):smoothstep(0.,.08,blue);
                        float weight=1./(1.+dot(offset,offset));
                        available+=weight;
                        total+=sampleColour*(weight*match);
                        weights+=weight*match;
                    }
                    // Missing photographic water samples must never fill a mapped channel with land colour.
                    // Fade to the matching surface fallback instead of jumping when the last sample disappears.
                    return mix(mask,total/max(weights,.000001),smoothstep(0.,1.,weights/available));
                }
                void main(){
                    vec2 p=vec2(screen.x,-screen.y)*1.12/camera.z;
                    float r=dot(p,p); if(r>1.){gl_FragColor=vec4(0.);return;}
                    float z=sqrt(max(0.,1.-r));
                    vec3 globe=cameraToWorld*vec3(p.x,-p.y,z);
                    float lat=geographicAngle(globe.y,length(globe.xz))/rad;
                    float lon=geographicAngle(globe.x,globe.z)/rad;
                    lon=mod(lon+180.,360.)-180.;
                    vec4 colour=texture2D(world,vec2((lon+180.)/360.,(90.-lat)/180.));
                    if(mapMode) colour=texture2D(mapWorld,vec2((lon+180.)/360.,(90.-lat)/180.));
                    float centre=(bounds.x+bounds.z)*.5;
                    float unwrapped=centre+mod(lon-centre+180.,360.)-180.;
                    vec2 uv=vec2((unwrapped-bounds.x)/(bounds.z-bounds.x),(bounds.w-lat)/(bounds.w-bounds.y));
                    if(detailReady && camera.z>1.5 && uv.x>=0. && uv.x<=1. && uv.y>=0. && uv.y<=1.) {
                        vec4 coast=texture2D(detail,uv);
                        colour.rgb=mix(colour.rgb,coast.rgb,mapMode?1.:coast.a*smoothstep(2.,6.,camera.z));
                    }
                    if(mapMode) colour.rgb=mapColour(colour.rgb,vec2(lon,lat));
                    float light=.8+.2*max(0.,dot(vec3(p.x,-p.y,z),normalize(vec3(-.35,.45,1.))));
                    vec3 surface=mix(colour.rgb*light,vec3(.32,.60,.88),.18*pow(1.-z,4.));
                    if(riverReady && camera.z>=12.) {
                        float rc=(riverBounds.x+riverBounds.z)*.5;
                        float rl=rc+mod(lon-rc+180.,360.)-180.;
                        vec2 ru=vec2((rl-riverBounds.x)/(riverBounds.z-riverBounds.x),
                                     (riverBounds.w-lat)/(riverBounds.w-riverBounds.y));
                        if(ru.x>=0. && ru.x<=1. && ru.y>=0. && ru.y<=1.) {
                            vec4 river=texture2D(rivers,ru);
                            if(river.a>0.) {
                                vec3 water=mapColour(vec3(7.,20.,64.)/255.,vec2(lon,lat));
                                vec3 riverSurface=mix(water*light,vec3(.32,.60,.88),.18*pow(1.-z,4.));
                                surface=mix(surface,riverSurface,river.a);
                            }
                        }
                    }
                    if(grid) {
                        float step=camera.z<4.?15.:camera.z<12.?5.:camera.z<60.?1.:camera.z<240.?.25:camera.z<1200.?.05:.01;
                        float latitudeLine=abs(mod(lat+step*.5,step)-step*.5);
                        float longitudeLine=abs(mod(lon+step*.5,step)-step*.5)*cos(lat*rad);
                        float width=128./(viewport*camera.z);
                        float line=1.-smoothstep(width*.4,width*1.4,min(latitudeLine,longitudeLine));
                        surface=mix(surface,vec3(.82,.89,.93),line*.5);
                    }
                    gl_FragColor=vec4(surface,1.);
                }`]
        ];
        for (const [type, source] of sources) {
            const shader = gl.createShader(type)!;
            gl.shaderSource(shader, source);
            gl.compileShader(shader);
            if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error('Globe shaders are unavailable.');
            gl.attachShader(program, shader);
            gl.deleteShader(shader);
        }
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

    private usesMap(): boolean { return this.map || this.view[2] >= 8; }

    private resetDetail(): void {
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
        this.rotation = (labDOM.call('lab_surface_camera', 1, this.orientation, null, null, null, 0) as number[][]).flat();
        this.view = labDOM.call('lab_globe_view', this.orientation, this.view[2], factor) as GlobeView;
        this.scheduleRiverUnload();
        if (!this.borders || this.view[2] >= 64) this.unloadBorders();
        if (wasMap !== this.usesMap()) this.resetDetail();
        this.draw();
        window.clearTimeout(this.timer);
        this.timer = window.setTimeout(() => {
            if (this.view[2] > 1.5) void this.loadDetail();
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
        if (this.highResolutionRequested || this.usesMap() || this.view[2] < 2 || !this.root.dataset.surface) return;
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
        if (this.mapReady || this.mapLoading) return;
        this.mapLoading = true;
        try {
            this.upload(2, await this.image([-180, -90, 180, 90], undefined, true));
            this.mapReady = true;
            this.draw();
        } catch (error) { if (this.usesMap()) this.status.textContent = `Shoreline unavailable; photograph retained. ${String(error)}`; }
        finally { this.mapLoading = false; }
    }

    private async loadDetail(): Promise<void> {
        if (!this.root.getClientRects().length || this.gl.isContextLost()) return;
        if (this.usesMap()) void this.loadMap();
        void this.loadHighResolution();
        const covers = (bounds: number[]): boolean => this.view[3] >= bounds[0] && this.view[4] >= bounds[1] &&
            this.view[5] <= bounds[2] && this.view[6] <= bounds[3];
        if (this.detailReady && covers(this.detailBounds) && this.view[2] <= this.detailZoom * 1.5) return;
        if (this.requestBounds) return;
        this.request?.abort();
        const generation = ++this.generation, zoom = this.view[2];
        const bounds = (labDOM.call('lab_globe_view', this.orientation, zoom / 1.25, 1) as GlobeView).slice(3);
        this.requestBounds = bounds;
        this.request = new AbortController();
        let loaded = false;
        try {
            const image = await this.image(bounds, this.request.signal);
            if (generation !== this.generation) return;
            this.detailBounds = bounds;
            this.detailZoom = zoom;
            this.detailReady = true;
            this.upload(1, image);
            loaded = true;
            this.status.textContent = 'Drag to rotate · drag vertically at either side to roll · scroll or pinch to zoom · select a town or point';
        } catch (error) {
            if (generation === this.generation) this.status.textContent = String(error);
        } finally {
            if (generation === this.generation) {
                this.requestBounds = null;
                if (loaded && this.view[2] > 1.5) {
                    window.clearTimeout(this.detailTimer);
                    this.detailTimer = window.setTimeout(() => { void this.loadDetail(); }, 300);
                }
            }
        }
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
        if (this.borderReady) labDOM.call('lab_globe_borders_clear');
        this.borderPath.setAttribute('d', '');
        this.borderSignature = '';
        this.borderReady = false;
        this.root.dataset.borders = this.borders ? 'zoom-hidden' : 'off';
    }

    private async loadBorders(): Promise<void> {
        if (!this.borders || this.view[2] >= 64 || !this.root.getClientRects().length || this.gl.isContextLost()) return;
        const zoom = this.view[2];
        const bounds = zoom <= 2 ? [-180, -90, 180, 90] :
            (labDOM.call('lab_globe_view', this.orientation, zoom / 1.15, 1) as GlobeView).slice(3);
        if (this.borderReady && this.view[3] >= this.borderBounds[0] && this.view[4] >= this.borderBounds[1] &&
            this.view[5] <= this.borderBounds[2] && this.view[6] <= this.borderBounds[3]) return;
        this.borderRequest?.abort();
        const request = this.borderRequest = new AbortController();
        const timeout = window.setTimeout(() => request.abort(), 15000);
        this.root.dataset.borders = 'loading';
        try {
            const [west, south, east, north] = bounds;
            const response = await labHttp.fetch('/almanac-borders', {
                method: 'POST', signal: request.signal,
                body: labWire.encode({west, south, east, north, format: 'segments'})
            });
            const result = await response.labData() as {ok?: boolean; segments?: string};
            if (request !== this.borderRequest || request.signal.aborted || !this.borders || this.view[2] >= 64) return;
            if (!response.ok || !result.ok || typeof result.segments !== 'string' ||
                !labDOM.call('lab_globe_borders_load', result.segments)) throw new Error('Border vectors unavailable');
            this.borderBounds = bounds; this.borderReady = true;
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

    private draw(): void {
        if (this.frame) return;
        this.frame = requestAnimationFrame(() => {
            this.frame = 0;
            const size = this.canvas.clientWidth;
            if (!size || !this.started || this.gl.isContextLost()) return;
            const pixels = Math.min(1536, Math.round(size * Math.min(devicePixelRatio, 1.5)));
            if (this.canvas.width !== pixels || this.canvas.height !== pixels)
                this.canvas.width = this.canvas.height = pixels;
            const gl = this.gl;
            gl.viewport(0, 0, pixels, pixels);
            gl.uniform3f(gl.getUniformLocation(this.program, 'camera'), this.view[0], this.view[1], this.view[2]);
            // Row-major world-to-camera values are the column-major inverse rotation required by GLSL.
            gl.uniformMatrix3fv(gl.getUniformLocation(this.program, 'cameraToWorld'), false, this.rotation);
            gl.uniform4fv(gl.getUniformLocation(this.program, 'bounds'), this.detailBounds);
            // A regional mask must not be composited as photographic detail while the map base is pending.
            const mapReady = this.usesMap() && this.mapReady;
            gl.uniform1i(gl.getUniformLocation(this.program, 'detailReady'),
                         Number(this.detailReady && (!this.usesMap() || mapReady)));
            gl.uniform4fv(gl.getUniformLocation(this.program, 'riverBounds'), this.riverBounds);
            gl.uniform1i(gl.getUniformLocation(this.program, 'riverReady'), Number(this.riverReady));
            gl.uniform1i(gl.getUniformLocation(this.program, 'grid'), Number(this.grid));
            gl.uniform1i(gl.getUniformLocation(this.program, 'mapMode'), Number(mapReady));
            gl.uniform1f(gl.getUniformLocation(this.program, 'viewport'), size);
            gl.drawArrays(gl.TRIANGLES, 0, 6);
            this.drawBorders(size);
            this.drawMarkers(size);
        });
    }

    private drawBorders(size: number): void {
        if (!this.borders || !this.borderReady || this.view[2] >= 64) return;
        const signature = `${size},${this.view[2]},${this.orientation.join(',')}`;
        if (signature === this.borderSignature) return;
        this.borderSignature = signature;
        const path = labDOM.call('lab_globe_borders_path', this.orientation, this.view[2], size) as string | null;
        this.borderOverlay.setAttribute('viewBox', `0 0 ${size} ${size}`);
        const fade = Math.max(0, Math.min(1, (this.view[2] - 24) / 40));
        this.borderPath.setAttribute('opacity', String(0.7 * (1 - fade * fade * (3 - 2 * fade))));
        this.borderPath.setAttribute('d', path || '');
        if (path === null) this.root.dataset.borders = 'unavailable';
    }

    private project(lat: number, lon: number, size: number): [number, number] | null {
        const [x, y, depth] = labDOM.call('lab_globe_project', lat, lon, this.orientation) as number[];
        const px = size * (0.5 + x * this.view[2] / 2.24), py = size * (0.5 + y * this.view[2] / 2.24);
        return depth > 0 && px >= 0 && px <= size && py >= 0 && py <= size ? [px, py] : null;
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
        const signature = `${this.orientation},${this.view[2]},${size},${this.input('Latitude').value},${this.input('Longitude').value},${townId}`;
        if (signature === this.markerSignature) return;
        this.markerSignature = signature;
        const occupied = new Set<string>(), visible = new Set<string | null>();
        const append = (lat: number, lon: number, place?: GlobeTown): void => {
            if (place && visible.has(this.townKey(place))) return;
            const point = this.project(lat, lon, size);
            if (!point) return;
            const active = Boolean(place && selected && this.townKey(place) === this.townKey(selected));
            const label = place ? MarsAlmanacGlobe.townLabel(place, this.view[2], active ? size - 8 : size - point[0]) : null;
            const width = label ? label.width : 28;
            if (active) {
                point[0] = Math.max(4, Math.min(size - width, point[0] + 14));
                point[1] = Math.max(14, Math.min(size - 14, point[1]));
            }
            if (place && (point[0] + width > size || point[1] < 14 || point[1] > size - 14)) return;
            const cells: string[] = [];
            for (let x = Math.floor((point[0] - 8) / 32); x <= Math.floor((point[0] + width) / 32); ++x)
                for (let y = Math.floor((point[1] - 15) / 24); y <= Math.floor((point[1] + 15) / 24); ++y)
                    cells.push(`${x},${y}`);
            if (place && !active && cells.some(cell => occupied.has(cell))) return;
            cells.forEach(cell => occupied.add(cell));
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
        if (this.view[2] >= 3 && !this.flightTarget) {
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
        const hundredths = Math.round(Math.abs(value) * 6000);
        const degrees = Math.floor(hundredths / 6000), minutes = (hundredths % 6000) / 100;
        return `${degrees}°${minutes ? ` ${minutes.toFixed(2)}′` : ''}${value ? ` ${value < 0 ? negative : positive}` : ''}`;
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
                    if (this.view[2] > 1.5) void this.loadDetail();
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
            this.move(0, 0, event.deltaY < 0 ? 1.15 : 1 / 1.15);
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
            if (!this.pointers.size) this.canvas.classList.remove('dragging');
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
            if (this.historyRestoring) return;
            if (this.retainsClickedView()) {
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
