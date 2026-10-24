/**
 * @file globe_gpu_checks.ts
 * @brief Real-GPU agreement checks for native coverage plans and visible SVG.
 *
 * Rasterising SVG is a test oracle only, never the production mask path. Tests
 * cover concavity, nesting, reversed/overlapping rings, horizon arcs, polar and
 * date-line cameras, empty coverage, repeated rendering and resource reuse.
 */
async function checkGlobeGPU(check: (condition: boolean, message: string) => void): Promise<void> {
    const moduleURL = '/js/globe_gpu.js';
    const {MarsGlobeMask} = await import(moduleURL) as typeof import('../../typescript/globe_gpu.js');
    const native = labWire.exports(), canvas = document.createElement('canvas');
    const gl = canvas.getContext('webgl')!;
    check(Boolean(gl), 'GPU mask test has a WebGL context');
    const renderer = new MarsGlobeMask(gl, labDOM.call('lab_globe_mask_shader', 0) as string,
                                     labDOM.call('lab_globe_mask_shader', 1) as string);
    const quad = gl.createBuffer()!, read = gl.createFramebuffer()!;
    let texture = gl.createTexture()!;
    gl.bindBuffer(gl.ARRAY_BUFFER, quad);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]), gl.STATIC_DRAW);
    const square = 'M1 0 -25 -25\nL25 -25\nL25 25\nL-25 25\nZ\n';
    const reverse = 'M1 0 -25 -25\nL-25 25\nL25 25\nL25 -25\nZ\n';
    const fixtures: [string, number[], number, number[]?][] = [
        [square, [0, 0, 0, 1], 1],
        [square + 'M2 0 -15 -15\nL15 -15\nL15 15\nL-15 15\nZ\n' +
            'M3 0 -5 -5\nL5 -5\nL5 5\nL-5 5\nZ\n', [0, 0, 0, 1], 2],
        ['M1 0 -25 -25\nL25 -25\nL25 -5\nL-5 -5\nL-5 25\nL-25 25\nZ\n', [0, 0, 0, 1], 2],
        [(square + reverse).repeat(128), [0, 0, 0, 1], 2],
        ['M1 0 70 -30\nL110 -30\nL110 30\nL70 30\nZ\n', [0, 0, 0, 1], 1],
        ['M1 1 -110 -90\nL-70 -90\nL-70 -60\nL-110 -60\nZ\n',
            labDOM.call('lab_globe_centre', -70, -90) as number[], 4],
        ['M1 0 170 -15\nL190 -15\nL190 15\nL170 15\nZ\n',
            labDOM.call('lab_globe_centre', 0, 180) as number[], 4, [170, -90, 190, 90]],
        ['', [0, 0, 0, 1], 1]
    ];
    try {
        for (const [text, orientation, zoom, bounds] of fixtures) {
            check(Boolean(labDOM.call('lab_globe_shore_load', text, bounds || [-180, -90, 180, 90])),
                  'GPU fixture admitted');
            const began = performance.now();
            const svg = labDOM.call('lab_globe_shore_frame', orientation, zoom, 800) as string;
            check(typeof svg === 'string', 'one native projection supplies SVG and binary coverage');
            const vertices = new Float32Array(native.memory.buffer, native.lab_globe_mask_vertices(),
                native.lab_globe_mask_vertex_count() * 2).slice();
            const batches = new Uint32Array(native.memory.buffer, native.lab_globe_mask_batches(),
                native.lab_globe_mask_batch_count() * 3).slice();
            const frame = {svg, vertices, batches}, projectMs = performance.now() - began;
            const submit = performance.now();
            texture = renderer.render(frame, 256, quad, texture);
            texture = renderer.render(frame, 256, quad, texture);
            const submitMs = (performance.now() - submit) / 2;
            gl.bindFramebuffer(gl.FRAMEBUFFER, read);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, texture, 0);
            const actual = new Uint8Array(256 * 256 * 4);
            gl.readPixels(0, 0, 256, 256, gl.RGBA, gl.UNSIGNED_BYTE, actual);
            gl.bindFramebuffer(gl.FRAMEBUFFER, null);
            const reference = document.createElement('canvas'); reference.width = reference.height = 256;
            const context = reference.getContext('2d')!;
            context.fillStyle = 'black'; context.fillRect(0, 0, 256, 256);
            const image = new Image(), url = URL.createObjectURL(new Blob([
                '<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="-400 -400 1600 1600">' +
                '<style>circle,path{fill:black;stroke:none}path:not([fill="#071440"]){fill:white}</style>' + svg + '</svg>'
            ], {type: 'image/svg+xml'}));
            try { image.src = url; await image.decode(); context.drawImage(image, 0, 0); }
            finally { URL.revokeObjectURL(url); }
            const expected = context.getImageData(0, 0, 256, 256).data;
            let differences = 0, actualLand = 0, expectedLand = 0;
            for (let i = 0; i < actual.length; i += 4) {
                if (actual[i] > 128) ++actualLand;
                if (expected[i] > 128) ++expectedLand;
                if ((expected[i] < 15 || expected[i] > 240) && Math.abs(actual[i] - expected[i]) > 128) {
                    const pixel = i / 4, x = pixel % 256, y = Math.floor(pixel / 256);
                    let edge = false;
                    // Repeated SVG alpha compositing expands fractional edge pixels towards opacity.
                    // Compare interiors exactly, allowing only a one-pixel boundary neighbourhood.
                    for (let dy = -1; dy <= 1; ++dy) for (let dx = -1; dx <= 1; ++dx) {
                        if (x + dx < 0 || x + dx >= 256 || y + dy < 0 || y + dy >= 256) continue;
                        const neighbour = ((y + dy) * 256 + x + dx) * 4;
                        if (Math.abs(expected[neighbour] - expected[i]) > 32 ||
                            Math.abs(actual[neighbour] - actual[i]) > 128) edge = true;
                    }
                    if (!edge) ++differences;
                }
            }
            check(differences === 0, `GPU coverage agrees with SVG, allowing boundary antialiasing ` +
                `(${differences} pixels, land ${actualLand}/${expectedLand}, zoom ${zoom}, geometry ${text.slice(0, 40)})`);
            check(gl.getError() === gl.NO_ERROR, 'GPU mask leaves no WebGL errors');
            console.log(`GPU MASK: ${vertices.length / 2} vertices, ${batches.length / 3} batches, ` +
                        `${projectMs.toFixed(2)}ms native projection, ${submitMs.toFixed(2)}ms CPU submission`);
        }
    } finally {
        renderer.dispose(); gl.deleteTexture(texture); gl.deleteBuffer(quad); gl.deleteFramebuffer(read);
        gl.getExtension('WEBGL_lose_context')?.loseContext();
        labDOM.call('lab_globe_shore_clear');
    }
}
