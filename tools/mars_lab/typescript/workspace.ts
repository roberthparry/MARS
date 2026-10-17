/**
 * @file workspace.ts
 * @brief Worksheet coordination and browser-session composition.
 *
 * Exposes workspace operations through WorksheetWorkspace and initialises typed
 * presentation, editor and calendar state after the native catalogue is loaded.
 * Other domain adapters call its API; native code owns mathematical state and policy.
 */
/**
 * Workspace state adapters and asynchronous form/request coordination.
 * C owns workspace DOM projection, control policy and presentation snapshots.
 * Loaded last, after the definition scripts, to initialise shared browser references
 * and views of native state before subscriptions and the first evaluation.
 */

namespace WorksheetWorkspace {
    /** Stable native mode indices shared by typed state views. */
    export const WORKSPACE_MODE_NAMES = Object.freeze(['expression', 'equation', 'diffequation', 'matrix', 'integrator', 'datetime', 'almanac'] as const);
    /** Session-owned native transfer buffer. */
    export const labWorkspaceBuffer: WorksheetNativeBuffer = new WorksheetNativeBuffer(labWire, WORKSPACE_MODE_NAMES);

    /** Live native views share the session buffer and immutable mode catalogue. */
    export const nativeViews: WorksheetNativeViews = new WorksheetNativeViews(labWire, labWorkspaceBuffer, WORKSPACE_MODE_NAMES);

    /** Owns integrator form metadata and coordinates native row workflows. */
    export class WorksheetIntegratorController {
        /** Retain capabilities without reading catalogue-dependent controls during construction. */
        public constructor(private readonly host: {
            readonly dom: Pick<typeof labDOM, 'call' | 'services'>;
            readonly wire: Pick<typeof labWire, 'exports' | 'intervals'>;
            readonly flow: WorksheetRequests.NativeFlowRuntime['continue'];
            readonly sync: WorksheetStateRuntimePort['sync'];
            readonly controls: Pick<WorksheetReferences, 'expr' | 'integratorBoundStack' | 'integratorIntervalCap'>;
            readonly defaults: Pick<typeof WorksheetContext, 'DEFAULT_INTEGRATOR_BOUNDS_TEXT' | 'DEFAULT_INTEGRATOR_INTERVAL_CAP'>;
            readonly locations: Pick<typeof WorksheetLocations.dates, 'marsFormNumber'>;
            readonly forms: WorksheetIntegratorForms;
            readonly bindingEditor: Pick<WorksheetBindings.WorksheetEditorAdapter, 'currentExpressionText'>;
            readonly bindingIntegratorPanel: Pick<WorksheetBindings.WorksheetIntegratorPanel, 'renderIntegratorRows'>;
        }) {
            // Preserve the detachable callback contract of the former namespace functions.
            this.integratorFormsExpression = this.integratorFormsExpression.bind(this);
            this.refreshIntegratorForms = this.refreshIntegratorForms.bind(this);
            this.integratorBlankRows = this.integratorBlankRows.bind(this);
            this.currentIntegratorRows = this.currentIntegratorRows.bind(this);
            this.activeIntegratorRowPlan = this.activeIntegratorRowPlan.bind(this);
            this.activeIntegratorBoundRows = this.activeIntegratorBoundRows.bind(this);
            this.activeIntegratorRows = this.activeIntegratorRows.bind(this);
            this.currentIntegratorBoundNames = this.currentIntegratorBoundNames.bind(this);
            this.applyIntegratorResultBound = this.applyIntegratorResultBound.bind(this);
            this.restoreIntegratorBoundsText = this.restoreIntegratorBoundsText.bind(this);
            this.currentIntegratorBoundsText = this.currentIntegratorBoundsText.bind(this);
            this.resetIntegratorBoundsToDefault = this.resetIntegratorBoundsToDefault.bind(this);
            this.resetIntegratorBoundsToBlank = this.resetIntegratorBoundsToBlank.bind(this);
            this.requestedIntegratorIntervalCap = this.requestedIntegratorIntervalCap.bind(this);
        }

        /** Native row metadata belongs exclusively to this integrator instance. */
        public get forms(): WorksheetIntegratorForms { return this.host.forms; }

        /** Read the current authored expression, falling back to the visible editor text. */
        public integratorFormsExpression(): string {
            return this.host.bindingEditor.currentExpressionText() || this.host.controls.expr.value || '';
        }

        /** Await native form refresh and return the rows accepted for the current expression. */
        public async refreshIntegratorForms(
            rows = this.currentIntegratorRows(), expression = this.integratorFormsExpression()): Promise<WorksheetIntegratorRow[] | null | undefined> {
            return this.host.flow<WorksheetIntegratorRow[] | null | undefined>(73, {rows, expression});
        }

        /** Obtain the native default row structure for an empty integrator form. */
        public integratorBlankRows(): WorksheetIntegratorRow[] {
            return this.host.dom.call<WorksheetIntegratorRow[]>('lab_binding_rows_default', 1);
        }

        /** Read structured integrator controls without interpreting their mathematical text. */
        public currentIntegratorRows(): WorksheetIntegratorRow[] {
            return this.host.dom.call<WorksheetIntegratorRow[]>('lab_binding_rows_read', this.host.controls.integratorBoundStack);
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
            return new Set(this.activeIntegratorBoundRows().map((row) => row.name));
        }

        /** Apply result-bound metadata through the native state synchroniser. */
        public applyIntegratorResultBound(data: WorksheetEvaluationData): void {
            this.host.sync(14, {data});
        }

        /** Restore bounds asynchronously, abandoning work when the supplied guard becomes false. */
        public async restoreIntegratorBoundsText(text: string, isCurrent: WorksheetGuard = () => true): Promise<boolean> {
            return this.host.flow<boolean>(75, {text, isCurrent, defaultBounds: this.host.defaults.DEFAULT_INTEGRATOR_BOUNDS_TEXT});
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
            return this.host.wire.intervals(this.host.locations.marsFormNumber(this.host.controls.integratorIntervalCap?.value), Number(this.host.defaults.DEFAULT_INTEGRATOR_INTERVAL_CAP));
        }
    }

    /** Coordinates native mode transitions and editor capture without duplicating native mode state. */
    export class WorksheetModeSession {
        /** Retain capabilities without reading catalogue-dependent controls during construction. */
        public constructor(private readonly host: {
            readonly wire: Pick<typeof labWire, 'exports' | 'intervals'>;
            readonly flow: WorksheetRequests.NativeFlowRuntime['continue'];
            readonly requests: Pick<typeof WorksheetRequests.labRequests, 'runUI'>;
            readonly sync: WorksheetStateRuntimePort['sync'];
            readonly equationVariable: null;
            readonly configuration: LabConfiguration;
            readonly modeNames: readonly WorksheetMode[];
            readonly controls: Pick<WorksheetReferences, 'matrixOperation' | 'integratorIntervalCap'>;
            readonly defaults: Pick<typeof WorksheetContext, 'DEFAULT_EXPRESSION_TEXT' | 'DEFAULT_EQUATION_TEXT' | 'DEFAULT_DIFFEQUATION_TEXT' | 'DEFAULT_MATRIX_TEXT' | 'DEFAULT_INTEGRATOR_TEXT' | 'DEFAULT_DATETIME_TEXT' | 'DEFAULT_ALMANAC_TEXT'>;
            readonly evaluation: Pick<WorksheetRequests.EvaluationController, 'evaluateLabMode'>;
            readonly persistence: Pick<WorksheetPersistence.WorksheetPersistenceController, 'saveWorksheet'>;
            readonly buffer: Pick<WorksheetNativeBuffer, 'stageText'>;
        }) {
            // Preserve the detachable callback contract of the former namespace functions.
            this.workspaceModeId = this.workspaceModeId.bind(this);
            this.currentMode = this.currentMode.bind(this);
            this.setMode = this.setMode.bind(this);
            this.workspaceDefaultText = this.workspaceDefaultText.bind(this);
            this.captureCurrentModeEditor = this.captureCurrentModeEditor.bind(this);
            this.restoreModeEditor = this.restoreModeEditor.bind(this);
            this.applyLabMode = this.applyLabMode.bind(this);
            this.evaluateCurrentMode = this.evaluateCurrentMode.bind(this);
            this.evaluateFromKeyboard = this.evaluateFromKeyboard.bind(this);
            this.clearWorksheetFromEvent = this.clearWorksheetFromEvent.bind(this);
            this.selectWorksheetMode = this.selectWorksheetMode.bind(this);
            this.normaliseWorksheetControl = this.normaliseWorksheetControl.bind(this);
            this.saveCurrentModeEditorState = this.saveCurrentModeEditorState.bind(this);
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
            return this.host.sync(6, {index: this.workspaceModeId(mode), options});
        }

        /** Return the catalogue expression for a native mode index. */
        public workspaceDefaultText(index: number): string {
            return [
                this.host.defaults.DEFAULT_EXPRESSION_TEXT, this.host.defaults.DEFAULT_EQUATION_TEXT, this.host.defaults.DEFAULT_DIFFEQUATION_TEXT, this.host.defaults.DEFAULT_MATRIX_TEXT,
                this.host.defaults.DEFAULT_INTEGRATOR_TEXT, this.host.defaults.DEFAULT_DATETIME_TEXT, this.host.defaults.DEFAULT_ALMANAC_TEXT
            ][index];
        }

        /** Capture the active editor asynchronously while the supplied guard remains current. */
        public async captureCurrentModeEditor(isCurrent: WorksheetGuard = () => true): Promise<boolean> {
            return this.host.flow<boolean>(23, {isCurrent});
        }

        /** Project the selected mode's retained editor text into its controls. */
        public restoreModeEditor(mode: WorksheetMode): void {
            this.host.sync(4, {index: this.workspaceModeId(mode)});
        }

        /** Apply the selected mode through the native state transition. */
        public applyLabMode(mode: WorksheetMode): void {
            this.host.sync(7, {mode});
        }

        /** Await evaluation of the active mode with the supplied history and input options. */
        public async evaluateCurrentMode(options: Parameters<WorksheetRequests.EvaluationController['evaluateLabMode']>[1] = {}): Promise<void> {
            await this.host.evaluation.evaluateLabMode(this.currentMode(), options);
        }

        /** Dispatch the native keyboard evaluation transition. */
        public evaluateFromKeyboard(): void {
            this.host.sync(8, {});
        }

        /** Await a guarded clear operation without disturbing a newer request. */
        public async clearWorksheetFromEvent(): Promise<void> {
            await this.host.flow(72, {});
        }

        /** Restore a selected worksheet across asynchronous editor and result preparation. */
        public async selectWorksheetMode(mode: WorksheetMode): Promise<void> {
            await this.host.requests.runUI('evaluate', this.currentMode(), (request: WorksheetRequest) => this.host.flow<void>(24, {mode, request}));
        }

        /** Convert browser control values using their existing native validators. */
        public normaliseWorksheetControl(kind: number): void {
            this.host.sync(
                11, {index: kind, controls: [this.host.controls.matrixOperation, this.host.equationVariable, this.host.controls.integratorIntervalCap], config: this.host.configuration});
        }

        /** Persist the active editor through the existing storage capability. */
        public saveCurrentModeEditorState(): void {
            this.host.persistence.saveWorksheet(this.currentMode());
        }
    }

    /** Provides native precision capabilities and guarded precision transitions. */
    export class WorksheetPrecisionController {
        /** Retain capabilities without reading catalogue-dependent controls during construction. */
        public constructor(private readonly host: {
            readonly wire: Pick<typeof labWire, 'exports' | 'intervals'>;
            readonly flow: WorksheetRequests.NativeFlowRuntime['continue'];
        }) {
            // Preserve the detachable callback contract of the former namespace functions.
            this.precisionDigitsForBits = this.precisionDigitsForBits.bind(this);
            this.requestedPrecisionBits = this.requestedPrecisionBits.bind(this);
            this.requestedValuePrecision = this.requestedValuePrecision.bind(this);
            this.atMinimumPrecision = this.atMinimumPrecision.bind(this);
            this.atMaximumPrecision = this.atMaximumPrecision.bind(this);
            this.setRequestedPrecisionBits = this.setRequestedPrecisionBits.bind(this);
            this.changeWorksheetPrecision = this.changeWorksheetPrecision.bind(this);
            this.setWorksheetPrecision = this.setWorksheetPrecision.bind(this);
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
            await this.host.flow<void>(25, {direction});
        }

        /** Commit native-selected slider precision once and await the existing persistence/evaluation workflow. */
        public async setWorksheetPrecision(bits: number): Promise<void> {
            await this.host.flow<void>(25, {bits});
        }
    }

    /** Owns browser presentation state and per-mode result snapshots. */
    export class WorksheetPresentationController {
        /** Retain capabilities without reading catalogue-dependent controls during construction. */
        public constructor(private readonly host: {
            readonly dom: Pick<typeof labDOM, 'call' | 'services'>;
            readonly sync: WorksheetStateRuntimePort['sync'];
            readonly editorState: Pick<WorksheetBindings.WorksheetBindingPanel, 'currentVariables' | 'currentDifferentiable'>;
            readonly equationVariable: null;
            readonly surfacePreview: Pick<typeof labSurfacePreview, 'captureResult'>;
            readonly controls: Pick<WorksheetReferences, 'modeTabs' | 'matrixOperation' | 'helpCards' | 'copyButtons' | 'moreDigitButtons' | 'derivativeButtons'>;
            readonly defaults: Pick<typeof WorksheetContext, 'ALMANAC_COVERAGE_TEXT'>;
            readonly functionCard: Pick<WorksheetRequests.FunctionCardController, 'clearFunctionRun'>;
            readonly calculus: Pick<WorksheetRequests.PlotRequestController, 'takeDerivative' | 'takeIntegral' | 'takePlot'>;
            readonly history: Pick<WorksheetPersistence.NativeHistoryStore, 'backwardCount' | 'forwardCount'>;
            readonly locations: { syncRoundedSelect(select: WorksheetSelect): void };
            readonly workspaceLayout: Pick<WorksheetWorkspace.WorksheetEditorLayout, 'scheduleEditorResizeGrip'>;
            readonly bindingEditor: Pick<WorksheetBindings.WorksheetEditorAdapter, 'expressionReadyToEvaluate' | 'bindingDisplayName'>;
            readonly workspacePrecision: Pick<WorksheetWorkspace.WorksheetPrecisionController, 'atMinimumPrecision' | 'atMaximumPrecision'>;
            readonly workspaceMode: Pick<WorksheetWorkspace.WorksheetModeSession, 'workspaceModeId' | 'currentMode'>;
            readonly bindingGoal: Pick<WorksheetBindings.WorksheetGoalController, 'canGoalSeek'>;
        }) {
            // Preserve the detachable callback contract of the former namespace functions.
            this.setStatus = this.setStatus.bind(this);
            this.syncModeTabs = this.syncModeTabs.bind(this);
            this.setValueCardVisible = this.setValueCardVisible.bind(this);
            this.saveCurrentModeResultState = this.saveCurrentModeResultState.bind(this);
            this.restoreModeResultState = this.restoreModeResultState.bind(this);
            this.syncMatrixControls = this.syncMatrixControls.bind(this);
            this.syncModeUI = this.syncModeUI.bind(this);
            this.syncHelpCards = this.syncHelpCards.bind(this);
            this.showResults = this.showResults.bind(this);
            this.toggleHelp = this.toggleHelp.bind(this);
            this.hideTargetEntry = this.hideTargetEntry.bind(this);
            this.syncWorksheetButtons = this.syncWorksheetButtons.bind(this);
            this.setBusy = this.setBusy.bind(this);
            this.updateHistoryButtons = this.updateHistoryButtons.bind(this);
            this.renderDerivativeButtons = this.renderDerivativeButtons.bind(this);
            this.setActionRunning = this.setActionRunning.bind(this);
            this.dispose = this.dispose.bind(this);
        }

        public readonly state = new WorksheetPresentationState();
        public resultStates: Partial<Record<WorksheetMode, WorksheetResultSnapshot>> = {};
        private readonly calculusListeners = new Map<HTMLButtonElement, EventListener>();

        /** Remove only calculus subscriptions installed by this presentation instance. */
        public dispose(): void {
            for (const [button, listener] of this.calculusListeners)
                button.removeEventListener('click', listener);
            this.calculusListeners.clear();
        }

        /** Write the worksheet status through the native DOM adapter. */
        public setStatus(text: string): void {
            this.host.dom.call<unknown>('lab_workspace_dom_status', String(text));
        }

        /** Project native mode selection onto the browser tab controls. */
        public syncModeTabs(): void {
            this.host.dom.call<unknown>('lab_workspace_dom_tabs', this.host.controls.modeTabs, this.host.workspaceMode.currentMode());
        }

        /** Set numerical-value card visibility through native layout policy. */
        public setValueCardVisible(visible: boolean): void {
            this.host.dom.call<unknown>('lab_workspace_dom_value', Number(!!visible));
        }

        /** Retain presentation metadata and plot state for the specified mode. */
        public saveCurrentModeResultState(mode = this.host.workspaceMode.currentMode()): void {
            this.resultStates[mode] = this.host.surfacePreview.captureResult(() => this.host.dom.call<WorksheetResultSnapshot>(
                'lab_workspace_dom_result_save', {lastTex: this.state.lastTex, lastDerivativeExpression: this.state.lastDerivativeExpression, currentVariables: this.host.editorState.currentVariables, currentDifferentiable: this.host.editorState.currentDifferentiable}));
        }

        /** Restore the specified mode's retained result presentation. */
        public restoreModeResultState(mode = this.host.workspaceMode.currentMode()): void {
            this.host.sync(5, {mode});
        }

        /** Project native matrix-operation policy onto its browser controls. */
        public syncMatrixControls(): void {
            this.host.locations.syncRoundedSelect(this.host.controls.matrixOperation);
            this.host.dom.call<unknown>('lab_workspace_dom_matrix', this.host.workspaceMode.workspaceModeId(this.host.workspaceMode.currentMode()));
            this.host.workspaceLayout.scheduleEditorResizeGrip();
        }

        /** Refresh controls, precision labels and result visibility for the active mode. */
        public syncModeUI(): void {
            this.host.dom.call<unknown>('lab_workspace_dom_mode', this.host.workspaceMode.workspaceModeId(this.host.workspaceMode.currentMode()), this.host.defaults.ALMANAC_COVERAGE_TEXT);
            this.host.functionCard.clearFunctionRun();
            this.syncMatrixControls();
            this.syncHelpCards();
            this.updateHistoryButtons();
            this.host.workspaceLayout.scheduleEditorResizeGrip();
        }

        /** Show the help cards selected by native mode policy. */
        public syncHelpCards(): void {
            this.host.dom.call<unknown>('lab_workspace_dom_help_cards', this.host.controls.helpCards, this.host.workspaceMode.currentMode());
        }

        /** Switch the right-hand pane from help to native result presentation. */
        public showResults(): void {
            this.host.dom.call<unknown>('lab_workspace_dom_help', 0);
        }

        /** Toggle the help pane using native workspace policy. */
        public toggleHelp(): void {
            this.host.dom.call<unknown>('lab_workspace_dom_help', 2);
        }

        /** Hide the goal-seek target entry through the native DOM adapter. */
        public hideTargetEntry(): void {
            this.host.dom.call<unknown>('lab_workspace_dom_target', 0);
        }

        /** Project busy state and native capability flags onto worksheet buttons. */
        public syncWorksheetButtons(isBusy = false): void {
            this.host.dom.call<unknown>(
                'lab_workspace_dom_controls', this.host.workspaceMode.workspaceModeId(this.host.workspaceMode.currentMode()), Number(!!isBusy),
                Number(!!this.host.bindingEditor.expressionReadyToEvaluate()), this.host.history.backwardCount(), this.host.history.forwardCount(),
                Number(!!this.host.bindingGoal.canGoalSeek()), Number(this.host.workspacePrecision.atMinimumPrecision()), Number(this.host.workspacePrecision.atMaximumPrecision()));
        }

        /** Update native busy-state presentation for the worksheet. */
        public setBusy(isBusy: boolean): void {
            this.syncWorksheetButtons(isBusy);
            this.host.dom.call<unknown>('lab_workspace_dom_busy', Number(!!isBusy), this.host.equationVariable, this.host.controls.copyButtons, this.host.controls.moreDigitButtons);
        }

        /** Refresh navigation buttons from native history lengths. */
        public updateHistoryButtons(): void {
            this.syncWorksheetButtons();
        }

        /** Render native calculus actions and attach the corresponding request callbacks. */
        public renderDerivativeButtons(variables: string[]): void {
            this.dispose();
            this.host.dom.call<unknown>(
                'lab_workspace_dom_derivatives', variables, variables.map(this.host.bindingEditor.bindingDisplayName.bind(this.host.bindingEditor)), Number(!!this.host.editorState.currentDifferentiable));
            for (const button of this.host.controls.derivativeButtons.querySelectorAll('button')) {
                const listener: EventListener = () => {
                    const action = [this.host.calculus.takeDerivative, this.host.calculus.takeIntegral, this.host.calculus.takePlot][Number(button.dataset.calculusAction)];
                    action.call(this.host.calculus, button.dataset.variable || '', button);
                };
                this.calculusListeners.set(button, listener);
                button.addEventListener('click', listener);
            }
        }

        /** Project a calculus action's running state onto its button. */
        public setActionRunning(button: WorksheetCopyButton, running: boolean): void {
            this.host.dom.call<unknown>('lab_workspace_dom_running', button, Number(!!running));
        }
    }

    /** Owns the pending animation frame for editor geometry updates. */
    export class WorksheetEditorLayout {
        /** Retain capabilities without reading catalogue-dependent controls during construction. */
        public constructor(private readonly host: {
            readonly dom: Pick<typeof labDOM, 'call' | 'services'>;
            readonly controls: Pick<WorksheetReferences, 'labTextareas'>;
            readonly cancelFrame: (handle: number) => void;
            readonly requestFrame: (callback: FrameRequestCallback) => number;
            readonly viewportHeight: number;
        }) {
            // Preserve the detachable callback contract of the former namespace functions.
            this.resetEditorManualSize = this.resetEditorManualSize.bind(this);
            this.syncEditorResizeGrip = this.syncEditorResizeGrip.bind(this);
            this.scheduleEditorResizeGrip = this.scheduleEditorResizeGrip.bind(this);
            this.dispose = this.dispose.bind(this);
        }

        public editorResizeFrame = 0;

        /** Release a queued geometry update when the session is disposed. */
        public dispose(): void {
            if (this.editorResizeFrame) this.host.cancelFrame(this.editorResizeFrame);
            this.editorResizeFrame = 0;
        }

        /** Release manual editor sizing through the native layout adapter. */
        public resetEditorManualSize(): void {
            this.host.dom.call<unknown>('lab_workspace_dom_editor_reset', this.host.controls.labTextareas);
        }

        /** Update the editor resize affordance from current browser geometry. */
        public syncEditorResizeGrip(): void {
            this.editorResizeFrame = 0;
            this.host.dom.call<unknown>('lab_workspace_dom_editor_resize', this.host.controls.labTextareas, this.host.viewportHeight);
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
        public constructor(private readonly host: {
            readonly sync: WorksheetStateRuntimePort['sync'];
        }) {
            // Preserve the detachable callback contract of the former namespace functions.
            this.pushExpressionHistory = this.pushExpressionHistory.bind(this);
            this.flushWorksheetState = this.flushWorksheetState.bind(this);
        }

        /** Pass an expression history entry to the native state controller. */
        public pushExpressionHistory(entry: unknown): void {
            this.host.sync(10, {entry});
        }

        /** Flush pending worksheet state through the native synchroniser. */
        public flushWorksheetState(): void {
            this.host.sync(12, {});
        }
    }

    /** Selects native-approved variable names and visible bindings for the active mode. */
    export class WorksheetBindingSelection {
        /** Retain capabilities without reading catalogue-dependent controls during construction. */
        public constructor(private readonly host: {
            readonly dom: Pick<typeof labDOM, 'call' | 'services'>;
            readonly workspaceIntegrator: Pick<WorksheetWorkspace.WorksheetIntegratorController, 'currentIntegratorBoundNames'>;
            readonly workspaceMode: Pick<WorksheetWorkspace.WorksheetModeSession, 'workspaceModeId' | 'currentMode'>;
        }) {
            // Preserve the detachable callback contract of the former namespace functions.
            this.variableNamesFromBindings = this.variableNamesFromBindings.bind(this);
            this.visibleBindingsForCurrentMode = this.visibleBindingsForCurrentMode.bind(this);
        }

        /** Extract native-approved variable names from structured binding metadata. */
        public variableNamesFromBindings(bindings: WorksheetBinding[]): string[] {
            return this.host.dom.call<string[]>('lab_binding_variables', bindings);
        }

        /** Select the bindings visible in the active mode using native policy. */
        public visibleBindingsForCurrentMode(bindings: WorksheetBinding[]): WorksheetBinding[] {
            const mode = this.host.workspaceMode.workspaceModeId(this.host.workspaceMode.currentMode());
            return this.host.dom.call<WorksheetBinding[]>('lab_binding_select', mode, bindings, mode === 4 ? this.host.workspaceIntegrator.currentIntegratorBoundNames() : null);
        }
    }

    /** Session-owned integrator form metadata and coordinates native row workflows. */
    export const integrator: WorksheetIntegratorController = new WorksheetIntegratorController({
        get dom() { return labDOM; },
        get wire() { return labWire; },
        get flow() { return WorksheetRequests.runtime.continue.bind(WorksheetRequests.runtime); },
        get sync() { return WorksheetPersistence.stateRuntime.sync.bind(WorksheetPersistence.stateRuntime); },
        get controls() { return WorksheetContext; },
        get defaults() { return WorksheetContext; },
        get locations() { return WorksheetLocations.dates; },
        forms: new WorksheetIntegratorForms(),
        get bindingEditor() { return WorksheetBindings.editor; },
        get bindingIntegratorPanel() { return WorksheetBindings.integratorPanel; }
    });

    /** Session-owned coordinator for native mode transitions and editor capture without duplicating native mode state. */
    export const mode: WorksheetModeSession = new WorksheetModeSession({
        get wire() { return labWire; },
        get flow() { return WorksheetRequests.runtime.continue.bind(WorksheetRequests.runtime); },
        get requests() { return WorksheetRequests.labRequests; },
        get sync() { return WorksheetPersistence.stateRuntime.sync.bind(WorksheetPersistence.stateRuntime); },
        get equationVariable() { return WorksheetContext.equationVariable; },
        get configuration() { return labApplication.configuration; },
        modeNames: WORKSPACE_MODE_NAMES,
        get controls() { return WorksheetContext; },
        get defaults() { return WorksheetContext; },
        get evaluation() { return WorksheetRequests.evaluation; },
        get persistence() { return WorksheetPersistence.persistence; },
        get buffer() { return WorksheetWorkspace.labWorkspaceBuffer; }
    });

    /** Session-owned provider of native precision capabilities and guarded precision transitions. */
    export const precision: WorksheetPrecisionController = new WorksheetPrecisionController({
        get wire() { return labWire; },
        get flow() { return WorksheetRequests.runtime.continue.bind(WorksheetRequests.runtime); }
    });

    /** Session-owned browser presentation state and per-mode result snapshots. */
    export const presentation: WorksheetPresentationController = new WorksheetPresentationController({
        get dom() { return labDOM; },
        get sync() { return WorksheetPersistence.stateRuntime.sync.bind(WorksheetPersistence.stateRuntime); },
        get editorState() { return WorksheetBindings.panel; },
        get equationVariable() { return WorksheetContext.equationVariable; },
        get surfacePreview() { return labSurfacePreview; },
        get controls() { return WorksheetContext; },
        get defaults() { return WorksheetContext; },
        get functionCard() { return WorksheetRequests.functionCard; },
        get calculus() { return WorksheetRequests.plots; },
        get history() { return WorksheetPersistence.history; },
        locations: { syncRoundedSelect: select => WorksheetLocations.selects.sync(select) },
        get workspaceLayout() { return WorksheetWorkspace.layout; },
        get bindingEditor() { return WorksheetBindings.editor; },
        get workspacePrecision() { return WorksheetWorkspace.precision; },
        get workspaceMode() { return WorksheetWorkspace.mode; },
        get bindingGoal() { return WorksheetBindings.goal; }
    });

    /** Session-owned animation-frame scheduling for editor geometry updates. */
    export const layout: WorksheetEditorLayout = new WorksheetEditorLayout({
        get dom() { return labDOM; },
        get controls() { return WorksheetContext; },
        cancelFrame: handle => cancelAnimationFrame(handle),
        requestFrame: callback => requestAnimationFrame(callback),
        get viewportHeight() { return window.innerHeight; }
    });

    /** Session-owned provider of native history and persistence commands without a duplicate history store. */
    export const history: WorksheetHistoryCommands = new WorksheetHistoryCommands({
        get sync() { return WorksheetPersistence.stateRuntime.sync.bind(WorksheetPersistence.stateRuntime); }
    });

    /** Session-owned selection of native-approved variable names and visible bindings for the active mode. */
    export const bindingSelection: WorksheetBindingSelection = new WorksheetBindingSelection({
        get dom() { return labDOM; },
        get workspaceIntegrator() { return WorksheetWorkspace.integrator; },
        get workspaceMode() { return WorksheetWorkspace.mode; }
    });

}

/** Browser references and state are initialised only after the native catalogue. */
namespace WorksheetContext {
    // Browser references and views of C-owned worksheet state, initialised after the definition scripts.
    export const {
        expr,
        subtitle,
        leftPaneTitle,
        matrixControls,
        matrixOperation,
        matrixOperand,
        matrixOperandLabel,
        equationControls,
        diffequationControls,
        integratorControls,
        integratorBoundStack,
        integratorIntervalCap,
        datetimeControls,
        datetimeDate,
        datetimeJdn,
        datetimeStart,
        datetimeYear,
        datetimeJurisdiction,
        datetimeTown,
        datetimeLatitude,
        datetimeLongitude,
        datetimeGmtOffset,
        datetimeLocal,
        datetimeLocalBody,
        almanacControls,
        almanacDate,
        almanacTime,
        almanacZone,
        almanacJurisdiction,
        almanacTown,
        almanacLatitude,
        almanacLongitude,
        almanacElevation,
        marsDatePicker,
        marsDatePickerMonth,
        marsDatePickerYear,
        marsDatePickerWeekdays,
        marsDatePickerGrid,
        marsDatePickerToday,
        marsDatePickerClose,
        run,
        back,
        forward,
        help,
        goalSeek,
        clear,
        targetRow,
        goalTarget,
        precisionSlider,
        precisionValue,
        derivativeButtons,
        variableValues,
        mobileAccess,
        mobileTitle,
        mobileHint,
        mobileUrl,
        mobileQr,
        statusEl,
        inputCopy,
        labWorkspace,
        rightPaneTitle,
        resultUseInput,
        resultPane,
        helpPane,
        rendered,
        renderedTitle,
        renderedMore,
        parsed,
        parsedMore,
        functionStyle,
        functionTitle,
        functionMore,
        functionRun,
        functionRunResult,
        functionRunOutput,
        valueCard,
        valueNoteCard,
        valueNote,
        value,
        valueTitle,
        valueMore,
        labTextareas,
        modeTabs,
        helpCards,
        copyButtons,
        moreDigitButtons,
        resultCards,
    } = labDOM.call<WorksheetReferences>('lab_workspace_dom_references');
    export const equationVariable = null;
    export const controlToken = labApplication.configuration.CONTROL_TOKEN;
    export const worksheetSession: WorksheetSession = new WorksheetSession();

    export const resultCardIds = new WeakMap(resultCards.map((card, index) => [card, index]));
    if (!labWire.exports().lab_view_cards_reset(resultCards.length))
        throw new Error('Too many result cards for the native view controller');

    labWire.exports().lab_workspace_reset();
    export const expressionHistory = 0;
    export const forwardHistory = 1;
    export const labEditorState = WorksheetWorkspace.nativeViews.sources();

    labDOM.services(
        labDOM.call<NativeServicePlan>('lab_bootstrap_workspace', 0, {
            token: controlToken,
            get search() {
                return window.location.search;
            },
            get prefix() {
                return String(labApplication.configuration.CONTROL_QUERY_PREFIX);
            },
            get pathname() {
                return window.location.pathname;
            },
            get hash() {
                return window.location.hash;
            }
        }),
        {replaceLocation: (url: string) => window.history.replaceState(null, '', url)});
    export const modePrecisionBits = WorksheetWorkspace.nativeViews.mode('precision');
    export const DEFAULT_EXPRESSION_TEXT = labApplication.configuration.DEFAULT_EXPRESSION;
    export const DEFAULT_EQUATION_TEXT = labApplication.configuration.DEFAULT_EQUATION;
    export const DEFAULT_DIFFEQUATION_TEXT = labApplication.configuration.DEFAULT_DIFFEQUATION;
    export const DEFAULT_MATRIX_TEXT = labApplication.configuration.DEFAULT_MATRIX;
    export const DEFAULT_INTEGRATOR_TEXT = labApplication.configuration.DEFAULT_INTEGRATOR;
    export const DEFAULT_INTEGRATOR_BOUNDS_TEXT = labApplication.configuration.DEFAULT_INTEGRATOR_BOUNDS;
    export const DEFAULT_INTEGRATOR_INTERVAL_CAP = labApplication.configuration.DEFAULT_INTEGRATOR_INTERVAL_CAP;
    export const DEFAULT_DATETIME_TEXT = labApplication.configuration.DEFAULT_DATETIME_TEXT;
    export const DEFAULT_DATETIME_DATE = labApplication.configuration.DEFAULT_DATETIME_DATE;
    export const DEFAULT_DATETIME_JURISDICTION = labApplication.configuration.DEFAULT_DATETIME_JURISDICTION;
    export const DEFAULT_DATETIME_GMT_OFFSET = labApplication.configuration.DEFAULT_DATETIME_GMT_OFFSET;
    export const DEFAULT_ALMANAC_TEXT = labApplication.configuration.DEFAULT_ALMANAC_TEXT;
    export const DEFAULT_ALMANAC_TIME = labApplication.configuration.DEFAULT_ALMANAC_TIME;
    export const DEFAULT_ALMANAC_ZONE = labApplication.configuration.DEFAULT_ALMANAC_ZONE;
    export const DEFAULT_ALMANAC_LATITUDE = labApplication.configuration.DEFAULT_ALMANAC_LATITUDE;
    export const DEFAULT_ALMANAC_LONGITUDE = labApplication.configuration.DEFAULT_ALMANAC_LONGITUDE;
    export const DEFAULT_ALMANAC_VISIBILITY = labApplication.configuration.DEFAULT_ALMANAC_VISIBILITY;
    export const ALMANAC_WORKSHEET_TITLE = labApplication.configuration.ALMANAC_WORKSHEET_TITLE;
    export const ALMANAC_COVERAGE_TEXT = labApplication.configuration.ALMANAC_COVERAGE_TEXT_JS;
    labDOM.services(
        labDOM.call<NativeServicePlan>('lab_bootstrap_workspace', 1, {control: datetimeJurisdiction, fallback: DEFAULT_DATETIME_JURISDICTION}),
        {writeValue: (control: HTMLInputElement, value: unknown) => control.value = String(value)});
    worksheetSession.calendar.datetimeAutoGmtOffset =
        labDOM.call<string>('lab_bootstrap_workspace', 2, {control: datetimeGmtOffset, fallback: DEFAULT_DATETIME_GMT_OFFSET});
    worksheetSession.calendar.almanacVisibilityMode = DEFAULT_ALMANAC_VISIBILITY;
    export const HOLIDAY_JURISDICTION_SET = new Set(labApplication.catalogue.options.map((row) => row[0]));
    export const JURISDICTION_TOWN_OPTIONS = labApplication.catalogue.towns;
    export const LAB_MODE_STORAGE_KEY = 'mars.exprLab.lastMode';
    export const modeEditorText = WorksheetWorkspace.nativeViews.mode('editor');
    Object.assign(modeEditorText, {
        expression: DEFAULT_EXPRESSION_TEXT,
        equation: DEFAULT_EQUATION_TEXT,
        diffequation: DEFAULT_DIFFEQUATION_TEXT,
        matrix: DEFAULT_MATRIX_TEXT,
        integrator: DEFAULT_INTEGRATOR_TEXT,
        datetime: DEFAULT_DATETIME_TEXT,
        almanac: DEFAULT_ALMANAC_TEXT
    });
    WorksheetWorkspace.presentation.resultStates =
        labDOM.call<Partial<Record<WorksheetMode, WorksheetResultSnapshot>>>('lab_workspace_dom_result_states');

    export const marsDatePickerState: WorksheetPickerState = {
        input: null,
        button: null,
        shell: null,
        get year() {
            const native = labWire.exports();
            return native.lab_forms_date_year(native.lab_forms_picker_date());
        },
        get month() {
            const native = labWire.exports();
            return native.lab_forms_date_month(native.lab_forms_picker_date());
        }
    };

}
