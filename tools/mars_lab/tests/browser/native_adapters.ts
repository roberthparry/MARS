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

/** @brief Historical fixture names mapped to their current owning properties. */
const labTestModulePaths: {[name: string]: string} = {
    // WorksheetRequests.
    fetch: 'WorksheetRequests.evaluationTransport.fetch',
    requestLabForms: 'WorksheetRequests.presentation.requestLabForms',
    requestLabPresentation: 'WorksheetRequests.presentation.requestLabPresentation',
    prepareLabEditor: 'WorksheetRequests.presentation.prepareLabEditor',
    fetchEvaluation: 'WorksheetRequests.evaluationTransport.fetchEvaluation',
    fetchMatrixEvaluation: 'WorksheetRequests.evaluationTransport.fetchMatrixEvaluation',
    fetchEquationEvaluation: 'WorksheetRequests.evaluationTransport.fetchEquationEvaluation',
    fetchDiffequationEvaluation: 'WorksheetRequests.evaluationTransport.fetchDiffequationEvaluation',
    fetchIntegratorEvaluation: 'WorksheetRequests.evaluationTransport.fetchIntegratorEvaluation',
    fetchDatetimeEvaluation: 'WorksheetRequests.evaluationTransport.fetchDatetimeEvaluation',
    fetchDatetimeWeather: 'WorksheetRequests.evaluationTransport.fetchDatetimeWeather',
    fetchAlmanacEvaluation: 'WorksheetRequests.evaluationTransport.fetchAlmanacEvaluation',
    clearFunctionRun: 'WorksheetRequests.functionCard.clearFunctionRun',
    runFunctionCard: 'WorksheetRequests.functionCard.runFunctionCard',
    takeCalculus: 'WorksheetRequests.plots.takeCalculus',
    takeDerivative: 'WorksheetRequests.plots.takeDerivative',
    takeIntegral: 'WorksheetRequests.plots.takeIntegral',
    takePlot: 'WorksheetRequests.plots.takePlot',
    schedulePlotControls: 'WorksheetRequests.plots.schedulePlotControls',
    schedulePlotWheelEnd: 'WorksheetRequests.plots.schedulePlotWheelEnd',
    interruptPlotDrag: 'WorksheetRequests.plots.interruptPlotDrag',
    pauseSurfaceSampling: 'WorksheetRequests.plots.pauseSurfaceSampling',
    setSurfaceZoom: 'WorksheetRequests.plots.setSurfaceZoom',
    zoomSurface: 'WorksheetRequests.plots.zoomSurface',
    redrawPlotDrag: 'WorksheetRequests.plots.redrawPlotDrag',
    scheduleAutomaticPlot: 'WorksheetRequests.plots.scheduleAutomaticPlot',
    fitSolverTexToCard: 'WorksheetRequests.solver.fitSolverTexToCard',
    refreshMobileAccess: 'WorksheetRequests.calendarRequests.refreshMobileAccess',
    refreshAlmanacLandTotality: 'WorksheetRequests.calendarRequests.refreshAlmanacLandTotality',
    refreshDatetimeWeather: 'WorksheetRequests.calendarRequests.refreshDatetimeWeather',
    evaluateLabMode: 'WorksheetRequests.evaluation.evaluateLabMode',
    installEvaluationResult: 'WorksheetRequests.evaluation.installEvaluationResult',
    evaluateActiveModeOnLoad: 'WorksheetRequests.evaluation.evaluateActiveModeOnLoad',
    startGoalSeekFromEvent: 'WorksheetRequests.goals.startGoalSeekFromEvent',
    runGoalSeek: 'WorksheetRequests.goals.runGoalSeek',
    labFlowContinue: 'WorksheetRequests.runtime.continue',
    labFlowCapabilities: 'WorksheetRequests.runtime.getRegistry',
    schedulePlotSlice: 'WorksheetRequests.plots.schedulePlotSlice',
    evaluateExpression: 'WorksheetRequests.evaluation.evaluateExpression',
    evaluateEquation: 'WorksheetRequests.evaluation.evaluateEquation',
    evaluateDiffequation: 'WorksheetRequests.evaluation.evaluateDiffequation',
    evaluateMatrix: 'WorksheetRequests.evaluation.evaluateMatrix',
    evaluateIntegrator: 'WorksheetRequests.evaluation.evaluateIntegrator',
    evaluateDatetime: 'WorksheetRequests.evaluation.evaluateDatetime',
    evaluateAlmanac: 'WorksheetRequests.evaluation.evaluateAlmanac',
    labPresentationEditorRequests: 'WorksheetRequests.presentation.pending',
    labSolverRenderRequest: 'WorksheetRequests.solver.pending',

    // WorksheetWorkspace.
    saveCurrentModeEditorState: 'WorksheetWorkspace.mode.saveCurrentModeEditorState',

    // WorksheetBindings.
    labBindingActions: 'WorksheetBindings.panel.actions',

    // WorksheetWorkspace.
    nextIntegratorFormsRevision: 'WorksheetWorkspace.integrator.forms.nextRevision',
    installIntegratorReferenceMetadata: 'WorksheetWorkspace.integrator.forms.install',
    integratorBoundsTextFromRows: 'WorksheetWorkspace.integrator.forms.text',
    planIntegratorRowEdit: 'WorksheetWorkspace.integrator.forms.edit',
    currentIntegratorBoundRows: 'WorksheetWorkspace.integrator.activeIntegratorBoundRows',
    workspaceStage: 'WorksheetWorkspace.labWorkspaceBuffer.stage',
    workspaceStageText: 'WorksheetWorkspace.labWorkspaceBuffer.stageText',
    workspaceReadBytes: 'WorksheetWorkspace.labWorkspaceBuffer.readBytes',
    workspaceReadSnapshot: 'WorksheetWorkspace.labWorkspaceBuffer.readSnapshot',
    workspaceStageSnapshot: 'WorksheetWorkspace.labWorkspaceBuffer.stageSnapshot',
    createWorkspaceModeView: 'WorksheetWorkspace.nativeViews.mode',
    createWorkspaceSourceView: 'WorksheetWorkspace.nativeViews.sources',
    scheduleWorkspacePanelFit: 'WorksheetWorkspace.layout.scheduleEditorResizeGrip',
    labIntegratorForms: 'WorksheetWorkspace.integrator.forms',

    // WorksheetBindings.
    labEditorData: 'WorksheetBindings.editor.labEditorData',
    expressionWithSortedConstants: 'WorksheetBindings.editor.expressionWithSortedConstants',
    currentExpressionText: 'WorksheetBindings.editor.currentExpressionText',
    expressionBodyForEditor: 'WorksheetBindings.editor.expressionBodyForEditor',
    expressionReadyToEvaluate: 'WorksheetBindings.editor.expressionReadyToEvaluate',
    bindingParts: 'WorksheetBindings.editor.bindingParts',
    compactExpressionForEditor: 'WorksheetBindings.editor.compactExpressionForEditor',
    expressionWithBindings: 'WorksheetBindings.editor.expressionWithBindings',
    expressionWithVisibleBindings: 'WorksheetBindings.editor.expressionWithVisibleBindings',
    bindingsWithAuthoredValues: 'WorksheetBindings.editor.bindingsWithAuthoredValues',
    replaceBindingKindInExpression: 'WorksheetBindings.editor.replaceBindingKindInExpression',
    bindingDisplayName: 'WorksheetBindings.editor.bindingDisplayName',
    visibleBindingValues: 'WorksheetBindings.panel.visibleBindingValues',
    clearVariableValues: 'WorksheetBindings.panel.clearVariableValues',
    renderVariableValues: 'WorksheetBindings.panel.renderVariableValues',
    editIntegratorRow: 'WorksheetBindings.integratorPanel.editIntegratorRow',
    prepareIntegratorBindingInput: 'WorksheetBindings.integratorPanel.prepareIntegratorBindingInput',
    renderIntegratorRows: 'WorksheetBindings.integratorPanel.renderIntegratorRows',
    applyIntegratorBindingState: 'WorksheetBindings.integratorPanel.applyIntegratorBindingState',
    canGoalSeek: 'WorksheetBindings.goal.canGoalSeek',
    clearGoalSeekRequest: 'WorksheetBindings.goal.clearGoalSeekRequest',
    currentGoalSeekSource: 'WorksheetBindings.goal.currentGoalSeekSource',
    solvedStartValuesForGoalSeek: 'WorksheetBindings.goal.solvedStartValuesForGoalSeek',
    goalSeekExpressionAndStarts: 'WorksheetBindings.goal.goalSeekExpressionAndStarts',
    labEditorSnapshot: 'WorksheetBindings.controller.labEditorSnapshot',
    clearExpressionSource: 'WorksheetBindings.controller.clearExpressionSource',
    applyUpdatedBindingExpression: 'WorksheetBindings.controller.applyUpdatedBindingExpression',
    applyMarsBindingExpression: 'WorksheetBindings.controller.applyMarsBindingExpression',
    applyMarsBindingsToEditedExpression: 'WorksheetBindings.controller.applyMarsBindingsToEditedExpression',
    scheduleEditedExpressionBindingRefresh: 'WorksheetBindings.controller.scheduleEditedExpressionBindingRefresh',
    commitLabBindingValues: 'WorksheetBindings.controller.commitLabBindingValues',
    commitBindingInput: 'WorksheetBindings.controller.commitBindingInput',
    commitVisibleBindingInputs: 'WorksheetBindings.controller.commitVisibleBindingInputs',
    toggleBindingKind: 'WorksheetBindings.controller.toggleBindingKind',
    refreshVariableValuesFromEditor: 'WorksheetBindings.controller.refreshVariableValuesFromEditor',
    queueBindingInputCommit: 'WorksheetBindings.controller.queueBindingInputCommit',
    setExpressionEditor: 'WorksheetBindings.controller.setExpressionEditor',
    refreshWorksheetFromInput: 'WorksheetBindings.controller.refreshWorksheetFromInput',
    labBindingFlowServices: 'WorksheetBindings.controller.labBindingFlowServices',

    // WorksheetWorkspace.
    integratorFormsExpression: 'WorksheetWorkspace.integrator.integratorFormsExpression',
    refreshIntegratorForms: 'WorksheetWorkspace.integrator.refreshIntegratorForms',
    integratorBlankRows: 'WorksheetWorkspace.integrator.integratorBlankRows',
    currentIntegratorRows: 'WorksheetWorkspace.integrator.currentIntegratorRows',
    activeIntegratorRowPlan: 'WorksheetWorkspace.integrator.activeIntegratorRowPlan',
    activeIntegratorBoundRows: 'WorksheetWorkspace.integrator.activeIntegratorBoundRows',
    activeIntegratorRows: 'WorksheetWorkspace.integrator.activeIntegratorRows',
    currentIntegratorBoundNames: 'WorksheetWorkspace.integrator.currentIntegratorBoundNames',
    applyIntegratorResultBound: 'WorksheetWorkspace.integrator.applyIntegratorResultBound',
    restoreIntegratorBoundsText: 'WorksheetWorkspace.integrator.restoreIntegratorBoundsText',
    currentIntegratorBoundsText: 'WorksheetWorkspace.integrator.currentIntegratorBoundsText',
    resetIntegratorBoundsToDefault: 'WorksheetWorkspace.integrator.resetIntegratorBoundsToDefault',
    resetIntegratorBoundsToBlank: 'WorksheetWorkspace.integrator.resetIntegratorBoundsToBlank',
    requestedIntegratorIntervalCap: 'WorksheetWorkspace.integrator.requestedIntegratorIntervalCap',
    workspaceModeId: 'WorksheetWorkspace.mode.workspaceModeId',
    currentMode: 'WorksheetWorkspace.mode.currentMode',
    setMode: 'WorksheetWorkspace.mode.setMode',
    workspaceDefaultText: 'WorksheetWorkspace.mode.workspaceDefaultText',
    captureCurrentModeEditor: 'WorksheetWorkspace.mode.captureCurrentModeEditor',
    restoreModeEditor: 'WorksheetWorkspace.mode.restoreModeEditor',
    applyLabMode: 'WorksheetWorkspace.mode.applyLabMode',
    evaluateCurrentMode: 'WorksheetWorkspace.mode.evaluateCurrentMode',
    evaluateFromKeyboard: 'WorksheetWorkspace.mode.evaluateFromKeyboard',
    clearWorksheetFromEvent: 'WorksheetWorkspace.mode.clearWorksheetFromEvent',
    selectWorksheetMode: 'WorksheetWorkspace.mode.selectWorksheetMode',
    normaliseWorksheetControl: 'WorksheetWorkspace.mode.normaliseWorksheetControl',
    precisionDigitsForBits: 'WorksheetWorkspace.precision.precisionDigitsForBits',
    requestedPrecisionBits: 'WorksheetWorkspace.precision.requestedPrecisionBits',
    requestedValuePrecision: 'WorksheetWorkspace.precision.requestedValuePrecision',
    atMinimumPrecision: 'WorksheetWorkspace.precision.atMinimumPrecision',
    atMaximumPrecision: 'WorksheetWorkspace.precision.atMaximumPrecision',
    setRequestedPrecisionBits: 'WorksheetWorkspace.precision.setRequestedPrecisionBits',
    changeWorksheetPrecision: 'WorksheetWorkspace.precision.changeWorksheetPrecision',
    setStatus: 'WorksheetWorkspace.presentation.setStatus',
    syncModeTabs: 'WorksheetWorkspace.presentation.syncModeTabs',
    setValueCardVisible: 'WorksheetWorkspace.presentation.setValueCardVisible',
    saveCurrentModeResultState: 'WorksheetWorkspace.presentation.saveCurrentModeResultState',
    restoreModeResultState: 'WorksheetWorkspace.presentation.restoreModeResultState',
    syncMatrixControls: 'WorksheetWorkspace.presentation.syncMatrixControls',
    syncModeUI: 'WorksheetWorkspace.presentation.syncModeUI',
    syncHelpCards: 'WorksheetWorkspace.presentation.syncHelpCards',
    showResults: 'WorksheetWorkspace.presentation.showResults',
    toggleHelp: 'WorksheetWorkspace.presentation.toggleHelp',
    hideTargetEntry: 'WorksheetWorkspace.presentation.hideTargetEntry',
    syncWorksheetButtons: 'WorksheetWorkspace.presentation.syncWorksheetButtons',
    setBusy: 'WorksheetWorkspace.presentation.setBusy',
    updateHistoryButtons: 'WorksheetWorkspace.presentation.updateHistoryButtons',
    renderDerivativeButtons: 'WorksheetWorkspace.presentation.renderDerivativeButtons',
    setActionRunning: 'WorksheetWorkspace.presentation.setActionRunning',
    resetEditorManualSize: 'WorksheetWorkspace.layout.resetEditorManualSize',
    syncEditorResizeGrip: 'WorksheetWorkspace.layout.syncEditorResizeGrip',
    scheduleEditorResizeGrip: 'WorksheetWorkspace.layout.scheduleEditorResizeGrip',
    pushExpressionHistory: 'WorksheetWorkspace.history.pushExpressionHistory',
    flushWorksheetState: 'WorksheetWorkspace.history.flushWorksheetState',
    variableNamesFromBindings: 'WorksheetWorkspace.bindingSelection.variableNamesFromBindings',
    visibleBindingsForCurrentMode: 'WorksheetWorkspace.bindingSelection.visibleBindingsForCurrentMode',

    // WorksheetPersistence.
    saveLabLocalState: 'WorksheetPersistence.persistence.writeLocal',
    loadLabLocalState: 'WorksheetPersistence.persistence.readLocal',
    validIntegratorIntervalCap: 'WorksheetPersistence.statePolicy.intervalCap',
    validMatrixOperation: 'WorksheetPersistence.statePolicy.matrixOperation',
    worksheetSaveSchema: 'WorksheetPersistence.statePolicy.schema',
    saveLabState: 'WorksheetPersistence.persistence.publish',
    savePrecisionState: 'WorksheetPersistence.persistence.savePrecision',
    saveLastLabMode: 'WorksheetPersistence.persistence.saveMode',
    saveWorksheetState: 'WorksheetPersistence.persistence.saveWorksheet',
    restoreWorksheetEditors: 'WorksheetPersistence.restoration.restoreEditors',
    restoreWorksheetControls: 'WorksheetPersistence.restoration.restoreControls',
    applySavedState: 'WorksheetPersistence.restoration.applySaved',
    restoreNewerLocalEditors: 'WorksheetPersistence.restoration.recoverLocalEditors',
    loadLastState: 'WorksheetPersistence.restoration.load',
    currentHistoryLength: 'WorksheetPersistence.history.backwardCount',
    currentForwardHistoryLength: 'WorksheetPersistence.history.forwardCount',
    historyStateForMode: 'WorksheetPersistence.history.snapshot',
    historyStatesEqual: 'WorksheetPersistence.history.equals',
    previousModeStateForHistory: 'WorksheetPersistence.history.previous',
    commitModeState: 'WorksheetPersistence.history.commit',
    clearForwardHistory: 'WorksheetPersistence.history.clearForward',
    restoreHistoryState: 'WorksheetPersistence.navigation.restore',
    navigateHistoryFromEvent: 'WorksheetPersistence.navigation.navigate',
    labStateSync: 'WorksheetPersistence.stateRuntime.sync',
    worksheetSaveTimers: 'WorksheetPersistence.persistence.scheduler.timers',

    // WorksheetResults.
    resetMoreDigitsButton: 'WorksheetResults.cards.resetMoreDigitsButton',
    clearRenderedError: 'WorksheetResults.cards.clearRenderedError',
    setRenderedError: 'WorksheetResults.cards.setRenderedError',
    setExpandableText: 'WorksheetResults.cards.setExpandableText',
    setValueText: 'WorksheetResults.cards.setValueText',
    renderDatetimeSections: 'WorksheetResults.calendarResults.renderDatetimeSections',
    displayMatrixResult: 'WorksheetResults.cards.displayMatrixResult',
    displayCalculusResult: 'WorksheetResults.cards.displayCalculusResult',
    setResultInputText: 'WorksheetResults.reuse.setResultInputText',
    resultExpressionTextForInput: 'WorksheetResults.reuse.resultExpressionTextForInput',
    sendResultExpressionToInput: 'WorksheetResults.reuse.sendResultExpressionToInput',
    setMatrixPrettyResult: 'WorksheetResults.cards.setMatrixPrettyResult',
    setRenderedResult: 'WorksheetResults.cards.setRenderedResult',
    renderTexSvg: 'WorksheetResults.actions.renderTexSvg',
    toggleTextDigits: 'WorksheetResults.cards.toggleTextDigits',
    toggleRenderedDigits: 'WorksheetResults.actions.toggleRenderedDigits',
    clearResultPane: 'WorksheetResults.reset.clearResultPane',
    clearResultDetails: 'WorksheetResults.reset.clearResultDetails',
    setAlmanacVisibility: 'WorksheetResults.calendarResults.setAlmanacVisibility',
    bindAlmanacTotalityActions: 'WorksheetResults.calendarResults.bindAlmanacTotalityActions',
    renderAlmanacWorksheet: 'WorksheetResults.calendarResults.renderAlmanacWorksheet',
    copyInputFromEvent: 'WorksheetResults.actions.copyInputFromEvent',
    installEvaluationTextCards: 'WorksheetResults.cards.installEvaluationTextCards',
    copyResultFromEvent: 'WorksheetResults.actions.copyResultFromEvent',
    showButtonTooltip: 'WorksheetResults.tooltip.show',
    hideButtonTooltip: 'WorksheetResults.tooltip.hide',
    fitRenderedTeXToCard: 'WorksheetResults.renderedFit.fit',
    scheduleRenderedTeXFit: 'WorksheetResults.renderedFit.schedule',
    scheduleSolverTexFit: 'WorksheetResults.solverFit.schedule',
    installLabPresentationData: 'WorksheetResults.caches.install',
    installLabFunctionSyntaxData: 'WorksheetResults.caches.installSyntax',
    copyTextForTarget: 'WorksheetResults.clipboard.copyTextForTarget',
    writeClipboardText: 'WorksheetResults.clipboard.write',
    flashCopyButton: 'WorksheetResults.clipboard.flash',

    // WorksheetLocations.
    formatAlmanacTimeInput: 'WorksheetLocations.calendarActions.formatAlmanacTimeInput',
    townsForJurisdiction: 'WorksheetLocations.townSelection.townsForJurisdiction',
    selectTownByCoordinates: 'WorksheetLocations.townSelection.selectTownByCoordinates',
    restoreTownSelection: 'WorksheetLocations.townSelection.restoreTownSelection',
    populateTownSelect: 'WorksheetLocations.townSelection.populateTownSelect',
    selectedTownOption: 'WorksheetLocations.townSelection.selectedTownOption',
    clearTownForCustomCoordinates: 'WorksheetLocations.townSelection.clearTownForCustomCoordinates',
    timeZoneOffsetHours: 'WorksheetLocations.dates.timeZoneOffsetHours',
    formatOffsetHours: 'WorksheetLocations.dates.formatOffsetHours',
    syncTownSelectors: 'WorksheetLocations.townSelection.syncTownSelectors',
    calendarLocationContext: 'WorksheetLocations.calendar.calendarLocationContext',
    acceptCalendarLocationContext: 'WorksheetLocations.calendar.acceptCalendarLocationContext',
    markDatetimeOffsetTouched: 'WorksheetLocations.calendar.markDatetimeOffsetTouched',
    readCalendarControls: 'WorksheetLocations.calendar.readCalendarControls',
    calendarState: 'WorksheetLocations.calendar.calendarState',
    applyCalendarState: 'WorksheetLocations.calendar.applyCalendarState',
    restoreCalendarHistory: 'WorksheetLocations.calendarActions.restoreCalendarHistory',
    calendarElements: 'WorksheetLocations.calendar.calendarElements',
    restoreDatetimeDefaultsIfBlank: 'WorksheetLocations.calendar.restoreDatetimeDefaultsIfBlank',
    restoreAlmanacDefaultsIfBlank: 'WorksheetLocations.calendar.restoreAlmanacDefaultsIfBlank',
    currentDatetimeState: 'WorksheetLocations.calendar.currentDatetimeState',
    currentAlmanacState: 'WorksheetLocations.calendar.currentAlmanacState',
    almanacSummaryText: 'WorksheetLocations.calendar.almanacSummaryText',
    datetimeSummaryText: 'WorksheetLocations.calendar.datetimeSummaryText',
    applyCalendarEvaluationFields: 'WorksheetLocations.calendar.applyCalendarEvaluationFields',
    applyAlmanacTotalityAction: 'WorksheetLocations.calendarActions.applyAlmanacTotalityAction',
    refreshDatetimeJurisdictionLocation: 'WorksheetLocations.calendarActions.refreshDatetimeJurisdictionLocation',
    triggerDatetimeAutoEvaluation: 'WorksheetLocations.calendarActions.triggerDatetimeAutoEvaluation',
    triggerAlmanacAutoEvaluation: 'WorksheetLocations.calendarActions.triggerAlmanacAutoEvaluation',
    formatAlmanacTimeFromEvent: 'WorksheetLocations.calendarActions.formatAlmanacTimeFromEvent',
    applyCalendarControlEvent: 'WorksheetLocations.calendarActions.applyCalendarControlEvent',
    marsFormNumber: 'WorksheetLocations.dates.marsFormNumber',
    parseMarsIsoDate: 'WorksheetLocations.dates.parseMarsIsoDate',
    marsTodayIsoDate: 'WorksheetLocations.dates.marsTodayIsoDate',
    marsCurrentGmtMoment: 'WorksheetLocations.dates.marsCurrentGmtMoment',
    syncRoundedSelect: 'WorksheetLocations.selects.sync',

    // WorksheetResults.
    setDatetimeLocalText: 'WorksheetResults.calendarResults.setDatetimeLocalText',

    // WorksheetLocations.
    enhanceRoundedSelect: 'WorksheetLocations.selects.enhance',
    marsTodayIsoDateForInput: 'WorksheetLocations.picker.todayForInput',
    closeMarsDatePicker: 'WorksheetLocations.picker.close',
    placeMarsDatePicker: 'WorksheetLocations.picker.place',
    commitMarsDateValue: 'WorksheetLocations.picker.commit',
    commitMarsTodayValue: 'WorksheetLocations.picker.commitToday',
    renderMarsDatePicker: 'WorksheetLocations.picker.render',
    openMarsDatePicker: 'WorksheetLocations.picker.open',
    setMarsDatePickerMonthYear: 'WorksheetLocations.picker.setMonthYear',
    shiftMarsDatePickerYear: 'WorksheetLocations.picker.shiftYear',
    shiftMarsDatePickerMonth: 'WorksheetLocations.picker.shiftMonth',

    // Browser HTTP transport.
    labFetch: 'labHttp.fetch',

    // WorksheetBindings.
    expressionTextOrEmpty: 'WorksheetBindings.expressionTextOrEmpty',
};

/** @brief Resolve module roots lazily because the fixture loads before application startup. */
const labTestModuleRoots: {[name: string]: object} = {
    get WorksheetRequests() { return WorksheetRequests; },
    get WorksheetWorkspace() { return WorksheetWorkspace; },
    get WorksheetBindings() { return WorksheetBindings; },
    get WorksheetPersistence() { return WorksheetPersistence; },
    get WorksheetResults() { return WorksheetResults; },
    get WorksheetLocations() { return WorksheetLocations; },
    get labHttp() { return labHttp; }
};

/** @brief Forward one fixture property, resolving replacement owners on every access. */
function labTestModuleProperty(path: string): PropertyDescriptor {
    const parts = path.split('.');
    const key = parts.pop()!;

    function owner(): object {
        let result = labTestModuleRoots[parts[0]];
        // Paths contain at most three owner components; this is not a service-name search.
        for (let index = 1; index < parts.length; ++index) {
            const next: unknown = Reflect.get(result, parts[index]);
            if (!next || typeof next !== 'object')
                throw new Error('Missing fixture module owner: ' + path);
            result = next;
        }
        return result;
    }

    return {
        get() { return Reflect.get(owner(), key); },
        set(value: unknown) { Reflect.set(owner(), key, value); }
    };
}

/** @brief Build the fixture-only forwarding object without exposing production compatibility globals. */
function createLabTestMethods(): object {
    const methods: object = Object.create(null);
    for (const name of Object.keys(labTestModulePaths))
        Object.defineProperty(methods, name, labTestModuleProperty(labTestModulePaths[name]));
    return methods;
}

/** @brief Fixture property adapters, constructed without touching application modules during startup. */
const labTestMethods = createLabTestMethods();

/** @brief Resolve a fixture replacement directly to the method on its owning instance. */
function labTestModuleOwner(name: string): object {
    return Object.hasOwn(labTestModulePaths, name) ? labTestMethods : window;
}

/** @brief Read native fixture defaults after the application has finished starting. */
function initialiseLabTestAdapters(): void {
    integratorCandidateNames = Object.freeze(labDOM.call<string[]>('lab_binding_row_names'));
    DEFAULT_EQUATION_VARIABLE_TEXT = labApplication.configuration.DEFAULT_EQUATION_VARIABLE;
}

// This copies structured DOM fields only. Native /forms owns all text interpretation.
/** @brief Normalise structured row fields through the native forms policy. */
function sanitizeIntegratorRow(row: unknown, fallbackName = 'x') {
    return labDOM.call<WorksheetIntegratorRow>('lab_binding_row_normalise', row, String(fallbackName));
}

/** @brief Ask the native row policy for an available default variable name. */
function integratorDefaultVariableName(rows: unknown[] = []) {
    return labDOM.call<string>('lab_binding_rows_name', rows);
}

/** @brief Render one structured bound row through its owning forms adapter. */
function integratorRowText(row: WorksheetIntegratorRow) {
    return WorksheetWorkspace.integrator.forms.text([row]);
}

/** @brief Parse bound text through the native request workflow. */
async function parseIntegratorBoundsText(text: unknown): Promise<WorksheetIntegratorRow[]> {
    return WorksheetRequests.runtime.continue(74, {text});
}

/** @brief Read the native default integrator rows. */
function integratorFallbackRows() {
    return labDOM.call<WorksheetIntegratorRow[]>('lab_binding_rows_default', 0);
}

/** @brief Read the native precision status label. */
function precisionStatusText() {
    return labDOM.call<string>('lab_workspace_dom_precision');
}

/** @brief Show the worksheet help using the native DOM policy. */
function showHelp() {
    labDOM.call('lab_workspace_dom_help', 1);
}

/** @brief Show the goal target using the native DOM policy. */
function showTargetEntry() {
    labDOM.call('lab_workspace_dom_target', 1);
}

/** @brief Existing storage key inspected by expression persistence regressions. */
const EXPRESSION_TIMESTAMP_STORAGE_KEY = 'mars.exprLab.lastExpressionUpdatedAt';

/** @brief Recognise the native Boolean marker for abbreviated output. */
function hasAbbreviatedValue(value: unknown) {
    return value === true;
}

/** @brief Read the native zoom index for a result card. */
function resultZoomIndex(card: HTMLElement) {
    return labWire.exports().lab_view_card_zoom(WorksheetContext.resultCardIds.get(card) ?? -1);
}

/** @brief Apply the native layout plan for a result card. */
function applyResultZoom(card: HTMLElement) {
    return labDOM.call('lab_layout_zoom', card);
}

/** @brief Set an absolute result-card zoom index. */
function setResultZoom(card: HTMLElement, index: number) {
    return labDOM.call('lab_layout_set_zoom', card, index, 0);
}

/** @brief Move the result-card zoom index by a relative step. */
function stepResultZoom(card: HTMLElement, direction: number) {
    return labDOM.call('lab_layout_set_zoom', card, direction, 1);
}

/** @brief Refresh the native result-card expansion layout. */
function renderResultCardExpansion() {
    return labDOM.call('lab_layout_expand', null, 0);
}

/** @brief Check whether the native expansion owner is this card. */
function resultCardExpanded(card: HTMLElement) {
    return labWire.exports().lab_view_card_expanded() === WorksheetContext.resultCardIds.get(card);
}

/** @brief Collapse the native result-card expansion. */
function collapseResultCards() {
    return labDOM.call('lab_layout_expand', null, 2);
}

/** @brief Toggle the result card owning the supplied button. */
function toggleResultCardExpansion(button: HTMLElement) {
    return labDOM.call('lab_layout_expand', button.closest('.result-card'), 1);
}

/** @brief Measure markup through the native layout policy. */
function svgMarkupIntrinsicWidth(markup: unknown) {
    return labDOM.call('lab_layout_markup_width', markup);
}

/** @brief Install a solver SVG variant through the native layout policy. */
function installSolverTexSvg(markup: string, variant: string) {
    return labDOM.call('lab_layout_solver_install', markup, variant);
}

/** @brief Read native expression copy text without reinterpreting its mathematics. */
function parsedExpressionText() {
    return labDOM.call('lab_result_copy', 'expression', WorksheetWorkspace.presentation.state.lastTex);
}

/** @brief Read copy text from the selected native presentation variant. */
function almanacWorksheetCopyText(data: unknown, visibility: string) {
    return almanacPresentationVariant(data, visibility).copy_text;
}

/** @brief Read a native evaluation label for the requested mode and field. */
function evaluationLabel(mode: number, field: number) {
    return labDOM.call<string>('lab_evaluation_label', mode, field);
}

/** @brief Validate date text with the native location policy and fallback. */
function validDateText(value: unknown, fallback = WorksheetContext.DEFAULT_DATETIME_DATE) {
    return labDOM.call<string>('lab_location_validate', 1, {value}, fallback, WorksheetContext.HOLIDAY_JURISDICTION_SET);
}

/** @brief Validate the selected jurisdiction through native metadata. */
function validDatetimeJurisdiction(value: unknown, fallback = WorksheetContext.DEFAULT_DATETIME_JURISDICTION) {
    return labDOM.call<string>('lab_location_validate', 2, {value}, fallback, WorksheetContext.HOLIDAY_JURISDICTION_SET);
}

/** @brief Ask the native location workflow to split a stored town value. */
function townValueParts(value: unknown) {
    return WorksheetRequests.runtime.continue(26, {action: 1, value});
}

/** @brief Ask the native location workflow whether an option matches a stored value. */
function townOptionMatchesValue(option: unknown, value: unknown) {
    return WorksheetRequests.runtime.continue(26, {action: 2, option, value});
}

/** @brief Compare browser control values through the native numeric policy. */
function numbersNearlyEqual(left: unknown, right: unknown, tolerance = 0.000001) {
    return Boolean(labWire.exports().lab_forms_nearly_equal(WorksheetLocations.dates.marsFormNumber(left), WorksheetLocations.dates.marsFormNumber(right), tolerance));
}

/** @brief Cache each mode's native calendar schema for the fixtures. */
function calendarSchema(mode: string) {
    if (!calendarSchemas.has(mode))
        calendarSchemas.set(mode, labDOM.call<LabTestCalendarSchema>('lab_location_schema', WorksheetWorkspace.mode.workspaceModeId(mode)));
    return calendarSchemas.get(mode)!;
}

/** @brief Validate the saved visibility choice through the native location policy. */
function validAlmanacVisibility(value: unknown, fallback = WorksheetContext.DEFAULT_ALMANAC_VISIBILITY) {
    return labDOM.call<string>('lab_location_validate', 3, {value}, fallback, WorksheetContext.HOLIDAY_JURISDICTION_SET);
}

/** @brief Read the native date-picker anchor rectangle. */
function marsDatePickerAnchorRect(shell: Element | null) {
    return labDOM.call('lab_widgets_picker_anchor', shell) as LabTestRectangle | null;
}

/** @brief Native calendar metadata cached per mode for this disposable fixture page. */
const calendarSchemas = new Map<string, LabTestCalendarSchema>();

/** @brief Validate precision through the native saved-state policy. */
function validPrecisionBits(bits: unknown, fallback: unknown) {
    return labDOM.call<number>('lab_storage_precision', [bits, fallback]);
}

/** @brief Read compact editor bindings from native cached metadata. */
function restoreCompactBindingValues(text: string) {
    return labDOM.call('lab_editor_metadata', 2, [text], WorksheetResults.labEvaluationView.caches.editors);
}

/** @brief Require a native presentation variant without regenerating its markup. */
function almanacPresentationVariant(data: unknown, visibility: string) {
    const variant = labDOM.call<LabTestAlmanacVariant | null>('lab_result_almanac_variant', data, visibility);
    if (!variant)
        throw new Error(
            'Almanac presentation is missing from the server response. ' +
            'Run make mars-lab-restart, reload the page and evaluate again.');
    return variant;
}
