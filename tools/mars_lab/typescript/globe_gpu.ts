/**
 * @file globe_gpu.ts
 * @brief Thin WebGL backend for native-owned coverage, travel borders and colour correction plans.
 *
 * No geographic coordinates, projection or shader generation lives here. The
 * backend retains GPU buffers and double-buffers coverage textures. A failed
 * render never replaces the visible texture. Eight-bit stencil batches use
 * signed non-zero winding, including concave rings and overlapping patches.
 * Native-shader colour correction palettes are rendered once per photographic
 * source, keeping neighbourhood sampling out of the animation loop.
 * Travel borders retain one native-generated vertex buffer and rotate it using shader uniforms.
 */
export interface GlobeMaskFrame {
    svg: string;
    vertices: Float32Array<ArrayBuffer>;
    batches: Uint32Array<ArrayBuffer>;
    projectMs?: number;
}

/** Rotate native world-border vertices on the GPU without per-frame SVG generation. */
export class MarsGlobeBorders {
    private readonly program: WebGLProgram;
    private readonly buffer: WebGLBuffer;
    private readonly camera: WebGLUniformLocation | null;
    private readonly zoom: WebGLUniformLocation | null;
    private readonly count: number;

    constructor(private readonly gl: WebGLRenderingContext, vertex: string, fragment: string, vertices: Float32Array) {
        if (!(vertices instanceof Float32Array) || !vertices.length || vertices.length % 6 || vertices.length > 6000000)
            throw new Error('Invalid border mesh');
        const program = gl.createProgram(), buffer = gl.createBuffer();
        if (!program || !buffer) {
            gl.deleteProgram(program); gl.deleteBuffer(buffer);
            throw new Error('GPU borders unavailable');
        }
        this.program = program; this.buffer = buffer; this.count = vertices.length / 3;
        try {
            for (const [type, source] of [[gl.VERTEX_SHADER, vertex], [gl.FRAGMENT_SHADER, fragment]] as const) {
                const shader = gl.createShader(type);
                if (!shader) throw new Error('Border shader allocation failed');
                gl.shaderSource(shader, source); gl.compileShader(shader);
                const compiled = gl.getShaderParameter(shader, gl.COMPILE_STATUS);
                if (compiled) gl.attachShader(program, shader);
                gl.deleteShader(shader);
                if (!compiled) throw new Error('Border shader compilation failed');
            }
            gl.bindAttribLocation(program, 0, 'position');
            gl.linkProgram(program);
            if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error('Border shader link failed');
            gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
            gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
        } catch (error) { this.dispose(); throw error; }
        this.camera = gl.getUniformLocation(program, 'cameraToWorld');
        this.zoom = gl.getUniformLocation(program, 'zoom');
    }

    draw(rotation: number[], zoom: number, pixelRatio: number): void {
        const gl = this.gl;
        gl.useProgram(this.program);
        gl.bindBuffer(gl.ARRAY_BUFFER, this.buffer);
        gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 0, 0);
        gl.uniformMatrix3fv(this.camera, false, rotation);
        gl.uniform1f(this.zoom, zoom);
        gl.enable(gl.BLEND);
        gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
        gl.lineWidth(pixelRatio);
        gl.drawArrays(gl.LINES, 0, this.count);
        gl.disable(gl.BLEND);
    }

    dispose(): void {
        this.gl.deleteProgram(this.program); this.gl.deleteBuffer(this.buffer);
    }
}

/** Build native-shader colour lookup textures once per photographic source, outside animation frames. */
export function buildGlobePalettes(gl: WebGLRenderingContext, program: WebGLProgram, resolution = 2048): WebGLTexture[] {
    if (!Number.isInteger(resolution) || resolution < 32 || resolution > 2048 || (resolution & (resolution - 1)))
        throw new Error('Invalid GPU palette dimensions');
    const framebuffer = gl.createFramebuffer();
    if (!framebuffer) throw new Error('GPU palette framebuffer unavailable');
    const viewport = gl.getParameter(gl.VIEWPORT) as Int32Array;
    const active = gl.getParameter(gl.ACTIVE_TEXTURE) as number;
    const textures: WebGLTexture[] = [];
    gl.activeTexture(gl.TEXTURE7);
    const photo = gl.getParameter(gl.TEXTURE_BINDING_2D) as WebGLTexture | null;
    const pass = gl.getUniformLocation(program, 'palettePass');
    try {
        gl.useProgram(program);
        gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
        const width = Math.min(resolution, gl.getParameter(gl.MAX_TEXTURE_SIZE) as number);
        gl.viewport(0, 0, width, width / 2);
        // Restore the input binding before drawing: the output must never feed back into a sampler.
        gl.activeTexture(gl.TEXTURE7);
        for (let i = 0; i < 2; ++i) {
            const texture = gl.createTexture();
            if (!texture) throw new Error('GPU palette allocation unavailable');
            textures.push(texture);
            gl.bindTexture(gl.TEXTURE_2D, texture);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, width / 2, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, texture, 0);
            gl.bindTexture(gl.TEXTURE_2D, photo);
            if (gl.checkFramebufferStatus(gl.FRAMEBUFFER) !== gl.FRAMEBUFFER_COMPLETE)
                throw new Error('GPU palette framebuffer incomplete');
            gl.uniform1i(pass, i + 1);
            gl.drawArrays(gl.TRIANGLES, 0, 6);
            if (gl.getError() !== gl.NO_ERROR || gl.isContextLost()) throw new Error('GPU palette draw unavailable');
        }
        return textures;
    } catch (error) {
        textures.forEach(texture => gl.deleteTexture(texture));
        throw error;
    } finally {
        gl.uniform1i(pass, 0);
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.deleteFramebuffer(framebuffer);
        gl.bindTexture(gl.TEXTURE_2D, photo);
        gl.activeTexture(active);
        gl.viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    }
}

/** Browser object table and API forwarding only; marslib owns allocation and rendering decisions. */
class GlobeGPUBridge {
    private readonly objects: (object | null)[] = [null];
    private readonly free: number[] = [];
    private readonly operations: ((a: number, b: number, c: number, d: number) => unknown)[];
    private failed = false;
    vertices: Float32Array<ArrayBuffer> | null = null;

    constructor(private readonly gl: WebGLRenderingContext, vertex: string, fragment: string) {
        const create = [() => gl.createProgram(), () => gl.createBuffer(), () => gl.createFramebuffer(),
            () => gl.createRenderbuffer(), () => gl.createTexture(), () => gl.createShader(gl.VERTEX_SHADER),
            () => gl.createShader(gl.FRAGMENT_SHADER)];
        const remove = [
            (id: number) => gl.deleteProgram(this.get<WebGLProgram>(id)),
            (id: number) => gl.deleteBuffer(this.get<WebGLBuffer>(id)),
            (id: number) => gl.deleteFramebuffer(this.get<WebGLFramebuffer>(id)),
            (id: number) => gl.deleteRenderbuffer(this.get<WebGLRenderbuffer>(id)),
            (id: number) => gl.deleteTexture(this.get<WebGLTexture>(id)),
            (id: number) => gl.deleteShader(this.get<WebGLShader>(id)),
            (id: number) => gl.deleteShader(this.get<WebGLShader>(id)),
            (_id: number) => {}
        ];
        // Direct-indexed ABI from webgl-t.h. No projection, shader policy, draw ordering or cache decisions.
        this.operations = [
            id => {
                gl.bindBuffer(gl.ARRAY_BUFFER, this.get<WebGLBuffer>(id));
                gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
            },
            enabled => gl.colorMask(Boolean(enabled), Boolean(enabled), Boolean(enabled), Boolean(enabled)),
            (func, ref, mask) => gl.stencilFunc(func, ref, mask),
            (face, fail, depthFail, pass) => gl.stencilOpSeparate(face, fail, depthFail, pass),
            (fail, depthFail, pass) => gl.stencilOp(fail, depthFail, pass),
            (mode, first, count) => gl.drawArrays(mode, first, count),
            (shade, id) => gl.uniform1f(this.get<WebGLUniformLocation>(id), shade),
            mask => gl.clear(mask),
            kind => this.adopt(create[kind]()),
            (kind, id) => {
                try { remove[kind](id); }
                finally { this.objects[id] = null; this.free.push(id); }
            },
            (id, source) => gl.shaderSource(this.get<WebGLShader>(id), source ? fragment : vertex),
            id => gl.compileShader(this.get<WebGLShader>(id)),
            id => gl.getShaderParameter(this.get<WebGLShader>(id), gl.COMPILE_STATUS),
            (program, shader) => gl.attachShader(this.get<WebGLProgram>(program), this.get<WebGLShader>(shader)),
            id => gl.bindAttribLocation(this.get<WebGLProgram>(id), 0, 'position'),
            id => gl.linkProgram(this.get<WebGLProgram>(id)),
            id => gl.getProgramParameter(this.get<WebGLProgram>(id), gl.LINK_STATUS),
            id => this.adopt(gl.getUniformLocation(this.get<WebGLProgram>(id), 'shade')),
            unit => gl.activeTexture(unit),
            id => gl.bindTexture(gl.TEXTURE_2D, this.get<WebGLTexture>(id)),
            (name, value) => gl.texParameteri(gl.TEXTURE_2D, name, value),
            pixels => gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, pixels, pixels, 0, gl.RGBA, gl.UNSIGNED_BYTE, null),
            id => gl.bindFramebuffer(gl.FRAMEBUFFER, this.get<WebGLFramebuffer>(id)),
            id => gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D,
                                         this.get<WebGLTexture>(id), 0),
            id => gl.bindRenderbuffer(gl.RENDERBUFFER, this.get<WebGLRenderbuffer>(id)),
            pixels => gl.renderbufferStorage(gl.RENDERBUFFER, gl.DEPTH_STENCIL, pixels, pixels),
            id => gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_STENCIL_ATTACHMENT, gl.RENDERBUFFER,
                                            this.get<WebGLRenderbuffer>(id)),
            () => gl.checkFramebufferStatus(gl.FRAMEBUFFER) === gl.FRAMEBUFFER_COMPLETE,
            pixels => gl.viewport(0, 0, pixels, pixels),
            id => gl.useProgram(this.get<WebGLProgram>(id)),
            capability => gl.disable(capability),
            capability => gl.enable(capability),
            mask => gl.stencilMask(mask),
            value => gl.clearStencil(value),
            (r, g, b, a) => gl.clearColor(r, g, b, a),
            id => gl.bindBuffer(gl.ARRAY_BUFFER, this.get<WebGLBuffer>(id)),
            bytes => gl.bufferData(gl.ARRAY_BUFFER, bytes, gl.DYNAMIC_DRAW),
            () => gl.bufferSubData(gl.ARRAY_BUFFER, 0, this.vertices!),
            index => gl.enableVertexAttribArray(index),
            () => {
                const error = gl.getError(), failed = this.failed;
                this.failed = false;
                return error || (failed ? gl.INVALID_OPERATION : gl.NO_ERROR);
            },
            () => gl.isContextLost()
        ];
    }

    adopt(object: object | null): number {
        if (!object) return 0;
        const id = this.free.pop() ?? this.objects.length;
        this.objects[id] = object;
        return id;
    }

    get<T>(id: number): T { return this.objects[id] as T; }

    invoke(operation: number, a: number, b: number, c: number, d: number): number {
        try { return Number(this.operations[operation](a, b, c, d) ?? 0); }
        catch { this.failed = true; return 0; }
    }
}

/** Thin adapter for marslib's native GPU resource owner. */
export class MarsGlobeMask {
    private readonly bridge: GlobeGPUBridge;
    private token: number;
    private quad: WebGLBuffer | null = null;
    private quadId = 0;
    private published: WebGLTexture | null = null;
    private publishedId = 0;
    private initialPrevious: WebGLTexture | null = null;
    private initialPreviousId = 0;

    constructor(gl: WebGLRenderingContext, vertex: string, fragment: string) {
        this.bridge = new GlobeGPUBridge(gl, vertex, fragment);
        this.token = labDOM.call('lab_webgl_renderer_create', this.bridge) as number;
        if (!this.token) throw new Error('Native GPU coverage allocation failed');
    }

    render(frame: GlobeMaskFrame, pixels: number, quad: WebGLBuffer, previous: WebGLTexture): WebGLTexture {
        const {vertices, batches} = frame;
        if (!this.token || !(vertices instanceof Float32Array) || !(batches instanceof Uint32Array) ||
            vertices.length % 6 || vertices.length > 2097152 || !Number.isInteger(pixels) || pixels < 1 || pixels > 8192)
            throw new Error('Invalid native GPU coverage plan');
        if (quad !== this.quad) {
            if (this.quad) throw new Error('GPU coverage quad changed');
            this.quad = quad;
            this.quadId = this.bridge.adopt(quad);
        }
        if (!this.published && !this.initialPrevious) {
            this.initialPrevious = previous;
            this.initialPreviousId = this.bridge.adopt(previous);
        }
        const previousId = previous === this.published ? this.publishedId :
            previous === this.initialPrevious ? this.initialPreviousId : 0;
        if (!previousId) throw new Error('Unexpected GPU coverage texture owner');
        this.bridge.vertices = vertices;
        try {
            const id = labDOM.call('lab_webgl_renderer_draw', this.bridge, this.token,
                new Uint8Array(batches.buffer, batches.byteOffset, batches.byteLength), vertices.length / 2,
                pixels, this.quadId, previousId) as number;
            if (!id) throw new Error('Native GPU coverage draw failed');
            this.publishedId = id;
            this.published = this.bridge.get<WebGLTexture>(id);
            this.initialPrevious = null;
            this.initialPreviousId = 0;
            return this.published;
        } finally { this.bridge.vertices = null; }
    }

    dispose(): void {
        if (this.token) labDOM.call('lab_webgl_renderer_free', this.bridge, this.token);
        this.token = 0;
    }
}
