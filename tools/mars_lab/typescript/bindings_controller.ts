/**
 * @file bindings_controller.ts
 * @brief Browser ownership of binding commits, editor refreshes and native callbacks.
 *
 * Serialises input commits and owns refresh requests and feedback timers. Native C
 * chooses the workflow, validates bindings and prepares editor projections. Service
 * tables use named methods and resolve borrowed capabilities when called; they do
 * not retain replacement methods or interpret mathematics. This file creates its
 * own page controller; its lazy dependencies allow concurrent definition loading.
 */

namespace WorksheetBindings {
    /** @brief Binding workflows implemented by lab_flow_binding.c and lab_flow_binding_edit.c. */
    export enum BindingFlow {
        AssembleExpression = 40,
        CommitVisible = 41,
        CommitRequest = 42,
        ToggleKind = 43,
        RefreshValues = 44,
        QueueInput = 45,
        GoalStarts = 46,
        EditIntegratorRow = 47,
        PrepareIntegratorInput = 48,
        CardAction = 49,
        DeferEditor = 50,
        CommitInputs = 51,
        ApplyExpression = 52,
        ApplyEdited = 53,
        RefreshEdited = 54,
        ReplaceKind = 55
    }

    /** @brief Fields installed by native binding synchronisation. */
    interface BindingProjection {
        variables: string[];
        differentiable: boolean;
    }

    /** @brief Default freshness guard when a caller has no request to check. */
    function bindingIsCurrent(): boolean { return true; }

    /** Owns binding commit sequencing, refresh timers and native continuation capabilities. */
    export class WorksheetBindingController {
        /** Retain narrow host capabilities; DOM and request services are resolved after startup. */
        public constructor(private readonly host: ControllerDependencies) {
            marsBindCallbacks(this, [
                'labEditorSnapshot', 'clearExpressionSource', 'applyUpdatedBindingExpression',
                'applyMarsBindingExpression', 'applyMarsBindingsToEditedExpression',
                'scheduleEditedExpressionBindingRefresh', 'commitLabBindingValues', 'commitBindingInput',
                'commitVisibleBindingInputs', 'toggleBindingKind', 'refreshVariableValuesFromEditor',
                'queueBindingInputCommit', 'setExpressionEditor', 'refreshWorksheetFromInput',
                'labBindingFlowServices', 'dispose'
            ]);
        }

        /** @brief Timer for the latest admitted editor refresh, or zero when cleared. */
        public expressionBindingRefreshTimer = 0;

        /** @brief Tail of the input commit queue, including status error handling. */
        public pendingExpressionBindingCommit: Promise<unknown> = Promise.resolve();
        private readonly feedbackTimers = new Set<number>();
        private readonly refreshRequests = new Set<WorksheetRequest>();
        private registry: NativeServiceRegistry | undefined;

        private readonly commitEffects = this.createCommitEffects();

        /** @brief Browser effects selected after a native binding commit. */
        private createCommitEffects(): NativeServiceRegistry {
            const owner = this;
            return Object.freeze({
                /** @brief Retain the committed source text. */
                source(text: string) { return owner.host.sources.fullText = text; },

                /** @brief Update native committed values and the browser cache. */
                cache(snapshot: WorksheetSnapshot, editor: HTMLTextAreaElement) {
                    return owner.host.dom.call('lab_binding_committed_cache', snapshot, editor, owner.host.bindingPanel.bindingValueCache);
                },

                /** @brief Apply the updated native expression projection. */
                applyUpdatedBindingExpression(text: string) { return owner.applyUpdatedBindingExpression(text); },

                /** @brief Commit a projected expression, preserving omitted arguments. */
                applyMarsBindingExpression(...args: [updated: string, editorBodyText?: string | null]) {
                    return owner.applyMarsBindingExpression(...args);
                },

                /** @brief Refresh binding values with an optional freshness guard. */
                refreshVariableValuesFromEditor(...args: [isCurrent?: WorksheetGuard]) {
                    return owner.refreshVariableValuesFromEditor(...args);
                },

                /** @brief Refresh history controls. */
                updateHistoryButtons() { return owner.host.workspacePresentation.updateHistoryButtons(); },

                /** @brief Save the current mode's editor state. */
                saveCurrentModeEditorState() { return owner.host.workspaceMode.saveCurrentModeEditorState(); },
            });
        }

        /** Cancel owned refresh work; in-flight native request acceptance remains request-owned. */
        public dispose(): void {
            this.host.clearTimer(this.expressionBindingRefreshTimer);
            this.expressionBindingRefreshTimer = 0;
            for (const timer of this.feedbackTimers) this.host.clearTimer(timer);
            this.feedbackTimers.clear();
            for (const request of this.refreshRequests) this.host.requests.finish(request);
            this.refreshRequests.clear();
        }

        /** Stage trimmed editor text as UTF-8 and return its native buffer length. */
        public labEditorSnapshot(): LabBindingEditorSnapshot {
            const text = this.host.input.value.trim();
            return {text, length: this.host.buffer.stageText(text)};
        }

        /** Apply the native plan for clearing the stored expression source. */
        public clearExpressionSource(): void {
            this.host.dom.services(this.host.dom.call('lab_binding_projection_clear', 0) as NativeServicePlan, this.labBindingFlowServices());
        }

        /** Apply the native editor projection for an updated binding expression. */
        public applyUpdatedBindingExpression(updated: string): void {
            const modeId = this.host.workspaceMode.workspaceModeId(this.host.workspaceMode.currentMode());
            const metadata = this.host.bindingEditor.labEditorData(updated);
            this.host.dom.services(
                this.host.dom.call('lab_binding_projection_update', modeId, updated, metadata) as NativeServicePlan,
                this.labBindingFlowServices());
        }

        /** Project a binding expression within an owned binding-commit request. */
        public async applyMarsBindingExpression(updated: string, editorBodyText: string | null = null): Promise<boolean | undefined> {
            const mode = this.host.workspaceMode.currentMode(), editorSnapshot = this.host.input.value;
            const host = this.host;
            // This callback retains the text admitted with this particular request.
            return host.requests.run('bindingCommit', mode, function applyExpression(request) {
                return host.flow(BindingFlow.ApplyExpression, {updated, editorBodyText, editorSnapshot, request});
            }) as Promise<boolean | undefined>;
        }

        /** Synchronise evaluated bindings with edited text under the supplied request. */
        public async applyMarsBindingsToEditedExpression(
            editedBody: string, sourceExpression: string, data: WorksheetEvaluationData, request: WorksheetRequest
        ): Promise<void> {
            return this.host.flow(BindingFlow.ApplyEdited, {editedBody, sourceExpression, data, request}) as Promise<void>;
        }

        /** Finish the admitted refresh request even when native preparation fails. */
        private async refreshEditedExpressionBindings(editedBody: string, sourceExpression: string, request: WorksheetRequest): Promise<void> {
            try {
                return await this.host.flow(BindingFlow.RefreshEdited, {editedBody, sourceExpression, request}) as void;
            } finally {
                this.host.requests.finish(request);
                this.refreshRequests.delete(request);
            }
        }

        /** Apply native refresh admission and scheduling plans for edited bindings. */
        public scheduleEditedExpressionBindingRefresh(): void {
            const context = {editedBody: this.host.input.value, sourceExpression: this.host.sources.fullText};
            // Native phase 0 admits the request; phase 1 marks controls and schedules its refresh.
            for (let phase = 0; phase < 2; ++phase) {
                const plan = this.host.dom.call('lab_binding_projection_refresh', phase, context) as NativeServicePlan;
                this.host.dom.services(plan, this.labBindingFlowServices());
            }
        }

        /** Commit selected inputs while honouring the caller's freshness guard. */
        public async commitLabBindingValues(inputs: HTMLInputElement[], isCurrent: WorksheetGuard = bindingIsCurrent): Promise<boolean | undefined> {
            return this.host.flow(BindingFlow.CommitInputs, {inputs, isCurrent}) as Promise<boolean | undefined>;
        }

        /** Commit a single binding input through the shared native workflow. */
        public async commitBindingInput(input: HTMLInputElement): Promise<boolean | undefined> {
            return this.commitLabBindingValues([input]);
        }

        /** Commit visible inputs after native pending-work and freshness checks. */
        public async commitVisibleBindingInputs(isCurrent: WorksheetGuard = bindingIsCurrent): Promise<boolean | undefined> {
            return this.host.flow(BindingFlow.CommitVisible, {isCurrent}) as Promise<boolean | undefined>;
        }

        /** Delegate a binding-kind toggle to the native continuation. */
        public async toggleBindingKind(binding: WorksheetBinding): Promise<void> {
            return this.host.flow(BindingFlow.ToggleKind, {binding}) as Promise<void>;
        }

        /** Refresh binding controls from editor text while honouring request freshness. */
        public async refreshVariableValuesFromEditor(isCurrent: WorksheetGuard = bindingIsCurrent): Promise<void> {
            return this.host.flow(BindingFlow.RefreshValues, {isCurrent}) as Promise<void>;
        }

        /** Serialise input commits and report rejected work through worksheet status. */
        public queueBindingInputCommit(input: HTMLInputElement): Promise<unknown> {
            const host = this.host;
            const mode = host.workspaceMode.currentMode();
            // Each queued commit retains its own mode and input until preceding work finishes.
            const pending = this.pendingExpressionBindingCommit.then(function commitInput() {
                return host.flow(BindingFlow.QueueInput, {mode, input});
            });
            this.pendingExpressionBindingCommit = pending.catch(function reportError(error: unknown) {
                host.workspacePresentation.setStatus(String(error));
            });
            return this.pendingExpressionBindingCommit;
        }

        /** Apply native editor preparation and projection without rewriting mathematics. */
        public setExpressionEditor(
            fullText: string, evaluatedBindings: WorksheetBinding[] | null = null,
            editorBodyText: string | null = null, evaluationReady: unknown = null
        ): void {
            this.host.dom.services(
                this.host.dom.call(
                    'lab_binding_projection_prepare',
                    {mode: this.host.workspaceMode.currentMode(), fullText, evaluatedBindings, editorBodyText, evaluationReady},
                    this.host.bindingEditor.labEditorData(fullText)) as NativeServicePlan,
                this.labBindingFlowServices());
            const mode = this.host.workspaceMode.workspaceModeId(this.host.workspaceMode.currentMode());
            const selected = this.host.dom.call(
                'lab_binding_editor', mode, {
                    text: fullText,
                    metadata: this.host.bindingEditor.labEditorData(fullText),
                    body: editorBodyText,
                    bodyMetadata: this.host.bindingEditor.labEditorData(editorBodyText),
                    bindings: evaluatedBindings,
                    ready: evaluationReady
                },
                mode === 4 ? this.host.workspaceIntegrator.currentIntegratorBoundNames() : null);
            this.host.dom.services(this.host.dom.call('lab_binding_projection_finish', selected) as NativeServicePlan, this.labBindingFlowServices());
        }

        /** Apply native input decisions while retaining editor storage and refresh timers in the host. */
        public refreshWorksheetFromInput(): void {
            this.host.requests.cancel('evaluate');
            this.host.requests.cancel('bindings');
            const mode = this.host.workspaceMode.currentMode(), native = this.host.wire.exports();
            const plan = this.host.dom.call(
                'lab_events_refresh', this.host.workspaceMode.workspaceModeId(mode), Number(!!this.host.bindingEditor.bindingParts(this.host.input.value)),
                native.lab_workspace_source_matches(this.host.buffer.stageText(this.host.input.value.trim())),
                {editor: this.host.input, defaultDatetimeText: this.host.defaultDatetimeText}) as NativeServicePlan;
            const owner = this;
            this.host.dom.services(plan, {
                /** @brief Retain the native-selected mode editor text. */
                setModeEditor(mode: WorksheetMode, text: string) {
                    owner.host.modeEditors[mode] = text;
                },

                /** @brief Retain the named native source projection. */
                setSource(field: WorksheetSourceKey, text: string) {
                    owner.host.sources[field] = text;
                },
                refreshVariableValuesFromEditor: this.refreshVariableValuesFromEditor.bind(this),

                /** @brief Save the native-selected text and persistence options. */
                saveState(mode: WorksheetMode, state: {text: string; options?: WorksheetSaveOptions}) {
                    return owner.host.persistence.saveWorksheet(mode, state.text, state.options);
                },

                /** @brief Cancel the pending refresh timer. */
                cancelBindingRefresh() { owner.host.clearTimer(owner.expressionBindingRefreshTimer); },

                /** @brief Apply native refresh flags to the editor. */
                markBindingRefresh(editor: HTMLTextAreaElement) {
                    return owner.host.dom.call('lab_events_refresh_mark', editor);
                },
                clearGoalSeekRequest: this.host.bindingGoal.clearGoalSeekRequest.bind(this.host.bindingGoal),
                scheduleEditedExpressionBindingRefresh: this.scheduleEditedExpressionBindingRefresh.bind(this),
                updateHistoryButtons: this.host.workspacePresentation.updateHistoryButtons.bind(this.host.workspacePresentation)
            });
        }

        /** Return the lazily created browser capability registry for native binding flows. */
        public labBindingFlowServices(): NativeServiceRegistry {
            return this.registry ||= this.createLabBindingServices();
        }

        /** Construct late-bound capabilities so replacement methods remain observable. */
        private createLabBindingServices(): NativeServiceRegistry {
            const owner = this;
            return Object.freeze({
                // Refresh admission, timers and editor projection.

                /** @brief Cancel the pending editor refresh timer. */
                bindingCancelRefresh() { return owner.host.clearTimer(owner.expressionBindingRefreshTimer); },

                /** @brief Admit and retain a refresh request until completion or disposal. */
                bindingBeginRefresh(
                    context: {request?: WorksheetRequest | null}, operation: string,
                    mode: WorksheetMode, options: WorksheetRequests.MarsApiOptions | undefined
                ) {
                    const request = owner.host.requests.begin(operation, mode, options);
                    context.request = request;
                    if (request) owner.refreshRequests.add(request);
                    return request;
                },

                /** @brief Mark editor bindings as awaiting refresh. */
                bindingRefreshPending() { return owner.host.input.dataset.bindingRefreshValid = 'pending'; },

                /** @brief Refresh history controls. */
                bindingRefreshHistory() { return owner.host.workspacePresentation.updateHistoryButtons(); },

                /** @brief Schedule refinement of the captured editor text and request. */
                bindingRefreshTimer(editedBody: string, sourceExpression: string, request: WorksheetRequest, delay: number) {
                    // Retain this refresh's exact input and admitted request until the timer fires.
                    owner.expressionBindingRefreshTimer = owner.host.setTimer(function refreshBindings() {
                        void owner.refreshEditedExpressionBindings(editedBody, sourceExpression, request);
                    }, delay);
                },

                /** @brief Store the named native source projection. */
                bindingSource(field: WorksheetSourceKey, value: string) { return owner.host.sources[field] = value; },

                /** @brief Discard cached binding values. */
                bindingResetCache() { return owner.host.bindingPanel.bindingValueCache = new Map(); },

                /** @brief Remove editor readiness and refresh flags. */
                bindingClearFlags() {
                    delete owner.host.input.dataset.bindingRefreshValid;
                    delete owner.host.input.dataset.evaluationReady;
                },

                /** @brief Clear the retained goal-seek source. */
                bindingClearGoal() { return owner.host.bindingGoal.clearGoalSeekRequest(); },

                /** @brief Clear the expression source through its native projection. */
                bindingClearSource() { return owner.clearExpressionSource(); },

                /** @brief Remove rendered binding controls. */
                bindingClearValues() { return owner.host.bindingPanel.clearVariableValues(); },

                /** @brief Install editor display text. */
                bindingSetText(text: string) { return owner.host.input.value = text; },

                /** @brief Continue deferred native editor preparation. */
                bindingDeferEditor(context: Record<string, unknown>) {
                    void owner.host.flow(BindingFlow.DeferEditor, context);
                },

                /** @brief Schedule the editor resize grip. */
                bindingResize() { return owner.host.workspaceLayout.scheduleEditorResizeGrip(); },

                /** @brief Render the supplied native binding values. */
                bindingRenderValues(bindings: WorksheetBinding[]) {
                    return owner.host.bindingPanel.renderVariableValues(bindings);
                },

                /** @brief Retain variable names selected by native code. */
                bindingSetVariables(bindings: WorksheetBinding[]) {
                    return owner.host.bindingPanel.currentVariables = owner.host.workspaceBindingSelection.variableNamesFromBindings(bindings);
                },

                /** @brief Refresh derivative controls for the current variables. */
                bindingRenderDerivatives() {
                    return owner.host.workspacePresentation.renderDerivativeButtons(owner.host.bindingPanel.currentVariables);
                },

                // Commit admission, request freshness and effects.

                /** @brief Capture the current mode, source and editor for freshness checks. */
                bindingCapture(): WorksheetBindingCapture {
                    return {
                        mode: owner.host.workspaceMode.currentMode(),
                        modeId: owner.host.workspaceMode.workspaceModeId(owner.host.workspaceMode.currentMode()),
                        source: owner.host.bindingEditor.currentExpressionText(),
                        text: owner.host.input.value,
                        editor: owner.host.input
                    };
                },

                /** @brief Invoke the caller's freshness guard. */
                bindingCurrent(callback: WorksheetGuard) { return callback(); },

                /** @brief Return the currently queued input commit. */
                bindingPending() { return owner.pendingExpressionBindingCommit; },

                /** @brief Commit inputs from the current binding panel. */
                bindingCommitVisible(isCurrent: WorksheetGuard) {
                    const inputs = owner.host.container.querySelectorAll<HTMLInputElement>('.binding-value-input');
                    return owner.commitLabBindingValues(Array.from(inputs), isCurrent);
                },

                /** @brief Run a binding commit within an owned request. */
                bindingRunCommit(frame: WorksheetBindingCommitFrame) {
                    // Capture this commit's inputs; later queued edits must not replace them.
                    return owner.host.requests.run('bindingCommit', frame.before.mode, function runCommit(request) {
                        return owner.host.flow(BindingFlow.CommitRequest, {
                            request, source: frame.before.source, modeId: frame.before.modeId,
                            inputs: frame.inputs, isCurrent: frame.isCurrent
                        });
                    });
                },

                /** @brief Commit one binding input. */
                bindingCommitInput(input: HTMLInputElement) { return owner.commitBindingInput(input); },

                /** @brief Request native editor metadata. */
                bindingPrepare(text: string) { return owner.host.requestPresentation.prepareLabEditor(text); },

                /** @brief Request native presentation, preserving omitted arguments. */
                bindingPresentation(...args: [payload: unknown, request?: WorksheetRequest | null]) {
                    return owner.host.requestPresentation.requestLabPresentation(...args);
                },

                /** @brief Read the expression from native editor metadata. */
                bindingExpression(data: WorksheetEvaluationData) { return data.editor.expression; },

                /** @brief Reject a missing native binding record. */
                bindingInvalid() {
                    throw new TypeError('Cannot read properties of null or undefined binding');
                },

                /** @brief Apply native-selected commit effects. */
                bindingEffects(plan: NativeServicePlan) { return owner.host.dom.services(plan, owner.commitEffects); },

                /** @brief Change a binding kind using native expression policy. */
                bindingReplaceKind(...args: [sourceExpression: string, targetName: string, nextKind: string]) {
                    return owner.host.bindingEditor.replaceBindingKindInExpression(...args);
                },

                /** @brief Apply the native editor-refresh scheduling plan. */
                bindingScheduleRefresh() { return owner.scheduleEditedExpressionBindingRefresh(); },

                /** @brief Render visible bindings and their derivative controls. */
                bindingProjectVisible(source: string) {
                    const compact = owner.host.bindingEditor.compactExpressionForEditor(source);
                    const bindings = owner.host.workspaceBindingSelection.visibleBindingsForCurrentMode(compact.bindings);
                    owner.host.bindingPanel.renderVariableValues(bindings);
                    owner.host.bindingPanel.currentVariables = owner.host.workspaceBindingSelection.variableNamesFromBindings(bindings);
                    owner.host.workspacePresentation.renderDerivativeButtons(owner.host.bindingPanel.currentVariables);
                },

                // Integrator rows and binding-card actions.

                /** @brief Apply a native row edit and refresh integrator forms. */
                bindingPrepareRows(index: number, operation: number) {
                    const rows = owner.host.workspaceIntegrator.currentIntegratorRows();
                    const edited = owner.host.forms.edit(rows, index, operation);
                    return owner.host.workspaceIntegrator.refreshIntegratorForms(edited);
                },

                /** @brief Render native-prepared integrator rows. */
                bindingRenderRows(rows: WorksheetIntegratorRow[]) {
                    return owner.host.bindingIntegratorPanel.renderIntegratorRows(rows);
                },

                /** @brief Attach an edit error to its row. */
                bindingRowError(item: HTMLElement, error: {message: string}) { return item.title = error.message; },

                /** @brief Refresh binding controls from editor text. */
                bindingRefresh() { return owner.refreshVariableValuesFromEditor(); },

                /** @brief Refresh the current integrator forms. */
                bindingRefreshForms() { return owner.host.workspaceIntegrator.refreshIntegratorForms(); },

                /** @brief Install native-prepared integrator control metadata. */
                bindingUpdateForms(prepared: unknown) {
                    return owner.host.dom.call('lab_binding_integrator_update', owner.host.integratorContainer, prepared);
                },

                /** @brief Report a native validation error on its input. */
                bindingValidity(input: HTMLInputElement, error: {message: string}) {
                    input.setCustomValidity(error.message);
                    input.reportValidity();
                },

                /** @brief Queue one input commit. */
                bindingQueue(input: HTMLInputElement) { return owner.queueBindingInputCommit(input); },

                /** @brief Restore focus after a native card action. */
                bindingRefocus(card: WorksheetCalendarCard) {
                    return owner.host.dom.call('lab_binding_refocus', owner.host.container, card, owner.host.input);
                },

                /** @brief Evaluate through the current worksheet mode. */
                bindingEvaluate() { return owner.host.workspaceMode.evaluateFromKeyboard(); },

                /** @brief Write card text to the browser clipboard. */
                bindingClipboard(text: string) { return owner.host.results.writeClipboardText(text); },

                /** @brief Show copy feedback on its button. */
                bindingFlash(button: WorksheetCopyButton, success: boolean) {
                    return owner.host.results.flashCopyButton(button, success);
                },

                /** @brief Schedule owned status feedback. */
                bindingReadyTimer() {
                    // Capture the timer ID so completion removes only its own feedback task.
                    const timer = owner.host.setTimer(function showReady() {
                        owner.feedbackTimers.delete(timer);
                        owner.host.workspacePresentation.setStatus('Ready');
                    }, 1000);
                    owner.feedbackTimers.add(timer);
                    return timer;
                },

                // Evaluation results and edited-expression synchronisation.

                /** @brief Install native editor text, preserving omitted arguments. */
                bindingSetEditor(...args: [
                    fullText: string, evaluatedBindings?: WorksheetBinding[] | null,
                    editorBodyText?: string | null, evaluationReady?: unknown
                ]) {
                    return owner.setExpressionEditor(...args);
                },

                /** @brief Fetch evaluation metadata for binding synchronisation. */
                bindingFetch(text: string, source: string, request: WorksheetRequest) {
                    return owner.host.evaluationTransport.fetchEvaluation(text, '', 'bindings', source, '', request);
                },

                /** @brief Reconcile native values with authored bindings. */
                bindingAuthored(bindings: WorksheetBinding[], source: string) {
                    return owner.host.bindingEditor.bindingsWithAuthoredValues(bindings, source);
                },

                /** @brief Read the native editor body. */
                bindingBody(text: string) { return owner.host.bindingEditor.expressionBodyForEditor(text); },

                /** @brief Save the current mode's editor state. */
                bindingSaveMode() { return owner.host.workspaceMode.saveCurrentModeEditorState(); },

                /** @brief Read binding metadata and body from native projections. */
                bindingInspectText(text: string) {
                    return {
                        parts: owner.host.bindingEditor.bindingParts(text),
                        body: owner.host.bindingEditor.expressionBodyForEditor(text)
                    };
                },

                /** @brief Install synchronised editor and binding projections. */
                bindingProjectEdited(assembled: string, body: string, bindings: WorksheetBinding[], data: WorksheetEvaluationData) {
                    owner.host.sources.fullText = assembled;
                    owner.host.sources.displayText = body;
                    const projected = owner.host.dom.call('lab_binding_sync_apply', owner.host.input, {displayText: body, bindings, data}) as BindingProjection;
                    owner.host.bindingPanel.renderVariableValues(bindings);
                    owner.host.bindingPanel.currentVariables = projected.variables;
                    owner.host.bindingPanel.currentDifferentiable = projected.differentiable;
                    owner.host.workspacePresentation.renderDerivativeButtons(owner.host.bindingPanel.currentVariables);
                    owner.host.persistence.saveWorksheet('expression', owner.host.sources.fullText, {debounce: true});
                },

                /** @brief Apply evaluated bindings under the supplied request. */
                bindingApplyEdited(...args: [editedBody: string, sourceExpression: string, data: WorksheetEvaluationData, request: WorksheetRequest]) {
                    return owner.applyMarsBindingsToEditedExpression(...args);
                },

                /** @brief Mark the editor's binding refresh as invalid. */
                bindingMarkInvalid() { return owner.host.input.dataset.bindingRefreshValid = 'false'; }
            });
        }
    }

    /** Session-owned binding commit sequencing, refresh timers and native continuation capabilities. */
    export const controller: WorksheetBindingController = new WorksheetBindingController({
        get input() { return WorksheetContext.expr; },
        get sources() { return WorksheetContext.labEditorState; },
        get modeEditors() { return WorksheetContext.modeEditorText; },
        get container() { return WorksheetContext.variableValues; },
        get integratorContainer() { return WorksheetContext.integratorBoundStack; },
        get defaultDatetimeText() { return WorksheetContext.DEFAULT_DATETIME_TEXT; },
        setTimer(callback, delay) { return setTimeout(callback, delay); },
        clearTimer(timer) { clearTimeout(timer); },
        get requests() { return WorksheetRequests.labRequests; },
        get flow() { return WorksheetRequests.runtime.continue.bind(WorksheetRequests.runtime); },
        get dom() { return labDOM; },
        get wire() { return labWire; },
        get requestPresentation() { return WorksheetRequests.presentation; },
        get evaluationTransport() { return WorksheetRequests.evaluationTransport; },
        get persistence() { return WorksheetPersistence.persistence; },
        results: {
            writeClipboardText(text) { return WorksheetResults.clipboard.write(text); },
            flashCopyButton(button, success) { WorksheetResults.clipboard.flash(button, success); }
        },
        get bindingEditor() { return WorksheetBindings.editor; },
        get workspaceBindingSelection() { return WorksheetWorkspace.bindingSelection; },
        get workspaceIntegrator() { return WorksheetWorkspace.integrator; },
        get workspaceMode() { return WorksheetWorkspace.mode; },
        get workspaceLayout() { return WorksheetWorkspace.layout; },
        get workspacePresentation() { return WorksheetWorkspace.presentation; },
        get forms() { return WorksheetWorkspace.integrator.forms; },
        get bindingPanel() { return WorksheetBindings.panel; },
        get bindingIntegratorPanel() { return WorksheetBindings.integratorPanel; },
        get bindingGoal() { return WorksheetBindings.goal; },
        get buffer() { return WorksheetWorkspace.labWorkspaceBuffer; }
    });

}
