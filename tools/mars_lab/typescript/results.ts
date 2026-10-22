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
 * controller instances directly. results_types.d.ts describes the borrowed capabilities;
 * results_services.ts supplies the late-bound native service tables.
 */

namespace WorksheetResults {
    /** @brief Native result workflow identifiers from the lab_flow result-action range. */
    const enum ResultFlowKind {
        reuseInput = 56,
        matrixPresentation = 57,
        renderedDigits = 58,
        copyInput = 59,
        copyResult = 60,
        renderTeX = 61
    }

    /** Own the active tooltip node; native code retains presentation policy. */
    export class WorksheetTooltipController {
        private activeButton: HTMLElement | null = null;
        private readonly selectedCountries = new Map<HTMLElement, MutationObserver>();
        private flagRequest: {code: string; controller: AbortController} | null = null;
        private countryObserver: IntersectionObserver | null = null;
        private countryMutation: MutationObserver | null = null;
        private countryShell: Element | null = null;
        private readonly visibleCountries = new Set<HTMLElement>();
        private menuRequest: AbortController | null = null;
        private readonly pendingFlags = new Map<string, {
            controller: AbortController;
            consumers: number;
            promise: Promise<{name: string; flag: string}>;
        }>();

        private async fetchFlag(code: string, signal: AbortSignal): Promise<{name: string; flag: string}> {
            if (signal.aborted) throw new DOMException('Cancelled', 'AbortError');
            const cached = this.cachedCountry(code);
            if (cached) return cached;
            let pending = this.pendingFlags.get(code);
            if (!pending) {
                const controller = new AbortController();
                const promise = (async () => {
                    const response = await labHttp.fetch('/almanac-country', {
                        method: 'POST', body: labWire.encode({country_code: code}), signal: controller.signal
                    });
                    const data = await response.labData() as {ok?: boolean; country?: string; country_flag?: string};
                    if (controller.signal.aborted || !response.ok || !data.ok) throw new Error('Flag unavailable');
                    const name = typeof data.country === 'string' ? data.country : '';
                    const flag = typeof data.country_flag === 'string' &&
                        data.country_flag.startsWith('data:image/svg+xml;base64,') ? data.country_flag : '';
                    if (!this.cacheFlag(code, name, flag)) throw new Error('Flag cache rejected metadata');
                    return {name, flag};
                })();
                pending = {controller, consumers: 0, promise};
                this.pendingFlags.set(code, pending);
            }
            const shared = pending;
            ++shared.consumers;
            return new Promise((resolve, reject) => {
                let released = false;
                const release = () => {
                    if (released) return false;
                    released = true;
                    signal.removeEventListener('abort', cancel);
                    if (--shared.consumers === 0) {
                        shared.controller.abort();
                        if (this.pendingFlags.get(code) === shared) this.pendingFlags.delete(code);
                    }
                    return true;
                };
                const cancel = () => {
                    if (release()) reject(new DOMException('Cancelled', 'AbortError'));
                };
                signal.addEventListener('abort', cancel, {once: true});
                shared.promise.then(value => {
                    if (release()) resolve(value);
                }, error => {
                    if (release()) reject(error);
                });
            });
        }

        private countryArtwork(button: HTMLElement, flag: string): void {
            if (button.dataset.countryOption !== '1' || !flag) return;
            let image = button.querySelector<HTMLImageElement>('img.country-flag');
            if (!image) {
                image = document.createElement('img');
                image.decoding = 'async';
                image.className = 'country-flag'; image.alt = '';
                button.prepend(image);
            }
            if (image.getAttribute('src') !== flag) image.src = flag;
        }

        /** @brief Read copied metadata from the bounded native dictionary. */
        cachedCountry(code: string): {name: string; flag: string} | null {
            return this.native().call('lab_flag_cache_get', code) as {name: string; flag: string} | null;
        }

        private cacheFlag(code: string, name: string, flag: string): boolean {
            if (!this.native().call('lab_flag_cache_put', code, name, flag)) return false;
            // Only two selected-country controls exist; neither requires opening or hovering its menu.
            for (const id of ['almanacJurisdiction', 'datetimeJurisdiction']) {
                const button = document.getElementById(id)?.closest('.select-shell')?.querySelector<HTMLElement>('.select-button');
                if (!button) continue;
                for (const [old, observer] of this.selectedCountries) if (!old.isConnected) {
                    observer.disconnect();
                    this.selectedCountries.delete(old);
                }
                if (!this.selectedCountries.has(button)) {
                    const observer = new MutationObserver(() => this.selectedCountryArtwork(button));
                    observer.observe(button, {childList: true, attributes: true, attributeFilter: ['data-country-code']});
                    this.selectedCountries.set(button, observer);
                }
                this.selectedCountryArtwork(button);
            }
            return true;
        }

        private selectedCountryArtwork(button: HTMLElement): void {
            const flag = this.cachedCountry(button.dataset.countryCode || '')?.flag;
            if (flag) this.countryArtwork(button, flag);
            else button.querySelector('img.country-flag')?.remove();
        }

        /** @brief Share completed globe metadata with selected-country controls and tooltip consumers. */
        rememberCountry(code: string, name: string, flag: string): void {
            this.cacheFlag(code, name, flag);
        }

        private observeCountryMenu(button: HTMLElement): void {
            if (button.dataset.countryOption !== '1') return;
            const shell = button.closest('.select-shell');
            if (!shell) return;
            if (shell === this.countryShell) { void this.fillCountryMenu(); return; }
            this.countryObserver?.disconnect();
            this.countryMutation?.disconnect();
            this.menuRequest?.abort();
            this.menuRequest = null;
            this.countryShell = shell;
            this.visibleCountries.clear();
            this.countryObserver = new IntersectionObserver(entries => {
                for (const entry of entries) {
                    const item = entry.target as HTMLElement;
                    if (entry.isIntersecting) this.visibleCountries.add(item);
                    else {
                        this.visibleCountries.delete(item);
                        item.querySelector('img.country-flag')?.remove();
                    }
                }
                if (!this.visibleCountries.size) this.menuRequest?.abort();
                void this.fillCountryMenu();
            });
            shell.querySelectorAll<HTMLElement>('[data-country-option="1"]').forEach(item => this.countryObserver!.observe(item));
            this.countryMutation = new MutationObserver(records => {
                for (const item of this.visibleCountries) {
                    if (!item.isConnected) {
                        this.visibleCountries.delete(item);
                        this.countryObserver?.unobserve(item);
                    }
                }
                if (records.some(record => (record.target as Element).classList?.contains('select-options'))) {
                    this.countryObserver?.disconnect();
                    this.visibleCountries.clear();
                    shell.querySelectorAll<HTMLElement>('[data-country-option="1"]').forEach(item => this.countryObserver!.observe(item));
                }
                void this.fillCountryMenu();
            });
            this.countryMutation.observe(shell, {childList: true, subtree: true});
        }

        private async fillCountryMenu(): Promise<void> {
            // Selection rebuilds the button's text; restore resident artwork before waiting on any other country.
            for (const item of this.visibleCountries) {
                const cached = this.cachedCountry(item.dataset.countryCode || '');
                if (cached) this.countryArtwork(item, cached.flag);
            }
            if (this.menuRequest) return;
            for (const item of this.visibleCountries) {
                const code = item.dataset.countryCode || '';
                if (!/^[A-Z]{2}(?:-[A-Z0-9]{1,3})?$/.test(code)) continue;
                if (this.cachedCountry(code)) continue;
                const request = new AbortController();
                this.menuRequest = request;
                const timer = window.setTimeout(() => request.abort(), 5000);
                try {
                    const {flag} = await this.fetchFlag(code, request.signal);
                    if (request.signal.aborted) return;
                    if (this.visibleCountries.has(item) && item.dataset.countryCode === code) this.countryArtwork(item, flag);
                } catch { return; }
                finally {
                    window.clearTimeout(timer);
                    if (this.menuRequest === request) this.menuRequest = null;
                }
            }
        }

        private loadFlag(button: WorksheetCopyButton): void {
            this.observeCountryMenu(button);
            const code = button.dataset.countryCode || '';
            if (this.flagRequest?.code !== code) {
                this.flagRequest?.controller.abort();
                this.flagRequest = null;
            }
            delete button.dataset.countryFlag;
            delete button.dataset.countryName;
            if (!/^[A-Z]{2}(?:-[A-Z0-9]{1,3})?$/.test(code)) return;
            const entry = this.cachedCountry(code);
            if (entry) {
                button.dataset.countryFlag = entry.flag;
                button.dataset.countryName = entry.name;
                this.countryArtwork(button, entry.flag);
                return;
            }
            if (this.flagRequest) return;
            const request = {code, controller: new AbortController()};
            this.flagRequest = request;
            const timer = window.setTimeout(() => request.controller.abort(), 5000);
            void (async () => {
                try {
                    const {name, flag} = await this.fetchFlag(code, request.controller.signal);
                    if (request.controller.signal.aborted || this.flagRequest !== request) return;
                    const active = this.activeButton;
                    if (active?.isConnected && active.dataset.countryCode === code) {
                        active.dataset.countryFlag = flag;
                        active.dataset.countryName = name;
                        this.countryArtwork(active, flag);
                        const {width, height} = this.viewport();
                        this.native().call('lab_tooltip_show', active, active, width, height);
                    }
                } catch {
                    // Country artwork is optional; retain the immediately available textual tooltip.
                } finally {
                    window.clearTimeout(timer);
                    if (this.flagRequest === request) this.flagRequest = null;
                }
            })();
        }

        /** @brief Inject native projection and live viewport measurement. */
        constructor(
            private readonly native: () => ResultNative,
            private readonly viewport: () => {width: number; height: number}
        ) {
            marsBindCallbacks(this, [
                'show', 'hide', 'dispose'
            ]);
        }

        /** @brief Return the active node without transferring its ownership. */
        get active(): HTMLElement | null { return this.activeButton; }

        /** @brief Show a valid candidate without replacing ownership when native code rejects it. */
        show(button: WorksheetCopyButton): void {
            if (button) {
                if (this.activeButton && this.activeButton !== button) {
                    delete this.activeButton.dataset.countryFlag;
                    delete this.activeButton.dataset.countryName;
                }
                this.loadFlag(button);
            }
            const {width, height} = this.viewport();
            if (this.native().call('lab_tooltip_show', button, this.activeButton, width, height))
                this.activeButton = button;
        }

        /** @brief Hide native presentation and release the active node. */
        hide(): void {
            this.flagRequest?.controller.abort();
            this.flagRequest = null;
            if (this.activeButton) {
                delete this.activeButton.dataset.countryFlag;
                delete this.activeButton.dataset.countryName;
            }
            this.native().call('lab_tooltip_hide', this.activeButton);
            this.activeButton = null;
        }

        /** @brief Release this owner's active tooltip during shutdown. */
        dispose(): void {
            for (const observer of this.selectedCountries.values()) observer.disconnect();
            this.selectedCountries.clear();
            this.countryObserver?.disconnect();
            this.countryObserver = null;
            this.countryMutation?.disconnect();
            this.countryMutation = null;
            this.countryShell = null;
            this.visibleCountries.clear();
            this.menuRequest?.abort();
            if (this.activeButton) this.hide();
            this.flagRequest?.controller.abort();
            this.flagRequest = null;
            this.native().call('lab_flag_cache_clear');
            for (const pending of this.pendingFlags.values()) pending.controller.abort();
            this.pendingFlags.clear();
        }
    }

    // DOM capability adapters only; C owns layout and native card state.

    /** Own one replaceable frame and release it before invoking client code. */
    export class WorksheetFitScheduler {
        private frame = 0;
        private callback: FrameRequestCallback | null = null;

        /** @brief Inject frame scheduling so browser lifetime and test scheduling remain explicit. */
        constructor(
            private readonly request: (callback: FrameRequestCallback) => number,
            private readonly cancel: (frame: number) => void,
            private readonly perform: () => void
        ) {
            marsBindCallbacks(this, [
                'schedule', 'complete', 'fit', 'dispose'
            ]);
            this.runFrame = this.runFrame.bind(this);
        }

        /** @brief Replace pending work; detached callbacks never retain a completed frame. */
        schedule(callback: FrameRequestCallback = this.fit): void {
            this.dispose();
            this.callback = callback;
            this.frame = this.request(this.runFrame);
        }

        private runFrame(time: number): void {
            const callback = this.callback;
            this.frame = 0;
            this.callback = null;
            if (callback) callback(time);
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
            this.callback = null;
        }
    }

    /** Retains native presentations as a single browser-owned cache lifetime. */
    export class WorksheetPresentationCaches {
        /** @brief Retain the native metadata installer without accessing it before bootstrap. */
        constructor(private readonly native: () => ResultNative) {
            marsBindCallbacks(this, [
                'install', 'installSyntax', 'dispose'
            ]);
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
            this.native().call('lab_result_presentation_install', this, data);
        }

        /** @brief Install native function syntax without browser parsing. */
        installSyntax(data: unknown): void {
            this.native().call('lab_result_syntax_install', this, data);
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

    /** Browser clipboard resources and owned, replaceable button feedback timers. */
    export class WorksheetClipboardController {
        private readonly timers = new Map<WorksheetCopyButton, number>();

        /** @brief Retain browser capabilities and exact native copy-source providers. */
        constructor(private readonly dependencies: ClipboardDependencies) {
            marsBindCallbacks(this, [
                'copyTextForTarget', 'write', 'flash', 'dispose'
            ]);
        }

        /** @brief Export the current surface when requested, otherwise use the native card source. */
        copyTextForTarget(target: string | undefined): string {
            if (target === 'plot') {
                const surface = this.dependencies.surface();
                if (surface) return surface;
            }
            return this.dependencies.native().call('lab_result_copy', target, this.dependencies.lastTeX()) as string;
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
            this.dependencies.native().call('lab_result_copy_flash', button, ok ? 1 : 2);
            const previous = this.timers.get(button);
            if (previous !== undefined) this.dependencies.cancel(previous);
            this.timers.set(button, this.dependencies.schedule(this.finishFlash.bind(this, button), 1200));
        }

        private finishFlash(button: WorksheetCopyButton): void {
            this.timers.delete(button);
            this.dependencies.native().call('lab_result_copy_flash', button, 0);
        }

        /** @brief Cancel this owner's feedback and release its retained buttons. */
        dispose(): void {
            for (const [button, timer] of this.timers) {
                this.dependencies.cancel(timer);
                this.dependencies.native().call('lab_result_copy_flash', button, 0);
            }
            this.timers.clear();
        }
    }

    // Browser-owned live state; C owns visibility selection and event service ordering.
    const labAlmanacEventState = Object.freeze({
        get visibility() {
            return WorksheetContext.worksheetSession.calendar.almanacVisibilityMode;
        },
        get worksheet() {
            return WorksheetContext.worksheetSession.calendar.almanacLastWorksheetData;
        }
    });

    /** @brief Native result-card projection and reusable presentation state; never interprets mathematics. */
    export class ResultCardPresenter {
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: ResultCardPresenterDependencies) {
            marsBindCallbacks(this, [
                'resetMoreDigitsButton', 'clearRenderedError', 'setRenderedError', 'setExpandableText',
                'setValueText', 'displayMatrixResult', 'displayCalculusResult', 'setMatrixPrettyResult',
                'setRenderedResult', 'toggleTextDigits', 'installEvaluationTextCards'
            ]);
        }

        /** @brief Reset native expansion availability for one card. */
        resetMoreDigitsButton(button: WorksheetCopyButton, enabled: boolean): void {
            this.dependencies.native().call('lab_layout_more', button, Number(!!enabled));
        }

        /** @brief Clear native error presentation. */
        clearRenderedError(): void {
            this.dependencies.native().call('lab_layout_error', '', 0);
        }

        /** @brief Display an error as text without interpreting its contents. */
        setRenderedError(message: string): void {
            this.dependencies.native().call('lab_layout_error', String(message || ''), 1);
        }

        /** @brief Project abbreviated and full native text with its expansion control. */
        setExpandableText(element: HTMLElement, button: WorksheetCopyButton, displayText: string, fullText: string): void {
            if (this.dependencies.native().call(
                    'lab_result_expandable', this.dependencies.caches, element, button, String(displayText || ''),
                    String(fullText || '')))
                this.dependencies.functionRun().clearFunctionRun();
        }

        /** @brief Project a native numerical value as expandable text. */
        setValueText(fullText: unknown): void {
            this.dependencies.native().call('lab_result_value', this.dependencies.caches, String(fullText || ''));
        }

        /** @brief Install matrix presentation and accept its native reusable bindings. */
        displayMatrixResult(data: ResultData): void {
            this.dependencies.functionRun().clearFunctionRun();
            const state = this.dependencies.native().call(
                'lab_result_display', this.dependencies.caches, data, 1) as WorksheetDisplayResult;
            this.dependencies.presentationState.lastTex = state.TeX;
            this.dependencies.presentationState.resultInputBindings = state.bindings;
        }

        /** @brief Install calculus presentation and accept its native reusable bindings. */
        displayCalculusResult(result: ResultData): void {
            this.dependencies.functionRun().clearFunctionRun();
            const state = this.dependencies.native().call(
                'lab_result_display', this.dependencies.caches, result, 2) as WorksheetDisplayResult;
            this.dependencies.presentationState.lastTex = state.TeX;
            this.dependencies.presentationState.resultInputBindings = state.bindings;
        }

        /** @brief Request native matrix presentation while preserving target ownership. */
        setMatrixPrettyResult(resultText: string, prettyText: string,
                              element = this.dependencies.context().functionStyle,
                              moreButton = this.dependencies.context().functionMore): Promise<unknown> {
            return this.dependencies.flow().continue(ResultFlowKind.matrixPresentation, {
                text: String(resultText || ''), pretty: String(prettyText || ''), element, button: moreButton
            });
        }

        /** @brief Install rendered mathematical output and retain its exact native TeX. */
        setRenderedResult(data: ResultData): void {
            this.dependencies.presentationState.lastTex = this.dependencies.native().call(
                'lab_result_rendered', this.dependencies.caches, data, 0) as string;
        }

        /** @brief Toggle a text card's native abbreviated or complete presentation. */
        toggleTextDigits(element: HTMLElement, button: WorksheetCopyButton): void {
            this.dependencies.native().call('lab_result_text_digits', this.dependencies.caches, element, button);
        }

        /** @brief Install the native expression, function, input and value card texts. */
        installEvaluationTextCards(cards: WorksheetTextCards): void {
            const context = this.dependencies.context();
            this.setExpandableText(context.parsed, context.parsedMore, cards.expression, cards.full_expression);
            this.dependencies.reuse().setResultInputText(cards.input);
            this.setExpandableText(context.functionStyle, context.functionMore, cards.function, cards.full_function);
            this.setValueText(cards.value);
        }
    }

    /** @brief Native result text and binding snapshots for transfer back into the editor. */
    export class ResultReuseController {
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: ResultReuseControllerDependencies) {
            marsBindCallbacks(this, [
                'setResultInputText', 'resultExpressionTextForInput', 'sendResultExpressionToInput'
            ]);
        }

        /** @brief Store reusable result text and a native snapshot of its bindings. */
        setResultInputText(text: string, bindings: WorksheetBinding[] | null = null): void {
            this.dependencies.native().call('lab_result_input_set', String(text || ''));
            this.dependencies.presentationState.resultInputBindings = this.dependencies.native().call(
                'lab_result_binding_snapshot', bindings) as WorksheetBinding[];
        }

        /** @brief Read the native-selected expression for reuse in the current mode. */
        resultExpressionTextForInput(): string {
            return this.dependencies.native().call('lab_result_input_get', this.dependencies.mode().currentMode()) as string;
        }

        /** @brief Run the native continuation that transfers a result into the editor. */
        sendResultExpressionToInput(): Promise<unknown> {
            return this.dependencies.flow().continue(ResultFlowKind.reuseInput, {});
        }
    }

    /** @brief Result commands delegated to native continuations rather than duplicated browser workflow. */
    export class ResultActionController {
        private readonly statusTimers = new Set<number>();
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: ResultActionControllerDependencies) {
            marsBindCallbacks(this, [
                'statusLater', 'dispose', 'renderTexSvg', 'toggleRenderedDigits', 'copyInputFromEvent', 'copyResultFromEvent'
            ]);
        }

        /** @brief Schedule deferred status feedback and retain its timer until completion. */
        statusLater(text: string, delay: number): number {
            // This closure retains the timer ID and message for this single scheduled status.
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
        renderTexSvg(TeX: string): Promise<unknown> {
            return this.dependencies.flow().continue(ResultFlowKind.renderTeX, {TeX});
        }

        /** @brief Run the native continuation for rendered-result expansion. */
        toggleRenderedDigits(): Promise<unknown> {
            return this.dependencies.flow().continue(ResultFlowKind.renderedDigits, {});
        }

        /** @brief Copy after outstanding binding commits, using the browser clipboard. */
        copyInputFromEvent(): Promise<unknown> {
            return this.dependencies.flow().continue(ResultFlowKind.copyInput, {button: this.dependencies.context().inputCopy});
        }

        /** @brief Copy an exact native card source through the browser clipboard. */
        copyResultFromEvent(button: WorksheetCopyButton): Promise<unknown> {
            return this.dependencies.flow().continue(ResultFlowKind.copyResult, {button});
        }
    }

    /** @brief Reset coordination for native cards, binding state and result-specific browser resources. */
    export class ResultResetController {
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: ResultResetControllerDependencies) {
            marsBindCallbacks(this, [
                'clearResultPane', 'clearResultDetails'
            ]);
        }

        /** @brief Clear native result cards, local-date content and reusable result details. */
        clearResultPane(): void {
            this.dependencies.native().call('lab_result_pane_clear');
            this.dependencies.calendarResults().setDatetimeLocalText('');
            this.clearResultDetails();
        }

        /** @brief Reset native result state, optionally retaining authored bindings. */
        clearResultDetails(options: WorksheetResultResetOptions = {}): void {
            this.dependencies.functionRun().clearFunctionRun();
            const state = this.dependencies.native().call(
                'lab_result_reset', this.dependencies.caches, this.dependencies.mode().currentMode(), options) as ResultResetPlan;
            const presentation = this.dependencies.presentationState;
            const bindings = this.dependencies.bindings();
            presentation.resultInputBindings = state.resultInputBindings;
            presentation.lastTex = state.lastTex;
            presentation.lastDerivativeExpression = state.lastDerivativeExpression;
            bindings.currentVariables = state.currentVariables;
            bindings.currentDifferentiable = state.currentDifferentiable;
            this.dependencies.presentation().renderDerivativeButtons(bindings.currentVariables);
            this.dependencies.native().services(state, {
                clearVariableValues(): void { bindings.clearVariableValues(); }
            });
        }
    }

    /** @brief Native calendar presentation and live visibility state, with explicit missing-data failures. */
    export class CalendarResultPresenter {
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: CalendarResultPresenterDependencies) {
            marsBindCallbacks(this, [
                'setDatetimeLocalText', 'renderDatetimeSections', 'setAlmanacVisibility', 'bindAlmanacTotalityActions', 'renderAlmanacWorksheet'
            ]);
        }

        /** @brief Render local-date sections and apply native card-visibility decisions. */
        setDatetimeLocalText(text: string, sections: unknown = null): void {
            const context = this.dependencies.context();
            const native = this.dependencies.native();
            const plan = native.call(
                'lab_flow_location_local', {value: text}, sections,
                context.datetimeLocalBody, context.datetimeLocal) as NativeServicePlan;
            native.services(plan, this.dependencies.locationServices());
        }

        /** @brief Render native calendar sections; reject missing presentation metadata. */
        renderDatetimeSections(element: HTMLElement, button: WorksheetCopyButton | null, sections: unknown, fallbackText = ''): void {
            if (!this.dependencies.native().call('lab_result_datetime', element, button, sections, String(fallbackText || '')))
                throw new Error(
                    'DateTime presentation is missing from the server response. ' +
                    'Run make mars-lab-restart, reload the page and evaluate again.');
        }

        /** @brief Store the native-selected almanac visibility mode. */
        setAlmanacVisibility(value: string): void {
            this.dependencies.context().worksheetSession.calendar.almanacVisibilityMode = value;
        }

        /** @brief Install native action delegation beneath an optional result root. */
        bindAlmanacTotalityActions(root?: HTMLElement): void {
            this.dependencies.native().call('lab_almanac_events_install', root, null);
        }

        /** @brief Validate and project a native almanac presentation or report its absence. */
        renderAlmanacWorksheet(target: HTMLElement, data: unknown): void {
            const visibility = this.dependencies.native().call(
                'lab_almanac_events_render', target, data, this.dependencies.almanacState) as string | null;
            if (visibility === null)
                throw new Error(
                    'Almanac presentation is missing from the server response. ' +
                    'Run make mars-lab-restart, reload the page and evaluate again.');
            this.setAlmanacVisibility(visibility);
        }
    }

    // Resolve page owners only when an action runs; definition scripts load before bootstrap.
    /** @brief Lazy browser capabilities shared by this module's resource owners. */
    const resultHosts = {
        native(): ResultNativeServices { return labDOM; },
        viewport() { return {width: window.innerWidth, height: window.innerHeight}; },
        context() { return WorksheetContext; },
        flow(): ResultFlow { return WorksheetRequests.runtime; },
        functionRun(): ResultFunctionRun { return WorksheetRequests.functionCard; },
        reuse(): ResultInput { return reuse; },
        mode(): ResultMode { return WorksheetWorkspace.mode; },
        presentation(): ResultDerivativeControls { return WorksheetWorkspace.presentation; },
        bindings(): ResultBindings { return WorksheetBindings.panel; },
        calendarResults(): ResultLocalDate { return calendarResults; },
        locationServices(): NativeServiceRegistry { return WorksheetLocations.locationServices; },

        requestFrame(callback: FrameRequestCallback): number { return requestAnimationFrame(callback); },
        cancelFrame(frame: number): void { cancelAnimationFrame(frame); },
        fitRendered(): void { labDOM.call('lab_layout_fit'); },
        fitSolver(): void { void WorksheetRequests.solver.fitSolverTexToCard(); },

        document(): ResultClipboardDocument { return document; },
        clipboard(): ResultClipboard | undefined { return navigator.clipboard; },
        secure(): boolean { return window.isSecureContext; },
        schedule(callback: () => void, delay: number): number { return setTimeout(callback, delay); },
        cancel(timer: number): void { clearTimeout(timer); },
        surface(): string | null { return labSurfacePreview.exportSvg(); },
        lastTeX(): string { return WorksheetWorkspace.presentation.state.lastTex; },
        status(text: string): void { WorksheetWorkspace.presentation.setStatus(text); }
    };

    // Page-owned instances. Constructors retain capabilities without invoking them.
    /** @brief Page-lifetime tooltip owner. */
    export const tooltip = new WorksheetTooltipController(resultHosts.native, resultHosts.viewport);

    /** @brief Owner of pending rendered-result fitting. */
    export const renderedFit = new WorksheetFitScheduler(
        resultHosts.requestFrame, resultHosts.cancelFrame, resultHosts.fitRendered);

    /** @brief Independent owner of pending solver fitting. */
    export const solverFit = new WorksheetFitScheduler(
        resultHosts.requestFrame, resultHosts.cancelFrame, resultHosts.fitSolver);

    /** @brief Native presentation metadata retained for the page lifetime. */
    export const caches = new WorksheetPresentationCaches(resultHosts.native);

    /** @brief Clipboard sources, browser writes and feedback timers. */
    export const clipboard = new WorksheetClipboardController(resultHosts);

    /** @brief Native result-card projection and reusable presentation state. */
    export const cards = new ResultCardPresenter({
        context: resultHosts.context,
        get presentationState() { return WorksheetWorkspace.presentation.state; },
        flow: resultHosts.flow,
        functionRun: resultHosts.functionRun,
        reuse: resultHosts.reuse,
        native: resultHosts.native,
        caches
    });

    /** @brief Native result text and binding snapshots for editor reuse. */
    export const reuse = new ResultReuseController({
        get presentationState() { return WorksheetWorkspace.presentation.state; },
        flow: resultHosts.flow,
        mode: resultHosts.mode,
        native: resultHosts.native
    });

    /** @brief Result commands and their owned status timers. */
    export const actions = new ResultActionController({
        context: resultHosts.context,
        flow: resultHosts.flow,
        status: resultHosts.status,
        schedule: resultHosts.schedule,
        cancel: resultHosts.cancel
    });

    /** @brief Native reset coordination for cards and associated browser state. */
    export const reset = new ResultResetController({
        get presentationState() { return WorksheetWorkspace.presentation.state; },
        functionRun: resultHosts.functionRun,
        mode: resultHosts.mode,
        presentation: resultHosts.presentation,
        bindings: resultHosts.bindings,
        calendarResults: resultHosts.calendarResults,
        native: resultHosts.native,
        caches
    });

    /** @brief Native calendar rendering with live visibility state. */
    export const calendarResults = new CalendarResultPresenter({
        context: resultHosts.context,
        native: resultHosts.native,
        almanacState: labAlmanacEventState,
        locationServices: resultHosts.locationServices
    });

}
