/**
 * @file history.ts
 * @brief Browser identity, bounded history I/O and offline recovery for every worksheet.
 *
 * Native WASM owns live history and atomic replacement. This adapter transports
 * opaque Protobuf snapshots, retaining one bounded IndexedDB recovery record per
 * mode. Optimistic revisions protect independent tabs; random browser identifiers
 * separate profiles but are not authentication credentials.
 */
interface SavedWorksheetHistory {
    version: number;
    current: Uint8Array<ArrayBuffer>;
    back: Uint8Array<ArrayBuffer>[];
    forward: Uint8Array<ArrayBuffer>[];
}
interface SavedHistoryRecord {
    mode: number;
    epoch: string;
    revision: number;
    history: SavedWorksheetHistory;
    client?: string;
    pending?: boolean;
}

class WorksheetHistoryPersistence {
    public restoredGlobeCamera?: number[];
    private client = '';
    private session = '';
    private started = false;
    private restoring = false;
    private pendingSave: Promise<void> | null = null;
    private timer = 0;
    private poll = 0;
    private database: IDBDatabase | null = null;
    private readonly records = new Map<number, SavedHistoryRecord>();
    private readonly revisions = new Map<number, number>();
    private readonly dirty = new Set<number>();
    private readonly blocked = new Set<number>();
    private readonly names = ['expression', 'equation', 'diffequation', 'matrix', 'integrator', 'datetime', 'almanac'] as const;
    private readonly mark = (): void => {
        if (!this.started || this.restoring) return;
        const mode = labWire.exports().lab_workspace_mode();
        this.dirty.add(mode);
        try { localStorage.setItem('mars.history.mode.v1', String(mode)); } catch { /* Saving history still works. */ }
        this.schedule();
    };

    private id(): string {
        return [...crypto.getRandomValues(new Uint8Array(16))].map(value => value.toString(16).padStart(2, '0')).join('');
    }

    private warn(text: string): void {
        let notice = document.getElementById('historyPersistenceStatus');
        if (!notice) {
            notice = document.createElement('p');
            notice.id = 'historyPersistenceStatus';
            notice.setAttribute('role', 'status');
            document.getElementById('back')?.parentElement?.append(notice);
        }
        notice.textContent = text;
    }

    private async openCache(): Promise<void> {
        try {
            this.database = await new Promise<IDBDatabase>((resolve, reject) => {
                const request = indexedDB.open('mars.history.v1', 1);
                request.onupgradeneeded = () => request.result.createObjectStore('modes', {keyPath: 'mode'});
                request.onsuccess = () => resolve(request.result);
                request.onerror = () => reject(request.error);
                request.onblocked = () => reject(new Error('History cache blocked'));
            });
        } catch { this.warn('Offline history recovery is unavailable; server saves remain enabled.'); }
    }

    private async cache(record?: SavedHistoryRecord): Promise<SavedHistoryRecord[]> {
        if (!this.database) return [];
        return new Promise(resolve => {
            try {
                const transaction = this.database!.transaction('modes', record ? 'readwrite' : 'readonly');
                const store = transaction.objectStore('modes');
                const request = record ? store.put({...record, client: this.client}) : store.getAll();
                let values: SavedHistoryRecord[] = [];
                request.onsuccess = () => { values = record ? [record] : request.result as SavedHistoryRecord[]; };
                transaction.oncomplete = () => resolve(values);
                transaction.onerror = transaction.onabort = () => {
                    this.warn('Offline history recovery could not be saved; existing server history is unchanged.');
                    resolve([]);
                };
            } catch { resolve([]); }
        });
    }

    private async request(fields: Record<string, unknown>, keepalive = false): Promise<{
        response: Response; data: {records?: SavedHistoryRecord[]; epoch?: string; revision?: number}
    }> {
        const controller = new AbortController(), timer = setTimeout(() => controller.abort(), 8000);
        try {
            const response = await labHttp.fetch('/history', {method: 'POST', signal: controller.signal,
                keepalive, body: labWire.encode({client: this.client, session: this.session, ...fields})});
            return {response, data: await response.labData() as {
                records?: SavedHistoryRecord[]; epoch?: string; revision?: number
            }};
        } finally { clearTimeout(timer); }
    }

    /** Restore every mode before initial evaluation; no camera animation runs during startup. */
    public async start(): Promise<void> {
        if (this === worksheetHistoryPersistence) worksheetHistoryEditor.start();
        if (this.started || this.restoring) return;
        this.restoring = true;
        try {
            const stored = localStorage.getItem('mars.history.client.v1');
            this.client = stored && /^[a-f0-9]{32}$/.test(stored) ? stored : this.id();
            localStorage.setItem('mars.history.client.v1', this.client);
            this.session = this.id();
            await this.openCache();
            const cached = (await this.cache()).filter(record => record.client === this.client);
            let online = false, saved: SavedHistoryRecord[] = [];
            try {
                const result = await this.request({action: 'load'});
                if (!result.response.ok || !Array.isArray(result.data.records)) throw new Error('History unavailable');
                saved = result.data.records;
                online = true;
            } catch { this.warn('History server unavailable; using this browser’s saved recovery copy.'); }
            const byMode = new Map(saved.map(record => [record.mode, record]));
            for (const local of cached) {
                const server = byMode.get(local.mode);
                if (!online || (local.pending && ((server && local.epoch === server.epoch && local.revision === server.revision) ||
                    (!server && local.revision === 0)))) byMode.set(local.mode, local);
            }
            const initial = WorksheetWorkspace.mode.currentMode();
            const lastMode = Number(localStorage.getItem('mars.history.mode.v1'));
            const active = Number.isInteger(lastMode) && byMode.has(lastMode) ? this.names[lastMode] : initial;
            const states = new Map<number, WorksheetSnapshot>();
            for (const [mode, record] of byMode) {
                try {
                    const state = this.install(record);
                    if (record.pending) this.dirty.add(mode);
                    this.records.set(mode, record);
                    await this.cache(record);
                    states.set(mode, state);
                    WorksheetWorkspace.mode.setMode(this.names[mode], {force: true});
                    await WorksheetPersistence.navigation.restore(state);
                    if (state.mode === 'almanac') this.restoredGlobeCamera = state.globeCamera;
                } catch { this.warn('A damaged or unsupported history record was ignored.'); this.blocked.add(mode); }
            }
            WorksheetWorkspace.mode.setMode(active, {force: true});
            const current = states.get(this.names.indexOf(active));
            if (current) await WorksheetPersistence.navigation.restore(current);
            WorksheetWorkspace.presentation.updateHistoryButtons();
            for (let mode = 0; mode < 7; ++mode)
                this.revisions.set(mode, labWire.exports().lab_workspace_history_revision(mode));
            this.started = true;
            for (const event of ['input', 'change', 'pointerup', 'wheel', 'mars-almanac-observer-committed'])
                document.addEventListener(event, this.mark, {passive: true});
            window.addEventListener('pagehide', this.leave);
            document.addEventListener('visibilitychange', this.visibility);
            this.poll = window.setInterval(() => this.observe(), 500);
        } catch { this.warn('Persistent history needs browser storage; this session’s Back/Forward still works.'); }
        finally { this.restoring = false; }
        if (this.dirty.size) this.schedule();
    }

    /** Validate all snapshots before the native atomic replacement touches the live cursor. */
    public install(record: SavedHistoryRecord): WorksheetSnapshot {
        const {mode, history} = record;
        if (!Number.isInteger(mode) || mode < 0 || mode >= 7 || !Number.isSafeInteger(record.revision) ||
            record.revision < 0 || (record.revision > 0 && !/^[a-f0-9]{32}$/.test(record.epoch)) || history?.version !== 1 ||
            !Array.isArray(history.back) || !Array.isArray(history.forward) ||
            history.back.length > 64 || history.forward.length > 64 || labWire.encode(history).byteLength > 1048576)
            throw new Error('Invalid history record');
        const records = [history.current, ...history.back, ...history.forward];
        let current: WorksheetSnapshot | null = null, length = 0;
        for (const [index, bytes] of records.entries()) {
            if (!(bytes instanceof Uint8Array) || !bytes.length || bytes.length > 262144) throw new Error('Invalid snapshot size');
            const state = labDOM.call('lab_workspace_snapshot_validate', labWire.decode(bytes.buffer.slice(
                bytes.byteOffset, bytes.byteOffset + bytes.byteLength)), this.names) as WorksheetSnapshot;
            if (!state || state.mode !== this.names[mode]) throw new Error('Invalid snapshot mode');
            if (index === 0) current = state;
            length += bytes.length + 4;
        }
        const packet = new Uint8Array(length), sizes = new DataView(packet.buffer);
        let offset = 0;
        for (const bytes of records) {
            sizes.setUint32(offset, bytes.length, true);
            packet.set(bytes, offset + 4);
            offset += bytes.length + 4;
        }
        const staged = WorksheetWorkspace.labWorkspaceBuffer.stage(packet);
        if (!labWire.exports().lab_workspace_history_replace(mode, history.back.length, history.forward.length, staged))
            throw new Error('History memory limit reached');
        return current!;
    }

    /** Capture bounded opaque bytes without popping the live native stacks. */
    public capture(mode: number): SavedWorksheetHistory | null {
        const native = labWire.exports(), buffer = WorksheetWorkspace.labWorkspaceBuffer;
        const current = mode === native.lab_workspace_mode() ? labWire.encode({state: WorksheetPersistence.history.snapshot()}) :
            buffer.readBytes(native.lab_workspace_history_read(mode, 2, 0));
        if (!current.length) return null;
        if (current.length > 262144) throw new Error('Current worksheet exceeds the 256 KiB history snapshot limit.');
        const result: SavedWorksheetHistory = {version: 1, current, back: [], forward: []};
        let budget = 1048576 - current.length - 4096;
        // Reserve room fairly between both directions, retaining each nearest contiguous sequence.
        const counts = [native.lab_workspace_history_count(mode, 0), native.lab_workspace_history_count(mode, 1)];
        const open = [true, true];
        for (let depth = 0; depth < 64; ++depth) {
            for (let side = 0; side < 2; ++side) {
                if (!open[side] || depth >= counts[side]) continue;
                const bytes = buffer.readBytes(native.lab_workspace_history_read(mode, side, counts[side] - depth - 1));
                if (bytes.length > 262144 || bytes.length + 32 > budget) { open[side] = false; continue; }
                (side ? result.forward : result.back).push(bytes);
                budget -= bytes.length + 32;
            }
        }
        result.back.reverse(); result.forward.reverse();
        return result;
    }

    private observe(): void {
        if (!this.started || this.restoring) return;
        if (this === worksheetHistoryPersistence) worksheetHistoryEditor.refresh();
        for (let mode = 0; mode < 7; ++mode) {
            const revision = labWire.exports().lab_workspace_history_revision(mode);
            if (revision !== this.revisions.get(mode)) {
                this.revisions.set(mode, revision);
                this.dirty.add(mode);
            }
        }
        if (this.dirty.size) this.schedule();
    }

    /** Save a native timeline edit without adding a navigation entry. */
    public changed(mode: number): void {
        if (!this.started || this.restoring) return;
        this.dirty.add(mode);
        this.schedule();
    }

    private schedule(): void {
        if (!this.timer) this.timer = window.setTimeout(() => { this.timer = 0; void this.flush(); }, 500);
    }

    /** Save changed modes serially; an acknowledgement never discards edits made while it was in flight. */
    public async flush(keepalive = false): Promise<void> {
        if (!this.started || this.restoring) return;
        // A caller flushing newer edits must wait for the previous save, not silently
        // return while it still contains the older snapshot (notably before disposal).
        while (this.pendingSave) await this.pendingSave;
        if (!this.started || this.restoring) return;
        const pending = this.save(keepalive);
        this.pendingSave = pending;
        try { await pending; }
        finally { if (this.pendingSave === pending) this.pendingSave = null; }
    }

    /** Write one bounded batch; callers serialize batches through pendingSave. */
    private async save(keepalive: boolean): Promise<void> {
        if (this === worksheetHistoryPersistence && worksheetHistoryEditor.pending) return;
        for (const mode of [...this.dirty]) {
            this.dirty.delete(mode);
            if (this.blocked.has(mode)) continue;
            let history: SavedWorksheetHistory | null;
            try { history = this.capture(mode); }
            catch (error) { this.warn(String(error)); continue; }
            if (!history) continue;
            const previous = this.records.get(mode);
            const record: SavedHistoryRecord = {mode, epoch: previous?.epoch || '', revision: previous?.revision || 0,
                history, pending: true};
            this.records.set(mode, record);
            const cached = (await this.cache(record)).length > 0;
            try {
                const {response, data} = await this.request({action: 'save', ...record}, keepalive);
                if (response.status === 409) {
                    this.blocked.add(mode);
                    this.warn('History changed in another tab or was evicted. Reload before saving this mode again.');
                    continue;
                }
                if (!response.ok || !data.epoch || data.revision !== record.revision + 1) throw new Error('History save failed');
                record.epoch = data.epoch; record.revision = data.revision; record.pending = false;
                await this.cache(record);
            } catch {
                this.dirty.add(mode);
                this.warn(cached ? 'History is saved in this browser; server synchronisation will retry.' :
                    'History could not be saved locally or on the server; this session will retry.');
            }
        }
    }

    private readonly leave = (): void => { this.mark(); this.observe(); void this.flush(true); };
    private readonly visibility = (): void => { if (document.hidden) this.leave(); };

    /** Stop subscriptions without erasing saved history; used by application teardown and isolated browser fixtures. */
    public dispose(): void {
        if (this === worksheetHistoryPersistence) worksheetHistoryEditor.dispose();
        this.started = false;
        clearTimeout(this.timer); clearInterval(this.poll);
        this.timer = this.poll = 0;
        for (const event of ['input', 'change', 'pointerup', 'wheel', 'mars-almanac-observer-committed'])
            document.removeEventListener(event, this.mark);
        window.removeEventListener('pagehide', this.leave);
        document.removeEventListener('visibilitychange', this.visibility);
        this.database?.close(); this.database = null;
    }
}

const worksheetHistoryPersistence = new WorksheetHistoryPersistence();

/** Browser dialog lifetime and opaque snapshot transfer; native code owns timeline edits and row presentation. */
class WorksheetHistoryEditor {
    private lifetime: AbortController | null = null;
    private artwork: AbortController | null = null;
    private mode = -1;
    private revision = -1;
    private editing = false;
    private drag: WorksheetHistoryDrag | null = null;

    /** Hold persistence while a native timeline edit is still restoring its browser worksheet. */
    public get pending(): boolean { return this.editing || Boolean(this.drag?.active); }

    /** Attach detachable browser listeners after native workspace startup. */
    public start(): void {
        if (this.lifetime) return;
        this.lifetime = new AbortController();
        const options = {signal: this.lifetime.signal};
        document.getElementById('historyEdit')?.addEventListener('click', this.open, options);
        document.getElementById('historyEditorClose')?.addEventListener('click', this.close, options);
        document.getElementById('historyEditorEntries')?.addEventListener('click', this.click, options);
        document.getElementById('historyEditor')?.addEventListener('close', this.closed, options);
        const list = document.getElementById('historyEditorEntries');
        if (list) this.drag = new WorksheetHistoryDrag(list, this.lifetime.signal,
            () => !this.editing && this.mode === labWire.exports().lab_workspace_mode() &&
                this.revision === labWire.exports().lab_workspace_history_revision(this.mode) &&
                Boolean((document.getElementById('historyEditor') as HTMLDialogElement)?.open),
            (from, to) => { void this.editPosition(from, 'move', to); }, text => this.status(text),
            () => queueMicrotask(() => { void worksheetHistoryPersistence.flush(); }));
    }

    private readonly open = (): void => {
        try {
            WorksheetPersistence.history.commit();
            this.mode = labWire.exports().lab_workspace_mode();
            this.revision = -1;
            this.status('');
            this.render();
            (document.getElementById('historyEditor') as HTMLDialogElement).showModal();
            const current = document.getElementById('historyEditorEntries')?.querySelector<HTMLElement>('[aria-current] button');
            current?.focus({preventScroll: true});
            current?.scrollIntoView({block: 'nearest'});
        } catch (error) { WorksheetWorkspace.presentation.setStatus(String(error)); }
    };

    private readonly close = (): void => {
        this.closed();
        (document.getElementById('historyEditor') as HTMLDialogElement)?.close();
        document.getElementById('historyEdit')?.focus({preventScroll: true});
    };

    private readonly closed = (): void => {
        this.drag?.cancel();
        this.cancelArtwork();
    };

    private status(text: string): void {
        const status = document.getElementById('historyEditorStatus');
        if (status) status.textContent = text;
    }

    /** Reproject only when native history changes; no browser history copy is maintained. */
    public refresh(): void {
        const dialog = document.getElementById('historyEditor') as HTMLDialogElement | null;
        if (!dialog?.open) return;
        if (this.mode !== labWire.exports().lab_workspace_mode()) { this.close(); return; }
        if (this.revision !== labWire.exports().lab_workspace_history_revision(this.mode)) this.render();
    }

    private render(): void {
        const native = labWire.exports(), buffer = WorksheetWorkspace.labWorkspaceBuffer;
        const list = document.getElementById('historyEditorEntries');
        if (!list || this.mode < 0) return;
        this.drag?.cancel();
        this.cancelArtwork();
        const current = native.lab_workspace_history_current(this.mode);
        const count = current + 1 + native.lab_workspace_history_count(this.mode, 1);
        const scroll = list.scrollTop;
        list.replaceChildren();
        for (let position = 0; position < count; ++position) {
            const snapshot = buffer.readSnapshot(native.lab_workspace_history_item(this.mode, position));
            labDOM.call('lab_workspace_dom_history_row', list, snapshot, position, current, count);
        }
        this.revision = native.lab_workspace_history_revision(this.mode);
        list.scrollTop = scroll;
        this.artwork = new AbortController();
        void this.loadArtwork(list, this.artwork.signal);
    }

    private readonly cancelArtwork = (): void => {
        this.artwork?.abort();
        this.artwork = null;
    };

    /** Transfer country metadata asynchronously; native code owns labels, artwork validation and DOM projection. */
    private async loadArtwork(list: HTMLElement, signal: AbortSignal): Promise<void> {
        const buttons = [...list.querySelectorAll<HTMLButtonElement>('[data-history-location]')];
        let next = 0;
        const load = async (): Promise<void> => {
            while (!signal.aborted && next < buttons.length) {
                const button = buttons[next++], code = button.dataset.historyCountryCode;
                if (!code && (!button.dataset.historyLatitude || !button.dataset.historyLongitude)) continue;
                const request = new AbortController(), cancel = (): void => request.abort();
                signal.addEventListener('abort', cancel, {once: true});
                const timer = window.setTimeout(cancel, 5000);
                try {
                    let metadata: unknown;
                    if (code) metadata = await WorksheetResults.tooltip.countryMetadata(code, request.signal);
                    else {
                        const response = await labHttp.fetch('/almanac-country', {method: 'POST', signal: request.signal,
                            body: labWire.encode({lat: button.dataset.historyLatitude, lon: button.dataset.historyLongitude})});
                        if (!response.ok) continue;
                        metadata = await response.labData();
                    }
                    if (!signal.aborted && !request.signal.aborted && button.isConnected)
                        labDOM.call('lab_workspace_dom_history_country', button, metadata);
                } catch { /* Saved names remain usable when country artwork is unavailable. */ }
                finally {
                    window.clearTimeout(timer);
                    signal.removeEventListener('abort', cancel);
                }
            }
        };
        await Promise.all([load(), load(), load()]);
    }

    private readonly click = (event: Event): void => { void this.edit(event); };

    private async edit(event: Event): Promise<void> {
        const button = event.target instanceof Element ? event.target.closest<HTMLButtonElement>('button[data-history-action]') : null;
        const row = button?.closest<HTMLElement>('[data-history-position]');
        if (!button || button.disabled || !row || this.drag?.active) return;
        await this.editPosition(Number(row.dataset.historyPosition), button.dataset.historyAction || '');
    }

    /** Both pointer drops and keyboard buttons submit one native edit and share persistence and error handling. */
    private async editPosition(position: number, action: string, target?: number): Promise<void> {
        if (this.editing || this.mode !== labWire.exports().lab_workspace_mode()) return;
        if (this.revision !== labWire.exports().lab_workspace_history_revision(this.mode)) {
            this.render();
            this.status('History changed; choose the entry again.');
            return;
        }
        this.editing = true;
        try {
            const native = labWire.exports();
            if (action === 'open') {
                this.close();
                await WorksheetPersistence.navigation.select(position);
                return;
            }
            if (action === 'remove' && position === native.lab_workspace_history_current(this.mode)) {
                await WorksheetPersistence.navigation.select(position, true);
            } else {
                await WorksheetRequests.labRequests.runUI('evaluate', WorksheetWorkspace.mode.currentMode(), () => {
                    if (action === 'remove') {
                        if (!native.lab_workspace_history_remove(this.mode, position))
                            throw new Error('Could not remove this history entry');
                    } else {
                        const destination = target ?? position + (action === 'earlier' ? -1 : 1);
                        if (!native.lab_workspace_history_move(this.mode, position, destination))
                            throw new Error('This move would exceed the history limit; remove an older entry first');
                    }
                });
            }
            WorksheetWorkspace.presentation.updateHistoryButtons();
            worksheetHistoryPersistence.changed(this.mode);
            this.render();
            this.status(action === 'remove' ? 'Entry removed.' : 'Entry moved.');
            const movedPosition = target ?? position + (action === 'earlier' ? -1 : action === 'later' ? 1 : 0);
            const focus = Math.min(movedPosition,
                native.lab_workspace_history_current(this.mode) + native.lab_workspace_history_count(this.mode, 1));
            const row = document.querySelector<HTMLElement>(`#historyEditorEntries [data-history-position="${focus}"]`);
            const control = action === 'earlier' || action === 'later' ? action : 'open';
            const selected = row?.querySelector<HTMLButtonElement>(`[data-history-action="${control}"]:not(:disabled)`) ??
                row?.querySelector<HTMLButtonElement>('[data-history-action="open"]');
            selected?.focus({preventScroll: true});
            row?.scrollIntoView({block: 'nearest'});
        } catch (error) { this.status(String(error)); }
        finally {
            this.editing = false;
            worksheetHistoryPersistence.changed(this.mode);
        }
    }

    /** Release browser listeners and close an open editor without modifying saved history. */
    public dispose(): void {
        this.closed();
        this.drag = null;
        this.lifetime?.abort();
        this.lifetime = null;
        (document.getElementById('historyEditor') as HTMLDialogElement | null)?.close();
    }
}

const worksheetHistoryEditor = new WorksheetHistoryEditor();
