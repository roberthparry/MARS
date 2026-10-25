/**
 * @file globe_gpu.ts
 * @brief Thin WebGL backend for native-owned coverage and colour correction plans.
 *
 * No geographic coordinates, projection or shader generation lives here. The
 * backend retains GPU buffers and double-buffers coverage textures. A failed
 * render never replaces the visible texture. Eight-bit stencil batches use
 * signed non-zero winding, including concave rings and overlapping patches.
 * Native-shader colour correction palettes are rendered once per photographic
 * source, keeping neighbourhood sampling out of the animation loop.
 */
export interface GlobeMaskFrame {
    svg: string;
    vertices: Float32Array<ArrayBuffer>;
    batches: Uint32Array<ArrayBuffer>;
    projectMs?: number;
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

export class MarsGlobeMask {
    private readonly program: WebGLProgram;
    private readonly buffer: WebGLBuffer;
    private readonly framebuffer: WebGLFramebuffer;
    private readonly stencil: WebGLRenderbuffer;
    private readonly shade: WebGLUniformLocation | null;
    private spare: WebGLTexture;
    private pixels = 0;
    private capacity = 0;
    private readonly sizes = new WeakMap<WebGLTexture, number>();

    constructor(private readonly gl: WebGLRenderingContext, vertex: string, fragment: string) {
        const program = gl.createProgram(), buffer = gl.createBuffer(), framebuffer = gl.createFramebuffer();
        const stencil = gl.createRenderbuffer(), texture = gl.createTexture();
        if (!program || !buffer || !framebuffer || !stencil || !texture) {
            gl.deleteProgram(program); gl.deleteBuffer(buffer); gl.deleteFramebuffer(framebuffer);
            gl.deleteRenderbuffer(stencil); gl.deleteTexture(texture);
            throw new Error('GPU coverage allocation failed');
        }
        this.program = program; this.buffer = buffer; this.framebuffer = framebuffer;
        this.stencil = stencil; this.spare = texture;
        try {
            for (const [type, source] of [[gl.VERTEX_SHADER, vertex], [gl.FRAGMENT_SHADER, fragment]] as const) {
                const shader = gl.createShader(type);
                if (!shader) throw new Error('GPU coverage shader allocation failed');
                gl.shaderSource(shader, source); gl.compileShader(shader);
                const compiled = gl.getShaderParameter(shader, gl.COMPILE_STATUS);
                if (compiled) gl.attachShader(program, shader);
                gl.deleteShader(shader);
                if (!compiled) throw new Error('GPU coverage shader compilation failed');
            }
            gl.bindAttribLocation(program, 0, 'position');
            gl.linkProgram(program);
            if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error('GPU coverage link failed');
        } catch (error) { this.dispose(); throw error; }
        this.shade = gl.getUniformLocation(program, 'shade');
    }

    /** Publish a replacement texture only after a complete native coverage plan has been submitted. */
    render(frame: GlobeMaskFrame, pixels: number, quad: WebGLBuffer, previous: WebGLTexture): WebGLTexture {
        const gl = this.gl, {vertices, batches} = frame;
        if (!(vertices instanceof Float32Array) || !(batches instanceof Uint32Array) ||
            vertices.length % 6 || vertices.length > 2097152 || batches.length % 3 || batches.length > 49152)
            throw new Error('Invalid native GPU coverage plan');
        for (let i = 0; i < batches.length; i += 3) {
            if (batches[i] % 3 || batches[i + 1] % 3 || batches[i] + batches[i + 1] > vertices.length / 2 ||
                batches[i + 2] > 1) throw new Error('Invalid native GPU coverage range');
        }
        try {
            gl.activeTexture(gl.TEXTURE4);
            gl.bindTexture(gl.TEXTURE_2D, this.spare);
            if (this.sizes.get(this.spare) !== pixels) {
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
                gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, pixels, pixels, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
                this.sizes.set(this.spare, pixels);
            }
            gl.bindFramebuffer(gl.FRAMEBUFFER, this.framebuffer);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.spare, 0);
            if (this.pixels !== pixels) {
                gl.bindRenderbuffer(gl.RENDERBUFFER, this.stencil);
                gl.renderbufferStorage(gl.RENDERBUFFER, gl.DEPTH_STENCIL, pixels, pixels);
                gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_STENCIL_ATTACHMENT, gl.RENDERBUFFER, this.stencil);
                this.pixels = pixels;
            }
            if (gl.checkFramebufferStatus(gl.FRAMEBUFFER) !== gl.FRAMEBUFFER_COMPLETE)
                throw new Error('GPU coverage framebuffer incomplete');
            gl.viewport(0, 0, pixels, pixels);
            gl.useProgram(this.program);
            gl.disable(gl.BLEND); gl.disable(gl.CULL_FACE); gl.disable(gl.DEPTH_TEST);
            gl.enable(gl.STENCIL_TEST); gl.stencilMask(255); gl.clearStencil(0);
            gl.colorMask(true, true, true, true); gl.clearColor(0, 0, 0, 1);
            gl.clear(gl.COLOR_BUFFER_BIT | gl.STENCIL_BUFFER_BIT);
            gl.bindBuffer(gl.ARRAY_BUFFER, this.buffer);
            if (vertices.byteLength > this.capacity) {
                this.capacity = vertices.byteLength;
                gl.bufferData(gl.ARRAY_BUFFER, this.capacity, gl.DYNAMIC_DRAW);
            }
            if (vertices.length) gl.bufferSubData(gl.ARRAY_BUFFER, 0, vertices);
            gl.enableVertexAttribArray(0);
            for (let i = 0; i < batches.length; i += 3) {
                if (!batches[i + 1]) continue;
                gl.bindBuffer(gl.ARRAY_BUFFER, this.buffer);
                gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
                gl.colorMask(false, false, false, false);
                gl.stencilFunc(gl.ALWAYS, 0, 255);
                gl.stencilOpSeparate(gl.FRONT, gl.KEEP, gl.KEEP, gl.INCR_WRAP);
                gl.stencilOpSeparate(gl.BACK, gl.KEEP, gl.KEEP, gl.DECR_WRAP);
                gl.drawArrays(gl.TRIANGLES, batches[i], batches[i + 1]);
                gl.colorMask(true, true, true, true);
                gl.stencilFunc(gl.NOTEQUAL, 0, 255);
                gl.stencilOp(gl.KEEP, gl.KEEP, gl.KEEP);
                gl.bindBuffer(gl.ARRAY_BUFFER, quad);
                gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
                gl.uniform1f(this.shade, batches[i + 2]);
                gl.drawArrays(gl.TRIANGLES, 0, 6);
                gl.clear(gl.STENCIL_BUFFER_BIT);
            }
            const error = gl.getError();
            if (gl.isContextLost() || error !== gl.NO_ERROR) throw new Error(`GPU coverage draw failed (${error})`);
            const result = this.spare;
            this.spare = previous;
            return result;
        } finally {
            gl.colorMask(true, true, true, true); gl.disable(gl.STENCIL_TEST);
            gl.bindFramebuffer(gl.FRAMEBUFFER, null); gl.bindRenderbuffer(gl.RENDERBUFFER, null);
        }
    }

    /** Release retained resources; the currently published texture belongs to the globe, not this backend. */
    dispose(): void {
        const gl = this.gl;
        gl.deleteProgram(this.program); gl.deleteBuffer(this.buffer); gl.deleteFramebuffer(this.framebuffer);
        gl.deleteRenderbuffer(this.stencil); gl.deleteTexture(this.spare);
    }
}
