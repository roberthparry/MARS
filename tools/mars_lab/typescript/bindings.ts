/**
 * @file bindings.ts
 * @brief Binding controls, authored values and editor synchronisation.
 *
 * Encapsulates delegated binding events and the browser capabilities used by native
 * binding continuations. WorksheetRequests owns request lifetimes; WorksheetWorkspace,
 * WorksheetResults and WorksheetPersistence supply editor, rendering and storage services.
 * Native code retains mathematical interpretation, validation and workflow policy.
 * This classic script defines its namespace before startup initialises worksheet state.
 */

/** Browser binding operations and native continuation capabilities. */
namespace WorksheetBindings {
    /** One delegated subscription per root; native code still selects every action. */
    export class WorksheetBindingEventRouter {
        private readonly subscriptions = new Map<HTMLElement, Map<string, EventListener>>();

        /** Retain the native event classifier without taking ownership of the bridge. */
        public constructor(private readonly dom: Pick<typeof labDOM, 'call'> = labDOM) {
            // Preserve the detachable callback contract of the former namespace functions.
            this.install = this.install.bind(this);
            this.dispose = this.dispose.bind(this);
        }

        /** Roots already subscribed to delegated events; weak ownership permits collection. */
        readonly roots = new WeakSet<HTMLElement>();

        /** Subscribe once per root and dispatch correlated native actions to their handlers. */
        install(...[root, nativeEvent, actions]: LabBindingEventInstallation): void {
            if (this.roots.has(root))
                return;
            this.roots.add(root);
            const listeners = new Map<string, EventListener>();
            this.subscriptions.set(root, listeners);
            for (const type of ['keydown', 'change', 'input', 'pointerdown', 'click']) {
                const listener: EventListener = event => {
                    const action = this.dom.call<LabBindingNativeAction | null>(nativeEvent, root, event);
                    if (action?.prevent)
                        event.preventDefault();
                    if (action)
                        this.dispatch(actions, action);
                };
                listeners.set(type, listener);
                root.addEventListener(type, listener, true);
            }
        }

        /** Remove only subscriptions installed by this router; unrelated roots remain untouched. */
        public dispose(): void {
            for (const [root, listeners] of this.subscriptions) {
                for (const [type, listener] of listeners) root.removeEventListener(type, listener, true);
                this.roots.delete(root);
            }
            this.subscriptions.clear();
        }

        private dispatch<K extends keyof LabBindingEventPayloads>(
            actions: LabBindingEventHandlers, action: LabBindingNativeAction<K>
        ): void {
            void actions[action.action]?.(action);
        }
    }

    /** Native metadata, formatting and assembly capabilities; mathematical decisions remain in C. */
    export class WorksheetEditorAdapter {
        /** Retain narrow host capabilities; DOM and request services are resolved after startup. */
        public constructor(private readonly host: {
            readonly view: typeof WorksheetResults.labEvaluationView;
            readonly flow: WorksheetRequests.NativeFlowRuntime['continue'];
            readonly dom: Pick<typeof labDOM, 'call' | 'services'>;
            readonly workspaceMode: Pick<WorksheetWorkspace.WorksheetModeSession, 'currentMode'>;
        }) {
            // Preserve the detachable callback contract of the former namespace functions.
            this.labEditorData = this.labEditorData.bind(this);
            this.expressionWithSortedConstants = this.expressionWithSortedConstants.bind(this);
            this.currentExpressionText = this.currentExpressionText.bind(this);
            this.expressionBodyForEditor = this.expressionBodyForEditor.bind(this);
            this.expressionReadyToEvaluate = this.expressionReadyToEvaluate.bind(this);
            this.bindingParts = this.bindingParts.bind(this);
            this.compactExpressionForEditor = this.compactExpressionForEditor.bind(this);
            this.expressionWithBindings = this.expressionWithBindings.bind(this);
            this.expressionWithVisibleBindings = this.expressionWithVisibleBindings.bind(this);
            this.bindingsWithAuthoredValues = this.bindingsWithAuthoredValues.bind(this);
            this.replaceBindingKindInExpression = this.replaceBindingKindInExpression.bind(this);
            this.bindingDisplayName = this.bindingDisplayName.bind(this);
        }

        /** Retrieve native editor metadata from the shared evaluation cache. */
        public labEditorData(text: string | null): WorksheetEditorMetadata | null {
            return this.host.dom.call<WorksheetEditorMetadata | null>('lab_editor_metadata', 0, [text], this.host.view.caches.editors);
        }

        /** Request the native expression representation with ordered constants. */
        public expressionWithSortedConstants(text: string): string {
            return this.host.dom.call<string>('lab_editor_metadata', 1, [text], this.host.view.caches.editors);
        }

        /** Obtain the current expression from the native evaluation view. */
        public currentExpressionText(): string {
            return this.host.dom.call<string>('lab_editor_current', this.host.view);
        }

        /** Obtain the native editor body without interpreting its mathematics. */
        public expressionBodyForEditor(text: string): string {
            return this.host.dom.call<string>('lab_editor_metadata', 3, [text], this.host.view.caches.editors);
        }

        /** Ask the native editor whether the current mode is ready for evaluation. */
        public expressionReadyToEvaluate(): boolean {
            return this.host.dom.call<boolean>('lab_editor_ready', [this.host.workspaceMode.currentMode()], this.host.view);
        }

        /** Retrieve native metadata for an expression containing bindings. */
        public bindingParts(text: string): WorksheetEditorMetadata | null {
            return this.host.dom.call<WorksheetEditorMetadata | null>('lab_editor_metadata', 4, [text], this.host.view.caches.editors);
        }

        /** Obtain native compact editor text and its associated bindings. */
        public compactExpressionForEditor(fullText: string): LabBindingCompactEditor {
            return this.host.dom.call<LabBindingCompactEditor>('lab_editor_metadata', 5, [fullText], this.host.view.caches.editors);
        }

        /** Assemble expression text and bindings through the native continuation. */
        public async expressionWithBindings(bodyText: string, bindings: WorksheetBinding[]): Promise<string> {
            return this.host.flow<string>(40, {body: bodyText, bindings});
        }

        /** Assemble visible bindings, retaining the source when no replacement is returned. */
        public async expressionWithVisibleBindings(sourceExpression: string, visibleBindings: WorksheetBinding[]): Promise<string> {
            return await this.expressionWithBindings(sourceExpression, visibleBindings) || sourceExpression;
        }

        /** Ask native code to reconcile authored, evaluated and visible binding values. */
        public bindingsWithAuthoredValues(bindings: WorksheetBinding[], sourceExpression: string, visibleBindings: WorksheetBinding[] = []): WorksheetBinding[] {
            return this.host.dom.call<WorksheetBinding[]>(
                'lab_binding_authored', bindings, this.compactExpressionForEditor(sourceExpression).bindings, visibleBindings);
        }

        /** Request a native change to the named binding's kind. */
        public async replaceBindingKindInExpression(sourceExpression: string, targetName: string, nextKind: string): Promise<string> {
            return this.host.flow<string>(55, {source: sourceExpression, name: targetName, nextKind});
        }

        /** Obtain the native display label for a binding name. */
        public bindingDisplayName(name: string): string {
            return this.host.dom.call<string>('lab_binding_label', [name]);
        }
    }

    /** Owns rendered binding controls, their value caches and delegated card actions. */
    export class WorksheetBindingPanel {
        /** Retain narrow host capabilities; DOM and request services are resolved after startup. */
        public constructor(private readonly host: {
            readonly container: HTMLElement;
            readonly flow: WorksheetRequests.NativeFlowRuntime['continue'];
            readonly dom: Pick<typeof labDOM, 'call' | 'services'>;
            readonly bindingController: Pick<WorksheetBindings.WorksheetBindingController, 'queueBindingInputCommit' | 'toggleBindingKind'>;
            readonly workspacePresentation: Pick<WorksheetWorkspace.WorksheetPresentationController, 'updateHistoryButtons'>;
        }) {
            // Preserve the detachable callback contract of the former namespace functions.
            this.visibleBindingValues = this.visibleBindingValues.bind(this);
            this.clearVariableValues = this.clearVariableValues.bind(this);
            this.renderVariableValues = this.renderVariableValues.bind(this);
            this.dispose = this.dispose.bind(this);
            this.router = new WorksheetBindingEventRouter(host.dom);
        }

        public currentVariables: string[] = [];
        public currentBindingKinds = new Map<string, string>();
        public currentDifferentiable = true;
        public bindingValueCache = new Map<string, string>();
        private readonly router: WorksheetBindingEventRouter;

        /** Release only this panel's delegated event subscriptions. */
        public dispose(): void { this.router.dispose(); }

        /** Card callbacks are resolved at invocation time to preserve capability replacement. */
        public readonly actions = {
            evaluate: ({card}: LabBindingCardPayload): Promise<unknown> => this.host.flow(49, {card, operation: 0}),
            commit: ({card}: LabBindingCardPayload): Promise<unknown> => this.host.bindingController.queueBindingInputCommit(card.input),
            history: (): void => this.host.workspacePresentation.updateHistoryButtons(),
            clear: ({card}: LabBindingCardPayload): Promise<void> => this.host.flow<void>(49, {card, operation: 1}),
            copy: ({card}: LabBindingCardPayload): Promise<void> => this.host.flow<void>(49, {card, operation: 2}),
            toggle: ({card}: LabBindingCardPayload): Promise<void> => this.host.bindingController.toggleBindingKind(card.binding)
        } satisfies LabBindingCardHandlers;

        /** Read binding values from the currently rendered controls. */
        public visibleBindingValues(): WorksheetBinding[] {
            return this.host.dom.call<WorksheetBinding[]>('lab_binding_visible', this.host.container);
        }

        /** Clear rendered binding controls and their cached kinds. */
        public clearVariableValues(): void {
            this.host.dom.call<unknown>('lab_binding_clear', this.host.container);
            this.currentBindingKinds = new Map();
        }

        /** Render binding controls and refresh their value and kind caches. */
        public renderVariableValues(bindings: WorksheetBinding[]): void {
            this.router.install(this.host.container, 'lab_binding_event', this.actions);
            const rendered = this.host.dom.call<WorksheetBindingRendered>('lab_binding_render', this.host.container, bindings);
            this.bindingValueCache = new Map(rendered.values);
            this.currentBindingKinds = new Map(rendered.kinds);
        }
    }

    /** Owns integrator control rendering and delegated edits, separate from native row metadata. */
    export class WorksheetIntegratorPanel {
        /** Retain narrow host capabilities; DOM and request services are resolved after startup. */
        public constructor(private readonly host: {
            readonly modeEditors: Record<WorksheetMode, string>;
            readonly container: HTMLElement;
            readonly view: typeof WorksheetResults.labEvaluationView;
            readonly flow: WorksheetRequests.NativeFlowRuntime['continue'];
            readonly dom: Pick<typeof labDOM, 'call' | 'services'>;
            readonly workspaceIntegrator: Pick<WorksheetWorkspace.WorksheetIntegratorController, 'currentIntegratorBoundNames'>;
            readonly forms: Pick<WorksheetIntegratorForms, 'nextRevision' | 'metadata'>;
            readonly bindingPanel: Pick<WorksheetBindings.WorksheetBindingPanel, 'renderVariableValues' | 'clearVariableValues'>;
            readonly bindingController: Pick<WorksheetBindings.WorksheetBindingController, 'setExpressionEditor'>;
        }) {
            // Preserve the detachable callback contract of the former namespace functions.
            this.editIntegratorRow = this.editIntegratorRow.bind(this);
            this.prepareIntegratorBindingInput = this.prepareIntegratorBindingInput.bind(this);
            this.renderIntegratorRows = this.renderIntegratorRows.bind(this);
            this.applyIntegratorBindingState = this.applyIntegratorBindingState.bind(this);
            this.dispose = this.dispose.bind(this);
            this.router = new WorksheetBindingEventRouter(host.dom);
        }

        private readonly router: WorksheetBindingEventRouter;

        /** Release only this panel's delegated event subscriptions. */
        public dispose(): void { this.router.dispose(); }

        private readonly actions = {
            invalidate: ({card}: LabBindingInputPayload): void => {
                this.host.forms.nextRevision();
                this.host.forms.metadata = null;
                card.input.setCustomValidity('');
            },
            prepare: ({card}: LabBindingInputPayload): Promise<void> => this.prepareIntegratorBindingInput(card.input),
            edit: ({item, index, operation}: Omit<LabBindingRowPayload, 'card'>): Promise<void> => this.editIntegratorRow(item, index, operation)
        } satisfies LabBindingIntegratorHandlers;

        /** Apply a native integrator row operation through its continuation. */
        public async editIntegratorRow(item: HTMLElement, index: number, operation: number): Promise<void> {
            return this.host.flow<void>(47, {item, index, operation});
        }

        /** Prepare an integrator input using native validation and binding rules. */
        public async prepareIntegratorBindingInput(input: HTMLInputElement): Promise<void> {
            return this.host.flow<void>(48, {input});
        }

        /** Render native-prepared integrator rows and install delegated input handling. */
        public renderIntegratorRows(rows: WorksheetIntegratorRow[]): void {
            this.host.forms.nextRevision();
            const safeRows = this.host.dom.call<WorksheetIntegratorRow[]>('lab_binding_rows_prepare', rows);
            this.router.install(this.host.container, 'lab_binding_integrator_event', this.actions);
            this.host.dom.call<unknown>('lab_binding_integrator_render', this.host.container, safeRows);
        }

        /** Apply native-validated integrator binding state and its editor effects. */
        public applyIntegratorBindingState(data: unknown, fallbackExpression?: string): void {
            this.host.dom.services(
                this.host.dom.call<NativeServicePlan>(
                    'lab_binding_integrator_state', data, fallbackExpression, this.host.view.caches.editors,
                    this.host.workspaceIntegrator.currentIntegratorBoundNames()),
                {
                    setExpressionEditor: this.host.bindingController.setExpressionEditor.bind(this.host.bindingController),
                    clearVariableValues: this.host.bindingPanel.clearVariableValues.bind(this.host.bindingPanel),
                    renderVariableValues: this.host.bindingPanel.renderVariableValues.bind(this.host.bindingPanel),
                    setIntegratorBindingExpression: (expression: string) => this.host.modeEditors.integrator = expression
                });
        }
    }

    /** Coordinates goal-source preparation without retaining a second native source model. */
    export class WorksheetGoalController {
        /** Retain narrow host capabilities; DOM and request services are resolved after startup. */
        public constructor(private readonly host: {
            readonly input: HTMLTextAreaElement;
            readonly flow: WorksheetRequests.NativeFlowRuntime['continue'];
            readonly dom: Pick<typeof labDOM, 'call' | 'services'>;
            readonly wire: Pick<typeof labWire, 'exports'>;
            readonly bindingController: Pick<WorksheetBindings.WorksheetBindingController, 'labBindingFlowServices'>;
            readonly buffer: Pick<WorksheetNativeBuffer, 'stageText' | 'readText'>;
            readonly bindingEditor: Pick<WorksheetBindings.WorksheetEditorAdapter, 'labEditorData'>;
            readonly bindingPanel: Pick<WorksheetBindings.WorksheetBindingPanel, 'currentVariables' | 'bindingValueCache'>;
        }) {
            // Preserve the detachable callback contract of the former namespace functions.
            this.canGoalSeek = this.canGoalSeek.bind(this);
            this.clearGoalSeekRequest = this.clearGoalSeekRequest.bind(this);
            this.currentGoalSeekSource = this.currentGoalSeekSource.bind(this);
            this.solvedStartValuesForGoalSeek = this.solvedStartValuesForGoalSeek.bind(this);
            this.goalSeekExpressionAndStarts = this.goalSeekExpressionAndStarts.bind(this);
        }

        /** Report whether the current editor exposes variables for goal seeking. */
        public canGoalSeek(): boolean {
            return this.host.bindingPanel.currentVariables.length > 0;
        }

        /** Apply the native plan for clearing the goal-seek source. */
        public clearGoalSeekRequest(): void {
            this.host.dom.services(this.host.dom.call<NativeServicePlan>('lab_binding_projection_clear', 1), this.host.bindingController.labBindingFlowServices());
        }

        /** Resolve the goal-seek source in the native workspace and read its text. */
        public currentGoalSeekSource(): string {
            const length = this.host.wire.exports().lab_workspace_source_resolve(this.host.buffer.stageText(this.host.input.value.trim()), 1);
            return this.host.buffer.readText(length);
        }

        /** Resolve goal-seek starting values using native precedence rules. */
        public solvedStartValuesForGoalSeek(sourceExpression: string, solvedExpression: string, providedStart: Record<string, string> = {}): Record<string, string> {
            return this.host.dom.call<Record<string, string>>(
                'lab_binding_goal_starts', this.host.bindingEditor.labEditorData(sourceExpression), this.host.bindingEditor.labEditorData(solvedExpression), providedStart,
                this.host.bindingPanel.bindingValueCache);
        }

        /** Prepare the native goal-seek expression and starting-value map. */
        public async goalSeekExpressionAndStarts(sourceExpression: string, providedStart: Record<string, string> = {}): Promise<LabBindingGoalStart> {
            return this.host.flow<LabBindingGoalStart>(46, {source: sourceExpression, providedStart});
        }
    }

    /** Owns binding commit sequencing, refresh timers and native continuation capabilities. */
    export class WorksheetBindingController {
        /** Retain narrow host capabilities; DOM and request services are resolved after startup. */
        public constructor(private readonly host: {
            readonly input: HTMLTextAreaElement;
            readonly sources: WorksheetSources;
            readonly modeEditors: Record<WorksheetMode, string>;
            readonly container: HTMLElement;
            readonly integratorContainer: HTMLElement;
            readonly defaultDatetimeText: string;
            readonly setTimer: (callback: () => void, delay: number) => number;
            readonly clearTimer: (timer: number) => void;
            readonly requests: Pick<typeof WorksheetRequests.labRequests, 'begin' | 'finish' | 'run' | 'cancel'>;
            readonly flow: WorksheetRequests.NativeFlowRuntime['continue'];
            readonly dom: Pick<typeof labDOM, 'call' | 'services'>;
            readonly wire: Pick<typeof labWire, 'exports'>;
            readonly requestPresentation: Pick<WorksheetRequests.EditorPresentationController, 'prepareLabEditor' | 'requestLabPresentation'>;
            readonly evaluationTransport: Pick<WorksheetRequests.EvaluationTransport, 'fetchEvaluation'>;
            readonly persistence: Pick<WorksheetPersistence.WorksheetPersistenceController, 'saveWorksheet'>;
            readonly results: { writeClipboardText(text: string): Promise<void>; flashCopyButton(button: WorksheetCopyButton, success: boolean): void };
            readonly bindingEditor: Pick<WorksheetBindings.WorksheetEditorAdapter, 'replaceBindingKindInExpression' | 'compactExpressionForEditor' | 'bindingsWithAuthoredValues' | 'expressionBodyForEditor' | 'currentExpressionText' | 'labEditorData' | 'bindingParts'>;
            readonly workspaceBindingSelection: Pick<WorksheetWorkspace.WorksheetBindingSelection, 'visibleBindingsForCurrentMode' | 'variableNamesFromBindings'>;
            readonly workspaceIntegrator: Pick<WorksheetWorkspace.WorksheetIntegratorController, 'currentIntegratorBoundNames' | 'refreshIntegratorForms' | 'currentIntegratorRows'>;
            readonly workspaceMode: Pick<WorksheetWorkspace.WorksheetModeSession, 'saveCurrentModeEditorState' | 'evaluateFromKeyboard' | 'workspaceModeId' | 'currentMode'>;
            readonly workspaceLayout: Pick<WorksheetWorkspace.WorksheetEditorLayout, 'scheduleEditorResizeGrip'>;
            readonly workspacePresentation: Pick<WorksheetWorkspace.WorksheetPresentationController, 'renderDerivativeButtons' | 'updateHistoryButtons' | 'setStatus'>;
            readonly forms: Pick<WorksheetIntegratorForms, 'edit'>;
            readonly bindingPanel: Pick<WorksheetBindings.WorksheetBindingPanel, 'renderVariableValues' | 'clearVariableValues' | 'currentVariables' | 'currentDifferentiable' | 'bindingValueCache'>;
            readonly bindingIntegratorPanel: Pick<WorksheetBindings.WorksheetIntegratorPanel, 'renderIntegratorRows'>;
            readonly bindingGoal: Pick<WorksheetBindings.WorksheetGoalController, 'clearGoalSeekRequest'>;
            readonly buffer: Pick<WorksheetNativeBuffer, 'stageText'>;
        }) {
            // Preserve the detachable callback contract of the former namespace functions.
            this.labEditorSnapshot = this.labEditorSnapshot.bind(this);
            this.clearExpressionSource = this.clearExpressionSource.bind(this);
            this.applyUpdatedBindingExpression = this.applyUpdatedBindingExpression.bind(this);
            this.applyMarsBindingExpression = this.applyMarsBindingExpression.bind(this);
            this.applyMarsBindingsToEditedExpression = this.applyMarsBindingsToEditedExpression.bind(this);
            this.scheduleEditedExpressionBindingRefresh = this.scheduleEditedExpressionBindingRefresh.bind(this);
            this.commitLabBindingValues = this.commitLabBindingValues.bind(this);
            this.commitBindingInput = this.commitBindingInput.bind(this);
            this.commitVisibleBindingInputs = this.commitVisibleBindingInputs.bind(this);
            this.toggleBindingKind = this.toggleBindingKind.bind(this);
            this.refreshVariableValuesFromEditor = this.refreshVariableValuesFromEditor.bind(this);
            this.queueBindingInputCommit = this.queueBindingInputCommit.bind(this);
            this.setExpressionEditor = this.setExpressionEditor.bind(this);
            this.refreshWorksheetFromInput = this.refreshWorksheetFromInput.bind(this);
            this.labBindingFlowServices = this.labBindingFlowServices.bind(this);
            this.dispose = this.dispose.bind(this);
        }

        public expressionBindingRefreshTimer = 0;
        public pendingExpressionBindingCommit: Promise<unknown> = Promise.resolve();
        private readonly feedbackTimers = new Set<number>();
        private readonly refreshRequests = new Set<WorksheetRequest>();
        private registry: ReturnType<WorksheetBindingController['createLabBindingServices']> | undefined;

        private readonly commitEffects = Object.freeze({
            source: (text: string) => this.host.sources.fullText = text,
            cache: (snapshot: WorksheetSnapshot, editor: HTMLTextAreaElement) => this.host.dom.call<unknown>('lab_binding_committed_cache', snapshot, editor, this.host.bindingPanel.bindingValueCache),
            applyUpdatedBindingExpression: (text: string) => this.applyUpdatedBindingExpression(text),
            applyMarsBindingExpression: (...args: Parameters<WorksheetBindings.WorksheetBindingController['applyMarsBindingExpression']>) => this.applyMarsBindingExpression(...args),
            refreshVariableValuesFromEditor: (...args: Parameters<WorksheetBindings.WorksheetBindingController['refreshVariableValuesFromEditor']>) => this.refreshVariableValuesFromEditor(...args),
            updateHistoryButtons: () => this.host.workspacePresentation.updateHistoryButtons(),
            saveCurrentModeEditorState: () => this.host.workspaceMode.saveCurrentModeEditorState()
        });

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
            this.host.dom.services(this.host.dom.call<NativeServicePlan>('lab_binding_projection_clear', 0), this.labBindingFlowServices());
        }

        /** Apply the native editor projection for an updated binding expression. */
        public applyUpdatedBindingExpression(updated: string): void {
            this.host.dom.services(
                this.host.dom.call<NativeServicePlan>('lab_binding_projection_update', this.host.workspaceMode.workspaceModeId(this.host.workspaceMode.currentMode()), updated, this.host.bindingEditor.labEditorData(updated)),
                this.labBindingFlowServices());
        }

        /** Project a binding expression within an owned binding-commit request. */
        public async applyMarsBindingExpression(updated: string, editorBodyText: string | null = null): Promise<boolean | undefined> {
            const mode = this.host.workspaceMode.currentMode(), editorSnapshot = this.host.input.value;
            return this.host.requests.run(
                'bindingCommit', mode, (request: WorksheetRequest) => this.host.flow<boolean>(52, {updated, editorBodyText, editorSnapshot, request}));
        }

        /** Synchronise evaluated bindings with edited text under the supplied request. */
        public async applyMarsBindingsToEditedExpression(editedBody: string, sourceExpression: string, data: WorksheetEvaluationData, request: WorksheetRequest): Promise<void> {
            return this.host.flow<void>(53, {editedBody, sourceExpression, data, request});
        }

        /** Finish the admitted refresh request even when native preparation fails. */
        private async refreshEditedExpressionBindings(editedBody: string, sourceExpression: string, request: WorksheetRequest): Promise<void> {
            try {
                return await this.host.flow<void>(54, {editedBody, sourceExpression, request});
            } finally {
                this.host.requests.finish(request);
                this.refreshRequests.delete(request);
            }
        }

        /** Apply native refresh admission and scheduling plans for edited bindings. */
        public scheduleEditedExpressionBindingRefresh(): void {
            const context = {editedBody: this.host.input.value, sourceExpression: this.host.sources.fullText};
            for (let phase = 0; phase < 2; ++phase)
                this.host.dom.services(this.host.dom.call<NativeServicePlan>('lab_binding_projection_refresh', phase, context), this.labBindingFlowServices());
        }

        /** Commit selected inputs while honouring the caller's freshness guard. */
        public async commitLabBindingValues(inputs: HTMLInputElement[], isCurrent: WorksheetGuard = () => true): Promise<boolean | undefined> {
            return this.host.flow<boolean | undefined>(51, {inputs, isCurrent});
        }

        /** Commit a single binding input through the shared native workflow. */
        public async commitBindingInput(input: HTMLInputElement): Promise<boolean | undefined> {
            return this.commitLabBindingValues([input]);
        }

        /** Commit visible inputs after native pending-work and freshness checks. */
        public async commitVisibleBindingInputs(isCurrent: WorksheetGuard = () => true): Promise<boolean | undefined> {
            return this.host.flow<boolean | undefined>(41, {isCurrent});
        }

        /** Delegate a binding-kind toggle to the native continuation. */
        public async toggleBindingKind(binding: WorksheetBinding): Promise<void> {
            return this.host.flow<void>(43, {binding});
        }

        /** Refresh binding controls from editor text while honouring request freshness. */
        public async refreshVariableValuesFromEditor(isCurrent: WorksheetGuard = () => true): Promise<void> {
            return this.host.flow<void>(44, {isCurrent});
        }

        /** Serialise input commits and report rejected work through worksheet status. */
        public queueBindingInputCommit(input: HTMLInputElement): Promise<unknown> {
            const mode = this.host.workspaceMode.currentMode();
            const pending = this.pendingExpressionBindingCommit.then(() => this.host.flow(45, {mode, input}));
            this.pendingExpressionBindingCommit = pending.catch((err: unknown) => this.host.workspacePresentation.setStatus(String(err)));
            return this.pendingExpressionBindingCommit;
        }

        /** Apply native editor preparation and projection without rewriting mathematics. */
        public setExpressionEditor(fullText: string, evaluatedBindings: WorksheetBinding[] | null = null, editorBodyText: string | null = null, evaluationReady: unknown = null): void {
            this.host.dom.services(
                this.host.dom.call<NativeServicePlan>(
                    'lab_binding_projection_prepare',
                    {mode: this.host.workspaceMode.currentMode(), fullText, evaluatedBindings, editorBodyText, evaluationReady},
                    this.host.bindingEditor.labEditorData(fullText)),
                this.labBindingFlowServices());
            const mode = this.host.workspaceMode.workspaceModeId(this.host.workspaceMode.currentMode());
            const selected = this.host.dom.call<unknown>(
                'lab_binding_editor', mode, {
                    text: fullText,
                    metadata: this.host.bindingEditor.labEditorData(fullText),
                    body: editorBodyText,
                    bodyMetadata: this.host.bindingEditor.labEditorData(editorBodyText),
                    bindings: evaluatedBindings,
                    ready: evaluationReady
                },
                mode === 4 ? this.host.workspaceIntegrator.currentIntegratorBoundNames() : null);
            this.host.dom.services(this.host.dom.call<NativeServicePlan>('lab_binding_projection_finish', selected), this.labBindingFlowServices());
        }

        /** Apply native input decisions while retaining editor storage and refresh timers in the host. */
        public refreshWorksheetFromInput(): void {
            this.host.requests.cancel('evaluate');
            this.host.requests.cancel('bindings');
            const mode = this.host.workspaceMode.currentMode(), native = this.host.wire.exports();
            const plan = this.host.dom.call<NativeServicePlan>(
                'lab_events_refresh', this.host.workspaceMode.workspaceModeId(mode), Number(!!this.host.bindingEditor.bindingParts(this.host.input.value)),
                native.lab_workspace_source_matches(this.host.buffer.stageText(this.host.input.value.trim())),
                {editor: this.host.input, defaultDatetimeText: this.host.defaultDatetimeText});
            this.host.dom.services(plan, {
                setModeEditor: (mode: WorksheetMode, text: string) => {
                    this.host.modeEditors[mode] = text;
                },
                setSource: (field: WorksheetSourceKey, text: string) => {
                    this.host.sources[field] = text;
                },
                refreshVariableValuesFromEditor: this.refreshVariableValuesFromEditor.bind(this),
                saveState: (mode: WorksheetMode, state: {text: string; options?: WorksheetSaveOptions}) => this.host.persistence.saveWorksheet(mode, state.text, state.options),
                cancelBindingRefresh: () => this.host.clearTimer(this.expressionBindingRefreshTimer),
                markBindingRefresh: (editor: HTMLTextAreaElement) => this.host.dom.call<unknown>('lab_events_refresh_mark', editor),
                clearGoalSeekRequest: this.host.bindingGoal.clearGoalSeekRequest.bind(this.host.bindingGoal),
                scheduleEditedExpressionBindingRefresh: this.scheduleEditedExpressionBindingRefresh.bind(this),
                updateHistoryButtons: this.host.workspacePresentation.updateHistoryButtons.bind(this.host.workspacePresentation)
            });
        }

        /** Return the lazily created browser capability registry for native binding flows. */
        public labBindingFlowServices(): ReturnType<WorksheetBindingController['createLabBindingServices']> {
            return this.registry ||= this.createLabBindingServices();
        }

        /** Construct late-bound capabilities so replacement methods remain observable. */
        private createLabBindingServices() {
            return Object.freeze({
                bindingCancelRefresh: () => this.host.clearTimer(this.expressionBindingRefreshTimer),
                bindingBeginRefresh: (context: {request?: WorksheetRequest | null}, operation: Parameters<typeof WorksheetRequests.labRequests.begin>[0], mode: WorksheetMode, options: Parameters<typeof WorksheetRequests.labRequests.begin>[2]) => {
                    const request = this.host.requests.begin(operation, mode, options);
                    context.request = request;
                    if (request) this.refreshRequests.add(request);
                    return request;
                },
                bindingRefreshPending: () => this.host.input.dataset.bindingRefreshValid = 'pending',
                bindingRefreshHistory: () => this.host.workspacePresentation.updateHistoryButtons(),
                bindingRefreshTimer: (editedBody: string, sourceExpression: string, request: WorksheetRequest, delay: number) => {
                    this.expressionBindingRefreshTimer = this.host.setTimer(() => {
                        void this.refreshEditedExpressionBindings(editedBody, sourceExpression, request);
                    }, delay);
                },
                bindingSource: (field: WorksheetSourceKey, value: string) => this.host.sources[field] = value,
                bindingResetCache: () => this.host.bindingPanel.bindingValueCache = new Map(),
                bindingClearFlags: () => {
                    delete this.host.input.dataset.bindingRefreshValid;
                    delete this.host.input.dataset.evaluationReady;
                },
                bindingClearGoal: () => this.host.bindingGoal.clearGoalSeekRequest(),
                bindingClearSource: () => this.clearExpressionSource(),
                bindingClearValues: () => this.host.bindingPanel.clearVariableValues(),
                bindingSetText: (text: string) => this.host.input.value = text,
                bindingDeferEditor: (context: Record<string, unknown>) => {
                    void this.host.flow(50, context);
                },
                bindingResize: () => this.host.workspaceLayout.scheduleEditorResizeGrip(),
                bindingRenderValues: (bindings: WorksheetBinding[]) => this.host.bindingPanel.renderVariableValues(bindings),
                bindingSetVariables: (bindings: WorksheetBinding[]) => this.host.bindingPanel.currentVariables = this.host.workspaceBindingSelection.variableNamesFromBindings(bindings),
                bindingRenderDerivatives: () => this.host.workspacePresentation.renderDerivativeButtons(this.host.bindingPanel.currentVariables),
                bindingCapture: () => ({
                    mode: this.host.workspaceMode.currentMode(),
                    modeId: this.host.workspaceMode.workspaceModeId(this.host.workspaceMode.currentMode()),
                    source: this.host.bindingEditor.currentExpressionText(),
                    text: this.host.input.value,
                    editor: this.host.input
                }),
                bindingCurrent: (callback: WorksheetGuard) => callback(),
                bindingPending: () => this.pendingExpressionBindingCommit,
                bindingCommitVisible: (isCurrent: WorksheetGuard) =>
                    this.commitLabBindingValues(Array.from(this.host.container.querySelectorAll<HTMLInputElement>('.binding-value-input')), isCurrent),
                bindingRunCommit: (frame: WorksheetBindingCommitFrame) => this.host.requests.run('bindingCommit', frame.before.mode, (request: WorksheetRequest) => this.host.flow(42, {
                                                                                                   request,
                                                                                                   source: frame.before.source,
                                                                                                   modeId: frame.before.modeId,
                                                                                                   inputs: frame.inputs,
                                                                                                   isCurrent: frame.isCurrent
                                                                                               })),
                bindingCommitInput: (input: HTMLInputElement) => this.commitBindingInput(input),
                bindingPrepare: (text: string) => this.host.requestPresentation.prepareLabEditor(text),
                bindingPresentation: (...args: Parameters<WorksheetRequests.EditorPresentationController['requestLabPresentation']>) => this.host.requestPresentation.requestLabPresentation(...args),
                bindingExpression: (data: WorksheetEvaluationData) => data.editor.expression,
                bindingInvalid: () => {
                    throw new TypeError('Cannot read properties of null or undefined binding');
                },
                bindingEffects: (plan: NativeServicePlan) => this.host.dom.services(plan, this.commitEffects),
                bindingReplaceKind: (...args: Parameters<WorksheetBindings.WorksheetEditorAdapter['replaceBindingKindInExpression']>) => this.host.bindingEditor.replaceBindingKindInExpression(...args),
                bindingScheduleRefresh: () => this.scheduleEditedExpressionBindingRefresh(),
                bindingProjectVisible: (source: string) => {
                    const bindings = this.host.workspaceBindingSelection.visibleBindingsForCurrentMode(this.host.bindingEditor.compactExpressionForEditor(source).bindings);
                    this.host.bindingPanel.renderVariableValues(bindings);
                    this.host.bindingPanel.currentVariables = this.host.workspaceBindingSelection.variableNamesFromBindings(bindings);
                    this.host.workspacePresentation.renderDerivativeButtons(this.host.bindingPanel.currentVariables);
                },
                bindingPrepareRows: (index: number, operation: number) =>
                    this.host.workspaceIntegrator.refreshIntegratorForms(this.host.forms.edit(this.host.workspaceIntegrator.currentIntegratorRows(), index, operation)),
                bindingRenderRows: (rows: WorksheetIntegratorRow[]) => this.host.bindingIntegratorPanel.renderIntegratorRows(rows),
                bindingRowError: (item: HTMLElement, error: {message: string}) => item.title = error.message,
                bindingRefresh: () => this.refreshVariableValuesFromEditor(),
                bindingRefreshForms: () => this.host.workspaceIntegrator.refreshIntegratorForms(),
                bindingUpdateForms: (prepared: unknown) => this.host.dom.call<unknown>('lab_binding_integrator_update', this.host.integratorContainer, prepared),
                bindingValidity: (input: HTMLInputElement, error: {message: string}) => {
                    input.setCustomValidity(error.message);
                    input.reportValidity();
                },
                bindingQueue: (input: HTMLInputElement) => this.queueBindingInputCommit(input),
                bindingRefocus: (card: WorksheetCalendarCard) => this.host.dom.call<unknown>('lab_binding_refocus', this.host.container, card, this.host.input),
                bindingEvaluate: () => this.host.workspaceMode.evaluateFromKeyboard(),
                bindingClipboard: (text: string) => this.host.results.writeClipboardText(text),
                bindingFlash: (button: WorksheetCopyButton, success: boolean) => this.host.results.flashCopyButton(button, success),
                bindingReadyTimer: () => {
                    const timer = this.host.setTimer(() => {
                        this.feedbackTimers.delete(timer);
                        this.host.workspacePresentation.setStatus('Ready');
                    }, 1000);
                    this.feedbackTimers.add(timer);
                    return timer;
                },
                bindingSetEditor: (...args: Parameters<WorksheetBindings.WorksheetBindingController['setExpressionEditor']>) => this.setExpressionEditor(...args),
                bindingFetch: (text: string, source: string, request: WorksheetRequest) => this.host.evaluationTransport.fetchEvaluation(text, '', 'bindings', source, '', request),
                bindingAuthored: (bindings: WorksheetBinding[], source: string) => this.host.bindingEditor.bindingsWithAuthoredValues(bindings, source),
                bindingBody: (text: string) => this.host.bindingEditor.expressionBodyForEditor(text),
                bindingSaveMode: () => this.host.workspaceMode.saveCurrentModeEditorState(),
                bindingInspectText: (text: string) => ({parts: this.host.bindingEditor.bindingParts(text), body: this.host.bindingEditor.expressionBodyForEditor(text)}),
                bindingProjectEdited: (assembled: string, body: string, bindings: WorksheetBinding[], data: WorksheetEvaluationData) => {
                    this.host.sources.fullText = assembled;
                    this.host.sources.displayText = body;
                    const projected = this.host.dom.call<{variables: string[]; differentiable: boolean}>('lab_binding_sync_apply', this.host.input, {displayText: body, bindings, data});
                    this.host.bindingPanel.renderVariableValues(bindings);
                    this.host.bindingPanel.currentVariables = projected.variables;
                    this.host.bindingPanel.currentDifferentiable = projected.differentiable;
                    this.host.workspacePresentation.renderDerivativeButtons(this.host.bindingPanel.currentVariables);
                    this.host.persistence.saveWorksheet('expression', this.host.sources.fullText, {debounce: true});
                },
                bindingApplyEdited: (...args: Parameters<WorksheetBindings.WorksheetBindingController['applyMarsBindingsToEditedExpression']>) => this.applyMarsBindingsToEditedExpression(...args),
                bindingMarkInvalid: () => this.host.input.dataset.bindingRefreshValid = 'false'
            });
        }
    }

    /** Session-owned native metadata, formatting and assembly capabilities; mathematical decisions remain in C. */
    export const editor: WorksheetEditorAdapter = new WorksheetEditorAdapter({
        get view() { return WorksheetResults.labEvaluationView; },
        get flow() { return WorksheetRequests.runtime.continue.bind(WorksheetRequests.runtime); },
        get dom() { return labDOM; },
        get workspaceMode() { return WorksheetWorkspace.mode; }
    });

    /** Session-owned rendered binding controls, their value caches and delegated card actions. */
    export const panel: WorksheetBindingPanel = new WorksheetBindingPanel({
        get container() { return WorksheetContext.variableValues; },
        get flow() { return WorksheetRequests.runtime.continue.bind(WorksheetRequests.runtime); },
        get dom() { return labDOM; },
        get bindingController() { return WorksheetBindings.controller; },
        get workspacePresentation() { return WorksheetWorkspace.presentation; }
    });

    /** Session-owned integrator control rendering and delegated edits, separate from native row metadata. */
    export const integratorPanel: WorksheetIntegratorPanel = new WorksheetIntegratorPanel({
        get modeEditors() { return WorksheetContext.modeEditorText; },
        get container() { return WorksheetContext.integratorBoundStack; },
        get view() { return WorksheetResults.labEvaluationView; },
        get flow() { return WorksheetRequests.runtime.continue.bind(WorksheetRequests.runtime); },
        get dom() { return labDOM; },
        get workspaceIntegrator() { return WorksheetWorkspace.integrator; },
        get forms() { return WorksheetWorkspace.integrator.forms; },
        get bindingPanel() { return WorksheetBindings.panel; },
        get bindingController() { return WorksheetBindings.controller; }
    });

    /** Session-owned coordinator for goal-source preparation without retaining a second native source model. */
    export const goal: WorksheetGoalController = new WorksheetGoalController({
        get input() { return WorksheetContext.expr; },
        get flow() { return WorksheetRequests.runtime.continue.bind(WorksheetRequests.runtime); },
        get dom() { return labDOM; },
        get wire() { return labWire; },
        get bindingController() { return WorksheetBindings.controller; },
        get buffer() { return WorksheetWorkspace.labWorkspaceBuffer; },
        get bindingEditor() { return WorksheetBindings.editor; },
        get bindingPanel() { return WorksheetBindings.panel; }
    });

    /** Session-owned binding commit sequencing, refresh timers and native continuation capabilities. */
    export const controller: WorksheetBindingController = new WorksheetBindingController({
        get input() { return WorksheetContext.expr; },
        get sources() { return WorksheetContext.labEditorState; },
        get modeEditors() { return WorksheetContext.modeEditorText; },
        get container() { return WorksheetContext.variableValues; },
        get integratorContainer() { return WorksheetContext.integratorBoundStack; },
        get defaultDatetimeText() { return WorksheetContext.DEFAULT_DATETIME_TEXT; },
        setTimer: (callback, delay) => setTimeout(callback, delay),
        clearTimer: timer => clearTimeout(timer),
        get requests() { return WorksheetRequests.labRequests; },
        get flow() { return WorksheetRequests.runtime.continue.bind(WorksheetRequests.runtime); },
        get dom() { return labDOM; },
        get wire() { return labWire; },
        get requestPresentation() { return WorksheetRequests.presentation; },
        get evaluationTransport() { return WorksheetRequests.evaluationTransport; },
        get persistence() { return WorksheetPersistence.persistence; },
        results: {
            writeClipboardText: text => WorksheetResults.clipboard.write(text),
            flashCopyButton: (button, success) => WorksheetResults.clipboard.flash(button, success)
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

    /** Return editor text, retaining the empty-text fallback. */
    export function expression_text_or_empty(text: string): string {
        return String(text || '');
    }

}
