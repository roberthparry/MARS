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
        // Insert between retained neighbours, then exercise actual timeline buttons and persisted edits.
        const historyText = (position: number): string => buffer.readSnapshot(native.lab_workspace_history_item(0, position))!.text;
        const editedState = (text: string): Uint8Array<ArrayBuffer> => labWire.encode({state: {...snapshots[0], text}});
        const editingRecord: SavedHistoryRecord = {mode: 0, epoch: '', revision: 0,
            history: {version: 1, current: editedState('3+4'), back: [editedState('2+3')], forward: [editedState('4+5')]}};
        const settle = async (condition: () => boolean, message: string): Promise<void> => {
            const done = (): boolean => !worksheetHistoryEditor.pending && condition();
            for (let i = 0; i < 500 && !done(); ++i) await LabTestBrowser.wait(10);
            check(done(), message + ': ' + JSON.stringify({current: native.lab_workspace_history_current(0),
                editor: WorksheetBindings.editor.currentExpressionText(),
                status: document.getElementById('historyEditorStatus')?.textContent,
                pending: worksheetHistoryEditor.pending}));
        };
        const historyButton = (position: number, action: string): HTMLButtonElement =>
            document.querySelector<HTMLButtonElement>(`#historyEditorEntries [data-history-position="${position}"] [data-history-action="${action}"]`)!;
        const historyRow = (position: number): HTMLElement =>
            document.querySelector<HTMLElement>(`#historyEditorEntries [data-history-position="${position}"]`)!;
        const pointer = (type: string, x: number, y: number, pointerType = 'mouse'): PointerEvent =>
            new PointerEvent(type, {bubbles: true, cancelable: true, pointerId: 91, isPrimary: true,
                pointerType, button: 0, buttons: type === 'pointerup' ? 0 : 1, clientX: x, clientY: y});
        const beginDrag = async (from: number, to: number, pointerType = 'mouse', after = false): Promise<void> => {
            const grip = historyRow(from).querySelector<HTMLElement>('.history-drag-handle')!;
            check(Boolean(grip) && getComputedStyle(grip).touchAction === 'none', 'history has a touch-safe drag grip');
            const start = grip.getBoundingClientRect(), target = historyRow(to).getBoundingClientRect();
            const x = start.left + start.width / 2, y = after ? target.bottom - 5 : target.top + 5;
            grip.dispatchEvent(pointer('pointerdown', x, start.top + start.height / 2, pointerType));
            document.dispatchEvent(pointer('pointermove', x, y, pointerType));
            await new Promise(requestAnimationFrame);
        };
        const endDrag = (from: number, to: number, pointerType = 'mouse', after = false): void => {
            const start = historyRow(from).querySelector('.history-drag-handle')!.getBoundingClientRect();
            const target = historyRow(to).getBoundingClientRect();
            document.dispatchEvent(pointer('pointerup', start.left + start.width / 2,
                after ? target.bottom - 5 : target.top + 5, pointerType));
        };
        try {
            worksheetHistoryEditor.start();
            owner.install(editingRecord);
            WorksheetWorkspace.mode.setMode('expression', {force: true});
            await WorksheetPersistence.navigation.restore(buffer.readSnapshot(native.lab_workspace_history_read(0, 2, 0)));
            WorksheetBindings.controller.setExpressionEditor('5+6');
            await WorksheetRequests.evaluation.evaluateExpression();
            check(native.lab_workspace_history_count(0, 0) === 2 && native.lab_workspace_history_count(0, 1) === 1 &&
                historyText(0) === '2+3' && historyText(1) === '3+4' && historyText(2) === '5+6' && historyText(3) === '4+5',
                'a new evaluation is inserted after the current worksheet without losing Forward');
            document.getElementById('historyEdit')!.click();
            check((document.getElementById('historyEditor') as HTMLDialogElement).open &&
                document.querySelectorAll('#historyEditorEntries li').length === 4,
                'history editor shows the native timeline: ' + JSON.stringify({
                    open: (document.getElementById('historyEditor') as HTMLDialogElement).open,
                    rows: document.querySelectorAll('#historyEditorEntries li').length,
                    status: document.getElementById('status')?.textContent,
                    persistence: document.getElementById('historyPersistenceStatus')?.textContent}));
            for (const [position, text] of ['2+3', '3+4', '5+6', '4+5'].entries()) {
                const open = historyButton(position, 'open'), row = open.closest('li')!;
                check(open.textContent === text && open.innerHTML === text &&
                    (open.getAttribute('title') || open.dataset.marsTooltipTitle) === text &&
                    open.getAttribute('aria-label') === 'Open worksheet: ' + text,
                    'worksheet formula is rendered as visible, accessible DOM text: ' + JSON.stringify({
                        position, text, content: open.textContent, html: open.innerHTML,
                        title: open.getAttribute('title'), accessible: open.getAttribute('aria-label')}));
                check(row.classList.contains('history-entry') && open.classList.contains('history-entry-open') &&
                    open.classList.contains('secondary') && !!row.querySelector('.history-entry-actions') &&
                    getComputedStyle(row).display === 'flex' && open.getBoundingClientRect().width > 100,
                    'native DOM setters apply history row and button styling');
                check(historyButton(position, 'earlier').innerHTML === '↑ Earlier' &&
                    historyButton(position, 'later').innerHTML === '↓ Later' &&
                    historyButton(position, 'remove').innerHTML === 'Remove', 'all history actions have visible labels');
                check(historyButton(position, 'earlier').matches(':disabled') === (position === 0) &&
                    historyButton(position, 'later').matches(':disabled') === (position === 3) &&
                    !historyButton(position, 'remove').matches(':disabled'),
                    'unavailable moves are disabled through the browser DOM');
            }
            check(document.querySelector('#historyEditorEntries [aria-current="step"] .history-current')?.innerHTML === 'Current',
                'the current worksheet has a visible marker');
            const preview = document.createElement('ol');
            labDOM.call('lab_workspace_dom_history_row', preview, {text: ''}, 0, 0, 1);
            check(preview.querySelector('.history-entry-open')?.innerHTML === 'Empty worksheet' &&
                [...preview.querySelectorAll<HTMLButtonElement>('.history-entry-actions button')].every(b => b.matches(':disabled')),
                'a sole empty worksheet has a readable placeholder and disabled move/remove controls');
            preview.replaceChildren();
            const authored = '<img src=x onerror=alert(1)>\n  + x';
            labDOM.call('lab_workspace_dom_history_row', preview, {text: authored}, 0, 0, 1);
            check(preview.querySelector('.history-entry-open')?.textContent === authored.replace(/\s+/g, ' ') &&
                !preview.querySelector('img'), 'authored worksheet text stays literal and cannot insert markup');
            const place = {mode: 'almanac', text: 'Raw navigation worksheet summary', almanac: {
                date: '2026-10-30', time: '10:22:22', jurisdiction: 'GB-ENG',
                town: 'Shrewsbury (GeoNames 2637891)|52.7077|-2.7541|75', latitude: '52.7077', longitude: '-2.7541'
            }};
            const savedPlace = JSON.stringify(place);
            preview.replaceChildren();
            labDOM.call('lab_workspace_dom_history_row', preview, place, 0, 0, 1);
            const placeButton = preview.querySelector<HTMLButtonElement>('[data-history-location]')!;
            check(placeButton.textContent?.includes('Shrewsbury') && placeButton.textContent.includes('England') &&
                !placeButton.textContent.includes('GeoNames') && !placeButton.textContent.includes('Raw navigation') &&
                placeButton.querySelector('.history-entry-place-detail')?.textContent === '2026-10-30 10:22:22 GMT',
                'existing calendar history shows town, country and GMT without exposing the raw summary or catalogue IDs');
            const flagURI = 'data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciLz4=';
            labDOM.call('lab_workspace_dom_history_country', placeButton, {name: 'England', flag: flagURI});
            check(placeButton.querySelector<HTMLImageElement>('img')?.getAttribute('src') === flagURI &&
                !placeButton.querySelector<HTMLImageElement>('img')?.hidden && JSON.stringify(place) === savedPlace,
                'database artwork appears without modifying the saved worksheet');
            preview.replaceChildren();
            labDOM.call('lab_workspace_dom_history_row', preview, {...place, almanac: {...place.almanac,
                jurisdiction: '', town: ''}}, 0, 0, 1);
            const customButton = preview.querySelector<HTMLButtonElement>('[data-history-location]')!;
            labDOM.call('lab_workspace_dom_history_country', customButton, {country: 'South Africa',
                country_flag: 'https://invalid.example/flag.svg'});
            check(customButton.textContent?.includes('South Africa') && customButton.textContent.includes('52° 42.46′ N') &&
                customButton.textContent.includes('2° 45.25′ W') &&
                customButton.querySelector<HTMLImageElement>('img')?.hidden,
                'coordinate-only entries accept resolved country text but reject remote artwork');
            preview.replaceChildren();
            labDOM.call('lab_workspace_dom_history_row', preview, {mode: 'datetime', text: 'Calendar summary',
                datetime: {...place.almanac, jurisdiction: 'FR', town: 'Brest|-|0|0'}}, 0, 0, 1);
            labDOM.call('lab_workspace_dom_history_country', preview.querySelector('.history-entry-open'),
                {name: 'France', flag: flagURI});
            check(preview.querySelector('.history-entry-open')?.textContent?.includes('Brest') &&
                preview.querySelector('.history-entry-country')?.textContent?.includes('France') &&
                preview.querySelector('.history-entry-place-detail')?.textContent === '2026-10-30',
                'DateTime history uses the same saved place presentation without inventing a GMT time: ' + preview.textContent);
            const townSelect = document.getElementById('almanacTown') as HTMLSelectElement;
            const selectedTown = townSelect.value, pendingTown = document.createElement('option');
            pendingTown.value = 'History-only town (GeoNames 1234)|0|90|0';
            townSelect.append(pendingTown);
            try {
                townSelect.value = pendingTown.value;
                const notice = document.createElement('div');
                const start = [...labDOM.call('lab_globe_centre', 0, 0) as number[], 100];
                const middle = [...labDOM.call('lab_globe_centre', 0, 45) as number[], 100];
                const target = [...labDOM.call('lab_globe_centre', 0, 90) as number[], 100];
                labDOM.call('lab_globe_travel_notice', notice, start, target);
                check(notice.textContent === 'Travelling to History-only town · 10008 km remaining',
                    'journeys use the saved town name without waiting for the town cache');
                labDOM.call('lab_globe_travel_notice', notice, middle, target);
                check(notice.textContent === 'Travelling to History-only town · 5004 km remaining',
                    'remaining journey distance decreases with the camera position');
                labDOM.call('lab_globe_travel_notice', notice, target, target);
                check(notice.textContent === 'Orienting view at History-only town',
                    'arrival orientation does not report the original distance as remaining travel');
            } finally { townSelect.value = selectedTown; pendingTown.remove(); }
            await beginDrag(3, 0);
            check(historyRow(3).classList.contains('history-dragging') &&
                historyRow(0).classList.contains('history-drop-before') && worksheetHistoryEditor.pending,
                'drag feedback shows the insertion point without changing native history');
            check(historyText(0) === '2+3' && native.lab_workspace_history_current(0) === 2,
                'previewing a drag does not move the current worksheet');
            endDrag(3, 0);
            await settle(() => historyText(0) === '4+5' && native.lab_workspace_history_current(0) === 3,
                'mouse drop moves across several entries while preserving current identity');
            historyButton(0, 'open').dispatchEvent(new MouseEvent('click', {bubbles: true, cancelable: true, detail: 1}));
            check((document.getElementById('historyEditor') as HTMLDialogElement).open &&
                native.lab_workspace_history_current(0) === 3 && WorksheetBindings.editor.currentExpressionText() === '5+6',
                'releasing a drag never accidentally opens the row below the pointer');
            await beginDrag(0, 3, 'touch', true);
            endDrag(0, 3, 'touch', true);
            await settle(() => historyText(0) === '2+3' && historyText(3) === '4+5' &&
                native.lab_workspace_history_current(0) === 2, 'touch drop moves an entry to the end');
            const beforeCancelledDrag = native.lab_workspace_history_revision(0);
            await beginDrag(3, 0);
            document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape', bubbles: true, cancelable: true}));
            check(!worksheetHistoryEditor.pending && !document.querySelector('.history-dragging, .history-drop-before, .history-drop-after') &&
                native.lab_workspace_history_revision(0) === beforeCancelledDrag &&
                (document.getElementById('historyEditor') as HTMLDialogElement).open,
                'Escape cancels the gesture, retains history and leaves the editor open');
            await beginDrag(3, 0, 'touch');
            document.dispatchEvent(pointer('pointercancel', 0, 0, 'touch'));
            check(!worksheetHistoryEditor.pending && native.lab_workspace_history_revision(0) === beforeCancelledDrag,
                'touch cancellation never commits a move');
            await beginDrag(3, 0);
            check(native.lab_workspace_history_move(0, 0, 3), 'simulate a concurrent native history edit');
            const changedDuringDrag = native.lab_workspace_history_revision(0);
            endDrag(3, 0);
            check(native.lab_workspace_history_revision(0) === changedDuringDrag && !worksheetHistoryEditor.pending,
                'a stale drag cannot overwrite a newer timeline revision');
            check(native.lab_workspace_history_move(0, 3, 0), 'restore the authored history after revision checks');
            worksheetHistoryEditor.refresh();
            historyButton(3, 'earlier').click();
            await settle(() => historyText(2) === '4+5', 'move a forward entry across the current worksheet');
            await LabTestBrowser.wait(20);
            check(native.lab_workspace_history_current(0) === 3 && historyText(3) === '5+6', 'moving keeps the active worksheet');
            check(document.activeElement === historyButton(2, 'earlier'), 'focus follows the moved entry and its action');
            (document.activeElement as HTMLButtonElement).click();
            await settle(() => historyText(1) === '4+5', 'the same move control can be used repeatedly');
            check(document.activeElement === historyButton(1, 'earlier'), 'a repeated move retains its control');
            historyButton(1, 'later').click();
            await settle(() => historyText(2) === '4+5', 'move the same entry back later');
            check(document.activeElement === historyButton(2, 'later'), 'Later follows its entry rather than its old neighbour');
            historyButton(2, 'remove').click();
            await settle(() => native.lab_workspace_history_current(0) === 2, 'remove an archived entry');
            await LabTestBrowser.wait(20);
            historyButton(2, 'remove').click();
            await settle(() => document.getElementById('historyEditorStatus')!.textContent === 'Entry removed.' &&
                native.lab_workspace_history_current(0) === 1 && historyText(1) === '3+4' &&
                WorksheetBindings.editor.currentExpressionText() === '3+4', 'remove current and restore its predecessor');
            check(WorksheetBindings.editor.currentExpressionText() === '3+4', 'promoted worksheet is restored into the editor');
            const editedClient = id(), captured = owner.capture(0)!;
            const savedEdit = await call({action: 'save', client: editedClient, mode: 0, epoch: '', revision: 0, history: captured});
            const loadedEdit = (await call({action: 'load', client: editedClient})).body.records as SavedHistoryRecord[];
            check(savedEdit.status === 200 && loadedEdit.length === 1, 'edited history persists through the server');
            owner.install(loadedEdit[0]);
            check(historyText(0) === '2+3' && historyText(1) === '3+4', 'saved removal and ordering survive reinstallation');
            worksheetHistoryEditor.refresh();
            historyButton(0, 'open').click();
            await settle(() => native.lab_workspace_history_current(0) === 0 &&
                WorksheetBindings.editor.currentExpressionText() === '2+3', 'open an arbitrary saved entry');
            check(!(document.getElementById('historyEditor') as HTMLDialogElement).open, 'opening an entry returns to the worksheet');
            const editedHistory = owner.capture(0)!;
            owner.install({mode: 0, epoch: '', revision: 0, history: {version: 1, current: editedState('2+3'),
                back: Array.from({length: 45}, (_, i) => editedState(`${i}+1`)), forward: [editedState('4+5')]}});
            const scrollBeforeHistory = window.scrollY;
            document.getElementById('historyEdit')!.click();
            const dialog = document.getElementById('historyEditor') as HTMLDialogElement;
            const list = document.getElementById('historyEditorEntries')!;
            const heading = dialog.querySelector<HTMLElement>('.history-editor-heading')!;
            const phone = document.createElement('iframe');
            phone.style.cssText = 'position:fixed;left:-10000px;top:0;width:325px;height:672px;border:0';
            try {
                const loaded = new Promise<void>(resolve => { phone.onload = (): void => resolve(); });
                phone.srcdoc = '<!doctype html><html><head><meta name="viewport" content="width=device-width, initial-scale=1">' +
                    '<link rel="stylesheet" href="/index.css"></head><body></body></html>';
                document.body.append(phone);
                await loaded;
                const doc = phone.contentDocument!, view = phone.contentWindow!;
                doc.documentElement.dataset.skin = document.documentElement.dataset.skin;
                const copy = dialog.cloneNode(true) as HTMLDialogElement;
                copy.open = false;
                doc.body.append(copy);
                const entries = copy.querySelector<HTMLElement>('#historyEditorEntries')!;
                labDOM.call('lab_workspace_dom_history_row', entries, place, 46, 46, 48);
                labDOM.call('lab_workspace_dom_history_row', entries, {...place, almanac: {...place.almanac,
                    jurisdiction: 'AU-NSW', town: 'Sydney|0|0|0'}}, 47, 46, 48);
                copy.querySelector<HTMLElement>('#historyEditorStatus')!.textContent = '';
                copy.showModal();
                await doc.fonts.ready;
                for (const [width, height] of [[325, 672], [390, 844], [672, 325]]) {
                    phone.style.width = width + 'px';
                    phone.style.height = height + 'px';
                    await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
                    const box = copy.getBoundingClientRect(), listBox = entries.getBoundingClientRect();
                    check(Math.abs(box.width - width) < 1 && Math.abs(box.height - height) < 1 && box.top === 0 && box.left === 0,
                        `${width}x${height}: History fills the phone viewport`);
                    check(listBox.height > height * .5 && entries.scrollWidth <= entries.clientWidth && copy.scrollTop === 0,
                        `${width}x${height}: the entry list gets most of the screen without horizontal overflow`);
                    check(view.getComputedStyle(copy.querySelector('#historyEditorStatus')!).display === 'none',
                        'an empty live status does not reserve a blank paragraph');
                    const help = copy.querySelector<HTMLDetailsElement>('.history-editor-help')!;
                    check(view.getComputedStyle(help).display === 'none' &&
                        view.getComputedStyle(copy.querySelector('.history-editor-intro')!).display === 'none',
                        `${width}x${height}: mobile History omits the introduction and help section`);
                    check(listBox.top - copy.querySelector('.history-editor-heading')!.getBoundingClientRect().bottom <= 8,
                        `${width}x${height}: entries follow the heading without an instructions gap`);
                    for (const row of entries.querySelectorAll<HTMLElement>('.history-entry')) {
                        const open = row.querySelector<HTMLElement>('.history-entry-open')!;
                        const grip = row.querySelector<HTMLElement>('.history-drag-handle')!;
                        const actions = [...row.querySelectorAll<HTMLButtonElement>('.history-entry-actions button')];
                        const rowBox = row.getBoundingClientRect(), openBox = open.getBoundingClientRect();
                        check(grip.getBoundingClientRect().width >= 44 && openBox.width >= 180 && openBox.height >= 44 &&
                            openBox.left >= listBox.left && openBox.right <= listBox.right && rowBox.width <= listBox.width,
                            `${width}x${height}: grips and worksheet previews share a readable row`);
                        const top = actions[0].getBoundingClientRect().top;
                        check(actions.every(button => {
                            const actionBox = button.getBoundingClientRect();
                            return actionBox.height >= 44 && actionBox.width >= 44 && Math.abs(actionBox.top - top) < 1 &&
                                actionBox.left >= listBox.left && actionBox.right <= listBox.right;
                        }), `${width}x${height}: Earlier, Later and Remove stay together with finger-sized targets`);
                    }
                    entries.scrollTop = entries.scrollHeight;
                    const closeBox = copy.querySelector<HTMLElement>('#historyEditorClose')!.getBoundingClientRect();
                    check(copy.scrollTop === 0 && closeBox.top >= 0 && closeBox.bottom <= height &&
                        entries.getBoundingClientRect().height > height * .3,
                        `${width}x${height}: a scrolled list leaves Close and the entries accessible`);
                }
                entries.replaceChildren();
                labDOM.call('lab_workspace_dom_history_row', entries, place, 0, 0, 1);
                const soleOpen = entries.querySelector<HTMLElement>('.history-entry-open')!.getBoundingClientRect();
                const soleAction = entries.querySelector<HTMLElement>('.history-entry-actions button')!.getBoundingClientRect();
                check(soleOpen.width > 300 && soleAction.top >= soleOpen.bottom,
                    'a single landscape entry uses the full width without its disabled actions overlapping the name');
                phone.style.width = '900px';
                phone.style.height = '900px';
                check(view.getComputedStyle(copy.querySelector('.history-editor-help')!).display !== 'none' &&
                    view.getComputedStyle(copy.querySelector('.history-editor-intro')!).display !== 'none',
                    'desktop History retains its introduction and help section');
            } finally { phone.remove(); }
            list.scrollTop = list.scrollHeight;
            await new Promise(requestAnimationFrame);
            const dialogBox = dialog.getBoundingClientRect(), headingBox = heading.getBoundingClientRect();
            check(list.scrollTop > 0 && dialog.scrollTop === 0 && headingBox.top >= dialogBox.top &&
                headingBox.bottom <= dialogBox.bottom && headingBox.top >= 0,
                'long history scrolls its entries while the heading and Close button remain visible');
            const last = list.lastElementChild as HTMLElement;
            const grip = last.querySelector<HTMLElement>('.history-drag-handle')!, gripBox = grip.getBoundingClientRect();
            const edge = list.getBoundingClientRect(), scrollBeforeDrag = list.scrollTop;
            grip.dispatchEvent(pointer('pointerdown', gripBox.left + gripBox.width / 2, gripBox.top + gripBox.height / 2, 'touch'));
            document.dispatchEvent(pointer('pointermove', gripBox.left + gripBox.width / 2, edge.top + 8, 'touch'));
            for (let attempt = 0; attempt < 100 && list.scrollTop >= scrollBeforeDrag - 30; ++attempt)
                await LabTestBrowser.wait(10);
            check(list.scrollTop < scrollBeforeDrag - 30 && window.scrollY === scrollBeforeHistory && dialog.scrollTop === 0,
                'holding a touch drag near an edge scrolls only the history list');
            document.dispatchEvent(pointer('pointercancel', 0, 0, 'touch'));
            check(!worksheetHistoryEditor.pending && !list.querySelector('.history-dragging'),
                'cancelling an auto-scrolling drag clears feedback and its animation frame');
            document.getElementById('historyEditorClose')!.click();
            check(Math.abs(window.scrollY - scrollBeforeHistory) < 1, 'closing history preserves the page scroll position');
            owner.install({mode: 0, epoch: '', revision: 0, history: editedHistory});
        } finally {
            (document.getElementById('historyEditor') as HTMLDialogElement).close();
            owner.install(records[0]);
        }
        // Exercise the real navigation/restoration/evaluation pipeline, not only the native stack API.
        const matchMedia = window.matchMedia, historyFetch = window.fetch, scroll = window.scrollY;
        const globe = Reflect.get(MarsAlmanacGlobe, 'instance') as {
            orientation: number[]; view: number[]; worldLoading: boolean; flightTarget: number[] | null; started: boolean;
            flightGeneration: number; flightFrame: number; travelNotice: HTMLElement;
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
            const savedObserver: WorksheetSnapshot = {...snapshots[6], almanac: {
                ...snapshots[6].almanac as Record<string, string>, jurisdiction: 'GB-ENG',
                town: 'Shrewsbury|52.7077|-2.7541|75', latitude: '52.7077', longitude: '-2.7541',
                date: '2026-10-30', time: '10:22:22'
            }};
            owner.install({mode: 6, epoch: '', revision: 0, history: {version: 1,
                current: labWire.encode({state: savedObserver}), back: [], forward: []}});
            await WorksheetPersistence.navigation.restore(savedObserver);
            await window.checkLabMobileLayout('Almanac history controls');
            labDOM.call('lab_flag_cache_clear');
            document.getElementById('historyEdit')!.click();
            const savedPlaceButton = document.querySelector<HTMLButtonElement>('#historyEditorEntries [data-history-location]')!;
            check(savedPlaceButton.textContent?.includes('Shrewsbury') && savedPlaceButton.textContent.includes('England'),
                'calendar history is readable immediately while flag artwork loads asynchronously');
            await settle(() => {
                const flag = savedPlaceButton.querySelector<HTMLImageElement>('.history-entry-flag');
                return !!flag && !flag.hidden && flag.complete && flag.naturalWidth > 0;
            }, 'the history flag loads and decodes through the shared country metadata service');
            check(WorksheetContext.almanacLatitude.value === '52.7077' &&
                WorksheetContext.almanacLongitude.value === '-2.7541' &&
                native.lab_workspace_history_count(6, 0) === 0 && native.lab_workspace_history_count(6, 1) === 0,
                'loading history names and flags does not move the observer or add history entries');
            document.getElementById('historyEditorClose')!.click();
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
                if (drawingAdvanced) {
                    const target = globe.flightTarget, generation = globe.flightGeneration;
                    const beforeToggles = globe.orientation.slice();
                    for (const action of ['grid', 'borders']) {
                        const button = document.querySelector<HTMLButtonElement>(`[data-globe-action="${action}"]`)!;
                        const pressed = button.getAttribute('aria-pressed');
                        button.click();
                        check(button.getAttribute('aria-pressed') !== pressed && globe.flightTarget === target &&
                              globe.flightGeneration === generation && globe.flightFrame !== 0 && !globe.travelNotice.hidden,
                              `${action} enables during travel without cancelling or restarting the journey`);
                        button.click();
                        check(button.getAttribute('aria-pressed') === pressed && globe.flightTarget === target &&
                              globe.flightGeneration === generation && globe.flightFrame !== 0 && !globe.travelNotice.hidden,
                              `${action} disables during travel without cancelling or restarting the journey`);
                    }
                    for (let i = 0; i < 12; ++i) await new Promise(requestAnimationFrame);
                    check(globe.orientation.some((value, index) => Math.abs(value - beforeToggles[index]) > 1e-7),
                          'the real journey continues moving after its grid and border toggles change');
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
                  globe.view[2] === 87.25,
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
