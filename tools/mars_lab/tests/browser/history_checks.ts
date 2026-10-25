/**
 * @file history_checks.ts
 * @brief Durable history round trips and atomic native import regressions.
 *
 * Uses temporary browser identities and the isolated server fixture to check all
 * modes, separate profiles, stale writers and restoration of both cursor sides.
 */
interface Window {
    /** @brief Verify history persistence, reopening, concurrency and bounds. */
    checkLabHistoryPersistence(): Promise<void>;
}

window.checkLabHistoryPersistence = async (): Promise<void> => {
    const check = (condition: unknown, message: string): void => { if (!condition) throw new Error('History: ' + message); };
    const native = labWire.exports(), buffer = WorksheetWorkspace.labWorkspaceBuffer;
    const names = WorksheetWorkspace.WORKSPACE_MODE_NAMES, owner = new WorksheetHistoryPersistence();
    const originalMode = WorksheetWorkspace.mode.currentMode(), originalState = WorksheetPersistence.history.snapshot();
    WorksheetWorkspace.presentation.saveCurrentModeResultState();
    const originalResults = names.map(mode => WorksheetWorkspace.presentation.resultStates[mode]);
    const originalClient = localStorage.getItem('mars.history.client.v1'), originalLast = localStorage.getItem('mars.history.mode.v1');
    const originalCamera = MarsAlmanacGlobe.captureHistory();
    const retained = names.map((mode, index) => {
        const read = (side: number, at: number): Uint8Array<ArrayBuffer> => buffer.readBytes(native.lab_workspace_history_read(index, side, at));
        let current = read(2, 0);
        if (!current.length) current = labWire.encode({state: {mode, text: ''}});
        return {mode: index, epoch: '', revision: 0, history: {version: 1, current,
            back: Array.from({length: native.lab_workspace_history_count(index, 0)}, (_, i) => read(0, i)),
            forward: Array.from({length: native.lab_workspace_history_count(index, 1)}, (_, i) => read(1, i))}};
    });
    const id = (): string => [...crypto.getRandomValues(new Uint8Array(16))].map(x => x.toString(16).padStart(2, '0')).join('');
    const client = id(), session = id(), mobile = id();
    const call = async (fields: Record<string, unknown>): Promise<{status: number; body: Record<string, unknown>}> => {
        const response = await labHttp.fetch('/history', {method: 'POST', body: labWire.encode({client, session, ...fields})});
        return {status: response.status, body: await response.labData() as Record<string, unknown>};
    };
    const snapshots: WorksheetSnapshot[] = names.map((mode, index) => {
        const state: WorksheetSnapshot = index >= 5 ? WorksheetPersistence.history.snapshot(mode) : {mode, text: `${index + 1}+2`};
        if (index === 6) state.globeCamera = [...labDOM.call('lab_globe_centre', 55.9533, -3.1883) as number[], 87.25];
        return state;
    });
    const records: SavedHistoryRecord[] = snapshots.map((state, mode) => ({mode, epoch: '', revision: 0,
        history: {version: 1, current: labWire.encode({state}),
            back: [labWire.encode({state: {...state, text: state.text + ' '}})],
            forward: [labWire.encode({state: {...state, text: state.text + '  '}})]}}));
    let reopened: WorksheetHistoryPersistence | null = null;
    const originalFetch = labHttp.fetch;
    try {
        for (const record of records) {
            const saved = await call({action: 'save', ...record});
            check(saved.status === 200, `save ${names[record.mode]}`);
            record.epoch = saved.body.epoch as string; record.revision = saved.body.revision as number;
        }
        const empty = await call({action: 'load', client: mobile});
        check(empty.status === 200 && (empty.body.records as unknown[]).length === 0, 'mobile has independent empty history');
        const saved = (await call({action: 'load'})).body.records as SavedHistoryRecord[];
        check(saved.length === 7, 'all modes persist independently');
        for (const record of saved) {
            owner.install(record);
            check(native.lab_workspace_history_count(record.mode, 0) === 1 &&
                  native.lab_workspace_history_count(record.mode, 1) === 1, 'both navigation directions restored');
            check(buffer.readSnapshot(native.lab_workspace_history_read(record.mode, 2, 0))!.text === snapshots[record.mode].text,
                  'current snapshot retains exact authored input');
            const back = native.lab_workspace_navigate(record.mode, 0, buffer.stage(record.history.current), 1);
            check(buffer.readSnapshot(back)!.text === snapshots[record.mode].text + ' ', 'restored Back selects the previous entry');
            const forward = native.lab_workspace_navigate(record.mode, 1, buffer.stage(record.history.back[0]), 1);
            check(buffer.readSnapshot(forward)!.text === snapshots[record.mode].text, 'restored Forward returns to the current entry');
            owner.install(record);
        }
        // Exercise the real navigation/restoration/evaluation pipeline, not only the native stack API.
        const matchMedia = window.matchMedia, historyFetch = window.fetch, scroll = window.scrollY;
        const globe = Reflect.get(MarsAlmanacGlobe, 'instance') as {
            orientation: number[]; view: number[]; worldLoading: boolean; flightTarget: number[] | null; started: boolean;
            applyCamera(camera: number[]): void;
        };
        const locationState = (lat: number, lon: number, cameraLat = lat, cameraLon = lon): WorksheetSnapshot => ({
            ...snapshots[6],
            almanac: {...snapshots[6].almanac as Record<string, string>, jurisdiction: '', town: '',
                latitude: String(lat), longitude: String(lon)},
            globeCamera: [...labDOM.call('lab_globe_centre', cameraLat, cameraLon) as number[], 87.25]
        });
        try {
            window.matchMedia = query => query === '(prefers-reduced-motion: reduce)' ?
                {matches: true} as MediaQueryList : matchMedia.call(window, query);
            WorksheetWorkspace.mode.setMode('almanac', {force: true});
            WorksheetWorkspace.presentation.syncModeUI();
            document.getElementById('almanacGlobe')!.scrollIntoView({block: 'center'});
            for (let i = 0; i < 100 && !globe.started; ++i) await new Promise(resolve => setTimeout(resolve, 25));
            check(globe.started, 'the visible globe starts before exercising background calculation');
            const edinburgh = locationState(55.9533, -3.1883);
            const santiago = locationState(-33.4489, -70.6693, 58.3019, -134.4197);
            owner.install({mode: 6, epoch: '', revision: 0, history: {version: 1,
                current: labWire.encode({state: edinburgh}), back: [labWire.encode({state: santiago})], forward: []}});
            await WorksheetPersistence.navigation.restore(edinburgh);
            document.querySelector<HTMLButtonElement>('[data-globe-action="centre"]')!.click();
            // The isolated server has no ephemeris database. Keep the real request/install
            // pipeline, supplying native-rendered fixture output only after checking its inputs.
            let destinationRequests = 0;
            const calculation = LabTestBrowser.responseGate(), calculationEntered = LabTestBrowser.signal();
            window.fetch = (url, options) => {
                if (String(url) !== '/almanac-eval') return historyFetch(url, options);
                const payload = labWire.decode(options!.body as Uint8Array<ArrayBuffer>) as Record<string, unknown>;
                check(Number(payload.latitude) === -33.4489 && Number(payload.longitude) === -70.6693,
                      'Back evaluates the restored Santiago observer, not the stale camera location');
                ++destinationRequests;
                calculationEntered.resolve();
                return calculation.promise;
            };
            window.matchMedia = query => query === '(prefers-reduced-motion: reduce)' ?
                {matches: false} as MediaQueryList : matchMedia.call(window, query);
            const departure = globe.orientation.slice();
            let calculationFinished = false, drawingAdvanced = false;
            const back = WorksheetPersistence.navigation.navigate(0).then(() => { calculationFinished = true; });
            try {
                await Promise.race([calculationEntered.promise, back]);
                for (let i = 0; i < 400; ++i) {
                    await new Promise(requestAnimationFrame);
                    if (i >= 12 && globe.orientation.some((value, index) => Math.abs(value - departure[index]) > 1e-7)) {
                        drawingAdvanced = Boolean(globe.flightTarget) && !calculationFinished;
                        break;
                    }
                }
            } finally {
                calculation.resolve(new Response(labWire.encode({...window.labAlmanacFixtures()[4], ok: true}),
                    {headers: {'Content-Type': 'application/x-protobuf'}}));
                await back;
                window.matchMedia = query => query === '(prefers-reduced-motion: reduce)' ?
                    {matches: true} as MediaQueryList : matchMedia.call(window, query);
                await new Promise(requestAnimationFrame);
                await new Promise(requestAnimationFrame);
            }
            check(drawingAdvanced, 'real history travel draws and advances while worksheet calculation is still pending');
            window.fetch = historyFetch;
            check(destinationRequests === 1, 'Back evaluates the restored observer exactly once');
            const latitude = document.getElementById('almanacLatitude') as HTMLInputElement;
            const longitude = document.getElementById('almanacLongitude') as HTMLInputElement;
            check(Math.abs(globe.view[0] + 33.4489) < 1e-7 && Math.abs(globe.view[1] + 70.6693) < 1e-7 &&
                  Number(latitude.value) === -33.4489 && Number(longitude.value) === -70.6693,
                  'real Back restores Santiago controls and camera together despite a legacy Juneau camera');
            const coordinateSummary = (name: string): string => [...document.querySelectorAll('.almanac-summary-row')]
                .find(row => row.querySelector('dt')?.textContent === name)?.querySelector('dd')?.textContent || '';
            check(/33°\s*26\.93/.test(coordinateSummary('Latitude')) && coordinateSummary('Latitude').includes('S') &&
                  /70°\s*40\.16/.test(coordinateSummary('Longitude')) && coordinateSummary('Longitude').includes('W'),
                  'the evaluated result card shows the same Santiago coordinates as the restored globe and controls: ' +
                  JSON.stringify([coordinateSummary('Latitude'), coordinateSummary('Longitude')]));
            // Click the real controls while responses remain unresolved. The mocked transport
            // deliberately ignores aborts so late completions also exercise ownership checks.
            const gates = Array.from({length: 3}, () => LabTestBrowser.responseGate());
            const requests: {signal: AbortSignal | null; latitude: number}[] = [];
            const waitFor = async (condition: () => boolean, message: string): Promise<void> => {
                const deadline = performance.now() + 8000;
                while (!condition() && performance.now() < deadline)
                    await new Promise(resolve => setTimeout(resolve, 10));
                check(condition(), message);
            };
            const reply = (stale: boolean): Response => {
                const fixture = window.labAlmanacFixtures()[4];
                const presentation = structuredClone(fixture.almanac_presentation);
                if (stale) {
                    presentation.all.html = '<p>obsolete-calculation</p>';
                    presentation.visible.html = '<p>obsolete-calculation</p>';
                }
                return new Response(labWire.encode({...fixture, ok: true, almanac_presentation: presentation}),
                    {headers: {'Content-Type': 'application/x-protobuf'}});
            };
            window.fetch = (url, options) => {
                if (String(url) !== '/almanac-eval') return historyFetch(url, options);
                const payload = labWire.decode(options!.body as Uint8Array<ArrayBuffer>) as Record<string, unknown>;
                const index = requests.length;
                requests.push({signal: options?.signal || null, latitude: Number(payload.latitude)});
                check(index < gates.length, 'each replacement button starts only one evaluation');
                return gates[index].promise;
            };
            try {
                const forwardButton = document.getElementById('forward') as HTMLButtonElement;
                const backButton = document.getElementById('back') as HTMLButtonElement;
                const runButton = document.getElementById('run') as HTMLButtonElement;
                forwardButton.click();
                await waitFor(() => requests.length === 1, 'Forward starts an asynchronous calculation');
                check(!document.getElementById('worksheetPending')!.classList.contains('hidden'),
                      'pending navigation marks displayed results as belonging to the previous calculation');
                check(requests[0].latitude === 55.9533 && !backButton.disabled && !runButton.disabled &&
                      forwardButton.disabled, 'pending calculation keeps Back available but disables the empty Forward endpoint');
                forwardButton.click();
                await new Promise(requestAnimationFrame);
                await new Promise(requestAnimationFrame);
                check(requests.length === 1 && !requests[0].signal?.aborted,
                      'Forward at an empty endpoint neither starts work nor cancels the pending calculation');
                backButton.click();
                await waitFor(() => requests.length === 2, 'Back works without waiting for Forward to calculate');
                check(requests[0].signal?.aborted && requests[1].latitude === -33.4489 &&
                      backButton.disabled && !forwardButton.disabled && !runButton.disabled,
                      'Back cancels old work and restores its observer');
                backButton.click();
                await new Promise(requestAnimationFrame);
                await new Promise(requestAnimationFrame);
                check(requests.length === 2 && !requests[1].signal?.aborted,
                      'Back at an empty endpoint leaves the active calculation alone');
                runButton.click();
                await waitFor(() => requests.length === 3, 'Run can replace an unfinished calculation');
                check(requests[1].signal?.aborted && !requests[2].signal?.aborted,
                      'Run retains only the latest request owner');
                gates[2].resolve(reply(false));
                await waitFor(() => !WorksheetRequests.labRequests.busy(), 'latest calculation publishes normally');
                check(document.getElementById('worksheetPending')!.classList.contains('hidden'),
                      'only the completed latest calculation clears the pending result notice');
            } finally {
                gates.forEach((gate, index) => gate.resolve(reply(index !== 2)));
                window.fetch = historyFetch;
            }
            await new Promise(resolve => setTimeout(resolve, 100));
            check(Number(latitude.value) === -33.4489 && !document.body.textContent?.includes('obsolete-calculation'),
                  'late cancelled calculations cannot overwrite the latest observer or result');
            globe.applyCamera([...globe.orientation, 87.25]);
            const wideToken = MarsAlmanacGlobe.beginHistory();
            MarsAlmanacGlobe.finishHistory(wideToken,
                [...labDOM.call('lab_globe_centre', -30, -70) as number[], 1], true);
            check(Math.abs(globe.view[0] + 33.4489) < 1e-7 && Math.abs(globe.view[1] + 70.6693) < 1e-7 &&
                  globe.view[2] === labDOM.call('lab_globe_arrival_zoom', 87.25,
                      document.querySelector<HTMLCanvasElement>('.almanac-globe canvas')!.clientWidth, 21600),
                  'applying the current zoom recentres a saved overview that would otherwise hide the observer');
            await WorksheetPersistence.navigation.navigate(1);
            check(Math.abs(globe.view[0] - 55.9533) < 1e-7 && Math.abs(globe.view[1] + 3.1883) < 1e-7 &&
                  Number(latitude.value) === 55.9533, 'real Forward restores the corresponding location and camera');
            window.matchMedia = query => query === '(prefers-reduced-motion: reduce)' ?
                {matches: false} as MediaQueryList : matchMedia.call(window, query);
            const loading = globe.worldLoading;
            globe.worldLoading = true;
            try {
                const token = MarsAlmanacGlobe.beginHistory();
                latitude.value = '58.3019'; longitude.value = '-134.4197';
                MarsAlmanacGlobe.finishHistory(token, locationState(58.3019, -134.4197).globeCamera, true);
                latitude.value = '-33.4489'; longitude.value = '-70.6693';
                const captured = WorksheetPersistence.history.snapshot();
                const centre = labDOM.call('lab_globe_pick', 0, 0, captured.globeCamera!.slice(0, 4)) as number[];
                check(Math.abs(centre[0] + 33.4489) < 1e-7 && Math.abs(centre[1] + 70.6693) < 1e-7,
                      'selecting another location mid-flight cannot attach the old destination camera to its snapshot');
            } finally {
                MarsAlmanacGlobe.beginHistory();
                globe.worldLoading = loading;
                document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!.click();
            }
        } finally {
            window.matchMedia = matchMedia;
            window.fetch = historyFetch;
            window.scrollTo(0, scroll);
            for (const record of saved) owner.install(record);
        }
        const previous = native.lab_workspace_history_revision(0);
        buffer.stage(new Uint8Array([255, 255, 255, 255]));
        check(!native.lab_workspace_history_replace(0, 1, 1, 4) && native.lab_workspace_history_revision(0) === previous &&
              native.lab_workspace_history_count(0, 0) === 1, 'invalid native import is atomic');
        check((await call({action: 'save', ...records[0], revision: 0, epoch: '', session: id()})).status === 409,
              'a stale tab cannot replace an existing mode');
        check((await call({action: 'save', ...records[0], session: id()})).status === 200, 'loaded revision can save');
        check((await call({action: 'save', ...records[0]})).status === 409, 'older revision remains rejected');
        localStorage.setItem('mars.history.client.v1', client);
        localStorage.setItem('mars.history.mode.v1', '6');
        for (let mode = 0; mode < 7; ++mode) {
            native.lab_workspace_history_clear(mode, 0); native.lab_workspace_history_clear(mode, 1);
        }
        reopened = new WorksheetHistoryPersistence();
        await reopened.start();
        check(WorksheetWorkspace.mode.currentMode() === 'almanac', 'reopening restores the browser’s current mode');
        check(reopened.restoredGlobeCamera?.[4] === 87.25, 'reopening restores retained camera zoom');
        await WorksheetRequests.evaluation.evaluateActiveModeOnLoad();
        for (let mode = 0; mode < 7; ++mode)
            check(native.lab_workspace_history_count(mode, 0) === 1 && native.lab_workspace_history_count(mode, 1) === 1,
                  `${names[mode]}: reopening and initial evaluation retain both sides of the cursor`);
        const beforeCount = WorksheetPersistence.history.backwardCount();
        document.dispatchEvent(new Event('change'));
        await reopened.flush();
        check(WorksheetPersistence.history.backwardCount() === beforeCount, 'checkpointing adds no navigation steps');
        reopened.dispose(); reopened = null;
        labHttp.fetch = async (url, options) => {
            if (String(url) === '/history') throw new Error('Offline history fixture');
            return originalFetch.call(labHttp, url, options);
        };
        reopened = new WorksheetHistoryPersistence();
        await reopened.start();
        check(native.lab_workspace_history_count(6, 0) === 1, 'offline reopening recovers the bounded local copy');
        const altitude = document.getElementById('almanacElevation') as HTMLInputElement;
        altitude.value = '123';
        altitude.dispatchEvent(new Event('input'));
        document.dispatchEvent(new Event('change'));
        await reopened.flush();
        const offlineSnapshot = WorksheetPersistence.history.snapshot();
        reopened.dispose(); reopened = null;
        labHttp.fetch = originalFetch;
        reopened = new WorksheetHistoryPersistence();
        await reopened.start();
        check(Number(altitude.value) === 123, 'unacknowledged offline edits recover when the server revision still matches: ' +
            JSON.stringify({value: altitude.value, offlineSnapshot, restored: WorksheetPersistence.history.snapshot(),
                notice: document.getElementById('historyPersistenceStatus')?.textContent}));
        await reopened.flush();
        reopened.dispose(); reopened = null;
        const text = labWire.encode({state: {mode: 'expression', text: 'x'.repeat(100000)}});
        for (let i = 0; i < 100; ++i) native.lab_workspace_history_push(0, 0, buffer.stage(text), 1, 0);
        const count = native.lab_workspace_history_count(0, 0), bounded = owner.capture(0)!;
        check(bounded.back.length <= 64 && labWire.encode(bounded).length <= 1048576 &&
              native.lab_workspace_history_count(0, 0) === count, 'checkpoint limits do not pop live history');
    } finally {
        labHttp.fetch = originalFetch;
        reopened?.dispose(); owner.dispose();
        for (const record of retained) owner.install(record);
        WorksheetWorkspace.mode.setMode(originalMode, {force: true});
        await WorksheetPersistence.navigation.restore(originalState);
        names.forEach((mode, index) => WorksheetWorkspace.presentation.resultStates[mode] = originalResults[index]);
        WorksheetWorkspace.presentation.restoreModeResultState(originalMode);
        if (originalCamera) {
            const globe = Reflect.get(MarsAlmanacGlobe, 'instance') as {applyCamera(camera: number[]): void};
            globe?.applyCamera(originalCamera);
        }
        if (originalClient === null) localStorage.removeItem('mars.history.client.v1');
        else localStorage.setItem('mars.history.client.v1', originalClient);
        if (originalLast === null) localStorage.removeItem('mars.history.mode.v1');
        else localStorage.setItem('mars.history.mode.v1', originalLast);
        document.getElementById('historyPersistenceStatus')?.remove();
        WorksheetWorkspace.presentation.updateHistoryButtons();
    }
};
