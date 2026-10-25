/**
 * @file state_types.d.ts
 * @brief Typed frames and browser capabilities for worksheet persistence continuations.
 *
 * These declaration-only contracts connect WorksheetPersistence to native state plans and the
 * request, workspace, binding, location and result modules. Input frames describe each operation's
 * initial arguments; C owns later continuation stages, validation and mathematical interpretation.
 * Explicit overloads preserve synchronous and asynchronous return contracts. Unknown values
 * denote native-owned payloads, not browser-side schemas or unchecked capability bags.
 * Capabilities that forward an owner's complete call shape retain typeof coupling;
 * their briefs describe inputs and results. Wrappers that deliberately omit owner
 * options declare their narrower signatures explicitly. State tables are checked
 * against these contracts with satisfies; this file emits no JavaScript or WASM.
 */

/** @brief Lazily obtain a saved field for native interpretation. */
interface WorksheetStateReader { (key: string): unknown; }

/** @brief Opaque saved values indexed by their native schema names. */
interface WorksheetStateRecord { [key: string]: unknown; }

/** @brief Browser timer callback with no payload. */
interface WorksheetStateCallback { (): void; }

/** @brief Publication operation supplied to a save scheduler. */
interface WorksheetStatePublisher { (save: WorksheetSavePlan): void; }

/** @brief Asynchronous history action owned by an evaluation request. */
interface WorksheetHistoryAction { (request: WorksheetRequest): Promise<void>; }

/** @brief Native storage-control field identifiers; their numeric values are part of the ABI. */
type WorksheetStorageField = 0 | 1 | 2 | 3 | 4;

/** @brief Local storage key exposed by the native persistence schema. */
interface WorksheetStateSchemaField { local: string; }

/** @brief Mode-bound access to history whose contents remain owned by the native workspace. */
interface WorksheetStateHistoryCursor {
    /** @brief Current native entry count for the bound history. */
    readonly length: number;

    /** @brief Retain a snapshot and return the native count, or throw on storage failure. */
    push(snapshot: WorksheetSnapshot): number;

    /** @brief Remove the top snapshot, returning null when no entry is available. */
    pop(): WorksheetSnapshot | null;
}

/** @brief Empty frame for operations with no initial arguments. */
interface WorksheetStateEmptyFrame { [field: string]: never; }

/** @brief Authored source and save options for one worksheet. */
interface WorksheetStateSaveFrame { index: number; text: string; options: WorksheetSaveOptions; }

/** @brief Native-selected deferred publication. */
interface WorksheetStatePublishFrame { save: WorksheetSavePlan; }

/** @brief Named worksheet mode supplied to native policy. */
interface WorksheetStateModeFrame { mode: WorksheetMode; }

/** @brief Native worksheet index. */
interface WorksheetStateIndexFrame { index: number; }

/** @brief Mode selection and projection options. */
interface WorksheetStateSelectionFrame { index: number; options: WorksheetModeOptions; }

/** @brief History capture with an optional authored-source override. */
interface WorksheetStateSnapshotFrame { index: number; textOverride: string | null; }

/** @brief Opaque native history entry. */
interface WorksheetStateEntryFrame { entry: unknown; }

/** @brief Controls and configuration supplied for native projection. */
interface WorksheetStateControlsFrame {
    index: number;
    controls: (HTMLElement | null | undefined)[];
    config: LabConfiguration;
}

/** @brief Opaque native restoration payload. */
interface WorksheetStateDataFrame { data: unknown; }

/** @brief Lazy storage reader and source selection for restoration. */
interface WorksheetStateReadFrame { read: WorksheetStateReader; local: boolean; }

/** @brief Guarded snapshot restoration. */
interface WorksheetStateHistoryFrame { state: WorksheetSnapshot | null; isCurrent: WorksheetGuard; }

/** @brief Navigation direction and request ownership. */
interface WorksheetStateNavigateFrame { direction: number; request: WorksheetRequest; }

/**
 * @brief Native execution seam with checked operation/frame/result overloads.
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
interface WorksheetStateRuntimePort {
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
}

/** @brief Browser timer resources supplied to one save scheduler. */
interface WorksheetSaveTimerPort {
    /** @brief Schedule a callback and return its browser timer handle. */
    schedule(callback: WorksheetStateCallback, delay: number): number;

    /** @brief Release a previously scheduled callback. */
    cancel(handle: number | undefined): void;
}

/** @brief Storage and wire capabilities used by one persistence owner. */
interface WorksheetPersistenceIO {
    /** @brief Read an encoded local record. */
    read(key: string): string | null;

    /** @brief Write an encoded local record, propagating storage failures. */
    write(key: string, value: string): void;

    /** @brief Encode a native payload. */
    encode(value: unknown): Uint8Array<ArrayBuffer>;

    /** @brief Encode a local record as bounded native base64/Protobuf text. */
    encodeStored(value: unknown): string;

    /** @brief Decode local storage text through native validation, throwing for corrupt or oversized records. */
    decodeStored(text: string): unknown;

    /** @brief Publish an already encoded record with optional unload-safe delivery. */
    post(bytes: Uint8Array<ArrayBuffer>, keepalive: boolean): Promise<unknown>;
}

/** @brief Live editor and precision inputs for persistence; no mutable context bag is exposed. */
interface WorksheetPersistenceInputs {
    /** @brief Read the selected worksheet mode. */
    mode(): WorksheetMode;

    /** @brief Resolve a mode to its native index. */
    modeIndex(mode: WorksheetMode): number;

    /** @brief Read the authored source at save invocation time. */
    source(): string;

    /** @brief Return the trusted native precision view for identity-based snapshotting. */
    precision(): typeof WorksheetContext.modePrecisionBits;
}

/** @brief Native validation and schema access; policy remains implemented by C. */
interface WorksheetStatePolicyPort {
    /** @brief Validate the interval budget against current native configuration. */
    intervalCap(value: unknown): number;

    /** @brief Validate the selected matrix operation. */
    matrixOperation(value: unknown): string;

    /** @brief Normalise a mode using the native mode catalogue. */
    mode(value: unknown): WorksheetMode;

    /** @brief Read native persistence field metadata. */
    schema(index: number): WorksheetStateSchemaField[];
}

/** @brief Native staging and history operations needed by a bound cursor. */
interface WorksheetHistoryCursorPort {
    /** @brief Read the native entry count. */
    count(index: number, store: number): number;

    /** @brief Retain staged snapshot bytes. */
    push(index: number, store: number, length: number, hasText: number, flags: number): number;

    /** @brief Remove a native entry and return its staged length. */
    pop(index: number, store: number): number;

    /** @brief Stage a snapshot in the selected native slot. */
    stage(snapshot: WorksheetSnapshot | null, slot?: number): number;

    /** @brief Decode the current native staging buffer. */
    read(length: number): WorksheetSnapshot | null;
}

/** @brief Native history operations and scoped snapshot buffers supplied to history owners. */
interface WorksheetHistoryPort {
    /** @brief Read a native history count. */
    count(index: number, store: number): number;

    /** @brief Retain staged bytes using the native history flags. */
    push(index: number, store: number, length: number, hasText: number, flags: number): number;

    /** @brief Remove a native entry and return its staged length. */
    pop(index: number, store: number): number;

    /** @brief Atomically navigate with the staged current snapshot. */
    navigate(index: number, direction: number, length: number, hasText: number): number;

    /** @brief Compare two native staging slots. */
    equal(leftLength: number, rightLength: number): number;

    /** @brief Select the predecessor for a staged snapshot. */
    previous(index: number, length: number): number;

    /** @brief Commit staged state, returning the native acceptance flag. */
    commit(index: number, length: number): number;

    /** @brief Clear a native history direction. */
    clear(index: number, store: number): void;

    /** @brief Stage a snapshot without retaining a browser copy of native history. */
    stage(snapshot: WorksheetSnapshot | null, slot?: number): number;

    /** @brief Decode the current native staging buffer. */
    read(length: number): WorksheetSnapshot | null;

    /** @brief Read the selected mode name. */
    mode(): WorksheetMode;

    /** @brief Resolve a mode name to a native index. */
    modeIndex(mode: WorksheetMode): number;

    /** @brief Read the native current index for atomic navigation. */
    currentIndex(): number;

    /** @brief Read the configured backward-history store identifier. */
    backwardStore(): number;

    /** @brief Read the configured forward-history store identifier. */
    forwardStore(): number;
}

/** @brief Request ownership required by history navigation, independent of the request implementation. */
interface WorksheetHistoryNavigationPort {
    /** @brief Read the selected mode before acquiring navigation ownership. */
    mode(): WorksheetMode;

    /** @brief Read the native history count before replacing any active request. */
    count(direction: number): number;

    /** @brief Run navigation inside an owned evaluation request. */
    run(mode: WorksheetMode, action: WorksheetHistoryAction): Promise<unknown>;
}

/** @brief Persistence capability subset registered with the native runtime. */
interface WorksheetPersistenceCapabilities {
    /** @brief Publish a WorksheetStateRecord patch with optional delivery options; snapshot shared precision and return void. */
    stateSave: typeof WorksheetPersistence.persistence.publish;

    /** @brief Save editor text for the supplied native mode index. */
    stateSaveEditor(index: number): void;

    /** @brief Save calendar controls for the supplied native mode index. */
    stateSaveCalendar(index: number): void;

    /** @brief Persist the currently selected worksheet mode. */
    stateSaveMode(): void;

    /** @brief Publish current precision settings with no arguments; return void. */
    stateSavePrecision: typeof WorksheetPersistence.persistence.savePrecision;

    /** @brief Build persistence records using native schema and precision settings. */
    stateRecords(index: number, values: unknown): unknown;

    /** @brief Write native-selected records to local storage. */
    stateLocalRecords(records: WorksheetStateRecord): void;

    /** @brief Cancel and remove a mode's pending save timer. */
    stateCancelSaveTimer(index: number): void;

    /** @brief Remove a completed timer from the scheduler without cancelling it. */
    stateForgetSaveTimer(index: number): boolean;

    /** @brief Schedule publication of the supplied native save plan. */
    stateScheduleSave(save: WorksheetSavePlan): Map<number, number>;

    /** @brief Execute native publication policy for a save plan. */
    statePublishSave(save: WorksheetSavePlan): void;

    /** @brief Encode an opaque state value and write it under the supplied storage key; return void or propagate write errors. */
    stateWriteCalendar: typeof WorksheetPersistence.persistence.writeLocal;

    /** @brief Build a native calendar patch with current precision settings. */
    stateCalendarPatch(mode: WorksheetMode, state: unknown): unknown;

    /** @brief Write the selected mode name to local storage. */
    stateWriteMode(mode: WorksheetMode): void;

    /** @brief Persist the current DateTime controls. */
    stateSaveDatetime(): void;

    /** @brief Persist the current almanac controls. */
    stateSaveAlmanac(): void;

    /**
     * @brief Save a named mode with optional authored text and keepalive/debounce options; omitted text uses live
     * source, returning void.
     */
    stateSaveWorksheet: typeof WorksheetPersistence.persistence.saveWorksheet;

    /** @brief Save the editor identified by its native mode index. */
    stateSaveNamedEditor(index: number): void;

    /** @brief Save current worksheet text with unload-safe delivery. */
    stateFlushSave(mode: WorksheetMode): void;
}

/** @brief Restoration capability subset registered with the native runtime. */
interface WorksheetRestorationCapabilities {
    /** @brief Read a saved field through the supplied lazy reader. */
    stateRead(read: WorksheetStateReader, key: string): unknown;

    /** @brief Read the current epoch timestamp in milliseconds. */
    stateNow(): number;

    /** @brief Accept authored text and return a promise for native WorksheetEditorMetadata. */
    statePrepare: typeof WorksheetRequests.presentation.prepareLabEditor;

    /** @brief Accept expression text and return a string with constants ordered by native policy. */
    stateCanonical: typeof WorksheetBindings.editor.expressionWithSortedConstants;

    /** @brief Normalise a saved control value using native storage policy. */
    stateControl(field: WorksheetStorageField, value: unknown, local: boolean): unknown;

    /** @brief Accept bounds text and an optional ownership guard; return a promise reporting whether restoration completed. */
    stateBounds: typeof WorksheetWorkspace.integrator.restoreIntegratorBoundsText;

    /** @brief Restore editor fields through lazy reads of a server record. */
    stateServerEditors(data: WorksheetStateRecord): Promise<void>;

    /** @brief Restore control fields through lazy reads of a server record. */
    stateServerControls(data: WorksheetStateRecord): Promise<void>;

    /**
     * @brief Apply opaque source to a named mode with optional operation (default 0) and prefix (default empty);
     * return WorksheetCalendarState.
     */
    stateCalendar: typeof WorksheetLocations.calendar.applyCalendarState;

    /** @brief Synchronise town menus without selecting default towns. */
    stateSyncTowns(): Promise<void>;

    /** @brief Restore a calendar's town using its current jurisdiction and coordinates. */
    stateTown(index: number, town: string | undefined): Promise<boolean | null>;

    /** @brief Normalise a saved mode through the native mode catalogue. */
    stateMode(value: unknown): WorksheetMode;

    /** @brief Prepare metadata for a mode's retained editor text. */
    statePrepareMode(mode: WorksheetMode): Promise<WorksheetEditorMetadata>;

    /** @brief Apply the supplied worksheet mode through the workspace controller; return void. */
    stateApplyMode: typeof WorksheetWorkspace.mode.applyLabMode;

    /** @brief Build a native recovery plan from server data and local editor records. */
    stateRecovery(index: number, data: unknown): unknown;

    /** @brief Build the native recovered record from accepted editor text and timestamp. */
    stateRecovered(index: number, recovery: unknown, text: string, now: number): unknown;

    /** @brief Restore retained editor text for the supplied worksheet mode; return void. */
    stateRestoreEditor: typeof WorksheetWorkspace.mode.restoreModeEditor;

    /** @brief Fetch the server's saved worksheet state. */
    stateFetch(): Promise<WorksheetResponse>;

    /** @brief Decode the saved-state response without interpreting its schema. */
    stateDecode(response: WorksheetResponse): Promise<unknown>;

    /** @brief Accept an opaque server payload and return a promise completing native restoration. */
    stateApplySaved: typeof WorksheetPersistence.restoration.applySaved;

    /** @brief Accept opaque server data and return a promise completing newer-local-editor recovery. */
    stateRecover: typeof WorksheetPersistence.restoration.recoverLocalEditors;

    /** @brief Restore editor fields through lazy local-storage reads. */
    stateLocalEditors(): Promise<void>;

    /** @brief Restore control fields through lazy local-storage reads. */
    stateLocalControls(): Promise<void>;

    /** @brief Accept a storage key and return the decoded opaque fallback, or null for missing, invalid or inaccessible data. */
    stateLocalCalendar: typeof WorksheetPersistence.persistence.readLocal;

    /** @brief Read the locally retained worksheet mode. */
    stateLocalMode(): string | null;

    /** @brief Invoke the supplied restoration ownership guard. */
    stateGuard(guard: WorksheetGuard): boolean;

    /** @brief Capture DateTime controls with no arguments and return native-normalised WorksheetCalendarState. */
    stateDatetime: typeof WorksheetLocations.calendar.currentDatetimeState;

    /** @brief Capture almanac controls with no arguments and return native-normalised WorksheetCalendarState. */
    stateAlmanac: typeof WorksheetLocations.calendar.currentAlmanacState;

    /** @brief Obtain native summary text for DateTime state. */
    stateDatetimeSummary(state: WorksheetSnapshot): string;

    /** @brief Obtain native summary text for almanac state. */
    stateAlmanacSummary(state: WorksheetSnapshot): string;
}

/** @brief History capability subset registered with the native runtime. */
interface WorksheetHistoryCapabilities {
    /** @brief Build a native history plan for the requested restoration phase. */
    stateHistoryPlan(phase: number, state: WorksheetSnapshot): unknown;

    /** @brief Restore a mode's opaque source with an optional ownership guard; return a promise completing restoration. */
    stateCalendarHistory: typeof WorksheetLocations.calendarActions.restoreCalendarHistory;

    /** @brief Navigate native history using the current snapshot. */
    stateNavigate(direction: number): WorksheetSnapshot | null;

    /** @brief Refresh history navigation button availability. */
    stateHistoryButtons: typeof WorksheetWorkspace.presentation.updateHistoryButtons;

    /** @brief Restore a snapshot while its request remains current. */
    stateRestoreHistory(state: WorksheetSnapshot, request: WorksheetRequest): Promise<boolean>;

    /** @brief Evaluate restored state without adding another history entry. */
    stateEvaluateHistory(): Promise<void>;

    /** @brief Clear the current worksheet's native forward history. */
    stateClearForward(): void;

    /** @brief Read the native-validated interval budget as snapshot text. */
    stateHistoryCap(): string;

    /** @brief Build a native history snapshot from editor and control values. */
    stateHistoryRecord(index: number, text: string, calendar: unknown, bounds: string, cap: string): WorksheetSnapshot;

    /** @brief Capture current history with an optional authored-text override. */
    stateCurrentHistory(textOverride?: string | null): WorksheetSnapshot;

    /** @brief Stage and retain a snapshot, propagating native history rejection. */
    statePushSnapshot(snapshot: WorksheetSnapshot): void;
}

/** @brief Editor capability subset registered with the native runtime. */
interface WorksheetEditorCapabilities {
    /** @brief Replace retained editor text for a native mode index. */
    stateEditor(index: number, text: string): string;

    /** @brief Update the expression timestamp and editor text. */
    stateExpression(text: string, timestamp: number): void;

    /** @brief Update and return the retained expression timestamp. */
    stateTimestamp(timestamp: number): number;

    /** @brief Set expression editor text through the binding controller. */
    stateSetExpression(text: string): void;

    /** @brief Retain the integrator's authored source on its editor element. */
    stateIntegratorSource(text: string): string;

    /** @brief Apply the supplied updated expression text through the binding controller; return void. */
    stateUpdated: typeof WorksheetBindings.controller.applyUpdatedBindingExpression;

    /** @brief Clear the binding controller's retained expression source. */
    stateClearSource: typeof WorksheetBindings.controller.clearExpressionSource;

    /** @brief Clear displayed variable binding values. */
    stateClearBindings: typeof WorksheetBindings.panel.clearVariableValues;

    /** @brief Commit visible inputs with an optional ownership guard; return a promise of the boolean outcome or undefined. */
    stateCommitBindings: typeof WorksheetBindings.controller.commitVisibleBindingInputs;

    /** @brief Cancel the binding refresh timer and its pending request. */
    stateCancelBindings(): void;

    /** @brief Capture an editor guard combining caller ownership, mode and context. */
    stateCaptureGuard(isCurrent: WorksheetGuard): WorksheetGuard;

    /** @brief Stage current editor text and its default for native capture. */
    stateCaptureEditor(): number;

    /** @brief Capture current editor state while the supplied request remains current. */
    stateCaptureRequest(request: WorksheetRequest): Promise<boolean>;

    /** @brief Capture staged editor text, propagating native rejection. */
    stateRetainText(index: number, text: string): void;

    /** @brief Replace retained mode text with its native default. */
    stateDefaultEditor(mode: WorksheetMode, index: number): string;

    /** @brief Restore and read native editor text with its default fallback. */
    stateRestoredText(index: number): string;

    /** @brief Accept expression text and return WorksheetEditorMetadata, or null when metadata is unavailable. */
    stateBindingParts: typeof WorksheetBindings.editor.bindingParts;

    /** @brief Read current expression text with the visible editor as fallback. */
    stateEditorText(): string;

    /** @brief Read the retained expression-mode editor text. */
    stateExpressionSavedText(): string;
}

/** @brief Workspace capability subset registered with the native runtime. */
interface WorksheetWorkspaceCapabilities {
    /** @brief Cancel pending binding refresh and evaluation, binding and function requests. */
    stateClearPrelude(): void;

    /** @brief Continue native clearing within an owned evaluation request. */
    stateClearOwned(): Promise<unknown>;

    /** @brief Apply the native clear action for the current worksheet. */
    stateClearProjection(): number;

    /** @brief Reset current calendar controls through native state policy. */
    stateResetCalendar(): WorksheetCalendarState;

    /** @brief Read the current worksheet's native mode index. */
    stateModeIndex(): number;

    /** @brief Select a worksheet mode through the workspace controller. */
    stateSetMode(mode: WorksheetMode): boolean;

    /** @brief Capture a guard comparing request context and latest main-request token. */
    stateSelectionGuard(): WorksheetGuard;

    /** @brief Hide the goal-seeking target entry. */
    stateHideTarget: typeof WorksheetWorkspace.presentation.hideTargetEntry;

    /** @brief Restore retained editor text for the current worksheet. */
    stateRestoreCurrentEditor(): void;

    /** @brief Synchronise controls with the selected worksheet mode. */
    stateSyncUI: typeof WorksheetWorkspace.presentation.syncModeUI;

    /** @brief Normalise the displayed interval budget and focus the editor. */
    stateFinishIntegrator(): void;

    /** @brief Fill blank DateTime defaults and focus its date input. */
    stateFinishDatetime(): void;

    /** @brief Fill blank almanac defaults and focus its date input. */
    stateFinishAlmanac(): void;

    /** @brief Focus the worksheet source editor. */
    stateFocusEditor(): void;

    /** @brief Present the supplied status text; return void. */
    stateStatus: typeof WorksheetWorkspace.presentation.setStatus;

    /** @brief Notify the request manager of the current worksheet mode. */
    stateNotifyMode(): void;

    /** @brief Clear the editor's manually selected size. */
    stateResetEditorSize: typeof WorksheetWorkspace.layout.resetEditorManualSize;

    /** @brief Synchronise worksheet mode tabs. */
    stateSyncTabs: typeof WorksheetWorkspace.presentation.syncModeTabs;

    /** @brief Normalise and forcibly select a worksheet mode. */
    stateForceMode(mode: WorksheetMode): boolean;

    /** @brief Apply native DateTime defaults only to blank controls. */
    stateDatetimeDefaults: typeof WorksheetLocations.calendar.restoreDatetimeDefaultsIfBlank;

    /** @brief Apply native almanac defaults only to blank controls. */
    stateAlmanacDefaults: typeof WorksheetLocations.calendar.restoreAlmanacDefaultsIfBlank;

    /** @brief Refresh DateTime location, then continue native state projection. */
    stateRefreshLocation(): void;

    /** @brief Resolve a worksheet mode name to its native index. */
    stateNamedIndex(mode: WorksheetMode): number;

    /** @brief Synchronise matrix controls with the selected operation. */
    stateSyncMatrix: typeof WorksheetWorkspace.presentation.syncMatrixControls;
}

/** @brief Forms capability subset registered with the native runtime. */
interface WorksheetFormsCapabilities {
    /** @brief Replace integrator bounds with blank rows. */
    stateBlankBounds: typeof WorksheetWorkspace.integrator.resetIntegratorBoundsToBlank;

    /** @brief Request native form processing for an opaque payload. */
    stateFormsRequest(payload: unknown): Promise<unknown>;

    /** @brief Read integrator preparation text with no arguments and return a string. */
    stateFormsExpression: typeof WorksheetWorkspace.integrator.integratorFormsExpression;

    /** @brief Install metadata from an opaque expression value and WorksheetEvaluationData response; return void. */
    stateFormsMetadata: typeof WorksheetWorkspace.integrator.forms.install;

    /** @brief Capture the rows property once, preserving null and undefined distinctly. */
    stateFormsRows(result: {rows?: WorksheetIntegratorRow[] | null}): {value: WorksheetIntegratorRow[] | null | undefined};

    /** @brief Render integrator rows from the supplied captured value. */
    stateFormsRender(rows: {value: WorksheetIntegratorRow[]}): void;

    /** @brief Ask native state policy whether integrator bounds are needed. */
    stateNeedsBounds(): boolean;

    /** @brief Restore default integrator bounds with no arguments; return a promise reporting completion. */
    stateResetBounds: typeof WorksheetWorkspace.integrator.resetIntegratorBoundsToDefault;

    /** @brief Read and natively validate the selected matrix operation. */
    stateMatrixOperation(): string;

    /** @brief Read trimmed matrix operand text. */
    stateMatrixOperand(): string;

    /** @brief Read the current integrator bounds with no arguments and return their text. */
    stateCurrentBounds: typeof WorksheetWorkspace.integrator.currentIntegratorBoundsText;

    /** @brief Read the requested interval budget with no arguments and return a native-validated number. */
    stateIntervalCap: typeof WorksheetWorkspace.integrator.requestedIntegratorIntervalCap;

    /** @brief Render the current integrator rows. */
    stateRenderActiveRows(): void;

    /** @brief Count active bound rows in the integrator. */
    stateBoundCount(): number;

    /** @brief Replace the displayed interval budget with its native-normalised value. */
    stateNormaliseCap(): void;

    /** @brief Accept an opaque matrix operation value and return its native-normalised string. */
    stateValidOperation: typeof WorksheetPersistence.statePolicy.matrixOperation;

    /** @brief Return a native-normalised interval budget as text. */
    stateValidCap(value: unknown): string;

    /** @brief Merge native response data into the current integrator rows. */
    stateMergeRows(data: unknown): WorksheetIntegratorRow[];

    /** @brief Render the supplied WorksheetIntegratorRow array; return void. */
    stateRenderRows: typeof WorksheetBindings.integratorPanel.renderIntegratorRows;
}

/** @brief Presentation capability subset registered with the native runtime. */
interface WorksheetPresentationCapabilities {
    /** @brief Capture the current mode's result presentation. */
    stateSaveResult(): void;

    /** @brief Restore the current mode's retained result presentation. */
    stateRestoreResult(): void;

    /** @brief Clear the retained function-card run. */
    stateClearFunction: typeof WorksheetRequests.functionCard.clearFunctionRun;

    /** @brief Restore native result content and its associated surface preview. */
    stateResultProjection(mode: WorksheetMode): unknown;

    /** @brief Clear the result pane through its presentation owner. */
    stateClearResult: typeof WorksheetResults.reset.clearResultPane;

    /** @brief Restore retained TeX, derivative and binding presentation fields. */
    stateResultValues(state: WorksheetResultValues): void;

    /** @brief Refresh derivative buttons for the current variables. */
    stateDerivativeButtons(): void;

    /** @brief Schedule rendered-result layout fitting. */
    stateRenderedFit(): void;

    /** @brief Schedule solver-result layout fitting. */
    stateSolverFit(): void;
}

/** @brief Evaluation capability subset registered with the native runtime. */
interface WorksheetEvaluationCapabilities {
    /** @brief Apply explicit precision bits or a native-selected precision step. */
    statePrecisionStep(direction: number, bits?: number | null): void;

    /** @brief Capture goal inputs and the native-selected precision action. */
    statePrecisionPlan(): WorksheetPrecisionPlan;

    /** @brief Rerun goal seeking at the new precision without adding history. */
    statePrecisionGoal(plan: WorksheetPrecisionPlan): Promise<unknown> | undefined;

    /** @brief Re-evaluate the last expression input without adding history. */
    statePrecisionExpression(): Promise<unknown> | undefined;

    /** @brief Evaluate the currently selected worksheet mode. */
    stateEvaluate(): Promise<void>;

    /** @brief Report whether the expression is ready for evaluation. */
    stateReady: typeof WorksheetBindings.editor.expressionReadyToEvaluate;

    /** @brief Evaluate the current expression worksheet. */
    stateEvaluateExpression(): Promise<unknown> | undefined;

    /** @brief Evaluate the current equation worksheet. */
    stateEvaluateEquation(): Promise<unknown> | undefined;

    /** @brief Evaluate the current differential-equation worksheet. */
    stateEvaluateDiffequation(): Promise<unknown> | undefined;

    /** @brief Evaluate the current matrix worksheet. */
    stateEvaluateMatrix(): Promise<unknown> | undefined;

    /** @brief Evaluate the current integrator worksheet. */
    stateEvaluateIntegrator(): Promise<unknown> | undefined;

    /** @brief Evaluate the current DateTime worksheet. */
    stateEvaluateDatetime(): Promise<unknown> | undefined;
}

/** @brief Complete native service contract assembled from domain-owned capabilities. */
interface WorksheetStateFlowServices extends
    WorksheetPersistenceCapabilities,
    WorksheetRestorationCapabilities,
    WorksheetHistoryCapabilities,
    WorksheetEditorCapabilities,
    WorksheetWorkspaceCapabilities,
    WorksheetFormsCapabilities,
    WorksheetPresentationCapabilities,
    WorksheetEvaluationCapabilities {}
