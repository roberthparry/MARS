/**
 * @file app.ts
 * @brief Bootstrap and lifetime management for the Lab browser application.
 *
 * Loads the native transport and domain adapters before initialising the worksheet.
 * Owns browser observers, resize subscriptions and periodic refresh cleanup; native
 * continuations retain control of startup policy and event dispatch.
 *
 * Definition-only classic scripts establish domain controllers before worksheet startup.
 * The application owns bootstrap data and browser subscriptions; domain
 * APIs provide the capabilities registered with native continuations after the catalogue
 * is available. Startup requires neither a bundler nor browser-side mathematical rewriting.
 */

/** Browser lifetime callback whose identity is retained for removal or cleanup. */
interface LabLifetimeCallback {
    /** @brief Perform one browser lifetime operation. */
    (): void;
}

/** Promise rejection capability retained by a single script load. */
interface LabScriptReject {
    /** @brief Reject the pending load with its original failure reason. */
    (error: unknown): void;
}

/** Owns one script element and its temporary load, execution-error and abort listeners. */
class LabScriptLoad {
    private started = false;
    private script!: HTMLScriptElement;
    private resolve!: LabLifetimeCallback;
    private reject!: LabScriptReject;

    /** @brief Retain the script URL and application lifetime without starting a load. */
    constructor(private readonly path: string, private readonly signal: AbortSignal) {
        this.cancelled = this.cancelled.bind(this);
        this.executionError = this.executionError.bind(this);
        this.loaded = this.loaded.bind(this);
        this.loadError = this.loadError.bind(this);
    }

    /** @brief Start this load once; executor failures become promise rejections. */
    start(): Promise<void> {
        if (this.started) return Promise.reject(new Error('Script load has already started'));
        this.started = true;
        return new Promise<void>(this.execute.bind(this));
    }

    private execute(resolve: LabLifetimeCallback, reject: LabScriptReject): void {
        this.signal.throwIfAborted();
        this.resolve = resolve;
        this.reject = reject;
        this.script = document.createElement('script');
        this.script.src = this.path;
        this.signal.addEventListener('abort', this.cancelled, {once: true});
        window.addEventListener('error', this.executionError);
        this.script.onload = this.loaded;
        this.script.onerror = this.loadError;
        try {
            document.head.appendChild(this.script);
        } catch (error) {
            this.cleanup();
            throw error;
        }
    }

    private cleanup(): void {
        window.removeEventListener('error', this.executionError);
        this.signal.removeEventListener('abort', this.cancelled);
        this.script.onload = null;
        this.script.onerror = null;
    }

    private cancelled(): void {
        this.cleanup();
        this.script.remove();
        this.reject(this.signal.reason);
    }

    private executionError(event: ErrorEvent): void {
        this.cleanup();
        this.reject(new Error(event.message || `Could not execute ${this.path}`));
    }

    private loaded(): void {
        this.cleanup();
        this.resolve();
    }

    private loadError(): void {
        this.cleanup();
        this.reject(new Error(`Could not load ${this.path}`));
    }
}

/** Owns startup and browser observers; native continuations decide worksheet policy. */
class LabApplication {
    private readonly observers: ResizeObserver[] = [];
    private readonly resizeCallbacks = new Set<LabLifetimeCallback>();
    private readonly cleanupCallbacks: LabLifetimeCallback[] = [];
    private readonly lifetime = new AbortController();
    private refreshTimer: number | undefined;
    private config: LabConfiguration | undefined;
    private nativeCatalogue: LabCatalogue | undefined;
    private definitionScriptNames: readonly string[] = [];

    /** @brief Return native bootstrap configuration; reject access before bootstrap supplies it. */
    public get configuration(): LabConfiguration {
        if (!this.config) throw new Error('Lab configuration is not ready');
        return this.config;
    }

    /** @brief Return the native catalogue after bootstrap has installed it. */
    public get catalogue(): LabCatalogue {
        if (!this.nativeCatalogue) throw new Error('Lab catalogue is not ready');
        return this.nativeCatalogue;
    }

    /** @brief Expose the immutable list of definition scripts selected by native bootstrap. */
    public get definitionScripts(): readonly string[] { return this.definitionScriptNames; }

    /** @brief Retain the resize event source; construction alone installs no browser subscriptions. */
    public constructor(private readonly resizeTarget: EventTarget = window) {}

    /** @brief Load a classic script and reject load or observed execution errors, removing temporary listeners. */
    public async loadScript(path: string): Promise<void> {
        this.lifetime.signal.throwIfAborted();
        await new LabScriptLoad(path, this.lifetime.signal).start();
    }

    /** @brief Start transport then run native bootstrap, loading definition scripts before worksheet initialisation. */
    public async start(): Promise<void> {
        await this.loadScript('/js/transport.js');
        this.lifetime.signal.throwIfAborted();
        this.cleanupCallbacks.push(this.disposeDom.bind(this), this.disposeWire.bind(this));
        await labWire.start();
        this.lifetime.signal.throwIfAborted();
        await labDOM.flow(64, {}, {}, {
            fetch: this.fetchResource.bind(this),
            decode: this.decodeResponse.bind(this),
            native: this.callNative.bind(this),
            config: this.installConfig.bind(this),
            catalogue: this.installCatalogue.bind(this),
            definitions: this.loadDefinitions.bind(this),
            script: this.loadScript.bind(this),
            events: this.bindEvents.bind(this),
            widgets: this.initialiseWidgets.bind(this),
            worksheet: this.startWorksheet.bind(this),
            fail: this.failStartup.bind(this)
        }, this.lifetime.signal);
    }

    private fetchResource(url: string) {
        return labHttp.fetch(url);
    }

    private decodeResponse(response: MarsTransportResponse): Promise<unknown> {
        return response.labData();
    }

    private callNative(entry: string, ...args: unknown[]): unknown {
        return labDOM.call(entry, ...args);
    }

    private installConfig(value: LabConfiguration): LabConfiguration {
        this.config = value;
        return value;
    }

    private installCatalogue(value: LabCatalogue): LabCatalogue {
        this.nativeCatalogue = value;
        return value;
    }

    private loadDefinitions(names: string[]): Promise<void[]> {
        this.definitionScriptNames = Object.freeze([...names]);
        const pending: Promise<void>[] = [];
        for (const name of names) pending.push(this.loadScript(`/js/${name}.js`));
        return Promise.all(pending);
    }

    private startWorksheet(): Promise<unknown> {
        this.initialiseWorksheet();
        return window.labInitialEvaluation;
    }

    private failStartup(message: string): never {
        throw new Error(message);
    }

    private disposeDom(): void {
        labDOM.dispose();
    }

    private disposeWire(): void {
        labWire.dispose();
    }

    /** @brief Retain a resize observer until disposal; ignore absent elements and require ResizeObserver support. */
    public observe(element: Element | null, callback: ResizeObserverCallback): void {
        if (!element) return;
        const observer = new ResizeObserver(callback);
        observer.observe(element);
        this.observers.push(observer);
    }

    /** @brief Replace any existing mobile-access refresh interval with a five-second subscription. */
    public startRefresh(): void {
        if (this.refreshTimer !== undefined) window.clearInterval(this.refreshTimer);
        this.refreshTimer = window.setInterval(WorksheetRequests.calendarRequests.refreshMobileAccess, 5000);
    }

    /** @brief Subscribe a resize callback once by identity and retain it for disposal. */
    public listenResize(callback: LabLifetimeCallback): void {
        if (this.resizeCallbacks.has(callback)) return;
        this.resizeCallbacks.add(callback);
        this.resizeTarget.addEventListener('resize', callback);
    }

    /** @brief Disconnect owned observers, timers and resize listeners without unloading scripts or native state. */
    public dispose(): void {
        this.lifetime.abort();
        for (const observer of this.observers) observer.disconnect();
        this.observers.length = 0;
        window.clearInterval(this.refreshTimer);
        this.refreshTimer = undefined;
        for (const callback of this.resizeCallbacks)
            this.resizeTarget.removeEventListener('resize', callback);
        this.resizeCallbacks.clear();
        for (const cleanup of this.cleanupCallbacks.splice(0).reverse()) cleanup();
    }

    /** Bind generic host services; C owns control subscriptions and dispatch policy. */
    private bindEvents(): void {
        this.cleanupCallbacks.push(this.closeSurface.bind(this), this.disposeVolume.bind(this));
        this.cleanupCallbacks.push(this.disposeControllers.bind(this));
        labDOM.bindPresentation({
            cardId: this.cardId.bind(this),
            scheduleFit: this.scheduleRenderedFit.bind(this)
        });
        this.installEventServices();
        labDOM.call('lab_events_install', document, window);
        labDOM.call('lab_widget_events_install', document, window, WorksheetContext.marsDatePickerState);
        if (typeof ResizeObserver === 'function') {
            for (const textarea of WorksheetContext.labTextareas) {
                this.observe(textarea, WorksheetWorkspace.layout.scheduleEditorResizeGrip);
            }
        }
    }

    private closeSurface(): void {
        labSurfacePreview.close();
    }

    private disposeVolume(): void {
        labSurfaceVolume.dispose();
    }

    private disposeControllers(): void {
        WorksheetRequests.plots.dispose();
        WorksheetRequests.presentation.dispose();
        WorksheetRequests.solver.dispose();
        WorksheetRequests.labRequests.dispose();
        WorksheetBindings.controller.dispose();
        WorksheetBindings.panel.dispose();
        WorksheetBindings.integratorPanel.dispose();
        WorksheetWorkspace.layout.dispose();
        WorksheetWorkspace.presentation.dispose();
        WorksheetPersistence.persistence.dispose();
        WorksheetLocations.selects.dispose();
        WorksheetLocations.picker.dispose();
        WorksheetResults.tooltip.dispose();
        WorksheetResults.renderedFit.dispose();
        WorksheetResults.solverFit.dispose();
        WorksheetResults.clipboard.dispose();
        WorksheetResults.actions.dispose();
        WorksheetResults.caches.dispose();
        WorksheetRequests.runtime.dispose();
    }

    private cardId(element: HTMLElement): number {
        return WorksheetContext.resultCardIds.get(element) ?? -1;
    }

    private scheduleRenderedFit(): void {
        WorksheetResults.renderedFit.schedule();
    }

    private scheduleSolverFit(): void {
        WorksheetResults.solverFit.schedule();
    }

    private resetPlotPreview(): void {
        WorksheetRequests.labRequests.cancel('plot');
        labSurfaceVolume.cancel();
        labSurfacePreview.dispose();
    }

    private markDatetimeOffset(): void {
        WorksheetLocations.calendar.markDatetimeOffsetTouched();
    }

    private saveAlmanac(): void {
        WorksheetPersistence.persistence.saveCalendar('almanac');
    }

    private installEventServices(): void {
        labDOM.bindEvents(Object.freeze({
            clearForwardHistory: WorksheetPersistence.history.clearForward,
            clearGoalSeekRequest: WorksheetBindings.goal.clearGoalSeekRequest,
            hideTargetEntry: WorksheetWorkspace.presentation.hideTargetEntry,
            evaluateCurrentMode: WorksheetWorkspace.mode.evaluateCurrentMode,
            navigateHistoryFromEvent: WorksheetPersistence.navigation.navigate,
            evaluateFromKeyboard: WorksheetWorkspace.mode.evaluateFromKeyboard,
            selectWorksheetMode: WorksheetWorkspace.mode.selectWorksheetMode,
            normaliseWorksheetControl: WorksheetWorkspace.mode.normaliseWorksheetControl,
            takePlot: WorksheetRequests.plots.takePlot,
            schedulePlotControls: WorksheetRequests.plots.schedulePlotControls,
            schedulePlotSlice: WorksheetRequests.plots.schedulePlotSlice,
            redrawPlotDrag: WorksheetRequests.plots.redrawPlotDrag,
            interruptPlotDrag: WorksheetRequests.plots.interruptPlotDrag,
            zoomSurface: WorksheetRequests.plots.zoomSurface,
            setSurfaceZoom: WorksheetRequests.plots.setSurfaceZoom,
            pauseSurfaceSampling: WorksheetRequests.plots.pauseSurfaceSampling,
            resetPlotPreview: this.resetPlotPreview.bind(this),
            schedulePlotWheelEnd: WorksheetRequests.plots.schedulePlotWheelEnd,
            startGoalSeekFromEvent: WorksheetRequests.goals.startGoalSeekFromEvent,
            setWorksheetPrecision: WorksheetWorkspace.precision.setWorksheetPrecision,
            toggleTextDigits: WorksheetResults.cards.toggleTextDigits,
            copyResultFromEvent: WorksheetResults.actions.copyResultFromEvent,
            flushWorksheetState: WorksheetWorkspace.history.flushWorksheetState,
            scheduleWorkspacePanelFit: WorksheetWorkspace.layout.scheduleEditorResizeGrip,
            openMarsDatePicker: WorksheetLocations.picker.open,
            closeMarsDatePicker: WorksheetLocations.picker.close,
            applyCalendarControlEvent: WorksheetLocations.calendarActions.applyCalendarControlEvent,
            refreshWorksheetFromInput: WorksheetBindings.controller.refreshWorksheetFromInput,
            clearWorksheetFromEvent: WorksheetWorkspace.mode.clearWorksheetFromEvent,
            toggleHelp: WorksheetWorkspace.presentation.toggleHelp,
            formatAlmanacTimeFromEvent: WorksheetLocations.calendarActions.formatAlmanacTimeFromEvent,
            toggleRenderedDigits: WorksheetResults.actions.toggleRenderedDigits,
            runFunctionCard: WorksheetRequests.functionCard.runFunctionCard,
            sendResultExpressionToInput: WorksheetResults.reuse.sendResultExpressionToInput,
            copyInputFromEvent: WorksheetResults.actions.copyInputFromEvent,
            scheduleEditorResizeGrip: WorksheetWorkspace.layout.scheduleEditorResizeGrip,
            commitMarsDateValue: WorksheetLocations.picker.commit,
            shiftMarsDatePickerMonth: WorksheetLocations.picker.shiftMonth,
            shiftMarsDatePickerYear: WorksheetLocations.picker.shiftYear,
            setMarsDatePickerMonthYear: WorksheetLocations.picker.setMonthYear,
            commitMarsTodayValue: WorksheetLocations.picker.commitToday,
            placeMarsDatePicker: WorksheetLocations.picker.place,
            showButtonTooltip: WorksheetResults.tooltip.show,
            hideButtonTooltip: WorksheetResults.tooltip.hide,
            markDatetimeOffsetTouched: this.markDatetimeOffset.bind(this),
            setAlmanacVisibility: WorksheetResults.calendarResults.setAlmanacVisibility,
            saveLastAlmanacState: this.saveAlmanac.bind(this),
            renderAlmanacWorksheet: WorksheetResults.calendarResults.renderAlmanacWorksheet,
            refreshAlmanacLandTotality: WorksheetRequests.calendarRequests.refreshAlmanacLandTotality,
            evaluateAlmanac: WorksheetRequests.evaluation.evaluateAlmanac,
            applyAlmanacTotalityAction: WorksheetLocations.calendarActions.applyAlmanacTotalityAction,
            setStatus: WorksheetWorkspace.presentation.setStatus
        }));
    }

    /** Browser observers and host callbacks for controls whose policies are native. */
    private initialiseWidgets(): void {
        labDOM.services(labDOM.call('lab_bootstrap_widgets') as NativeServicePlan, {enhance: WorksheetLocations.selects.enhance});
        if (typeof ResizeObserver === 'function') {
            this.observe(WorksheetContext.rendered, this.scheduleRenderedFit.bind(this));
            this.observe(document.getElementById('plotRendered'), this.scheduleRenderedFit.bind(this));
            this.observe(WorksheetContext.functionStyle, this.scheduleSolverFit.bind(this));
        } else {
            this.listenResize(this.scheduleRenderedFit.bind(this));
            this.listenResize(this.scheduleSolverFit.bind(this));
        }
    }

    /** Initialise worksheet controls and periodic services, exposing the initial calculation promise on window. */
    private initialiseWorksheet(): void {
        WorksheetWorkspace.presentation.syncModeTabs();
        WorksheetWorkspace.presentation.syncModeUI();
        WorksheetWorkspace.presentation.setStatus('Ready');
        WorksheetRequests.calendarRequests.refreshMobileAccess();
        this.startRefresh();
        window.labInitialEvaluation = WorksheetRequests.runtime.continue(62, {});
    }

    /** @brief Present startup failure without requiring transport or native initialisation. */
    public startupFailed(error: unknown): never {
        const status = document.getElementById('status');
        const output = document.getElementById('rendered');
        const button = document.getElementById('run');
        if (status) status.textContent = 'Startup failed';
        if (output) output.textContent = error instanceof Error ? error.message : String(error);
        if (button instanceof HTMLButtonElement) button.disabled = true;
        throw error;
    }

    /** @brief Preserve BFCache-owned resources and dispose only on permanent page departure. */
    public pageHidden(event: PageTransitionEvent): void {
        if (!event.persisted) this.dispose();
    }
}

/** @brief Page-wide application lifetime and native bootstrap owner. */
const labApplication: LabApplication = new LabApplication();

window.labReady = labApplication.start().catch(labApplication.startupFailed.bind(labApplication));
window.addEventListener('pagehide', labApplication.pageHidden.bind(labApplication));
