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
    /** @brief Own pending browser saves; native plans retain delay and stale-token policy. */
    export class WorksheetSaveScheduler {
        /** @brief Timer handles owned by this scheduler, indexed by native worksheet mode. */
        readonly timers = new Map<number, number>();

        /** @brief Inject browser timers and the native-selected publication operation. */
        constructor(private readonly clock: WorksheetSaveTimerPort,
                    private readonly publish: (save: WorksheetSavePlan) => void) {}

        /** @brief Cancel and release the pending timer for one worksheet. */
        readonly cancel = (index: number): void => {
            this.clock.cancel(this.timers.get(index));
            this.timers.delete(index);
        };

        /** @brief Forget a completed timer without cancelling a newer native-selected operation. */
        readonly forget = (index: number): boolean => this.timers.delete(index);

        /** @brief Schedule publication; native code decides whether an earlier timer must be cancelled. */
        readonly schedule = (save: WorksheetSavePlan): Map<number, number> =>
            this.timers.set(save.index, this.clock.schedule(() => this.publish(save), save.delay));

        /** @brief Release all pending browser timers when the owner is disposed. */
        readonly dispose = (): void => {
            for (const handle of this.timers.values()) this.clock.cancel(handle);
            this.timers.clear();
        };
    }

    /** @brief Own local fallback storage, publication and native-directed worksheet saves. */
    export class WorksheetPersistenceController {
        /** @brief Inject I/O, live inputs, native execution and an exclusively owned scheduler. */
        constructor(private readonly io: WorksheetPersistenceIO,
                    private readonly inputs: WorksheetPersistenceInputs,
                    private readonly runtime: WorksheetStateRuntimePort,
                    /** @brief Scheduler whose timers are released with this owner. */
                    readonly scheduler: WorksheetSaveScheduler) {}

        /** @brief Encode a fallback record, propagating codec and storage errors. */
        readonly writeLocal = (key: string, state: unknown): void => {
            const bytes = this.io.encode(state);
            const chunks: string[] = [];
            for (let offset = 0; offset < bytes.length; offset += 8192)
                chunks.push(String.fromCharCode(...bytes.subarray(offset, offset + 8192)));
            this.io.write(`${key}.protobuf`, btoa(chunks.join('')));
        };

        /** @brief Decode a bounded fallback; missing, corrupt or inaccessible storage yields null. */
        readonly readLocal = (key: string): unknown => {
            try {
                const text = this.io.read(`${key}.protobuf`);
                if (!text || text.length > this.io.base64Limit()) return null;
                const bytes = Uint8Array.from(atob(text), character => character.charCodeAt(0));
                return this.io.decode(bytes.buffer);
            } catch {
                return null;
            }
        };

        /** @brief Publish a precision-safe snapshot; asynchronous publication failure is non-fatal. */
        readonly publish = (patch: Record<string, unknown>, options: WorksheetSaveOptions = {}): void => {
            const payload = {...patch}, precision = this.inputs.precision();
            if (payload.precision_bits === precision) payload.precision_bits = {...precision};
            void this.io.post(this.io.encode(payload), !!options.keepalive).catch(() => {
                // Persistence is helpful, not essential.
            });
        };

        /** @brief Save the live native-backed precision view through the publication seam. */
        readonly savePrecision = (): void => this.publish({precision_bits: this.inputs.precision()});

        /** @brief Persist the selected worksheet through native mode-save policy. */
        readonly saveMode = (mode: WorksheetMode = this.inputs.mode()): void => {
            this.runtime.sync(3, {mode});
        };

        /** @brief Capture authored source at invocation time and execute native save policy. */
        readonly saveWorksheet = (mode: WorksheetMode, text = this.inputs.source(),
                                  options: WorksheetSaveOptions = {}): void => {
            this.runtime.sync(0, {index: this.inputs.modeIndex(mode), text, options});
        };

        /** @brief Capture and persist a calendar worksheet through native policy. */
        readonly saveCalendar = (mode: WorksheetMode): void => {
            this.runtime.sync(2, {mode});
        };

        /** @brief Release owned timer resources without changing persisted records. */
        readonly dispose = (): void => this.scheduler.dispose();
    }

    /** @brief Execute restoration with independent native frames and no shared pending-operation bag. */
    export class StateRestorationController {
        /** @brief Inject the typed native restoration executor. */
        constructor(private readonly runtime: WorksheetStateRuntimePort) {}

        /** @brief Restore sources with lazy reads and native-selected field order. */
        readonly restoreEditors = async (read: WorksheetStateReader, local = false): Promise<void> =>
            this.runtime.restore(16, {read, local});

        /** @brief Restore controls while preserving native await boundaries. */
        readonly restoreControls = async (read: WorksheetStateReader, local = false): Promise<void> =>
            this.runtime.restore(17, {read, local});

        /** @brief Apply an opaque server payload using native restoration policy. */
        readonly applySaved = async (data: unknown): Promise<void> => this.runtime.restore(18, {data});

        /** @brief Recover newer local sources according to native timestamp policy. */
        readonly recoverLocalEditors = async (data: unknown): Promise<void> => this.runtime.restore(19, {data});

        /** @brief Load server state and execute native-selected fallback or recovery. */
        readonly load = async (): Promise<void> => this.runtime.restore(20, {});
    }

    /** @brief Bind a mode and history direction without duplicating native history contents. */
    export class WorksheetHistoryCursor implements WorksheetStateHistoryCursor {
        /** @brief Inject the native history and staging operations used by this cursor. */
        constructor(private readonly index: number, private readonly store: number,
                    private readonly port: Pick<WorksheetHistoryPort, 'count' | 'push' | 'pop' | 'stage' | 'read'>) {}

        /** @brief Read the current native count without caching it. */
        get length(): number { return this.port.count(this.index, this.store); }

        /** @brief Retain a snapshot; detached callbacks keep their mode and direction. */
        readonly push = (snapshot: WorksheetSnapshot): number => {
            const count = this.port.push(this.index, this.store, this.port.stage(snapshot), Number(!!snapshot?.text), 0);
            if (count < 0) throw new Error('Could not retain worksheet history');
            return count;
        };

        /** @brief Remove and decode the native top entry, returning null when empty. */
        readonly pop = (): WorksheetSnapshot | null => this.port.read(this.port.pop(this.index, this.store));
    }

    /** @brief Adapt native history transactions and snapshot buffers without browser-side history policy. */
    export class NativeHistoryStore {
        /** @brief Inject native operations and the typed snapshot executor. */
        constructor(private readonly port: WorksheetHistoryPort, private readonly runtime: WorksheetStateRuntimePort) {}

        /** @brief Create an independently bound cursor for a native history direction. */
        readonly cursor = (store: number, mode: WorksheetMode = this.port.mode()): WorksheetStateHistoryCursor =>
            new WorksheetHistoryCursor(this.port.modeIndex(mode), store, this.port);

        /** @brief Atomically navigate using the current snapshot and native selected mode. */
        readonly navigate = (direction: number): WorksheetSnapshot | null => {
            const current = this.snapshot();
            const length = this.port.stage(current);
            return this.port.read(this.port.navigate(this.port.currentIndex(), direction, length, Number(!!current.text)));
        };

        /** @brief Read the selected worksheet's native backward count. */
        readonly backwardCount = (): number => this.cursor(this.port.backwardStore()).length;

        /** @brief Read the selected worksheet's native forward count. */
        readonly forwardCount = (): number => this.cursor(this.port.forwardStore()).length;

        /** @brief Capture a native record with an optional authored-source override. */
        readonly snapshot = (mode: WorksheetMode = this.port.mode(), textOverride: string | null = null): WorksheetSnapshot =>
            this.runtime.sync(9, {index: this.port.modeIndex(mode), textOverride});

        /** @brief Compare snapshots in separate native staging slots. */
        readonly equals = (left: WorksheetSnapshot, right: WorksheetSnapshot): boolean => {
            const leftLength = this.port.stage(left, 0), rightLength = this.port.stage(right, 1);
            return !!this.port.equal(leftLength, rightLength);
        };

        /** @brief Retrieve a native predecessor, retaining the current-mode fallback for a null snapshot. */
        readonly previous = (nextState: WorksheetSnapshot | null): WorksheetSnapshot | null => {
            const index = this.port.modeIndex(nextState && nextState.mode || this.port.mode());
            return this.port.read(this.port.previous(index, this.port.stage(nextState)));
        };

        /** @brief Commit the captured state and propagate native rejection. */
        readonly commit = (mode: WorksheetMode = this.port.mode(), textOverride: string | null = null): void => {
            const index = this.port.modeIndex(mode), length = this.port.stage(this.snapshot(mode, textOverride));
            if (!this.port.commit(index, length)) throw new Error('Could not commit worksheet state');
        };

        /** @brief Clear native forward history for the selected worksheet. */
        readonly clearForward = (mode: WorksheetMode = this.port.mode()): void =>
            this.port.clear(this.port.modeIndex(mode), 1);
    }

    /** @brief Coordinate history restoration with request-scoped navigation ownership. */
    export class HistoryNavigationController {
        /** @brief Inject native restoration and the narrow request-ownership capability. */
        constructor(private readonly runtime: WorksheetStateRuntimePort,
                    private readonly requests: WorksheetHistoryNavigationPort) {}

        /** @brief Restore while the supplied ownership guard remains current. */
        readonly restore = async (state: WorksheetSnapshot | null, isCurrent: WorksheetGuard = () => true): Promise<boolean> =>
            this.runtime.restore(21, {state, isCurrent});

        /** @brief Acquire evaluation ownership before executing native history navigation. */
        readonly navigate = async (direction: number): Promise<unknown> =>
            this.requests.run(this.requests.mode(), request => this.runtime.restore(22, {direction, request}));
    }

    /** @brief Typed native-state runtime ports shared by persistence, restoration and history owners. */
    export const stateRuntime: WorksheetStateRuntimePort = {
        sync: <K extends keyof WorksheetStateSyncFrames>(kind: K, frame: WorksheetStateSyncFrames[K]): WorksheetStateSyncResult<K> =>
            labDOM.sync<WorksheetStateSyncResult<K>>('lab_state_sync', kind, frame, WorksheetRequests.runtime.getRegistry()),
        restore: <K extends keyof WorksheetStateRestoreFrames>(kind: K, frame: WorksheetStateRestoreFrames[K]): Promise<WorksheetStateRestoreResult<K>> =>
            WorksheetRequests.runtime.continue<WorksheetStateRestoreResult<K>>(kind, frame)
    };

    /** @brief Native validation and persistence schema gateway; configuration is read only when invoked. */
    export const statePolicy: WorksheetStatePolicyPort = {
        intervalCap: value => labDOM.call<number>('lab_storage_intervals', [value], labApplication.configuration),
        matrixOperation: value => labDOM.call<string>('lab_storage_operation', [value]),
        mode: value => WorksheetWorkspace.WORKSPACE_MODE_NAMES[WorksheetWorkspace.mode.workspaceModeId(value)],
        schema: index => labDOM.call<WorksheetStateSchemaField[]>('lab_persist_schema', index)
    };

    /** @brief Application persistence owner with live browser adapters and independent timer ownership. */
    export const persistence: WorksheetPersistenceController = new WorksheetPersistenceController(
        {
            read: key => localStorage.getItem(key),
            write: (key, value) => localStorage.setItem(key, value),
            encode: value => labWire.encode(value),
            decode: bytes => labWire.decode(bytes),
            base64Limit: () => labWire.exports().lab_browser_base64_limit(),
            post: (bytes, keepalive) => labHttp.fetch('/state', {method: 'POST', body: bytes, keepalive})
        },
        {
            mode: () => WorksheetWorkspace.mode.currentMode(),
            modeIndex: mode => WorksheetWorkspace.mode.workspaceModeId(mode),
            source: () => WorksheetBindings.editor.currentExpressionText() || WorksheetContext.expr.value || '',
            precision: () => WorksheetContext.modePrecisionBits
        },
        stateRuntime,
        new WorksheetSaveScheduler(
            {schedule: (callback, delay) => setTimeout(callback, delay), cancel: handle => clearTimeout(handle)},
            save => stateRuntime.sync(1, {save})));

    /** @brief Application restoration owner, with independent frames for concurrent calls. */
    export const restoration: StateRestorationController = new StateRestorationController(stateRuntime);

    const historyPort: WorksheetHistoryPort = {
        count: (index, store) => labWire.exports().lab_workspace_history_count(index, store),
        push: (index, store, length, text, flags) => labWire.exports().lab_workspace_history_push(index, store, length, text, flags),
        pop: (index, store) => labWire.exports().lab_workspace_history_pop(index, store),
        navigate: (index, direction, length, text) => labWire.exports().lab_workspace_navigate(index, direction, length, text),
        equal: (left, right) => labWire.exports().lab_workspace_equal(left, right),
        previous: (index, length) => labWire.exports().lab_workspace_previous(index, length),
        commit: (index, length) => labWire.exports().lab_workspace_commit(index, length),
        clear: (index, store) => { labWire.exports().lab_workspace_history_clear(index, store); },
        stage: (snapshot, slot) => WorksheetWorkspace.labWorkspaceBuffer.stageSnapshot(snapshot, slot),
        read: length => WorksheetWorkspace.labWorkspaceBuffer.readSnapshot(length),
        mode: () => WorksheetWorkspace.mode.currentMode(),
        modeIndex: mode => WorksheetWorkspace.mode.workspaceModeId(mode),
        currentIndex: () => labWire.exports().lab_workspace_mode(),
        backwardStore: () => WorksheetContext.expressionHistory,
        forwardStore: () => WorksheetContext.forwardHistory
    };

    /** @brief Application history adapter; native memory remains the sole history store. */
    export const history: NativeHistoryStore = new NativeHistoryStore(historyPort, stateRuntime);

    /** @brief Application navigation owner sharing the request manager's native ownership protocol. */
    export const navigation: HistoryNavigationController = new HistoryNavigationController(stateRuntime, {
        mode: () => WorksheetWorkspace.mode.currentMode(),
        run: (mode, action) => WorksheetRequests.labRequests.runUI('evaluate', mode, action)
    });

    /** @brief Native-directed publication, local records and pending-save resources. */
    export const persistenceCapabilities = Object.freeze({
        stateSave: (...args: Parameters<typeof persistence.publish>) => persistence.publish(...args),
        stateSaveEditor: (index: number) => stateRuntime.sync(15, {index}),
        stateSaveCalendar: (index: number) => persistence.saveCalendar(WorksheetWorkspace.WORKSPACE_MODE_NAMES[index]),
        stateSaveMode: () => persistence.saveMode(WorksheetWorkspace.mode.currentMode()),
        stateSavePrecision: () => persistence.savePrecision(),
        stateRecords: (index: number, values: unknown) => labDOM.call<unknown>('lab_persist_records', index, values, WorksheetContext.modePrecisionBits),
        stateLocalRecords: (records: Record<string, unknown>) => {
            for (const [key, value] of Object.entries(records)) localStorage.setItem(key, String(value));
        },
        stateCancelSaveTimer: (index: number) => persistence.scheduler.cancel(index),
        stateForgetSaveTimer: (index: number) => persistence.scheduler.forget(index),
        stateScheduleSave: (save: WorksheetSavePlan) =>
            persistence.scheduler.schedule(save),
        statePublishSave: (save: WorksheetSavePlan) => stateRuntime.sync(1, {save}),
        stateWriteCalendar: (key: string, state: unknown) => persistence.writeLocal(key, state),
        stateCalendarPatch: (mode: WorksheetMode, state: unknown) =>
            labDOM.call<unknown>('lab_persist_calendar', WorksheetWorkspace.mode.workspaceModeId(mode), state, WorksheetContext.modePrecisionBits),
        stateWriteMode: (mode: WorksheetMode) => localStorage.setItem(WorksheetContext.LAB_MODE_STORAGE_KEY, mode),
        stateSaveDatetime: () => persistence.saveCalendar('datetime'),
        stateSaveAlmanac: () => persistence.saveCalendar('almanac'),
        stateSaveWorksheet: (...args: Parameters<typeof persistence.saveWorksheet>) => persistence.saveWorksheet(...args),
        stateSaveNamedEditor: (index: number) => persistence.saveWorksheet(WorksheetWorkspace.WORKSPACE_MODE_NAMES[index]),
        stateFlushSave: (mode: WorksheetMode) => persistence.saveWorksheet(mode, undefined, {keepalive: true})
    }) satisfies WorksheetPersistenceCapabilities;

    /** @brief Native-selected recovery and restoration adapters with live external capabilities. */
    export const restorationCapabilities = Object.freeze({
        stateRead: (read: (key: string) => unknown, key: string) => read(key),
        stateNow: () => Date.now(),
        statePrepare: (text: string) => WorksheetRequests.presentation.prepareLabEditor(text),
        stateCanonical: (text: string) => WorksheetBindings.editor.expressionWithSortedConstants(text),
        stateControl: (field: WorksheetStorageField, value: unknown, local: boolean) => labDOM.call<unknown>('lab_storage_control', field, [value], local, labApplication.configuration),
        stateBounds: (...args: Parameters<typeof WorksheetWorkspace.integrator.restoreIntegratorBoundsText>) => WorksheetWorkspace.integrator.restoreIntegratorBoundsText(...args),
        stateServerEditors: (data: Record<string, unknown>) => restoration.restoreEditors((key: string) => data[key]),
        stateServerControls: (data: Record<string, unknown>) => restoration.restoreControls((key: string) => data[key]),
        stateCalendar: (...args: Parameters<typeof WorksheetLocations.calendar.applyCalendarState>) => WorksheetLocations.calendar.applyCalendarState(...args),
        stateSyncTowns: () => WorksheetLocations.townSelection.syncTownSelectors({selectDefault: false}),
        stateTown: (index: number, town: string | undefined) => {
            const [select, jurisdiction, latitude, longitude] = ([
                [WorksheetContext.datetimeTown, WorksheetContext.datetimeJurisdiction, WorksheetContext.datetimeLatitude, WorksheetContext.datetimeLongitude],
                [WorksheetContext.almanacTown, WorksheetContext.almanacJurisdiction, WorksheetContext.almanacLatitude, WorksheetContext.almanacLongitude]
            ] as const)[index];
            return WorksheetLocations.townSelection.restoreTownSelection(
                select, jurisdiction && jurisdiction.value, town, latitude && latitude.value,
                longitude && longitude.value);
        },
        stateMode: (value: unknown) => statePolicy.mode(value),
        statePrepareMode: (mode: WorksheetMode) => WorksheetRequests.presentation.prepareLabEditor(WorksheetContext.modeEditorText[mode]),
        stateApplyMode: (mode: WorksheetMode) => WorksheetWorkspace.mode.applyLabMode(mode),
        stateRecovery: (index: number, data: unknown) => {
            const fields = statePolicy.schema(index);
            return labDOM.call<unknown>(
                'lab_storage_recovery', index, data,
                [localStorage.getItem(fields[0].local), localStorage.getItem(fields[1].local)], labApplication.configuration);
        },
        stateRecovered: (index: number, recovery: unknown, text: string, now: number) =>
            labDOM.call<unknown>('lab_storage_recovered', index, recovery, [text], now),
        stateRestoreEditor: (mode: WorksheetMode) => WorksheetWorkspace.mode.restoreModeEditor(mode),
        stateFetch: () => labHttp.fetch('/state'),
        stateDecode: (response: WorksheetResponse) => response.labData(),
        stateApplySaved: (data: unknown) => restoration.applySaved(data),
        stateRecover: (data: unknown) => restoration.recoverLocalEditors(data),
        stateLocalEditors: () => restoration.restoreEditors((key: string) => localStorage.getItem(key), true),
        stateLocalControls: () => restoration.restoreControls((key: string) => localStorage.getItem(key), true),
        stateLocalCalendar: (key: string) => persistence.readLocal(key),
        stateLocalMode: () => localStorage.getItem(WorksheetContext.LAB_MODE_STORAGE_KEY),
        stateGuard: (guard: WorksheetGuard) => guard(),
        stateDatetime: () => WorksheetLocations.calendar.currentDatetimeState(),
        stateAlmanac: () => WorksheetLocations.calendar.currentAlmanacState(),
        stateDatetimeSummary: (state: WorksheetSnapshot) => WorksheetLocations.calendar.datetimeSummaryText(state),
        stateAlmanacSummary: (state: WorksheetSnapshot) => WorksheetLocations.calendar.almanacSummaryText(state)
    }) satisfies WorksheetRestorationCapabilities;

    /** @brief Native history transactions and request-guarded navigation. */
    export const historyCapabilities = Object.freeze({
        stateHistoryPlan: (phase: number, state: WorksheetSnapshot) => labDOM.call<unknown>('lab_storage_history', phase, state, labApplication.configuration),
        stateCalendarHistory: (...args: Parameters<typeof WorksheetLocations.calendarActions.restoreCalendarHistory>) => WorksheetLocations.calendarActions.restoreCalendarHistory(...args),
        stateNavigate: (direction: number) => history.navigate(direction),
        stateHistoryButtons: () => WorksheetWorkspace.presentation.updateHistoryButtons(),
        stateRestoreHistory: (state: WorksheetSnapshot, request: WorksheetRequest) => navigation.restore(state, () => WorksheetRequests.labRequests.current(request)),
        stateEvaluateHistory: () => WorksheetWorkspace.mode.evaluateCurrentMode({skipHistoryUpdate: true}),
        stateClearForward: () => history.clearForward(),
        stateHistoryCap: () => String(statePolicy.intervalCap(WorksheetContext.integratorIntervalCap?.value)),
        stateHistoryRecord: (index: number, text: string, calendar: unknown, bounds: string, cap: string) =>
            labDOM.call<WorksheetSnapshot>('lab_persist_history', index, text, calendar, bounds, cap, labApplication.configuration),
        stateCurrentHistory: (...args: [textOverride?: string | null]) => history.snapshot(WorksheetWorkspace.mode.currentMode(), ...args),
        statePushSnapshot: (snapshot: WorksheetSnapshot) => {
            const mode = WorksheetWorkspace.mode.workspaceModeId(snapshot.mode), length = WorksheetWorkspace.labWorkspaceBuffer.stageSnapshot(snapshot);
            if (labWire.exports().lab_workspace_history_push(mode, 0, length, Number(!!snapshot.text), 3) < 0)
                throw new Error('Could not retain worksheet history');
        }
    }) satisfies WorksheetHistoryCapabilities;

    /** @brief Editor capture, binding commits and source projection capabilities. */
    export const editorCapabilities = Object.freeze({
        stateEditor: (index: number, text: string) => WorksheetContext.modeEditorText[WorksheetWorkspace.WORKSPACE_MODE_NAMES[index]] = text,
        stateExpression: (text: string, timestamp: number) => {
            WorksheetContext.worksheetSession.editor.lastExpressionUpdatedAt = timestamp;
            WorksheetBindings.controller.setExpressionEditor(text);
        },
        stateTimestamp: (timestamp: number) => WorksheetContext.worksheetSession.editor.lastExpressionUpdatedAt = timestamp,
        stateSetExpression: (text: string) => WorksheetBindings.controller.setExpressionEditor(text),
        stateIntegratorSource: (text: string) => WorksheetContext.expr.dataset.savedIntegratorExpression = text,
        stateUpdated: (text: string) => WorksheetBindings.controller.applyUpdatedBindingExpression(text),
        stateClearSource: () => WorksheetBindings.controller.clearExpressionSource(),
        stateClearBindings: () => WorksheetBindings.panel.clearVariableValues(),
        stateCommitBindings: (...args: Parameters<typeof WorksheetBindings.controller.commitVisibleBindingInputs>) => WorksheetBindings.controller.commitVisibleBindingInputs(...args),
        stateCancelBindings: () => {
            clearTimeout(WorksheetBindings.controller.expressionBindingRefreshTimer);
            WorksheetRequests.labRequests.cancel('bindings');
        },
        stateCaptureGuard: (isCurrent: WorksheetGuard) => {
            const mode = WorksheetWorkspace.mode.currentMode(), context = WorksheetRequests.labRequests.context();
            return () => isCurrent() && WorksheetWorkspace.mode.currentMode() === mode && WorksheetRequests.labRequests.context() === context;
        },
        stateCaptureEditor: () => {
            const native = labWire.exports(), index = native.lab_workspace_mode();
            const length = WorksheetWorkspace.labWorkspaceBuffer.stageText(WorksheetBindings.editor.currentExpressionText() || WorksheetContext.expr.value.trim());
            const defaultLength = WorksheetWorkspace.labWorkspaceBuffer.stageText(WorksheetWorkspace.mode.workspaceDefaultText(index), 1);
            if (!native.lab_workspace_editor_capture(index, length, defaultLength))
                throw new Error('Could not retain worksheet editor text');
            return index;
        },
        stateCaptureRequest: (request: WorksheetRequest) => WorksheetWorkspace.mode.captureCurrentModeEditor(() => WorksheetRequests.labRequests.current(request)),
        stateRetainText: (index: number, text: string) => {
            if (!labWire.exports().lab_workspace_editor_capture(index, WorksheetWorkspace.labWorkspaceBuffer.stageText(text), 0))
                throw new Error('Could not retain worksheet text');
        },
        stateDefaultEditor: (mode: WorksheetMode, index: number) => WorksheetContext.modeEditorText[mode] = WorksheetWorkspace.mode.workspaceDefaultText(index),
        stateRestoredText: (index: number) => {
            const length = labWire.exports().lab_workspace_editor_restore(
                index, WorksheetWorkspace.labWorkspaceBuffer.stageText(WorksheetWorkspace.mode.workspaceDefaultText(index), 1));
            return WorksheetWorkspace.labWorkspaceBuffer.readText(length);
        },
        stateBindingParts: (text: string) => WorksheetBindings.editor.bindingParts(text),
        stateEditorText: () => WorksheetBindings.editor.currentExpressionText() || WorksheetContext.expr.value || '',
        stateExpressionSavedText: () => WorksheetContext.modeEditorText.expression
    }) satisfies WorksheetEditorCapabilities;

    /** @brief Workspace lifecycle and mode projection capabilities. */
    export const workspaceCapabilities = Object.freeze({
        stateClearPrelude: () => {
            clearTimeout(WorksheetBindings.controller.expressionBindingRefreshTimer);
            WorksheetRequests.labRequests.cancel('evaluate');
            WorksheetRequests.labRequests.cancel('bindings');
            WorksheetRequests.labRequests.cancel('function');
        },
        stateClearOwned: () =>
            WorksheetRequests.labRequests.runUI('evaluate', WorksheetWorkspace.mode.currentMode(), (request: WorksheetRequest) => WorksheetRequests.runtime.continue(72, {stage: 3, request})),
        stateClearProjection: () => labDOM.call<number>('lab_events_clear', WorksheetWorkspace.mode.workspaceModeId(WorksheetWorkspace.mode.currentMode()), labApplication.configuration),
        stateResetCalendar: () => WorksheetLocations.calendar.applyCalendarState(WorksheetWorkspace.mode.currentMode(), {}, 3),
        stateModeIndex: () => WorksheetWorkspace.mode.workspaceModeId(WorksheetWorkspace.mode.currentMode()),
        stateSetMode: (mode: WorksheetMode) => WorksheetWorkspace.mode.setMode(mode),
        stateSelectionGuard: () => {
            const context = WorksheetRequests.labRequests.context(), main = WorksheetRequests.labRequests.latestMain();
            return () => context === WorksheetRequests.labRequests.context() && main === WorksheetRequests.labRequests.latestMain();
        },
        stateHideTarget: () => WorksheetWorkspace.presentation.hideTargetEntry(),
        stateRestoreCurrentEditor: () => WorksheetWorkspace.mode.restoreModeEditor(WorksheetWorkspace.mode.currentMode()),
        stateSyncUI: () => WorksheetWorkspace.presentation.syncModeUI(),
        stateFinishIntegrator: () => {
            if (WorksheetContext.integratorIntervalCap)
                WorksheetContext.integratorIntervalCap.value = String(statePolicy.intervalCap(WorksheetContext.integratorIntervalCap.value));
            WorksheetContext.expr.focus();
        },
        stateFinishDatetime: () => {
            WorksheetLocations.calendar.restoreDatetimeDefaultsIfBlank();
            WorksheetContext.datetimeDate?.focus();
        },
        stateFinishAlmanac: () => {
            WorksheetLocations.calendar.restoreAlmanacDefaultsIfBlank();
            WorksheetContext.almanacDate?.focus();
        },
        stateFocusEditor: () => WorksheetContext.expr.focus(),
        stateStatus: (text: string) => WorksheetWorkspace.presentation.setStatus(text),
        stateNotifyMode: () => WorksheetRequests.labRequests.modeChanged(WorksheetWorkspace.mode.currentMode()),
        stateResetEditorSize: () => WorksheetWorkspace.layout.resetEditorManualSize(),
        stateSyncTabs: () => WorksheetWorkspace.presentation.syncModeTabs(),
        stateForceMode: (mode: WorksheetMode) => WorksheetWorkspace.mode.setMode(statePolicy.mode(mode), {force: true}),
        stateDatetimeDefaults: () => WorksheetLocations.calendar.restoreDatetimeDefaultsIfBlank(),
        stateAlmanacDefaults: () => WorksheetLocations.calendar.restoreAlmanacDefaultsIfBlank(),
        stateRefreshLocation: () => {
            WorksheetLocations.calendarActions.refreshDatetimeJurisdictionLocation().then(() => stateRuntime.sync(13, {}));
        },
        stateNamedIndex: (mode: WorksheetMode) => WorksheetWorkspace.mode.workspaceModeId(mode),
        stateSyncMatrix: () => WorksheetWorkspace.presentation.syncMatrixControls()
    }) satisfies WorksheetWorkspaceCapabilities;

    /** @brief Integrator form preparation, control normalisation and row projection. */
    export const formsCapabilities = Object.freeze({
        stateBlankBounds: () => WorksheetWorkspace.integrator.resetIntegratorBoundsToBlank(),
        stateFormsRequest: <T = unknown>(payload: unknown): Promise<T> => WorksheetRequests.presentation.requestLabForms<T>(payload),
        stateFormsExpression: () => WorksheetWorkspace.integrator.integratorFormsExpression(),
        stateFormsMetadata: (expression: unknown, result: WorksheetEvaluationData) => WorksheetWorkspace.integrator.forms.install(expression, result),
        // Box the property once: null and undefined survive native handle scopes distinctly.
        stateFormsRows: (result: {rows?: WorksheetIntegratorRow[] | null}) => ({value: result.rows}),
        stateFormsRender: (rows: {value: WorksheetIntegratorRow[]}) => WorksheetBindings.integratorPanel.renderIntegratorRows(rows.value),
        stateNeedsBounds: () => stateRuntime.sync(16, {}),
        stateResetBounds: () => WorksheetWorkspace.integrator.resetIntegratorBoundsToDefault(),
        stateMatrixOperation: () => statePolicy.matrixOperation(WorksheetContext.matrixOperation && WorksheetContext.matrixOperation.value),
        stateMatrixOperand: () => String(WorksheetContext.matrixOperand && WorksheetContext.matrixOperand.value || '').trim(),
        stateCurrentBounds: () => WorksheetWorkspace.integrator.currentIntegratorBoundsText(),
        stateIntervalCap: () => WorksheetWorkspace.integrator.requestedIntegratorIntervalCap(),
        stateRenderActiveRows: () => WorksheetBindings.integratorPanel.renderIntegratorRows(WorksheetWorkspace.integrator.activeIntegratorRows()),
        stateBoundCount: () => WorksheetWorkspace.integrator.activeIntegratorBoundRows().length,
        stateNormaliseCap: () => {
            if (WorksheetContext.integratorIntervalCap)
                WorksheetContext.integratorIntervalCap.value = String(statePolicy.intervalCap(WorksheetContext.integratorIntervalCap.value));
        },
        stateValidOperation: (value: unknown) => statePolicy.matrixOperation(value),
        stateValidCap: (value: unknown) => String(statePolicy.intervalCap(value)),
        stateMergeRows: (data: unknown) => labDOM.call<WorksheetIntegratorRow[]>('lab_binding_rows_merge', WorksheetWorkspace.integrator.currentIntegratorRows(), data),
        stateRenderRows: (rows: WorksheetIntegratorRow[]) => WorksheetBindings.integratorPanel.renderIntegratorRows(rows)
    }) satisfies WorksheetFormsCapabilities;

    /** @brief Result projection, preview ownership and deferred layout capabilities. */
    export const presentationCapabilities = Object.freeze({
        stateSaveResult: () => WorksheetWorkspace.presentation.saveCurrentModeResultState(),
        stateRestoreResult: () => WorksheetWorkspace.presentation.restoreModeResultState(WorksheetWorkspace.mode.currentMode()),
        stateClearFunction: () => WorksheetRequests.functionCard.clearFunctionRun(),
        stateResultProjection: (mode: WorksheetMode) => {
            labSurfacePreview.dispose();
            const state = WorksheetWorkspace.presentation.resultStates[mode];
            const result = labDOM.call<unknown>('lab_workspace_dom_result_restore', state);
            labSurfacePreview.restoreResult(state?.surfaceView ?? null);
            return result;
        },
        stateClearResult: () => WorksheetResults.reset.clearResultPane(),
        stateResultValues: (state: WorksheetResultValues) => {
            ({lastTex: WorksheetWorkspace.presentation.state.lastTex, lastDerivativeExpression: WorksheetWorkspace.presentation.state.lastDerivativeExpression, currentVariables: WorksheetBindings.panel.currentVariables, currentDifferentiable: WorksheetBindings.panel.currentDifferentiable, resultInputBindings: WorksheetWorkspace.presentation.state.resultInputBindings} = state);
        },
        stateDerivativeButtons: () => WorksheetWorkspace.presentation.renderDerivativeButtons(WorksheetBindings.panel.currentVariables),
        stateRenderedFit: () => WorksheetResults.renderedFit.schedule(),
        stateSolverFit: () => WorksheetResults.solverFit.schedule()
    }) satisfies WorksheetPresentationCapabilities;

    /** @brief Precision changes and evaluation commands; native plans retain sequencing. */
    export const evaluationCapabilities = Object.freeze({
        statePrecisionStep: (direction: number, bits?: number | null) => WorksheetWorkspace.precision.setRequestedPrecisionBits(
            bits ?? labWire.exports().lab_workspace_precision_step(WorksheetWorkspace.precision.requestedPrecisionBits(), direction)),
        statePrecisionPlan: (): WorksheetPrecisionPlan => {
            const source = WorksheetBindings.goal.currentGoalSeekSource(), target = WorksheetContext.labEditorState.goalTarget || '';
            return {
                source,
                target,
                action: labWire.exports().lab_events_precision(WorksheetWorkspace.mode.workspaceModeId(WorksheetWorkspace.mode.currentMode()), Number(!!source), Number(!!target))
            };
        },
        statePrecisionGoal: (plan: WorksheetPrecisionPlan) => WorksheetRequests.goals.runGoalSeek(
            plan.source, plan.target,
            WorksheetBindings.goal.solvedStartValuesForGoalSeek(plan.source, WorksheetContext.labEditorState.fullText || WorksheetBindings.editor.currentExpressionText()),
            {skipHistoryUpdate: true}),
        statePrecisionExpression: () => WorksheetRequests.evaluation.evaluateExpression({skipHistoryUpdate: true, reuseLastInput: true}),
        stateEvaluate: () => WorksheetWorkspace.mode.evaluateCurrentMode(),
        stateReady: () => WorksheetBindings.editor.expressionReadyToEvaluate(),
        stateEvaluateExpression: () => WorksheetRequests.evaluation.evaluateExpression(),
        stateEvaluateEquation: () => WorksheetRequests.evaluation.evaluateEquation(),
        stateEvaluateDiffequation: () => WorksheetRequests.evaluation.evaluateDiffequation(),
        stateEvaluateMatrix: () => WorksheetRequests.evaluation.evaluateMatrix(),
        stateEvaluateIntegrator: () => WorksheetRequests.evaluation.evaluateIntegrator(),
        stateEvaluateDatetime: () => WorksheetRequests.evaluation.evaluateDatetime()
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
