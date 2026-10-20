/**
 * @file globe.ts
 * @brief Browser canvas, texture loading and input adapter for the native observer globe.
 *
 * A bundled NASA surface composite supplies wide-view colour; native filled SVG shorelines replace it at deep zoom.
 * WASM owns picking, movement and marker projection.
 * GPU sampling keeps rotation responsive while a cancellable request obtains finer shoreline imagery.
 */
type GlobeView = [number, number, number, number, number, number, number];
interface GlobeTown { town: LabTown; jurisdiction: string; country: string; }

class MarsAlmanacGlobe {
    private view: GlobeView = [0, 0, 1, -180, -90, 180, 90];
    private orientation = [0, 0, 0, 1];
    private rotation = [1, 0, 0, 0, 1, 0, 0, 0, 1];
    private readonly canvas: HTMLCanvasElement;
    private readonly markers: HTMLElement;
    private readonly status: HTMLElement;
    private readonly gridTooltip = document.createElement('div');
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
    private readonly markerNodes = new Map<GlobeTown | null, HTMLElement>();
    private markerSignature = '';
    private observerSignature = '';
    private readonly towns = new Map<string, GlobeTown[]>();
    private readonly townById = new Map<string, GlobeTown>();
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
    private request: AbortController | null = null;
    private riverRequest: AbortController | null = null;
    private riverGeneration = 0;
    private riverReady = false;
    private riverBounds = [-180, -90, 180, 90];
    private riverZoom = 0;
    private riverUnloadTimer = 0;

    constructor(private readonly root: HTMLElement) {
        this.canvas = root.querySelector('canvas')!;
        this.markers = root.querySelector('.almanac-globe-markers')!;
        this.status = root.querySelector('[role="status"]')!;
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
        document.addEventListener('lab-town-catalogue-ready', event => {
            if (!this.started) return;
            this.indexTowns((event as CustomEvent<LabTownCatalogue>).detail);
            this.markerSignature = '';
            this.draw();
        });
        new ResizeObserver(() => this.draw()).observe(this.canvas);
        const visible = new IntersectionObserver(entries => {
            if (!this.started && entries.some(entry => entry.isIntersecting)) {
                this.started = true;
                this.indexTowns();
                this.centre();
                void this.loadWorld();
            }
        });
        visible.observe(root);
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
                    // A moving local average retains photographic colour, not magnified raster detail.
                    // Reject opposite-surface samples so water cannot tint a crisp land boundary.
                    vec3 total=vec3(0.);
                    float weights=0.;
                    for(int y=-2;y<=2;y++) for(int x=-2;x<=2;x++) {
                        vec2 offset=vec2(float(x),float(y));
                        vec2 sampleLocation=location+offset*.08;
                        vec2 sampleUV=vec2(fract((sampleLocation.x+180.)/360.),
                                           clamp((90.-sampleLocation.y)/180.,0.,1.));
                        vec3 sampleColour=texture2D(world,sampleUV).rgb;
                        float low=min(sampleColour.r,min(sampleColour.g,sampleColour.b));
                        float high=max(sampleColour.r,max(sampleColour.g,sampleColour.b));
                        bool ice=low>.65 && high-low<.22;
                        bool water=sampleColour.b>sampleColour.r && sampleColour.b>sampleColour.g;
                        if(ice || (land != water)) {
                            float weight=1./(1.+dot(offset,offset));
                            total+=sampleColour*weight;
                            weights+=weight;
                        }
                    }
                    // Missing photographic water samples must never fill a mapped channel with land colour.
                    return weights>0.?total/weights:mask;
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

    private indexTowns(catalogue: LabTownCatalogue = WorksheetContext.JURISDICTION_TOWN_OPTIONS): void {
        const countries = new Intl.DisplayNames(['en-GB'], {type: 'region'});
        for (const [jurisdiction, towns] of Object.entries(catalogue)) {
            const country = countries.of(jurisdiction.split('-')[0]) || jurisdiction;
            for (const town of towns) {
                if (this.townById.has(JSON.stringify([jurisdiction, town.value]))) continue;
                const latitude = Number(town.latitude), longitude = Number(town.longitude);
                if (!Number.isFinite(latitude) || !Number.isFinite(longitude)) continue;
                const key = `${Math.floor(latitude / 5)},${Math.floor((longitude === 180 ? -180 : longitude) / 5)}`;
                const bucket = this.towns.get(key) || [];
                const place = {town, jurisdiction, country};
                bucket.push(place);
                this.townById.set(JSON.stringify([jurisdiction, town.value]), place);
                this.towns.set(key, bucket);
            }
        }
    }

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

    private centre(): void {
        const lat = this.input('Latitude').value, lon = this.input('Longitude').value;
        if (!lat.trim() || !lon.trim() || !Number.isFinite(Number(lat)) || !Number.isFinite(Number(lon)) ||
            Math.abs(Number(lat)) > 90 || Math.abs(Number(lon)) > 180) {
            this.status.textContent = 'Enter an observer location to centre the globe.';
            return;
        }
        this.orientation = labDOM.call('lab_globe_centre', Number(lat), Number(lon)) as number[];
        this.observerSignature = this.observerState();
        this.move(0, 0, 1);
    }

    private move(dx: number, dy: number, factor: number, roll = 0): void {
        this.hideGridHover();
        window.clearTimeout(this.detailTimer);
        ++this.riverGeneration;
        this.riverRequest?.abort();
        this.riverRequest = null;
        const wasMap = this.usesMap();
        this.orientation = labDOM.call('lab_globe_rotate', this.orientation, this.view[2], dx, dy, roll) as number[];
        this.rotation = (labDOM.call('lab_surface_camera', 1, this.orientation, null, null, null, 0) as number[][]).flat();
        this.view = labDOM.call('lab_globe_view', this.orientation, this.view[2], factor) as GlobeView;
        this.scheduleRiverUnload();
        if (wasMap !== this.usesMap()) this.resetDetail();
        this.draw();
        window.clearTimeout(this.timer);
        this.timer = window.setTimeout(() => {
            if (this.view[2] > 1.5) void this.loadDetail();
            void this.loadRivers();
            void labApplication.loadNearbyTowns(this.view[0], this.view[1]);
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
        } catch (error) { if (this.map) this.status.textContent = String(error); }
        finally { this.mapLoading = false; }
    }

    private async loadDetail(): Promise<void> {
        if (!this.root.getClientRects().length || this.gl.isContextLost()) return;
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
            gl.uniform1i(gl.getUniformLocation(this.program, 'detailReady'), Number(this.detailReady));
            gl.uniform4fv(gl.getUniformLocation(this.program, 'riverBounds'), this.riverBounds);
            gl.uniform1i(gl.getUniformLocation(this.program, 'riverReady'), Number(this.riverReady));
            gl.uniform1i(gl.getUniformLocation(this.program, 'grid'), Number(this.grid));
            gl.uniform1i(gl.getUniformLocation(this.program, 'mapMode'), Number(this.usesMap()));
            gl.uniform1f(gl.getUniformLocation(this.program, 'viewport'), size);
            gl.drawArrays(gl.TRIANGLES, 0, 6);
            this.drawMarkers(size);
        });
    }

    private project(lat: number, lon: number, size: number): [number, number] | null {
        const [x, y, depth] = labDOM.call('lab_globe_project', lat, lon, this.orientation) as number[];
        const px = size * (0.5 + x * this.view[2] / 2.24), py = size * (0.5 + y * this.view[2] / 2.24);
        return depth > 0 && px >= 0 && px <= size && py >= 0 && py <= size ? [px, py] : null;
    }

    private drawMarkers(size: number): void {
        const townId = JSON.stringify([this.input('Jurisdiction').value, this.input('Town').value]);
        const selected = this.townById.get(townId);
        const signature = `${this.orientation},${this.view[2]},${size},${this.input('Latitude').value},${this.input('Longitude').value},${townId}`;
        if (signature === this.markerSignature) return;
        this.markerSignature = signature;
        const occupied = new Set<string>(), visible = new Set<GlobeTown | null>();
        const append = (lat: number, lon: number, place?: GlobeTown): void => {
            if (place && visible.has(place)) return;
            const point = this.project(lat, lon, size);
            if (!point) return;
            const active = Boolean(place && place === selected);
            const width = place ? 128 : 28;
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
            const key = place || null;
            visible.add(key);
            let marker = this.markerNodes.get(key);
            if (!marker) {
                marker = document.createElement(place ? 'button' : 'span');
                marker.className = place ? 'globe-town' : 'globe-observer';
                marker.textContent = place ? `• ${place.town.name}` : '⊕';
                marker.title = place ? `${place.town.name}, ${place.country}` : 'Observer';
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
        if (this.view[2] >= 3) {
            const [, , , west, south, east, north] = this.view;
            for (let y = Math.floor(south / 5); y <= Math.floor(north / 5); ++y) {
                for (let x = Math.floor(west / 5); x <= Math.floor(east / 5); ++x) {
                    const wrapped = ((x + 36) % 72 + 72) % 72 - 36;
                    for (const place of this.towns.get(`${y},${wrapped}`) || [])
                        append(Number(place.town.latitude), Number(place.town.longitude), place);
                }
            }
        }
        for (const [key, marker] of this.markerNodes) {
            if (!visible.has(key)) { marker.remove(); this.markerNodes.delete(key); }
        }
    }

    private async select(latitude: number, longitude: number, place?: GlobeTown): Promise<void> {
        const generation = ++this.selection;
        const select = document.getElementById('almanacTown') as WorksheetSelect;
        const elevation = this.input('Elevation').value;
        try {
            if (place) {
                // The catalogue is already resident: commit the selection before yielding to evaluation.
                labDOM.call('lab_location_populate', select,
                    WorksheetLocations.townSelection.townsForJurisdiction(place.jurisdiction), false);
                (document.getElementById('almanacJurisdiction') as WorksheetSelect).value = place.jurisdiction;
                select.value = place.town.value;
                WorksheetLocations.selects.syncSelection(select);
                WorksheetLocations.selects.syncSelection(document.getElementById('almanacJurisdiction') as WorksheetSelect);
                WorksheetLocations.townSelection.applySelectedTown(WorksheetLocations.calendar.calendarTownControls('almanac', false));
                this.input('Elevation').value = elevation;
            } else {
                select.value = '';
                WorksheetLocations.selects.syncSelection(select);
            }
            this.input('Latitude').value = place ? place.town.latitude : String(latitude);
            this.input('Longitude').value = place ? place.town.longitude : String(longitude);
            // Deliberately leave the authored observer altitude untouched.
            this.status.textContent = place ? `Selected ${place.town.name}` : 'Selected custom observer location';
            this.observerSignature = this.observerState();
            this.draw();
            await WorksheetLocations.calendarActions.triggerAlmanacAutoEvaluation();
        } catch (error) { if (generation === this.selection) this.status.textContent = String(error); }
    }

    private pick(x: number, y: number): void {
        const box = this.canvas.getBoundingClientRect(), radius = box.width * this.view[2] / 2.24;
        const point = labDOM.call('lab_globe_pick', (x - box.left - box.width / 2) / radius,
            (y - box.top - box.height / 2) / radius, this.orientation) as number[] | null;
        if (point) void this.select(point[0], point[1]);
    }

    private hideGridHover(): void {
        this.gridTooltip.hidden = this.gridMarker.hidden = true;
        this.canvas.classList.remove('grid-hover');
    }

    private hoverGrid(event: PointerEvent): void {
        this.hideGridHover();
        if (!this.grid || this.pointers.size || event.target !== this.canvas) return;
        const box = this.canvas.getBoundingClientRect(), radius = box.width * this.view[2] / 2.24;
        const point = labDOM.call('lab_globe_pick', (event.clientX - box.left - box.width / 2) / radius,
            (event.clientY - box.top - box.height / 2) / radius, this.orientation) as number[] | null;
        if (!point) return;
        const [latitude, longitude, flags] = labDOM.call('lab_globe_grid', point[0], point[1], this.view[2], box.width) as number[];
        if (!flags) return;
        const marker = this.project(flags & 1 ? latitude : point[0], flags & 2 ? longitude : point[1], box.width);
        if (!marker) return;
        const labels: string[] = [];
        if (flags & 1) labels.push(`Latitude ${MarsAlmanacGlobe.coordinate(latitude, 'N', 'S')}`);
        if (flags & 2) labels.push(`Longitude ${MarsAlmanacGlobe.coordinate(longitude, 'E', 'W')}`);
        this.gridTooltip.textContent = labels.join(' · ');
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
        viewport.addEventListener('pointermove', event => this.hoverGrid(event));
        viewport.addEventListener('pointerleave', () => this.hideGridHover());
        this.root.querySelectorAll<HTMLButtonElement>('[data-globe-action]').forEach(button => {
            button.addEventListener('click', () => {
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
            event.preventDefault(); ++this.generation; this.request?.abort();
            window.clearTimeout(this.timer);
            this.status.textContent = 'Globe graphics interrupted. Reload to restore; observer fields remain available.';
        });
        document.addEventListener('mars-almanac-observer-committed', () => {
            if (this.observerSignature !== this.observerState()) {
                ++this.selection;
                this.centre();
            }
        });
        for (const id of ['almanacLatitude', 'almanacLongitude', 'almanacTown', 'almanacJurisdiction'])
            document.getElementById(id)?.addEventListener('change', () => {
                ++this.selection;
                requestAnimationFrame(() => this.centre());
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
