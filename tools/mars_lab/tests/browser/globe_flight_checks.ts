/**
 * @file globe_flight_checks.ts
 * @brief Long-route camera checks on the real browser animation clock.
 *
 * Exercises northern and southern great-circle journeys, including separate arrival
 * roll correction, current-zoom retention and same-frame GPU submission. Timings
 * are diagnostic rather than hardware-dependent pass thresholds.
 */
const globeFlightRoutes: [string, number, number, number, number][] = [
    ['Edinburgh–Juneau', 55.9533, -3.1883, 58.3019, -134.4197],
    ['Los Angeles–Moscow', 34.0522, -118.2437, 55.7558, 37.6173],
    ['Sydney–Santiago', -33.8688, 151.2093, -33.4489, -70.6693]
];

async function checkGlobeFlights(check: (condition: boolean, message: string) => void): Promise<void> {
    const globe = Reflect.get(MarsAlmanacGlobe, 'instance') as {
        orientation: number[]; view: number[]; flightTarget: number[] | null; gl: WebGLRenderingContext;
        worldLoading: boolean; shoreRetryAfter: number; palettesReady: boolean;
        flightGeneration: number; historyRedirect: boolean; saveCamera(): void;
        applyCamera(camera: number[]): void; stopRegionRequests(): void;
        shoreJob(entry: GlobeWorkerRequest['entry'], args: GlobeWorkerRequest['args'], signal?: AbortSignal): Promise<unknown>;
    };
    const savedMode = WorksheetWorkspace.mode.currentMode();
    const savedCamera = [...globe.orientation, globe.view[2]];
    const latitude = document.getElementById('almanacLatitude') as HTMLInputElement;
    const longitude = document.getElementById('almanacLongitude') as HTMLInputElement;
    const savedObserver = [latitude.value, longitude.value];
    const savedRetry = globe.shoreRetryAfter;
    const applyCamera = globe.applyCamera, native = labDOM.call, shoreJob = globe.shoreJob;
    const matchMedia = window.matchMedia, drawArrays = globe.gl.drawArrays;
    const timings: string[] = [];
    let bordersDuringFlight = 0, geometryDuringFlight = 0, submissions = 0, flightProgress = 0;
    let delayNextGeometry = false, waitingForGeometry = false;
    let releaseGeometry = (): void => {};
    try {
        WorksheetWorkspace.mode.setMode('almanac');
        window.matchMedia = query => query === '(prefers-reduced-motion: reduce)' ?
            {matches: false} as MediaQueryList : matchMedia.call(window, query);
        labDOM.call = <T>(name: string, ...args: unknown[]): T => {
            if (globe.flightTarget && name === 'lab_globe_borders_path') ++bordersDuringFlight;
            const value = native<T>(name, ...args);
            if (name === 'lab_globe_flight_advance') flightProgress = Number(value);
            return value;
        };
        globe.shoreJob = async (entry, args, signal) => {
            if (globe.flightTarget) ++geometryDuringFlight;
            if (globe.flightTarget && entry === 'fetch' && delayNextGeometry) {
                delayNextGeometry = false;
                waitingForGeometry = true;
                try { await new Promise<void>(resolve => { releaseGeometry = resolve; }); }
                finally { waitingForGeometry = false; }
            }
            return shoreJob.call(globe, entry, args, signal);
        };
        globe.gl.drawArrays = (mode, first, count) => {
            ++submissions;
            drawArrays.call(globe.gl, mode, first, count);
        };
        for (const [name, fromLat, fromLon, toLat, toLon] of globeFlightRoutes) {
            flightProgress = 0;
            delayNextGeometry = name === 'Edinburgh–Juneau';
            const from = [...labDOM.call('lab_globe_centre', fromLat, fromLon) as number[],
                name === 'Edinburgh–Juneau' ? 150 : name === 'Sydney–Santiago' ? 512 : 87.25];
            const to = [...labDOM.call('lab_globe_centre', toLat, toLon) as number[],
                name === 'Edinburgh–Juneau' ? 20 : name === 'Sydney–Santiago' ? 1 : 512];
            applyCamera.call(globe, from);
            const token = MarsAlmanacGlobe.beginHistory();
            if (delayNextGeometry) {
                globe.shoreRetryAfter = 0;
                native('lab_globe_shore_clear');
            }
            latitude.value = String(toLat);
            longitude.value = String(toLon);
            const frameTimes: number[] = [], submissionTimes: number[] = [];
            let arrivalRollFrames = 0, deferredFrames = 0, destinationSeen = false, destinationLost = false;
            globe.applyCamera = camera => {
                const start = performance.now(), before = submissions;
                applyCamera.call(globe, camera);
                if (submissions === before) ++deferredFrames;
                submissionTimes.push(performance.now() - start);
                frameTimes.push(start);
                if (frameTimes.length % 120 === 1) {
                    const program = globe.gl.getParameter(globe.gl.CURRENT_PROGRAM) as WebGLProgram;
                    check(Boolean(globe.gl.getUniform(program, globe.gl.getUniformLocation(program, 'naturalStyle')!)) &&
                          !globe.gl.getUniform(program, globe.gl.getUniformLocation(program, 'interactionPreview')!),
                          `${name}: history travel always keeps Natural Earth without a coarse map preview`);
                    check(document.querySelector<SVGSVGElement>('.globe-shore-overlay')!.style.display === 'none',
                          `${name}: the compositor never repaints hidden SVG underneath the GPU surface`);
                }
                const point = native('lab_globe_project', toLat, toLon, camera.slice(0, 4)) as number[];
                const visible = point[2] >= 0 && Math.max(Math.abs(point[0]), Math.abs(point[1])) * camera[4] <= 1.12;
                if (destinationSeen && !visible) destinationLost = true;
                destinationSeen ||= visible;
                if (camera[4] === from[4] && Math.abs(globe.view[0] - toLat) < 1e-8 &&
                    Math.abs(globe.view[1] - toLon) < 1e-8) ++arrivalRollFrames;
            };
            const loading = globe.worldLoading;
            globe.worldLoading = true;
            try {
                MarsAlmanacGlobe.finishHistory(token, to, true);
                await new Promise(resolve => setTimeout(resolve, 200));
                check(globe.orientation.every((value, i) => value === from[i]) && globe.view[2] === from[4],
                      `${name}: travel waits for the global surface without consuming its display clock`);
            } finally { globe.worldLoading = loading; }
            for (let i = 0; i < 200 && !globe.palettesReady; ++i) await new Promise(resolve => setTimeout(resolve, 25));
            check(globe.palettesReady, 'photographic correction palettes are ready before travelling');
            if (name === 'Edinburgh–Juneau') {
                const deadline = performance.now() + 15000;
                while ((!waitingForGeometry || flightProgress <= 0) && performance.now() < deadline)
                    await new Promise(resolve => setTimeout(resolve, 50));
                check(waitingForGeometry && flightProgress > 0,
                      'camera animation advances while local vectors load over the ready Natural Earth surface; ' +
                      `waiting=${waitingForGeometry}, progress=${flightProgress}`);
                releaseGeometry();
            }
            const began = performance.now();
            while (globe.flightTarget && performance.now() - began < 360000)
                await new Promise(resolve => setTimeout(resolve, 50));
            check(!globe.flightTarget && frameTimes.length > 30 && arrivalRollFrames > 2,
                  `${name}: completes a real animated flight with a separate arrival-roll phase; ` +
                  `progress=${flightProgress}, zoom=${globe.view[2]}, frames=${frameTimes.length}, arrival=${arrivalRollFrames}`);
            check(globe.view[2] === from[4] && globe.orientation.every((value, i) => Math.abs(value - to[i]) < 1e-12),
                  `${name}: retains the current zoom while restoring destination orientation`);
            check(deferredFrames === 0, `${name}: camera updates submit to the GPU in the same frame`);
            check(bordersDuringFlight === 0, `${name}: optional border projection waits until arrival`);
            check(destinationSeen && !destinationLost, `${name}: the destination remains visible once approached`);
            const gaps = frameTimes.slice(1).map((time, i) => time - frameTimes[i]).sort((a, b) => a - b);
            submissionTimes.sort((a, b) => a - b);
            timings.push(`${name}: ${((performance.now() - began) / 1000).toFixed(1)} s, ${frameTimes.length} frames; median/p95 interval ` +
                `${gaps[Math.floor(gaps.length / 2)].toFixed(1)}/${gaps[Math.floor(gaps.length * .95)].toFixed(1)} ms; ` +
                `median/p95 submit ${submissionTimes[Math.floor(submissionTimes.length / 2)].toFixed(1)}/` +
                `${submissionTimes[Math.floor(submissionTimes.length * .95)].toFixed(1)} ms`);
        }
        check(geometryDuringFlight > 0, 'shoreline detail can refresh during travel instead of waiting until arrival');
        globe.applyCamera = applyCamera;
        const bookmarkKey = 'mars.exprLab.globeCamera.v1';
        const bookmark = localStorage.getItem(bookmarkKey);
        const camera = (lat: number, lon: number, zoom: number): number[] =>
            [...native('lab_globe_centre', lat, lon) as number[], zoom];
        try {
            const departure = camera(55.9533, -3.1883, 150);
            const first = camera(58.3019, -134.4197, 150);
            globe.applyCamera(departure);
            const firstToken = MarsAlmanacGlobe.beginHistory();
            latitude.value = '58.3019'; longitude.value = '-134.4197';
            MarsAlmanacGlobe.finishHistory(firstToken, first, true);
            await new Promise(resolve => setTimeout(resolve, 250));
            const pending = MarsAlmanacGlobe.beginHistory();
            const pendingCamera = [...globe.orientation, globe.view[2]], continuingTarget = globe.flightTarget;
            await new Promise(resolve => setTimeout(resolve, 150));
            check(globe.flightTarget === continuingTarget &&
                  [...globe.orientation, globe.view[2]].some((value, i) => value !== pendingCamera[i]),
                  'the current flight keeps drawing while a replacement history entry restores asynchronously');
            MarsAlmanacGlobe.finishHistory(pending, first, false);
            check(globe.flightTarget === continuingTarget,
                  'an unsuccessful replacement leaves the current journey running');
            // Advance to the actual native midpoint without spending another whole journey here.
            const transit = native('lab_globe_flight', departure, first, .5, 0) as number[];
            globe.applyCamera(transit);
            check(transit[4] < 3 && MarsAlmanacGlobe.captureHistory()![4] === 150,
                  'mid-flight history captures the chosen close zoom, not the temporary overview');
            globe.saveCamera();
            check(JSON.parse(localStorage.getItem(bookmarkKey)!).camera[4] === 150,
                  'a mid-flight browser bookmark retains the destination camera');
            const stale = MarsAlmanacGlobe.beginHistory();
            const latest = MarsAlmanacGlobe.beginHistory();
            latitude.value = '34.0522'; longitude.value = '-118.2437';
            const next = camera(34.0522, -118.2437, 20);
            MarsAlmanacGlobe.finishHistory(stale, first, true);
            check(globe.flightTarget === continuingTarget, 'superseded restoration cannot replace the continuing journey');
            MarsAlmanacGlobe.finishHistory(latest, next, true);
            check(globe.flightTarget?.[4] === 150 && globe.view[2] === transit[4] && globe.historyRedirect,
                  'successive clicks retain chosen zoom through overlapping restorations without a camera jump');
            const end = globe.flightTarget!;
            const early = native('lab_globe_flight', transit, end, .05, 1) as number[];
            const centre = native('lab_globe_pick', 0, 0, transit.slice(0, 4)) as number[];
            const shifted = native('lab_globe_project', centre[0], centre[1], early.slice(0, 4)) as number[];
            check(Math.hypot(shifted[0], shifted[1]) > 1e-6,
                  'redirected travel starts moving without repeating a stationary departure-heading phase');
            const immediate = native('lab_globe_flight', transit, end, .0001, 1) as number[];
            const firstStep = native('lab_globe_project', centre[0], centre[1], immediate.slice(0, 4)) as number[];
            check(Math.hypot(firstStep[0], firstStep[1]) > 1e-6,
                  'redirects start with a non-zero route slope instead of repeating a near-stationary easing ramp');
            const town = document.getElementById('almanacTown') as HTMLSelectElement;
            const option = town.selectedOptions[0];
            if (option) {
                const value = option.value, generation = globe.flightGeneration;
                try {
                    option.value = value + '|refreshed';
                    document.dispatchEvent(new Event('mars-almanac-observer-committed'));
                    check(globe.flightGeneration === generation && globe.flightTarget === end,
                          'catalogue-only town key changes cannot cancel or recenter an active journey');
                } finally { option.value = value; }
            }
            document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!.click();
            const manualZoom = globe.view[2];
            const manualToken = MarsAlmanacGlobe.beginHistory();
            latitude.value = '-33.8688'; longitude.value = '151.2093';
            MarsAlmanacGlobe.finishHistory(manualToken, camera(-33.8688, 151.2093, 512), true);
            check(globe.flightTarget?.[4] === manualZoom && !globe.historyRedirect,
                  'manual zoom cancels old travel intent and establishes the scale for subsequent navigation');
        } finally {
            document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!.click();
            if (bookmark === null) localStorage.removeItem(bookmarkKey);
            else localStorage.setItem(bookmarkKey, bookmark);
        }
        const token = MarsAlmanacGlobe.beginHistory();
        const loading = globe.worldLoading;
        globe.worldLoading = true;
        try {
            MarsAlmanacGlobe.finishHistory(token, savedCamera, true);
            await new Promise(requestAnimationFrame);
            document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!.click();
            check(!globe.flightTarget, 'manual navigation cancels a journey even while imagery is pending');
        } finally { globe.worldLoading = loading; }
        document.getElementById('almanacGlobe')!.dataset.flightTestTiming = timings.join('; ');
    } finally {
        releaseGeometry();
        MarsAlmanacGlobe.beginHistory();
        globe.applyCamera = applyCamera;
        globe.shoreJob = shoreJob;
        globe.shoreRetryAfter = savedRetry;
        globe.gl.drawArrays = drawArrays;
        labDOM.call = native;
        window.matchMedia = matchMedia;
        document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!.click();
        globe.stopRegionRequests();
        [latitude.value, longitude.value] = savedObserver;
        globe.applyCamera(savedCamera);
        WorksheetWorkspace.mode.setMode(savedMode);
    }
}
