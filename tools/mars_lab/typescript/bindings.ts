/**
 * @file bindings.ts
 * @brief Binding controls, authored values and editor synchronisation.
 *
 * Encapsulates delegated binding events and the browser capabilities used by native
 * binding continuations. bindings_controller.ts owns commit queues and refresh work.
 * WorksheetRequests owns request lifetimes; WorksheetWorkspace,
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
        public constructor(private readonly dom: BindingProjectionNative = labDOM) {
            marsBindCallbacks(this, [
                'install', 'dispose'
            ]);
        }

        /** Roots already subscribed; dispose releases the retained listeners and roots. */
        readonly roots = new WeakSet<HTMLElement>();

        /** Subscribe once per root and dispatch correlated native actions to their handlers. */
        public install(...[root, nativeEvent, actions]: LabBindingEventInstallation): void {
            if (this.roots.has(root))
                return;
            this.roots.add(root);
            const listeners = new Map<string, EventListener>();
            this.subscriptions.set(root, listeners);
            // One closure retains this root's classifier and replaceable handler table.
            const owner = this;
            function handleEvent(event: Event): void {
                const action = owner.dom.call(nativeEvent, root, event) as LabBindingNativeAction | null;
                if (action?.prevent)
                    event.preventDefault();
                if (action)
                    owner.dispatch(actions, action);
            }
            for (const type of ['keydown', 'change', 'input', 'pointerdown', 'click']) {
                const listener: EventListener = handleEvent;
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

        /** Dispatch by payload shape; the handler table selects the individual action. */
        private dispatch(actions: LabBindingEventHandlers, action: LabBindingNativeAction): void {
            switch (action.action) {
                case 'evaluate':
                case 'commit':
                case 'history':
                case 'clear':
                case 'copy':
                case 'toggle':
                    void actions[action.action]?.(action);
                    break;
                case '':
                case 'invalidate':
                case 'prepare':
                    void actions[action.action]?.(action);
                    break;
                case 'edit':
                    void actions.edit?.(action);
                    break;
            }
        }
    }

    /** Native metadata, formatting and assembly capabilities; mathematical decisions remain in C. */
    export class WorksheetEditorAdapter {
        /** Retain narrow host capabilities; DOM and request services are resolved after startup. */
        public constructor(private readonly host: EditorDependencies) {
            marsBindCallbacks(this, [
                'labEditorData', 'expressionWithSortedConstants', 'currentExpressionText',
                'expressionBodyForEditor', 'expressionReadyToEvaluate', 'bindingParts',
                'compactExpressionForEditor', 'expressionWithBindings', 'expressionWithVisibleBindings',
                'bindingsWithAuthoredValues', 'replaceBindingKindInExpression', 'bindingDisplayName'
            ]);
        }

        /** Retrieve native editor metadata from the shared evaluation cache. */
        public labEditorData(text: string | null): WorksheetEditorMetadata | null {
            return this.host.dom.call('lab_editor_metadata', 0, [text], this.host.view.caches.editors) as WorksheetEditorMetadata | null;
        }

        /** Request the native expression representation with ordered constants. */
        public expressionWithSortedConstants(text: string): string {
            return this.host.dom.call('lab_editor_metadata', 1, [text], this.host.view.caches.editors) as string;
        }

        /** Obtain the current expression from the native evaluation view. */
        public currentExpressionText(): string {
            return this.host.dom.call('lab_editor_current', this.host.view) as string;
        }

        /** Obtain the native editor body without interpreting its mathematics. */
        public expressionBodyForEditor(text: string): string {
            return this.host.dom.call('lab_editor_metadata', 3, [text], this.host.view.caches.editors) as string;
        }

        /** Ask the native editor whether the current mode is ready for evaluation. */
        public expressionReadyToEvaluate(): boolean {
            return this.host.dom.call('lab_editor_ready', [this.host.workspaceMode.currentMode()], this.host.view) as boolean;
        }

        /** Retrieve native metadata for an expression containing bindings. */
        public bindingParts(text: string): WorksheetEditorMetadata | null {
            return this.host.dom.call('lab_editor_metadata', 4, [text], this.host.view.caches.editors) as WorksheetEditorMetadata | null;
        }

        /** Obtain native compact editor text and its associated bindings. */
        public compactExpressionForEditor(fullText: string): LabBindingCompactEditor {
            return this.host.dom.call('lab_editor_metadata', 5, [fullText], this.host.view.caches.editors) as LabBindingCompactEditor;
        }

        /** Assemble expression text and bindings through the native continuation. */
        public async expressionWithBindings(bodyText: string, bindings: WorksheetBinding[]): Promise<string> {
            return this.host.flow(BindingFlow.AssembleExpression, {body: bodyText, bindings}) as Promise<string>;
        }

        /** Assemble visible bindings, retaining the source when no replacement is returned. */
        public async expressionWithVisibleBindings(sourceExpression: string, visibleBindings: WorksheetBinding[]): Promise<string> {
            return await this.expressionWithBindings(sourceExpression, visibleBindings) || sourceExpression;
        }

        /** Ask native code to reconcile authored, evaluated and visible binding values. */
        public bindingsWithAuthoredValues(
            bindings: WorksheetBinding[], sourceExpression: string, visibleBindings: WorksheetBinding[] = []
        ): WorksheetBinding[] {
            return this.host.dom.call(
                'lab_binding_authored', bindings, this.compactExpressionForEditor(sourceExpression).bindings, visibleBindings) as WorksheetBinding[];
        }

        /** Request a native change to the named binding's kind. */
        public async replaceBindingKindInExpression(sourceExpression: string, targetName: string, nextKind: string): Promise<string> {
            return this.host.flow(BindingFlow.ReplaceKind, {source: sourceExpression, name: targetName, nextKind}) as Promise<string>;
        }

        /** Obtain the native display label for a binding name. */
        public bindingDisplayName(name: string): string {
            return this.host.dom.call('lab_binding_label', [name]) as string;
        }
    }

    /** Owns rendered binding controls, their value caches and delegated card actions. */
    export class WorksheetBindingPanel {
        /** Retain narrow host capabilities; DOM and request services are resolved after startup. */
        public constructor(private readonly host: PanelDependencies) {
            marsBindCallbacks(this, [
                'visibleBindingValues', 'clearVariableValues', 'renderVariableValues', 'dispose'
            ]);
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
        public readonly actions = this.createActions();

        private createActions() {
            const owner = this;
            return {
                /** @brief Evaluate the selected binding card. */
                evaluate({card}: LabBindingCardPayload): Promise<unknown> {
                    return owner.host.flow(BindingFlow.CardAction, {card, operation: 0});
                },

                /** @brief Queue a changed binding input. */
                commit({card}: LabBindingCardPayload): Promise<unknown> {
                    return owner.host.bindingController.queueBindingInputCommit(card.input);
                },

                /** @brief Refresh history after a native card edit. */
                history(): void { owner.host.workspacePresentation.updateHistoryButtons(); },

                /** @brief Clear the selected card through native policy. */
                clear({card}: LabBindingCardPayload): Promise<void> {
                    return owner.host.flow(BindingFlow.CardAction, {card, operation: 1}) as Promise<void>;
                },

                /** @brief Copy the selected card through native policy. */
                copy({card}: LabBindingCardPayload): Promise<void> {
                    return owner.host.flow(BindingFlow.CardAction, {card, operation: 2}) as Promise<void>;
                },

                /** @brief Toggle the native binding kind. */
                toggle({card}: LabBindingCardPayload): Promise<void> {
                    return owner.host.bindingController.toggleBindingKind(card.binding);
                }
            } satisfies LabBindingCardHandlers;
        }

        /** Read binding values from the currently rendered controls. */
        public visibleBindingValues(): WorksheetBinding[] {
            return this.host.dom.call('lab_binding_visible', this.host.container) as WorksheetBinding[];
        }

        /** Clear rendered binding controls and their cached kinds. */
        public clearVariableValues(): void {
            this.host.dom.call('lab_binding_clear', this.host.container);
            this.currentBindingKinds = new Map();
        }

        /** Render binding controls and refresh their value and kind caches. */
        public renderVariableValues(bindings: WorksheetBinding[]): void {
            this.router.install(this.host.container, 'lab_binding_event', this.actions);
            const rendered = this.host.dom.call('lab_binding_render', this.host.container, bindings) as WorksheetBindingRendered;
            this.bindingValueCache = new Map(rendered.values);
            this.currentBindingKinds = new Map(rendered.kinds);
        }
    }

    /** Owns integrator control rendering and delegated edits, separate from native row metadata. */
    export class WorksheetIntegratorPanel {
        /** Retain narrow host capabilities; DOM and request services are resolved after startup. */
        public constructor(private readonly host: IntegratorPanelDependencies) {
            marsBindCallbacks(this, [
                'editIntegratorRow', 'prepareIntegratorBindingInput', 'renderIntegratorRows',
                'applyIntegratorBindingState', 'dispose'
            ]);
            this.router = new WorksheetBindingEventRouter(host.dom);
        }

        private readonly router: WorksheetBindingEventRouter;

        /** Release only this panel's delegated event subscriptions. */
        public dispose(): void { this.router.dispose(); }

        private readonly actions = this.createActions();

        private createActions(): LabBindingIntegratorHandlers {
            const owner = this;
            return {
                /** @brief Invalidate metadata and validation feedback after a row edit. */
                invalidate({card}): void {
                    owner.host.forms.nextRevision();
                    owner.host.forms.metadata = null;
                    card.input.setCustomValidity('');
                },

                /** @brief Prepare a changed integrator input. */
                prepare({card}): Promise<void> { return owner.prepareIntegratorBindingInput(card.input); },

                /** @brief Apply the native-selected row operation. */
                edit({item, index, operation}): Promise<void> { return owner.editIntegratorRow(item, index, operation); }
            };
        }

        /** Apply a native integrator row operation through its continuation. */
        public async editIntegratorRow(item: HTMLElement, index: number, operation: number): Promise<void> {
            return this.host.flow(BindingFlow.EditIntegratorRow, {item, index, operation}) as Promise<void>;
        }

        /** Prepare an integrator input using native validation and binding rules. */
        public async prepareIntegratorBindingInput(input: HTMLInputElement): Promise<void> {
            return this.host.flow(BindingFlow.PrepareIntegratorInput, {input}) as Promise<void>;
        }

        /** Render native-prepared integrator rows and install delegated input handling. */
        public renderIntegratorRows(rows: WorksheetIntegratorRow[]): void {
            this.host.forms.nextRevision();
            const safeRows = this.host.dom.call('lab_binding_rows_prepare', rows) as WorksheetIntegratorRow[];
            this.router.install(this.host.container, 'lab_binding_integrator_event', this.actions);
            this.host.dom.call('lab_binding_integrator_render', this.host.container, safeRows);
        }

        /** Apply native-validated integrator binding state and its editor effects. */
        public applyIntegratorBindingState(data: unknown, fallbackExpression?: string): void {
            const host = this.host;
            this.host.dom.services(
                this.host.dom.call(
                    'lab_binding_integrator_state', data, fallbackExpression, this.host.view.caches.editors,
                    this.host.workspaceIntegrator.currentIntegratorBoundNames()) as NativeServicePlan,
                {
                    setExpressionEditor: this.host.bindingController.setExpressionEditor.bind(this.host.bindingController),
                    clearVariableValues: this.host.bindingPanel.clearVariableValues.bind(this.host.bindingPanel),
                    renderVariableValues: this.host.bindingPanel.renderVariableValues.bind(this.host.bindingPanel),
                    /** @brief Retain the native integrator expression in its mode editor. */
                    setIntegratorBindingExpression(expression: string): string {
                        return host.modeEditors.integrator = expression;
                    }
                });
        }
    }

    /** Coordinates goal-source preparation without retaining a second native source model. */
    export class WorksheetGoalController {
        /** Retain narrow host capabilities; DOM and request services are resolved after startup. */
        public constructor(private readonly host: GoalDependencies) {
            marsBindCallbacks(this, [
                'canGoalSeek', 'clearGoalSeekRequest', 'currentGoalSeekSource', 'solvedStartValuesForGoalSeek',
                'goalSeekExpressionAndStarts'
            ]);
        }

        /** Report whether the current editor exposes variables for goal seeking. */
        public canGoalSeek(): boolean {
            return this.host.bindingPanel.currentVariables.length > 0;
        }

        /** Apply the native plan for clearing the goal-seek source. */
        public clearGoalSeekRequest(): void {
            const plan = this.host.dom.call('lab_binding_projection_clear', 1) as NativeServicePlan;
            this.host.dom.services(plan, this.host.bindingController.labBindingFlowServices());
        }

        /** Resolve the goal-seek source in the native workspace and read its text. */
        public currentGoalSeekSource(): string {
            const length = this.host.wire.exports().lab_workspace_source_resolve(this.host.buffer.stageText(this.host.input.value.trim()), 1);
            return this.host.buffer.readText(length);
        }

        /** Resolve goal-seek starting values using native precedence rules. */
        public solvedStartValuesForGoalSeek(
            sourceExpression: string, solvedExpression: string, providedStart: Record<string, string> = {}
        ): Record<string, string> {
            const sourceMetadata = this.host.bindingEditor.labEditorData(sourceExpression);
            const solvedMetadata = this.host.bindingEditor.labEditorData(solvedExpression);
            return this.host.dom.call(
                'lab_binding_goal_starts', sourceMetadata, solvedMetadata, providedStart,
                this.host.bindingPanel.bindingValueCache) as Record<string, string>;
        }

        /** Prepare the native goal-seek expression and starting-value map. */
        public async goalSeekExpressionAndStarts(sourceExpression: string, providedStart: Record<string, string> = {}): Promise<LabBindingGoalStart> {
            return this.host.flow(BindingFlow.GoalStarts, {source: sourceExpression, providedStart}) as Promise<LabBindingGoalStart>;
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

    /** Return editor text, retaining the empty-text fallback. */
    export function expressionTextOrEmpty(text: string): string {
        return String(text || '');
    }

}
