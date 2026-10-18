/**
 * @file workspace_controllers.ts
 * @brief Browser owners for worksheet modes, forms, precision and presentation.
 *
 * Defines controllers without initialising page state. Native C selects mode,
 * binding and precision policy and projects the controls. These adapters retain
 * request callbacks, result snapshots and browser event/frame lifetimes. The
 * later workspace.ts script supplies lazy dependencies and creates page owners.
 */

namespace WorksheetWorkspace {
    /** @brief Asynchronous workspace operations implemented by native continuations. */
    enum WorkspaceFlow {
        CaptureEditor = 23,
        SelectMode = 24,
        SetPrecision = 25,
        ClearWorksheet = 72,
        RefreshIntegrator = 73,
        RestoreBounds = 75
    }

    /** @brief Synchronous commands implemented by the native state controller. */
    enum WorkspaceCommand {
        RestoreEditor = 4,
        RestoreResult = 5,
        SelectMode = 6,
        ApplyMode = 7,
        EvaluateKeyboard = 8,
        PushHistory = 10,
        NormaliseControl = 11,
        FlushState = 12,
        ApplyIntegratorResult = 14
    }

    /** @brief Default freshness guard when no request owner was supplied. */
    function workspaceIsCurrent(): boolean { return true; }

    /** Owns integrator form metadata and coordinates native row workflows. */
    export class WorksheetIntegratorController {
        /** Retain capabilities without reading catalogue-dependent controls during construction. */
        public constructor(private readonly host: IntegratorDependencies) {
            marsBindCallbacks(this, [
                'integratorFormsExpression', 'refreshIntegratorForms', 'integratorBlankRows',
                'currentIntegratorRows', 'activeIntegratorRowPlan', 'activeIntegratorBoundRows',
                'activeIntegratorRows', 'currentIntegratorBoundNames', 'applyIntegratorResultBound',
                'restoreIntegratorBoundsText', 'currentIntegratorBoundsText', 'resetIntegratorBoundsToDefault',
                'resetIntegratorBoundsToBlank', 'requestedIntegratorIntervalCap'
            ]);
        }

        /** Native row metadata belongs exclusively to this integrator instance. */
        public get forms(): WorksheetIntegratorForms { return this.host.forms; }

        /** Read the current authored expression, falling back to the visible editor text. */
        public integratorFormsExpression(): string {
            return this.host.bindingEditor.currentExpressionText() || this.host.controls.expr.value || '';
        }

        /** Await native form refresh and return the rows accepted for the current expression. */
        public async refreshIntegratorForms(
            rows = this.currentIntegratorRows(),
            expression = this.integratorFormsExpression()
        ): Promise<WorksheetIntegratorRow[] | null | undefined> {
            const result = this.host.flow(WorkspaceFlow.RefreshIntegrator, {rows, expression});
            return result as Promise<WorksheetIntegratorRow[] | null | undefined>;
        }

        /** Obtain the native default row structure for an empty integrator form. */
        public integratorBlankRows(): WorksheetIntegratorRow[] {
            return this.host.dom.call('lab_binding_rows_default', 1) as WorksheetIntegratorRow[];
        }

        /** Read structured integrator controls without interpreting their mathematical text. */
        public currentIntegratorRows(): WorksheetIntegratorRow[] {
            return this.host.dom.call('lab_binding_rows_read', this.host.controls.integratorBoundStack) as WorksheetIntegratorRow[];
        }

        /** Request the native active-row plan for the supplied rows and expression. */
        public activeIntegratorRowPlan(rows = this.currentIntegratorRows(), expressionText = ''): WorksheetRowPlan {
            return this.forms.plan(rows, expressionText || this.integratorFormsExpression());
        }

        /** Return the bound rows selected by native integrator policy. */
        public activeIntegratorBoundRows(rows = this.currentIntegratorRows(), expressionText = ''): WorksheetIntegratorRow[] {
            return this.activeIntegratorRowPlan(rows, expressionText).bounds;
        }

        /** Return all active rows selected by native integrator policy. */
        public activeIntegratorRows(rows = this.currentIntegratorRows(), expressionText = ''): WorksheetIntegratorRow[] {
            return this.activeIntegratorRowPlan(rows, expressionText).rows;
        }

        /** Return the unique variable names of the active integrator bounds. */
        public currentIntegratorBoundNames(): Set<string> {
            const names = new Set<string>();
            // Native policy has already selected these rows; this is only the browser lookup index.
            for (const row of this.activeIntegratorBoundRows()) names.add(row.name);
            return names;
        }

        /** Apply result-bound metadata through the native state synchroniser. */
        public applyIntegratorResultBound(data: WorksheetEvaluationData): void {
            this.host.sync(WorkspaceCommand.ApplyIntegratorResult, {data});
        }

        /** Restore bounds asynchronously, abandoning work when the supplied guard becomes false. */
        public async restoreIntegratorBoundsText(text: string, isCurrent: WorksheetGuard = workspaceIsCurrent): Promise<boolean> {
            return this.host.flow(WorkspaceFlow.RestoreBounds, {
                text, isCurrent, defaultBounds: this.host.defaults.DEFAULT_INTEGRATOR_BOUNDS_TEXT
            }) as Promise<boolean>;
        }

        /** Format the active integrator rows using the native expression formatter. */
        public currentIntegratorBoundsText(): string {
            return this.host.forms.text(this.activeIntegratorRows());
        }

        /** Restore catalogue bounds and report whether the guarded restoration completed. */
        public resetIntegratorBoundsToDefault(): Promise<boolean> {
            return this.restoreIntegratorBoundsText(this.host.defaults.DEFAULT_INTEGRATOR_BOUNDS_TEXT);
        }

        /** Render the native empty-form row structure. */
        public resetIntegratorBoundsToBlank(): void {
            this.host.bindingIntegratorPanel.renderIntegratorRows(this.integratorBlankRows());
        }

        /** Validate the requested integration interval limit against the catalogue default. */
        public requestedIntegratorIntervalCap(): number {
            const requested = this.host.locations.marsFormNumber(this.host.controls.integratorIntervalCap?.value);
            return this.host.wire.intervals(requested, Number(this.host.defaults.DEFAULT_INTEGRATOR_INTERVAL_CAP));
        }
    }

    /** Coordinates native mode transitions and editor capture without duplicating native mode state. */
    export class WorksheetModeSession {
        /** Retain capabilities without reading catalogue-dependent controls during construction. */
        public constructor(private readonly host: ModeDependencies) {
            marsBindCallbacks(this, [
                'workspaceModeId', 'currentMode', 'setMode', 'workspaceDefaultText',
                'captureCurrentModeEditor', 'restoreModeEditor', 'applyLabMode', 'evaluateCurrentMode',
                'evaluateFromKeyboard', 'clearWorksheetFromEvent', 'selectWorksheetMode',
                'normaliseWorksheetControl', 'saveCurrentModeEditorState'
            ]);
        }

        /** Resolve a worksheet mode through the native mode lookup. */
        public workspaceModeId(mode: unknown): number {
            const length = this.host.buffer.stageText(String(mode || '').trim());
            return this.host.wire.exports().lab_workspace_mode_id(length);
        }

        /** Return the mode selected by the native workspace controller. */
        public currentMode(): WorksheetMode {
            return this.host.modeNames[this.host.wire.exports().lab_workspace_mode()];
        }

        /** Ask the native state controller to select a mode and report acceptance. */
        public setMode(mode: WorksheetMode, options: WorksheetModeOptions = {}): boolean {
            return this.host.sync(WorkspaceCommand.SelectMode, {index: this.workspaceModeId(mode), options});
        }

        /** Return the catalogue expression for a native mode index. */
        public workspaceDefaultText(index: number): string {
            return [
                this.host.defaults.DEFAULT_EXPRESSION_TEXT,
                this.host.defaults.DEFAULT_EQUATION_TEXT,
                this.host.defaults.DEFAULT_DIFFEQUATION_TEXT,
                this.host.defaults.DEFAULT_MATRIX_TEXT,
                this.host.defaults.DEFAULT_INTEGRATOR_TEXT,
                this.host.defaults.DEFAULT_DATETIME_TEXT,
                this.host.defaults.DEFAULT_ALMANAC_TEXT
            ][index];
        }

        /** Capture the active editor asynchronously while the supplied guard remains current. */
        public async captureCurrentModeEditor(isCurrent: WorksheetGuard = workspaceIsCurrent): Promise<boolean> {
            return this.host.flow(WorkspaceFlow.CaptureEditor, {isCurrent}) as Promise<boolean>;
        }

        /** Project the selected mode's retained editor text into its controls. */
        public restoreModeEditor(mode: WorksheetMode): void {
            this.host.sync(WorkspaceCommand.RestoreEditor, {index: this.workspaceModeId(mode)});
        }

        /** Apply the selected mode through the native state transition. */
        public applyLabMode(mode: WorksheetMode): void {
            this.host.sync(WorkspaceCommand.ApplyMode, {mode});
        }

        /** Await evaluation of the active mode with the supplied history and input options. */
        public async evaluateCurrentMode(options: WorksheetEvaluationOptions = {}): Promise<void> {
            await this.host.evaluation.evaluateLabMode(this.currentMode(), options);
        }

        /** Dispatch the native keyboard evaluation transition. */
        public evaluateFromKeyboard(): void {
            this.host.sync(WorkspaceCommand.EvaluateKeyboard, {});
        }

        /** Await a guarded clear operation without disturbing a newer request. */
        public async clearWorksheetFromEvent(): Promise<void> {
            await this.host.flow(WorkspaceFlow.ClearWorksheet, {});
        }

        /** Restore a selected worksheet across asynchronous editor and result preparation. */
        public async selectWorksheetMode(mode: WorksheetMode): Promise<void> {
            const host = this.host;
            // Retain this selection until its admitted request runs.
            await host.requests.runUI('evaluate', this.currentMode(), function selectMode(request: WorksheetRequest) {
                return host.flow(WorkspaceFlow.SelectMode, {mode, request});
            });
        }

        /** Convert browser control values using their existing native validators. */
        public normaliseWorksheetControl(kind: number): void {
            this.host.sync(WorkspaceCommand.NormaliseControl, {
                index: kind,
                controls: [
                    this.host.controls.matrixOperation,
                    this.host.equationVariable,
                    this.host.controls.integratorIntervalCap
                ],
                config: this.host.configuration
            });
        }

        /** Persist the active editor through the existing storage capability. */
        public saveCurrentModeEditorState(): void {
            this.host.persistence.saveWorksheet(this.currentMode());
        }
    }

    /** Provides native precision capabilities and guarded precision transitions. */
    export class WorksheetPrecisionController {
        /** Retain capabilities without reading catalogue-dependent controls during construction. */
        public constructor(private readonly host: PrecisionDependencies) {
            marsBindCallbacks(this, [
                'precisionDigitsForBits', 'requestedPrecisionBits', 'requestedValuePrecision',
                'atMinimumPrecision', 'atMaximumPrecision', 'setRequestedPrecisionBits',
                'changeWorksheetPrecision', 'setWorksheetPrecision'
            ]);
        }

        /** Ask the native precision policy for the display digit count. */
        public precisionDigitsForBits(bits: number): number {
            return this.host.wire.exports().lab_workspace_digits(Number(bits));
        }

        /** Read the precision requested for the currently selected native mode. */
        public requestedPrecisionBits(): number {
            const native = this.host.wire.exports();
            return native.lab_workspace_requested_precision(native.lab_workspace_mode());
        }

        /** Return the display digit count for the currently requested precision. */
        public requestedValuePrecision(): number {
            return this.precisionDigitsForBits(this.requestedPrecisionBits());
        }

        /** Report whether native precision policy forbids a downward step. */
        public atMinimumPrecision(): boolean {
            const native = this.host.wire.exports();
            return !native.lab_workspace_precision_can_step(native.lab_workspace_mode(), -1);
        }

        /** Report whether native precision policy forbids an upward step. */
        public atMaximumPrecision(): boolean {
            const native = this.host.wire.exports();
            return !native.lab_workspace_precision_can_step(native.lab_workspace_mode(), 1);
        }

        /** Submit a precision request for the active native mode. */
        public setRequestedPrecisionBits(bits: number): void {
            const native = this.host.wire.exports();
            native.lab_workspace_request_precision(native.lab_workspace_mode(), Number(bits));
        }

        /** Await a native precision step and any resulting worksheet refresh. */
        public async changeWorksheetPrecision(direction: number): Promise<void> {
            await this.host.flow(WorkspaceFlow.SetPrecision, {direction});
        }

        /** Commit native-selected slider precision once and await the existing persistence/evaluation workflow. */
        public async setWorksheetPrecision(bits: number): Promise<void> {
            await this.host.flow(WorkspaceFlow.SetPrecision, {bits});
        }
    }

    /** Owns browser presentation state and per-mode result snapshots. */
    export class WorksheetPresentationController {
        /** Retain capabilities without reading catalogue-dependent controls during construction. */
        public constructor(private readonly host: PresentationDependencies) {
            marsBindCallbacks(this, [
                'setStatus', 'syncModeTabs', 'setValueCardVisible', 'saveCurrentModeResultState',
                'restoreModeResultState', 'syncMatrixControls', 'syncModeUI', 'syncHelpCards', 'showResults',
                'toggleHelp', 'hideTargetEntry', 'syncWorksheetButtons', 'setBusy', 'updateHistoryButtons',
                'renderDerivativeButtons', 'setActionRunning', 'dispose'
            ]);
            this.captureResultMetadata = this.captureResultMetadata.bind(this);
            this.onCalculusClick = this.onCalculusClick.bind(this);
        }

        /** @brief Presentation metadata retained by this browser session. */
        public readonly state = new WorksheetPresentationState();

        /** @brief Per-mode result snapshots; no saved result is required at startup. */
        public resultStates: WorkspaceResultStates = {};
        private readonly calculusButtons = new Set<HTMLButtonElement>();

        /** Remove only calculus subscriptions installed by this presentation instance. */
        public dispose(): void {
            for (const button of this.calculusButtons)
                button.removeEventListener('click', this.onCalculusClick);
            this.calculusButtons.clear();
        }

        /** Write the worksheet status through the native DOM adapter. */
        public setStatus(text: string): void {
            this.host.dom.call('lab_workspace_dom_status', String(text));
        }

        /** Project native mode selection onto the browser tab controls. */
        public syncModeTabs(): void {
            this.host.dom.call('lab_workspace_dom_tabs', this.host.controls.modeTabs, this.host.workspaceMode.currentMode());
        }

        /** Set numerical-value card visibility through native layout policy. */
        public setValueCardVisible(visible: boolean): void {
            this.host.dom.call('lab_workspace_dom_value', Number(!!visible));
        }

        /** Retain presentation metadata and plot state for the specified mode. */
        public saveCurrentModeResultState(mode = this.host.workspaceMode.currentMode()): void {
            this.resultStates[mode] = this.host.surfacePreview.captureResult(this.captureResultMetadata);
        }

        /** Capture native metadata while the surface owner has suspended its transient rendering. */
        private captureResultMetadata(): WorksheetResultSnapshot {
            return this.host.dom.call('lab_workspace_dom_result_save', {
                lastTex: this.state.lastTex,
                lastDerivativeExpression: this.state.lastDerivativeExpression,
                currentVariables: this.host.editorState.currentVariables,
                currentDifferentiable: this.host.editorState.currentDifferentiable
            }) as WorksheetResultSnapshot;
        }

        /** Restore the specified mode's retained result presentation. */
        public restoreModeResultState(mode = this.host.workspaceMode.currentMode()): void {
            this.host.sync(WorkspaceCommand.RestoreResult, {mode});
        }

        /** Project native matrix-operation policy onto its browser controls. */
        public syncMatrixControls(): void {
            this.host.locations.syncRoundedSelect(this.host.controls.matrixOperation);
            const mode = this.host.workspaceMode.workspaceModeId(this.host.workspaceMode.currentMode());
            this.host.dom.call('lab_workspace_dom_matrix', mode);
            this.host.workspaceLayout.scheduleEditorResizeGrip();
        }

        /** Refresh controls, precision labels and result visibility for the active mode. */
        public syncModeUI(): void {
            const mode = this.host.workspaceMode.workspaceModeId(this.host.workspaceMode.currentMode());
            this.host.dom.call('lab_workspace_dom_mode', mode, this.host.defaults.ALMANAC_COVERAGE_TEXT);
            this.host.functionCard.clearFunctionRun();
            this.syncMatrixControls();
            this.syncHelpCards();
            this.updateHistoryButtons();
            this.host.workspaceLayout.scheduleEditorResizeGrip();
        }

        /** Show the help cards selected by native mode policy. */
        public syncHelpCards(): void {
            this.host.dom.call('lab_workspace_dom_help_cards', this.host.controls.helpCards, this.host.workspaceMode.currentMode());
        }

        /** Switch the right-hand pane from help to native result presentation. */
        public showResults(): void {
            this.host.dom.call('lab_workspace_dom_help', 0);
        }

        /** Toggle the help pane using native workspace policy. */
        public toggleHelp(): void {
            this.host.dom.call('lab_workspace_dom_help', 2);
        }

        /** Hide the goal-seek target entry through the native DOM adapter. */
        public hideTargetEntry(): void {
            this.host.dom.call('lab_workspace_dom_target', 0);
        }

        /** Project busy state and native capability flags onto worksheet buttons. */
        public syncWorksheetButtons(isBusy = false): void {
            const mode = this.host.workspaceMode.workspaceModeId(this.host.workspaceMode.currentMode());
            this.host.dom.call(
                'lab_workspace_dom_controls', mode, Number(!!isBusy),
                Number(!!this.host.bindingEditor.expressionReadyToEvaluate()),
                this.host.history.backwardCount(), this.host.history.forwardCount(),
                Number(!!this.host.bindingGoal.canGoalSeek()),
                Number(this.host.workspacePrecision.atMinimumPrecision()),
                Number(this.host.workspacePrecision.atMaximumPrecision()));
        }

        /** Update native busy-state presentation for the worksheet. */
        public setBusy(isBusy: boolean): void {
            this.syncWorksheetButtons(isBusy);
            this.host.dom.call('lab_workspace_dom_busy', Number(!!isBusy), this.host.equationVariable,
                               this.host.controls.copyButtons, this.host.controls.moreDigitButtons);
        }

        /** Refresh navigation buttons from native history lengths. */
        public updateHistoryButtons(): void {
            this.syncWorksheetButtons();
        }

        /** Render native calculus actions and attach the corresponding request callbacks. */
        public renderDerivativeButtons(variables: string[]): void {
            this.dispose();
            // Native code formats all labels and renders the controls in one scoped call.
            this.host.dom.call('lab_workspace_dom_calculus', variables, Number(!!this.host.editorState.currentDifferentiable));
            for (const button of this.host.controls.derivativeButtons.querySelectorAll('button')) {
                this.calculusButtons.add(button);
                button.addEventListener('click', this.onCalculusClick);
            }
        }

        /** Route a native-created button to the current request owner, even after capability replacement. */
        private onCalculusClick(event: Event): void {
            // The listener is installed only on buttons; currentTarget also handles clicks on their child labels.
            const button = event.currentTarget as HTMLButtonElement;
            const calculus = this.host.calculus;
            const actions = [calculus.takeDerivative, calculus.takeIntegral, calculus.takePlot];
            actions[Number(button.dataset.calculusAction)].call(calculus, button.dataset.variable || '', button);
        }

        /** Project a calculus action's running state onto its button. */
        public setActionRunning(button: WorksheetCopyButton, running: boolean): void {
            this.host.dom.call('lab_workspace_dom_running', button, Number(!!running));
        }
    }

    /** Owns the pending animation frame for editor geometry updates. */
    export class WorksheetEditorLayout {
        /** Retain capabilities without reading catalogue-dependent controls during construction. */
        public constructor(private readonly host: LayoutDependencies) {
            marsBindCallbacks(this, [
                'resetEditorManualSize', 'syncEditorResizeGrip', 'scheduleEditorResizeGrip', 'dispose'
            ]);
        }

        /** @brief Pending geometry frame, or zero when no update is queued. */
        public editorResizeFrame = 0;

        /** Release a queued geometry update when the session is disposed. */
        public dispose(): void {
            if (this.editorResizeFrame) this.host.cancelFrame(this.editorResizeFrame);
            this.editorResizeFrame = 0;
        }

        /** Release manual editor sizing through the native layout adapter. */
        public resetEditorManualSize(): void {
            this.host.dom.call('lab_workspace_dom_editor_reset', this.host.controls.labTextareas);
        }

        /** Update the editor resize affordance from current browser geometry. */
        public syncEditorResizeGrip(): void {
            this.editorResizeFrame = 0;
            this.host.dom.call('lab_workspace_dom_editor_resize', this.host.controls.labTextareas, this.host.viewportHeight);
        }

        /** Coalesce editor resize updates into the next animation frame. */
        public scheduleEditorResizeGrip(): void {
            if (this.editorResizeFrame)
                this.host.cancelFrame(this.editorResizeFrame);
            this.editorResizeFrame = this.host.requestFrame(this.syncEditorResizeGrip.bind(this));
        }
    }

    /** Provides native history and persistence commands without a duplicate history store. */
    export class WorksheetHistoryCommands {
        /** Retain capabilities without reading catalogue-dependent controls during construction. */
        public constructor(private readonly host: HistoryDependencies) {
            marsBindCallbacks(this, [
                'pushExpressionHistory', 'flushWorksheetState'
            ]);
        }

        /** Pass an expression history entry to the native state controller. */
        public pushExpressionHistory(entry: unknown): void {
            this.host.sync(WorkspaceCommand.PushHistory, {entry});
        }

        /** Flush pending worksheet state through the native synchroniser. */
        public flushWorksheetState(): void {
            this.host.sync(WorkspaceCommand.FlushState, {});
        }
    }

    /** Selects native-approved variable names and visible bindings for the active mode. */
    export class WorksheetBindingSelection {
        /** Retain capabilities without reading catalogue-dependent controls during construction. */
        public constructor(private readonly host: BindingSelectionDependencies) {
            marsBindCallbacks(this, [
                'variableNamesFromBindings', 'visibleBindingsForCurrentMode'
            ]);
        }

        /** Extract native-approved variable names from structured binding metadata. */
        public variableNamesFromBindings(bindings: WorksheetBinding[]): string[] {
            return this.host.dom.call('lab_binding_variables', bindings) as string[];
        }

        /** Select the bindings visible in the active mode using native policy. */
        public visibleBindingsForCurrentMode(bindings: WorksheetBinding[]): WorksheetBinding[] {
            const mode = this.host.workspaceMode.workspaceModeId(this.host.workspaceMode.currentMode());
            const bounds = mode === 4 ? this.host.workspaceIntegrator.currentIntegratorBoundNames() : null;
            return this.host.dom.call('lab_binding_select', mode, bindings, bounds) as WorksheetBinding[];
        }
    }

}
