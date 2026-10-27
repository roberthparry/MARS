/**
 * @file results_services.ts
 * @brief Browser service tables for native result and evaluation continuations.
 *
 * Native C chooses service arguments and ordering. These named adapters resolve
 * page owners at invocation time, preserving bootstrap order and replaceable
 * capabilities. Named argument tuples preserve omitted optional arguments when
 * forwarding calls, without referring readers to another method's type definition.
 * These adapters do not interpret mathematics or own timers and UI resources;
 * those responsibilities remain with the controllers in results.ts.
 */

namespace WorksheetResults {
    // Only browser-side effects live here; C selects their arguments and execution order.
    /** @brief Provide deferred browser effects for native evaluation recovery. */
    export const labEvaluationRecovery = Object.freeze({
        /** @brief Clear the reusable matrix scalar source. */
        clearMatrixScalar() {
            WorksheetWorkspace.presentation.state.lastMatrixScalarExpression = '';
        },

        /** @brief Display a native evaluation error as text. */
        setRenderedError(text: string) { return WorksheetResults.cards.setRenderedError(text); },

        /** @brief Reset rendered-result expansion availability. */
        resetRenderedDigits() { return WorksheetResults.cards.resetMoreDigitsButton(WorksheetContext.renderedMore, false); },

        /** @brief Install local-date text and optional native section metadata. */
        setDatetimeLocalText(text: string) { return WorksheetResults.calendarResults.setDatetimeLocalText(text); },

        /** @brief Reset result details, optionally preserving authored bindings. */
        clearResultDetails(options: WorksheetResultResetOptions) {
            return WorksheetResults.reset.clearResultDetails(options);
        },

        /** @brief Remove rendered-result error presentation. */
        clearRenderedError() { return WorksheetResults.cards.clearRenderedError(); },

        /** @brief Apply native integrator binding metadata to the authored text. */
        applyIntegratorBindingState(data: WorksheetEvaluationData, text: string) {
            return WorksheetBindings.integratorPanel.applyIntegratorBindingState(data, text);
        },

        /** @brief Apply the native result bound to the integrator controls. */
        applyIntegratorResultBound(data: WorksheetEvaluationData) {
            return WorksheetWorkspace.integrator.applyIntegratorResultBound(data);
        },

        /** @brief Persist the selected worksheet's current authored source. */
        saveWorksheetState(mode: WorksheetMode) { return WorksheetPersistence.persistence.saveWorksheet(mode); }
    });

    // Host mutations are deliberately deferred: native plans never redefine live workspace getters.
    /** @brief Provide deferred installation effects while C retains evaluation policy. */
    export const labEvaluationInstall = Object.freeze({
        // Rendered output and authored bindings.
        /** @brief Install native rendered output and retain its exact TeX. */
        setRenderedResult(data: WorksheetEvaluationData) { return WorksheetResults.cards.setRenderedResult(data); },

        /** @brief Install source text with optional evaluated bindings and editor metadata. */
        setExpressionEditor(...args: [text: string, bindings?: WorksheetBinding[] | null, body?: string | null, ready?: unknown]) {
            return WorksheetBindings.controller.setExpressionEditor(...args);
        },

        /** @brief Render controls for the native binding list. */
        renderVariableValues(bindings: WorksheetBinding[]) {
            return WorksheetBindings.panel.renderVariableValues(bindings);
        },

        /** @brief Clear the visible variable-value controls. */
        clearVariableValues() { return WorksheetBindings.panel.clearVariableValues(); },

        /** @brief Install the native expression, function, value and reusable-input texts. */
        installEvaluationTextCards(cards: WorksheetTextCards) {
            return WorksheetResults.cards.installEvaluationTextCards(cards);
        },

        // Native card projection and retained sources.
        /** @brief Request native mode-specific card presentation. */
        present(mode: WorksheetMode, data: WorksheetEvaluationData) {
            return labDOM.call('lab_evaluation_cards_present', mode, data);
        },

        /** @brief Render native output, retain its TeX and return that exact text. */
        render(mode: WorksheetMode, data: WorksheetEvaluationData, expandable: unknown) {
            const text = labDOM.call('lab_evaluation_render', mode, data, Number(Boolean(expandable))) as string;
            WorksheetWorkspace.presentation.state.lastTex = text;
            return text;
        },

        /** @brief Set and return one live native source field. */
        source(key: WorksheetSourceKey, text: string) { return WorksheetContext.labEditorState[key] = text; },

        /** @brief Set and return the retained editor text for a worksheet mode. */
        modeSource(mode: WorksheetMode, text: string) { return WorksheetContext.modeEditorText[mode] = text; },

        /** @brief Set and return the reusable derivative source. */
        derivative(text: string) { return WorksheetWorkspace.presentation.state.lastDerivativeExpression = text; },

        /** @brief Set and return the reusable matrix scalar source. */
        scalar(text: string) { return WorksheetWorkspace.presentation.state.lastMatrixScalarExpression = text; },

        /** @brief Accept native variable names and differentiation availability. */
        variables(variables: string[], differentiable: boolean) {
            WorksheetBindings.panel.currentVariables = variables;
            WorksheetBindings.panel.currentDifferentiable = differentiable;
            WorksheetWorkspace.presentation.renderDerivativeButtons(WorksheetBindings.panel.currentVariables);
        },

        // Persistence and secondary cards.
        /** @brief Persist the worksheet with optional text and save options. */
        saveWorksheetState(...args: [mode: WorksheetMode, text?: string, options?: WorksheetSaveOptions]) {
            return WorksheetPersistence.persistence.saveWorksheet(...args);
        },

        /** @brief Install native matrix cards and reusable bindings. */
        displayMatrixResult(data: WorksheetEvaluationData) { return WorksheetResults.cards.displayMatrixResult(data); },

        /** @brief Replace the pending rendered-result fitting frame. */
        scheduleRenderedTeXFit() { return WorksheetResults.renderedFit.schedule(); },

        /** @brief Install native solver expression and function texts with reusable input. */
        solverTextCards(cards: WorksheetTextCards) {
            WorksheetResults.cards.setExpandableText(
                WorksheetContext.parsed, WorksheetContext.parsedMore, cards.expression, cards.full_expression);
            WorksheetResults.reuse.setResultInputText(cards.input);
            WorksheetResults.cards.setExpandableText(
                WorksheetContext.functionStyle, WorksheetContext.functionMore, cards.function, cards.full_function);
        },

        /** @brief Install native numerical value text. */
        setValueText(value: unknown) { return WorksheetResults.cards.setValueText(value); },

        /** @brief Apply the native-selected Value card visibility. */
        setValueCardVisible(visible: boolean) { return WorksheetWorkspace.presentation.setValueCardVisible(visible); },

        /** @brief Apply native integrator binding metadata to the authored text. */
        applyIntegratorBindingState(data: WorksheetEvaluationData, text: string) {
            return WorksheetBindings.integratorPanel.applyIntegratorBindingState(data, text);
        },

        /** @brief Apply the native result bound to the integrator controls. */
        applyIntegratorResultBound(data: WorksheetEvaluationData) {
            return WorksheetWorkspace.integrator.applyIntegratorResultBound(data);
        },

        // Calendar result installation.
        /** @brief Render the supplied native calendar card. */
        calendarCard(card: WorksheetCalendarCard) {
            return WorksheetResults.calendarResults.renderDatetimeSections(card.element, card.button, card.sections, card.text);
        },

        /** @brief Install local-date text and optional native section metadata. */
        setDatetimeLocalText(...args: [text: string, sections?: unknown]) {
            return WorksheetResults.calendarResults.setDatetimeLocalText(...args);
        },

        /** @brief Apply native calendar fields for the evaluated mode. */
        applyCalendarEvaluationFields(mode: WorksheetMode, data: WorksheetEvaluationData) {
            return WorksheetLocations.calendar.applyCalendarEvaluationFields(mode, data);
        },

        /** @brief Retain native result text for reuse in the editor. */
        setResultInputText(text: string) { return WorksheetResults.reuse.setResultInputText(text); },

        /** @brief Persist the current DateTime worksheet. */
        saveLastDatetimeState() { return WorksheetPersistence.persistence.saveCalendar('datetime'); },

        /** @brief Persist the current Almanac worksheet. */
        saveLastAlmanacState() { return WorksheetPersistence.persistence.saveCalendar('almanac'); },

        /** @brief Render native almanac metadata in the result card. */
        almanacRender(data: WorksheetEvaluationData) {
            return WorksheetResults.calendarResults.renderAlmanacWorksheet(WorksheetContext.rendered, data);
        },

        /** @brief Retain and return the latest native almanac worksheet. */
        almanacAccept(data: WorksheetEvaluationData) {
            return WorksheetContext.worksheetSession.calendar.almanacLastWorksheetData = data;
        },

        /** @brief Continue the native almanac detail request. */
        refreshAlmanacLandTotality(data: WorksheetEvaluationData) {
            return WorksheetRequests.calendarRequests.refreshAlmanacLandTotality(data);
        }
    });

    // Getters use direct native exports only; they must not open another DOM scope.
    /** @brief Expose live native evaluation capabilities without opening nested DOM scopes. */
    export const labEvaluationView = Object.freeze({
        get caches() {
            return caches;
        },
        get editors() {
            return caches.editors;
        },
        get fullText() {
            return WorksheetContext.labEditorState.fullText;
        },
        get rawEditor() {
            return WorksheetBindings.controller.labEditorSnapshot();
        },
        get bodyText() {
            return WorksheetContext.expr.value;
        },
        get lastInput() {
            return WorksheetContext.labEditorState.lastInput;
        }
    });

    /** @brief Provide status and section-rendering capabilities for native weather completion. */
    export const labWeatherServices = Object.freeze({
        /** @brief Replace the workspace status text. */
        setStatus(status: string) { return WorksheetWorkspace.presentation.setStatus(status); },

        /** @brief Project native section metadata into the calendar result. */
        installWeather(overview: unknown, data: WorksheetEvaluationData) {
            const cards = labDOM.call('lab_evaluation_weather_cards', overview, data) as ResultCalendarSections;
            WorksheetResults.calendarResults.renderDatetimeSections(WorksheetContext.rendered, null, cards.sections, cards.text);
        }
    });

    /** @brief Provide browser mutations for native goal-seeking continuations. */
    export const labGoalServices = Object.freeze({
        /** @brief Display a native evaluation error as text. */
        setRenderedError(message: string) { return WorksheetResults.cards.setRenderedError(message); },

        /** @brief Reset rendered-result expansion availability. */
        resetRenderedDigits() { return WorksheetResults.cards.resetMoreDigitsButton(WorksheetContext.renderedMore, false); },

        /** @brief Reset result details, optionally preserving authored bindings. */
        clearResultDetails(options: WorksheetResultResetOptions) {
            return WorksheetResults.reset.clearResultDetails(options);
        },

        /** @brief Replace the workspace status text. */
        setStatus(status: string) { return WorksheetWorkspace.presentation.setStatus(status); },

        /** @brief Capture the authored expression before replacement. */
        captureHistory() {
            return WorksheetWorkspace.history.pushExpressionHistory(WorksheetBindings.editor.currentExpressionText());
        },

        /** @brief Install native rendered output and retain its exact TeX. */
        setRenderedResult(data: WorksheetEvaluationData) { return WorksheetResults.cards.setRenderedResult(data); },

        /** @brief Install the native goal result with its captured editor context. */
        setEditor(data: WorksheetEvaluationData, context: WorksheetGoalContext): void {
            WorksheetBindings.controller.setExpressionEditor(
                context.editorExpression, data.binding_values || null, context.editorBody, data.evaluation_ready);
        },

        /** @brief Install native text cards from the goal context. */
        installCards(context: WorksheetGoalContext) {
            return WorksheetResults.cards.installEvaluationTextCards(context.cards);
        },

        /** @brief Retain the evaluated goal expression as the last input. */
        setLastInput(context: WorksheetGoalContext) {
            WorksheetContext.labEditorState.lastInput = context.editorExpression;
        },

        /** @brief Clear the previous derivative source. */
        clearDerivative() {
            WorksheetWorkspace.presentation.state.lastDerivativeExpression = '';
        },

        /** @brief Accept the native goal result's variable names. */
        setVariables(data: WorksheetEvaluationData) {
            WorksheetBindings.panel.currentVariables = WorksheetWorkspace.bindingSelection.variableNamesFromBindings(data.binding_values || []);
        },

        /** @brief Accept the native goal result's differentiation flag. */
        setDifferentiable(context: WorksheetGoalContext) {
            WorksheetBindings.panel.currentDifferentiable = context.cards.differentiable;
        },

        /** @brief Refresh derivative controls for the retained variables. */
        renderDerivatives() {
            return WorksheetWorkspace.presentation.renderDerivativeButtons(WorksheetBindings.panel.currentVariables);
        },

        /** @brief Retain the goal's source expression. */
        setSource(context: WorksheetGoalContext) {
            WorksheetContext.labEditorState.goalSource = WorksheetBindings.expressionTextOrEmpty(context.seek.expression).trim();
        },

        /** @brief Retain the goal's requested target. */
        setTarget(context: WorksheetGoalContext) {
            WorksheetContext.labEditorState.goalTarget = context.target;
        },

        /** @brief Hide goal-target entry after native completion. */
        hideTargetEntry() { return WorksheetWorkspace.presentation.hideTargetEntry(); },

        /** @brief Display the native goal-completion status. */
        setSuccessStatus(context: WorksheetGoalContext) {
            const status = labDOM.call('lab_goal_status', context.cards) as string;
            return WorksheetWorkspace.presentation.setStatus(status);
        }
    });

    /** @brief Native result protocol table with late-bound owner calls and unchanged service keys. */
    export const resultServices = {

        // Result reuse and editor state.
        /** @brief Read native result text suitable for editor reuse. */
        resultInput() { return WorksheetResults.reuse.resultExpressionTextForInput(); },

        /** @brief Capture mode, authored editor text and request context. */
        resultSnapshot() {
            return {
                mode: WorksheetWorkspace.mode.currentMode(), editor: WorksheetContext.expr.value,
                context: WorksheetRequests.labRequests.context()
            };
        },

        /** @brief Request native editor metadata for supplied source text. */
        resultPrepare(text: string) { return WorksheetRequests.presentation.prepareLabEditor(text); },

        /** @brief Compare native worksheet snapshots. */
        resultHistoryEqual(left: WorksheetSnapshot, right: WorksheetSnapshot) {
            return WorksheetPersistence.history.equals(left, right);
        },

        /** @brief Cancel goal seeking and hide its target entry. */
        resultClearGoal() {
            WorksheetBindings.goal.clearGoalSeekRequest();
            WorksheetWorkspace.presentation.hideTargetEntry();
        },

        /** @brief Read the native compact form of the current authored expression. */
        resultSourceBindings() {
            return WorksheetBindings.editor.compactExpressionForEditor(WorksheetBindings.editor.currentExpressionText());
        },

        /** @brief Read bindings retained with the current result. */
        resultBindings() { return WorksheetWorkspace.presentation.state.resultInputBindings; },

        /** @brief Restore authored binding values from source and optional visible bindings. */
        resultAuthoredBindings(...args: [bindings: WorksheetBinding[], source: string, visible?: WorksheetBinding[]]) {
            return WorksheetBindings.editor.bindingsWithAuthoredValues(...args);
        },

        /** @brief Apply native binding text with an optional editor body. */
        resultApplyBinding(...args: [text: string, body?: string | null]) {
            return WorksheetBindings.controller.applyMarsBindingExpression(...args);
        },

        /** @brief Reset matrix controls and synchronise their browser presentation. */
        resultMatrixControls() {
            WorksheetContext.matrixOperation.value = 'eval';
            WorksheetContext.matrixOperand.value = '';
            WorksheetLocations.selects.sync(WorksheetContext.matrixOperation);
            WorksheetWorkspace.presentation.syncMatrixControls();
        },

        /** @brief Persist the current mode's editor state. */
        resultSaveEditor() { return WorksheetWorkspace.mode.saveCurrentModeEditorState(); },

        // Presentation and copying.
        /** @brief Request native presentation metadata. */
        resultPresentation(payload: unknown) { return WorksheetRequests.presentation.requestLabPresentation(payload); },

        /** @brief Read the retained exact native TeX. */
        resultLastTeX() { return WorksheetWorkspace.presentation.state.lastTex; },

        /** @brief Run native SVG rendering for supplied TeX. */
        resultRenderTeX(TeX: string) { return WorksheetResults.actions.renderTexSvg(TeX); },

        /** @brief Read the native copy source selected by the button. */
        resultCopyText(button: WorksheetCopyButton) {
            return WorksheetResults.clipboard.copyTextForTarget(button.dataset.copyTarget);
        },

        /** @brief Read the current native DateTime summary. */
        resultDatetimeSummary() { return WorksheetLocations.calendar.datetimeSummaryText(); },

        /** @brief Write exact result text to the browser clipboard. */
        resultClipboard(text: string) { return WorksheetResults.clipboard.write(text); },

        /** @brief Replace one copy button's success or failure feedback. */
        resultFlash(...args: [button: WorksheetCopyButton, ok: boolean]) {
            return WorksheetResults.clipboard.flash(...args);
        },

        /** @brief Schedule status feedback and return its owned timer. */
        resultStatusLater(text: string, delay: number) { return actions.statusLater(text, delay); },

        // Network and startup.
        /** @brief Post an encoded native payload and return the HTTP response. */
        resultFetch(url: string, payload: unknown) {
            return labHttp.fetch(url, {method: 'POST', body: labWire.encode(payload)});
        },

        /** @brief Decode a bounded native HTTP response. */
        resultDecode(response: WorksheetResponse) { return response.labData(); },

        /** @brief Capture request ownership for native startup policy. */
        resultStartupSnapshot() {
            return ({context: WorksheetRequests.labRequests.context(), main: WorksheetRequests.labRequests.latestMain()});
        },

        /** @brief Restore the default native integrator bounds. */
        resultRestoreBounds() {
            return WorksheetWorkspace.integrator.restoreIntegratorBoundsText(WorksheetContext.DEFAULT_INTEGRATOR_BOUNDS_TEXT);
        },

        /** @brief Restore persisted worksheet state. */
        async resultLoadState() {
            await WorksheetPersistence.restoration.load();
            await worksheetHistoryPersistence.start();
            WorksheetWorkspace.presentation.syncModeTabs();
            WorksheetWorkspace.presentation.syncModeUI();
            document.documentElement.removeAttribute('data-starting');
            labApplication.worksheetRestored();
        },

        /** @brief Evaluate the restored mode when native startup policy requests it. */
        resultInitialEvaluation() { return WorksheetRequests.evaluation.evaluateActiveModeOnLoad(); }
    };
}
