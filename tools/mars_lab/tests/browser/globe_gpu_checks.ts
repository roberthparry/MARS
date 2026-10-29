/**
 * @file globe_gpu_checks.ts
 * @brief Real-GPU agreement checks for native coverage plans and visible SVG.
 *
 * Rasterising SVG is a test oracle only, never the production mask path. Tests
 * cover concavity, nesting, reversed/overlapping rings, horizon arcs, polar and
 * date-line cameras, deep zoom, dense outlines, empty coverage, repeated rendering
 * and resource reuse. Interior cracks cannot excuse themselves as boundary pixels.
 */
async function checkGlobeGPU(check: (condition: boolean, message: string) => void): Promise<void> {
    await checkGlobeBordersGPU(check);
    const moduleURL = '/js/globe_gpu.js';
    const {MarsGlobeMask} = await import(moduleURL) as typeof import('../../typescript/globe_gpu.js');
    const native = labWire.exports(), canvas = document.createElement('canvas');
    const planFor = (words: number[], vertices: number) => {
        const batches = new Uint32Array(words);
        return labDOM.call('lab_webgl_mask_commands', new Uint8Array(batches.buffer), vertices) as
            {pointer: number; count: number} | null;
    };
    check(planFor([], 0)?.count === 0, 'native GPU command planner accepts empty coverage');
    for (const words of [[1, 3, 1], [0, 4, 1], [0, 6, 1], [0, 3, 2], [0, 3, 1, 3, 3, 0], [0, 3]])
        check(planFor(words, 3) === null, 'native GPU planner rejects malformed or out-of-range batches atomically');
    const planned = planFor([0, 3, 1, 3, 3, 0], 6)!;
    const commands = new Uint32Array(native.memory.buffer, planned.pointer, planned.count * 5);
    check(planned.count === 26 && commands[10 * 5 + 1] === 1 && commands[23 * 5 + 1] === 0 &&
          commands[12 * 5] === 7 && commands[25 * 5] === 7,
          'native GPU policy paints land then water and clears stencil after each pass');
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
        [square, labDOM.call('lab_globe_centre', 1.321, 3.123) as number[], 64],
        [square, labDOM.call('lab_globe_centre', -2.345, 4.567) as number[], 256],
        [square, labDOM.call('lab_globe_centre', 3.456, -1.234) as number[], 4096],
        [reverse, [0, 0, 0, 1], 64],
        [square + 'M2 0 -1 -25\nL1 -25\nL1 25\nL-1 25\nZ\n' +
            'M3 0 -0.1 -0.1\nL0.1 -0.1\nL0.1 0.1\nL-0.1 0.1\nZ\n',
            [0, 0, 0, 1], 64],
        ['M1 0 80 -25\nL100 -25\nL100 25\nL80 25\nZ\n', [0, 0, 0, 1], 16],
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
    let dense = '';
    for (let i = 0; i < 4096; ++i) {
        const angle = i * 2 * Math.PI / 4096, radius = 20 + 5 * Math.sin(angle * 11);
        dense += `${i ? 'L' : 'M1 0 '}${(radius * Math.cos(angle)).toFixed(9)} ${(radius * Math.sin(angle)).toFixed(9)}\n`;
    }
    dense += 'Z\n';
    for (const zoom of [16, 32, 40, 48, 64, 256])
        fixtures.push([dense, labDOM.call('lab_globe_centre', 1.321, 3.123) as number[], zoom]);
    try {
        for (const [text, orientation, zoom, bounds] of fixtures) {
            const pixels = 256;
            check(Boolean(labDOM.call('lab_globe_shore_load', text, bounds || [-180, -90, 180, 90])),
                  'GPU fixture admitted');
            const began = performance.now();
            const svg = labDOM.call('lab_globe_shore_frame', orientation, zoom, 800) as string;
            check(typeof svg === 'string', 'one native projection supplies SVG and binary coverage');
            const vertices = new Float32Array(native.memory.buffer, native.lab_globe_mask_vertices(),
                native.lab_globe_mask_vertex_count() * 2).slice();
            if (zoom >= 8)
                check(vertices.every(coordinate => Math.abs(coordinate) <= 1),
                      'close-view native geometry is clipped before GPU float conversion');
            const batches = new Uint32Array(native.memory.buffer, native.lab_globe_mask_batches(),
                native.lab_globe_mask_batch_count() * 3).slice();
            // Match projected scale with a larger, low-zoom native SVG. Its path
            // bypasses viewport clipping, giving an independent clipping oracle.
            const referenceSize = zoom >= 8 && zoom <= 128 ? 800 * zoom / 7 : 800;
            const referenceSVG = referenceSize === 800 ? svg :
                `<g transform="translate(${(800 - referenceSize) / 2} ${(800 - referenceSize) / 2})">` +
                labDOM.call('lab_globe_shore_svg', orientation, 7, referenceSize) + '</g>';
            const frame = {svg, vertices, batches}, projectMs = performance.now() - began;
            const submit = performance.now();
            texture = renderer.render(frame, pixels, quad, texture);
            texture = renderer.render(frame, pixels, quad, texture);
            const submitMs = (performance.now() - submit) / 2;
            gl.bindFramebuffer(gl.FRAMEBUFFER, read);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, texture, 0);
            const actual = new Uint8Array(pixels * pixels * 4);
            gl.readPixels(0, 0, pixels, pixels, gl.RGBA, gl.UNSIGNED_BYTE, actual);
            gl.bindFramebuffer(gl.FRAMEBUFFER, null);
            const reference = document.createElement('canvas'); reference.width = reference.height = pixels;
            const context = reference.getContext('2d')!;
            context.fillStyle = 'black'; context.fillRect(0, 0, pixels, pixels);
            const image = new Image(), url = URL.createObjectURL(new Blob([
                `<svg xmlns="http://www.w3.org/2000/svg" width="${pixels}" height="${pixels}" viewBox="-400 -400 1600 1600">` +
                '<style>circle,path{fill:black;stroke:none}path:not([fill="#071440"]){fill:white}</style>' + referenceSVG + '</svg>'
            ], {type: 'image/svg+xml'}));
            try { image.src = url; await image.decode(); context.drawImage(image, 0, 0); }
            finally { URL.revokeObjectURL(url); }
            const expected = context.getImageData(0, 0, pixels, pixels).data;
            let differences = 0, actualLand = 0, expectedLand = 0;
            const differenceLocations: string[] = [];
            for (let i = 0; i < actual.length; i += 4) {
                if (actual[i] > 128) ++actualLand;
                if (expected[i] > 128) ++expectedLand;
                if ((expected[i] === 0 || expected[i] === 255) && Math.abs(actual[i] - expected[i]) > 128) {
                    const pixel = i / 4, x = pixel % pixels, y = Math.floor(pixel / pixels);
                    let edge = false;
                    // Repeated SVG alpha compositing expands fractional edge pixels towards opacity.
                    // Compare interiors exactly, allowing only a one-pixel boundary neighbourhood.
                    for (let dy = -1; dy <= 1; ++dy) for (let dx = -1; dx <= 1; ++dx) {
                        if (x + dx < 0 || x + dx >= pixels || y + dy < 0 || y + dy >= pixels) continue;
                        const neighbour = ((y + dy) * pixels + x + dx) * 4;
                        if (Math.abs(expected[neighbour] - expected[i]) > 32) edge = true;
                    }
                    if (!edge) {
                        ++differences;
                        if (differenceLocations.length < 12) differenceLocations.push(`${x},${y}:${actual[i]}/${expected[i]}`);
                    }
                }
            }
            check(differences === 0, `GPU coverage agrees with SVG, allowing boundary antialiasing ` +
                `(${differences} pixels, land ${actualLand}/${expectedLand}, zoom ${zoom}, ${differenceLocations.join(';')})`);
            check(gl.getError() === gl.NO_ERROR, 'GPU mask leaves no WebGL errors');
            console.log(`GPU MASK: ${vertices.length / 2} vertices, ${batches.length / 3} batches, ` +
                        `${projectMs.toFixed(2)}ms native projection, ${submitMs.toFixed(2)}ms CPU submission`);
        }
        let rejected = false;
        try { renderer.render({svg: '', vertices: new Float32Array(6), batches: new Uint32Array([0, 6, 1])},
                              256, quad, texture); }
        catch { rejected = true; }
        check(rejected && gl.isTexture(texture) && gl.getError() === gl.NO_ERROR,
              'invalid native frame preserves the published GPU texture without changing GPU state');
        renderer.dispose();
        renderer.dispose();
        check(gl.isTexture(texture) && gl.isBuffer(quad), 'native disposal preserves published texture and borrowed quad');
        for (let i = 0; i < 32; ++i) {
            const temporary = new MarsGlobeMask(gl, labDOM.call('lab_globe_mask_shader', 0) as string,
                                               labDOM.call('lab_globe_mask_shader', 1) as string);
            temporary.dispose();
        }
        check(gl.getError() === gl.NO_ERROR, 'native GPU owner slots and handles are reusable beyond the slot limit');
    } finally {
        renderer.dispose(); gl.deleteTexture(texture); gl.deleteBuffer(quad); gl.deleteFramebuffer(read);
        gl.getExtension('WEBGL_lose_context')?.loseContext();
        labDOM.call('lab_globe_shore_clear');
    }
}

/** Verify visible pixels, camera rotation and hidden-hemisphere clipping with the real GPU border shaders. */
async function checkGlobeBordersGPU(check: (condition: boolean, message: string) => void): Promise<void> {
    const moduleURL = '/js/globe_gpu.js';
    const {MarsGlobeBorders} = await import(moduleURL) as typeof import('../../typescript/globe_gpu.js');
    const count = labDOM.call('lab_globe_borders_mesh', '-10 0 10 0\n') as number;
    const native = labWire.exports();
    check(count > 2 && count % 2 === 0, 'native GPU borders subdivide spherical arcs');
    const vertices = new Float32Array(native.memory.buffer, native.lab_globe_borders_vertices(), count * 3).slice();
    for (let i = 0; i < vertices.length; i += 3)
        check(Math.abs(Math.hypot(vertices[i], vertices[i + 1], vertices[i + 2]) - 1) < 1e-6,
              'GPU border vertices remain on the sphere');
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = 256;
    const gl = canvas.getContext('webgl', {antialias: false})!;
    const renderer = new MarsGlobeBorders(gl, labDOM.call('lab_globe_borders_shader', 0) as string,
                                        labDOM.call('lab_globe_borders_shader', 1) as string, vertices);
    const pixels = new Uint8Array(256 * 256 * 4);
    try {
        gl.enableVertexAttribArray(0);
        gl.viewport(0, 0, 256, 256);
        for (const [lon, zoom, visible] of [[0, 1, true], [180, 1, false], [0, 4096, true]] as const) {
            const camera = labDOM.call('lab_globe_centre', 0, lon);
            const rotation = (labDOM.call('lab_surface_camera', 1, camera, null, null, null, 0) as number[][]).flat();
            gl.clear(gl.COLOR_BUFFER_BIT);
            renderer.draw(rotation, zoom, 1);
            gl.readPixels(0, 0, 256, 256, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
            check(pixels.some(value => value > 0) === visible, `GPU borders respect visibility at longitude ${lon}, zoom ${zoom}`);
            check(gl.getError() === gl.NO_ERROR, 'GPU border rendering has no WebGL error');
        }
        check(labDOM.call('lab_globe_borders_mesh', '0 91 1 0') === 0, 'invalid GPU border input is rejected');
        check(labDOM.call('lab_globe_borders_mesh', '-89 0 89 0\n'.repeat(1000)) === 0,
              'oversized border subdivision is rejected before GPU allocation');
    } finally { renderer.dispose(); labDOM.call('lab_globe_borders_clear'); }
}
