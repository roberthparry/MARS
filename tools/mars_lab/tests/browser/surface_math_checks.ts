/**
 * @file surface_math_checks.ts
 * @brief Linked WASM maths and viewport safeguard regressions.
 *
 * Exercises the actual browser module, comparing ordinary display arithmetic with
 * browser maths and checking exact landmarks, extreme inputs and intentional fallbacks.
 * The import check prevents the linked library from introducing a host maths or WASI dependency.
 */
interface Window {
    /** @brief Check linked maths, viewport limits and the browser module's imports. */
    checkLabSurfaceMath(): Promise<void>;
}

/** @brief Exercise linked maths through native exports and the scoped camera and gesture APIs. */
window.checkLabSurfaceMath = async function checkLabSurfaceMath(): Promise<void> {
    const native = labWire.exports();

    /** @brief Report one failed maths or linkage assertion. */
    function check(condition: boolean, message: string): void {
        if (!condition) throw new Error('Surface maths: ' + message);
    }

    const initialFetch = labHttp.fetch;
    let deferredRequest = false;
    try {
        labHttp.fetch = async (url, options) => {
            if (url !== '/almanac-eval') return initialFetch(url, options);
            const payload = labWire.decode(options!.body as Uint8Array<ArrayBuffer>) as {defer_events?: string};
            deferredRequest = payload.defer_events === 'true';
            return Object.assign(new Response(), {async labData() { return {ok: true}; }});
        };
        await WorksheetRequests.evaluationTransport.fetchAlmanacEvaluation();
        check(deferredRequest, 'initial Almanac request defers annual events so the observer card can appear first');
    } finally { labHttp.fetch = initialFetch; }

    const townPageResponse = await labHttp.fetch('/towns?after=0&jurisdiction=GB-WLS');
    const townPage = await townPageResponse.labData() as LabCatalogue;
    check(townPageResponse.ok && townPage.available && Boolean(townPage.towns['GB-WLS']?.length) &&
          Object.keys(townPage.towns).every(code => code === 'GB-WLS'),
          'on-demand town endpoint returns only the requested jurisdiction');
    const townEndResponse = await labHttp.fetch(`/towns?after=${townPage.next}&jurisdiction=GB-WLS`);
    const townEnd = await townEndResponse.labData() as LabCatalogue;
    check(townEndResponse.ok && townEnd.next === '0' && !Object.keys(townEnd.towns).length,
          'jurisdiction cursor terminates without fetching other countries');

    for (const [latitude, longitude] of [[52.7077, -2.7541], [0, 179.9], [89.9, -179.9], [-89.9, 0]]) {
        const orientation = labDOM.call('lab_globe_centre', latitude, longitude) as number[];
        const projected = labDOM.call('lab_globe_project', latitude, longitude, orientation) as number[];
        check(Math.abs(projected[0]) < 1e-10 && Math.abs(projected[1]) < 1e-10 && projected[2] > 0.99999,
              'observer is centred on globe');
        const picked = labDOM.call('lab_globe_pick', 0, 0, orientation) as number[];
        check(Math.abs(picked[0] - latitude) < 1e-8 && Math.abs(picked[1] - longitude) < 1e-8,
              'globe centre picks observer coordinates');
        const view = labDOM.call('lab_globe_view', orientation, 1, 8192) as number[];
        check(view[2] === 4096 && view[3] < view[5] && view[4] < view[6], 'zoom bounded with valid texture extent');
    }
    check(labDOM.call('lab_globe_pick', 2, 0, [0, 0, 0, 1]) === null, 'globe limb rejects outside clicks');
    for (const [lat, lon] of [[52, -2], [-33, 151], [89.99, -179.9], [0, 180]]) {
        const start = [...labDOM.call('lab_globe_centre', 0, 179.9) as number[], 150];
        const end = [...labDOM.call('lab_globe_rotate',
            labDOM.call('lab_globe_centre', lat, lon), 1, 0, 0, .4) as number[], 4096];
        const frame = (progress: number): number[] => labDOM.call('lab_globe_flight', start, end, progress) as number[];
        check(JSON.stringify(frame(0)) === JSON.stringify(start) && JSON.stringify(frame(1)) === JSON.stringify(end),
              'history flight retains exact endpoints including zoom and roll');
        check(frame(.125)[4] > 1 && frame(.125)[4] < 150 && frame(.5)[4] === 1 &&
              frame(.875)[4] > 1 && frame(.875)[4] < 4096, 'flight zooms out before rotating and then zooms in');
        for (const progress of [.125, .25, .4, .5, .6, .75, .875]) {
            const camera = frame(progress);
            check(camera.every(Number.isFinite) && Math.abs(Math.hypot(...camera.slice(0, 4)) - 1) < 1e-12,
                  'history interpolation stays finite and normalised at poles and the date line');
        }
        const flipped = end.map((value, index) => index < 4 ? -value : value);
        const a = frame(.5), b = labDOM.call('lab_globe_flight', start, flipped, .5) as number[];
        check(Math.abs(a.slice(0, 4).reduce((sum, value, i) => sum + value * b[i], 0) - 1) < 1e-12,
              'equivalent quaternion signs take the same shortest rotation');
        for (const invalid of [null, [], [0, 0, 0, 0, 2], [0, 0, 0, 1, Infinity], [0, 0, 0, 1, 4097]])
            check(labDOM.call('lab_globe_flight', start, invalid, .5) === null, 'malformed saved cameras are rejected');
    }
    for (const latitude of [80, -80]) {
        const start = labDOM.call('lab_globe_centre', latitude, 0) as number[];
        const dy = latitude > 0 ? .5 : -.5;
        const across = labDOM.call('lab_globe_rotate', start, 1, 0, dy, 0) as number[];
        const centre = labDOM.call('lab_globe_pick', 0, 0, across) as number[];
        check(Math.abs(centre[0] - Math.sign(latitude) * 70) < 1e-8 && Math.abs(Math.abs(centre[1]) - 180) < 1e-8,
              'quaternion rotation passes continuously over either pole instead of clamping');
        const back = labDOM.call('lab_globe_rotate', across, 1, 0, -dy, 0) as number[];
        check(Math.abs(Math.abs(start.reduce((sum, value, index) => sum + value * back[index], 0)) - 1) < 1e-12,
              'reversing a polar drag restores the original orientation');
    }
    let tumbling = labDOM.call('lab_globe_centre', 89.99, 179.99) as number[];
    for (let i = 0; i < 400; ++i) {
        tumbling = labDOM.call('lab_globe_rotate', tumbling, 1, .07, .11, 0) as number[];
        check(Math.abs(Math.hypot(...tumbling) - 1) < 1e-12, 'repeated globe rotations remain normalised');
        const point = labDOM.call('lab_globe_pick', .2, -.3, tumbling) as number[];
        const screen = labDOM.call('lab_globe_project', point[0], point[1], tumbling) as number[];
        check(Math.abs(screen[0] - .2) < 1e-9 && Math.abs(screen[1] + .3) < 1e-9 && screen[2] > 0,
              'picking and marker projection agree throughout rolled and polar orientations');
    }
    for (const zoom of [1, 100]) {
        const start = labDOM.call('lab_globe_centre', 52, -2) as number[];
        for (const roll of [-.5, .5]) {
            const rotated = labDOM.call('lab_globe_rotate', start, zoom, 0, 0, roll) as number[];
            const centre = labDOM.call('lab_globe_pick', 0, 0, rotated) as number[];
            check(Math.abs(centre[0] - 52) < 1e-9 && Math.abs(centre[1] + 2) < 1e-9,
                  'edge roll preserves the view centre at overview and close zoom');
            const before = labDOM.call('lab_globe_project', 55, -2, start) as number[];
            const after = labDOM.call('lab_globe_project', 55, -2, rotated) as number[];
            check(Math.abs(after[0] + Math.sign(roll) * Math.abs(before[1]) * .5) < 1e-9,
                  'opposite edge rolls turn the globe in opposite directions without zoom attenuation');
        }
    }
    for (const [lat, lon, zoom, expected] of [[60, 15, 1, 3], [61, 16, 1, 0],
        [-60, -30, 1, 3], [0.25, 0.25, 100, 3], [0, 180, 1, 3]]) {
        const grid = labDOM.call('lab_globe_grid', lat, lon, zoom, 800) as number[];
        check(grid[2] === expected, 'grid hover matches displayed spacing and hemispheres');
    }
    check(Boolean(document.getElementById('almanacGlobeCanvas')), 'observer globe exists in Almanac card');
    check(Boolean(document.getElementById('almanacGlobe')!.compareDocumentPosition(
        document.querySelector('#almanacControls .almanac-briefing')!) & Node.DOCUMENT_POSITION_FOLLOWING),
          'observer globe precedes the complete Astronav worksheet');

    const globeCanvas = document.getElementById('almanacGlobeCanvas') as HTMLCanvasElement;
    const space = getComputedStyle(document.querySelector<HTMLElement>('.almanac-globe-view')!);
    check(space.backgroundColor === 'rgb(0, 0, 0)' && space.backgroundImage.includes('radial-gradient'),
          'globe has a blue-black star field behind its transparent exterior');
    const earth = new Image();
    earth.src = '/earth_surface.jpg';
    await earth.decode();
    check(earth.naturalWidth === 2048 && earth.naturalHeight === 1024,
          'bundled NASA surface texture decodes at its original resolution');
    if (globeCanvas.getContext('webgl')) {
        const panel = document.getElementById('almanacControls')!;
        const latitude = document.getElementById('almanacLatitude') as HTMLInputElement;
        const longitude = document.getElementById('almanacLongitude') as HTMLInputElement;
        const elevation = document.getElementById('almanacElevation') as HTMLInputElement;
        const town = document.getElementById('almanacTown') as HTMLSelectElement, savedTown = town.value;
        const saved = [latitude.value, longitude.value, elevation.value], hidden = panel.classList.contains('hidden');
        const scroll = [window.scrollX, window.scrollY];
        const evaluate = WorksheetLocations.calendarActions.triggerAlmanacAutoEvaluation;
        let evaluations = 0;
        WorksheetLocations.calendarActions.triggerAlmanacAutoEvaluation = async () => { ++evaluations; };
        try {
            town.value = '';
            latitude.value = '52.7077'; longitude.value = '-2.7541'; elevation.value = '50';
            panel.classList.remove('hidden');
            globeCanvas.scrollIntoView({block: 'center'});
            await new Promise(resolve => setTimeout(resolve, 100));
            for (let attempt = 0; attempt < 100 && document.getElementById('almanacGlobe')!.dataset.surface !== 'blue-marble'; ++attempt)
                await new Promise(resolve => setTimeout(resolve, 50));
            check(document.getElementById('almanacGlobe')!.dataset.surface === 'blue-marble',
                  'Earth texture is uploaded to the live globe');
            check(document.getElementById('almanacGlobe')!.dataset.resolution === '2048',
                  'initial overview does not eagerly load the large texture');
            document.querySelector<HTMLButtonElement>('[data-globe-action="centre"]')!.click();
            await new Promise(requestAnimationFrame);
            const marker = document.querySelector<HTMLElement>('.globe-observer');
            check(Boolean(marker) && Math.abs(parseFloat(marker!.style.left) - globeCanvas.clientWidth / 2) < 1,
                  'globe initially centres the observer');
            const grid = document.querySelector<HTMLButtonElement>('[data-globe-action="grid"]')!;
            check(grid.getAttribute('aria-pressed') === 'false', 'coordinate grid starts off');
            const observerBox = marker!.getBoundingClientRect();
            const observerHoverEvaluations = evaluations;
            globeCanvas.dispatchEvent(new PointerEvent('pointermove', {bubbles: true,
                clientX: observerBox.left + observerBox.width / 2,
                clientY: observerBox.top + observerBox.height / 2}));
            const observerTooltip = document.querySelector<HTMLElement>('.globe-grid-tooltip')!;
            check(!observerTooltip.hidden &&
                  observerTooltip.textContent!.includes(`Latitude ${MarsAlmanacGlobe.coordinate(Number(latitude.value), 'N', 'S')}`) &&
                  observerTooltip.textContent!.includes(`Longitude ${MarsAlmanacGlobe.coordinate(Number(longitude.value), 'E', 'W')}`),
                  'observer marker hover shows exact coordinates without Shift or the grid');
            check(getComputedStyle(marker!).pointerEvents === 'none' && evaluations === observerHoverEvaluations,
                  'observer hover preserves dragging and does not trigger calculations');
            globeCanvas.parentElement!.dispatchEvent(new PointerEvent('pointerleave'));
            check(observerTooltip.hidden === true, 'leaving the globe clears observer hover');
            const countryFetch = labHttp.fetch;
            const countryReplies: Array<{resolve: (response: MarsTransportResponse) => void; signal?: AbortSignal | null}> = [];
            const originalLatitude = latitude.value;
            try {
                labHttp.fetch = (url, options) => {
                    if (url !== '/almanac-country') return countryFetch(url, options);
                    return new Promise(resolve => countryReplies.push({resolve, signal: options?.signal}));
                };
                const selectCountryPoint = async (value: string): Promise<void> => {
                    latitude.value = value;
                    document.querySelector<HTMLButtonElement>('[data-globe-action="centre"]')!.click();
                    await new Promise(requestAnimationFrame);
                    await new Promise(requestAnimationFrame);
                };
                const hoverObserver = (): void => {
                    const box = document.querySelector<HTMLElement>('.globe-observer')!.getBoundingClientRect();
                    globeCanvas.dispatchEvent(new PointerEvent('pointermove', {bubbles: true,
                        clientX: box.left + box.width / 2, clientY: box.top + box.height / 2}));
                };
                const reply = (country: string): MarsTransportResponse => Object.assign(new Response(), {
                    async labData() { return {ok: true, country,
                        country_flag: country ? 'data:image/svg+xml;base64,' + btoa('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 4 3"><path fill="blue" d="M0 0H4V3H0Z"/></svg>') : ''}; }
                });
                await selectCountryPoint('52.7081');
                check(countryReplies.length === 1, 'selection starts one asynchronous country lookup');
                hoverObserver(); hoverObserver();
                check(countryReplies.length === 1 && !observerTooltip.hidden,
                      'hover remains responsive and never starts a country query');
                await selectCountryPoint('52.7082');
                check(countryReplies.length === 2 && countryReplies[0].signal?.aborted === true,
                      'new selection cancels the obsolete country lookup');
                countryReplies[1].resolve(reply('United Kingdom'));
                await new Promise(resolve => setTimeout(resolve, 30));
                hoverObserver();
                check(observerTooltip.textContent!.includes('United Kingdom'), 'land observer tooltip includes its country');
                check(observerTooltip.querySelector<HTMLImageElement>('img.country-flag')?.alt === '',
                      'country tooltip includes an offline flag with adjacent accessible country text');
                countryReplies[0].resolve(reply('Obsolete country'));
                await new Promise(resolve => setTimeout(resolve, 30));
                hoverObserver();
                check(!observerTooltip.textContent!.includes('Obsolete country'), 'stale country reply cannot relabel selection');
                await selectCountryPoint('52.7083');
                hoverObserver();
                check(!observerTooltip.textContent!.includes('United Kingdom'), 'pending selection clears the old country');
                check(!observerTooltip.querySelector('img.country-flag'), 'pending selection clears the old flag');
                countryReplies[2].resolve(reply(''));
                await new Promise(resolve => setTimeout(resolve, 30));
                await selectCountryPoint('52.7082');
                hoverObserver();
                check(countryReplies.length === 3 && observerTooltip.textContent!.includes('United Kingdom'),
                      'revisiting a cached coordinate avoids another database request');
                check(evaluations === observerHoverEvaluations, 'country lookups do not evaluate the almanac');
                const hoverBox = globeCanvas.getBoundingClientRect();
                const shiftHover = (offset: number): void => {
                    globeCanvas.dispatchEvent(new PointerEvent('pointermove', {bubbles: true, shiftKey: true,
                        clientX: hoverBox.left + hoverBox.width / 2 + offset,
                        clientY: hoverBox.top + hoverBox.height / 2 + 12}));
                };
                shiftHover(11); shiftHover(12); shiftHover(13);
                check(countryReplies.length === 3 && observerTooltip.textContent!.includes('Latitude'),
                      'Shift hover displays coordinates immediately and debounces country requests');
                await new Promise(resolve => setTimeout(resolve, 260));
                check(countryReplies.length === 4, 'a settled Shift pointer starts one country lookup');
                countryReplies[3].resolve(reply('Wales'));
                await new Promise(resolve => setTimeout(resolve, 0));
                check(observerTooltip.textContent!.includes('Wales') &&
                      Boolean(observerTooltip.querySelector('img.country-flag')),
                      'Shift hover adds the independently resolved country and flag');
                shiftHover(14);
                check(!observerTooltip.textContent!.includes('Wales') &&
                      !observerTooltip.querySelector('img.country-flag'),
                      'moving the Shift pointer immediately clears old country metadata');
                await new Promise(resolve => setTimeout(resolve, 260));
                countryReplies[4].resolve(reply(''));
                await new Promise(resolve => setTimeout(resolve, 0));
                check(!observerTooltip.hidden && !observerTooltip.querySelector('img.country-flag'),
                      'sea or uncertain Shift hover retains coordinates without a flag');
                shiftHover(15);
                await new Promise(resolve => setTimeout(resolve, 260));
                window.dispatchEvent(new KeyboardEvent('keyup', {key: 'Shift'}));
                check(countryReplies[5].signal?.aborted === true, 'releasing Shift cancels its country request');
                countryReplies[5].resolve(reply('Obsolete hover country'));
                await new Promise(resolve => setTimeout(resolve, 0));
                check(!observerTooltip.textContent!.includes('Obsolete hover country') &&
                      evaluations === observerHoverEvaluations, 'late hover replies cannot relabel or evaluate the observer');
            } finally {
                labHttp.fetch = countryFetch;
                latitude.value = originalLatitude;
                document.querySelector<HTMLButtonElement>('[data-globe-action="centre"]')!.click();
                await new Promise(requestAnimationFrame);
                globeCanvas.parentElement!.dispatchEvent(new PointerEvent('pointerleave'));
            }
            const beforeGrid = [latitude.value, longitude.value, evaluations];
            const gl = globeCanvas.getContext('webgl')!;
            const program = gl.getParameter(gl.CURRENT_PROGRAM) as WebGLProgram;
            // Read coordinates from the actual fragment shader, independently of the native picker.
            const probe = gl.createProgram()!, probeShaders: WebGLShader[] = [];
            const viewport = gl.getParameter(gl.VIEWPORT) as Int32Array;
            try {
                for (const original of gl.getAttachedShaders(program)!) {
                    const type = gl.getShaderParameter(original, gl.SHADER_TYPE) as number;
                    let source = gl.getShaderSource(original)!;
                    if (type === gl.FRAGMENT_SHADER) {
                        source = source.replace('uniform float viewport;', 'uniform float viewport; uniform vec2 probeCentre;')
                            .replace('gl_FragColor=vec4(surface,1.);',
                                     'gl_FragColor=vec4(.5+(vec2(lon,lat)-probeCentre)*256.,0.,1.);');
                    }
                    const shader = gl.createShader(type)!;
                    probeShaders.push(shader);
                    gl.shaderSource(shader, source);
                    gl.compileShader(shader);
                    check(Boolean(gl.getShaderParameter(shader, gl.COMPILE_STATUS)), 'coordinate probe compiles');
                    gl.attachShader(probe, shader);
                }
                gl.bindAttribLocation(probe, gl.getAttribLocation(program, 'position'), 'position');
                gl.linkProgram(probe);
                check(Boolean(gl.getProgramParameter(probe, gl.LINK_STATUS)), 'coordinate probe links');
                gl.useProgram(probe);
                gl.viewport(0, 0, 1, 1);
                for (const [lat, lon] of [[36.75, -4.5], [52.7077, -2.7541], [-60, 120], [89.9, 179.9], [-89.9, -179.9]]) {
                    const orientation = labDOM.call('lab_globe_centre', lat, lon);
                    const rotation = (labDOM.call('lab_surface_camera', 1, orientation, null, null, null, 0) as number[][]).flat();
                    gl.uniformMatrix3fv(gl.getUniformLocation(probe, 'cameraToWorld'), false, rotation);
                    gl.uniform3f(gl.getUniformLocation(probe, 'camera'), lat, lon, 100);
                    gl.uniform2f(gl.getUniformLocation(probe, 'probeCentre'), lon, lat);
                    gl.drawArrays(gl.TRIANGLES, 0, 6);
                    const pixel = new Uint8Array(4);
                    gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
                    check(Math.abs(pixel[0] - 127.5) <= 3 && Math.abs(pixel[1] - 127.5) <= 3,
                          `rendered coordinates match native centre ${lat},${lon}: ${pixel[0]},${pixel[1]}`);
                }
            } finally {
                gl.useProgram(program);
                gl.viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
                gl.deleteProgram(probe);
                probeShaders.forEach(shader => gl.deleteShader(shader));
                gl.drawArrays(gl.TRIANGLES, 0, 6);
            }
            // Exercise the actual globe shader with controlled local imagery and shoreline colours.
            const activeTexture = gl.getParameter(gl.ACTIVE_TEXTURE) as number;
            const savedTextures: (WebGLTexture | null)[] = [], paletteTextures: WebGLTexture[] = [];
            const paletteUniforms = ['mapMode', 'detailReady', 'grid', 'riverReady'];
            const savedUniforms = paletteUniforms.map(name => gl.getUniform(program, gl.getUniformLocation(program, name)!));
            const savedCamera = gl.getUniform(program, gl.getUniformLocation(program, 'camera')!) as Float32Array;
            const savedRiverBounds = gl.getUniform(program, gl.getUniformLocation(program, 'riverBounds')!) as Float32Array;
            try {
                for (const unit of [0, 2, 3]) {
                    gl.activeTexture(gl.TEXTURE0 + unit);
                    savedTextures.push(gl.getParameter(gl.TEXTURE_BINDING_2D) as WebGLTexture | null);
                    const texture = gl.createTexture()!;
                    paletteTextures.push(texture);
                    gl.bindTexture(gl.TEXTURE_2D, texture);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
                }
                gl.uniform1i(gl.getUniformLocation(program, 'mapMode'), 1);
                gl.uniform1i(gl.getUniformLocation(program, 'detailReady'), 0);
                gl.uniform1i(gl.getUniformLocation(program, 'grid'), 0);
                gl.uniform1i(gl.getUniformLocation(program, 'riverReady'), 0);
                for (const [photo, land, expected, kind] of [
                    [[245, 250, 250], true, [245, 250, 250], 'ice'],
                    [[190, 165, 100], true, [190, 165, 100], 'sand'],
                    [[65, 85, 30], true, [65, 85, 30], 'vegetation'],
                    [[90, 135, 45], true, [90, 135, 45], 'bright vegetation'],
                    [[7, 20, 64], false, [7, 20, 64], 'water'],
                    [[12, 145, 175], false, [12, 145, 175], 'coastal water'],
                    [[65, 85, 30], false, [7, 20, 64], 'fjord absent from coarse photograph'],
                    [[7, 20, 64], true, [70, 85, 34], 'island absent from coarse photograph']] as const) {
                    for (const [unit, rgb] of [[0, photo], [2, land ? [70, 85, 34] : [7, 20, 64]]] as const) {
                        gl.activeTexture(gl.TEXTURE0 + unit);
                        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                                      new Uint8Array([...rgb, 255]));
                    }
                    gl.drawArrays(gl.TRIANGLES, 0, 6);
                    const pixel = new Uint8Array(4);
                    gl.readPixels(Math.floor(globeCanvas.width / 2), Math.floor(globeCanvas.height / 2),
                                  1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
                    // The centre's existing globe lighting multiplies every channel equally.
                    // Check the actual photographic RGB, not merely a broad colour category.
                    const light = .8 + .2 / Math.hypot(.35, .45, 1);
                    check(expected.every((channel, index) => Math.abs(pixel[index] - channel * light) <= 2),
                          `local photographic ${kind} retains its sampled colour in the shoreline map`);
                }
                // Cross the photograph's blue-dominance threshold without changing the geographic mask.
                // A binary sample filter jumps straight from the fallback to turquoise at this boundary.
                for (const land of [false, true]) {
                    gl.activeTexture(gl.TEXTURE2);
                    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                                  new Uint8Array(land ? [70, 85, 34, 255] : [7, 20, 64, 255]));
                    let previous: Uint8Array | null = null;
                    for (let blue = 120; blue <= 160; ++blue) {
                        gl.activeTexture(gl.TEXTURE0);
                        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                                      new Uint8Array([60, 140, blue, 255]));
                        gl.drawArrays(gl.TRIANGLES, 0, 6);
                        const pixel = new Uint8Array(4);
                        gl.readPixels(Math.floor(globeCanvas.width / 2), Math.floor(globeCanvas.height / 2),
                                      1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
                        check(!previous || pixel.every((channel, i) => Math.abs(channel - previous![i]) <= 25),
                              `photographic eligibility fades without a colour step on ${land ? 'land' : 'water'}`);
                        previous = pixel;
                    }
                }
                gl.uniform3f(gl.getUniformLocation(program, 'camera'), savedCamera[0], savedCamera[1], 64);
                // Deep zoom must not magnify photographic colour stripes into geographic-looking patches.
                const stripes = new Uint8Array(1024 * 4);
                for (let x = 0; x < 1024; ++x)
                    stripes.set(x % 4 < 2 ? [7, 20, 64, 255] : [12, 145, 175, 255], x * 4);
                gl.activeTexture(gl.TEXTURE0);
                gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1024, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, stripes);
                gl.activeTexture(gl.TEXTURE2);
                gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                              new Uint8Array([7, 20, 64, 255]));
                gl.drawArrays(gl.TRIANGLES, 0, 6);
                const localCentre = new Uint8Array(4);
                gl.readPixels(Math.floor(globeCanvas.width / 2), Math.floor(globeCanvas.height / 2),
                              1, 1, gl.RGBA, gl.UNSIGNED_BYTE, localCentre);
                for (const fraction of [.25, .75]) {
                    const pixel = new Uint8Array(4);
                    gl.readPixels(Math.floor(globeCanvas.width * fraction), Math.floor(globeCanvas.height / 2),
                                  1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
                    check(pixel.every((channel, i) => Math.abs(channel - localCentre[i]) <= 2),
                          'town-scale water uses a consistent local photographic colour, not raster patches');
                }
                gl.uniform4f(gl.getUniformLocation(program, 'riverBounds'), -180, -90, 180, 90);
                gl.activeTexture(gl.TEXTURE3);
                gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                              new Uint8Array([255, 255, 255, 255]));
                for (const photo of [[7, 20, 64], [12, 145, 175], [65, 85, 30]]) {
                    gl.activeTexture(gl.TEXTURE0);
                    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                                  new Uint8Array([...photo, 255]));
                    gl.activeTexture(gl.TEXTURE2);
                    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                                  new Uint8Array([7, 20, 64, 255]));
                    gl.uniform1i(gl.getUniformLocation(program, 'riverReady'), 0);
                    gl.drawArrays(gl.TRIANGLES, 0, 6);
                    const sea = new Uint8Array(4), river = new Uint8Array(4);
                    const centre = Math.floor(globeCanvas.width / 2);
                    gl.readPixels(centre, centre, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, sea);
                    gl.uniform1i(gl.getUniformLocation(program, 'riverReady'), 1);
                    gl.drawArrays(gl.TRIANGLES, 0, 6);
                    gl.readPixels(centre, centre, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, river);
                    check(sea.every((channel, i) => Math.abs(channel - river[i]) <= 1),
                          'river colour matches adjoining sea and fjord water, including missing-photo fallback');
                }
            } finally {
                [0, 2, 3].forEach((unit, index) => {
                    gl.activeTexture(gl.TEXTURE0 + unit);
                    gl.bindTexture(gl.TEXTURE_2D, savedTextures[index]);
                    gl.deleteTexture(paletteTextures[index]);
                });
                paletteUniforms.forEach((name, index) =>
                    gl.uniform1i(gl.getUniformLocation(program, name), Number(savedUniforms[index])));
                gl.uniform3fv(gl.getUniformLocation(program, 'camera'), savedCamera);
                gl.uniform4fv(gl.getUniformLocation(program, 'riverBounds'), savedRiverBounds);
                gl.activeTexture(activeTexture);
                gl.drawArrays(gl.TRIANGLES, 0, 6);
            }
            const map = document.querySelector<HTMLButtonElement>('[data-globe-action="map"]')!;
            check(map.getAttribute('aria-pressed') === 'false', 'photographic view is the default');
            const mapFetch = labHttp.fetch;
            let rejectMask: ((reason: Error) => void) | undefined;
            let maskRequests = 0;
            try {
                labHttp.fetch = (url, options) => {
                    if (url !== '/almanac-globe') return mapFetch(url, options);
                    ++maskRequests;
                    if (maskRequests === 1) return new Promise((_resolve, reject) => { rejectMask = reject; });
                    return Promise.resolve(Object.assign(new Response(), {
                        async labData() {
                            return {ok: true, image: '<svg xmlns="http://www.w3.org/2000/svg" width="2" height="2"><path fill="#465522" d="M0 0H2V2H0Z"/></svg>'};
                        }
                    }));
                };
                map.click();
                await new Promise(requestAnimationFrame);
                check(Boolean(rejectMask) && !gl.getUniform(program, gl.getUniformLocation(program, 'mapMode')!),
                      'pending shoreline mask keeps photographic rendering rather than the blue placeholder');
                rejectMask!(new Error('delayed shoreline fixture'));
                await new Promise(resolve => setTimeout(resolve, 50));
                await new Promise(requestAnimationFrame);
                check(!gl.getUniform(program, gl.getUniformLocation(program, 'mapMode')!),
                      'failed shoreline mask retains photographic rendering');
                map.click();
                map.click();
                for (let i = 0; i < 100 && !gl.getUniform(program, gl.getUniformLocation(program, 'mapMode')!); ++i)
                    await new Promise(resolve => setTimeout(resolve, 25));
                check(map.textContent === 'Map' && Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'mapMode')!)),
                      'retry switches to map rendering only after the shoreline image is decoded and uploaded');
            } finally { labHttp.fetch = mapFetch; }
            map.click();
            await new Promise(requestAnimationFrame);
            check(map.textContent === 'Natural Earth' && !gl.getUniform(program, gl.getUniformLocation(program, 'mapMode')!) &&
                  latitude.value === beforeGrid[0] && longitude.value === beforeGrid[1] && evaluations === beforeGrid[2],
                  'photographic view restores without changing or evaluating the observer');
            grid.click();
            await new Promise(requestAnimationFrame);
            check(grid.getAttribute('aria-pressed') === 'true' && Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'grid')!)),
                  'one click enables the rendered coordinate grid');
            const line = labDOM.call('lab_globe_project', 60, 0,
                labDOM.call('lab_globe_centre', 52.7077, -2.7541)) as number[];
            const hoverBox = globeCanvas.getBoundingClientRect();
            globeCanvas.dispatchEvent(new PointerEvent('pointermove', {bubbles: true,
                clientX: hoverBox.left + hoverBox.width * (.5 + line[0] / 2.24),
                clientY: hoverBox.top + hoverBox.width * (.5 + line[1] / 2.24)}));
            const gridTooltip = document.querySelector<HTMLElement>('.globe-grid-tooltip')!;
            check(!gridTooltip.hidden && gridTooltip.textContent!.includes('Latitude 60° N') &&
                  gridTooltip.textContent!.includes('Longitude 0°'), 'grid intersection tooltip names both coordinates');
            const gridMarker = document.querySelector<HTMLElement>('.globe-grid-marker')!;
            check(!gridMarker.hidden &&
                  Math.abs(parseFloat(gridMarker.style.left) - hoverBox.width * (.5 + line[0] / 2.24)) < 1 &&
                  Math.abs(parseFloat(gridMarker.style.top) - hoverBox.width * (.5 + line[1] / 2.24)) < 1 &&
                  getComputedStyle(gridMarker).pointerEvents === 'none',
                  'grid tooltip marks the actual intersection without intercepting pointer events');
            check(getComputedStyle(globeCanvas).cursor === 'crosshair' && gridMarker.getBoundingClientRect().width >= 14,
                  'grid hover replaces the grab hand with a crosshair and a visible outlined marker');
            grid.click();
            await new Promise(requestAnimationFrame);
            check(grid.getAttribute('aria-pressed') === 'false' && !gl.getUniform(program, gl.getUniformLocation(program, 'grid')!) &&
                  latitude.value === beforeGrid[0] && longitude.value === beforeGrid[1] && evaluations === beforeGrid[2],
                  'second click disables grid without changing or evaluating the observer');
            check(gridTooltip.hidden === true, 'disabling graticule hides its tooltip');
            check(gridMarker.hidden === true, 'disabling graticule hides its line marker');
            check(getComputedStyle(globeCanvas).cursor === 'grab', 'leaving grid hover restores the grab cursor');
            const freePoint = labDOM.call('lab_globe_project', 35.5, -23.25,
                labDOM.call('lab_globe_centre', 52.7077, -2.7541)) as number[];
            globeCanvas.dispatchEvent(new PointerEvent('pointermove', {bubbles: true,
                clientX: hoverBox.left + hoverBox.width * (.5 + freePoint[0] / 2.24),
                clientY: hoverBox.top + hoverBox.height / 2 + hoverBox.width * freePoint[1] / 2.24}));
            window.dispatchEvent(new KeyboardEvent('keydown', {key: 'Shift', shiftKey: true}));
            check(!gridTooltip.hidden && gridTooltip.textContent!.includes('Latitude 35° 30.00′ N') &&
                  gridTooltip.textContent!.includes('Longitude 23° 15.00′ W'),
                  'stationary Shift hover reports the pointer coordinates with the grid disabled');
            window.dispatchEvent(new KeyboardEvent('keyup', {key: 'Shift'}));
            check(gridTooltip.hidden === true, 'releasing Shift hides free-coordinate hover');
            window.dispatchEvent(new KeyboardEvent('keydown', {key: 'Shift', shiftKey: true}));
            window.dispatchEvent(new Event('blur'));
            check(gridTooltip.hidden === true, 'window blur clears the coordinate tooltip');
            window.dispatchEvent(new KeyboardEvent('keydown', {key: 'Shift', shiftKey: true}));
            check(gridTooltip.hidden === true, 'blur discards the stale pointer position');
            check(latitude.value === beforeGrid[0] && longitude.value === beforeGrid[1] && evaluations === beforeGrid[2],
                  'Shift hover neither selects an observer nor triggers calculations');
            globeCanvas.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight', bubbles: true}));
            await new Promise(requestAnimationFrame);
            check(document.querySelector('.globe-observer') === marker, 'camera movement reuses its observer marker');
            check(Math.abs(parseFloat(document.querySelector<HTMLElement>('.globe-observer')!.style.left) -
                globeCanvas.clientWidth / 2) > 1, 'keyboard rotates the globe');
            const jurisdiction = (document.getElementById('almanacJurisdiction') as HTMLSelectElement).value;
            const place = WorksheetContext.JURISDICTION_TOWN_OPTIONS[jurisdiction]?.find(candidate =>
                Array.from(town.options).some(option => option.value === candidate.value));
            check(Boolean(place), 'selected-town regression has a catalogue town');
            const beforeCommitZoom = (gl.getUniform(program, gl.getUniformLocation(program, 'camera')!) as Float32Array)[2];
            const beforeCommitEvaluations = evaluations;
            town.value = place!.value;
            // Use follows guarded town restoration with native field projection and the commit service.
            labDOM.call('lab_location_totality_finish', {
                latitude: place!.latitude, longitude: place!.longitude, elevation: '50'
            }, true);
            const saveCalendar = WorksheetPersistence.persistence.saveCalendar;
            WorksheetPersistence.persistence.saveCalendar = () => {};
            try {
                WorksheetLocations.locationServices.locSaveAlmanac();
            } finally {
                WorksheetPersistence.persistence.saveCalendar = saveCalendar;
            }
            await new Promise(requestAnimationFrame);
            const selectedLabel = document.querySelector<HTMLButtonElement>('.globe-town.selected');
            check(Boolean(selectedLabel) && selectedLabel!.textContent!.includes(place!.name),
                  'selected town keeps its label beside the observer');
            const committedMarker = document.querySelector<HTMLElement>('.globe-observer')!;
            check(Math.abs(parseFloat(committedMarker.style.left) - globeCanvas.clientWidth / 2) < 1 &&
                  Math.abs(parseFloat(committedMarker.style.top) - globeCanvas.clientHeight / 2) < 1,
                  'Use recentres the globe on the committed observer without a manual control change');
            check((gl.getUniform(program, gl.getUniformLocation(program, 'camera')!) as Float32Array)[2] === beforeCommitZoom &&
                  evaluations === beforeCommitEvaluations, 'observer notification preserves zoom and does not reevaluate');
            globeCanvas.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight', bubbles: true}));
            await new Promise(requestAnimationFrame);
            const rotated = gl.getUniform(program, gl.getUniformLocation(program, 'cameraToWorld')!) as Float32Array;
            document.dispatchEvent(new Event('mars-almanac-observer-committed'));
            await new Promise(requestAnimationFrame);
            const retained = gl.getUniform(program, gl.getUniformLocation(program, 'cameraToWorld')!) as Float32Array;
            check(rotated.every((value, index) => value === retained[index]),
                  'saving an unchanged observer does not undo a manual globe rotation');
            document.querySelector<HTMLButtonElement>('[data-globe-action="centre"]')!.click();
            await new Promise(requestAnimationFrame);
            const country = new Intl.DisplayNames(['en-GB'], {type: 'region'}).of(jurisdiction.split('-')[0]);
            check(selectedLabel!.dataset.marsTooltipTitle!.startsWith(`${place!.name}, ${country} · Latitude `) &&
                  selectedLabel!.dataset.marsTooltipTitle!.includes(`Longitude ${MarsAlmanacGlobe.coordinate(Number(place!.longitude), 'E', 'W')}`) &&
                  !selectedLabel!.hasAttribute('title'),
                  'town tooltip includes the full country name rather than its jurisdiction code');
            const townFlagFetch = labHttp.fetch;
            const originalTooltipName = selectedLabel!.dataset.townTooltipName!;
            const originalTooltipTitle = selectedLabel!.dataset.marsTooltipTitle!;
            selectedLabel!.dataset.townTooltipName = originalTooltipName + ' (GeoNames 1234567)';
            selectedLabel!.dataset.marsTooltipTitle = originalTooltipTitle.replace(originalTooltipName,
                selectedLabel!.dataset.townTooltipName);
            const townFlagReplies: Array<{resolve: (response: MarsTransportResponse) => void; signal?: AbortSignal | null}> = [];
            try {
                labHttp.fetch = (url, options) => url === '/almanac-country'
                    ? new Promise(resolve => townFlagReplies.push({resolve, signal: options?.signal}))
                    : townFlagFetch(url, options);
                selectedLabel!.dispatchEvent(new PointerEvent('pointerover', {bubbles: true}));
                const tooltip = document.querySelector<HTMLElement>('#marsButtonTooltip')!;
                check(townFlagReplies.length === 1 && tooltip.textContent!.includes(place!.name),
                      'town tooltip shows text immediately while its country flag loads');
                check(!tooltip.textContent!.includes('GeoNames') && !tooltip.textContent!.includes('1234567'),
                      'initial town tooltip omits catalogue source IDs');
                const flagResponse = Object.assign(new Response(), {async labData() { return {ok: true, country,
                    country_flag: 'data:image/svg+xml;base64,' + btoa('<svg xmlns="http://www.w3.org/2000/svg"/>')}; }});
                townFlagReplies[0].resolve(flagResponse);
                await new Promise(resolve => setTimeout(resolve, 0));
                check(tooltip.querySelector('img.country-flag')?.getAttribute('alt') === '' &&
                      tooltip.textContent!.includes(country!), 'town tooltip displays a decorative flag beside country text');
                check(!tooltip.textContent!.includes('GeoNames') && !tooltip.textContent!.includes('1234567'),
                      'asynchronous country enrichment does not restore source IDs');
                selectedLabel!.dataset.townTooltipName = originalTooltipName;
                selectedLabel!.dataset.marsTooltipTitle = originalTooltipTitle;
                WorksheetResults.tooltip.hide();
                WorksheetResults.tooltip.show(selectedLabel!);
                check(townFlagReplies.length === 1 && Boolean(tooltip.querySelector('img.country-flag')),
                      'repeated town hover reuses the cached country flag');
                WorksheetResults.tooltip.hide();
                const townShell = document.querySelector<HTMLSelectElement>('#almanacTown')!.closest('.select-shell')!;
                const townRow = townShell.querySelector<HTMLButtonElement>('.select-option.selected')!;
                check(townRow.dataset.countryCode === selectedLabel!.dataset.countryCode,
                      'dropdown towns carry the same jurisdiction metadata as globe labels');
                townShell.classList.add('open');
                townShell.querySelector('.select-menu')!.classList.remove('hidden');
                townRow.dispatchEvent(new PointerEvent('pointerover', {bubbles: true}));
                check(townFlagReplies.length === 1 && Boolean(tooltip.querySelector('img.country-flag')),
                      'actual dropdown hover reuses the flag cache and displays artwork');
                const rowName = townRow.dataset.townTooltipName;
                townRow.dataset.townTooltipName = 'Newport (Harbour) (GeoNames 1234567)';
                WorksheetResults.tooltip.show(townRow);
                check(tooltip.textContent!.includes('Newport (Harbour)') && !tooltip.textContent!.includes('GeoNames'),
                      'dropdown town tooltip keeps descriptive parentheses but omits GeoNames IDs');
                townRow.dataset.townTooltipName = rowName;
                WorksheetResults.tooltip.hide();
                townShell.classList.remove('open');
                townShell.querySelector('.select-menu')!.classList.add('hidden');
                selectedLabel!.dispatchEvent(new FocusEvent('focusin', {bubbles: true}));
                document.querySelector<HTMLButtonElement>('[data-globe-action="centre"]')!.click();
                await new Promise(requestAnimationFrame);
                check(!selectedLabel!.hasAttribute('title'),
                      'globe redraw never reinstates a competing plain browser tooltip');
                WorksheetResults.tooltip.hide();
                const savedCode = selectedLabel!.dataset.countryCode;
                selectedLabel!.dataset.countryCode = savedCode === 'DK' ? 'NL' : 'DK';
                WorksheetResults.tooltip.show(selectedLabel!);
                WorksheetResults.tooltip.hide();
                check(townFlagReplies[1].signal?.aborted === true, 'leaving a town cancels pending flag work');
                townFlagReplies[1].resolve(flagResponse);
                await new Promise(resolve => setTimeout(resolve, 0));
                check(!tooltip.classList.contains('visible'), 'late flag replies cannot reopen a hidden tooltip');
                selectedLabel!.dataset.countryCode = 'GB-WLS';
                WorksheetResults.tooltip.show(selectedLabel!);
                const welshResponse = Object.assign(new Response(), {async labData() { return {ok: true,
                    country: 'Wales', country_flag: 'data:image/svg+xml;base64,' +
                        btoa('<svg xmlns="http://www.w3.org/2000/svg"/>')}; }});
                townFlagReplies[2].resolve(welshResponse);
                await new Promise(resolve => setTimeout(resolve, 0));
                check(tooltip.textContent!.includes(', Wales · Latitude ') &&
                      Boolean(tooltip.querySelector('img.country-flag')),
                      'constituent-country tooltip pairs its own country name with its flag');
                WorksheetResults.tooltip.hide();
                selectedLabel!.dataset.countryCode = savedCode;
                const countryShell = document.querySelector<HTMLSelectElement>('#almanacJurisdiction')!.closest('.select-shell')!;
                const countryButton = countryShell.querySelector<HTMLButtonElement>('.select-button')!;
                countryButton.dispatchEvent(new FocusEvent('focusin', {bubbles: true}));
                check(countryButton.dataset.countryCode === savedCode &&
                      Boolean(countryButton.querySelector('img.country-flag')) &&
                      Boolean(tooltip.querySelector('img.country-flag')),
                      'selected country reuses the town flag cache in its button and tooltip');
                countryShell.classList.add('open');
                countryShell.querySelector('.select-menu')!.classList.remove('hidden');
                const countryRow = countryShell.querySelector<HTMLButtonElement>('.select-option.selected')!;
                countryRow.dispatchEvent(new PointerEvent('pointerover', {bubbles: true}));
                check(Boolean(countryRow.querySelector('img.country-flag')) &&
                      countryRow.querySelector('img')!.getAttribute('alt') === '',
                      'country dropdown row shows decorative cached artwork beside its existing label');
                countryShell.classList.remove('open');
                countryShell.querySelector('.select-menu')!.classList.add('hidden');
                const flagLoader = WorksheetResults.tooltip as unknown as {
                    fetchFlag(code: string, signal: AbortSignal): Promise<{name: string; flag: string}>;
                };
                const firstConsumer = new AbortController(), secondConsumer = new AbortController();
                const beforeShared = townFlagReplies.length;
                const firstResult = flagLoader.fetchFlag('FR', firstConsumer.signal).catch(() => null);
                const secondResult = flagLoader.fetchFlag('FR', secondConsumer.signal);
                check(townFlagReplies.length === beforeShared + 1,
                      'concurrent menu and tooltip consumers share one asynchronous country request');
                firstConsumer.abort();
                check(!townFlagReplies[beforeShared].signal?.aborted,
                      'leaving one consumer preserves the request needed by another');
                townFlagReplies[beforeShared].resolve(flagResponse);
                check(await firstResult === null && Boolean((await secondResult).flag),
                      'cancelled consumers stop independently while the remaining consumer receives artwork');
            } finally {
                selectedLabel!.dataset.townTooltipName = originalTooltipName;
                selectedLabel!.dataset.marsTooltipTitle = originalTooltipTitle;
                WorksheetResults.tooltip.hide();
                labHttp.fetch = townFlagFetch;
            }
            check(getComputedStyle(selectedLabel!).color === 'rgb(255, 211, 121)' &&
                  selectedLabel!.getAttribute('aria-current') === 'location', 'selected label is gold and identified');
            for (const target of [globeCanvas, selectedLabel!]) {
                const beforeZoom = (gl.getUniform(program, gl.getUniformLocation(program, 'camera')!) as Float32Array)[2];
                const wheel = new WheelEvent('wheel', {bubbles: true, cancelable: true, deltaY: -80});
                target.dispatchEvent(wheel);
                await new Promise(requestAnimationFrame);
                const afterZoom = (gl.getUniform(program, gl.getUniformLocation(program, 'camera')!) as Float32Array)[2];
                check(wheel.defaultPrevented && afterZoom > beforeZoom,
                      'wheel over canvas or town label zooms instead of scrolling the page');
                target.dispatchEvent(new WheelEvent('wheel', {bubbles: true, cancelable: true, deltaY: 80}));
                await new Promise(requestAnimationFrame);
            }
            town.value = '';
            const beforeTownClick = evaluations;
            labDOM.call('lab_workspace_dom_busy', 1, null, [], []);
            labDOM.call('lab_workspace_dom_busy', 1, null, [], []);
            for (const id of ['datetimeJurisdiction', 'datetimeTown', 'almanacJurisdiction', 'almanacTown']) {
                const select = document.getElementById(id) as HTMLSelectElement;
                const shell = select.closest('.select-shell')!;
                check(!select.disabled &&
                      Array.from(shell.querySelectorAll<HTMLInputElement | HTMLButtonElement>('input, button'))
                          .every(control => !control.disabled),
                      `${id} and its custom menu remain usable during repeated busy notifications`);
            }
            const townShell = town.closest('.select-shell')!;
            const townButton = townShell.querySelector<HTMLButtonElement>('.select-button')!;
            townButton.click();
            check(townShell.classList.contains('open'), 'town menu opens while calculations are busy');
            const townSearch = townShell.querySelector<HTMLInputElement>('.select-search')!;
            townSearch.value = place!.name;
            townSearch.dispatchEvent(new Event('input', {bubbles: true}));
            check(townShell.querySelectorAll('.select-option:not(:disabled)').length > 0,
                  'town search returns selectable results while calculations are busy');
            townButton.click();
            check(!(selectedLabel as HTMLButtonElement).disabled &&
                  !document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!.disabled,
                  'town selection and globe navigation remain available during refresh');
            selectedLabel!.click();
            labDOM.call('lab_workspace_dom_busy', 0, null, [], []);
            check(town.value === place!.value && latitude.value === place!.latitude && longitude.value === place!.longitude &&
                  elevation.value === '50' && evaluations === beforeTownClick + 1,
                  'one town click immediately selects its observer and evaluates once without changing elevation');
            for (let i = 0; i < 12; ++i)
                document.querySelector<HTMLButtonElement>('[data-globe-action="out"]')!.click();
            await new Promise(requestAnimationFrame);
            check(document.querySelector('.globe-town.selected') === selectedLabel,
                  'selected town label survives overview zoom and decluttering');
            globeCanvas.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight', bubbles: true}));
            document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!.click();
            await new Promise(requestAnimationFrame);
            globeCanvas.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
            await new Promise(requestAnimationFrame);
            check(elevation.value === '50' && Number(longitude.value) !== Number(place!.longitude),
                  'globe selection changes location without resetting altitude');
            document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!.click();
            if ((gl.getParameter(gl.MAX_TEXTURE_SIZE) as number) >= 4096) {
                for (let attempt = 0; attempt < 400 && document.getElementById('almanacGlobe')!.dataset.resolution === '2048'; ++attempt)
                    await new Promise(resolve => setTimeout(resolve, 50));
                const resolution = Number(document.getElementById('almanacGlobe')!.dataset.resolution);
                check(resolution === 4096 || resolution === 8192,
                      `zoom upgrades to real higher-resolution imagery (${document.getElementById('almanacGlobeStatus')?.textContent})`);
                check(!gl.isContextLost(), 'higher-resolution upload retains the graphics context');
            }
            for (let i = 0; i < 4; ++i)
                document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!.click();
            await new Promise(requestAnimationFrame);
            check(Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'mapMode')!)) &&
                  map.getAttribute('aria-pressed') === 'false', 'deep zoom automatically replaces imagery with shoreline map');
            for (let i = 0; i < 4; ++i)
                document.querySelector<HTMLButtonElement>('[data-globe-action="out"]')!.click();
            await new Promise(requestAnimationFrame);
            check(!gl.getUniform(program, gl.getUniformLocation(program, 'mapMode')!),
                  'zooming out restores photographic imagery');
            check(!document.querySelector('.globe-town.selected'), 'custom location clears selected-town highlight');
            const box = globeCanvas.getBoundingClientRect();
            const click = {bubbles: true, button: 0, clientX: box.left + box.width * .56, clientY: box.top + box.height * .5};
            const beforePointClick = evaluations, previousLongitude = longitude.value;
            const pointFetch = labHttp.fetch;
            labHttp.fetch = (url, options) => url === '/almanac-country'
                ? Promise.resolve(Object.assign(new Response(), {async labData() {
                    return {ok: true, country: '', country_code: '', country_flag: ''};
                }})) : pointFetch(url, options);
            globeCanvas.dispatchEvent(new MouseEvent('click', click));
            check((document.getElementById('almanacJurisdiction') as HTMLSelectElement).value === '',
                  'custom point immediately clears the previous country');
            await new Promise(resolve => setTimeout(resolve, 0));
            check(longitude.value !== previousLongitude && town.value === '' && evaluations === beforePointClick + 1,
                  'one left click selects exact coordinates and evaluates once after country lookup');
            const selectedLongitude = longitude.value;
            const jurisdictionSelect = document.getElementById('almanacJurisdiction') as HTMLSelectElement;
            jurisdictionSelect.value = jurisdiction;
            labDOM.call('lab_location_evaluation', 6,
                {observer_surface: 'Sea', fields: {jurisdiction}},
                WorksheetLocations.calendar.calendarLocationContext());
            check(jurisdictionSelect.value === '' && town.value === '' && longitude.value === selectedLongitude &&
                  jurisdictionSelect.closest('.select-shell')!.querySelector('.select-button')!.textContent!
                      .includes('Country not identified'),
                  'offshore evaluation clears stale country and town controls without changing coordinates');
            jurisdictionSelect.value = jurisdiction;
            labDOM.call('lab_location_evaluation', 6, {fields: {jurisdiction: ''}},
                WorksheetLocations.calendar.calendarLocationContext());
            check(jurisdictionSelect.value === '', 'evaluation preserves an explicitly unidentified country');
            globeCanvas.dispatchEvent(new MouseEvent('click', {...click, button: 2}));
            check(longitude.value === selectedLongitude && evaluations === beforePointClick + 1,
                  'right click does not select an observer');
            const capture = globeCanvas.setPointerCapture;
            globeCanvas.setPointerCapture = () => {};
            try {
                for (const side of [.1, .9]) {
                    await new Promise(requestAnimationFrame);
                    const matrixLocation = gl.getUniformLocation(program, 'cameraToWorld')!;
                    const before = gl.getUniform(program, matrixLocation) as Float32Array;
                    const edge = {...click, pointerId: 43, pointerType: 'mouse', clientX: box.left + box.width * side};
                    globeCanvas.dispatchEvent(new PointerEvent('pointerdown', edge));
                    globeCanvas.dispatchEvent(new PointerEvent('pointermove', {...edge, clientY: edge.clientY + 40}));
                    globeCanvas.dispatchEvent(new PointerEvent('pointerup', {...edge, clientY: edge.clientY + 40}));
                    await new Promise(requestAnimationFrame);
                    const after = gl.getUniform(program, matrixLocation) as Float32Array;
                    check([6, 7, 8].every(index => Math.abs(before[index] - after[index]) < 1e-6) &&
                          before.some((value, index) => Math.abs(value - after[index]) > .01),
                          'vertical drag at either edge rolls the rendered globe without moving its centre');
                    globeCanvas.dispatchEvent(new MouseEvent('click', edge));
                    check(evaluations === beforePointClick + 1, 'edge roll does not select or evaluate an observer');
                }
                const pointer = {...click, pointerId: 42, pointerType: 'mouse'};
                globeCanvas.dispatchEvent(new PointerEvent('pointerdown', pointer));
                globeCanvas.dispatchEvent(new PointerEvent('pointermove', {...pointer, clientX: pointer.clientX + 30}));
                globeCanvas.dispatchEvent(new PointerEvent('pointerup', {...pointer, clientX: pointer.clientX + 30}));
                globeCanvas.dispatchEvent(new MouseEvent('click', click));
                check(evaluations === beforePointClick + 1, 'a completed drag does not select an observer');
                globeCanvas.dispatchEvent(new PointerEvent('pointerdown', pointer));
                globeCanvas.dispatchEvent(new PointerEvent('pointermove', {...pointer, clientX: pointer.clientX + 3}));
                globeCanvas.dispatchEvent(new PointerEvent('pointerup', pointer));
                globeCanvas.dispatchEvent(new MouseEvent('click', click));
                await new Promise(resolve => setTimeout(resolve, 0));
                check(evaluations === beforePointClick + 2, 'a single click after dragging tolerates small hand movement');
            } finally { globeCanvas.setPointerCapture = capture; labHttp.fetch = pointFetch; }
            try {
                let countryCalls = 0;
                let resolveCountry!: (response: MarsTransportResponse) => void;
                labHttp.fetch = (url, options) => url === '/almanac-country'
                    ? (++countryCalls, new Promise(resolve => { resolveCountry = resolve; })) : pointFetch(url, options);
                const clickedCamera = MarsAlmanacGlobe.captureHistory()!;
                const landClick = {...click, clientX: click.clientX + 14};
                globeCanvas.dispatchEvent(new MouseEvent('click', landClick));
                const pickedCoordinates = [latitude.value, longitude.value];
                await new Promise(requestAnimationFrame);
                check(countryCalls === 1, 'custom selection and its marker share one pending geographic lookup');
                resolveCountry(Object.assign(new Response(), {async labData() {
                    return {ok: true, country: 'Wales', country_code: 'GB-WLS', country_flag: ''};
                }}));
                await new Promise(resolve => setTimeout(resolve, 0));
                check((document.getElementById('almanacJurisdiction') as HTMLSelectElement).value === 'GB-WLS' &&
                      town.value === '' && latitude.value === pickedCoordinates[0] && longitude.value === pickedCoordinates[1],
                      'identified land changes the country menu without snapping the clicked point to a town');
                globeCanvas.dispatchEvent(new MouseEvent('click', landClick));
                await new Promise(resolve => setTimeout(resolve, 0));
                check(countryCalls === 1 && jurisdictionSelect.value === 'GB-WLS',
                      'reselecting cached custom land restores its country without another request');
                document.dispatchEvent(new Event('mars-almanac-observer-committed'));
                check(MarsAlmanacGlobe.captureHistory()!.every((value, index) => value === clickedCamera[index]),
                      'asynchronous country resolution preserves the clicked camera and its saved zoom');
                jurisdictionSelect.value = '';
                latitude.value = String(Number(latitude.value));
                document.dispatchEvent(new Event('mars-almanac-observer-committed'));
                check(MarsAlmanacGlobe.captureHistory()!.every((value, index) => value === clickedCamera[index]),
                      'evaluation metadata changes do not recentre a clicked position');
                latitude.value = String(Number(latitude.value) + 1);
                town.dispatchEvent(new Event('change', {bubbles: true}));
                await new Promise(requestAnimationFrame);
                const dropdownCamera = MarsAlmanacGlobe.captureHistory()!;
                const centred = labDOM.call('lab_globe_centre', Number(latitude.value), Number(longitude.value)) as number[];
                check(centred.every((value, index) => value === dropdownCamera[index]),
                      'dropdown changes still centre the selected observer');
            } finally { labHttp.fetch = pointFetch; }
            const originalFetch = labHttp.fetch;
            const originalNearby = labApplication.loadNearbyTowns;
            let nearbyRequests = 0;
            const pendingSurface: ((response: MarsTransportResponse) => void)[] = [];
            const pending: {resolve: (response: MarsTransportResponse) => void; signal?: AbortSignal | null}[] = [];
            const globeRoot = document.getElementById('almanacGlobe')!;
            const zoomOut = globeRoot.querySelector<HTMLButtonElement>('[data-globe-action="out"]')!;
            const zoomIn = globeRoot.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!;
            const originalUpload = gl.texImage2D;
            let releasedTextures = 0;
            const riverReply = (): MarsTransportResponse => Object.assign(new Response(), {
                async labData() {
                    return {ok: true, image: '<svg xmlns="http://www.w3.org/2000/svg" width="2" height="2"><path d="M0 0L2 2" stroke="white"/></svg>'};
                }
            });
            try {
                gl.texImage2D = function (...args: unknown[]): void {
                    if (gl.getParameter(gl.ACTIVE_TEXTURE) === gl.TEXTURE3 && args.length === 9 &&
                        args[3] === 1 && args[4] === 1) ++releasedTextures;
                    Reflect.apply(originalUpload, gl, args);
                };
                labHttp.fetch = (url, options) => {
                    if (url === '/almanac-globe') return new Promise(resolve => pendingSurface.push(resolve));
                    if (url !== '/almanac-rivers') return originalFetch(url, options);
                    return new Promise(resolve => pending.push({resolve, signal: options?.signal}));
                };
                for (let i = 0; i < 24; ++i) zoomOut.click();
                delete globeRoot.dataset.rivers;
                await new Promise(resolve => setTimeout(resolve, 400));
                check(pending.length === 0, 'overview does not fetch river data');
                for (let i = 0; i < 9; ++i) zoomIn.click();
                for (let i = 0; i < 40 && pending.length < 1; ++i) await new Promise(resolve => setTimeout(resolve, 25));
                check(pending.length === 1, 'close zoom starts an independent asynchronous river request');
                labApplication.loadNearbyTowns = async () => { ++nearbyRequests; };
                globeCanvas.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight', bubbles: true}));
                check(pendingSurface.length > 0, 'surface request remains in flight during movement');
                pendingSurface.shift()!(riverReply());
                check(pending[0].signal?.aborted === true, 'moving while rivers load cancels the obsolete request');
                for (let i = 0; i < 40 && pending.length < 2; ++i) await new Promise(resolve => setTimeout(resolve, 25));
                check(pending.length === 2, 'new view fetches rivers without waiting for the old response');
                check(nearbyRequests > 0, 'surface completion cannot cancel the pending nearby-town request');
                pending[0].resolve(riverReply());
                await new Promise(resolve => setTimeout(resolve, 100));
                check(globeRoot.dataset.rivers !== 'ready', 'late cancelled river response cannot replace current imagery');
                pending[1].resolve(riverReply());
                for (let i = 0; i < 40 && globeRoot.dataset.rivers !== 'ready'; ++i)
                    await new Promise(resolve => setTimeout(resolve, 25));
                check(globeRoot.dataset.rivers === 'ready', 'current asynchronous river image reaches the live globe');
                const observer = [latitude.value, longitude.value, town.value, town.options.length];
                for (let i = 0; i < 9; ++i) zoomOut.click();
                await new Promise(resolve => setTimeout(resolve, 100));
                check(releasedTextures === 0, 'brief zoom-out retains the river texture during its grace period');
                for (let i = 0; i < 9; ++i) zoomIn.click();
                await new Promise(resolve => setTimeout(resolve, 2200));
                check(pending.length === 2 && releasedTextures === 0 && globeRoot.dataset.rivers === 'ready',
                      'returning during the grace period reuses rivers and cancels the pending unload');
                for (let i = 0; i < 9; ++i) zoomOut.click();
                await new Promise(resolve => setTimeout(resolve, 2200));
                check(releasedTextures === 1 && globeRoot.dataset.rivers === 'unloaded',
                      'sustained zoom-out releases regional texture storage asynchronously');
                check(JSON.stringify(observer) === JSON.stringify([latitude.value, longitude.value, town.value, town.options.length]),
                      'river unloading retains town choices and the selected observer');
                for (let i = 0; i < 9; ++i) zoomIn.click();
                for (let i = 0; i < 40 && pending.length < 3; ++i) await new Promise(resolve => setTimeout(resolve, 25));
                check(pending.length === 3, 'returning after eviction requests visible rivers again');
                pending[2].resolve(riverReply());
                for (let i = 0; i < 40 && globeRoot.dataset.rivers !== 'ready'; ++i)
                    await new Promise(resolve => setTimeout(resolve, 25));
                check(globeRoot.dataset.rivers === 'ready', 'evicted rivers reload without blocking the globe');
            } finally {
                gl.texImage2D = originalUpload;
                for (let i = 0; i < 24; ++i) zoomOut.click();
                pending.forEach(request => request.resolve(riverReply()));
                pendingSurface.forEach(resolve => resolve(riverReply()));
                labApplication.loadNearbyTowns = originalNearby;
                labHttp.fetch = originalFetch;
            }
            const bordersButton = globeRoot.querySelector<HTMLButtonElement>('[data-globe-action="borders"]')!;
            const borderFetch = labHttp.fetch, borderUpload = gl.texImage2D;
            const borderReplies: Array<{resolve: (response: MarsTransportResponse) => void; signal?: AbortSignal | null}> = [];
            let borderReleases = 0;
            try {
                check(bordersButton.getAttribute('aria-pressed') === 'false', 'country borders default to off');
                labHttp.fetch = (url, options) => url === '/almanac-borders' ?
                    new Promise(resolve => borderReplies.push({resolve, signal: options?.signal})) : borderFetch(url, options);
                gl.texImage2D = function (...args: unknown[]): void {
                    if (gl.getParameter(gl.ACTIVE_TEXTURE) === gl.TEXTURE4 && args.length === 9 &&
                        args[3] === 1 && args[4] === 1) ++borderReleases;
                    Reflect.apply(borderUpload, gl, args);
                };
                bordersButton.click();
                check(borderReplies.length === 1, 'enabling borders starts an asynchronous request');
                globeCanvas.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight', bubbles: true}));
                check(borderReplies[0].signal?.aborted === true, 'moving cancels obsolete border requests');
                for (let i = 0; i < 40 && borderReplies.length < 2; ++i) await new Promise(resolve => setTimeout(resolve, 25));
                check(borderReplies.length === 2, 'new viewport requests replacement borders');
                borderReplies[0].resolve(riverReply());
                await new Promise(resolve => setTimeout(resolve, 30));
                check(globeRoot.dataset.borders !== 'ready', 'stale border response cannot publish');
                borderReplies[1].resolve(riverReply());
                for (let i = 0; i < 40 && globeRoot.dataset.borders !== 'ready'; ++i)
                    await new Promise(resolve => setTimeout(resolve, 25));
                check(globeRoot.dataset.borders === 'ready', 'visible shared borders upload independently');
                for (let i = 0; i < 9; ++i) zoomIn.click();
                await new Promise(requestAnimationFrame);
                check(globeRoot.dataset.borders === 'zoom-hidden' && borderReleases > 0 &&
                      !gl.getUniform(program, gl.getUniformLocation(program, 'borderReady')!),
                      'close zoom hides coarse borders and releases their texture memory');
                for (let i = 0; i < 24; ++i) zoomOut.click();
                for (let i = 0; i < 40 && borderReplies.length < 3; ++i) await new Promise(resolve => setTimeout(resolve, 25));
                check(borderReplies.length === 3, 'zooming back out reloads borders');
                borderReplies[2].resolve(riverReply());
                for (let i = 0; i < 40 && globeRoot.dataset.borders !== 'ready'; ++i)
                    await new Promise(resolve => setTimeout(resolve, 25));
                bordersButton.click();
                await new Promise(requestAnimationFrame);
                check(globeRoot.dataset.borders === 'off' && borderReleases >= 2,
                      'switching borders off releases imagery without changing the observer');
            } finally {
                if (bordersButton.getAttribute('aria-pressed') === 'true') bordersButton.click();
                borderReplies.forEach(request => request.resolve(riverReply()));
                labHttp.fetch = borderFetch; gl.texImage2D = borderUpload;
                for (let i = 0; i < 24; ++i) zoomOut.click();
            }
        } finally {
            [latitude.value, longitude.value, elevation.value] = saved;
            town.value = savedTown;
            WorksheetLocations.selects.syncSelection(town);
            WorksheetLocations.calendarActions.triggerAlmanacAutoEvaluation = evaluate;
            panel.classList.toggle('hidden', hidden);
            window.scrollTo(scroll[0], scroll[1]);
        }
    }

    /** @brief Allow small library rounding differences while retaining NaN and signed-zero checks. */
    function close(actual: number, expected: number, message: string): void {
        if (Number.isNaN(expected)) check(Number.isNaN(actual), message);
        else if (expected === 0 || !Number.isFinite(expected)) check(Object.is(actual, expected), message);
        else check(Math.abs(actual - expected) <= 4e-15 * Math.abs(expected), message);
    }

    // At -90 degrees elevation and zero roll, only the azimuth half-angle remains.
    // Exercise the actual camera API rather than exporting library functions for tests.
    const angles = [-1e20, -720, -180, -90, -1, 0, 1, 90, 180, 720, 1e20];
    for (const angle of angles) {
        const quaternion = labSurfaceCamera.fromAngles([angle, -90, 0]);
        const half = angle * Math.PI / 360;
        check(quaternion[0] === 0 && quaternion[1] === 0, 'pure azimuth rotation at ' + angle);
        close(quaternion[2], Math.sin(half), 'camera sine at ' + angle);
        close(quaternion[3], Math.cos(half), 'camera cosine at ' + angle);
    }
    for (const angle of [-720, -361, -360, -1, -0, 0, 1, 359, 360, 361, 720]) {
        const input: MarsSurfaceAngles = [angle, 30, 27];
        const decoded = labSurfaceCamera.angles(labSurfaceCamera.fromAngles(input), input);
        const expected = ((angle % 360) + 360) % 360;
        const difference = Math.abs(decoded[0] - expected);
        check(decoded[0] >= 0 && decoded[0] <= 360 && Math.min(difference, 360 - difference) < 1e-10,
              'camera azimuth wraps whole turns and negative angles: ' + angle);
        check(Math.abs(decoded[1] - 30) < 1e-10 && Math.abs(decoded[2] - 27) < 1e-10,
              'angle wrapping preserves elevation and roll');
    }
    // A surface pinch needs only a valid separation here; no chart or range controls are changed.
    // The large finite fixture would overflow if its coordinates were squared before taking a root.
    for (const [x, y] of [[3, 4], [0, 8], [6, 8], [1e200, 1e200], [1e-200, 1e-200],
                          [Infinity, 1], [NaN, 1], [Number.MAX_VALUE, Number.MAX_VALUE]]) {
        const state = {pointerId: 1, touchX: 0, touchY: 0, secondX: x, secondY: y, pan: false};
        const event = {pointerId: 1, clientX: 0, clientY: 0};
        const accepted = labDOM.call('lab_plot_pinch_move', state, event);
        const distance = Math.hypot(x, y);
        check(Boolean(accepted) === (Number.isFinite(distance) && distance >= 8),
              'pinch separation is bounded without intermediate overflow: ' + x + ', ' + y);
    }
    const vectors = [[1, 1], [1, -1], [-1, -1], [-1, 1], [1, 0], [-1, 0], [0, 1], [-0, 1],
                     [0, -1], [-0, -1], [Number.MAX_VALUE, Number.MIN_VALUE],
                     [Number.MIN_VALUE, Number.MAX_VALUE], [Infinity, Infinity], [NaN, 1]];
    for (const [y, x] of vectors)
        close(native.lab_surface_atan2(y, x), Math.atan2(y, x), 'direction at ' + y + ', ' + x);
    for (const x of [0, -0]) {
        for (const y of [0, -0])
            check(Object.is(native.lab_surface_atan2(y, x), 0), 'zero-vector fallback');
    }
    const exponents = [-Infinity, -2000, -1022, -10.5, -1, -0, 0, 0.5, 1, 10.5, 1023, 2000, Infinity, NaN];
    for (const exponent of exponents) {
        const clamped = Math.max(-1022, Math.min(1023, exponent));
        close(native.lab_surface_exp2(exponent), 2 ** clamped, 'binary scale at ' + exponent);
    }
    for (const value of [Number.MIN_VALUE, 2 ** -1022, 0.125, 0.5, 1, 1.5, 2, 3, 1024, Number.MAX_VALUE])
        close(native.lab_surface_log2(value), Math.log2(value), 'binary logarithm at ' + value);
    for (const invalid of [-1, -0, 0, NaN, Infinity, -Infinity])
        check(Object.is(native.lab_surface_log2(invalid), 0), 'invalid ratio returns zero');
    for (let exponent = -1000; exponent <= 1000; exponent += 25)
        check(native.lab_surface_log2(native.lab_surface_exp2(exponent)) === exponent, 'exact power-of-two round trip');

    for (const filename of ['lab_browser.wasm', 'lab_codec.wasm']) {
        const response = await fetch('/wasm/' + filename);
        check(response.ok, filename + ' is available');
        const module = await WebAssembly.compile(await response.arrayBuffer());
        for (const entry of WebAssembly.Module.exports(module)) {
            check(!entry.name.startsWith('protobuf_') && !entry.name.startsWith('pthread_') &&
                  !['malloc', 'calloc', 'realloc', 'free'].includes(entry.name),
                  filename + ' exposes a library implementation detail: ' + entry.name);
        }
        for (const entry of WebAssembly.Module.imports(module)) {
            const allowed = entry.name.startsWith('lab_host_') ||
                (filename === 'lab_browser.wasm' && entry.name.startsWith('lab_dom_'));
            check(entry.kind === 'function' && entry.module === 'env' && allowed,
                  filename + ' has unexpected host import: ' + entry.module + '.' + entry.name);
        }
    }
};
