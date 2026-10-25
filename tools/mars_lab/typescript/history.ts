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
        for (let mode = 0; mode < 7; ++mode) {
            const revision = labWire.exports().lab_workspace_history_revision(mode);
            if (revision !== this.revisions.get(mode)) {
                this.revisions.set(mode, revision);
                this.dirty.add(mode);
            }
        }
        if (this.dirty.size) this.schedule();
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
