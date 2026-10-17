/**
 * @file native_adapters.ts
 * @brief Test-only native-policy adapters and module-owner lookup.
 *
 * Provides fixture conveniences for exercising native policies and replacing namespace-owned services without adding production compatibility globals.
 */

/**
 * Test-only adapters for native browser policies, injected into the private browser fixture.
 * Production scripts call these C policies directly. Keep convenience wrappers here so
 * regression checks can exercise them without shipping unused adapters to users.
 */

let integratorCandidateNames: readonly string[];
let DEFAULT_EQUATION_VARIABLE_TEXT: string;

/** Direct owner lookup used only by fixtures retaining historical service names. */
const lab_test_targets: Readonly<Record<string, readonly [() => object, string]>> = {
    requestLabForms: [() => WorksheetRequests.presentation, 'requestLabForms'],
    requestLabPresentation: [() => WorksheetRequests.presentation, 'requestLabPresentation'],
    prepareLabEditor: [() => WorksheetRequests.presentation, 'prepareLabEditor'],
    fetchEvaluation: [() => WorksheetRequests.evaluationTransport, 'fetchEvaluation'],
    fetchMatrixEvaluation: [() => WorksheetRequests.evaluationTransport, 'fetchMatrixEvaluation'],
    fetchEquationEvaluation: [() => WorksheetRequests.evaluationTransport, 'fetchEquationEvaluation'],
    fetchDiffequationEvaluation: [() => WorksheetRequests.evaluationTransport, 'fetchDiffequationEvaluation'],
    fetchIntegratorEvaluation: [() => WorksheetRequests.evaluationTransport, 'fetchIntegratorEvaluation'],
    fetchDatetimeEvaluation: [() => WorksheetRequests.evaluationTransport, 'fetchDatetimeEvaluation'],
    fetchDatetimeWeather: [() => WorksheetRequests.evaluationTransport, 'fetchDatetimeWeather'],
    fetchAlmanacEvaluation: [() => WorksheetRequests.evaluationTransport, 'fetchAlmanacEvaluation'],
    clearFunctionRun: [() => WorksheetRequests.functionCard, 'clearFunctionRun'],
    runFunctionCard: [() => WorksheetRequests.functionCard, 'runFunctionCard'],
    takeCalculus: [() => WorksheetRequests.plots, 'takeCalculus'],
    takeDerivative: [() => WorksheetRequests.plots, 'takeDerivative'],
    takeIntegral: [() => WorksheetRequests.plots, 'takeIntegral'],
    takePlot: [() => WorksheetRequests.plots, 'takePlot'],
    schedulePlotControls: [() => WorksheetRequests.plots, 'schedulePlotControls'],
    schedulePlotWheelEnd: [() => WorksheetRequests.plots, 'schedulePlotWheelEnd'],
    interruptPlotDrag: [() => WorksheetRequests.plots, 'interruptPlotDrag'],
    pauseSurfaceSampling: [() => WorksheetRequests.plots, 'pauseSurfaceSampling'],
    setSurfaceZoom: [() => WorksheetRequests.plots, 'setSurfaceZoom'],
    zoomSurface: [() => WorksheetRequests.plots, 'zoomSurface'],
    redrawPlotDrag: [() => WorksheetRequests.plots, 'redrawPlotDrag'],
    scheduleAutomaticPlot: [() => WorksheetRequests.plots, 'scheduleAutomaticPlot'],
    fitSolverTexToCard: [() => WorksheetRequests.solver, 'fitSolverTexToCard'],
    refreshMobileAccess: [() => WorksheetRequests.calendarRequests, 'refreshMobileAccess'],
    refreshAlmanacLandTotality: [() => WorksheetRequests.calendarRequests, 'refreshAlmanacLandTotality'],
    refreshDatetimeWeather: [() => WorksheetRequests.calendarRequests, 'refreshDatetimeWeather'],
    evaluateLabMode: [() => WorksheetRequests.evaluation, 'evaluateLabMode'],
    installEvaluationResult: [() => WorksheetRequests.evaluation, 'installEvaluationResult'],
    evaluateActiveModeOnLoad: [() => WorksheetRequests.evaluation, 'evaluateActiveModeOnLoad'],
    startGoalSeekFromEvent: [() => WorksheetRequests.goals, 'startGoalSeekFromEvent'],
    runGoalSeek: [() => WorksheetRequests.goals, 'runGoalSeek'],
    labFlowContinue: [() => WorksheetRequests.runtime, 'continue'],
    labFlowCapabilities: [() => WorksheetRequests.runtime, 'getRegistry'],
    schedulePlotSlice: [() => WorksheetRequests.plots, 'schedulePlotSlice'],
    evaluateExpression: [() => WorksheetRequests.evaluation, 'evaluateExpression'],
    evaluateEquation: [() => WorksheetRequests.evaluation, 'evaluateEquation'],
    evaluateDiffequation: [() => WorksheetRequests.evaluation, 'evaluateDiffequation'],
    evaluateMatrix: [() => WorksheetRequests.evaluation, 'evaluateMatrix'],
    evaluateIntegrator: [() => WorksheetRequests.evaluation, 'evaluateIntegrator'],
    evaluateDatetime: [() => WorksheetRequests.evaluation, 'evaluateDatetime'],
    evaluateAlmanac: [() => WorksheetRequests.evaluation, 'evaluateAlmanac'],
    labPresentationEditorRequests: [() => WorksheetRequests.presentation, 'pending'],
    labSolverRenderRequest: [() => WorksheetRequests.solver, 'pending'],
    saveCurrentModeEditorState: [() => WorksheetWorkspace.mode, 'saveCurrentModeEditorState'],
    labBindingActions: [() => WorksheetBindings.panel, 'actions'],
    nextIntegratorFormsRevision: [() => WorksheetWorkspace.integrator.forms, 'nextRevision'],
    installIntegratorReferenceMetadata: [() => WorksheetWorkspace.integrator.forms, 'install'],
    integratorBoundsTextFromRows: [() => WorksheetWorkspace.integrator.forms, 'text'],
    planIntegratorRowEdit: [() => WorksheetWorkspace.integrator.forms, 'edit'],
    currentIntegratorBoundRows: [() => WorksheetWorkspace.integrator, 'activeIntegratorBoundRows'],
    workspaceStage: [() => WorksheetWorkspace.labWorkspaceBuffer, 'stage'],
    workspaceStageText: [() => WorksheetWorkspace.labWorkspaceBuffer, 'stageText'],
    workspaceReadBytes: [() => WorksheetWorkspace.labWorkspaceBuffer, 'readBytes'],
    workspaceReadSnapshot: [() => WorksheetWorkspace.labWorkspaceBuffer, 'readSnapshot'],
    workspaceStageSnapshot: [() => WorksheetWorkspace.labWorkspaceBuffer, 'stageSnapshot'],
    createWorkspaceModeView: [() => WorksheetWorkspace.nativeViews, 'mode'],
    createWorkspaceSourceView: [() => WorksheetWorkspace.nativeViews, 'sources'],
    scheduleWorkspacePanelFit: [() => WorksheetWorkspace.layout, 'scheduleEditorResizeGrip'],
    labIntegratorForms: [() => WorksheetWorkspace.integrator, 'forms'],
    labEditorData: [() => WorksheetBindings.editor, 'labEditorData'],
    expressionWithSortedConstants: [() => WorksheetBindings.editor, 'expressionWithSortedConstants'],
    currentExpressionText: [() => WorksheetBindings.editor, 'currentExpressionText'],
    expressionBodyForEditor: [() => WorksheetBindings.editor, 'expressionBodyForEditor'],
    expressionReadyToEvaluate: [() => WorksheetBindings.editor, 'expressionReadyToEvaluate'],
    bindingParts: [() => WorksheetBindings.editor, 'bindingParts'],
    compactExpressionForEditor: [() => WorksheetBindings.editor, 'compactExpressionForEditor'],
    expressionWithBindings: [() => WorksheetBindings.editor, 'expressionWithBindings'],
    expressionWithVisibleBindings: [() => WorksheetBindings.editor, 'expressionWithVisibleBindings'],
    bindingsWithAuthoredValues: [() => WorksheetBindings.editor, 'bindingsWithAuthoredValues'],
    replaceBindingKindInExpression: [() => WorksheetBindings.editor, 'replaceBindingKindInExpression'],
    bindingDisplayName: [() => WorksheetBindings.editor, 'bindingDisplayName'],
    visibleBindingValues: [() => WorksheetBindings.panel, 'visibleBindingValues'],
    clearVariableValues: [() => WorksheetBindings.panel, 'clearVariableValues'],
    renderVariableValues: [() => WorksheetBindings.panel, 'renderVariableValues'],
    editIntegratorRow: [() => WorksheetBindings.integratorPanel, 'editIntegratorRow'],
    prepareIntegratorBindingInput: [() => WorksheetBindings.integratorPanel, 'prepareIntegratorBindingInput'],
    renderIntegratorRows: [() => WorksheetBindings.integratorPanel, 'renderIntegratorRows'],
    applyIntegratorBindingState: [() => WorksheetBindings.integratorPanel, 'applyIntegratorBindingState'],
    canGoalSeek: [() => WorksheetBindings.goal, 'canGoalSeek'],
    clearGoalSeekRequest: [() => WorksheetBindings.goal, 'clearGoalSeekRequest'],
    currentGoalSeekSource: [() => WorksheetBindings.goal, 'currentGoalSeekSource'],
    solvedStartValuesForGoalSeek: [() => WorksheetBindings.goal, 'solvedStartValuesForGoalSeek'],
    goalSeekExpressionAndStarts: [() => WorksheetBindings.goal, 'goalSeekExpressionAndStarts'],
    labEditorSnapshot: [() => WorksheetBindings.controller, 'labEditorSnapshot'],
    clearExpressionSource: [() => WorksheetBindings.controller, 'clearExpressionSource'],
    applyUpdatedBindingExpression: [() => WorksheetBindings.controller, 'applyUpdatedBindingExpression'],
    applyMarsBindingExpression: [() => WorksheetBindings.controller, 'applyMarsBindingExpression'],
    applyMarsBindingsToEditedExpression: [() => WorksheetBindings.controller, 'applyMarsBindingsToEditedExpression'],
    scheduleEditedExpressionBindingRefresh: [() => WorksheetBindings.controller, 'scheduleEditedExpressionBindingRefresh'],
    commitLabBindingValues: [() => WorksheetBindings.controller, 'commitLabBindingValues'],
    commitBindingInput: [() => WorksheetBindings.controller, 'commitBindingInput'],
    commitVisibleBindingInputs: [() => WorksheetBindings.controller, 'commitVisibleBindingInputs'],
    toggleBindingKind: [() => WorksheetBindings.controller, 'toggleBindingKind'],
    refreshVariableValuesFromEditor: [() => WorksheetBindings.controller, 'refreshVariableValuesFromEditor'],
    queueBindingInputCommit: [() => WorksheetBindings.controller, 'queueBindingInputCommit'],
    setExpressionEditor: [() => WorksheetBindings.controller, 'setExpressionEditor'],
    refreshWorksheetFromInput: [() => WorksheetBindings.controller, 'refreshWorksheetFromInput'],
    labBindingFlowServices: [() => WorksheetBindings.controller, 'labBindingFlowServices'],
    integratorFormsExpression: [() => WorksheetWorkspace.integrator, 'integratorFormsExpression'],
    refreshIntegratorForms: [() => WorksheetWorkspace.integrator, 'refreshIntegratorForms'],
    integratorBlankRows: [() => WorksheetWorkspace.integrator, 'integratorBlankRows'],
    currentIntegratorRows: [() => WorksheetWorkspace.integrator, 'currentIntegratorRows'],
    activeIntegratorRowPlan: [() => WorksheetWorkspace.integrator, 'activeIntegratorRowPlan'],
    activeIntegratorBoundRows: [() => WorksheetWorkspace.integrator, 'activeIntegratorBoundRows'],
    activeIntegratorRows: [() => WorksheetWorkspace.integrator, 'activeIntegratorRows'],
    currentIntegratorBoundNames: [() => WorksheetWorkspace.integrator, 'currentIntegratorBoundNames'],
    applyIntegratorResultBound: [() => WorksheetWorkspace.integrator, 'applyIntegratorResultBound'],
    restoreIntegratorBoundsText: [() => WorksheetWorkspace.integrator, 'restoreIntegratorBoundsText'],
    currentIntegratorBoundsText: [() => WorksheetWorkspace.integrator, 'currentIntegratorBoundsText'],
    resetIntegratorBoundsToDefault: [() => WorksheetWorkspace.integrator, 'resetIntegratorBoundsToDefault'],
    resetIntegratorBoundsToBlank: [() => WorksheetWorkspace.integrator, 'resetIntegratorBoundsToBlank'],
    requestedIntegratorIntervalCap: [() => WorksheetWorkspace.integrator, 'requestedIntegratorIntervalCap'],
    workspaceModeId: [() => WorksheetWorkspace.mode, 'workspaceModeId'],
    currentMode: [() => WorksheetWorkspace.mode, 'currentMode'],
    setMode: [() => WorksheetWorkspace.mode, 'setMode'],
    workspaceDefaultText: [() => WorksheetWorkspace.mode, 'workspaceDefaultText'],
    captureCurrentModeEditor: [() => WorksheetWorkspace.mode, 'captureCurrentModeEditor'],
    restoreModeEditor: [() => WorksheetWorkspace.mode, 'restoreModeEditor'],
    applyLabMode: [() => WorksheetWorkspace.mode, 'applyLabMode'],
    evaluateCurrentMode: [() => WorksheetWorkspace.mode, 'evaluateCurrentMode'],
    evaluateFromKeyboard: [() => WorksheetWorkspace.mode, 'evaluateFromKeyboard'],
    clearWorksheetFromEvent: [() => WorksheetWorkspace.mode, 'clearWorksheetFromEvent'],
    selectWorksheetMode: [() => WorksheetWorkspace.mode, 'selectWorksheetMode'],
    normaliseWorksheetControl: [() => WorksheetWorkspace.mode, 'normaliseWorksheetControl'],
    precisionDigitsForBits: [() => WorksheetWorkspace.precision, 'precisionDigitsForBits'],
    requestedPrecisionBits: [() => WorksheetWorkspace.precision, 'requestedPrecisionBits'],
    requestedValuePrecision: [() => WorksheetWorkspace.precision, 'requestedValuePrecision'],
    atMinimumPrecision: [() => WorksheetWorkspace.precision, 'atMinimumPrecision'],
    atMaximumPrecision: [() => WorksheetWorkspace.precision, 'atMaximumPrecision'],
    setRequestedPrecisionBits: [() => WorksheetWorkspace.precision, 'setRequestedPrecisionBits'],
    changeWorksheetPrecision: [() => WorksheetWorkspace.precision, 'changeWorksheetPrecision'],
    setStatus: [() => WorksheetWorkspace.presentation, 'setStatus'],
    syncModeTabs: [() => WorksheetWorkspace.presentation, 'syncModeTabs'],
    setValueCardVisible: [() => WorksheetWorkspace.presentation, 'setValueCardVisible'],
    saveCurrentModeResultState: [() => WorksheetWorkspace.presentation, 'saveCurrentModeResultState'],
    restoreModeResultState: [() => WorksheetWorkspace.presentation, 'restoreModeResultState'],
    syncMatrixControls: [() => WorksheetWorkspace.presentation, 'syncMatrixControls'],
    syncModeUI: [() => WorksheetWorkspace.presentation, 'syncModeUI'],
    syncHelpCards: [() => WorksheetWorkspace.presentation, 'syncHelpCards'],
    showResults: [() => WorksheetWorkspace.presentation, 'showResults'],
    toggleHelp: [() => WorksheetWorkspace.presentation, 'toggleHelp'],
    hideTargetEntry: [() => WorksheetWorkspace.presentation, 'hideTargetEntry'],
    syncWorksheetButtons: [() => WorksheetWorkspace.presentation, 'syncWorksheetButtons'],
    setBusy: [() => WorksheetWorkspace.presentation, 'setBusy'],
    updateHistoryButtons: [() => WorksheetWorkspace.presentation, 'updateHistoryButtons'],
    renderDerivativeButtons: [() => WorksheetWorkspace.presentation, 'renderDerivativeButtons'],
    setActionRunning: [() => WorksheetWorkspace.presentation, 'setActionRunning'],
    resetEditorManualSize: [() => WorksheetWorkspace.layout, 'resetEditorManualSize'],
    syncEditorResizeGrip: [() => WorksheetWorkspace.layout, 'syncEditorResizeGrip'],
    scheduleEditorResizeGrip: [() => WorksheetWorkspace.layout, 'scheduleEditorResizeGrip'],
    pushExpressionHistory: [() => WorksheetWorkspace.history, 'pushExpressionHistory'],
    flushWorksheetState: [() => WorksheetWorkspace.history, 'flushWorksheetState'],
    variableNamesFromBindings: [() => WorksheetWorkspace.bindingSelection, 'variableNamesFromBindings'],
    visibleBindingsForCurrentMode: [() => WorksheetWorkspace.bindingSelection, 'visibleBindingsForCurrentMode'],
    saveLabLocalState: [() => WorksheetPersistence.persistence, 'writeLocal'],
    loadLabLocalState: [() => WorksheetPersistence.persistence, 'readLocal'],
    validIntegratorIntervalCap: [() => WorksheetPersistence.statePolicy, 'intervalCap'],
    validMatrixOperation: [() => WorksheetPersistence.statePolicy, 'matrixOperation'],
    worksheetSaveSchema: [() => WorksheetPersistence.statePolicy, 'schema'],
    saveLabState: [() => WorksheetPersistence.persistence, 'publish'],
    savePrecisionState: [() => WorksheetPersistence.persistence, 'savePrecision'],
    saveLastLabMode: [() => WorksheetPersistence.persistence, 'saveMode'],
    saveWorksheetState: [() => WorksheetPersistence.persistence, 'saveWorksheet'],
    restoreWorksheetEditors: [() => WorksheetPersistence.restoration, 'restoreEditors'],
    restoreWorksheetControls: [() => WorksheetPersistence.restoration, 'restoreControls'],
    applySavedState: [() => WorksheetPersistence.restoration, 'applySaved'],
    restoreNewerLocalEditors: [() => WorksheetPersistence.restoration, 'recoverLocalEditors'],
    loadLastState: [() => WorksheetPersistence.restoration, 'load'],
    currentHistoryLength: [() => WorksheetPersistence.history, 'backwardCount'],
    currentForwardHistoryLength: [() => WorksheetPersistence.history, 'forwardCount'],
    historyStateForMode: [() => WorksheetPersistence.history, 'snapshot'],
    historyStatesEqual: [() => WorksheetPersistence.history, 'equals'],
    previousModeStateForHistory: [() => WorksheetPersistence.history, 'previous'],
    commitModeState: [() => WorksheetPersistence.history, 'commit'],
    clearForwardHistory: [() => WorksheetPersistence.history, 'clearForward'],
    restoreHistoryState: [() => WorksheetPersistence.navigation, 'restore'],
    navigateHistoryFromEvent: [() => WorksheetPersistence.navigation, 'navigate'],
    labStateSync: [() => WorksheetPersistence.stateRuntime, 'sync'],
    worksheetSaveTimers: [() => WorksheetPersistence.persistence.scheduler, 'timers'],
    resetMoreDigitsButton: [() => WorksheetResults.cards, 'resetMoreDigitsButton'],
    clearRenderedError: [() => WorksheetResults.cards, 'clearRenderedError'],
    setRenderedError: [() => WorksheetResults.cards, 'setRenderedError'],
    setExpandableText: [() => WorksheetResults.cards, 'setExpandableText'],
    setValueText: [() => WorksheetResults.cards, 'setValueText'],
    renderDatetimeSections: [() => WorksheetResults.calendarResults, 'renderDatetimeSections'],
    displayMatrixResult: [() => WorksheetResults.cards, 'displayMatrixResult'],
    displayCalculusResult: [() => WorksheetResults.cards, 'displayCalculusResult'],
    setResultInputText: [() => WorksheetResults.reuse, 'setResultInputText'],
    resultExpressionTextForInput: [() => WorksheetResults.reuse, 'resultExpressionTextForInput'],
    sendResultExpressionToInput: [() => WorksheetResults.reuse, 'sendResultExpressionToInput'],
    setMatrixPrettyResult: [() => WorksheetResults.cards, 'setMatrixPrettyResult'],
    setRenderedResult: [() => WorksheetResults.cards, 'setRenderedResult'],
    renderTexSvg: [() => WorksheetResults.actions, 'renderTexSvg'],
    toggleTextDigits: [() => WorksheetResults.cards, 'toggleTextDigits'],
    toggleRenderedDigits: [() => WorksheetResults.actions, 'toggleRenderedDigits'],
    clearResultPane: [() => WorksheetResults.reset, 'clearResultPane'],
    clearResultDetails: [() => WorksheetResults.reset, 'clearResultDetails'],
    setAlmanacVisibility: [() => WorksheetResults.calendarResults, 'setAlmanacVisibility'],
    bindAlmanacTotalityActions: [() => WorksheetResults.calendarResults, 'bindAlmanacTotalityActions'],
    renderAlmanacWorksheet: [() => WorksheetResults.calendarResults, 'renderAlmanacWorksheet'],
    copyInputFromEvent: [() => WorksheetResults.actions, 'copyInputFromEvent'],
    installEvaluationTextCards: [() => WorksheetResults.cards, 'installEvaluationTextCards'],
    copyResultFromEvent: [() => WorksheetResults.actions, 'copyResultFromEvent'],
    showButtonTooltip: [() => WorksheetResults.tooltip, 'show'],
    hideButtonTooltip: [() => WorksheetResults.tooltip, 'hide'],
    fitRenderedTeXToCard: [() => WorksheetResults.renderedFit, 'fit'],
    scheduleRenderedTeXFit: [() => WorksheetResults.renderedFit, 'schedule'],
    scheduleSolverTexFit: [() => WorksheetResults.solverFit, 'schedule'],
    installLabPresentationData: [() => WorksheetResults.caches, 'install'],
    installLabFunctionSyntaxData: [() => WorksheetResults.caches, 'installSyntax'],
    copyTextForTarget: [() => WorksheetResults.clipboard, 'copyTextForTarget'],
    writeClipboardText: [() => WorksheetResults.clipboard, 'write'],
    flashCopyButton: [() => WorksheetResults.clipboard, 'flash'],
    formatAlmanacTimeInput: [() => WorksheetLocations.calendarActions, 'formatAlmanacTimeInput'],
    townsForJurisdiction: [() => WorksheetLocations.townSelection, 'townsForJurisdiction'],
    selectTownByCoordinates: [() => WorksheetLocations.townSelection, 'selectTownByCoordinates'],
    restoreTownSelection: [() => WorksheetLocations.townSelection, 'restoreTownSelection'],
    populateTownSelect: [() => WorksheetLocations.townSelection, 'populateTownSelect'],
    selectedTownOption: [() => WorksheetLocations.townSelection, 'selectedTownOption'],
    clearTownForCustomCoordinates: [() => WorksheetLocations.townSelection, 'clearTownForCustomCoordinates'],
    timeZoneOffsetHours: [() => WorksheetLocations.dates, 'timeZoneOffsetHours'],
    formatOffsetHours: [() => WorksheetLocations.dates, 'formatOffsetHours'],
    syncTownSelectors: [() => WorksheetLocations.townSelection, 'syncTownSelectors'],
    calendarLocationContext: [() => WorksheetLocations.calendar, 'calendarLocationContext'],
    acceptCalendarLocationContext: [() => WorksheetLocations.calendar, 'acceptCalendarLocationContext'],
    markDatetimeOffsetTouched: [() => WorksheetLocations.calendar, 'markDatetimeOffsetTouched'],
    readCalendarControls: [() => WorksheetLocations.calendar, 'readCalendarControls'],
    calendarState: [() => WorksheetLocations.calendar, 'calendarState'],
    applyCalendarState: [() => WorksheetLocations.calendar, 'applyCalendarState'],
    restoreCalendarHistory: [() => WorksheetLocations.calendarActions, 'restoreCalendarHistory'],
    calendarElements: [() => WorksheetLocations.calendar, 'calendarElements'],
    restoreDatetimeDefaultsIfBlank: [() => WorksheetLocations.calendar, 'restoreDatetimeDefaultsIfBlank'],
    restoreAlmanacDefaultsIfBlank: [() => WorksheetLocations.calendar, 'restoreAlmanacDefaultsIfBlank'],
    currentDatetimeState: [() => WorksheetLocations.calendar, 'currentDatetimeState'],
    currentAlmanacState: [() => WorksheetLocations.calendar, 'currentAlmanacState'],
    almanacSummaryText: [() => WorksheetLocations.calendar, 'almanacSummaryText'],
    datetimeSummaryText: [() => WorksheetLocations.calendar, 'datetimeSummaryText'],
    applyCalendarEvaluationFields: [() => WorksheetLocations.calendar, 'applyCalendarEvaluationFields'],
    applyAlmanacTotalityAction: [() => WorksheetLocations.calendarActions, 'applyAlmanacTotalityAction'],
    refreshDatetimeJurisdictionLocation: [() => WorksheetLocations.calendarActions, 'refreshDatetimeJurisdictionLocation'],
    triggerDatetimeAutoEvaluation: [() => WorksheetLocations.calendarActions, 'triggerDatetimeAutoEvaluation'],
    triggerAlmanacAutoEvaluation: [() => WorksheetLocations.calendarActions, 'triggerAlmanacAutoEvaluation'],
    formatAlmanacTimeFromEvent: [() => WorksheetLocations.calendarActions, 'formatAlmanacTimeFromEvent'],
    applyCalendarControlEvent: [() => WorksheetLocations.calendarActions, 'applyCalendarControlEvent'],
    marsFormNumber: [() => WorksheetLocations.dates, 'marsFormNumber'],
    parseMarsIsoDate: [() => WorksheetLocations.dates, 'parseMarsIsoDate'],
    marsTodayIsoDate: [() => WorksheetLocations.dates, 'marsTodayIsoDate'],
    marsCurrentGmtMoment: [() => WorksheetLocations.dates, 'marsCurrentGmtMoment'],
    syncRoundedSelect: [() => WorksheetLocations.selects, 'sync'],
    setDatetimeLocalText: [() => WorksheetResults.calendarResults, 'setDatetimeLocalText'],
    enhanceRoundedSelect: [() => WorksheetLocations.selects, 'enhance'],
    marsTodayIsoDateForInput: [() => WorksheetLocations.picker, 'todayForInput'],
    closeMarsDatePicker: [() => WorksheetLocations.picker, 'close'],
    placeMarsDatePicker: [() => WorksheetLocations.picker, 'place'],
    commitMarsDateValue: [() => WorksheetLocations.picker, 'commit'],
    commitMarsTodayValue: [() => WorksheetLocations.picker, 'commitToday'],
    renderMarsDatePicker: [() => WorksheetLocations.picker, 'render'],
    openMarsDatePicker: [() => WorksheetLocations.picker, 'open'],
    setMarsDatePickerMonthYear: [() => WorksheetLocations.picker, 'setMonthYear'],
    shiftMarsDatePickerYear: [() => WorksheetLocations.picker, 'shiftYear'],
    shiftMarsDatePickerMonth: [() => WorksheetLocations.picker, 'shiftMonth'],
    labFetch: [() => labHttp, 'fetch'],
    expression_text_or_empty: [() => WorksheetBindings, 'expression_text_or_empty'],
};

/** Fixture-only property adapters; production exposes no forwarding functions. */
const lab_test_methods: object = Object.defineProperties(Object.create(null),
    Object.fromEntries(Object.entries(lab_test_targets).map(([name, [owner, key]]) => [name, {
        get: () => Reflect.get(owner(), key),
        set: (value: unknown) => { Reflect.set(owner(), key, value); }
    }])));

/** Resolve a fixture replacement directly to the method on its owning instance. */
function labTestModuleOwner(name: string): object {
    return Object.hasOwn(lab_test_targets, name) ? lab_test_methods : window;
}

function initialise_lab_test_adapters() {
    integratorCandidateNames = Object.freeze(labDOM.call<string[]>('lab_binding_row_names'));
    DEFAULT_EQUATION_VARIABLE_TEXT = labApplication.configuration.DEFAULT_EQUATION_VARIABLE;
}

// This copies structured DOM fields only. Native /forms owns all text interpretation.
function sanitizeIntegratorRow(row: unknown, fallbackName = 'x') {
    return labDOM.call<WorksheetIntegratorRow>('lab_binding_row_normalise', row, String(fallbackName));
}

function integratorDefaultVariableName(rows: unknown[] = []) {
    return labDOM.call<string>('lab_binding_rows_name', rows);
}

function integratorRowText(row: WorksheetIntegratorRow) {
    return WorksheetWorkspace.integrator.forms.text([row]);
}

async function parseIntegratorBoundsText(text: unknown): Promise<WorksheetIntegratorRow[]> {
    return WorksheetRequests.runtime.continue(74, {text});
}

function integratorFallbackRows() {
    return labDOM.call<WorksheetIntegratorRow[]>('lab_binding_rows_default', 0);
}

function precisionStatusText() {
    return labDOM.call<string>('lab_workspace_dom_precision');
}

function showHelp() {
    labDOM.call('lab_workspace_dom_help', 1);
}

function showTargetEntry() {
    labDOM.call('lab_workspace_dom_target', 1);
}

const EXPRESSION_TIMESTAMP_STORAGE_KEY = 'mars.exprLab.lastExpressionUpdatedAt';

const hasAbbreviatedValue = (value: unknown) => value === true;

const resultZoomIndex = (card: HTMLElement) => labWire.exports().lab_view_card_zoom(WorksheetContext.resultCardIds.get(card) ?? -1);

const applyResultZoom = (card: HTMLElement) => labDOM.call('lab_layout_zoom', card);

const setResultZoom = (card: HTMLElement, index: number) => labDOM.call('lab_layout_set_zoom', card, index, 0);

const stepResultZoom = (card: HTMLElement, direction: number) => labDOM.call('lab_layout_set_zoom', card, direction, 1);

const renderResultCardExpansion = () => labDOM.call('lab_layout_expand', null, 0);

const resultCardExpanded = (card: HTMLElement) => labWire.exports().lab_view_card_expanded() === WorksheetContext.resultCardIds.get(card);

const collapseResultCards = () => labDOM.call('lab_layout_expand', null, 2);

const toggleResultCardExpansion = (button: HTMLElement) => labDOM.call('lab_layout_expand', button.closest('.result-card'), 1);

const svgMarkupIntrinsicWidth = (markup: unknown) => labDOM.call('lab_layout_markup_width', markup);

const installSolverTexSvg = (markup: string, variant: string) => labDOM.call('lab_layout_solver_install', markup, variant);

function parsedExpressionText() {
    return labDOM.call('lab_result_copy', 'expression', WorksheetWorkspace.presentation.state.lastTex);
}

function almanacWorksheetCopyText(data: unknown, visibility: string) {
    return almanacPresentationVariant(data, visibility).copy_text;
}

function evaluationLabel(mode: number, field: number) {
    return labDOM.call<string>('lab_evaluation_label', mode, field);
}

function validDateText(value: unknown, fallback = WorksheetContext.DEFAULT_DATETIME_DATE) {
    return labDOM.call<string>('lab_location_validate', 1, {value}, fallback, WorksheetContext.HOLIDAY_JURISDICTION_SET);
}

function validDatetimeJurisdiction(value: unknown, fallback = WorksheetContext.DEFAULT_DATETIME_JURISDICTION) {
    return labDOM.call<string>('lab_location_validate', 2, {value}, fallback, WorksheetContext.HOLIDAY_JURISDICTION_SET);
}

function townValueParts(value: unknown) {
    return WorksheetRequests.runtime.continue(26, {action: 1, value});
}

function townOptionMatchesValue(option: unknown, value: unknown) {
    return WorksheetRequests.runtime.continue(26, {action: 2, option, value});
}

function numbersNearlyEqual(left: unknown, right: unknown, tolerance = 0.000001) {
    return Boolean(labWire.exports().lab_forms_nearly_equal(WorksheetLocations.dates.marsFormNumber(left), WorksheetLocations.dates.marsFormNumber(right), tolerance));
}

function calendarSchema(mode: string) {
    if (!calendarSchemas.has(mode))
        calendarSchemas.set(mode, labDOM.call<LabTestCalendarSchema>('lab_location_schema', WorksheetWorkspace.mode.workspaceModeId(mode)));
    return calendarSchemas.get(mode)!;
}

function validAlmanacVisibility(value: unknown, fallback = WorksheetContext.DEFAULT_ALMANAC_VISIBILITY) {
    return labDOM.call<string>('lab_location_validate', 3, {value}, fallback, WorksheetContext.HOLIDAY_JURISDICTION_SET);
}

function marsDatePickerAnchorRect(shell: Element | null) {
    return labDOM.call<Pick<DOMRect, 'left' | 'right' | 'top' | 'bottom' | 'width' | 'height'> | null>(
        'lab_widgets_picker_anchor', shell);
}

const calendarSchemas = new Map<string, LabTestCalendarSchema>();

function validPrecisionBits(bits: unknown, fallback: unknown) {
    return labDOM.call<number>('lab_storage_precision', [bits, fallback]);
}

function restoreCompactBindingValues(text: string) {
    return labDOM.call('lab_editor_metadata', 2, [text], WorksheetResults.labEvaluationView.caches.editors);
}

function almanacPresentationVariant(data: unknown, visibility: string) {
    const variant = labDOM.call<LabTestAlmanacVariant | null>('lab_result_almanac_variant', data, visibility);
    if (!variant)
        throw new Error(
            'Almanac presentation is missing from the server response. ' +
            'Run make mars-lab-restart, reload the page and evaluate again.');
    return variant;
}
