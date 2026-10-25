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

    await checkGlobeHTTP(check);
    await checkGlobeGPU(check);
    await checkGlobePhoto(check);
    const regionalSvg = window.labAlmanacFixtures()[3];
    const regionalFlag = 'data:image/svg+xml;base64,' + btoa(regionalSvg);
    const existingCountry = document.getElementById('almanacJurisdiction');
    if (existingCountry) existingCountry.id = 'regional-test-saved-jurisdiction';
    const shell = document.createElement('div');
    shell.className = 'select-shell';
    shell.innerHTML = '<select id="almanacJurisdiction"><option value="GB-ENG">England</option></select>' +
        '<button class="select-button" data-country-option="1" data-country-code="GB-ENG">England</button>';
    document.body.append(shell);
    const selected = shell.querySelector<HTMLButtonElement>('button')!;
    try {
        const parentFlag = 'data:image/svg+xml;base64,' + btoa('<svg xmlns="http://www.w3.org/2000/svg"/>');
        WorksheetResults.tooltip.rememberCountry('GB-ENG', 'England', parentFlag);
        WorksheetResults.tooltip.rememberCountry('GB-CON', 'Cornwall, England', regionalFlag);
        WorksheetResults.tooltip.selectedObserverFlag('GB-CON');
        check(selected.querySelector('img')?.getAttribute('src') === regionalFlag &&
              shell.querySelector('select')!.value === 'GB-ENG',
              'selected country control shows regional animation without changing jurisdiction');
        WorksheetResults.tooltip.show(selected);
        check(selected.querySelector('img')?.getAttribute('src') === regionalFlag &&
              selected.dataset.countryFlag === regionalFlag,
              'hovering the selected dropdown preserves its resolved regional artwork');
        WorksheetResults.tooltip.selectedObserverFlag('');
        check(selected.querySelector('img')?.getAttribute('src') === parentFlag,
              'changing location replaces stale regional selection artwork with the cached parent flag');
        shell.querySelector('select')!.append(new Option('Country not identified', ''));
        shell.querySelector('select')!.value = '';
        selected.dataset.countryCode = '';
        selected.dataset.countryOption = '';
        WorksheetResults.tooltip.rememberCountry('ZZ-SEA', 'Offshore', '/offshore_flag.svg');
        WorksheetResults.tooltip.selectedObserverFlag('ZZ-SEA');
        check(selected.querySelector('img')?.getAttribute('src') === '/offshore_flag.svg' &&
              shell.querySelector('select')!.value === '', 'offshore artwork never selects a fictitious country');
        check(!labDOM.call('lab_flag_cache_put', 'ZZ-SEA', 'Offshore', 'https://example.invalid/flag.png') &&
              !labDOM.call('lab_flag_cache_put', 'ZZ-SEA', 'Offshore', '/other.svg') &&
              !labDOM.call('lab_flag_cache_put', 'ZZ-SEA', 'Offshore', '/offshore_flag.png'),
              'local vector artwork permits only the bundled SVG, not raster or arbitrary sources');
    } finally {
        WorksheetResults.tooltip.hide(); shell.remove();
        if (existingCountry) existingCountry.id = 'almanacJurisdiction';
        labDOM.call('lab_flag_cache_clear');
    }
    const previewCard = document.createElement('div');
    previewCard.style.cssText = 'position:fixed;left:20px;top:20px;width:700px';
    previewCard.innerHTML = window.labAlmanacFixtures()[0].almanac_presentation.all.html;
    document.body.append(previewCard);
    const previewButton = previewCard.querySelector<HTMLButtonElement>('.almanac-location-flag')!;
    const smallFlag = previewButton.querySelector<HTMLImageElement>('img.country-flag')!;
    smallFlag.src = regionalFlag;
    try {
        previewButton.setAttribute('aria-describedby', 'original-description');
        smallFlag.dispatchEvent(new MouseEvent('pointerover', {bubbles: true}));
        const popup = document.querySelector<HTMLElement>('#marsButtonTooltip')!;
        check(popup.classList.contains('visible') && popup.classList.contains('mars-flag-tooltip'),
              'hovering only the Location flag opens its image preview through native event dispatch');
        // Headless Firefox reports no hover device; keyboard focus permits tooltip layout there too.
        previewButton.focus({focusVisible: true} as FocusOptions);
        const image = popup.querySelector<HTMLImageElement>('img.country-flag')!;
        await image.decode();
        const smallRect = smallFlag.getBoundingClientRect(), largeRect = image.getBoundingClientRect();
        check(smallRect.width === 24 && smallRect.height === 18 &&
              largeRect.width === smallRect.width * 14 && largeRect.height === smallRect.height * 14 &&
              image.src === smallFlag.src && popup.textContent === '',
              'preview is exactly 336 by 252 pixels and reuses the same SVG without enlarging the original: ' +
              JSON.stringify({small: [smallRect.width, smallRect.height], large: [largeRect.width, largeRect.height],
                  sameSource: image.src === smallFlag.src, text: popup.textContent}));
        check(image.alt === previewButton.getAttribute('aria-label') &&
              previewButton.getAttribute('aria-describedby')!.includes('marsButtonTooltip'),
              'image preview has an accessible description');
        const canvas = document.createElement('canvas');
        canvas.width = 100; canvas.height = 75;
        const context = canvas.getContext('2d')!;
        const pixel = () => {
            context.clearRect(0, 0, 100, 75);
            context.drawImage(image, 0, 0, 100, 75);
            return context.getImageData(10, 10, 1, 1).data;
        };
        const first = pixel();
        check(first[3] > 240 && first[0] < 25, 'native regional SVG renders St Piran first inside an image');
        const reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;
        await new Promise(resolve => window.setTimeout(resolve, 3750));
        check(pixel()[0] < 25, 'regional flag holds for five seconds before the unchanged 2.5-second fade');
        await new Promise(resolve => window.setTimeout(resolve, 2500));
        const outward = pixel();
        check(outward[3] > 240 && (reduced ? outward[0] < 25 : outward[0] > 50 && outward[0] < 205),
              'regional artwork gradually blends with England halfway through the outward fade');
        await new Promise(resolve => window.setTimeout(resolve, 2500));
        const second = pixel();
        check(second[3] > 240 && (reduced ? second[0] < 25 : second[0] > 230),
              'parent flag holds after the fade, or stays regional with reduced motion');
        await new Promise(resolve => window.setTimeout(resolve, 5000));
        const inward = pixel();
        check(inward[3] > 240 && (reduced ? inward[0] < 25 : inward[0] > 30 && inward[0] < 200),
              'parent artwork gradually blends back into the regional flag');
        await new Promise(resolve => window.setTimeout(resolve, 1500));
        check(pixel()[0] < 25, 'enlarged preview retains the same fifteen-second flag sequence');
        smallFlag.src = '/offshore_flag.svg';
        WorksheetResults.tooltip.show(previewButton);
        const offshorePreview = popup.querySelector<HTMLImageElement>('img')!;
        await offshorePreview.decode();
        const offshoreResponse = await fetch('/offshore_flag.svg');
        const offshoreSvg = new DOMParser().parseFromString(await offshoreResponse.text(), 'image/svg+xml');
        check(offshoreResponse.ok && offshoreResponse.headers.get('Content-Type') === 'image/svg+xml' &&
              offshoreSvg.documentElement.localName === 'svg' && !!offshoreSvg.querySelector('path') &&
              !offshoreSvg.querySelector('image, script, foreignObject, parsererror'),
              'offshore emblem is genuine vector artwork, with no embedded bitmap or active content');
        check(offshorePreview.getAttribute('src') === '/offshore_flag.svg' && offshorePreview.naturalWidth > 1000 &&
              offshorePreview.getBoundingClientRect().width === 336 && offshorePreview.getBoundingClientRect().height === 252,
              'bundled offshore artwork decodes and uses the same enlarged preview');
        for (const [width, height] of [[320, 640], [640, 240]]) {
            labDOM.call('lab_tooltip_show', previewButton, previewButton, width, height);
            const fitted = popup.querySelector<HTMLImageElement>('img')!.getBoundingClientRect();
            check(fitted.width <= width - 48 && fitted.height <= height - 48 &&
                  Math.abs(fitted.width / fitted.height - 4 / 3) < .001,
                  'enlarged flag scales proportionally to fit narrow or short viewports');
        }
        WorksheetResults.tooltip.show(previewButton);
        const spacer = document.createElement('div');
        spacer.style.height = '300px';
        previewCard.prepend(spacer);
        previewCard.style.height = '400px';
        previewCard.style.overflow = 'auto';
        WorksheetResults.tooltip.show(previewButton);
        const popupHeight = popup.getBoundingClientRect().height;
        check(Math.abs(parseFloat(popup.style.top) + popupHeight + 8 -
              previewButton.getBoundingClientRect().top) < .1 && popup.parentElement === document.body,
              'flag preview prefers above and can overlap its scrollable card without being clipped');
        previewCard.scrollTop = previewButton.getBoundingClientRect().top - 20;
        WorksheetResults.tooltip.show(previewButton);
        check(previewButton.getBoundingClientRect().top < popupHeight + 16 &&
              Math.abs(parseFloat(popup.style.top) - previewButton.getBoundingClientRect().bottom - 8) < .1,
              'scrolling the flag near the viewport top puts the preview below when above cannot fit');
        previewCard.scrollTop = 0;
        WorksheetResults.tooltip.show(previewButton);
        check(Math.abs(parseFloat(popup.style.top) + popupHeight + 8 -
              previewButton.getBoundingClientRect().top) < .1,
              'reopening after scrolling back prefers above again');
        document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape', bubbles: true}));
        check(!popup.classList.contains('visible') &&
              previewButton.getAttribute('aria-describedby') === 'original-description',
              'Escape dismisses the flag preview and restores its original description');
        previewButton.blur();
        previewButton.focus();
        check(popup.classList.contains('visible') && popup.classList.contains('mars-flag-tooltip'),
              'keyboard focus also opens the flag preview');
        previewButton.blur();
        check(!popup.classList.contains('visible'), 'leaving keyboard focus dismisses the preview');
        smallFlag.dispatchEvent(new MouseEvent('pointerover', {bubbles: true}));
        smallFlag.dispatchEvent(new MouseEvent('pointerout', {bubbles: true}));
        check(!popup.classList.contains('visible'), 'leaving the flag dismisses its hover preview');
        previewCard.append(previewButton);
        WorksheetResults.tooltip.show(previewButton);
        check(!popup.classList.contains('mars-flag-tooltip') && !popup.querySelector('img'),
              'the same flag control outside the summary never gets an enlarged preview');
    } finally {
        WorksheetResults.tooltip.hide();
        previewCard.remove();
    }

    const borderCamera = [0, 0, 0, 1];
    const localFrame = [0, 0, 0, 1, 64, 800];
    const identity = labDOM.call('lab_globe_frame_transform', localFrame, localFrame) as number[];
    check(identity.every((value, i) => Math.abs(value - [1, 0, 0, 1, 0, 0][i]) < 1e-8),
          'retained vector transform is exactly identity for an unchanged camera');
    const shifted = labDOM.call('lab_globe_rotate', borderCamera, 64, .1, -.05, 0) as number[];
    const affine = labDOM.call('lab_globe_frame_transform', localFrame, [...shifted, 64, 800]) as number[];
    const centrePoint = labDOM.call('lab_globe_project', 0, 0, shifted) as number[];
    check(Math.abs(affine[0] * 400 + affine[2] * 400 + affine[4] - (400 + centrePoint[0] * 800 * 64 / 2.24)) < 1e-7 &&
          Math.abs(affine[1] * 400 + affine[3] * 400 + affine[5] - (400 + centrePoint[1] * 800 * 64 / 2.24)) < 1e-7,
          'native retained vectors track the same camera centre as markers and picking');
    const shoreLoad = (text: string | Uint8Array<ArrayBuffer>, bounds = [-180, -90, 180, 90]) =>
        Boolean(labDOM.call('lab_globe_shore_load', text, bounds));
    const shoreSvg = (camera = borderCamera, zoom = 1) =>
        labDOM.call('lab_globe_shore_svg', camera, zoom, 800) as string | null;
    try {
        const square = 'M1 0 -5 -5\nL5 -5\nL5 5\nL-5 5\nZ\n';
        check(shoreLoad(square), 'WASM accepts bounded geographic shoreline polygons');
        const front = shoreSvg();
        const packed = new Uint8Array(4 + 5 * 17);
        packed.set([77, 83, 72, 49]);
        const data = new DataView(packed.buffer);
        [[1, -5, -5], [16, 5, -5], [16, 5, 5], [16, -5, 5], [0, 0, 0]].forEach((point, i) => {
            packed[4 + i * 17] = point[0];
            data.setFloat64(5 + i * 17, point[1], true);
            data.setFloat64(13 + i * 17, point[2], true);
        });
        const decoded = labWire.decode(labWire.encode({vectors: packed})) as {vectors: Uint8Array<ArrayBuffer>};
        check(decoded.vectors instanceof Uint8Array && decoded.vectors.every((byte, i) => byte === packed[i]),
              'binary coordinates round-trip through Protobuf without text encoding');
        const workerDecoded = await labWire.decodeAsync(labWire.encode({vectors: packed}).buffer as ArrayBuffer) as
            {vectors: Uint8Array<ArrayBuffer>};
        check(workerDecoded.vectors instanceof Uint8Array && workerDecoded.vectors.every((byte, i) => byte === packed[i]),
              'background codec delivers the same opaque binary buffer');
        check(shoreLoad(decoded.vectors) && shoreSvg() === front, 'binary and fixture shoreline projection agree');
        check(!shoreLoad(packed.slice(0, -1)) && shoreSvg() === front, 'truncated binary replacement is atomic');
        const malformed = packed.slice();
        new DataView(malformed.buffer).setFloat64(5, NaN, true);
        check(!shoreLoad(malformed) && shoreSvg() === front, 'non-finite binary coordinates are rejected');
        check(Boolean(front?.includes('<path fill="#465522"')) && !front!.includes('<image'),
              'native shoreline output contains live filled SVG paths, not raster images');
        check(!shoreLoad('M1 0 NaN 0\nZ\n') && shoreSvg() === front,
              'malformed shoreline replacement preserves the accepted native cache');
        check(!shoreLoad('M1 0 0 0\nL1 1\n') && !shoreLoad('M5 0 0 0\nL1 1\nL1 0\nZ\n'),
              'shoreline admission rejects incomplete rings and invalid nesting levels');
        // Actual collapsed islands returned by the native Málaga viewport at intermediate zoom levels.
        const collapsed = 'M1 0 -4.513957500 36.596137900\nZ\n' +
            'M1 0 -5.481348100 35.911432600\nL-5.481880000 35.911179000\nZ\n';
        check(shoreLoad(collapsed + square + collapsed) && shoreSvg() === front,
              'collapsed display islands are discarded without rejecting or changing the detailed coastline');
        check(shoreLoad(collapsed) && !shoreSvg()!.includes('<path'),
              'a view containing only collapsed islands remains valid water rather than photographic fallback');
        const lines = 'M1 0 -8 35\nL-5 35\nL-3 35\nL-8 35\nZ\n' +
            'M1 0 18 -36\nL18 -35\nL18 -34\nL18 -36\nZ\n' +
            'M1 0 0 0\nL0 0\nL1 1\nL2 2\nZ\n';
        check(shoreLoad(lines) && !shoreSvg()!.includes('<path'),
              'collinear clipping remnants never become stroked lines across water');
        check(shoreLoad(lines + square) && shoreSvg() === front,
              'discarding zero-width clipping remnants leaves real coastlines unchanged');
        // A concave polygon clipped at longitude zero becomes two filled pieces
        // joined by a retraced, zero-width edge. That join is not coastline.
        const bridge = 'M1 0 0 -10\nL-10 -10\nL-10 -5\nL0 -5\nL0 5\nL-10 5\nL-10 10\nL0 10\nZ\n';
        check(shoreLoad(bridge), 'clipped disconnected land pieces load');
        const bridgeImage = new Image(), bridgeURL = URL.createObjectURL(new Blob([
            `<svg xmlns="http://www.w3.org/2000/svg" width="800" height="800">${shoreSvg()}</svg>`
        ], {type: 'image/svg+xml'}));
        try {
            bridgeImage.src = bridgeURL;
            await bridgeImage.decode();
            const canvas = document.createElement('canvas');
            canvas.width = canvas.height = 800;
            const context = canvas.getContext('2d')!;
            context.drawImage(bridgeImage, 0, 0);
            const pixels = context.getImageData(399, 380, 2, 40).data;
            check(pixels.every((value, i) => value === [7, 20, 64, 255][i % 4]),
                  'retraced clipping joins do not draw straight coastlines through open water');
        } finally { URL.revokeObjectURL(bridgeURL); }
        const curvedBridge = 'M1 0 -1.31 48.17\nL-2.71 48.17\nL-2.71 48.93\nL-1.31 48.93\n' +
            'L-1.31 49.83\nL-2.71 49.83\nL-2.71 50.91\nL-1.31 50.91\nZ\n';
        const separatePieces = 'M1 0 -1.31 48.17\nL-2.71 48.17\nL-2.71 48.93\nL-1.31 48.93\nZ\n' +
            'M1 0 -1.31 49.83\nL-2.71 49.83\nL-2.71 50.91\nL-1.31 50.91\nZ\n';
        const camera = labDOM.call('lab_globe_rotate', labDOM.call('lab_globe_centre', 49.51, -3.17), 1, 0, 0, .37) as number[];
        const raster = async (text: string): Promise<Uint8ClampedArray> => {
            check(shoreLoad(text), 'curved clipping fixture loads');
            const image = new Image(), url = URL.createObjectURL(new Blob([
                `<svg xmlns="http://www.w3.org/2000/svg" width="800" height="800">${shoreSvg(camera, 32)}</svg>`
            ], {type: 'image/svg+xml'}));
            try {
                image.src = url;
                await image.decode();
                const canvas = document.createElement('canvas');
                canvas.width = canvas.height = 800;
                const context = canvas.getContext('2d')!;
                context.drawImage(image, 0, 0);
                return context.getImageData(0, 0, 800, 800).data;
            } finally { URL.revokeObjectURL(url); }
        };
        const background = 'M1 0 -10 40\nL10 40\nL10 60\nL-10 60\nZ\n';
        const bridgeCases: [string, string, number[]][] = [
            [curvedBridge, separatePieces, [7, 20, 64]],
            [curvedBridge.replaceAll('M1 0', 'M1 1'), separatePieces.replaceAll('M1 0', 'M1 1'), [7, 20, 64]],
            [background + curvedBridge.replaceAll('M1 0', 'M2 0'),
                background + separatePieces.replaceAll('M1 0', 'M2 0'), [70, 85, 34]]
        ];
        for (const [joined, separate, colour] of bridgeCases) {
            const actual = await raster(joined), expected = await raster(separate);
            let changed = 0;
            for (let y = 1; y < 799; ++y) for (let x = 1; x < 799; ++x) {
                const offset = (y * 800 + x) * 4;
                if (colour.some((value, channel) => expected[offset + channel] !== value) ||
                    colour.every((value, channel) => actual[offset + channel] === value)) continue;
                let boundary = false;
                for (let dy = -1; dy <= 1; ++dy) for (let dx = -1; dx <= 1; ++dx)
                    if (colour.some((value, channel) => expected[((y + dy) * 800 + x + dx) * 4 + channel] !== value))
                        boundary = true;
                if (!boundary) ++changed;
            }
            check(changed === 0, `projected clipping joins cancel for land, ice and lakes (${changed} pixels)`);
        }
        check(shoreLoad('M1 0 0 0\nL1 0\nL1 0.000000001\nZ\n') && shoreSvg()!.includes('<path'),
              'exact degeneracy checks retain extremely thin real polygons');
        check(shoreLoad(square), 'restore valid shoreline after collapsed-island checks');
        const rear = labDOM.call('lab_globe_centre', 0, 180) as number[];
        check(!shoreSvg(rear)!.includes('<path'), 'SVG shoreline hides rear-hemisphere polygons');
        check(shoreLoad('M1 0 85 -5\nL95 -5\nL95 5\nL85 5\nZ\n') &&
              Boolean(shoreSvg()?.includes('A')) && !shoreSvg()!.includes('NaN'),
              'native hemisphere clipping closes a crossing polygon along the curved limb');
        check(shoreLoad('M1 0 175 -5\nL185 -5\nL185 5\nL175 5\nZ\n', [170, -10, 190, 10]) &&
              Boolean(shoreSvg(rear)?.includes('<path')) &&
              Boolean(labDOM.call('lab_globe_shore_covers', [0, -180, 1, -185, -5, -175, 5])),
              'SVG polygons and cached coverage cross the date line without a world-spanning chord');
        check(Boolean(labDOM.call('lab_globe_shore_frame_covers', [0, 0, 16, -5, -5, 5, 5], [-10, -10, 10, 10])) &&
              !labDOM.call('lab_globe_shore_frame_covers', [0, 0, 16, -15, -5, 5, 5], [-10, -10, 10, 10]),
              'published frame coverage survives cache replacement but rejects newly exposed geography');
        for (const sign of [-1, 1]) {
            const polar = `M1 1 -5 ${sign * 80}\nL5 ${sign * 80}\nL5 ${sign * 90}\nL-5 ${sign * 90}\nZ\n`;
            const camera = labDOM.call('lab_globe_centre', sign * 90, 0) as number[];
            check(shoreLoad(polar), 'native SVG accepts polar land without latitude clamping');
            let colours = '';
            for (const zoom of [1, 1.99, 2, 7.99, 8, 16]) {
                const svg = shoreSvg(camera, zoom)!;
                const parsed = new DOMParser().parseFromString(
                    `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 800 800">${svg}</svg>`, 'image/svg+xml');
                const stops = [...parsed.querySelectorAll('stop')].map(stop => stop.getAttribute('stop-color')).join(',');
                if (!colours) colours = stops;
                check(!parsed.querySelector('parsererror') && Boolean(parsed.querySelector('linearGradient')) &&
                      stops === colours && !svg.includes('data-ice=') && !svg.includes('<pattern') &&
                      !svg.includes('<image') && !svg.includes('<filter'),
                      'polar surface colour is zoom-independent, with no invented floes or photographic ice');
                const path = parsed.querySelector('path')!;
                check(path.getAttribute('stroke') === path.getAttribute('fill') &&
                      path.getAttribute('stroke-width') === '1' &&
                      path.getAttribute('vector-effect') === 'non-scaling-stroke',
                      'polar patch edges cover antialiasing seams with their own fill, not apparent gridlines');
            }
        }
        const polarTiles = [-10, 0].map(west =>
            `M1 1 ${west} -80\nL${west + 10} -80\nL${west + 10} -90\nL${west} -90\nZ\n`).join('');
        check(shoreLoad(polarTiles), 'adjacent polar patches load');
        const polarCamera = labDOM.call('lab_globe_centre', -90, 0) as number[];
        const image = new Image();
        image.src = URL.createObjectURL(new Blob([
            `<svg xmlns="http://www.w3.org/2000/svg" width="800" height="800">${shoreSvg(polarCamera)}</svg>`
        ], {type: 'image/svg+xml'}));
        try {
            await image.decode();
            const canvas = document.createElement('canvas');
            canvas.width = canvas.height = 800;
            const context = canvas.getContext('2d')!;
            context.drawImage(image, 0, 0);
            const point = labDOM.call('lab_globe_project', -85, 0, polarCamera) as number[];
            const x = Math.floor(400 + point[0] * 800 / 2.24);
            const y = Math.floor(400 + point[1] * 800 / 2.24);
            const pixel = context.getImageData(x, y, 1, 1).data;
            check(pixel[0] >= 235 && pixel[1] >= 237 && pixel[2] >= 234,
                  'rendered adjacent Antarctic patches have no dark meridian seam when no grid is drawn');
        } finally { URL.revokeObjectURL(image.src); }
        check(shoreLoad(square + 'M2 0 -2 -2\nL2 -2\nL2 2\nL-2 2\nZ\n' +
                        'M3 0 -1 -1\nL1 -1\nL1 1\nL-1 1\nZ\n') &&
              (shoreSvg()!.match(/<path/g) || []).length === 3,
              'live SVG retains land, lake and island nesting');
    } finally { labDOM.call('lab_globe_shore_clear'); }
    const dense = globeFixtureBytes('M1 0 -5 -5\nL5 -5\nL5 5\nL-5 5\nZ\n'.repeat(4096));
    const denseFixture = globeHTTPFixture(async () => new Response(labWire.encode({ok: true, vectors: dense}),
        {headers: {'Content-Type': 'application/x-protobuf'}}));
    const projectionWorker = new denseFixture.Worker('/js/globe_worker.js');
    let workerId = 0;
    const workerCall = (entry: GlobeWorkerRequest['entry'], args: GlobeWorkerRequest['args']) =>
        new Promise<unknown>((resolve, reject) => {
            projectionWorker.onerror = event => reject(new Error(event.message));
            projectionWorker.onmessage = event => event.data.error ? reject(new Error(event.data.error)) : resolve(event.data.value);
            projectionWorker.postMessage({id: ++workerId, entry, args, geometry: 1});
        });
    let heartbeat = 0;
    const pulse = new MessageChannel();
    try {
        check(Boolean(await workerCall('fetch', [-180, -90, 180, 90])),
              'dedicated worker admits a dense native shoreline fixture');
        pulse.port1.onmessage = () => {
            if (++heartbeat < 10000) pulse.port2.postMessage(null);
        };
        pulse.port2.postMessage(null);
        const began = performance.now();
        const svg = await workerCall('lab_globe_shore_svg', [[0, 0, 0, 1], 1, 800]);
        check(typeof svg === 'string' && svg.includes('<path') && heartbeat > 0,
              `browser event loop keeps running while WASM projects 4,096 shoreline rings off-thread: ${typeof svg}, ${typeof svg === 'string' ? svg.length : 0} bytes, ${heartbeat} heartbeats`);
        console.log(`GLOBE WORKER: ${Math.round(performance.now() - began)} ms projection, ${heartbeat} UI heartbeats`);
    } finally {
        pulse.port1.close();
        pulse.port2.close();
        denseFixture.dispose();
    }
    check(Boolean(labDOM.call('lab_globe_nearby_covers', 10, -179.8, 10, 179.8)) &&
          !labDOM.call('lab_globe_nearby_covers', 12, -179.8, 10, 179.8) &&
          !labDOM.call('lab_globe_nearby_covers', NaN, 0, 0, 0),
          'native nearby-town reuse handles the date line and rejects invalid or distant centres');
    labDOM.call('lab_globe_markers_begin', 800);
    const admitMarker = (x: number, y: number, width = 80, town = true, selected = false) =>
        labDOM.call('lab_globe_markers_place', x, y, width, Number(town), Number(selected)) as number[] | null;
    check(JSON.stringify(admitMarker(100, 100)) === '[100,100]', 'native marker admission preserves pixels');
    check(admitMarker(110, 100) === null && Boolean(admitMarker(300, 100)),
          'native direct-indexed grid rejects overlap but admits separated labels');
    check(Boolean(admitMarker(110, 100, 80, true, true)), 'selected marker bypasses overlap suppression');
    check(admitMarker(790, 100)?.[2] === -1 && admitMarker(100, 5) === null,
          'right-edge labels can face left while vertical clipping protects edges');
    labDOM.call('lab_globe_markers_begin', 800);
    check(Boolean(admitMarker(100, 100)), 'new frame resets native occupancy');
    check(JSON.stringify(admitMarker(790, 799, 80, true, true)) === '[720,786]',
          'selected marker is clamped inside the viewport');
    labDOM.call('lab_globe_markers_begin', 800);
    admitMarker(380, 400, 28, false);
    check(JSON.stringify(admitMarker(340, 400, 78)) === '[262,400,-1]',
          'a nearby observer cannot suppress a town when its label fits on the other side');
    labDOM.call('lab_globe_markers_begin', Infinity);
    check(admitMarker(100, 100) === null, 'invalid viewport disables native marker admission');
    check(labDOM.call('lab_globe_coordinate', -4.42, 'E', 'W') === '4° 25.20′ W' &&
          labDOM.call('lab_globe_coordinate', 0, 'N', 'S') === '0°' &&
          labDOM.call('lab_globe_coordinate', 12.9999999, 'N', 'S') === '13° N',
          'native coordinate formatting preserves hemispheres, zero and minute rollover');
    check(JSON.stringify(labDOM.call('lab_globe_marker_pixels', [0, 0, 1], 1, 800)) === '[400,400]' &&
          labDOM.call('lab_globe_marker_pixels', [0, 0, -1], 1, 800) === null &&
          labDOM.call('lab_globe_marker_pixels', [1, 0, 1], 10, 800) === null,
          'native viewport projection clips the far hemisphere and offscreen points');
    const borderGeometry = (camera = borderCamera, zoom = 1, size = 800): string | null =>
        labDOM.call('lab_globe_borders_path', camera, zoom, size) as string | null;
    try {
        const covered = (bounds: number[]) => Boolean(labDOM.call('lab_globe_borders_covers', [0, 0, 1, ...bounds]));
        check(Boolean(labDOM.call('lab_globe_borders_load_region', '-10 0 10 0\n', [-180, -90, 180, 90])) &&
              covered([170, -80, 530, 80]), 'native worldwide coverage recognises wrapped views');
        check(Boolean(labDOM.call('lab_globe_borders_load_region', '175 0 185 0\n', [170, -20, 190, 20])) &&
              covered([-185, -10, -175, 10]) && !covered([-185, -30, -175, 10]) &&
              !covered([-170, -10, -160, 10]), 'native regional coverage checks longitude wrapping and latitude');
        check(!labDOM.call('lab_globe_borders_load_region', '0 0 NaN 0', [-180, -90, 180, 90]) &&
              covered([-185, -10, -175, 10]) && !covered([-170, -10, -160, 10]),
              'invalid geometry cannot replace native coverage');
        labDOM.call('lab_globe_borders_clear');
        check(!covered([-185, -10, -175, 10]), 'clearing native borders also clears geographic coverage');
        check(Boolean(labDOM.call('lab_globe_borders_load', '-10 0 10 0\n')), 'native border coordinates load');
        const path = borderGeometry()!;
        check(path.startsWith('M337.98 400.00L') && path.endsWith('462.02 400.00'),
              'border projection agrees with the globe equator and screen scale');
        check(!labDOM.call('lab_globe_borders_load', '0 0 NaN 0') && borderGeometry() === path,
              'invalid replacement preserves accepted native geometry');
        check(!labDOM.call('lab_globe_borders_load', '0 91 1 0') &&
              !labDOM.call('lab_globe_borders_load', '0 0 1'), 'invalid latitude and incomplete segments are rejected');
        labDOM.call('lab_globe_borders_load', '170 0 -170 0\n');
        check(borderGeometry() === '', 'far hemisphere borders are hidden');
        const dateLine = labDOM.call('lab_globe_centre', 0, 180) as number[];
        check(Boolean(borderGeometry(dateLine)) && borderGeometry(dateLine)!.length < 1000,
              'antimeridian segment follows the short visible arc');
        labDOM.call('lab_globe_borders_load', '80 0 100 0\n');
        check(borderGeometry()!.endsWith('757.14 400.00'), 'crossing border clips exactly at the visible limb');
        labDOM.call('lab_globe_borders_load', '-10 0 10 0\n');
        const rolled = [0, 0, Math.SQRT1_2, Math.SQRT1_2];
        check(borderGeometry(rolled)!.startsWith('M400.00 462.02L'), 'border projection follows camera roll');
        const close = borderGeometry(borderCamera, 10)!;
        check(close.startsWith('M0.00 400.00L') && close.endsWith('800.00 400.00'),
              'zoomed border is clipped to the viewport');
        for (const zoom of [24, 64, 1024, 4096]) {
            const detailed = borderGeometry(borderCamera, zoom);
            check(Boolean(detailed?.startsWith('M0.00 400.00L')) && detailed!.endsWith('800.00 400.00'),
                `borders remain visible and viewport-clipped at ${zoom}x zoom`);
        }
        check(borderGeometry(borderCamera, 4097) === null, 'border projection rejects zoom beyond the camera limit');
        // Match the resident geometry size of a real world response, not just a small regional fixture.
        check(Boolean(labDOM.call('lab_globe_borders_load', '0 0 0.01 0.01\n'.repeat(80000))),
            'world-sized border geometry fits alongside the application');
        const begin = performance.now();
        check(Boolean(borderGeometry()), 'world-sized border layer projects without reserving a maximum-sized path');
        check(performance.now() - begin < 2000, 'large border projection has bounded execution time');
    } finally { labDOM.call('lab_globe_borders_clear'); }

    const flag = 'data:image/svg+xml;base64,' + btoa('<svg xmlns="http://www.w3.org/2000/svg"/>');
    const cachedFlag = (code: string) => labDOM.call('lab_flag_cache_get', code) as {name: string; flag: string} | null;
    const flagStats = () => labDOM.call('lab_flag_cache_stats') as {entries: number; bytes: number};
    labDOM.call('lab_flag_cache_clear');
    try {
        check(Boolean(labDOM.call('lab_flag_cache_put', 'BR', 'Brasil — 巴西', flag)), 'native flag insertion succeeds');
        check(cachedFlag('BR')?.name === 'Brasil — 巴西' && cachedFlag('BR')?.flag === flag,
              'dictionary preserves copied Unicode names and artwork across bridge calls');
        check(flagStats().bytes === new TextEncoder().encode('Brasil — 巴西' + flag).length,
              'native cache accounts for actual UTF-8 text storage');
        check(Boolean(labDOM.call('lab_flag_cache_put', 'BR', 'Brazil', flag)) && flagStats().entries === 1 &&
              cachedFlag('BR')?.name === 'Brazil', 'replacement updates one native dictionary entry');
        check(!labDOM.call('lab_flag_cache_put', 'br', 'invalid', flag) &&
              !labDOM.call('lab_flag_cache_put', 'BR', 'bad\ud800', flag) &&
              !labDOM.call('lab_flag_cache_put', 'BR', 'bad', 'javascript:bad') &&
              cachedFlag('BR')?.name === 'Brazil', 'invalid metadata preserves the cached entry');
        labDOM.call('lab_flag_cache_clear');
        const codes = Array.from({length: 33}, (_, i) => String.fromCharCode(65 + Math.floor(i / 26), 65 + i % 26));
        for (const code of codes) check(Boolean(labDOM.call('lab_flag_cache_put', code, code, flag)), 'bounded cache insert');
        check(flagStats().entries === 32 && !cachedFlag(codes[0]) && Boolean(cachedFlag(codes[32])),
              'the 33rd flag evicts the oldest insertion');
        labDOM.call('lab_flag_cache_clear');
        const large = 'data:image/svg+xml;base64,' + 'A'.repeat(600000);
        const largeFirst = labDOM.call('lab_flag_cache_put', 'BR', 'Brazil', large);
        const largeSecond = labDOM.call('lab_flag_cache_put', 'UY', 'Uruguay', large);
        check(Boolean(largeFirst) && Boolean(largeSecond) &&
              flagStats().entries === 1 && flagStats().bytes <= 1048576 && !cachedFlag('BR'),
              `byte pressure evicts entries before the count limit is reached: ${largeFirst}, ${largeSecond}, ${JSON.stringify(flagStats())}`);
        check(!labDOM.call('lab_flag_cache_put', 'UY', 'too large', large + large) &&
              cachedFlag('UY')?.name === 'Uruguay', 'oversized replacement leaves the old flag intact');
    } finally { labDOM.call('lab_flag_cache_clear'); }
    check(flagStats().entries === 0 && flagStats().bytes === 0, 'cache shutdown releases entries and accounting');

    const town = (name: string, latitude: string, longitude: string): LabTown => ({
        name, latitude, longitude, elevation: '0', timezone: '', default: false, value: name, detail: name
    });
    const townBatch = [town('Native western', '0', '-180'), town('Native eastern', '0', '180'),
        town('Native pole', '90', '0')];
    check(labDOM.call('lab_town_store_add', 'ZZ', townBatch) === 3,
          'native array stores a town batch');
    check(labDOM.call('lab_town_store_add', 'ZZ', townBatch) === 0,
          'native dictionary deduplicates catalogue rows');
    const sourceNames = ['Heidelberg (GeoNames 123)', 'Heidelberg (GeoNames 456)',
        'Newport (Harbour) (GeoNames 789)'];
    check(labDOM.call('lab_town_store_add', 'ZA', sourceNames.map(name => town(name, '-34', '18'))) === 3,
          'same-name towns retain independent catalogue keys');
    const labelledGlobe = MarsAlmanacGlobe as unknown as {
        townLabel(place: GlobeTown, zoom: number, available: number): {name: string; full: string; width: number}
    };
    for (const [i, sourceName] of sourceNames.entries()) {
        const place = labDOM.call('lab_town_store_get', 'ZA', sourceName) as GlobeTown;
        const expected = i < 2 ? 'Heidelberg' : 'Newport (Harbour)';
        check(place.town.name === sourceName && place.town.value === sourceName &&
              place.displayName === expected && place.fullName === expected,
              'native display names hide GeoNames IDs without rewriting catalogue identity');
        for (const zoom of [3, 16]) {
            const label = labelledGlobe.townLabel(place, zoom, 600);
            check(label.name === expected && label.full === expected && label.width > 20 && label.width <= 128,
                  'globe markers and accessible names omit IDs without treating them as expanded names');
        }
    }
    townBatch[0].name = 'mutated browser input';
    const retained = labDOM.call('lab_town_store_get', 'ZZ', 'Native western') as {town: LabTown};
    check(retained.town.name === 'Native western', 'town storage owns its strings independently of input objects');
    retained.town.name = 'mutated snapshot';
    check((labDOM.call('lab_town_store_get', 'ZZ', 'Native western') as {town: LabTown}).town.name === 'Native western',
          'snapshot changes cannot modify native town records');
    const dateline = labDOM.call('lab_town_store_region', 175, -1, 185, 1) as {jurisdiction: string; town: LabTown}[];
    check(dateline.filter(place => place.jurisdiction === 'ZZ').length === 2,
          'spatial buckets wrap both representations of the date line');
    check((labDOM.call('lab_town_store_region', -5, 90, 5, 90) as {jurisdiction: string}[])
        .some(place => place.jurisdiction === 'ZZ'), 'north-pole bucket remains in bounds');
    check((labDOM.call('lab_town_store_region', 0, 1e300, 5, 1e300) as unknown[]).length === 0,
          'out-of-range latitude bounds cannot overflow bucket indexes');
    check(labDOM.call('lab_town_store_add', 'PT', [town('Horta', '38.5333', '-28.6333'),
        town('Madalena', '38.53642', '-28.5266')]) === 2, 'Azores regression towns enter the native catalogue');
    const islandOrientation = labDOM.call('lab_globe_centre', 38.535, -28.58) as number[];
    const islandMarkers = document.createElement('div');
    const islandGlobe = Object.assign(Object.create(MarsAlmanacGlobe.prototype), {
        orientation: islandOrientation,
        view: labDOM.call('lab_globe_view', islandOrientation, 512, 1),
        flightTarget: [...islandOrientation, 512], markerSignature: '',
        markers: islandMarkers, markerNodes: new Map(), loadCountry: () => {},
        input: () => ({value: ''}),
        countryNames: new Intl.DisplayNames(['en-GB'], {type: 'region'})
    }) as {flightTarget: number[] | null; drawMarkers(size: number): void};
    islandGlobe.drawMarkers(800);
    check(islandMarkers.childElementCount === 0, 'camera travel temporarily suppresses unselected town labels');
    islandGlobe.flightTarget = null;
    islandGlobe.drawMarkers(800);
    check(islandMarkers.textContent!.includes('Horta') && islandMarkers.textContent!.includes('Madalena'),
          `arrival restores Azores labels without movement, new data or a changed camera: ${islandMarkers.textContent}`);
    const hortaMarker = islandMarkers.firstElementChild;
    islandGlobe.drawMarkers(800);
    check(islandMarkers.firstElementChild === hortaMarker, 'stationary arrival reuses the restored town markers');
    const englishTown = {...town('London', '51.5074', '-0.1278'), country_code: 'GB-ENG'};
    check(labDOM.call('lab_town_store_add', 'GB', [englishTown]) === 1,
          'UK catalogue accepts a separate native display country');
    const englishPlace = labDOM.call('lab_town_store_get', 'GB', 'London') as GlobeTown;
    check(englishPlace.jurisdiction === 'GB' && englishPlace.town.country_code === 'GB-ENG',
          'native storage preserves display country without changing catalogue membership');
    const englishCamera = labDOM.call('lab_globe_centre', 51.5074, -.1278) as number[];
    const englishMarkers = document.createElement('div');
    const englishInputs: Record<string, {value: string}> = {
        Jurisdiction: {value: 'GB'}, Town: {value: 'London'},
        Latitude: {value: '51.5074'}, Longitude: {value: '-0.1278'}
    };
    const englishGlobe = Object.assign(Object.create(MarsAlmanacGlobe.prototype), {
        orientation: englishCamera, view: labDOM.call('lab_globe_view', englishCamera, 512, 1),
        markerSignature: '', markers: englishMarkers, markerNodes: new Map(), loadCountry: () => {},
        input: (key: string) => englishInputs[key], countryNames: new Intl.DisplayNames(['en-GB'], {type: 'region'})
    }) as {drawMarkers(size: number): void; observerCountryKey(): string};
    englishGlobe.drawMarkers(800);
    const englishMarker = englishMarkers.querySelector<HTMLButtonElement>('.globe-town')!;
    check(englishMarker.dataset.countryCode === 'GB-ENG' && englishGlobe.observerCountryKey() === 'town:GB-ENG',
          'marker and selected observer use England even when the catalogue group is UK');
    const englishSelect = document.createElement('select');
    labDOM.call('lab_location_populate', englishSelect, [englishPlace.town], true);
    check(englishSelect.options[0].dataset.countryCode === 'GB-ENG',
          'town menu carries the same display country as the globe marker');
    const englishFlag = 'data:image/svg+xml;base64,' + btoa('<svg xmlns="http://www.w3.org/2000/svg"><path fill="red"/></svg>');
    WorksheetResults.tooltip.rememberCountry('GB', 'United Kingdom',
        'data:image/svg+xml;base64,' + btoa('<svg xmlns="http://www.w3.org/2000/svg"><path fill="blue"/></svg>'));
    WorksheetResults.tooltip.rememberCountry('GB-ENG', 'England', englishFlag);
    document.body.append(englishMarkers);
    try {
        WorksheetResults.tooltip.show(englishMarker);
        const tooltip = document.querySelector<HTMLElement>('#marsButtonTooltip')!;
        check(tooltip.textContent!.includes('London, England') &&
              tooltip.querySelector('img.country-flag')?.getAttribute('src') === englishFlag,
              'a cached UK flag cannot replace England in a London tooltip');
    } finally {
        WorksheetResults.tooltip.hide();
        englishMarkers.remove();
        labDOM.call('lab_flag_cache_clear');
    }
    check(labDOM.call('lab_town_store_add', 'ZZ', [town('rollback', '0', '0'), town('bad\ud800', '0', '0')]) === -1 &&
          !labDOM.call('lab_town_store_get', 'ZZ', 'rollback'), 'invalid UTF-8 input rolls back the complete batch');
    const overBudget = Array.from({length: 130}, (_, i) => ({...town(`quota-${i}`, '1', '1'), detail: 'x'.repeat(65536)}));
    check(labDOM.call('lab_town_store_add', 'ZZ', overBudget) === -1 &&
          !labDOM.call('lab_town_store_get', 'ZZ', 'quota-0') &&
          Boolean(labDOM.call('lab_town_store_get', 'ZZ', 'Native western')),
          'quota failure rolls back new rows and preserves existing records across native memory growth');
    const largeCatalogue = Array.from({length: 5000}, (_, i) => town(`Native catalogue ${i}`, 'invalid', 'invalid'));
    check(labDOM.call('lab_town_store_add', 'ZX', largeCatalogue) === 5000,
          'large native catalogue insertion keeps temporary bridge handles bounded');
    check((labDOM.call('lab_town_store_list', 'ZX') as LabTown[]).length === 5000 &&
          (labDOM.call('lab_town_store_get', 'ZX', 'Native catalogue 4999') as GlobeTown).town.value === 'Native catalogue 4999',
          'large catalogue snapshots remain bounded and exact selection keys use the dictionary');
    labDOM.call('lab_town_store_add', 'GB-WLS', [town('Llanfairpwllgwyngyll', '53.22', '-4.2')]);
    check((labDOM.call('lab_town_store_get', 'GB-WLS', 'Llanfairpwllgwyngyll') as GlobeTown)
        .fullName === 'Llanfairpwllgwyngyllgogerychwyrndrobwllllantysiliogogogoch',
        'native exception dictionary supplies the full Welsh town name');
    labDOM.call('lab_country_cache_clear');
    labDOM.call('lab_country_cache_put', 'region:truro', 'Cornwall, England', 'GB-ENG', '', 'GB-CON');
    const regionCache = labDOM.call('lab_country_cache_get', 'region:truro') as {code: string; flagCode: string};
    check(regionCache.code === 'GB-ENG' && regionCache.flagCode === 'GB-CON',
          'native cache separates cultural artwork from geographic ownership');
    for (let i = 0; i < 65; ++i) labDOM.call('lab_country_cache_put', `test:${i}`, 'Country — 巴西', 'BR', '');
    check(!labDOM.call('lab_country_cache_get', 'test:0') &&
        (labDOM.call('lab_country_cache_get', 'test:64') as {name: string}).name === 'Country — 巴西',
        'native spatial-country cache preserves Unicode and evicts its oldest entry');
    labDOM.call('lab_country_cache_put', 'water:41,51', '', '', '3');
    check((labDOM.call('lab_country_cache_get', 'water:41,51') as {nauticalZone: string}).nauticalZone === '3',
        'native geographic cache preserves nautical suggestions independently of country names');
    labDOM.call('lab_country_cache_clear');

    const initialFetch = labHttp.fetch;
    let deferredRequest = false;
    try {
        labHttp.fetch = async (url, options) => {
            if (url !== '/almanac-eval') return initialFetch.call(labHttp, url, options);
            const payload = labWire.decode(options!.body as Uint8Array<ArrayBuffer>) as {defer_events?: string};
            deferredRequest = payload.defer_events === 'true';
            return Object.assign(new Response(), {async labData() { return {ok: true}; }});
        };
        await WorksheetRequests.evaluationTransport.fetchAlmanacEvaluation();
        check(deferredRequest, 'initial Almanac request defers annual events so the observer card can appear first');
    } finally { labHttp.fetch = initialFetch; }

    const borderFetch = labHttp.fetch;
    const borderInputs: Record<string, {value: string}> = {
        Jurisdiction: {value: 'BR'}, Town: {value: 'border-town'},
        Latitude: {value: '-33.69'}, Longitude: {value: '-53.46'}
    };
    const borderGlobe = Object.assign(Object.create(MarsAlmanacGlobe.prototype), {
        input: (key: string) => borderInputs[key], draw: () => {},
        countryNames: new Intl.DisplayNames(['en-GB'], {type: 'region'}),
        countryPending: new Map()
    }) as {loadCountry(point?: number[]): void; country: string; hoverCountry: string; hoverCountryKey: string};
    labDOM.call('lab_town_store_add', 'BR', [town('border-town', '-33.69', '-53.46')]);
    const borderRequests: Array<{country_code?: string}> = [];
    try {
        labHttp.fetch = async (url, options) => {
            if (url !== '/almanac-country') return borderFetch.call(labHttp, url, options);
            const request = labWire.decode(options!.body as Uint8Array<ArrayBuffer>) as {country_code?: string};
            borderRequests.push(request);
            return Object.assign(new Response(), {async labData() { return {
                ok: true, country: request.country_code === 'BR' ? 'Brazil' : 'Uruguay',
                country_code: request.country_code || 'UY',
                country_flag: 'data:image/svg+xml;base64,' + btoa('<svg xmlns="http://www.w3.org/2000/svg"/>')
            }; }});
        };
        borderGlobe.loadCountry();
        await new Promise(resolve => setTimeout(resolve, 0));
        check(borderGlobe.country === 'Brazil' && borderRequests[0].country_code === 'BR',
              'named border-town observer uses the same catalogue country as its town label');
        const selectedCountry = document.getElementById('almanacJurisdiction')!
            .closest('.select-shell')!.querySelector<HTMLElement>('.select-button')!;
        const oldCode = selectedCountry.dataset.countryCode;
        const oldChildren = Array.from(selectedCountry.childNodes);
        try {
            selectedCountry.dataset.countryCode = 'BR';
            selectedCountry.textContent = 'Brazil';
            await new Promise(resolve => setTimeout(resolve, 0));
            check(Boolean(selectedCountry.querySelector('img.country-flag')) && borderRequests.length === 1,
                  'globe flag populates an unhovered closed country selector without another lookup');
            selectedCountry.textContent = 'Brazil';
            await new Promise(resolve => setTimeout(resolve, 0));
            check(Boolean(selectedCountry.querySelector('img.country-flag')) && borderRequests.length === 1,
                  'native text refresh immediately restores the already loaded selected-country flag');
            selectedCountry.dataset.countryCode = '';
            await new Promise(resolve => setTimeout(resolve, 0));
            check(!selectedCountry.querySelector('img.country-flag'),
                  'clearing the jurisdiction removes stale selected-country artwork');
        } finally {
            if (oldCode === undefined) delete selectedCountry.dataset.countryCode;
            else selectedCountry.dataset.countryCode = oldCode;
            selectedCountry.replaceChildren(...oldChildren);
        }
        borderGlobe.hoverCountryKey = '-33.69,-53.46';
        borderGlobe.loadCountry([-33.69, -53.46]);
        await new Promise(resolve => setTimeout(resolve, 0));
        check(borderGlobe.hoverCountry === 'Uruguay' && !borderRequests[1].country_code,
              'free-coordinate hover retains geographic lookup, separate from named-town metadata');
        borderInputs.Latitude.value = '-33.7';
        borderGlobe.loadCountry();
        await new Promise(resolve => setTimeout(resolve, 0));
        check(borderGlobe.country === 'Uruguay' && !borderRequests[2].country_code,
              'edited coordinates cannot inherit a stale selected town country');
    } finally { labHttp.fetch = borderFetch; }

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
    const regionalCamera = [0.438915922285622, -0.03708466129533938, -0.09795837018398082, 0.8924022068393097];
    const regionalView = labDOM.call('lab_globe_view', regionalCamera, 8.671137658463438 / 2, 1) as number[];
    check(regionalView[5] - regionalView[3] < 80 && regionalView[5] - regionalView[3] > 70,
          'mid-latitude regional prefetch remains regional instead of requesting most of a hemisphere');
    for (const latitude of [0, 52, -52, 80, -80]) {
        for (const longitude of [-179.95, 0, 179.95]) {
            const camera = labDOM.call('lab_globe_rotate',
                labDOM.call('lab_globe_centre', latitude, longitude), 1, 0, 0, .37) as number[];
            for (const zoom of [1, 2, 4.335568829231719, 8, 64, 4096]) {
                const view = labDOM.call('lab_globe_view', camera, zoom, 1) as number[];
                const radius = Math.min(1, 1.6 / zoom) * (1 - 1e-12);
                for (let i = 0; i < 72; ++i) {
                    const angle = i * Math.PI / 36;
                    const point = labDOM.call('lab_globe_pick', radius * Math.cos(angle),
                        radius * Math.sin(angle), camera) as number[];
                    const lon = point[1] + 360 * Math.round((view[1] - point[1]) / 360);
                    check(point[0] >= view[4] - 1e-8 && point[0] <= view[6] + 1e-8 &&
                          lon >= view[3] - 1e-8 && lon <= view[5] + 1e-8,
                          'tight spherical bounds retain the entire rolled viewport, poles and date-line crossings');
                }
            }
        }
    }
    check(labDOM.call('lab_globe_pick', 2, 0, [0, 0, 0, 1]) === null, 'globe limb rejects outside clicks');
    {
        const juneau = [...labDOM.call('lab_globe_centre', 58.3019, -134.4197) as number[], 150];
        const repaired = labDOM.call('lab_globe_history_camera', juneau, -33.4489, -70.6693, 20) as number[];
        const centre = labDOM.call('lab_globe_pick', 0, 0, repaired.slice(0, 4)) as number[];
        check(Math.abs(centre[0] + 33.4489) < 1e-8 && Math.abs(centre[1] + 70.6693) < 1e-8 && repaired[4] === 150,
              'Santiago history cannot restore Juneau geometry; valid magnification survives legacy camera repair');
        check((labDOM.call('lab_globe_history_camera', juneau, 58.3019, -134.4197, 20) as number[])
            .every((value, i) => value === juneau[i]), 'matching saved cameras retain their exact orientation and zoom');
        check(labDOM.call('lab_globe_history_camera', juneau, 91, 0, 20) === null,
              'history camera repair rejects invalid geographic observers');
    }
    for (const [lat, lon] of [[52, -2], [-33, 151], [89.99, -179.9], [0, 180]]) {
        const start = [...labDOM.call('lab_globe_centre', 0, 179.9) as number[], 150];
        const end = [...labDOM.call('lab_globe_rotate',
            labDOM.call('lab_globe_centre', lat, lon), 1, 0, 0, .4) as number[], 4096];
        const frame = (progress: number): number[] => labDOM.call('lab_globe_flight', start, end, progress) as number[];
        check(JSON.stringify(frame(0)) === JSON.stringify(start) && JSON.stringify(frame(1)) === JSON.stringify(end),
              'history flight retains exact endpoints including zoom and roll');
        const destination = labDOM.call('lab_globe_project', lat, lon, start.slice(0, 4)) as number[];
        const extent = Math.max(Math.abs(destination[0]), Math.abs(destination[1]));
        const visible = destination[2] > 0 && extent * 150 <= 1.12;
        const cruise = visible ? 150 : Math.min(150, 1.05 / Math.sqrt((1 - destination[2]) / 2));
        if (cruise < 150) {
            check(Math.abs(frame(.5)[4] - cruise) < 1e-8 && frame(.25)[4] > cruise &&
                  frame(.25)[4] < 150 && frame(.75)[4] > cruise && frame(.75)[4] < 4096,
                  'flight uses a distance-appropriate overview rather than always showing the entire globe');
        } else {
            check(frame(.125)[4] >= 150 && frame(.5)[4] >= 150,
                  'an already visible destination never causes a preliminary zoom-out');
        }
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
    const direction = (latitude: number, longitude: number): number[] => {
        const p = latitude * Math.PI / 180, l = longitude * Math.PI / 180;
        return [Math.cos(p) * Math.sin(l), Math.sin(p), Math.cos(p) * Math.cos(l)];
    };
    const separation = (a: number[], b: number[]): number => Math.atan2(Math.hypot(
        a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]),
        a.reduce((sum, value, i) => sum + value * b[i], 0));
    for (const [fromLat, fromLon, toLat, toLon, zoom] of [
        [51, -2, 51.1, -1.9, 150], [51, -2, 52, -1, 4096], [70, -60, 70, 60, 128],
        [0, 179.9, 0, -179.9, 150], [89.9, 0, 89.9, 180, 150],
        [0, 0, 0, 180, 150], [0, 0, .000001, 179.999999, 150], [51, -2, 51, -2, 150],
        ...globeFlightRoutes.map(([, a, b, c, d]) => [a, b, c, d, 150])
    ]) {
        const camera = (lat: number, lon: number, roll: number): number[] => [
            ...labDOM.call('lab_globe_rotate', labDOM.call('lab_globe_centre', lat, lon), 1, 0, 0, roll) as number[], zoom
        ];
        const start = camera(fromLat, fromLon, .7), end = camera(toLat, toLon, -1.1);
        const from = direction(fromLat, fromLon), to = direction(toLat, toLon), distance = separation(from, to);
        const projected = labDOM.call('lab_globe_project', toLat, toLon, start.slice(0, 4)) as number[];
        const extent = Math.max(Math.abs(projected[0]), Math.abs(projected[1]));
        const visible = projected[2] > 0 && extent * zoom <= 1.12;
        const cruise = visible ? zoom : Math.min(zoom, 1.05 / Math.sqrt((1 - projected[2]) / 2));
        const alignsHeading = projected[2] <= 0 || Math.hypot(projected[0], projected[1]) * zoom > 1.12 * Math.SQRT2;
        let previousDistance = 0;
        for (const progress of [.01, .125, .25, .375, .5, .625, .75, .875, .99]) {
            const frame = labDOM.call('lab_globe_flight', start, end, progress) as number[];
            const location = labDOM.call('lab_globe_pick', 0, 0, frame.slice(0, 4)) as number[];
            const centre = direction(location[0], location[1]);
            const travelled = separation(from, centre);
            check(Math.abs(travelled + separation(centre, to) - distance) < 1e-8 &&
                  travelled >= previousDistance - 1e-8,
                  'different saved rolls cannot bend or reverse the shortest great-circle route');
            previousDistance = travelled;
            check(frame.every(Number.isFinite) && frame[4] >= 1 && frame[4] <= zoom + 1e-8,
                  'flight is finite and bounded, including coincident and antipodal centres');
            if (visible) {
                check(frame[4] === zoom, 'visible destinations keep identical saved zoom throughout the rotation');
                if (distance > 1e-8 && progress === .25)
                    check(travelled > 0, 'visible destinations travel after the departure heading alignment');
            } else if (progress === .5) {
                check(Math.abs(frame[4] - cruise) < 1e-7,
                      'the overview fits the remaining half-route, including far-side destinations');
            }
            if (progress <= .125)
                check(travelled < 1e-8 && frame[4] === zoom, 'departure heading alignment keeps the centre and zoom fixed');
            if (progress >= .875)
                check(separation(centre, to) < 1e-8 && frame[4] === zoom,
                      'arrival roll correction keeps the destination centre and departure zoom fixed');
            if (progress === .5)
                check(Math.abs(travelled - distance / 2) < 1e-8, 'widest view coincides with the geographic halfway point');
            const opposite = labDOM.call('lab_globe_flight', start, end, 1 - progress) as number[];
            check(Math.abs(frame[4] - opposite[4]) < 1e-7, 'zoom-out and zoom-in have symmetric pacing');
            if (alignsHeading && progress >= .125 && progress <= .875 && distance > 1e-6 && distance < Math.PI - 1e-6) {
                // The great-circle tangent is positive screen Y throughout travel, not north-up.
                const axis = [from[1] * to[2] - from[2] * to[1], from[2] * to[0] - from[0] * to[2],
                    from[0] * to[1] - from[1] * to[0]];
                const tangent = [axis[1] * centre[2] - axis[2] * centre[1],
                    axis[2] * centre[0] - axis[0] * centre[2], axis[0] * centre[1] - axis[1] * centre[0]];
                const matrix = (labDOM.call('lab_surface_camera', 1, frame.slice(0, 4), null, null, null, 0) as number[][]).flat();
                const x = tangent.reduce((sum, value, i) => sum + value * matrix[i], 0);
                const y = tangent.reduce((sum, value, i) => sum + value * matrix[i + 3], 0);
                check(Math.abs(x) < 1e-8 && y > 0, 'transported heading faces the route throughout travel');
                const otherRoll = camera(toLat, toLon, .4);
                const other = labDOM.call('lab_globe_flight', start, otherRoll, progress) as number[];
                check(Math.abs(Math.abs(frame.slice(0, 4).reduce((sum, v, i) => sum + v * other[i], 0)) - 1) < 1e-12,
                      'destination roll has no influence before arrival');
            }
        }
        let seen = false, stayedVisible = true;
        for (let i = 0; i <= 1000; ++i) {
            const frame = labDOM.call('lab_globe_flight', start, end, i / 1000) as number[];
            const point = labDOM.call('lab_globe_project', toLat, toLon, frame.slice(0, 4)) as number[];
            const inView = point[2] >= -1e-10 && Math.max(Math.abs(point[0]), Math.abs(point[1])) * frame[4] <= 1.12 + 1e-8;
            if (seen && !inView) stayedVisible = false;
            seen ||= inView;
        }
        check(seen && stayedVisible,
              `once the destination enters the viewport it never leaves: ${fromLat},${fromLon} to ${toLat},${toLon}`);
    }
    {
        const start = [...labDOM.call('lab_globe_centre', 0, 0) as number[], 1];
        let previousStep = Infinity;
        for (const longitude of [10, 60, 120, 179]) {
            const target = [...labDOM.call('lab_globe_centre', 0, longitude) as number[], 1];
            const advance = (elapsed: number): number =>
                labDOM.call('lab_globe_flight_advance', start, target, .4, elapsed, 800) as number;
            const step = advance(16) - .4;
            check(step > 0 && step < previousStep, 'longer journeys advance more slowly at the same zoom');
            previousStep = step;
            check(advance(5000) === advance(50), 'stalled animation frames cannot cause a catch-up jump');
            check(advance(0) === .4, 'waiting for imagery does not advance the travel clock');
            check(labDOM.call('lab_globe_flight_advance', start, target, .4, -1, 800) === null,
                  'negative animation time is rejected');
            const departure = labDOM.call('lab_globe_flight_advance', start, target, 0, 16, 800) as number;
            check(Math.abs(departure - .016 / (1.2 / .125)) < 1e-12,
                  'departure heading has a fixed short duration, independent of the route length');
        }
        const maxStart = [...labDOM.call('lab_globe_centre', 55.9533, -3.1883) as number[], 4096];
        const maxEnd = [...labDOM.call('lab_globe_centre', 58.3019, -134.4197) as number[], 4096];
        for (const redirect of [0, 1]) {
            for (const progress of [.02, .12, .13, .2, .4, .5, .6, .8, .86, .88, .98]) {
                const next = labDOM.call('lab_globe_flight_advance', maxStart, maxEnd, progress, 50, 800, redirect) as number;
                const a = labDOM.call('lab_globe_flight', maxStart, maxEnd, progress, redirect) as number[];
                const b = labDOM.call('lab_globe_flight', maxStart, maxEnd, next, redirect) as number[];
                const centre = labDOM.call('lab_globe_pick', 0, 0, a.slice(0, 4)) as number[];
                const shifted = labDOM.call('lab_globe_project', centre[0], centre[1], b.slice(0, 4)) as number[];
                const pixels = Math.hypot(shifted[0], shifted[1]) * b[4] * 800 / 2.24;
                const scale = Math.abs(Math.log(b[4] / a[4])) * 400;
                check(next > progress && pixels + scale <= 18.001,
                      'ordinary and redirected flights bound motion to 18 pixels even after a slow frame');
            }
        }
        const durations: number[] = [];
        for (const rate of [60, 30, 20]) {
            let progress = 0, frames = 0;
            while (progress < 1 && frames < rate * 120) {
                progress = labDOM.call('lab_globe_flight_advance', maxStart, maxEnd, progress, 1000 / rate, 800) as number;
                ++frames;
            }
            check(progress === 1, `close-view travel completes at ${rate} Hz`);
            durations.push(frames / rate);
        }
        check(Math.max(...durations) - Math.min(...durations) < 1,
              `render rate does not multiply the journey time: ${durations.join(', ')}`);
        for (const [name, fromLat, fromLon, toLat, toLon] of globeFlightRoutes) {
            for (const [fromZoom, toZoom] of [[1, 150], [150, 20], [150, 512]]) {
                const a = [...labDOM.call('lab_globe_centre', fromLat, fromLon) as number[], fromZoom];
                const b = [...labDOM.call('lab_globe_centre', toLat, toLon) as number[], toZoom];
                let progress = 0, frames = 0;
                while (progress < 1 && frames < 1800) {
                    progress = labDOM.call('lab_globe_flight_advance', a, b, progress, 1000 / 60, 800) as number;
                    ++frames;
                }
                check(progress === 1 && frames / 60 < 30,
                      `${name}: bounded 60 Hz travel completes within 30 display seconds (${fromZoom} to ${toZoom}: ${frames / 60})`);
                const arrived = labDOM.call('lab_globe_flight', a, b, progress) as number[];
                check(arrived.every((value, i) => value === b[i]),
                      `${name}: faster travel reaches the exact saved camera without dropping its zoom`);
            }
        }
    }
    for (const [lat, lon, zoom, expected] of [[60, 15, 1, 3], [61, 16, 1, 0],
        [-60, -30, 1, 3], [0.25, 0.25, 100, 3], [0, 180, 1, 3]]) {
        const grid = labDOM.call('lab_globe_grid', lat, lon, zoom, 800) as number[];
        check(grid[2] === expected, 'grid hover matches displayed spacing and hemispheres');
    }
    check(Boolean(document.getElementById('almanacGlobeCanvas')), 'observer globe exists in Almanac card');
    check(Boolean(document.getElementById('almanacGlobe')!.compareDocumentPosition(
        document.querySelector('#almanacControls .datetime-field-groups')!) & Node.DOCUMENT_POSITION_FOLLOWING),
          'observer globe precedes the worksheet controls');
    check(!document.querySelector('#almanacControls .almanac-briefing'), 'removed introduction stays absent');

    const globeCanvas = document.getElementById('almanacGlobeCanvas') as HTMLCanvasElement;
    const space = getComputedStyle(document.querySelector<HTMLElement>('.almanac-globe-view')!);
    check(space.backgroundColor === 'rgb(0, 0, 0)' && space.backgroundImage.includes('radial-gradient'),
          'globe has a blue-black star field behind its transparent exterior');
    const earth = new Image();
    earth.src = '/earth_surface.jpg';
    await earth.decode();
    check(earth.naturalWidth === 2048 && earth.naturalHeight === 1024,
          'bundled NASA overview decodes at its prepared resolution');
    if (globeCanvas.getContext('webgl')) {
        const originalWorker = globalThis.Worker;
        const httpFixture = globeHTTPFixture(async body => {
            const response = await labHttp.fetch('/almanac-globe', {method: 'POST', body});
            const data = await response.labData() as {ok?: boolean; vectors?: string | Uint8Array<ArrayBuffer>};
            if (typeof data.vectors === 'string') data.vectors = globeFixtureBytes(data.vectors);
            return new Response(labWire.encode(data), {status: response.status,
                headers: {'Content-Type': 'application/x-protobuf'}});
        });
        globalThis.Worker = httpFixture.Worker;
        const resetGlobeWorker = (): void => {
            const globe = (MarsAlmanacGlobe as unknown as {
                instance: {stopShoreWorker(error: Error): void} | null
            }).instance;
            globe?.stopShoreWorker(new Error('Browser fixture transport changed'));
        };
        // Earlier history checks may already own a worker using the previous transport.
        resetGlobeWorker();
        const panel = document.getElementById('almanacControls')!;
        const latitude = document.getElementById('almanacLatitude') as HTMLInputElement;
        const longitude = document.getElementById('almanacLongitude') as HTMLInputElement;
        const elevation = document.getElementById('almanacElevation') as HTMLInputElement;
        const town = document.getElementById('almanacTown') as HTMLSelectElement, savedTown = town.value;
        const saved = [latitude.value, longitude.value, elevation.value], hidden = panel.classList.contains('hidden');
        const scroll = [window.scrollX, window.scrollY];
        const highResolutionAlreadyRequested = Boolean(Reflect.get(
            Reflect.get(MarsAlmanacGlobe, 'instance'), 'highResolutionRequested'));
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
            if (!highResolutionAlreadyRequested)
                check(document.getElementById('almanacGlobe')!.dataset.resolution === '2048',
                      'initial overview does not eagerly load the large texture');
            else
                check(Number(document.getElementById('almanacGlobe')!.dataset.resolution) >= 2048,
                      'earlier navigation may retain its already-requested higher-resolution texture');
            document.querySelector<HTMLButtonElement>('[data-globe-action="centre"]')!.click();
            await new Promise(requestAnimationFrame);
            const marker = document.querySelector<HTMLElement>('.globe-observer');
            check(Boolean(marker) && Math.abs(parseFloat(marker!.style.left) - globeCanvas.clientWidth / 2) < 1,
                  'globe initially centres the observer');
            const clipboardDescriptor = Object.getOwnPropertyDescriptor(navigator, 'clipboard');
            let copiedImage: Blob | null = null;
            Object.defineProperty(navigator, 'clipboard', {configurable: true, value: {
                async write(items: ClipboardItem[]): Promise<void> {
                    copiedImage = await items[0].getType('image/png');
                }
            }});
            try {
                await MarsAlmanacGlobe.copyImage();
                check(copiedImage !== null, 'globe copying writes a PNG rather than editor text');
                const bitmap = await createImageBitmap(copiedImage!);
                check(bitmap.width === globeCanvas.width && bitmap.height === globeCanvas.height,
                      'copied globe preserves display resolution');
                bitmap.close();
            } finally {
                if (clipboardDescriptor) Object.defineProperty(navigator, 'clipboard', clipboardDescriptor);
                else Reflect.deleteProperty(navigator, 'clipboard');
            }
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
                    if (url !== '/almanac-country') return countryFetch.call(labHttp, url, options);
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
                    async labData() { return {ok: true, country, country_code: country ? 'GB' : '',
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
                            .replace('gl_FragColor=vectorReady && !naturalShore?overlay:vec4(surface,1.);',
                                     'gl_FragColor=vec4(.5+(vec2(lon,lat)-probeCentre)*256.,0.,1.);');
                        check(source.includes('gl_FragColor=vec4(.5+'), 'coordinate probe replaces the final colour output');
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
            const paletteModuleURL = '/js/globe_gpu.js';
            const paletteBackend = await import(paletteModuleURL) as typeof import('../../typescript/globe_gpu.js');
            const activeTexture = gl.getParameter(gl.ACTIVE_TEXTURE) as number;
            const savedTextures: (WebGLTexture | null)[] = [], paletteTextures: WebGLTexture[] = [];
            const paletteUniforms = ['mapMode', 'detailReady', 'grid', 'riverReady', 'vectorReady', 'naturalShore', 'palettesReady', 'photoReady'];
            const savedUniforms = paletteUniforms.map(name => gl.getUniform(program, gl.getUniformLocation(program, name)!));
            const savedCamera = gl.getUniform(program, gl.getUniformLocation(program, 'camera')!) as Float32Array;
            const savedGridMatrix = gl.getUniform(program, gl.getUniformLocation(program, 'cameraToWorld')!) as Float32Array;
            const savedRiverBounds = gl.getUniform(program, gl.getUniformLocation(program, 'riverBounds')!) as Float32Array;
            const savedPhotoPixels = gl.getUniform(program, gl.getUniformLocation(program, 'photoPixels')!) as number;
            try {
                gl.uniform1i(gl.getUniformLocation(program, 'palettesReady'), 0);
                gl.uniform1i(gl.getUniformLocation(program, 'photoReady'), 0);
                for (const unit of [0, 2, 3, 4]) {
                    gl.activeTexture(gl.TEXTURE0 + unit);
                    savedTextures.push(gl.getParameter(gl.TEXTURE_BINDING_2D) as WebGLTexture | null);
                    const texture = gl.createTexture()!;
                    paletteTextures.push(texture);
                    gl.bindTexture(gl.TEXTURE_2D, texture);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
                }
                gl.uniform1i(gl.getUniformLocation(program, 'mapMode'), 1);
                gl.uniform1i(gl.getUniformLocation(program, 'naturalShore'), 0);
                gl.uniform1i(gl.getUniformLocation(program, 'vectorReady'), 0);
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
                    const cachedTextures: (WebGLTexture | null)[] = [];
                    for (const unit of [5, 6]) {
                        gl.activeTexture(gl.TEXTURE0 + unit);
                        cachedTextures.push(gl.getParameter(gl.TEXTURE_BINDING_2D) as WebGLTexture | null);
                    }
                    const palettes = paletteBackend.buildGlobePalettes(gl, program, 32);
                    try {
                        palettes.forEach((texture, i) => {
                            gl.activeTexture(gl.TEXTURE5 + i);
                            gl.bindTexture(gl.TEXTURE_2D, texture);
                        });
                        gl.uniform1i(gl.getUniformLocation(program, 'palettesReady'), 1);
                        gl.drawArrays(gl.TRIANGLES, 0, 6);
                        const cached = new Uint8Array(4);
                        gl.readPixels(Math.floor(globeCanvas.width / 2), Math.floor(globeCanvas.height / 2),
                                      1, 1, gl.RGBA, gl.UNSIGNED_BYTE, cached);
                        check(cached.every((channel, i) => Math.abs(channel - pixel[i]) <= 2),
                              `cached GPU photographic ${kind} agrees with direct correction`);
                    } finally {
                        gl.uniform1i(gl.getUniformLocation(program, 'palettesReady'), 0);
                        palettes.forEach((texture, i) => {
                            gl.activeTexture(gl.TEXTURE5 + i);
                            gl.bindTexture(gl.TEXTURE_2D, cachedTextures[i]);
                            gl.deleteTexture(texture);
                        });
                    }
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
                gl.uniform3f(gl.getUniformLocation(program, 'camera'), savedCamera[0], savedCamera[1], 16);
                gl.uniform1f(gl.getUniformLocation(program, 'photoPixels'), 2048);
                // Photographic variation must survive close zoom instead of becoming a camera-centred flat fill.
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
                let closeVariation = 0;
                for (const fraction of [.2, .3, .4, .6, .7, .8]) {
                    const pixel = new Uint8Array(4);
                    gl.readPixels(Math.floor(globeCanvas.width * fraction), Math.floor(globeCanvas.height / 2),
                                  1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
                    closeVariation = Math.max(closeVariation, ...pixel.map((channel, i) => Math.abs(channel - localCentre[i])));
                }
                check(closeVariation > 4, 'close zoom preserves geographical photographic colour variation');
                const shoreNames = ['shoreBounds', 'shoreTransform', 'shoreOffset'];
                const shoreSaved = shoreNames.map(name =>
                    gl.getUniform(program, gl.getUniformLocation(program, name)!) as Float32Array);
                const naturalSaved = gl.getUniform(program, gl.getUniformLocation(program, 'naturalShore')!) as number;
                try {
                    gl.uniform1i(gl.getUniformLocation(program, 'naturalShore'), 1);
                    gl.uniform4f(gl.getUniformLocation(program, 'shoreBounds'), 0, 91, 1, 92);
                    gl.uniform4f(gl.getUniformLocation(program, 'shoreTransform'), 1, 0, 0, 1);
                    gl.uniform3f(gl.getUniformLocation(program, 'shoreOffset'), 0, 0, globeCanvas.clientWidth);
                    gl.drawArrays(gl.TRIANGLES, 0, 6);
                    const outside = new Uint8Array(4);
                    gl.readPixels(Math.floor(globeCanvas.width / 2), Math.floor(globeCanvas.height / 2),
                                  1, 1, gl.RGBA, gl.UNSIGNED_BYTE, outside);
                    check(outside.every((channel, i) => Math.abs(channel - localCentre[i]) <= 2),
                          'outside a retained patch preserves the same world-anchored photograph');
                } finally {
                    gl.uniform1i(gl.getUniformLocation(program, 'naturalShore'), naturalSaved);
                    shoreNames.forEach((name, i) => {
                        const location = gl.getUniformLocation(program, name);
                        if (shoreSaved[i].length === 3) gl.uniform3fv(location, shoreSaved[i]);
                        else gl.uniform4fv(location, shoreSaved[i]);
                    });
                }
                gl.uniform1f(gl.getUniformLocation(program, 'photoPixels'), 8192);
                gl.drawArrays(gl.TRIANGLES, 0, 6);
                let detailedVariation = 0;
                for (const fraction of [.2, .3, .4, .6, .7, .8]) {
                    const pixel = new Uint8Array(4);
                    gl.readPixels(Math.floor(globeCanvas.width * fraction), Math.floor(globeCanvas.height / 2),
                                  1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
                    detailedVariation = Math.max(detailedVariation, ...pixel.map((value, i) => Math.abs(value - localCentre[i])));
                }
                check(detailedVariation > 4, 'higher-resolution photography retains useful colour detail at the same zoom');
                gl.uniform1f(gl.getUniformLocation(program, 'photoPixels'), 2048);
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
                // Constant geographic coordinates isolate real shader grid coverage from projection and imagery.
                gl.uniform1i(gl.getUniformLocation(program, 'riverReady'), 0);
                gl.uniform3f(gl.getUniformLocation(program, 'camera'), -89.875, .125, 100);
                for (const [unit, rgb] of [[0, [245, 250, 250]], [2, [70, 85, 34]]] as const) {
                    gl.activeTexture(gl.TEXTURE0 + unit);
                    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                                  new Uint8Array([...rgb, 255]));
                }
                const polarPixel = (lat: number, lon: number, enabled: boolean): Uint8Array => {
                    const rad = Math.PI / 180;
                    gl.uniformMatrix3fv(gl.getUniformLocation(program, 'cameraToWorld'), false,
                        new Float32Array([0, 0, 0, 0, 0, 0, Math.cos(lat * rad) * Math.sin(lon * rad),
                            Math.sin(lat * rad), Math.cos(lat * rad) * Math.cos(lon * rad)]));
                    gl.uniform1i(gl.getUniformLocation(program, 'grid'), Number(enabled));
                    gl.drawArrays(gl.TRIANGLES, 0, 6);
                    const pixel = new Uint8Array(4);
                    gl.readPixels(Math.floor(globeCanvas.width / 2), Math.floor(globeCanvas.height / 2),
                                  1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
                    return pixel;
                };
                for (const lat of [-89.875, 89.875]) {
                    const plain = polarPixel(lat, .125, false), gap = polarPixel(lat, .125, true);
                    const line = polarPixel(lat, 0, true);
                    check([0, 1, 2].every(i => Math.abs(plain[i] - gap[i]) <= 1),
                          'enabled polar grid preserves the surface between neighbouring meridians');
                    check(line[0] < plain[0] - 10 && line[0] > plain[0] * .65,
                          'polar meridians remain visible as restrained dark strokes on ice');
                }
                // Historical sea ice must not disappear when zoom selects the independent SVG map.
                for (const lat of [-70, 70]) for (const zoom of [7.99, 8, 8.01, 16]) {
                    gl.uniform3f(gl.getUniformLocation(program, 'camera'), lat, -90, zoom);
                    for (const vectors of [false, true]) {
                        gl.uniform1i(gl.getUniformLocation(program, 'vectorReady'), Number(vectors));
                        const ice = polarPixel(lat, -90, false);
                        check(ice[0] >= 240 && ice[1] >= 240 && ice[2] >= 240 && ice[3] === 255,
                              'source-imaged ice in both hemispheres remains visible across zoom and SVG transitions');
                    }
                }
                gl.uniform1i(gl.getUniformLocation(program, 'vectorReady'), 1);
                let flatIce: Uint8Array | null = null;
                for (const zoom of [8, 64, 512]) {
                    gl.uniform3f(gl.getUniformLocation(program, 'camera'), -70, -90, zoom);
                    for (const rgb of [[230, 235, 235], [245, 250, 250], [255, 255, 255]]) {
                        gl.activeTexture(gl.TEXTURE0);
                        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                                      new Uint8Array([...rgb, 255]));
                        const ice = polarPixel(-70, -90, false);
                        check(ice[3] === 255 && (!flatIce || ice.every((value, i) => value === flatIce![i])),
                              'mapped ice stays flat and opaque rather than magnifying photographic shading at deep zoom');
                        flatIce = ice;
                    }
                }
                // An enlarged square ice corner is reconstructed on the GPU, not blurred on screen.
                const iceWidth = 64, iceHeight = 32, iceTexture = new Uint8Array(iceWidth * iceHeight * 4);
                for (let y = 0; y < iceHeight; ++y) {
                    for (let x = 0; x < iceWidth; ++x) {
                        iceTexture.set(x < 16 && y >= 29 ? [255, 255, 255, 255] : [7, 20, 64, 255],
                            (y * iceWidth + x) * 4);
                    }
                }
                gl.activeTexture(gl.TEXTURE0);
                gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, iceWidth, iceHeight, 0, gl.RGBA,
                    gl.UNSIGNED_BYTE, iceTexture);
                const iceViewport = gl.getParameter(gl.VIEWPORT) as Int32Array;
                gl.viewport(0, 0, 1, 1);
                gl.uniform1f(gl.getUniformLocation(program, 'photoPixels'), iceWidth);
                gl.uniform1i(gl.getUniformLocation(program, 'grid'), 0);
                const shoreUniforms = ['shoreBounds', 'shoreTransform', 'shoreOffset'];
                const savedShore = shoreUniforms.map(name =>
                    gl.getUniform(program, gl.getUniformLocation(program, name)!) as Float32Array);
                gl.activeTexture(gl.TEXTURE4);
                gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                    new Uint8Array([0, 0, 0, 255]));
                gl.uniform4f(gl.getUniformLocation(program, 'shoreBounds'), -180, -90, 180, 90);
                gl.uniform4f(gl.getUniformLocation(program, 'shoreTransform'), 1, 0, 0, 1);
                gl.uniform3f(gl.getUniformLocation(program, 'shoreOffset'), 0, 0, 1);
                const sampleIce = (x: number, y: number, zoom: number, natural = false): number => {
                    const lat = (90 - y / iceHeight * 180) * Math.PI / 180;
                    const lon = (x / iceWidth * 360 - 180) * Math.PI / 180;
                    gl.uniformMatrix3fv(gl.getUniformLocation(program, 'cameraToWorld'), false,
                        new Float32Array([0, 0, 0, 0, 0, 0, Math.cos(lat) * Math.sin(lon),
                            Math.sin(lat), Math.cos(lat) * Math.cos(lon)]));
                    // Keep the overview below the reconstruction threshold even for this tiny fixture.
                    gl.uniform1f(gl.getUniformLocation(program, 'viewport'), 1);
                    gl.uniform3f(gl.getUniformLocation(program, 'camera'), 0, 0, zoom);
                    gl.uniform1i(gl.getUniformLocation(program, 'naturalShore'), Number(natural));
                    gl.drawArrays(gl.TRIANGLES, 0, 6);
                    const pixel = new Uint8Array(4);
                    gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
                    return natural ? (pixel[0] > 220 ? 255 : 0) : pixel[3];
                };
                try {
                    let changed = 0, antialiased = 0;
                    for (let y = 0; y < 8; ++y) {
                        for (let x = 0; x < 8; ++x) {
                            const u = 15 + x / 8, v = 29 + y / 8;
                            const original = sampleIce(u, v, 1), rounded = sampleIce(u, v, 512);
                            if (original !== rounded) ++changed;
                            if (rounded > 0 && rounded < 255) ++antialiased;
                            check((sampleIce(u, v, 1024) > 127) === (rounded > 127),
                                'close ice contours stay geographically fixed across zoom and redraw');
                            const naturalIce = sampleIce(u, v, 512, true);
                            check((rounded > 0 && rounded < 255) || naturalIce === rounded,
                                'Natural Earth and Map use the same reconstructed ice boundary');
                        }
                    }
                    check(changed > 0, 'enlarged square ice cells receive a genuinely different contour');
                    check(antialiased <= 4, `ice antialiasing remains a narrow edge, not a fuzzy fringe: ${antialiased}`);
                    check(sampleIce(10, 31, 512) === 255 && sampleIce(24, 28, 512) === 0,
                        'ice reconstruction preserves solid interiors and open water');
                    const edges: number[] = [];
                    for (let x = 5; x < 13; ++x) {
                        for (let step = 0; step <= 32; ++step) {
                            const y = 27 + step / 8;
                            if (sampleIce(x, y, 512) === 255) { edges.push(y); break; }
                        }
                    }
                    check(edges.length === 8 && Math.max(...edges) - Math.min(...edges) >= .5,
                        'long straight source-cell runs become irregular, not just rounded at their ends');
                } finally {
                    gl.uniform1i(gl.getUniformLocation(program, 'naturalShore'), 0);
                    gl.uniform4fv(gl.getUniformLocation(program, 'shoreBounds'), savedShore[0]);
                    gl.uniform4fv(gl.getUniformLocation(program, 'shoreTransform'), savedShore[1]);
                    gl.uniform3fv(gl.getUniformLocation(program, 'shoreOffset'), savedShore[2]);
                    gl.viewport(iceViewport[0], iceViewport[1], iceViewport[2], iceViewport[3]);
                    gl.uniform1f(gl.getUniformLocation(program, 'viewport'), globeCanvas.width);
                    gl.uniform1f(gl.getUniformLocation(program, 'photoPixels'), 2048);
                }
                // The grid overlay must not fade just because hidden photography changes underneath it.
                gl.uniform1i(gl.getUniformLocation(program, 'vectorReady'), 1);
                gl.uniform3f(gl.getUniformLocation(program, 'camera'), 0, 0, 16);
                let gridReference: Uint8Array | null = null;
                for (const rgb of [[7, 20, 64], [245, 250, 250], [70, 85, 34]]) {
                    gl.activeTexture(gl.TEXTURE0);
                    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                                  new Uint8Array([...rgb, 255]));
                    const line = polarPixel(0, 0, true);
                    check(line[3] > 150 && (!gridReference || line.every((value, i) => value === gridReference![i])),
                          'SVG grid contrast and opacity are independent of invisible photographic colours');
                    gridReference = line;
                    const off = polarPixel(0, 0, false);
                    check(off[3] === 0, 'grid-off leaves no coordinate-line overlay');
                }
            } finally {
                [0, 2, 3, 4].forEach((unit, index) => {
                    gl.activeTexture(gl.TEXTURE0 + unit);
                    gl.bindTexture(gl.TEXTURE_2D, savedTextures[index]);
                    gl.deleteTexture(paletteTextures[index]);
                });
                paletteUniforms.forEach((name, index) =>
                    gl.uniform1i(gl.getUniformLocation(program, name), Number(savedUniforms[index])));
                gl.uniform1f(gl.getUniformLocation(program, 'photoPixels'), savedPhotoPixels);
                gl.uniform3fv(gl.getUniformLocation(program, 'camera'), savedCamera);
                gl.uniformMatrix3fv(gl.getUniformLocation(program, 'cameraToWorld'), false, savedGridMatrix);
                gl.uniform4fv(gl.getUniformLocation(program, 'riverBounds'), savedRiverBounds);
                gl.activeTexture(activeTexture);
                gl.drawArrays(gl.TRIANGLES, 0, 6);
            }
            check(gl.getError() === gl.NO_ERROR, 'shader probes restore valid uniform state before live interaction');
            check(!document.querySelector('[data-globe-action="map"]') &&
                  Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'naturalStyle')!)),
                  'Natural Earth is the only display style and there is no map toggle');
            const savedLatitude = latitude.value;
            for (const lat of [61, 59, -61, -59]) {
                latitude.value = String(lat);
                document.querySelector<HTMLButtonElement>('[data-globe-action="centre"]')!.click();
                await new Promise(requestAnimationFrame);
                check(!gl.getUniform(program, gl.getUniformLocation(program, 'mapMode')!),
                      'crossing either polar latitude never switches Natural Earth into map mode');
            }
            latitude.value = savedLatitude;
            document.querySelector<HTMLButtonElement>('[data-globe-action="centre"]')!.click();
            await new Promise(requestAnimationFrame);
            const zoomBefore = (gl.getUniform(program, gl.getUniformLocation(program, 'camera')!) as Float32Array)[2];
            globeCanvas.dispatchEvent(new WheelEvent('wheel', {deltaY: -1, bubbles: true, cancelable: true}));
            await new Promise(requestAnimationFrame);
            const zoomAfter = (gl.getUniform(program, gl.getUniformLocation(program, 'camera')!) as Float32Array)[2];
            check(zoomAfter > zoomBefore && zoomAfter < zoomBefore * 1.01,
                  'small wheel deltas produce small zoom changes rather than fixed fifteen-percent jumps');
            globeCanvas.dispatchEvent(new WheelEvent('wheel', {deltaY: 1, bubbles: true, cancelable: true}));
            await new Promise(requestAnimationFrame);
            const mapFetch = labHttp.fetch;
            const mapNative = labDOM.call;
            const detailGlobe = Reflect.get(MarsAlmanacGlobe, 'instance') as {
                orientation: number[]; view: number[]; applyCamera(camera: number[]): void;
            };
            const overviewZoom = detailGlobe.view[2];
            let rejectMask: ((reason: Error) => void) | undefined;
            let maskRequests = 0;
            let holdVectors = false;
            let previewRequests = 0;
            let rotationCalls = 0;
            let missingCoverage = false;
            let refreshCoverage = false;
            let releaseVectors: (() => void) | undefined;
            try {
                labDOM.call = ((entry: string, ...args: unknown[]) => {
                    check(entry !== 'lab_globe_shore_svg' && entry !== 'lab_globe_shore_frame',
                          'live SVG projection never runs on the UI thread');
                    if (entry === 'lab_globe_rotate') ++rotationCalls;
                    if (entry === 'lab_globe_shore_covers' && refreshCoverage) return false;
                    if ((entry === 'lab_globe_shore_covers' || entry === 'lab_globe_shore_frame_covers') && missingCoverage)
                        return false;
                    return mapNative(entry, ...args);
                }) as typeof labDOM.call;
                labHttp.fetch = (url, options) => {
                    if (url !== '/almanac-globe') return mapFetch.call(labHttp, url, options);
                    const payload = labWire.decode(options!.body as Uint8Array<ArrayBuffer>) as {
                        format?: string; west: number; south: number; east: number; north: number
                    };
                    if (payload.format === 'vectors') {
                        const camera = MarsAlmanacGlobe.captureHistory()!;
                        const current = mapNative('lab_globe_view', camera.slice(0, 4), camera[4], 1);
                        check(Boolean(mapNative('lab_globe_shore_frame_covers', current,
                            [payload.west, payload.south, payload.east, payload.north])),
                            'directional prefetch retains coverage of the currently displayed camera');
                        ++maskRequests;
                        if (maskRequests === 1) return new Promise((_resolve, reject) => { rejectMask = reject; });
                        const x = (Number(payload.west) + Number(payload.east)) / 2;
                        const y = (Number(payload.south) + Number(payload.north)) / 2;
                        const dx = (Number(payload.east) - Number(payload.west)) / 4;
                        const dy = (Number(payload.north) - Number(payload.south)) / 4;
                        const vectors = `M1 0 ${x.toFixed(9)} ${y.toFixed(9)}\nZ\n` +
                            `M1 0 ${x.toFixed(9)} ${y.toFixed(9)}\nL${(x + dx).toFixed(9)} ${y.toFixed(9)}\nZ\n` +
                            `M1 0 ${(x - dx).toFixed(9)} ${(y - dy).toFixed(9)}\n` +
                            `L${(x + dx).toFixed(9)} ${(y - dy).toFixed(9)}\n` +
                            `L${(x + dx).toFixed(9)} ${(y + dy).toFixed(9)}\n` +
                            `L${(x - dx).toFixed(9)} ${(y + dy).toFixed(9)}\nZ\n`;
                        const response = Object.assign(new Response(), {async labData() { return {ok: true, vectors}; }});
                        if (holdVectors) return new Promise(resolve => { releaseVectors = () => resolve(response); });
                        return Promise.resolve(response);
                    }
                    ++previewRequests;
                    return Promise.resolve(Object.assign(new Response(), {async labData() {
                        return {ok: true, image: '<svg xmlns="http://www.w3.org/2000/svg" width="2" height="1"><path fill="#465522" d="M0 0H2V1H0Z"/></svg>'};
                    }}));
                };
                detailGlobe.applyCamera([...detailGlobe.orientation, 12]);
                for (let i = 0; i < 100 && !rejectMask; ++i) await new Promise(resolve => setTimeout(resolve, 25));
                check(Boolean(rejectMask), 'close Natural Earth views load detailed SVG');
                check(previewRequests === 0, 'Natural Earth never requests the coarse whole-world map preview');
                rejectMask!(new Error('delayed shoreline fixture'));
                await new Promise(resolve => setTimeout(resolve, 50));
                await new Promise(requestAnimationFrame);
                check(!gl.getUniform(program, gl.getUniformLocation(program, 'interactionPreview')!) &&
                      Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'naturalStyle')!)),
                      'failed detail requests retain photography, never the coarse flat map');
                grid.click();
                grid.click();
                for (let i = 0; i < 100 && !gl.getUniform(program, gl.getUniformLocation(program, 'mapMode')!); ++i)
                    await new Promise(resolve => setTimeout(resolve, 25));
                check(Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'naturalShore')!)),
                      `retry restores detailed photographic colouring: ${maskRequests}; ` +
                      `${document.getElementById('almanacGlobeStatus')?.textContent}`);
                for (let i = 0; i < 100 && !gl.getUniform(program, gl.getUniformLocation(program, 'vectorReady')!); ++i)
                    await new Promise(resolve => setTimeout(resolve, 25));
                const shoreOverlay = document.querySelector<SVGSVGElement>('.globe-shore-overlay')!;
                check(Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'vectorReady')!)) &&
                      Boolean(shoreOverlay.querySelector('path')) && !shoreOverlay.querySelector('image'),
                      'collapsed islands do not prevent live SVG; the canvas retains only grid and river overlays: ' +
                      document.getElementById('almanacGlobeStatus')?.textContent);
                const priorPaths = shoreOverlay.querySelector('g')!.innerHTML;
                const priorCamera = Array.from(gl.getUniform(program, gl.getUniformLocation(program, 'camera')!) as Float32Array);
                check(!gl.getUniform(program, gl.getUniformLocation(program, 'grid')!), 'gridlines remain off unless requested');
                holdVectors = true;
                labDOM.call('lab_globe_shore_clear');
                document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!.click();
                for (let i = 0; i < 60 && !releaseVectors; ++i) await new Promise(resolve => setTimeout(resolve, 25));
                check(Boolean(releaseVectors) && shoreOverlay.querySelector('g')!.innerHTML === priorPaths &&
                      shoreOverlay.style.display === 'none' &&
                      Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'naturalShore')!)) &&
                      (gl.getUniform(program, gl.getUniformLocation(program, 'camera')!) as Float32Array)[2] > priorCamera[2] &&
                      !gl.getUniform(program, gl.getUniformLocation(program, 'grid')!),
                      'zoom retains aligned GPU detail without duplicate SVG painting or automatic gridlines');
                grid.click();
                await new Promise(requestAnimationFrame);
                check(Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'grid')!)),
                      'grid can be enabled on the retained frame while shoreline is pending');
                grid.click();
                await new Promise(requestAnimationFrame);
                check(!gl.getUniform(program, gl.getUniformLocation(program, 'grid')!),
                      'switching grid off clears retained lines without waiting for shoreline');
                holdVectors = false;
                releaseVectors!();
                for (let i = 0; i < 100 && document.getElementById('almanacGlobe')!.dataset.surfacePending === 'true'; ++i)
                    await new Promise(resolve => setTimeout(resolve, 25));
                check(document.getElementById('almanacGlobe')!.dataset.surfacePending === 'false',
                      'the new camera frame appears after vector coverage is ready');
                document.querySelector<HTMLButtonElement>('[data-globe-action="out"]')!.click();
                await new Promise(resolve => setTimeout(resolve, 400));
                const zoomIn = document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!;
                const zoomOut = document.querySelector<HTMLButtonElement>('[data-globe-action="out"]')!;
                for (let i = 0; i < 6; ++i) { zoomIn.click(); await new Promise(requestAnimationFrame); }
                for (let i = 0; i < 150 && !gl.getUniform(program, gl.getUniformLocation(program, 'vectorReady')!); ++i)
                    await new Promise(resolve => setTimeout(resolve, 25));
                check(Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'vectorReady')!)),
                      'close view has detailed vectors before starting a drag');
                for (let i = 0; i < 100 && !gl.getUniform(program, gl.getUniformLocation(program, 'naturalShore')!); ++i)
                    await new Promise(resolve => setTimeout(resolve, 25));
                check(Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'naturalShore')!)),
                      'Natural Earth retains photographic colouring at close vector zoom: ' +
                      `${gl.getUniform(program, gl.getUniformLocation(program, 'camera')!)}; ` +
                      `${document.getElementById('almanacGlobeStatus')?.textContent}`);
                gl.activeTexture(gl.TEXTURE0);
                const naturalTexture = gl.getParameter(gl.TEXTURE_BINDING_2D) as WebGLTexture;
                const sandTexture = gl.createTexture()!;
                const photoReady = gl.getUniform(program, gl.getUniformLocation(program, 'photoReady')!);
                const photoBounds = gl.getUniform(program, gl.getUniformLocation(program, 'photoBounds')!) as Float32Array;
                gl.activeTexture(gl.TEXTURE7);
                const naturalPatch = gl.getParameter(gl.TEXTURE_BINDING_2D) as WebGLTexture;
                const patchTexture = gl.createTexture()!;
                gl.activeTexture(gl.TEXTURE0);
                try {
                    gl.uniform1i(gl.getUniformLocation(program, 'photoReady'), 0);
                    gl.bindTexture(gl.TEXTURE_2D, sandTexture);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
                    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                                  new Uint8Array([190, 165, 100, 255]));
                    gl.drawArrays(gl.TRIANGLES, 0, 6);
                    const sand = new Uint8Array(4);
                    gl.readPixels(Math.floor(globeCanvas.width / 2), Math.floor(globeCanvas.height / 2),
                                  1, 1, gl.RGBA, gl.UNSIGNED_BYTE, sand);
                    const light = .8 + .2 / Math.hypot(.35, .45, 1);
                    check([190, 165, 100].every((channel, i) => Math.abs(sand[i] - channel * light) <= 2),
                          'actual SVG land coverage uses sandy photographic RGB instead of the olive map fill');
                    gl.activeTexture(gl.TEXTURE7);
                    gl.bindTexture(gl.TEXTURE_2D, patchTexture);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
                    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                                  new Uint8Array([95, 125, 45, 255]));
                    const camera = gl.getUniform(program, gl.getUniformLocation(program, 'camera')!) as Float32Array;
                    const patch = labDOM.call('lab_globe_photo_patch', camera[0], camera[1]) as number[];
                    gl.uniform1i(gl.getUniformLocation(program, 'photoReady'), 1);
                    for (const offset of [0, 360]) {
                        gl.uniform4f(gl.getUniformLocation(program, 'photoBounds'),
                                     patch[3] + offset, patch[4], patch[5] + offset, patch[6]);
                        gl.drawArrays(gl.TRIANGLES, 0, 6);
                        gl.readPixels(Math.floor(globeCanvas.width / 2), Math.floor(globeCanvas.height / 2),
                                      1, 1, gl.RGBA, gl.UNSIGNED_BYTE, sand);
                        check([95, 125, 45].every((channel, i) => Math.abs(sand[i] - channel * light) <= 2),
                              'higher-resolution patch supplies shoreline colour with wrapped longitude');
                    }
                    gl.uniform4f(gl.getUniformLocation(program, 'photoBounds'), 60, -30, 120, 30);
                    gl.drawArrays(gl.TRIANGLES, 0, 6);
                    gl.readPixels(Math.floor(globeCanvas.width / 2), Math.floor(globeCanvas.height / 2),
                                  1, 1, gl.RGBA, gl.UNSIGNED_BYTE, sand);
                    check([190, 165, 100].every((channel, i) => Math.abs(sand[i] - channel * light) <= 2),
                          'outside the photographic patch the overview remains intact');
                } finally {
                    gl.uniform1i(gl.getUniformLocation(program, 'photoReady'), Number(photoReady));
                    gl.uniform4fv(gl.getUniformLocation(program, 'photoBounds'), photoBounds);
                    gl.activeTexture(gl.TEXTURE7);
                    gl.bindTexture(gl.TEXTURE_2D, naturalPatch);
                    gl.deleteTexture(patchTexture);
                    gl.activeTexture(gl.TEXTURE0);
                    gl.bindTexture(gl.TEXTURE_2D, naturalTexture);
                    gl.deleteTexture(sandTexture);
                }
                await new Promise(resolve => setTimeout(resolve, 400));
                holdVectors = true;
                missingCoverage = true;
                grid.click();
                await new Promise(requestAnimationFrame);
                check(Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'naturalShore')!)) &&
                      Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'vectorReady')!)),
                      'Natural Earth retains the covered part of its detailed frame while the next region loads');
                missingCoverage = false;
                holdVectors = false;
                releaseVectors?.();
                grid.click();
                await new Promise(resolve => setTimeout(resolve, 400));
                await new Promise(requestAnimationFrame);
                check(Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'naturalShore')!)),
                      'replacing coverage never switches photographic detail to flat fills');
                await new Promise(resolve => setTimeout(resolve, 400));
                holdVectors = true;
                refreshCoverage = true;
                const requestsBeforeRefresh = maskRequests;
                grid.click();
                for (let i = 0; i < 100 && maskRequests === requestsBeforeRefresh; ++i)
                    await new Promise(resolve => setTimeout(resolve, 10));
                refreshCoverage = false;
                check(maskRequests > requestsBeforeRefresh,
                      'replacement geometry request starts while the published frame remains valid');
                holdVectors = false;
                releaseVectors?.();
                for (let i = 0; i < 20; ++i) {
                    await new Promise(requestAnimationFrame);
                    check(Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'vectorReady')!)) &&
                          !gl.getUniform(program, gl.getUniformLocation(program, 'interactionPreview')!),
                          'geometry replacement and mask decoding never invalidate the covered published frame');
                }
                grid.click();
                for (const button of [zoomOut, zoomIn]) {
                    button.click();
                    await new Promise(requestAnimationFrame);
                    check(Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'vectorReady')!)) &&
                          !gl.getUniform(program, gl.getUniformLocation(program, 'interactionPreview')!),
                          'one zoom step retains detailed coastlines without a coarse preview flash');
                }
                const capture = globeCanvas.setPointerCapture;
                const rotationsBeforeDrag = rotationCalls;
                globeCanvas.setPointerCapture = () => {};
                try {
                    const rect = globeCanvas.getBoundingClientRect();
                    const x = rect.left + rect.width / 2, y = rect.top + rect.height / 2;
                    globeCanvas.dispatchEvent(new PointerEvent('pointerdown', {pointerId: 71, button: 0, clientX: x, clientY: y}));
                    for (let i = 1; i <= 5; ++i) {
                        globeCanvas.dispatchEvent(new PointerEvent('pointermove', {pointerId: 71, clientX: x + i * 10, clientY: y + i * 2}));
                        await new Promise(requestAnimationFrame);
                        check(shoreOverlay.style.display === 'none' && Boolean(shoreOverlay.querySelector('path')) &&
                              Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'vectorReady')!)) &&
                              !gl.getUniform(program, gl.getUniformLocation(program, 'interactionPreview')!),
                              'every close-view drag frame retains GPU detail without repainting the duplicate SVG');
                    }
                    check(Boolean(shoreOverlay.querySelector('g')!.getAttribute('transform')),
                          'retained detailed paths move through the native camera transform during drag');
                    check(rotationCalls >= rotationsBeforeDrag + 10,
                          'each drag sample predicts an additional native camera ahead of the current position');
                } finally {
                    globeCanvas.dispatchEvent(new PointerEvent('pointerup', {pointerId: 71}));
                    // End the gesture's click suppression before subsequent synthetic click-only fixtures.
                    globeCanvas.dispatchEvent(new PointerEvent('pointerdown', {pointerId: 72, button: 0}));
                    globeCanvas.dispatchEvent(new PointerEvent('pointerup', {pointerId: 72}));
                    globeCanvas.setPointerCapture = capture;
                }
                holdVectors = true;
                missingCoverage = true;
                grid.click();
                await new Promise(requestAnimationFrame);
                check(Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'naturalStyle')!)) &&
                      !gl.getUniform(program, gl.getUniformLocation(program, 'interactionPreview')!),
                      'missing surrounding coverage cannot switch the view to a blocky map preview');
                missingCoverage = false;
                holdVectors = false;
                releaseVectors?.();
                grid.click();
                for (let i = 0; i < 6; ++i) { zoomOut.click(); await new Promise(requestAnimationFrame); }
                document.querySelector<HTMLButtonElement>('[data-globe-action="centre"]')!.click();
                await new Promise(resolve => setTimeout(resolve, 400));
                check(previewRequests === 0, 'no map-preview requests are made during zoom, drag or detail replacement');
            } finally {
                labHttp.fetch = mapFetch;
                labDOM.call = mapNative;
                detailGlobe.applyCamera([...detailGlobe.orientation, overviewZoom]);
            }
            await new Promise(requestAnimationFrame);
            check(Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'naturalStyle')!)) &&
                  !gl.getUniform(program, gl.getUniformLocation(program, 'mapMode')!) &&
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
            const place = (labDOM.call('lab_town_store_list', jurisdiction) as LabTown[] | null)?.find(candidate =>
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
                    : townFlagFetch.call(labHttp, url, options);
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
                    fillCountryMenu(): Promise<void>;
                    visibleCountries: Set<HTMLElement>;
                    menuRequest: AbortController | null;
                };
                const previousMenuRequest = flagLoader.menuRequest;
                const wasVisible = flagLoader.visibleCountries.has(countryButton);
                const blockedMenu = new AbortController();
                try {
                    flagLoader.menuRequest = blockedMenu;
                    flagLoader.visibleCountries.add(countryButton);
                    countryButton.querySelector('img.country-flag')?.remove();
                    const beforeCached = townFlagReplies.length;
                    const repaint = flagLoader.fillCountryMenu();
                    check(Boolean(countryButton.querySelector('img.country-flag')) &&
                          townFlagReplies.length === beforeCached && !blockedMenu.signal.aborted,
                          'selected country restores cached artwork immediately while another flag request is pending');
                    await repaint;
                } finally {
                    flagLoader.menuRequest = previousMenuRequest;
                    if (!wasVisible) flagLoader.visibleCountries.delete(countryButton);
                }
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
                  elevation.value === place!.elevation && evaluations === beforeTownClick + 1,
                  'one town click applies its catalogue elevation before evaluating, replacing the previous altitude');
            for (let i = 0; i < 12; ++i)
                document.querySelector<HTMLButtonElement>('[data-globe-action="out"]')!.click();
            await new Promise(requestAnimationFrame);
            check(document.querySelector('.globe-town.selected') === selectedLabel,
                  'selected town label survives overview zoom and decluttering');
            globeCanvas.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight', bubbles: true}));
            document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!.click();
            await new Promise(requestAnimationFrame);
            elevation.value = '50';
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
            check(Boolean(gl.getUniform(program, gl.getUniformLocation(program, 'naturalStyle')!)) &&
                  !gl.getUniform(program, gl.getUniformLocation(program, 'interactionPreview')!),
                  'deep zoom remains Natural Earth while detailed shoreline loads');
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
            const observerZone = document.getElementById('almanacZone') as HTMLInputElement;
            observerZone.value = '0';
            labHttp.fetch = (url, options) => url === '/almanac-country'
                ? Promise.resolve(Object.assign(new Response(), {async labData() {
                    return {ok: true, country: '', country_code: '', country_flag: '', nautical_zone: '3'};
                }})) : pointFetch.call(labHttp, url, options);
            const waterFetch = labHttp.fetch;
            globeCanvas.dispatchEvent(new MouseEvent('click', click));
            check((document.getElementById('almanacJurisdiction') as HTMLSelectElement).value === '',
                  'custom point immediately clears the previous country');
            await new Promise(resolve => setTimeout(resolve, 0));
            check(longitude.value !== previousLongitude && town.value === '' && evaluations === beforePointClick + 1,
                  'one left click selects exact coordinates and evaluates once after country lookup');
            check(observerZone.value === '3', 'unassigned inland water adopts the native nautical UTC-offset suggestion');
            labDOM.call('lab_country_cache_clear');
            let resolveZone!: (response: MarsTransportResponse) => void;
            labHttp.fetch = (url, options) => url === '/almanac-country'
                ? new Promise<MarsTransportResponse>(resolve => { resolveZone = resolve; }) : pointFetch.call(labHttp, url, options);
            globeCanvas.dispatchEvent(new MouseEvent('click', click));
            observerZone.value = '5.5';
            observerZone.dispatchEvent(new Event('input', {bubbles: true}));
            resolveZone(Object.assign(new Response(), {async labData() {
                return {ok: true, country: '', country_code: '', country_flag: '', nautical_zone: '3'};
            }}));
            await new Promise(resolve => setTimeout(resolve, 0));
            check(observerZone.value === '5.5', 'a delayed nautical suggestion cannot overwrite a manual zone edit');
            globeCanvas.dispatchEvent(new MouseEvent('click', click));
            await new Promise(resolve => setTimeout(resolve, 0));
            check(observerZone.value === '3', 'a new water selection retains its nautical suggestion through the native cache');
            labHttp.fetch = waterFetch;
            const afterZoneSelections = evaluations;
            const selectedLongitude = longitude.value;
            const jurisdictionSelect = document.getElementById('almanacJurisdiction') as HTMLSelectElement;
            jurisdictionSelect.value = jurisdiction;
            labDOM.call('lab_location_evaluation', 6,
                {observer_surface: 'Sea', fields: {jurisdiction: ''}},
                WorksheetLocations.calendar.calendarLocationContext());
            check(jurisdictionSelect.value === '' && town.value === '' && longitude.value === selectedLongitude &&
                  jurisdictionSelect.closest('.select-shell')!.querySelector('.select-button')!.textContent!
                      .includes('Country not identified'),
                  'unidentified evaluation clears stale country and town controls without changing coordinates');
            for (const observer_surface of ['Land', 'Coastal', 'Sea', 'Inland water']) {
                jurisdictionSelect.value = '';
                labDOM.call('lab_location_evaluation', 6, {observer_surface, fields: {jurisdiction}},
                    WorksheetLocations.calendar.calendarLocationContext());
                check(jurisdictionSelect.value === jurisdiction && town.value === '' &&
                      longitude.value === selectedLongitude,
                      `${observer_surface} evaluation restores the verified country without moving the observer`);
            }
            jurisdictionSelect.value = jurisdiction;
            labDOM.call('lab_location_evaluation', 6, {fields: {jurisdiction: ''}},
                WorksheetLocations.calendar.calendarLocationContext());
            check(jurisdictionSelect.value === '', 'evaluation preserves an explicitly unidentified country');
            globeCanvas.dispatchEvent(new MouseEvent('click', {...click, button: 2}));
            check(longitude.value === selectedLongitude && evaluations === afterZoneSelections,
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
                    check(evaluations === afterZoneSelections, 'edge roll does not select or evaluate an observer');
                }
                const pointer = {...click, pointerId: 42, pointerType: 'mouse'};
                globeCanvas.dispatchEvent(new PointerEvent('pointerdown', pointer));
                globeCanvas.dispatchEvent(new PointerEvent('pointermove', {...pointer, clientX: pointer.clientX + 30}));
                globeCanvas.dispatchEvent(new PointerEvent('pointerup', {...pointer, clientX: pointer.clientX + 30}));
                globeCanvas.dispatchEvent(new MouseEvent('click', click));
                check(evaluations === afterZoneSelections, 'a completed drag does not select an observer');
                globeCanvas.dispatchEvent(new PointerEvent('pointerdown', pointer));
                globeCanvas.dispatchEvent(new PointerEvent('pointermove', {...pointer, clientX: pointer.clientX + 3}));
                globeCanvas.dispatchEvent(new PointerEvent('pointerup', pointer));
                globeCanvas.dispatchEvent(new MouseEvent('click', click));
                await new Promise(resolve => setTimeout(resolve, 0));
                check(evaluations === afterZoneSelections + 1, 'a single click after dragging tolerates small hand movement');
            } finally { globeCanvas.setPointerCapture = capture; labHttp.fetch = pointFetch; }
            try {
                let countryCalls = 0;
                let civilCalls = 0;
                let civilOffset = '1';
                let releaseCivil: ((response: MarsTransportResponse) => void) | undefined;
                let delayCivil = false;
                const civilReply = () => Object.assign(new Response(), {async labData() {
                    return {ok: true, gmt_offset: civilOffset, latitude: '53.3190', longitude: '-3.4916'};
                }});
                let resolveCountry!: (response: MarsTransportResponse) => void;
                labHttp.fetch = (url, options) => url === '/almanac-country'
                    ? (++countryCalls, new Promise(resolve => { resolveCountry = resolve; }))
                    : url === '/datetime-jurisdiction-location'
                        ? (++civilCalls, delayCivil ? new Promise(resolve => { releaseCivil = resolve; })
                            : Promise.resolve(civilReply())) : pointFetch.call(labHttp, url, options);
                const clickedCamera = MarsAlmanacGlobe.captureHistory()!;
                const landClick = {...click, clientX: click.clientX + 14};
                globeCanvas.dispatchEvent(new MouseEvent('click', landClick));
                const pickedCoordinates = [latitude.value, longitude.value];
                await new Promise(requestAnimationFrame);
                check(countryCalls === 1, 'custom selection and its marker share one pending geographic lookup');
                resolveCountry(Object.assign(new Response(), {async labData() {
                    return {ok: true, country: 'Wales', country_code: 'GB-WLS', country_flag: '', nautical_zone: '4'};
                }}));
                await new Promise(resolve => setTimeout(resolve, 0));
                check((document.getElementById('almanacJurisdiction') as HTMLSelectElement).value === 'GB-WLS' &&
                      town.value === '' && latitude.value === pickedCoordinates[0] && longitude.value === pickedCoordinates[1],
                      'identified land changes the country menu without snapping the clicked point to a town');
                check(observerZone.value === '1' && civilCalls === 1,
                      'an identified jurisdiction uses its native civil offset instead of the inherited or nautical offset');
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
                const dateInput = document.getElementById('almanacDate') as HTMLInputElement;
                const originalDate = dateInput.value;
                dateInput.value = '2026-12-01';
                civilOffset = '0';
                globeCanvas.dispatchEvent(new MouseEvent('click', landClick));
                await new Promise(resolve => setTimeout(resolve, 0));
                check(observerZone.value === '0' && civilCalls === 3 && countryCalls === 1,
                      'cached geography obtains a fresh civil offset for the selected date, including zero');
                delayCivil = true;
                globeCanvas.dispatchEvent(new MouseEvent('click', landClick));
                await new Promise(resolve => setTimeout(resolve, 0));
                observerZone.value = '5.5';
                observerZone.dispatchEvent(new Event('input', {bubbles: true}));
                releaseCivil!(civilReply());
                await new Promise(resolve => setTimeout(resolve, 0));
                check(observerZone.value === '5.5', 'a pending civil offset cannot overwrite a manual zone edit');
                globeCanvas.dispatchEvent(new MouseEvent('click', landClick));
                await new Promise(resolve => setTimeout(resolve, 0));
                dateInput.value = originalDate;
                releaseCivil!(civilReply());
                await new Promise(resolve => setTimeout(resolve, 0));
                check(observerZone.value === '5.5', 'an obsolete date cannot install its civil offset');
                globeCanvas.dispatchEvent(new MouseEvent('click', landClick));
                await new Promise(resolve => setTimeout(resolve, 0));
                const oldCivil = releaseCivil!;
                globeCanvas.dispatchEvent(new MouseEvent('click', {...landClick, clientX: landClick.clientX + 5}));
                oldCivil(civilReply());
                await new Promise(resolve => setTimeout(resolve, 0));
                check(observerZone.value === '5.5', 'an obsolete point cannot install its civil offset');
                resolveCountry(Object.assign(new Response(), {async labData() {
                    return {ok: true, country: '', country_code: '', country_flag: ''};
                }}));
                await new Promise(resolve => setTimeout(resolve, 0));
                check(observerZone.value === '5.5', 'unassigned points without a native suggestion retain the authored zone');
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
                    if (url !== '/almanac-rivers') return originalFetch.call(labHttp, url, options);
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
            const borderFetch = labHttp.fetch;
            const borderReplies: Array<{resolve: (response: MarsTransportResponse) => void; signal?: AbortSignal | null;
                bounds: number[]}> = [];
            const borderFormats: string[] = [];
            const vectorReply = (index = borderReplies.length - 1): MarsTransportResponse => {
                const [west, south, east, north] = borderReplies[index].bounds;
                const x = (west + east) / 2, y = (south + north) / 2, dx = Math.min(1, (east - west) / 4);
                const segments = `${(x - dx).toFixed(9)} ${y.toFixed(9)} ${(x + dx).toFixed(9)} ${y.toFixed(9)}\n`;
                return {ok: true, status: 200, labData: async () => ({ok: true, segments})} as MarsTransportResponse;
            };
            const borderPath = globeRoot.querySelector<SVGPathElement>('.globe-border-overlay path')!;
            try {
                check(bordersButton.getAttribute('aria-pressed') === 'false', 'country borders default to off');
                labHttp.fetch = (url, options) => {
                    if (url !== '/almanac-borders') return borderFetch.call(labHttp, url, options);
                    const payload = labWire.decode(options!.body as Uint8Array<ArrayBuffer>) as {
                        format: string; west: number; south: number; east: number; north: number
                    };
                    borderFormats.push(payload.format);
                    return new Promise(resolve => borderReplies.push({resolve, signal: options?.signal,
                        bounds: [payload.west, payload.south, payload.east, payload.north]}));
                };
                bordersButton.click();
                check(borderReplies.length === 1, 'enabling borders starts an asynchronous request');
                check(borderFormats[0] === 'segments' && borderPath.getAttribute('stroke-width') === '1' &&
                      borderPath.getAttribute('vector-effect') === 'non-scaling-stroke',
                      'borders request native vectors and draw a non-scaling one-pixel SVG stroke');
                await new Promise(resolve => setTimeout(resolve, 400));
                check(borderReplies.length === 1 && !borderReplies[0].signal?.aborted,
                      'scheduled viewport refresh shares an identical pending border request');
                globeCanvas.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight', bubbles: true}));
                check(borderReplies[0].signal?.aborted === true, 'moving cancels obsolete border requests');
                for (let i = 0; i < 40 && borderReplies.length < 2; ++i) await new Promise(resolve => setTimeout(resolve, 25));
                check(borderReplies.length === 2, 'new viewport requests replacement borders');
                borderReplies[0].resolve(vectorReply(0));
                await new Promise(resolve => setTimeout(resolve, 30));
                check(globeRoot.dataset.borders !== 'ready', 'stale border response cannot publish');
                borderReplies[1].resolve(vectorReply());
                for (let i = 0; i < 40 && globeRoot.dataset.borders !== 'ready'; ++i)
                    await new Promise(resolve => setTimeout(resolve, 25));
                check(globeRoot.dataset.borders === 'ready', 'visible shared borders load independently');
                bordersButton.click();
                bordersButton.click();
                await new Promise(requestAnimationFrame);
                check(globeRoot.dataset.borders === 'ready' && borderReplies.length === 2,
                      'switching borders back on reuses native geometry without a network round trip');
                for (let i = 0; i < 9; ++i) zoomIn.click();
                await new Promise(requestAnimationFrame);
                check(Number(borderPath.getAttribute('opacity')) === 0.7 && globeRoot.dataset.borders !== 'zoom-hidden',
                      'close zoom never silently fades or disables an enabled border overlay');
                for (let i = 0; i < 40 && borderReplies.length < 3; ++i) await new Promise(resolve => setTimeout(resolve, 25));
                check(borderReplies.length === 3, 'close zoom requests a bounded local border selection');
                check(borderReplies[2].bounds[2] - borderReplies[2].bounds[0] <
                      borderReplies[1].bounds[2] - borderReplies[1].bounds[0], 'zooming in narrows border coverage');
                borderReplies[2].resolve(vectorReply());
                for (let i = 0; i < 40 && globeRoot.dataset.borders !== 'ready'; ++i)
                    await new Promise(resolve => setTimeout(resolve, 25));
                await new Promise(requestAnimationFrame);
                check(globeRoot.dataset.borders === 'ready' && Boolean(borderPath.getAttribute('d')),
                      `regional borders draw at close zoom: ${globeRoot.dataset.borders}; ` +
                      `${JSON.stringify(borderReplies[2].bounds)}; ${borderPath.getAttribute('d')}; ` +
                      `${JSON.stringify(gl.getUniform(program, gl.getUniformLocation(program, 'camera')!))}`);
                bordersButton.click();
                bordersButton.click();
                await new Promise(requestAnimationFrame);
                check(borderReplies.length === 3 && Boolean(borderPath.getAttribute('d')),
                      'off/on at close zoom restores cached borders without another request');
                for (let i = 0; i < 24; ++i) zoomOut.click();
                await new Promise(resolve => setTimeout(resolve, 400));
                check(borderReplies.length === 4, 'zooming beyond local coverage requests the wider region');
                borderReplies[3].resolve(vectorReply());
                for (let i = 0; i < 40 && globeRoot.dataset.borders !== 'ready'; ++i)
                    await new Promise(resolve => setTimeout(resolve, 25));
                bordersButton.click();
                await new Promise(requestAnimationFrame);
                check(globeRoot.dataset.borders === 'off' && borderPath.getAttribute('d') === '',
                      'switching borders off hides vectors without changing the observer');
            } finally {
                if (bordersButton.getAttribute('aria-pressed') === 'true') bordersButton.click();
                borderReplies.forEach(request => request.resolve(vectorReply()));
                labHttp.fetch = borderFetch;
                for (let i = 0; i < 24; ++i) zoomOut.click();
            }
            await checkGlobeFlights(check);
        } finally {
            globalThis.Worker = originalWorker;
            resetGlobeWorker();
            httpFixture.dispose();
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
