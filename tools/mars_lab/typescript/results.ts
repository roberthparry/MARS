/**
 * @file results.ts
 * @brief Native result presentation capabilities and browser feedback lifecycles.
 *
 * WorksheetResults exposes card actions and deferred native service capabilities. Instance
 * controllers own animation frames, tooltip references and clipboard feedback timers; private
 * caches retain native presentation metadata. The native evaluation view supplies those
 * caches to authorised consumers without recreating mathematical presentation in TypeScript.
 * WorksheetRequests drives continuations, WorksheetLocations renders calendar content, and
 * the workspace presenter owns result metadata. This classic script defers workspace access
 * until bootstrap has loaded every definition module. Each resource owner exposes disposal;
 * disposal never shuts down unrelated request or application owners. Callers address
 * controller instances directly; native service tables retain their protocol names.
 */

namespace WorksheetResults {
    /** Own the active tooltip node; native code retains presentation policy. */
    export class WorksheetTooltipController {
        private activeButton: HTMLElement | null = null;

        /** @brief Inject native projection and live viewport measurement. */
        constructor(
            private readonly native: () => Pick<typeof labDOM, 'call'>,
            private readonly viewport: () => {width: number; height: number}
        ) {
            this.show = this.show.bind(this);
            this.hide = this.hide.bind(this);
            this.dispose = this.dispose.bind(this);
        }

        /** @brief Return the active node without transferring its ownership. */
        get active(): HTMLElement | null { return this.activeButton; }

        /** @brief Show a valid candidate without replacing ownership when native code rejects it. */
        show(button: WorksheetCopyButton): void {
            const {width, height} = this.viewport();
            if (this.native().call<number>('lab_tooltip_show', button, this.activeButton, width, height))
                this.activeButton = button;
        }

        /** @brief Hide native presentation and release the active node. */
        hide(): void {
            this.native().call<void>('lab_tooltip_hide', this.activeButton);
            this.activeButton = null;
        }

        /** @brief Release this owner's active tooltip during shutdown. */
        dispose(): void {
            if (this.activeButton) this.hide();
        }
    }
    /** @brief Page-lifetime tooltip owner; consumers may inspect but cannot replace its active node. */
    export const tooltip: WorksheetTooltipController = new WorksheetTooltipController(() => labDOM, () => ({width: window.innerWidth, height: window.innerHeight}));

    // DOM capability adapters only; C owns layout and native card state.

    /** Own one replaceable frame and release it before invoking client code. */
    export class WorksheetFitScheduler {
        private frame = 0;

        /** @brief Inject frame scheduling so browser lifetime and test scheduling remain explicit. */
        constructor(
            private readonly request: (callback: FrameRequestCallback) => number,
            private readonly cancel: (frame: number) => void,
            private readonly perform: () => void
        ) {
            this.schedule = this.schedule.bind(this);
            this.complete = this.complete.bind(this);
            this.fit = this.fit.bind(this);
            this.dispose = this.dispose.bind(this);
        }

        /** @brief Replace pending work; detached callbacks never retain a completed frame. */
        schedule(callback: FrameRequestCallback = () => this.fit()): void {
            this.dispose();
            this.frame = this.request(time => {
                this.frame = 0;
                callback(time);
            });
        }

        /** @brief Cancel a queued fit when fitting has already completed synchronously. */
        complete(): void { this.dispose(); }

        /** @brief Complete the owned frame and perform the injected native fitting operation. */
        fit(): void {
            this.complete();
            this.perform();
        }

        /** @brief Cancel pending work when the result view is released. */
        dispose(): void {
            this.cancel(this.frame);
            this.frame = 0;
        }
    }
    /** @brief Owner of pending rendered-result fitting. */
    export const renderedFit: WorksheetFitScheduler = new WorksheetFitScheduler(callback => requestAnimationFrame(callback), frame => cancelAnimationFrame(frame), () => labDOM.call<void>('lab_layout_fit'));
    /** @brief Owner of pending solver fitting, independent of rendered-result work. */
    export const solverFit: WorksheetFitScheduler = new WorksheetFitScheduler(callback => requestAnimationFrame(callback), frame => cancelAnimationFrame(frame), () => { void WorksheetRequests.solver.fitSolverTexToCard(); });

    /** Retains native presentations as a single browser-owned cache lifetime. */
    export class WorksheetPresentationCaches {
        /** @brief Retain the native metadata installer without accessing it before bootstrap. */
        constructor(private readonly native: () => Pick<typeof labDOM, 'call'>) {
            this.install = this.install.bind(this);
            this.installSyntax = this.installSyntax.bind(this);
            this.dispose = this.dispose.bind(this);
        }

        readonly numeric = new Map<string, boolean>();
        readonly solver = new Map<string, string>();
        readonly compact = new Map<string, string>();
        readonly editors = new Map<string, WorksheetEditorMetadata>();
        readonly expansions = new Map<string, Map<string, boolean>>();
        readonly functions = new Map<string, unknown>();
        readonly headings = new Map<string, unknown>();

        /** @brief Install opaque native presentation metadata in the owned maps. */
        install(data: unknown): void {
            this.native().call<void>('lab_result_presentation_install', this, data);
        }

        /** @brief Install native function syntax without browser parsing. */
        installSyntax(data: unknown): void {
            this.native().call<void>('lab_result_syntax_install', this, data);
        }

        /** @brief Release only this owner's metadata maps. */
        dispose(): void {
            this.numeric.clear();
            this.solver.clear();
            this.compact.clear();
            this.editors.clear();
            this.expansions.clear();
            this.functions.clear();
            this.headings.clear();
        }
    }
    /** @brief Native presentation metadata with an explicit page lifetime. */
    export const caches: WorksheetPresentationCaches = new WorksheetPresentationCaches(() => labDOM);
    const labResultCaches = caches;

    // Called by the transport bridge before exposing decoded native responses.

    // These maps retain native presentations only; the browser does not classify or format their text.

    interface ClipboardDependencies {
        native: () => Pick<typeof labDOM, 'call'>;
        document: () => Pick<Document, 'createElement' | 'body' | 'execCommand'>;
        clipboard: () => Pick<Clipboard, 'writeText'> | undefined;
        secure: () => boolean;
        schedule: (callback: () => void, delay: number) => number;
        cancel: (timer: number) => void;
        surface: () => string | null;
        lastTeX: () => string;
    }

    /** Browser clipboard resources and owned, replaceable button feedback timers. */
    export class WorksheetClipboardController {
        private readonly timers = new Map<WorksheetCopyButton, number>();

        /** @brief Retain browser capabilities and exact native copy-source providers. */
        constructor(private readonly dependencies: ClipboardDependencies) {
            this.copyTextForTarget = this.copyTextForTarget.bind(this);
            this.write = this.write.bind(this);
            this.flash = this.flash.bind(this);
            this.dispose = this.dispose.bind(this);
        }

        /** @brief Export the current surface when requested, otherwise use the native card source. */
        copyTextForTarget(target: string | undefined): string {
            if (target === 'plot') {
                const surface = this.dependencies.surface();
                if (surface) return surface;
            }
            return this.dependencies.native().call<string>('lab_result_copy', target, this.dependencies.lastTeX());
        }

        /** @brief Write text through the secure clipboard or a temporary browser selection. */
        async write(text: string): Promise<void> {
            const clipboard = this.dependencies.clipboard();
            if (clipboard && this.dependencies.secure()) {
                await clipboard.writeText(text);
                return;
            }

            const document = this.dependencies.document();
            const area = document.createElement('textarea');
            area.value = text;
            area.setAttribute('readonly', '');
            area.style.position = 'fixed';
            area.style.left = '-9999px';
            area.style.top = '0';
            document.body.appendChild(area);
            let ok = false;
            try {
                area.select();
                ok = document.execCommand('copy');
            } finally {
                document.body.removeChild(area);
            }
            if (!ok)
                throw new Error('Copy was blocked by the browser');
        }

        /** @brief Replace one button's feedback timer without affecting other buttons. */
        flash(button: WorksheetCopyButton, ok: boolean): void {
            this.dependencies.native().call<void>('lab_result_copy_flash', button, ok ? 1 : 2);
            const previous = this.timers.get(button);
            if (previous !== undefined) this.dependencies.cancel(previous);
            this.timers.set(button, this.dependencies.schedule(() => {
                this.timers.delete(button);
                this.dependencies.native().call<void>('lab_result_copy_flash', button, 0);
            }, 1200));
        }

        /** @brief Cancel this owner's feedback and release its retained buttons. */
        dispose(): void {
            for (const [button, timer] of this.timers) {
                this.dependencies.cancel(timer);
                this.dependencies.native().call<void>('lab_result_copy_flash', button, 0);
            }
            this.timers.clear();
        }
    }
    /** @brief Clipboard sources, browser writes and feedback for this result view. */
    export const clipboard: WorksheetClipboardController = new WorksheetClipboardController({
        native: () => labDOM, document: () => document, clipboard: () => navigator.clipboard,
        secure: () => window.isSecureContext, schedule: (callback, delay) => setTimeout(callback, delay),
        cancel: timer => clearTimeout(timer), surface: () => labSurfacePreview.exportSvg(),
        lastTeX: () => WorksheetWorkspace.presentation.state.lastTex
    });

    interface WorksheetResultResetOptions { keepBindings?: boolean; }

    // Browser-owned live state; C owns visibility selection and event service ordering.
    const labAlmanacEventState = Object.freeze({
        get visibility() {
            return WorksheetContext.worksheetSession.calendar.almanacVisibilityMode;
        },
        get worksheet() {
            return WorksheetContext.worksheetSession.calendar.almanacLastWorksheetData;
        }
    });

    // Only browser-side effects live here; C selects their arguments and execution order.
    /** @brief Provide deferred browser effects for native evaluation recovery. */
    export const labEvaluationRecovery = Object.freeze({
        clearMatrixScalar: () => {
            WorksheetWorkspace.presentation.state.lastMatrixScalarExpression = '';
        },
        setRenderedError: (text: string) => WorksheetResults.cards.setRenderedError(text),
        resetRenderedDigits: () => WorksheetResults.cards.resetMoreDigitsButton(WorksheetContext.renderedMore, false),
        setDatetimeLocalText: (text: string) => WorksheetResults.calendarResults.setDatetimeLocalText(text),
        clearResultDetails: (options: WorksheetResultResetOptions) => WorksheetResults.reset.clearResultDetails(options),
        clearRenderedError: () => WorksheetResults.cards.clearRenderedError(),
        applyIntegratorBindingState: (data: WorksheetEvaluationData, text: string) => WorksheetBindings.integratorPanel.applyIntegratorBindingState(data, text),
        applyIntegratorResultBound: (data: WorksheetEvaluationData) => WorksheetWorkspace.integrator.applyIntegratorResultBound(data),
        saveWorksheetState: (mode: WorksheetMode) => WorksheetPersistence.persistence.saveWorksheet(mode)
    });

    // Host mutations are deliberately deferred: native plans never redefine live workspace getters.
    /** @brief Provide deferred installation effects while C retains evaluation policy. */
    export const labEvaluationInstall = Object.freeze({
        setRenderedResult: (data: WorksheetEvaluationData) => WorksheetResults.cards.setRenderedResult(data),
        setExpressionEditor: (...args: Parameters<typeof WorksheetBindings.controller.setExpressionEditor>) => WorksheetBindings.controller.setExpressionEditor(...args),
        renderVariableValues: (bindings: WorksheetBinding[]) => WorksheetBindings.panel.renderVariableValues(bindings),
        clearVariableValues: () => WorksheetBindings.panel.clearVariableValues(),
        installEvaluationTextCards: (cards: WorksheetTextCards) => WorksheetResults.cards.installEvaluationTextCards(cards),
        present: (mode: WorksheetMode, data: WorksheetEvaluationData) => labDOM.call<unknown>('lab_evaluation_cards_present', mode, data),
        render: (mode: WorksheetMode, data: WorksheetEvaluationData, expandable: unknown) => WorksheetWorkspace.presentation.state.lastTex = labDOM.call<string>('lab_evaluation_render', mode, data, Number(expandable)),
        source: (key: WorksheetSourceKey, text: string) => WorksheetContext.labEditorState[key] = text,
        modeSource: (mode: WorksheetMode, text: string) => WorksheetContext.modeEditorText[mode] = text,
        derivative: (text: string) => WorksheetWorkspace.presentation.state.lastDerivativeExpression = text,
        scalar: (text: string) => WorksheetWorkspace.presentation.state.lastMatrixScalarExpression = text,
        variables: (variables: string[], differentiable: boolean) => {
            WorksheetBindings.panel.currentVariables = variables;
            WorksheetBindings.panel.currentDifferentiable = differentiable;
            WorksheetWorkspace.presentation.renderDerivativeButtons(WorksheetBindings.panel.currentVariables);
        },
        saveWorksheetState: (...args: Parameters<typeof WorksheetPersistence.persistence.saveWorksheet>) => WorksheetPersistence.persistence.saveWorksheet(...args),
        displayMatrixResult: (data: WorksheetEvaluationData) => WorksheetResults.cards.displayMatrixResult(data),
        scheduleRenderedTeXFit: () => WorksheetResults.renderedFit.schedule(),
        solverTextCards: (cards: WorksheetTextCards) => {
            WorksheetResults.cards.setExpandableText(WorksheetContext.parsed, WorksheetContext.parsedMore, cards.expression, cards.full_expression);
            WorksheetResults.reuse.setResultInputText(cards.input);
            WorksheetResults.cards.setExpandableText(WorksheetContext.functionStyle, WorksheetContext.functionMore, cards.function, cards.full_function);
        },
        setValueText: (value: unknown) => WorksheetResults.cards.setValueText(value),
        setValueCardVisible: (visible: boolean) => WorksheetWorkspace.presentation.setValueCardVisible(visible),
        applyIntegratorBindingState: (data: WorksheetEvaluationData, text: string) => WorksheetBindings.integratorPanel.applyIntegratorBindingState(data, text),
        applyIntegratorResultBound: (data: WorksheetEvaluationData) => WorksheetWorkspace.integrator.applyIntegratorResultBound(data),
        calendarCard: (card: WorksheetCalendarCard) => WorksheetResults.calendarResults.renderDatetimeSections(card.element, card.button, card.sections, card.text),
        setDatetimeLocalText: (...args: Parameters<typeof WorksheetResults.calendarResults.setDatetimeLocalText>) => WorksheetResults.calendarResults.setDatetimeLocalText(...args),
        applyCalendarEvaluationFields: (mode: WorksheetMode, data: WorksheetEvaluationData) => WorksheetLocations.calendar.applyCalendarEvaluationFields(mode, data),
        setResultInputText: (text: string) => WorksheetResults.reuse.setResultInputText(text),
        saveLastDatetimeState: () => WorksheetPersistence.persistence.saveCalendar('datetime'),
        saveLastAlmanacState: () => WorksheetPersistence.persistence.saveCalendar('almanac'),
        almanacRender: (data: WorksheetEvaluationData) => WorksheetResults.calendarResults.renderAlmanacWorksheet(WorksheetContext.rendered, data),
        almanacAccept: (data: WorksheetEvaluationData) => WorksheetContext.worksheetSession.calendar.almanacLastWorksheetData = data,
        refreshAlmanacLandTotality: (data: WorksheetEvaluationData) => WorksheetRequests.calendarRequests.refreshAlmanacLandTotality(data)
    });

    // Getters use direct native exports only; they must not open another DOM scope.
    /** @brief Expose live native evaluation capabilities without opening nested DOM scopes. */
    export const labEvaluationView = Object.freeze({
        get caches() {
            return labResultCaches;
        },
        get editors() {
            return labResultCaches.editors;
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
        setStatus: (status: string) => WorksheetWorkspace.presentation.setStatus(status),
        installWeather: (overview: unknown, data: WorksheetEvaluationData) => {
            const cards = labDOM.call<{sections: unknown; text: string}>('lab_evaluation_weather_cards', overview, data);
            WorksheetResults.calendarResults.renderDatetimeSections(WorksheetContext.rendered, null, cards.sections, cards.text);
        }
    });

    /** @brief Provide browser mutations for native goal-seeking continuations. */
    export const labGoalServices = Object.freeze({
        setRenderedError: (message: string) => WorksheetResults.cards.setRenderedError(message),
        resetRenderedDigits: () => WorksheetResults.cards.resetMoreDigitsButton(WorksheetContext.renderedMore, false),
        clearResultDetails: (options: WorksheetResultResetOptions) => WorksheetResults.reset.clearResultDetails(options),
        setStatus: (status: string) => WorksheetWorkspace.presentation.setStatus(status),
        captureHistory: () => WorksheetWorkspace.history.pushExpressionHistory(WorksheetBindings.editor.currentExpressionText()),
        setRenderedResult: (data: WorksheetEvaluationData) => WorksheetResults.cards.setRenderedResult(data),
        setEditor: (data: WorksheetEvaluationData, context: WorksheetGoalContext) => WorksheetBindings.controller.setExpressionEditor(
            context.editorExpression, data.binding_values || null, context.editorBody, data.evaluation_ready),
        installCards: (context: WorksheetGoalContext) => WorksheetResults.cards.installEvaluationTextCards(context.cards),
        setLastInput: (context: WorksheetGoalContext) => {
            WorksheetContext.labEditorState.lastInput = context.editorExpression;
        },
        clearDerivative: () => {
            WorksheetWorkspace.presentation.state.lastDerivativeExpression = '';
        },
        setVariables: (data: WorksheetEvaluationData) => {
            WorksheetBindings.panel.currentVariables = WorksheetWorkspace.bindingSelection.variableNamesFromBindings(data.binding_values || []);
        },
        setDifferentiable: (context: WorksheetGoalContext) => {
            WorksheetBindings.panel.currentDifferentiable = context.cards.differentiable;
        },
        renderDerivatives: () => WorksheetWorkspace.presentation.renderDerivativeButtons(WorksheetBindings.panel.currentVariables),
        setSource: (context: WorksheetGoalContext) => {
            WorksheetContext.labEditorState.goalSource = WorksheetBindings.expression_text_or_empty(context.seek.expression).trim();
        },
        setTarget: (context: WorksheetGoalContext) => {
            WorksheetContext.labEditorState.goalTarget = context.target;
        },
        hideTargetEntry: () => WorksheetWorkspace.presentation.hideTargetEntry(),
        setSuccessStatus: (context: WorksheetGoalContext) => WorksheetWorkspace.presentation.setStatus(labDOM.call<string>('lab_goal_status', context.cards))
    });

    /** @brief Native result protocol table with late-bound owner calls and unchanged service keys. */
    export const resultServices = {
            resultInput: () => WorksheetResults.reuse.resultExpressionTextForInput(),
            resultSnapshot: () => ({mode: WorksheetWorkspace.mode.currentMode(), editor: WorksheetContext.expr.value, context: WorksheetRequests.labRequests.context()}),
            resultPrepare: (text: string) => WorksheetRequests.presentation.prepareLabEditor(text),
            resultHistoryEqual: (left: WorksheetSnapshot, right: WorksheetSnapshot) => WorksheetPersistence.history.equals(left, right),
            resultClearGoal: () => {
                WorksheetBindings.goal.clearGoalSeekRequest();
                WorksheetWorkspace.presentation.hideTargetEntry();
            },
            resultSourceBindings: () => WorksheetBindings.editor.compactExpressionForEditor(WorksheetBindings.editor.currentExpressionText()),
            resultBindings: () => WorksheetWorkspace.presentation.state.resultInputBindings,
            resultAuthoredBindings: (...args: Parameters<typeof WorksheetBindings.editor.bindingsWithAuthoredValues>) => WorksheetBindings.editor.bindingsWithAuthoredValues(...args),
            resultApplyBinding: (...args: Parameters<typeof WorksheetBindings.controller.applyMarsBindingExpression>) => WorksheetBindings.controller.applyMarsBindingExpression(...args),
            resultMatrixControls: () => {
                WorksheetContext.matrixOperation.value = 'eval';
                WorksheetContext.matrixOperand.value = '';
                WorksheetLocations.selects.sync(WorksheetContext.matrixOperation);
                WorksheetWorkspace.presentation.syncMatrixControls();
            },
            resultSaveEditor: () => WorksheetWorkspace.mode.saveCurrentModeEditorState(),
            resultPresentation: (payload: unknown) => WorksheetRequests.presentation.requestLabPresentation(payload),
            resultLastTeX: () => WorksheetWorkspace.presentation.state.lastTex,
            resultRenderTeX: (TeX: string) => WorksheetResults.actions.renderTexSvg(TeX),
            resultCopyText: (button: WorksheetCopyButton) => WorksheetResults.clipboard.copyTextForTarget(button.dataset.copyTarget),
            resultDatetimeSummary: () => WorksheetLocations.calendar.datetimeSummaryText(),
            resultClipboard: (text: string) => WorksheetResults.clipboard.write(text),
            resultFlash: (...args: Parameters<typeof WorksheetResults.clipboard.flash>) => WorksheetResults.clipboard.flash(...args),
            resultStatusLater: (text: string, delay: number) => actions.statusLater(text, delay),
            resultFetch: (url: string, payload: unknown) => labHttp.fetch(url, {method: 'POST', body: labWire.encode(payload)}),
            resultDecode: (response: WorksheetResponse) => response.labData(),
            resultStartupSnapshot: () => ({context: WorksheetRequests.labRequests.context(), main: WorksheetRequests.labRequests.latestMain()}),
            resultRestoreBounds: () => WorksheetWorkspace.integrator.restoreIntegratorBoundsText(WorksheetContext.DEFAULT_INTEGRATOR_BOUNDS_TEXT),
            resultLoadState: () => WorksheetPersistence.restoration.load(),
            resultInitialEvaluation: () => WorksheetRequests.evaluation.evaluateActiveModeOnLoad()
    };

    interface ResultCardPresenterDependencies {
        context: () => Pick<typeof WorksheetContext, 'functionStyle' | 'functionMore' | 'parsed' | 'parsedMore'>;
        readonly presentationState: Pick<WorksheetPresentationState, 'lastTex' | 'resultInputBindings'>;
        flow: () => Pick<WorksheetRequests.NativeFlowRuntime, 'continue'>;
        functionRun: () => Pick<WorksheetRequests.FunctionCardController, 'clearFunctionRun'>;
        reuse: () => Pick<ResultReuseController, 'setResultInputText'>;
        native: () => Pick<typeof labDOM, 'call'>;
        caches: WorksheetPresentationCaches;
    }

    /** @brief Native result-card projection and reusable presentation state; never interprets mathematics. */
    export class ResultCardPresenter {
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: ResultCardPresenterDependencies) {
            this.resetMoreDigitsButton = this.resetMoreDigitsButton.bind(this);
            this.clearRenderedError = this.clearRenderedError.bind(this);
            this.setRenderedError = this.setRenderedError.bind(this);
            this.setExpandableText = this.setExpandableText.bind(this);
            this.setValueText = this.setValueText.bind(this);
            this.displayMatrixResult = this.displayMatrixResult.bind(this);
            this.displayCalculusResult = this.displayCalculusResult.bind(this);
            this.setMatrixPrettyResult = this.setMatrixPrettyResult.bind(this);
            this.setRenderedResult = this.setRenderedResult.bind(this);
            this.toggleTextDigits = this.toggleTextDigits.bind(this);
            this.installEvaluationTextCards = this.installEvaluationTextCards.bind(this);
        }

        /** @brief Reset native expansion availability for one card. */
        resetMoreDigitsButton(button: WorksheetCopyButton, enabled: boolean): void {
            this.dependencies.native().call<void>('lab_layout_more', button, Number(!!enabled));
        }

        /** @brief Clear native error presentation. */
        clearRenderedError(): void {
            this.dependencies.native().call<void>('lab_layout_error', '', 0);
        }

        /** @brief Display an error as text without interpreting its contents. */
        setRenderedError(message: string): void {
            this.dependencies.native().call<void>('lab_layout_error', String(message || ''), 1);
        }

        /** @brief Project abbreviated and full native text with its expansion control. */
        setExpandableText(element: HTMLElement, button: WorksheetCopyButton, displayText: string, fullText: string) {
            if (this.dependencies.native().call<number>(
                    'lab_result_expandable', this.dependencies.caches, element, button, String(displayText || ''),
                    String(fullText || '')))
                this.dependencies.functionRun().clearFunctionRun();
        }

        /** @brief Project a native numerical value as expandable text. */
        setValueText(fullText: unknown) {
            this.dependencies.native().call<void>('lab_result_value', this.dependencies.caches, String(fullText || ''));
        }

        /** @brief Install matrix presentation and accept its native reusable bindings. */
        displayMatrixResult(data: Partial<WorksheetEvaluationData>) {
            this.dependencies.functionRun().clearFunctionRun();
            const state = this.dependencies.native().call<WorksheetDisplayResult>('lab_result_display', this.dependencies.caches, data, 1);
            this.dependencies.presentationState.lastTex = state.TeX;
            this.dependencies.presentationState.resultInputBindings = state.bindings;
        }

        /** @brief Install calculus presentation and accept its native reusable bindings. */
        displayCalculusResult(result: Partial<WorksheetEvaluationData>) {
            this.dependencies.functionRun().clearFunctionRun();
            const state = this.dependencies.native().call<WorksheetDisplayResult>('lab_result_display', this.dependencies.caches, result, 2);
            this.dependencies.presentationState.lastTex = state.TeX;
            this.dependencies.presentationState.resultInputBindings = state.bindings;
        }

        /** @brief Request native matrix presentation while preserving target ownership. */
        setMatrixPrettyResult(resultText: string, prettyText: string, element = this.dependencies.context().functionStyle, moreButton = this.dependencies.context().functionMore) {
            return this.dependencies.flow().continue(
                57, {text: String(resultText || ''), pretty: String(prettyText || ''), element, button: moreButton});
        }

        /** @brief Install rendered mathematical output and retain its exact native TeX. */
        setRenderedResult(data: Partial<WorksheetEvaluationData>) {
            this.dependencies.presentationState.lastTex = this.dependencies.native().call<string>('lab_result_rendered', this.dependencies.caches, data, 0);
        }

        /** @brief Toggle a text card's native abbreviated or complete presentation. */
        toggleTextDigits(element: HTMLElement, button: WorksheetCopyButton) {
            this.dependencies.native().call<void>('lab_result_text_digits', this.dependencies.caches, element, button);
        }

        /** @brief Install the native expression, function, input and value card texts. */
        installEvaluationTextCards(cards: WorksheetTextCards) {
            this.setExpandableText(this.dependencies.context().parsed, this.dependencies.context().parsedMore, cards.expression, cards.full_expression);
            this.dependencies.reuse().setResultInputText(cards.input);
            this.setExpandableText(this.dependencies.context().functionStyle, this.dependencies.context().functionMore, cards.function, cards.full_function);
            this.setValueText(cards.value);
        }
    }

    /** @brief Native result-card projection and reusable presentation state; never interprets mathematics. */
    export const cards: ResultCardPresenter = new ResultCardPresenter({context: () => WorksheetContext,
        get presentationState() { return WorksheetWorkspace.presentation.state; },
        flow: () => WorksheetRequests.runtime, functionRun: () => WorksheetRequests.functionCard, reuse: () => reuse, native: () => labDOM, caches: labResultCaches});

    interface ResultReuseControllerDependencies {
        readonly presentationState: Pick<WorksheetPresentationState, 'resultInputBindings'>;
        flow: () => Pick<WorksheetRequests.NativeFlowRuntime, 'continue'>;
        mode: () => Pick<WorksheetWorkspace.WorksheetModeSession, 'currentMode'>;
        native: () => Pick<typeof labDOM, 'call'>;
    }

    /** @brief Native result text and binding snapshots for transfer back into the editor. */
    export class ResultReuseController {
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: ResultReuseControllerDependencies) {
            this.setResultInputText = this.setResultInputText.bind(this);
            this.resultExpressionTextForInput = this.resultExpressionTextForInput.bind(this);
            this.sendResultExpressionToInput = this.sendResultExpressionToInput.bind(this);
        }

        /** @brief Store reusable result text and a native snapshot of its bindings. */
        setResultInputText(text: string, bindings: WorksheetBinding[] | null = null) {
            this.dependencies.native().call<void>('lab_result_input_set', String(text || ''));
            this.dependencies.presentationState.resultInputBindings = this.dependencies.native().call<WorksheetBinding[]>('lab_result_binding_snapshot', bindings);
        }

        /** @brief Read the native-selected expression for reuse in the current mode. */
        resultExpressionTextForInput() {
            return this.dependencies.native().call<string>('lab_result_input_get', this.dependencies.mode().currentMode());
        }

        /** @brief Run the native continuation that transfers a result into the editor. */
        sendResultExpressionToInput() {
            return this.dependencies.flow().continue(56, {});
        }
    }

    /** @brief Native result text and binding snapshots for transfer back into the editor. */
    export const reuse: ResultReuseController = new ResultReuseController({
        get presentationState() { return WorksheetWorkspace.presentation.state; },
        flow: () => WorksheetRequests.runtime, mode: () => WorksheetWorkspace.mode, native: () => labDOM});

    interface ResultActionControllerDependencies {
        context: () => Pick<typeof WorksheetContext, 'inputCopy'>;
        flow: () => Pick<WorksheetRequests.NativeFlowRuntime, 'continue'>;
        status: (text: string) => void;
        schedule: (callback: () => void, delay: number) => number;
        cancel: (timer: number) => void;
    }

    /** @brief Result commands delegated to native continuations rather than duplicated browser workflow. */
    export class ResultActionController {
        private readonly statusTimers = new Set<number>();
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: ResultActionControllerDependencies) {
            this.statusLater = this.statusLater.bind(this);
            this.dispose = this.dispose.bind(this);
            this.renderTexSvg = this.renderTexSvg.bind(this);
            this.toggleRenderedDigits = this.toggleRenderedDigits.bind(this);
            this.copyInputFromEvent = this.copyInputFromEvent.bind(this);
            this.copyResultFromEvent = this.copyResultFromEvent.bind(this);
        }

        /** @brief Schedule deferred status feedback and retain its timer until completion. */
        statusLater(text: string, delay: number): number {
            const timer = this.dependencies.schedule(() => {
                this.statusTimers.delete(timer);
                this.dependencies.status(text);
            }, delay);
            this.statusTimers.add(timer);
            return timer;
        }

        /** @brief Cancel only status callbacks owned by this result controller. */
        dispose(): void {
            for (const timer of this.statusTimers) this.dependencies.cancel(timer);
            this.statusTimers.clear();
        }

        /** @brief Request native SVG rendering of supplied TeX. */
        renderTexSvg(TeX: string) {
            return this.dependencies.flow().continue(61, {TeX});
        }

        /** @brief Run the native continuation for rendered-result expansion. */
        toggleRenderedDigits() {
            return this.dependencies.flow().continue(58, {});
        }

        /** @brief Copy after outstanding binding commits, using the browser clipboard. */
        copyInputFromEvent() {
            return this.dependencies.flow().continue(59, {button: this.dependencies.context().inputCopy});
        }

        /** @brief Copy an exact native card source through the browser clipboard. */
        copyResultFromEvent(button: WorksheetCopyButton) {
            return this.dependencies.flow().continue(60, {button});
        }
    }

    /** @brief Result commands delegated to native continuations rather than duplicated browser workflow. */
    export const actions: ResultActionController = new ResultActionController({context: () => WorksheetContext, flow: () => WorksheetRequests.runtime,
        status: text => WorksheetWorkspace.presentation.setStatus(text), schedule: (callback, delay) => setTimeout(callback, delay), cancel: timer => clearTimeout(timer)});

    interface ResultResetControllerDependencies {
        readonly presentationState: Pick<WorksheetPresentationState, 'lastTex' | 'resultInputBindings' | 'lastDerivativeExpression'>;
        functionRun: () => Pick<WorksheetRequests.FunctionCardController, 'clearFunctionRun'>;
        mode: () => Pick<WorksheetWorkspace.WorksheetModeSession, 'currentMode'>;
        presentation: () => Pick<WorksheetWorkspace.WorksheetPresentationController, 'renderDerivativeButtons'>;
        bindings: () => Pick<WorksheetBindings.WorksheetBindingPanel, 'clearVariableValues' | 'currentVariables' | 'currentDifferentiable'>;
        calendarResults: () => Pick<CalendarResultPresenter, 'setDatetimeLocalText'>;
        native: () => Pick<typeof labDOM, 'call' | 'services'>;
        caches: WorksheetPresentationCaches;
    }

    /** @brief Reset coordination for native cards, binding state and result-specific browser resources. */
    export class ResultResetController {
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: ResultResetControllerDependencies) {
            this.clearResultPane = this.clearResultPane.bind(this);
            this.clearResultDetails = this.clearResultDetails.bind(this);
        }

        /** @brief Clear native result cards, local-date content and reusable result details. */
        clearResultPane() {
            this.dependencies.native().call<void>('lab_result_pane_clear');
            this.dependencies.calendarResults().setDatetimeLocalText('');
            this.clearResultDetails();
        }

        /** @brief Reset native result state, optionally retaining authored bindings. */
        clearResultDetails(options: WorksheetResultResetOptions = {}): void {
            this.dependencies.functionRun().clearFunctionRun();
            const state = this.dependencies.native().call<WorksheetResultValues & NativeServicePlan>('lab_result_reset', this.dependencies.caches, this.dependencies.mode().currentMode(), options);
            const presentation = this.dependencies.presentationState;
            const bindings = this.dependencies.bindings();
            ({resultInputBindings: presentation.resultInputBindings, lastTex: presentation.lastTex,
                lastDerivativeExpression: presentation.lastDerivativeExpression,
                currentVariables: bindings.currentVariables, currentDifferentiable: bindings.currentDifferentiable} = state);
            this.dependencies.presentation().renderDerivativeButtons(bindings.currentVariables);
            this.dependencies.native().services(state, {clearVariableValues: () => bindings.clearVariableValues()});
        }
    }

    /** @brief Reset coordination for native cards, binding state and result-specific browser resources. */
    export const reset: ResultResetController = new ResultResetController({
        get presentationState() { return WorksheetWorkspace.presentation.state; },
        functionRun: () => WorksheetRequests.functionCard, mode: () => WorksheetWorkspace.mode, presentation: () => WorksheetWorkspace.presentation, bindings: () => WorksheetBindings.panel, calendarResults: () => calendarResults, native: () => labDOM, caches: labResultCaches});

    interface CalendarResultPresenterDependencies {
        context: () => Pick<typeof WorksheetContext, 'worksheetSession' | 'datetimeLocalBody' | 'datetimeLocal'>;
        native: () => Pick<typeof labDOM, 'call' | 'services'>;
        locationServices: () => NativeServiceRegistry;
        almanacState: typeof labAlmanacEventState;
    }

    /** @brief Native calendar presentation and live visibility state, with explicit missing-data failures. */
    export class CalendarResultPresenter {
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: CalendarResultPresenterDependencies) {
            this.setDatetimeLocalText = this.setDatetimeLocalText.bind(this);
            this.renderDatetimeSections = this.renderDatetimeSections.bind(this);
            this.setAlmanacVisibility = this.setAlmanacVisibility.bind(this);
            this.bindAlmanacTotalityActions = this.bindAlmanacTotalityActions.bind(this);
            this.renderAlmanacWorksheet = this.renderAlmanacWorksheet.bind(this);
        }

        /** @brief Render local-date sections and apply native card-visibility decisions. */
        setDatetimeLocalText(text: string, sections: unknown = null): void {
            const context = this.dependencies.context();
            const native = this.dependencies.native();
            native.services(native.call<NativeServicePlan>(
                'lab_flow_location_local', {value: text}, sections, context.datetimeLocalBody, context.datetimeLocal),
                this.dependencies.locationServices());
        }

        /** @brief Render native calendar sections; reject missing presentation metadata. */
        renderDatetimeSections(element: HTMLElement, button: WorksheetCopyButton | null, sections: unknown, fallbackText = '') {
            if (!this.dependencies.native().call<number>('lab_result_datetime', element, button, sections, String(fallbackText || '')))
                throw new Error(
                    'DateTime presentation is missing from the server response. ' +
                    'Run make mars-lab-restart, reload the page and evaluate again.');
        }

        /** @brief Store the native-selected almanac visibility mode. */
        setAlmanacVisibility(value: string) {
            this.dependencies.context().worksheetSession.calendar.almanacVisibilityMode = value;
        }

        /** @brief Install native action delegation beneath an optional result root. */
        bindAlmanacTotalityActions(root?: HTMLElement) {
            this.dependencies.native().call<void>('lab_almanac_events_install', root, null);
        }

        /** @brief Validate and project a native almanac presentation or report its absence. */
        renderAlmanacWorksheet(target: HTMLElement, data: unknown) {
            const visibility = this.dependencies.native().call<string | null>('lab_almanac_events_render', target, data, this.dependencies.almanacState);
            if (visibility === null)
                throw new Error(
                    'Almanac presentation is missing from the server response. ' +
                    'Run make mars-lab-restart, reload the page and evaluate again.');
            this.setAlmanacVisibility(visibility);
        }
    }

    /** @brief Native calendar presentation and live visibility state, with explicit missing-data failures. */
    export const calendarResults: CalendarResultPresenter = new CalendarResultPresenter({context: () => WorksheetContext, native: () => labDOM, almanacState: labAlmanacEventState, locationServices: () => WorksheetLocations.locationServices});

}
