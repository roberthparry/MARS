/**
 * @file state.ts
 * @brief Browser persistence resources and native worksheet-state coordination.
 *
 * WorksheetPersistence owns local fallback storage, pending save timers and mode-bound history cursors.
 * Native continuations select save records, normalisation, recovery and guarded projection; this module
 * supplies browser I/O through WorksheetRequests, WorksheetWorkspace, WorksheetBindings,
 * WorksheetLocations and WorksheetResults. The DOM bridge and wire codec retain native payload ownership.
 * Loaded as a classic script, the module exposes only its namespace and resolves public capabilities
 * at invocation time so asynchronous work and test replacements use the current implementation.
 */

namespace WorksheetPersistence {
    /** @brief Stateless callbacks; live browser dependencies are resolved when invoked. */
    const stateCallbacks = {
        /** @brief Accept restoration without an external ownership constraint. */
        current(): boolean { return true; },

        /** @brief Read an opaque saved record lazily. */
        readRecord(data: WorksheetStateRecord, key: string): unknown { return data[key]; },

        /** @brief Read local storage lazily, preserving storage failures. */
        readLocal(key: string): string | null { return localStorage.getItem(key); },

        /** @brief Resolve the current owner of a captured request. */
        requestCurrent(request: WorksheetRequest): boolean {
            return WorksheetRequests.labRequests.current(request);
        },

        /** @brief Check the caller's guard before comparing captured editor ownership. */
        editorCurrent(guard: WorksheetGuard, mode: WorksheetMode, context: number): boolean {
            return guard() && WorksheetWorkspace.mode.currentMode() === mode &&
                WorksheetRequests.labRequests.context() === context;
        },

        /** @brief Check both captured selection ownership tokens. */
        selectionCurrent(context: number, main: number): boolean {
            return context === WorksheetRequests.labRequests.context() &&
                main === WorksheetRequests.labRequests.latestMain();
        },

        /** @brief Continue native clearing inside its owned evaluation request. */
        clearOwned(request: WorksheetRequest): Promise<unknown> {
            return WorksheetRequests.runtime.continue(72, {stage: 3, request});
        },

        /** @brief Continue state projection after location refresh through the supplied runtime. */
        refreshLocation(runtime: WorksheetStateRuntimePort): void { runtime.sync(13, {}); },

        /** @brief Publish a timer's native-selected save through the supplied runtime. */
        publishSave(runtime: WorksheetStateRuntimePort, save: WorksheetSavePlan): void {
            runtime.sync(1, {save});
        }
    };

    /** @brief Own pending browser saves; native plans retain delay and stale-token policy. */
    export class WorksheetSaveScheduler {
        /** @brief Timer handles owned by this scheduler, indexed by native worksheet mode. */
        readonly timers = new Map<number, number>();

        /** @brief Inject browser timers and the native-selected publication operation. */
        constructor(private readonly clock: WorksheetSaveTimerPort,
                    private readonly publish: WorksheetStatePublisher) {
            this.cancel = this.cancel.bind(this);
            this.forget = this.forget.bind(this);
            this.schedule = this.schedule.bind(this);
            this.dispose = this.dispose.bind(this);
        }

        /** @brief Cancel and release the pending timer for one worksheet. */
        cancel(index: number): void {
            this.clock.cancel(this.timers.get(index));
            this.timers.delete(index);
        }

        /** @brief Forget a completed timer without cancelling a newer native-selected operation. */
        forget(index: number): boolean {
            return this.timers.delete(index);
        }

        /** @brief Schedule publication; native code decides whether an earlier timer must be cancelled. */
        schedule(save: WorksheetSavePlan): Map<number, number> {
            return this.timers.set(save.index, this.clock.schedule(this.publishSave.bind(this, save), save.delay));
        }

        /** @brief Release all pending browser timers when the owner is disposed. */
        dispose(): void {
            for (const handle of this.timers.values()) this.clock.cancel(handle);
            this.timers.clear();
        }

        private publishSave(save: WorksheetSavePlan): void {
            this.publish(save);
        }
    }

    /** @brief Own local fallback storage, publication and native-directed worksheet saves. */
    export class WorksheetPersistenceController {
        /** @brief Inject I/O, live inputs, native execution and an exclusively owned scheduler. */
        constructor(private readonly io: WorksheetPersistenceIO,
                    private readonly inputs: WorksheetPersistenceInputs,
                    private readonly runtime: WorksheetStateRuntimePort,
                    /** @brief Scheduler whose timers are released with this owner. */
                    readonly scheduler: WorksheetSaveScheduler) {
            this.writeLocal = this.writeLocal.bind(this);
            this.readLocal = this.readLocal.bind(this);
            this.publish = this.publish.bind(this);
            this.savePrecision = this.savePrecision.bind(this);
            this.saveMode = this.saveMode.bind(this);
            this.saveWorksheet = this.saveWorksheet.bind(this);
            this.saveCalendar = this.saveCalendar.bind(this);
            this.dispose = this.dispose.bind(this);
        }

        /** @brief Encode a fallback record, propagating codec and storage errors. */
        writeLocal(key: string, state: unknown): void {
            this.io.write(`${key}.protobuf`, this.io.encodeStored(state));
        }

        /** @brief Decode a bounded fallback; missing, corrupt or inaccessible storage yields null. */
        readLocal(key: string): unknown {
            try {
                const text = this.io.read(`${key}.protobuf`);
                return text ? this.io.decodeStored(text) : null;
            } catch {
                return null;
            }
        }

        /** @brief Publish a precision-safe snapshot; asynchronous publication failure is non-fatal. */
        publish(patch: WorksheetStateRecord, options: WorksheetSaveOptions = {}): void {
            const payload = {...patch}, precision = this.inputs.precision();
            if (payload.precision_bits === precision) payload.precision_bits = {...precision};
            void this.io.post(this.io.encode(payload), !!options.keepalive).catch(this.publicationFailed);
        }

        /** @brief Save the live native-backed precision view through the publication seam. */
        savePrecision(): void {
            return this.publish({precision_bits: this.inputs.precision()});
        }

        /** @brief Persist the selected worksheet through native mode-save policy. */
        saveMode(mode: WorksheetMode = this.inputs.mode()): void {
            this.runtime.sync(3, {mode});
        }

        /** @brief Capture authored source at invocation time and execute native save policy. */
        saveWorksheet(mode: WorksheetMode, text = this.inputs.source(),
                      options: WorksheetSaveOptions = {}): void {
            this.runtime.sync(0, {index: this.inputs.modeIndex(mode), text, options});
        }

        /** @brief Capture and persist a calendar worksheet through native policy. */
        saveCalendar(mode: WorksheetMode): void {
            this.runtime.sync(2, {mode});
        }

        /** @brief Release owned timer resources without changing persisted records. */
        dispose(): void {
            return this.scheduler.dispose();
        }

        private publicationFailed(): void {
            // Persistence is helpful, not essential.
        }
    }

    /** @brief Execute restoration with independent native frames and no shared pending-operation bag. */
    export class StateRestorationController {
        /** @brief Inject the typed native restoration executor. */
        constructor(private readonly runtime: WorksheetStateRuntimePort) {
            this.restoreEditors = this.restoreEditors.bind(this);
            this.restoreControls = this.restoreControls.bind(this);
            this.applySaved = this.applySaved.bind(this);
            this.recoverLocalEditors = this.recoverLocalEditors.bind(this);
            this.load = this.load.bind(this);
        }

        /** @brief Restore sources with lazy reads and native-selected field order. */
        async restoreEditors(read: WorksheetStateReader, local = false): Promise<void> {
            return this.runtime.restore(16, {read, local});
        }

        /** @brief Restore controls while preserving native await boundaries. */
        async restoreControls(read: WorksheetStateReader, local = false): Promise<void> {
            return this.runtime.restore(17, {read, local});
        }

        /** @brief Apply an opaque server payload using native restoration policy. */
        async applySaved(data: unknown): Promise<void> {
            return this.runtime.restore(18, {data});
        }

        /** @brief Recover newer local sources according to native timestamp policy. */
        async recoverLocalEditors(data: unknown): Promise<void> {
            return this.runtime.restore(19, {data});
        }

        /** @brief Load server state and execute native-selected fallback or recovery. */
        async load(): Promise<void> {
            return this.runtime.restore(20, {});
        }
    }

    /** @brief Bind a mode and history direction without duplicating native history contents. */
    export class WorksheetHistoryCursor implements WorksheetStateHistoryCursor {
        /** @brief Inject the native history and staging operations used by this cursor. */
        constructor(private readonly index: number, private readonly store: number,
                    private readonly port: WorksheetHistoryCursorPort) {
            this.push = this.push.bind(this);
            this.pop = this.pop.bind(this);
        }

        /** @brief Read the current native count without caching it. */
        get length(): number { return this.port.count(this.index, this.store); }

        /** @brief Retain a snapshot; detached callbacks keep their mode and direction. */
        push(snapshot: WorksheetSnapshot): number {
            // Flags 0: plain stack push, without deduplication or forward-history clearing.
            const count = this.port.push(this.index, this.store, this.port.stage(snapshot), Number(!!snapshot?.text), 0);
            if (count < 0) throw new Error('Could not retain worksheet history');
            return count;
        }

        /** @brief Remove and decode the native top entry, returning null when empty. */
        pop(): WorksheetSnapshot | null {
            return this.port.read(this.port.pop(this.index, this.store));
        }
    }

    /** @brief Adapt native history transactions and snapshot buffers without browser-side history policy. */
    export class NativeHistoryStore {
        /** @brief Inject native operations and the typed snapshot executor. */
        constructor(private readonly port: WorksheetHistoryPort, private readonly runtime: WorksheetStateRuntimePort) {
            this.cursor = this.cursor.bind(this);
            this.navigate = this.navigate.bind(this);
            this.backwardCount = this.backwardCount.bind(this);
            this.forwardCount = this.forwardCount.bind(this);
            this.snapshot = this.snapshot.bind(this);
            this.equals = this.equals.bind(this);
            this.previous = this.previous.bind(this);
            this.commit = this.commit.bind(this);
            this.clearForward = this.clearForward.bind(this);
        }

        /** @brief Create an independently bound cursor for a native history direction. */
        cursor(store: number, mode: WorksheetMode = this.port.mode()): WorksheetStateHistoryCursor {
            return new WorksheetHistoryCursor(this.port.modeIndex(mode), store, this.port);
        }

        /** @brief Atomically navigate using the current snapshot and native selected mode. */
        navigate(direction: number): WorksheetSnapshot | null {
            const current = this.snapshot();
            const length = this.port.stage(current);
            return this.port.read(this.port.navigate(this.port.currentIndex(), direction, length, Number(!!current.text)));
        }

        /** @brief Read the selected worksheet's native backward count. */
        backwardCount(): number {
            return this.cursor(this.port.backwardStore()).length;
        }

        /** @brief Read the selected worksheet's native forward count. */
        forwardCount(): number {
            return this.cursor(this.port.forwardStore()).length;
        }

        /** @brief Capture a native record with an optional authored-source override. */
        snapshot(mode: WorksheetMode = this.port.mode(), textOverride: string | null = null): WorksheetSnapshot {
            return this.runtime.sync(9, {index: this.port.modeIndex(mode), textOverride});
        }

        /** @brief Compare snapshots in separate native staging slots. */
        equals(left: WorksheetSnapshot, right: WorksheetSnapshot): boolean {
            const leftLength = this.port.stage(left, 0), rightLength = this.port.stage(right, 1);
            return !!this.port.equal(leftLength, rightLength);
        }

        /** @brief Retrieve a native predecessor, retaining the current-mode fallback for a null snapshot. */
        previous(nextState: WorksheetSnapshot | null): WorksheetSnapshot | null {
            const index = this.port.modeIndex(nextState && nextState.mode || this.port.mode());
            return this.port.read(this.port.previous(index, this.port.stage(nextState)));
        }

        /** @brief Commit the captured state and propagate native rejection. */
        commit(mode: WorksheetMode = this.port.mode(), textOverride: string | null = null): void {
            const index = this.port.modeIndex(mode), length = this.port.stage(this.snapshot(mode, textOverride));
            if (!this.port.commit(index, length)) throw new Error('Could not commit worksheet state');
        }

        /** @brief Clear native forward history for the selected worksheet. */
        clearForward(mode: WorksheetMode = this.port.mode()): void {
            return this.port.clear(this.port.modeIndex(mode), 1);
        }
    }

    /** @brief Coordinate history restoration with request-scoped navigation ownership. */
    export class HistoryNavigationController {
        /** @brief Inject native restoration and the narrow request-ownership capability. */
        constructor(private readonly runtime: WorksheetStateRuntimePort,
                    private readonly requests: WorksheetHistoryNavigationPort) {
            this.restore = this.restore.bind(this);
            this.navigate = this.navigate.bind(this);
        }

        /** @brief Restore while the supplied ownership guard remains current. */
        async restore(state: WorksheetSnapshot | null, isCurrent: WorksheetGuard = stateCallbacks.current): Promise<boolean> {
            return this.runtime.restore(21, {state, isCurrent});
        }

        /** @brief Acquire evaluation ownership before executing native history navigation. */
        async navigate(direction: number): Promise<unknown> {
            return this.requests.run(this.requests.mode(), this.navigateRequest.bind(this, direction));
        }

        private navigateRequest(direction: number, request: WorksheetRequest): Promise<void> {
            return this.runtime.restore(22, {direction, request});
        }
    }

    /**
     * @brief Native state executor with checked operation/frame pairs and detached callbacks.
     *
     * Sync kinds (lab_flow_state_sync.h); frame names omit the WorksheetState prefix:
     *   0 save worksheet: SaveFrame       |  1 publish save: PublishFrame
     *   2 save calendar: ModeFrame        |  3 save mode: ModeFrame
     *   4 restore editor: IndexFrame      |  5 restore result: ModeFrame
     *   6 select mode: SelectionFrame     |  7 apply mode: ModeFrame
     *   8 keyboard: EmptyFrame            |  9 snapshot: SnapshotFrame
     *  10 push history: EntryFrame        | 11 controls: ControlsFrame
     *  12 flush saves: EmptyFrame         | 13 location completion: EmptyFrame
     *  14 integrator result: DataFrame    | 15 capture save: IndexFrame
     *  16 bounds availability: EmptyFrame
     *
     * Restore kinds (asynchronous continuation operations, a separate number space):
     *  16 editors: ReadFrame              | 17 controls: ReadFrame
     *  18 apply saved: DataFrame          | 19 recover local: DataFrame
     *  20 load: EmptyFrame                | 21 history: HistoryFrame
     *  22 navigate: NavigateFrame
     *
     * Keep the overloads: they check the frame and result for each operation at call sites.
     */
    class StateRuntime implements WorksheetStateRuntimePort {
        /** @brief Bind the execution seams without resolving bootstrap dependencies. */
        constructor() {
            this.sync = this.sync.bind(this);
            this.restore = this.restore.bind(this);
        }

        /** @brief Save authored worksheet source. */
        sync(kind: 0, frame: WorksheetStateSaveFrame): void;

        /** @brief Publish a native-selected save. */
        sync(kind: 1, frame: WorksheetStatePublishFrame): void;

        /** @brief Apply the selected mode operation. */
        sync(kind: 2 | 3 | 5 | 7, frame: WorksheetStateModeFrame): void;

        /** @brief Apply the selected indexed operation. */
        sync(kind: 4 | 15, frame: WorksheetStateIndexFrame): void;

        /** @brief Select a worksheet mode and report change. */
        sync(kind: 6, frame: WorksheetStateSelectionFrame): boolean;

        /** @brief Apply a native operation without initial arguments. */
        sync(kind: 8 | 12 | 13, frame: WorksheetStateEmptyFrame): void;

        /** @brief Capture the current native snapshot. */
        sync(kind: 9, frame: WorksheetStateSnapshotFrame): WorksheetSnapshot;

        /** @brief Apply a native history entry. */
        sync(kind: 10, frame: WorksheetStateEntryFrame): void;

        /** @brief Project supplied controls. */
        sync(kind: 11, frame: WorksheetStateControlsFrame): void;

        /** @brief Apply an opaque native payload. */
        sync(kind: 14, frame: WorksheetStateDataFrame): void;

        /** @brief Report whether integrator bounds are needed. */
        sync(kind: 16, frame: WorksheetStateEmptyFrame): boolean;

        /** @brief Execute native synchronous policy without intercepting failures. */
        sync(kind: number, frame: object): void | boolean | WorksheetSnapshot {
            return labDOM.sync('lab_state_sync', kind, frame, WorksheetRequests.runtime.getRegistry()) as void | boolean | WorksheetSnapshot;
        }

        /** @brief Restore fields through the supplied reader. */
        restore(kind: 16 | 17, frame: WorksheetStateReadFrame): Promise<void>;

        /** @brief Apply or recover an opaque payload. */
        restore(kind: 18 | 19, frame: WorksheetStateDataFrame): Promise<void>;

        /** @brief Load saved state with native fallback ordering. */
        restore(kind: 20, frame: WorksheetStateEmptyFrame): Promise<void>;

        /** @brief Restore history while its guard remains current. */
        restore(kind: 21, frame: WorksheetStateHistoryFrame): Promise<boolean>;

        /** @brief Navigate under request ownership. */
        restore(kind: 22, frame: WorksheetStateNavigateFrame): Promise<void>;

        /** @brief Retain the native continuation's asynchronous completion contract. */
        restore(kind: number, frame: object): Promise<void | boolean> {
            return WorksheetRequests.runtime.continue(kind, frame) as Promise<void | boolean>;
        }
    }

    /** @brief Typed native-state runtime ports shared by persistence, restoration and history owners. */
    export const stateRuntime: WorksheetStateRuntimePort = new StateRuntime();

    /** @brief Native validation and persistence schema gateway; configuration is read only when invoked. */
    export const statePolicy: WorksheetStatePolicyPort = {
        /** @brief Validate an interval budget through native configuration. */
        intervalCap(value) {
            return labDOM.call('lab_storage_intervals', [value], labApplication.configuration) as number;
        },

        /** @brief Validate a matrix operation through native policy. */
        matrixOperation(value) {
            return labDOM.call('lab_storage_operation', [value]) as string;
        },

        /** @brief Resolve the current worksheet mode. */
        mode(value) {
            return WorksheetWorkspace.WORKSPACE_MODE_NAMES[WorksheetWorkspace.mode.workspaceModeId(value)];
        },

        /** @brief Read the native persistence schema. */
        schema(index) {
            return labDOM.call('lab_persist_schema', index) as WorksheetStateSchemaField[];
        }
    };

    /** @brief Application persistence owner with live browser adapters and independent timer ownership. */
    export const persistence: WorksheetPersistenceController = new WorksheetPersistenceController(
        {
            /** @brief Read local encoded state. */
            read(key) {
                return localStorage.getItem(key);
            },

            /** @brief Write encoded local state, propagating storage failures. */
            write(key, value) {
                return localStorage.setItem(key, value);
            },

            /** @brief Encode a native-owned payload. */
            encode(value) {
                return labWire.encode(value);
            },

            /** @brief Encode fallback records using the native base64/Protobuf codec. */
            encodeStored(value) {
                return labWire.encodeStored(value);
            },

            /** @brief Validate and decode stored text without interpreting its schema. */
            decodeStored(text) {
                return labWire.decodeStored(text);
            },

            /** @brief Publish encoded state with the requested unload policy. */
            post(bytes, keepalive) {
                return labHttp.fetch('/state', {method: 'POST', body: bytes, keepalive});
            }
        },
        {
            /** @brief Resolve the current worksheet mode. */
            mode() {
                return WorksheetWorkspace.mode.currentMode();
            },

            /** @brief Resolve a worksheet mode to its native index. */
            modeIndex(mode) {
                return WorksheetWorkspace.mode.workspaceModeId(mode);
            },

            /** @brief Read authored source at invocation time. */
            source() {
                return WorksheetBindings.editor.currentExpressionText() || WorksheetContext.expr.value || '';
            },

            /** @brief Read the native-backed precision view. */
            precision() {
                return WorksheetContext.modePrecisionBits;
            }
        },
        stateRuntime,
        new WorksheetSaveScheduler(
            {
                /** @brief Schedule a browser-owned save callback. */
                schedule(callback, delay) { return setTimeout(callback, delay); },
                /** @brief Cancel the supplied browser timer. */
                cancel(handle) { clearTimeout(handle); }
            },
            stateCallbacks.publishSave.bind(null, stateRuntime)));

    /** @brief Application restoration owner, with independent frames for concurrent calls. */
    export const restoration: StateRestorationController = new StateRestorationController(stateRuntime);

    /** @brief Lazy native history and snapshot-buffer operations. */
    const historyPort: WorksheetHistoryPort = {
        /** @brief Read a native history count. */
        count(index, store) {
            return labWire.exports().lab_workspace_history_count(index, store);
        },

        /** @brief Retain staged bytes in native history. */
        push(index, store, length, text, flags) {
            return labWire.exports().lab_workspace_history_push(index, store, length, text, flags);
        },

        /** @brief Remove the native top history entry. */
        pop(index, store) {
            return labWire.exports().lab_workspace_history_pop(index, store);
        },

        /** @brief Navigate native history with staged current state. */
        navigate(index, direction, length, text) {
            return labWire.exports().lab_workspace_navigate(index, direction, length, text);
        },

        /** @brief Compare native staging slots. */
        equal(left, right) {
            return labWire.exports().lab_workspace_equal(left, right);
        },

        /** @brief Select the native predecessor. */
        previous(index, length) {
            return labWire.exports().lab_workspace_previous(index, length);
        },

        /** @brief Commit native staged state. */
        commit(index, length) {
            return labWire.exports().lab_workspace_commit(index, length);
        },

        /** @brief Clear a native history direction. */
        clear(index, store) { labWire.exports().lab_workspace_history_clear(index, store); },

        /** @brief Stage a snapshot in native memory. */
        stage(snapshot, slot) {
            return WorksheetWorkspace.labWorkspaceBuffer.stageSnapshot(snapshot, slot);
        },

        /** @brief Read local encoded state. */
        read(length) {
            return WorksheetWorkspace.labWorkspaceBuffer.readSnapshot(length);
        },

        /** @brief Resolve the current worksheet mode. */
        mode() {
            return WorksheetWorkspace.mode.currentMode();
        },

        /** @brief Resolve a worksheet mode to its native index. */
        modeIndex(mode) {
            return WorksheetWorkspace.mode.workspaceModeId(mode);
        },

        /** @brief Read the current native mode index. */
        currentIndex() {
            return labWire.exports().lab_workspace_mode();
        },

        /** @brief Read the backward-history store identifier. */
        backwardStore() {
            return WorksheetContext.expressionHistory;
        },

        /** @brief Read the forward-history store identifier. */
        forwardStore() {
            return WorksheetContext.forwardHistory;
        }
    };

    /** @brief Application history adapter; native memory remains the sole history store. */
    export const history: NativeHistoryStore = new NativeHistoryStore(historyPort, stateRuntime);

    /** @brief Application navigation owner sharing the request manager's native ownership protocol. */
    export const navigation: HistoryNavigationController = new HistoryNavigationController(stateRuntime, {
        /** @brief Resolve the current worksheet mode. */
        mode() {
            return WorksheetWorkspace.mode.currentMode();
        },

        /** @brief Acquire request ownership for history navigation. */
        run(mode, action) {
            return WorksheetRequests.labRequests.runUI('evaluate', mode, action);
        }
    });

    /** @brief Native-directed publication, local records and pending-save resources. */
    export const persistenceCapabilities = Object.freeze({
        // --- Publication and worksheet saves ---
        /** @brief Publish a state patch with a precision snapshot when required. */
        stateSave(patch: WorksheetStateRecord, options: WorksheetSaveOptions = {}) {
            return persistence.publish(patch, options);
        },

        /** @brief Save editor text for the supplied native mode index. */
        stateSaveEditor(index: number) {
            return stateRuntime.sync(15, {index});
        },

        /** @brief Save the editor identified by its native mode index. */
        stateSaveNamedEditor(index: number) {
            return persistence.saveWorksheet(WorksheetWorkspace.WORKSPACE_MODE_NAMES[index]);
        },

        /** @brief Save authored worksheet text through native persistence policy. */
        stateSaveWorksheet(mode: WorksheetMode, text?: string, options: WorksheetSaveOptions = {}) {
            return persistence.saveWorksheet(mode, text, options);
        },

        /** @brief Save current worksheet text with unload-safe delivery. */
        stateFlushSave(mode: WorksheetMode) {
            return persistence.saveWorksheet(mode, undefined, {keepalive: true});
        },

        /** @brief Persist the currently selected worksheet mode. */
        stateSaveMode() {
            return persistence.saveMode(WorksheetWorkspace.mode.currentMode());
        },

        /** @brief Publish the current native-backed precision settings. */
        stateSavePrecision() {
            return persistence.savePrecision();
        },

        // --- Records and calendar storage ---
        /** @brief Build persistence records using native schema and precision settings. */
        stateRecords(index: number, values: unknown) {
            return labDOM.call('lab_persist_records', index, values, WorksheetContext.modePrecisionBits);
        },

        /** @brief Write native-selected records to local storage. */
        stateLocalRecords(records: WorksheetStateRecord) {
            for (const [key, value] of Object.entries(records)) localStorage.setItem(key, String(value));
        },

        /** @brief Write the selected mode name to local storage. */
        stateWriteMode(mode: WorksheetMode) {
            return localStorage.setItem(WorksheetContext.LAB_MODE_STORAGE_KEY, mode);
        },

        /** @brief Save calendar controls for the supplied native mode index. */
        stateSaveCalendar(index: number) {
            return persistence.saveCalendar(WorksheetWorkspace.WORKSPACE_MODE_NAMES[index]);
        },

        /** @brief Encode and write a calendar fallback record. */
        stateWriteCalendar(key: string, state: unknown) {
            return persistence.writeLocal(key, state);
        },

        /** @brief Build a native calendar patch with current precision settings. */
        stateCalendarPatch(mode: WorksheetMode, state: unknown) {
            return labDOM.call('lab_persist_calendar', WorksheetWorkspace.mode.workspaceModeId(mode), state, WorksheetContext.modePrecisionBits);
        },

        /** @brief Persist the current DateTime controls. */
        stateSaveDatetime() {
            return persistence.saveCalendar('datetime');
        },

        /** @brief Persist the current almanac controls. */
        stateSaveAlmanac() {
            return persistence.saveCalendar('almanac');
        },

        // --- Pending save timers ---
        /** @brief Cancel and remove a mode's pending save timer. */
        stateCancelSaveTimer(index: number) {
            return persistence.scheduler.cancel(index);
        },

        /** @brief Remove a completed timer from the scheduler without cancelling it. */
        stateForgetSaveTimer(index: number) {
            return persistence.scheduler.forget(index);
        },

        /** @brief Schedule publication of the supplied native save plan. */
        stateScheduleSave(save: WorksheetSavePlan) {
            return persistence.scheduler.schedule(save);
        },

        /** @brief Execute native publication policy for a save plan. */
        statePublishSave(save: WorksheetSavePlan) {
            return stateRuntime.sync(1, {save});
        }
    }) satisfies WorksheetPersistenceCapabilities;

    /** @brief Native-selected recovery and restoration adapters with live external capabilities. */
    export const restorationCapabilities = Object.freeze({
        // --- Lazy readers, metadata and guards ---
        /** @brief Read a saved field through the supplied lazy reader. */
        stateRead(read: WorksheetStateReader, key: string) {
            return read(key);
        },

        /** @brief Read the current epoch timestamp in milliseconds. */
        stateNow() {
            return Date.now();
        },

        /** @brief Invoke the supplied restoration ownership guard. */
        stateGuard(guard: WorksheetGuard) {
            return guard();
        },

        /** @brief Request native editor metadata for authored text. */
        statePrepare(text: string) {
            return WorksheetRequests.presentation.prepareLabEditor(text);
        },

        /** @brief Request expression text with natively ordered constants. */
        stateCanonical(text: string) {
            return WorksheetBindings.editor.expressionWithSortedConstants(text);
        },

        /** @brief Normalise a saved control value using native storage policy. */
        stateControl(field: WorksheetStorageField, value: unknown, local: boolean) {
            return labDOM.call('lab_storage_control', field, [value], local, labApplication.configuration);
        },

        /** @brief Restore integrator bounds under the supplied ownership guard. */
        stateBounds(text: string, isCurrent?: WorksheetGuard) {
            return WorksheetWorkspace.integrator.restoreIntegratorBoundsText(text, isCurrent);
        },

        // --- Server restoration and local recovery ---
        /** @brief Fetch the server's saved worksheet state. */
        stateFetch() {
            return labHttp.fetch('/state');
        },

        /** @brief Decode the saved-state response without interpreting its schema. */
        stateDecode(response: WorksheetResponse) {
            return response.labData();
        },

        /** @brief Apply a server payload through native restoration policy. */
        stateApplySaved(data: unknown) {
            return restoration.applySaved(data);
        },

        /** @brief Restore editor fields through lazy reads of a server record. */
        stateServerEditors(data: WorksheetStateRecord) {
            return restoration.restoreEditors(stateCallbacks.readRecord.bind(null, data));
        },

        /** @brief Restore control fields through lazy reads of a server record. */
        stateServerControls(data: WorksheetStateRecord) {
            return restoration.restoreControls(stateCallbacks.readRecord.bind(null, data));
        },

        /** @brief Recover newer local editor text using native timestamp policy. */
        stateRecover(data: unknown) {
            return restoration.recoverLocalEditors(data);
        },

        /** @brief Build a native recovery plan from server data and local editor records. */
        stateRecovery(index: number, data: unknown) {
            const fields = statePolicy.schema(index);
            return labDOM.call(
                'lab_storage_recovery', index, data,
                [localStorage.getItem(fields[0].local), localStorage.getItem(fields[1].local)], labApplication.configuration);
        },

        /** @brief Build the native recovered record from accepted editor text and timestamp. */
        stateRecovered(index: number, recovery: unknown, text: string, now: number) {
            return labDOM.call('lab_storage_recovered', index, recovery, [text], now);
        },

        /** @brief Restore editor fields through lazy local-storage reads. */
        stateLocalEditors() {
            return restoration.restoreEditors(stateCallbacks.readLocal, true);
        },

        /** @brief Restore control fields through lazy local-storage reads. */
        stateLocalControls() {
            return restoration.restoreControls(stateCallbacks.readLocal, true);
        },

        /** @brief Decode a bounded local calendar fallback record. */
        stateLocalCalendar(key: string) {
            return persistence.readLocal(key);
        },

        /** @brief Read the locally retained worksheet mode. */
        stateLocalMode() {
            return localStorage.getItem(WorksheetContext.LAB_MODE_STORAGE_KEY);
        },

        // --- Mode and editor projection ---
        /** @brief Normalise a saved mode through the native mode catalogue. */
        stateMode(value: unknown) {
            return statePolicy.mode(value);
        },

        /** @brief Prepare metadata for a mode's retained editor text. */
        statePrepareMode(mode: WorksheetMode) {
            return WorksheetRequests.presentation.prepareLabEditor(WorksheetContext.modeEditorText[mode]);
        },

        /** @brief Apply a worksheet mode through the workspace controller. */
        stateApplyMode(mode: WorksheetMode) {
            return WorksheetWorkspace.mode.applyLabMode(mode);
        },

        /** @brief Restore the retained editor for the supplied worksheet mode. */
        stateRestoreEditor(mode: WorksheetMode) {
            return WorksheetWorkspace.mode.restoreModeEditor(mode);
        },

        // --- Calendar controls and summaries ---
        /** @brief Apply native-normalised calendar state to browser controls. */
        stateCalendar(mode: WorksheetMode, source: unknown, operation = 0, prefix = '') {
            return WorksheetLocations.calendar.applyCalendarState(mode, source, operation, prefix);
        },

        /** @brief Synchronise town menus without selecting default towns. */
        stateSyncTowns() {
            return WorksheetLocations.townSelection.syncTownSelectors({selectDefault: false});
        },

        /** @brief Restore a calendar's town using its current jurisdiction and coordinates. */
        stateTown(index: number, town: string | undefined) {
            const [select, jurisdiction, latitude, longitude] = ([
                [WorksheetContext.datetimeTown, WorksheetContext.datetimeJurisdiction, WorksheetContext.datetimeLatitude, WorksheetContext.datetimeLongitude],
                [WorksheetContext.almanacTown, WorksheetContext.almanacJurisdiction, WorksheetContext.almanacLatitude, WorksheetContext.almanacLongitude]
            ] as const)[index];
            return WorksheetLocations.townSelection.restoreTownSelection(
                select, jurisdiction && jurisdiction.value, town, latitude && latitude.value,
                longitude && longitude.value);
        },

        /** @brief Capture native-normalised DateTime controls. */
        stateDatetime() {
            return WorksheetLocations.calendar.currentDatetimeState();
        },

        /** @brief Capture native-normalised almanac controls. */
        stateAlmanac() {
            return WorksheetLocations.calendar.currentAlmanacState();
        },

        /** @brief Obtain native summary text for DateTime state. */
        stateDatetimeSummary(state: WorksheetSnapshot) {
            return WorksheetLocations.calendar.datetimeSummaryText(state);
        },

        /** @brief Obtain native summary text for almanac state. */
        stateAlmanacSummary(state: WorksheetSnapshot) {
            return WorksheetLocations.calendar.almanacSummaryText(state);
        }
    }) satisfies WorksheetRestorationCapabilities;

    /** @brief Native history transactions and request-guarded navigation. */
    export const historyCapabilities = Object.freeze({
        // --- Restoration and guarded navigation ---
        /** @brief Build a native history plan for the requested restoration phase. */
        stateHistoryPlan(phase: number, state: WorksheetSnapshot) {
            return labDOM.call('lab_storage_history', phase, state, labApplication.configuration);
        },

        /** @brief Restore calendar history under the supplied ownership guard. */
        stateCalendarHistory(mode: WorksheetMode, source: unknown, isCurrent?: WorksheetGuard) {
            return WorksheetLocations.calendarActions.restoreCalendarHistory(mode, source, isCurrent);
        },

        /** @brief Navigate native history using the current snapshot. */
        stateNavigate(direction: number) {
            return history.navigate(direction);
        },

        /** @brief Restore a snapshot while its request remains current. */
        stateRestoreHistory(state: WorksheetSnapshot, request: WorksheetRequest) {
            return navigation.restore(state, stateCallbacks.requestCurrent.bind(null, request));
        },

        /** @brief Evaluate restored state without adding another history entry. */
        stateEvaluateHistory() {
            return WorksheetWorkspace.mode.evaluateCurrentMode({skipHistoryUpdate: true});
        },

        /** @brief Refresh history navigation button availability. */
        stateHistoryButtons() {
            return WorksheetWorkspace.presentation.updateHistoryButtons();
        },

        // --- Snapshot capture and stack updates ---
        /** @brief Read the native-validated interval budget as snapshot text. */
        stateHistoryCap() {
            return String(statePolicy.intervalCap(WorksheetContext.integratorIntervalCap?.value));
        },

        /** @brief Build a native history snapshot from editor and control values. */
        stateHistoryRecord(index: number, text: string, calendar: unknown, bounds: string, cap: string) {
            return labDOM.call('lab_persist_history', index, text, calendar, bounds, cap, labApplication.configuration) as WorksheetSnapshot;
        },

        /** @brief Capture current history with an optional authored-text override. */
        stateCurrentHistory(textOverride?: string | null) {
            return history.snapshot(WorksheetWorkspace.mode.currentMode(), textOverride);
        },

        /** @brief Stage and retain a snapshot, propagating native history rejection. */
        statePushSnapshot(snapshot: WorksheetSnapshot) {
            const mode = WorksheetWorkspace.mode.workspaceModeId(snapshot.mode), length = WorksheetWorkspace.labWorkspaceBuffer.stageSnapshot(snapshot);
            // Flags 3: bit 0 deduplicates adjacent snapshots; bit 1 clears forward history,
            // even for a blank or duplicate snapshot. A plain cursor push must not do this.
            if (labWire.exports().lab_workspace_history_push(mode, 0, length, Number(!!snapshot.text), 3) < 0)
                throw new Error('Could not retain worksheet history');
        },

        /** @brief Clear the current worksheet's native forward history. */
        stateClearForward() {
            return history.clearForward();
        }
    }) satisfies WorksheetHistoryCapabilities;

    /** @brief Editor capture, binding commits and source projection capabilities. */
    export const editorCapabilities = Object.freeze({
        // --- Retained source and visible editor text ---
        /** @brief Replace retained editor text for a native mode index. */
        stateEditor(index: number, text: string) {
            return WorksheetContext.modeEditorText[WorksheetWorkspace.WORKSPACE_MODE_NAMES[index]] = text;
        },

        /** @brief Update the expression timestamp and editor text. */
        stateExpression(text: string, timestamp: number) {
            WorksheetContext.worksheetSession.editor.lastExpressionUpdatedAt = timestamp;
            WorksheetBindings.controller.setExpressionEditor(text);
        },

        /** @brief Update and return the retained expression timestamp. */
        stateTimestamp(timestamp: number) {
            return WorksheetContext.worksheetSession.editor.lastExpressionUpdatedAt = timestamp;
        },

        /** @brief Set expression editor text through the binding controller. */
        stateSetExpression(text: string) {
            return WorksheetBindings.controller.setExpressionEditor(text);
        },

        /** @brief Retain the integrator's authored source on its editor element. */
        stateIntegratorSource(text: string) {
            return WorksheetContext.expr.dataset.savedIntegratorExpression = text;
        },

        /** @brief Apply expression text returned by a binding update. */
        stateUpdated(text: string) {
            return WorksheetBindings.controller.applyUpdatedBindingExpression(text);
        },

        /** @brief Clear the binding controller's retained expression source. */
        stateClearSource() {
            return WorksheetBindings.controller.clearExpressionSource();
        },

        /** @brief Read current expression text with the visible editor as fallback. */
        stateEditorText() {
            return WorksheetBindings.editor.currentExpressionText() || WorksheetContext.expr.value || '';
        },

        /** @brief Read the retained expression-mode editor text. */
        stateExpressionSavedText() {
            return WorksheetContext.modeEditorText.expression;
        },

        // --- Binding inputs and metadata ---
        /** @brief Clear displayed variable binding values. */
        stateClearBindings() {
            return WorksheetBindings.panel.clearVariableValues();
        },

        /** @brief Commit visible binding inputs under the supplied ownership guard. */
        stateCommitBindings(isCurrent?: WorksheetGuard) {
            return WorksheetBindings.controller.commitVisibleBindingInputs(isCurrent);
        },

        /** @brief Cancel the binding refresh timer and its pending request. */
        stateCancelBindings() {
            clearTimeout(WorksheetBindings.controller.expressionBindingRefreshTimer);
            WorksheetRequests.labRequests.cancel('bindings');
        },

        /** @brief Obtain native binding sections for expression text. */
        stateBindingParts(text: string) {
            return WorksheetBindings.editor.bindingParts(text);
        },

        // --- Guarded capture and restoration ---
        /** @brief Capture an editor guard combining caller ownership, mode and context. */
        stateCaptureGuard(isCurrent: WorksheetGuard) {
            const mode = WorksheetWorkspace.mode.currentMode(), context = WorksheetRequests.labRequests.context();
            return stateCallbacks.editorCurrent.bind(null, isCurrent, mode, context);
        },

        /** @brief Stage current editor text and its default for native capture. */
        stateCaptureEditor() {
            const native = labWire.exports(), index = native.lab_workspace_mode();
            const length = WorksheetWorkspace.labWorkspaceBuffer.stageText(WorksheetBindings.editor.currentExpressionText() || WorksheetContext.expr.value.trim());
            const defaultLength = WorksheetWorkspace.labWorkspaceBuffer.stageText(WorksheetWorkspace.mode.workspaceDefaultText(index), 1);
            if (!native.lab_workspace_editor_capture(index, length, defaultLength))
                throw new Error('Could not retain worksheet editor text');
            return index;
        },

        /** @brief Capture current editor state while the supplied request remains current. */
        stateCaptureRequest(request: WorksheetRequest) {
            return WorksheetWorkspace.mode.captureCurrentModeEditor(stateCallbacks.requestCurrent.bind(null, request));
        },

        /** @brief Capture staged editor text, propagating native rejection. */
        stateRetainText(index: number, text: string) {
            if (!labWire.exports().lab_workspace_editor_capture(index, WorksheetWorkspace.labWorkspaceBuffer.stageText(text), 0))
                throw new Error('Could not retain worksheet text');
        },

        /** @brief Replace retained mode text with its native default. */
        stateDefaultEditor(mode: WorksheetMode, index: number) {
            return WorksheetContext.modeEditorText[mode] = WorksheetWorkspace.mode.workspaceDefaultText(index);
        },

        /** @brief Restore and read native editor text with its default fallback. */
        stateRestoredText(index: number) {
            const length = labWire.exports().lab_workspace_editor_restore(
                index, WorksheetWorkspace.labWorkspaceBuffer.stageText(WorksheetWorkspace.mode.workspaceDefaultText(index), 1));
            return WorksheetWorkspace.labWorkspaceBuffer.readText(length);
        }
    }) satisfies WorksheetEditorCapabilities;

    /** @brief Workspace lifecycle and mode projection capabilities. */
    export const workspaceCapabilities = Object.freeze({
        // --- Request lifecycle ---
        /** @brief Cancel pending binding refresh and evaluation, binding and function requests. */
        stateClearPrelude() {
            clearTimeout(WorksheetBindings.controller.expressionBindingRefreshTimer);
            WorksheetRequests.labRequests.cancel('evaluate');
            WorksheetRequests.labRequests.cancel('bindings');
            WorksheetRequests.labRequests.cancel('function');
        },

        /** @brief Continue native clearing within an owned evaluation request. */
        stateClearOwned() {
            return WorksheetRequests.labRequests.runUI('evaluate', WorksheetWorkspace.mode.currentMode(), stateCallbacks.clearOwned);
        },

        /** @brief Apply the native clear action for the current worksheet. */
        stateClearProjection() {
            return labDOM.call('lab_events_clear', WorksheetWorkspace.mode.workspaceModeId(WorksheetWorkspace.mode.currentMode()), labApplication.configuration) as number;
        },

        /** @brief Notify the request manager of the current worksheet mode. */
        stateNotifyMode() {
            return WorksheetRequests.labRequests.modeChanged(WorksheetWorkspace.mode.currentMode());
        },

        /** @brief Capture a guard comparing request context and latest main-request token. */
        stateSelectionGuard() {
            const context = WorksheetRequests.labRequests.context(), main = WorksheetRequests.labRequests.latestMain();
            return stateCallbacks.selectionCurrent.bind(null, context, main);
        },

        // --- Mode selection ---
        /** @brief Select a worksheet mode through the workspace controller. */
        stateSetMode(mode: WorksheetMode) {
            return WorksheetWorkspace.mode.setMode(mode);
        },

        /** @brief Normalise and forcibly select a worksheet mode. */
        stateForceMode(mode: WorksheetMode) {
            return WorksheetWorkspace.mode.setMode(statePolicy.mode(mode), {force: true});
        },

        /** @brief Read the current worksheet's native mode index. */
        stateModeIndex() {
            return WorksheetWorkspace.mode.workspaceModeId(WorksheetWorkspace.mode.currentMode());
        },

        /** @brief Resolve a worksheet mode name to its native index. */
        stateNamedIndex(mode: WorksheetMode) {
            return WorksheetWorkspace.mode.workspaceModeId(mode);
        },

        /** @brief Synchronise worksheet mode tabs. */
        stateSyncTabs() {
            return WorksheetWorkspace.presentation.syncModeTabs();
        },

        /** @brief Synchronise controls with the selected worksheet mode. */
        stateSyncUI() {
            return WorksheetWorkspace.presentation.syncModeUI();
        },

        // --- Editor focus and completion ---
        /** @brief Focus the worksheet source editor. */
        stateFocusEditor() {
            return WorksheetContext.expr.focus();
        },

        /** @brief Restore retained editor text for the current worksheet. */
        stateRestoreCurrentEditor() {
            return WorksheetWorkspace.mode.restoreModeEditor(WorksheetWorkspace.mode.currentMode());
        },

        /** @brief Normalise the displayed interval budget and focus the editor. */
        stateFinishIntegrator() {
            if (WorksheetContext.integratorIntervalCap)
                WorksheetContext.integratorIntervalCap.value = String(statePolicy.intervalCap(WorksheetContext.integratorIntervalCap.value));
            WorksheetContext.expr.focus();
        },

        /** @brief Fill blank DateTime defaults and focus its date input. */
        stateFinishDatetime() {
            WorksheetLocations.calendar.restoreDatetimeDefaultsIfBlank();
            WorksheetContext.datetimeDate?.focus();
        },

        /** @brief Fill blank almanac defaults and focus its date input. */
        stateFinishAlmanac() {
            WorksheetLocations.calendar.restoreAlmanacDefaultsIfBlank();
            WorksheetContext.almanacDate?.focus();
        },

        /** @brief Clear the editor's manually selected size. */
        stateResetEditorSize() {
            return WorksheetWorkspace.layout.resetEditorManualSize();
        },

        // --- Calendar ---
        /** @brief Reset current calendar controls through native state policy. */
        stateResetCalendar() {
            return WorksheetLocations.calendar.applyCalendarState(WorksheetWorkspace.mode.currentMode(), {}, 3);
        },

        /** @brief Apply native DateTime defaults only to blank controls. */
        stateDatetimeDefaults() {
            return WorksheetLocations.calendar.restoreDatetimeDefaultsIfBlank();
        },

        /** @brief Apply native almanac defaults only to blank controls. */
        stateAlmanacDefaults() {
            return WorksheetLocations.calendar.restoreAlmanacDefaultsIfBlank();
        },

        /** @brief Refresh DateTime location, then continue native state projection. */
        stateRefreshLocation() {
            WorksheetLocations.calendarActions.refreshDatetimeJurisdictionLocation()
                .then(stateCallbacks.refreshLocation.bind(null, stateRuntime));
        },

        // --- Status and controls ---
        /** @brief Present worksheet status text. */
        stateStatus(text: string) {
            return WorksheetWorkspace.presentation.setStatus(text);
        },

        /** @brief Hide the goal-seeking target entry. */
        stateHideTarget() {
            return WorksheetWorkspace.presentation.hideTargetEntry();
        },

        /** @brief Synchronise matrix controls with the selected operation. */
        stateSyncMatrix() {
            return WorksheetWorkspace.presentation.syncMatrixControls();
        }
    }) satisfies WorksheetWorkspaceCapabilities;

    /** @brief Integrator form preparation, control normalisation and row projection. */
    export const formsCapabilities = Object.freeze({
        // --- Form preparation and response projection ---
        /** @brief Request native form processing for an opaque payload. */
        stateFormsRequest(payload: unknown): Promise<unknown> {
            return WorksheetRequests.presentation.requestLabForms(payload);
        },

        /** @brief Read the expression used for integrator form preparation. */
        stateFormsExpression() {
            return WorksheetWorkspace.integrator.integratorFormsExpression();
        },

        /** @brief Install native integrator form metadata for an expression. */
        stateFormsMetadata(expression: unknown, result: WorksheetEvaluationData) {
            return WorksheetWorkspace.integrator.forms.install(expression, result);
        },

        /** @brief Capture the rows property once, preserving null and undefined distinctly. */
        stateFormsRows(result: {rows?: WorksheetIntegratorRow[] | null}) {
            return ({value: result.rows});
        },

        /** @brief Render integrator rows from the supplied captured value. */
        stateFormsRender(rows: {value: WorksheetIntegratorRow[]}) {
            return WorksheetBindings.integratorPanel.renderIntegratorRows(rows.value);
        },

        /** @brief Merge native response data into the current integrator rows. */
        stateMergeRows(data: unknown) {
            return labDOM.call('lab_binding_rows_merge', WorksheetWorkspace.integrator.currentIntegratorRows(), data) as WorksheetIntegratorRow[];
        },

        /** @brief Render the supplied integrator rows. */
        stateRenderRows(rows: WorksheetIntegratorRow[]) {
            return WorksheetBindings.integratorPanel.renderIntegratorRows(rows);
        },

        /** @brief Render the current integrator rows. */
        stateRenderActiveRows() {
            return WorksheetBindings.integratorPanel.renderIntegratorRows(WorksheetWorkspace.integrator.activeIntegratorRows());
        },

        // --- Integrator bounds and interval budget ---
        /** @brief Replace integrator bounds with blank rows. */
        stateBlankBounds() {
            return WorksheetWorkspace.integrator.resetIntegratorBoundsToBlank();
        },

        /** @brief Ask native state policy whether integrator bounds are needed. */
        stateNeedsBounds() {
            return stateRuntime.sync(16, {});
        },

        /** @brief Restore the default integrator bounds. */
        stateResetBounds() {
            return WorksheetWorkspace.integrator.resetIntegratorBoundsToDefault();
        },

        /** @brief Read the current integrator bounds as text. */
        stateCurrentBounds() {
            return WorksheetWorkspace.integrator.currentIntegratorBoundsText();
        },

        /** @brief Count active bound rows in the integrator. */
        stateBoundCount() {
            return WorksheetWorkspace.integrator.activeIntegratorBoundRows().length;
        },

        /** @brief Read the requested native-validated interval budget. */
        stateIntervalCap() {
            return WorksheetWorkspace.integrator.requestedIntegratorIntervalCap();
        },

        /** @brief Replace the displayed interval budget with its native-normalised value. */
        stateNormaliseCap() {
            if (WorksheetContext.integratorIntervalCap)
                WorksheetContext.integratorIntervalCap.value = String(statePolicy.intervalCap(WorksheetContext.integratorIntervalCap.value));
        },

        /** @brief Return a native-normalised interval budget as text. */
        stateValidCap(value: unknown) {
            return String(statePolicy.intervalCap(value));
        },

        // --- Matrix controls ---
        /** @brief Read and natively validate the selected matrix operation. */
        stateMatrixOperation() {
            return statePolicy.matrixOperation(WorksheetContext.matrixOperation && WorksheetContext.matrixOperation.value);
        },

        /** @brief Read trimmed matrix operand text. */
        stateMatrixOperand() {
            return String(WorksheetContext.matrixOperand && WorksheetContext.matrixOperand.value || '').trim();
        },

        /** @brief Validate a matrix operation through native policy. */
        stateValidOperation(value: unknown) {
            return statePolicy.matrixOperation(value);
        }
    }) satisfies WorksheetFormsCapabilities;

    /** @brief Result projection, preview ownership and deferred layout capabilities. */
    export const presentationCapabilities = Object.freeze({
        // --- Result capture, restoration and clearing ---
        /** @brief Capture the current mode's result presentation. */
        stateSaveResult() {
            return WorksheetWorkspace.presentation.saveCurrentModeResultState();
        },

        /** @brief Restore the current mode's retained result presentation. */
        stateRestoreResult() {
            return WorksheetWorkspace.presentation.restoreModeResultState(WorksheetWorkspace.mode.currentMode());
        },

        /** @brief Restore native result content and its associated surface preview. */
        stateResultProjection(mode: WorksheetMode) {
            labSurfacePreview.dispose();
            const state = WorksheetWorkspace.presentation.resultStates[mode];
            const result = labDOM.call('lab_workspace_dom_result_restore', state);
            labSurfacePreview.restoreResult(state?.surfaceView ?? null);
            return result;
        },

        /** @brief Restore retained TeX, derivative and binding presentation fields. */
        stateResultValues(state: WorksheetResultValues) {
            WorksheetWorkspace.presentation.state.lastTex = state.lastTex;
            WorksheetWorkspace.presentation.state.lastDerivativeExpression = state.lastDerivativeExpression;
            WorksheetBindings.panel.currentVariables = state.currentVariables;
            WorksheetBindings.panel.currentDifferentiable = state.currentDifferentiable;
            WorksheetWorkspace.presentation.state.resultInputBindings = state.resultInputBindings;
        },

        /** @brief Clear the result pane through its presentation owner. */
        stateClearResult() {
            return WorksheetResults.reset.clearResultPane();
        },

        /** @brief Clear the retained function-card run. */
        stateClearFunction() {
            return WorksheetRequests.functionCard.clearFunctionRun();
        },

        // --- Derivative controls and deferred layout ---
        /** @brief Refresh derivative buttons for the current variables. */
        stateDerivativeButtons() {
            return WorksheetWorkspace.presentation.renderDerivativeButtons(WorksheetBindings.panel.currentVariables);
        },

        /** @brief Schedule rendered-result layout fitting. */
        stateRenderedFit() {
            return WorksheetResults.renderedFit.schedule();
        },

        /** @brief Schedule solver-result layout fitting. */
        stateSolverFit() {
            return WorksheetResults.solverFit.schedule();
        }
    }) satisfies WorksheetPresentationCapabilities;

    /** @brief Precision changes and evaluation commands; native plans retain sequencing. */
    export const evaluationCapabilities = Object.freeze({
        // --- Precision changes and re-evaluation ---
        /** @brief Apply explicit precision bits or a native-selected precision step. */
        statePrecisionStep(direction: number, bits?: number | null) {
            return WorksheetWorkspace.precision.setRequestedPrecisionBits(
                bits ?? labWire.exports().lab_workspace_precision_step(WorksheetWorkspace.precision.requestedPrecisionBits(), direction));
        },

        /** @brief Capture goal inputs and the native-selected precision action. */
        statePrecisionPlan(): WorksheetPrecisionPlan {
            const source = WorksheetBindings.goal.currentGoalSeekSource(), target = WorksheetContext.labEditorState.goalTarget || '';
            return {
                source,
                target,
                action: labWire.exports().lab_events_precision(WorksheetWorkspace.mode.workspaceModeId(WorksheetWorkspace.mode.currentMode()), Number(!!source), Number(!!target))
            };
        },

        /** @brief Rerun goal seeking at the new precision without adding history. */
        statePrecisionGoal(plan: WorksheetPrecisionPlan) {
            return WorksheetRequests.goals.runGoalSeek(
                plan.source, plan.target,
                WorksheetBindings.goal.solvedStartValuesForGoalSeek(plan.source, WorksheetContext.labEditorState.fullText || WorksheetBindings.editor.currentExpressionText()),
                {skipHistoryUpdate: true});
        },

        /** @brief Re-evaluate the last expression input without adding history. */
        statePrecisionExpression() {
            return WorksheetRequests.evaluation.evaluateExpression({skipHistoryUpdate: true, reuseLastInput: true});
        },

        // --- Readiness and mode-specific evaluation ---
        /** @brief Report whether the expression is ready for evaluation. */
        stateReady() {
            return WorksheetBindings.editor.expressionReadyToEvaluate();
        },

        /** @brief Evaluate the currently selected worksheet mode. */
        stateEvaluate() {
            return WorksheetWorkspace.mode.evaluateCurrentMode();
        },

        /** @brief Evaluate the current expression worksheet. */
        stateEvaluateExpression() {
            return WorksheetRequests.evaluation.evaluateExpression();
        },

        /** @brief Evaluate the current equation worksheet. */
        stateEvaluateEquation() {
            return WorksheetRequests.evaluation.evaluateEquation();
        },

        /** @brief Evaluate the current differential-equation worksheet. */
        stateEvaluateDiffequation() {
            return WorksheetRequests.evaluation.evaluateDiffequation();
        },

        /** @brief Evaluate the current matrix worksheet. */
        stateEvaluateMatrix() {
            return WorksheetRequests.evaluation.evaluateMatrix();
        },

        /** @brief Evaluate the current integrator worksheet. */
        stateEvaluateIntegrator() {
            return WorksheetRequests.evaluation.evaluateIntegrator();
        },

        /** @brief Evaluate the current DateTime worksheet. */
        stateEvaluateDatetime() {
            return WorksheetRequests.evaluation.evaluateDatetime();
        }
    }) satisfies WorksheetEvaluationCapabilities;

    /** @brief Domain-owned capability groups consumed by the composition root. */
    export const capabilityGroups = Object.freeze({
        persistence: persistenceCapabilities,
        restoration: restorationCapabilities,
        history: historyCapabilities,
        editor: editorCapabilities,
        workspace: workspaceCapabilities,
        forms: formsCapabilities,
        presentation: presentationCapabilities,
        evaluation: evaluationCapabilities
    });

    /** @brief Complete immutable registration view; each callback remains owned by its domain group. */
    export const capabilities: WorksheetStateFlowServices = Object.freeze({
        ...persistenceCapabilities,
        ...restorationCapabilities,
        ...historyCapabilities,
        ...editorCapabilities,
        ...workspaceCapabilities,
        ...formsCapabilities,
        ...presentationCapabilities,
        ...evaluationCapabilities
    });
}
