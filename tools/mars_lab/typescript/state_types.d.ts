/**
 * @file state_types.d.ts
 * @brief Typed frames and browser capabilities for worksheet persistence continuations.
 *
 * These declaration-only contracts connect WorksheetPersistence to native state plans and the
 * request, workspace, binding, location and result modules. Input frames describe each operation's
 * initial arguments; C owns later continuation stages, validation and mathematical interpretation.
 * Conditional result types preserve synchronous and asynchronous return contracts. Unknown values
 * denote native-owned payloads, not browser-side schemas or unchecked capability bags.
 */
/** @brief Lazily obtain a saved field for native interpretation. */
type WorksheetStateReader = (key: string) => unknown;
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

/** Input frames are allocated per invocation; native continuations own their subsequent stages. */
interface WorksheetStateSyncFrames {
    0: {index: number; text: string; options: WorksheetSaveOptions};
    1: {save: WorksheetSavePlan};
    2: {mode: WorksheetMode};
    3: {mode: WorksheetMode};
    4: {index: number};
    5: {mode: WorksheetMode};
    6: {index: number; options: WorksheetModeOptions};
    7: {mode: WorksheetMode};
    8: Record<string, never>;
    9: {index: number; textOverride: string | null};
    10: {entry: unknown};
    11: {index: number; controls: (HTMLElement | null | undefined)[]; config: LabConfiguration};
    12: Record<string, never>;
    13: Record<string, never>;
    14: {data: unknown};
    15: {index: number};
    16: Record<string, never>;
}
/** @brief Map each synchronous operation to its native completion value. */
type WorksheetStateSyncResult<K extends keyof WorksheetStateSyncFrames> =
    K extends 6 | 16 ? boolean : K extends 9 ? WorksheetSnapshot : void;

/** @brief Initial frames for asynchronous restoration, recovery and history navigation. */
interface WorksheetStateRestoreFrames {
    16: {read: WorksheetStateReader; local: boolean};
    17: {read: WorksheetStateReader; local: boolean};
    18: {data: unknown};
    19: {data: unknown};
    20: Record<string, never>;
    21: {state: WorksheetSnapshot | null; isCurrent: WorksheetGuard};
    22: {direction: number; request: WorksheetRequest};
}
/** @brief Report history restoration success while other restoration operations complete without a value. */
type WorksheetStateRestoreResult<K extends keyof WorksheetStateRestoreFrames> = K extends 21 ? boolean : void;

/** @brief Typed native execution seam; the composition root supplies the current runtime. */
interface WorksheetStateRuntimePort {
    /** @brief Execute a synchronous native operation with its matching frame. */
    sync<K extends keyof WorksheetStateSyncFrames>(kind: K, frame: WorksheetStateSyncFrames[K]): WorksheetStateSyncResult<K>;
    /** @brief Await native restoration without moving its sequencing policy into the browser. */
    restore<K extends keyof WorksheetStateRestoreFrames>(kind: K, frame: WorksheetStateRestoreFrames[K]): Promise<WorksheetStateRestoreResult<K>>;
}

/** @brief Browser timer resources supplied to one save scheduler. */
interface WorksheetSaveTimerPort {
    /** @brief Schedule a callback and return its browser timer handle. */
    schedule(callback: () => void, delay: number): number;
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
    /** @brief Decode a native payload without interpreting its schema. */
    decode(bytes: ArrayBuffer): unknown;
    /** @brief Obtain the native maximum base64 record length. */
    base64Limit(): number;
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
    /** @brief Run navigation inside an owned evaluation request. */
    run(mode: WorksheetMode, action: (request: WorksheetRequest) => Promise<void>): Promise<unknown>;
}

/** @brief Browser capabilities consumed by synchronous native persistence and projection plans. */
interface WorksheetStateSyncServices {
    stateRetainText(index: number, text: string): void;
    stateMatrixOperation(): string;
    stateMatrixOperand(): string;
    stateCurrentBounds: typeof WorksheetWorkspace.integrator.currentIntegratorBoundsText;
    stateIntervalCap: typeof WorksheetWorkspace.integrator.requestedIntegratorIntervalCap;
    stateRecords(index: number, values: unknown): unknown;
    stateLocalRecords(records: Record<string, unknown>): void;
    stateCancelSaveTimer(index: number): void;
    stateForgetSaveTimer(index: number): boolean;
    stateScheduleSave(save: WorksheetSavePlan): Map<number, number>;
    statePublishSave(save: WorksheetSavePlan): void;
    stateDatetime: typeof WorksheetLocations.calendar.currentDatetimeState;
    stateAlmanac: typeof WorksheetLocations.calendar.currentAlmanacState;
    stateDefaultEditor(mode: WorksheetMode, index: number): string;
    stateWriteCalendar: typeof WorksheetPersistence.persistence.writeLocal;
    stateCalendarPatch(mode: WorksheetMode, state: unknown): unknown;
    stateWriteMode(mode: WorksheetMode): void;
    stateRestoredText(index: number): string;
    stateBindingParts: typeof WorksheetBindings.editor.bindingParts;
    stateClearFunction: typeof WorksheetRequests.functionCard.clearFunctionRun;
    stateResultProjection(mode: WorksheetMode): unknown;
    stateClearResult: typeof WorksheetResults.reset.clearResultPane;
    stateResultValues(state: WorksheetResultValues): void;
    stateDerivativeButtons(): ReturnType<typeof WorksheetWorkspace.presentation.renderDerivativeButtons>;
    stateRenderedFit: typeof WorksheetResults.renderedFit.schedule;
    stateSolverFit: typeof WorksheetResults.solverFit.schedule;
    stateNotifyMode(): ReturnType<typeof WorksheetRequests.labRequests.modeChanged>;
    stateResetEditorSize: typeof WorksheetWorkspace.layout.resetEditorManualSize;
    stateSyncTabs: typeof WorksheetWorkspace.presentation.syncModeTabs;
    stateForceMode(mode: WorksheetMode): ReturnType<typeof WorksheetWorkspace.mode.setMode>;
    stateRenderActiveRows(): ReturnType<typeof WorksheetBindings.integratorPanel.renderIntegratorRows>;
    stateDatetimeDefaults: typeof WorksheetLocations.calendar.restoreDatetimeDefaultsIfBlank;
    stateAlmanacDefaults: typeof WorksheetLocations.calendar.restoreAlmanacDefaultsIfBlank;
    stateRefreshLocation(): void;
    stateSaveDatetime: () => void;
    stateSaveAlmanac: () => void;
    stateBoundCount(): number;
    stateNormaliseCap(): void;
    stateReady: typeof WorksheetBindings.editor.expressionReadyToEvaluate;
    stateClearForward(): void;
    stateEvaluateExpression: typeof WorksheetRequests.evaluation.evaluateExpression;
    stateEvaluateEquation: typeof WorksheetRequests.evaluation.evaluateEquation;
    stateEvaluateDiffequation: typeof WorksheetRequests.evaluation.evaluateDiffequation;
    stateEvaluateMatrix: typeof WorksheetRequests.evaluation.evaluateMatrix;
    stateEvaluateIntegrator: typeof WorksheetRequests.evaluation.evaluateIntegrator;
    stateEvaluateDatetime: typeof WorksheetRequests.evaluation.evaluateDatetime;
    stateDatetimeSummary(state: WorksheetSnapshot): ReturnType<typeof WorksheetLocations.calendar.datetimeSummaryText>;
    stateAlmanacSummary(state: WorksheetSnapshot): ReturnType<typeof WorksheetLocations.calendar.almanacSummaryText>;
    stateEditorText(): string;
    stateHistoryCap(): string;
    stateHistoryRecord(index: number, text: string, calendar: unknown, bounds: string, cap: string): WorksheetSnapshot;
    stateCurrentHistory(textOverride?: string | null): WorksheetSnapshot;
    statePushSnapshot(snapshot: WorksheetSnapshot): void;
    stateValidOperation: typeof WorksheetPersistence.statePolicy.matrixOperation;
    stateValidCap(value: unknown): string;
    stateSyncMatrix: typeof WorksheetWorkspace.presentation.syncMatrixControls;
    stateSaveWorksheet: typeof WorksheetPersistence.persistence.saveWorksheet;
    stateSaveNamedEditor(index: number): void;
    stateExpressionSavedText(): string;
    stateNamedIndex(mode: WorksheetMode): number;
    stateFlushSave(mode: WorksheetMode): void;
    stateMergeRows(data: unknown): WorksheetIntegratorRow[];
    stateRenderRows: typeof WorksheetBindings.integratorPanel.renderIntegratorRows;
}

/** @brief Late-bound capabilities for guarded restoration, evaluation and recovery continuations. */
interface WorksheetStateFlowServices extends WorksheetStateSyncServices {
    stateClearPrelude(): void;
    stateClearOwned(): Promise<unknown>;
    stateClearProjection(): number;
    stateBlankBounds: typeof WorksheetWorkspace.integrator.resetIntegratorBoundsToBlank;
    stateResetCalendar(): ReturnType<typeof WorksheetLocations.calendar.applyCalendarState>;
    stateFormsRequest: typeof WorksheetRequests.presentation.requestLabForms;
    stateFormsExpression: typeof WorksheetWorkspace.integrator.integratorFormsExpression;
    stateFormsMetadata: typeof WorksheetWorkspace.integrator.forms.install;
    stateFormsRows(result: {rows?: WorksheetIntegratorRow[] | null}): {value: WorksheetIntegratorRow[] | null | undefined};
    stateFormsRender(rows: {value: WorksheetIntegratorRow[]}): ReturnType<typeof WorksheetBindings.integratorPanel.renderIntegratorRows>;
    stateRead(read: WorksheetStateReader, key: string): unknown;
    stateNow(): number;
    statePrepare: typeof WorksheetRequests.presentation.prepareLabEditor;
    stateCanonical: typeof WorksheetBindings.editor.expressionWithSortedConstants;
    stateEditor(index: number, text: string): string;
    stateExpression(text: string, timestamp: number): void;
    stateTimestamp(timestamp: number): number;
    stateSetExpression: typeof WorksheetBindings.controller.setExpressionEditor;
    stateIntegratorSource(text: string): string;
    stateControl(field: WorksheetStorageField, value: unknown, local: boolean): unknown;
    stateBounds: typeof WorksheetWorkspace.integrator.restoreIntegratorBoundsText;
    stateServerEditors(data: Record<string, unknown>): Promise<void>;
    stateServerControls(data: Record<string, unknown>): Promise<void>;
    stateCalendar: typeof WorksheetLocations.calendar.applyCalendarState;
    stateSyncTowns(): ReturnType<typeof WorksheetLocations.townSelection.syncTownSelectors>;
    stateTown(index: number, town: string | undefined): ReturnType<typeof WorksheetLocations.townSelection.restoreTownSelection>;
    stateMode(value: unknown): WorksheetMode;
    statePrepareMode(mode: WorksheetMode): ReturnType<typeof WorksheetRequests.presentation.prepareLabEditor>;
    stateApplyMode: typeof WorksheetWorkspace.mode.applyLabMode;
    stateRecovery(index: number, data: unknown): unknown;
    stateRecovered(index: number, recovery: unknown, text: string, now: number): unknown;
    stateModeIndex(): number;
    stateRestoreEditor: typeof WorksheetWorkspace.mode.restoreModeEditor;
    stateSave: typeof WorksheetPersistence.persistence.publish;
    stateFetch(): ReturnType<typeof labHttp.fetch>;
    stateDecode(response: WorksheetResponse): ReturnType<WorksheetResponse['labData']>;
    stateApplySaved: typeof WorksheetPersistence.restoration.applySaved;
    stateRecover: typeof WorksheetPersistence.restoration.recoverLocalEditors;
    stateLocalEditors(): Promise<void>;
    stateLocalControls(): Promise<void>;
    stateLocalCalendar: typeof WorksheetPersistence.persistence.readLocal;
    stateLocalMode(): string | null;
    stateGuard(guard: WorksheetGuard): boolean;
    stateHistoryPlan(phase: number, state: WorksheetSnapshot): unknown;
    stateCalendarHistory: typeof WorksheetLocations.calendarActions.restoreCalendarHistory;
    stateUpdated: typeof WorksheetBindings.controller.applyUpdatedBindingExpression;
    stateClearSource: typeof WorksheetBindings.controller.clearExpressionSource;
    stateClearBindings: typeof WorksheetBindings.panel.clearVariableValues;
    stateCommitBindings: typeof WorksheetBindings.controller.commitVisibleBindingInputs;
    stateNavigate(direction: number): WorksheetSnapshot | null;
    stateHistoryButtons: typeof WorksheetWorkspace.presentation.updateHistoryButtons;
    stateCancelBindings(): void;
    stateRestoreHistory(state: WorksheetSnapshot, request: WorksheetRequest): Promise<boolean>;
    stateEvaluateHistory(): ReturnType<typeof WorksheetWorkspace.mode.evaluateCurrentMode>;
    stateCaptureGuard(isCurrent: WorksheetGuard): WorksheetGuard;
    stateCaptureEditor(): number;
    stateSaveEditor(index: number): void;
    stateSaveCalendar(index: number): void;
    stateCaptureRequest(request: WorksheetRequest): ReturnType<typeof WorksheetWorkspace.mode.captureCurrentModeEditor>;
    stateSaveResult: typeof WorksheetWorkspace.presentation.saveCurrentModeResultState;
    stateSetMode: typeof WorksheetWorkspace.mode.setMode;
    stateSelectionGuard(): WorksheetGuard;
    stateSaveMode(): void;
    stateHideTarget: typeof WorksheetWorkspace.presentation.hideTargetEntry;
    stateRestoreCurrentEditor(): ReturnType<typeof WorksheetWorkspace.mode.restoreModeEditor>;
    stateSyncUI: typeof WorksheetWorkspace.presentation.syncModeUI;
    stateRestoreResult(): ReturnType<typeof WorksheetWorkspace.presentation.restoreModeResultState>;
    stateNeedsBounds(): boolean;
    stateResetBounds: typeof WorksheetWorkspace.integrator.resetIntegratorBoundsToDefault;
    stateFinishIntegrator(): void;
    stateFinishDatetime(): void;
    stateFinishAlmanac(): void;
    stateFocusEditor(): void;
    stateStatus: typeof WorksheetWorkspace.presentation.setStatus;
    statePrecisionStep(direction: number, bits?: number | null): ReturnType<typeof WorksheetWorkspace.precision.setRequestedPrecisionBits>;
    stateSavePrecision: typeof WorksheetPersistence.persistence.savePrecision;
    statePrecisionPlan(): WorksheetPrecisionPlan;
    statePrecisionGoal(plan: WorksheetPrecisionPlan): ReturnType<typeof WorksheetRequests.goals.runGoalSeek>;
    statePrecisionExpression(): ReturnType<typeof WorksheetRequests.evaluation.evaluateExpression>;
    stateEvaluate(): ReturnType<typeof WorksheetWorkspace.mode.evaluateCurrentMode>;
}

/** @brief Persistence capability subset registered with the native runtime. */
type WorksheetPersistenceCapabilities = Pick<WorksheetStateFlowServices,
    'stateSave' | 'stateSaveEditor' | 'stateSaveCalendar' | 'stateSaveMode' | 'stateSavePrecision' | 'stateRecords'
    | 'stateLocalRecords' | 'stateCancelSaveTimer' | 'stateForgetSaveTimer' | 'stateScheduleSave'
    | 'statePublishSave' | 'stateWriteCalendar' | 'stateCalendarPatch' | 'stateWriteMode' | 'stateSaveDatetime'
    | 'stateSaveAlmanac' | 'stateSaveWorksheet' | 'stateSaveNamedEditor' | 'stateFlushSave'>;

/** @brief Restoration capability subset registered with the native runtime. */
type WorksheetRestorationCapabilities = Pick<WorksheetStateFlowServices,
    'stateRead' | 'stateNow' | 'statePrepare' | 'stateCanonical' | 'stateControl' | 'stateBounds'
    | 'stateServerEditors' | 'stateServerControls' | 'stateCalendar' | 'stateSyncTowns' | 'stateTown' | 'stateMode'
    | 'statePrepareMode' | 'stateApplyMode' | 'stateRecovery' | 'stateRecovered' | 'stateRestoreEditor'
    | 'stateFetch' | 'stateDecode' | 'stateApplySaved' | 'stateRecover' | 'stateLocalEditors'
    | 'stateLocalControls' | 'stateLocalCalendar' | 'stateLocalMode' | 'stateGuard' | 'stateDatetime'
    | 'stateAlmanac' | 'stateDatetimeSummary' | 'stateAlmanacSummary'>;

/** @brief History capability subset registered with the native runtime. */
type WorksheetHistoryCapabilities = Pick<WorksheetStateFlowServices,
    'stateHistoryPlan' | 'stateCalendarHistory' | 'stateNavigate' | 'stateHistoryButtons' | 'stateRestoreHistory'
    | 'stateEvaluateHistory' | 'stateClearForward' | 'stateHistoryCap' | 'stateHistoryRecord'
    | 'stateCurrentHistory' | 'statePushSnapshot'>;

/** @brief Editor capability subset registered with the native runtime. */
type WorksheetEditorCapabilities = Pick<WorksheetStateFlowServices,
    'stateEditor' | 'stateExpression' | 'stateTimestamp' | 'stateSetExpression' | 'stateIntegratorSource'
    | 'stateUpdated' | 'stateClearSource' | 'stateClearBindings' | 'stateCommitBindings' | 'stateCancelBindings'
    | 'stateCaptureGuard' | 'stateCaptureEditor' | 'stateCaptureRequest' | 'stateRetainText' | 'stateDefaultEditor'
    | 'stateRestoredText' | 'stateBindingParts' | 'stateEditorText' | 'stateExpressionSavedText'>;

/** @brief Workspace capability subset registered with the native runtime. */
type WorksheetWorkspaceCapabilities = Pick<WorksheetStateFlowServices,
    'stateClearPrelude' | 'stateClearOwned' | 'stateClearProjection' | 'stateResetCalendar' | 'stateModeIndex'
    | 'stateSetMode' | 'stateSelectionGuard' | 'stateHideTarget' | 'stateRestoreCurrentEditor' | 'stateSyncUI'
    | 'stateFinishIntegrator' | 'stateFinishDatetime' | 'stateFinishAlmanac' | 'stateFocusEditor' | 'stateStatus'
    | 'stateNotifyMode' | 'stateResetEditorSize' | 'stateSyncTabs' | 'stateForceMode' | 'stateDatetimeDefaults'
    | 'stateAlmanacDefaults' | 'stateRefreshLocation' | 'stateNamedIndex' | 'stateSyncMatrix'>;

/** @brief Forms capability subset registered with the native runtime. */
type WorksheetFormsCapabilities = Pick<WorksheetStateFlowServices,
    'stateBlankBounds' | 'stateFormsRequest' | 'stateFormsExpression' | 'stateFormsMetadata' | 'stateFormsRows'
    | 'stateFormsRender' | 'stateNeedsBounds' | 'stateResetBounds' | 'stateMatrixOperation' | 'stateMatrixOperand'
    | 'stateCurrentBounds' | 'stateIntervalCap' | 'stateRenderActiveRows' | 'stateBoundCount' | 'stateNormaliseCap'
    | 'stateValidOperation' | 'stateValidCap' | 'stateMergeRows' | 'stateRenderRows'>;

/** @brief Presentation capability subset registered with the native runtime. */
type WorksheetPresentationCapabilities = Pick<WorksheetStateFlowServices,
    'stateSaveResult' | 'stateRestoreResult' | 'stateClearFunction' | 'stateResultProjection' | 'stateClearResult'
    | 'stateResultValues' | 'stateDerivativeButtons' | 'stateRenderedFit' | 'stateSolverFit'>;

/** @brief Evaluation capability subset registered with the native runtime. */
type WorksheetEvaluationCapabilities = Pick<WorksheetStateFlowServices,
    'statePrecisionStep' | 'statePrecisionPlan' | 'statePrecisionGoal' | 'statePrecisionExpression'
    | 'stateEvaluate' | 'stateReady' | 'stateEvaluateExpression' | 'stateEvaluateEquation'
    | 'stateEvaluateDiffequation' | 'stateEvaluateMatrix' | 'stateEvaluateIntegrator' | 'stateEvaluateDatetime'>;
