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

/** Owns startup and browser observers; native continuations decide worksheet policy. */
class LabApplication {
    private readonly observers: ResizeObserver[] = [];
    private readonly resizeCallbacks = new Set<() => void>();
    private readonly cleanup_callbacks: (() => void)[] = [];
    private readonly lifetime = new AbortController();
    private refreshTimer: number | undefined;
    private config: LabConfiguration | undefined;
    private native_catalogue: LabCatalogue | undefined;
    private definition_scripts: readonly string[] = [];

    /** Return native bootstrap configuration; reject access before bootstrap supplies it. */
    public get configuration(): LabConfiguration {
        if (!this.config) throw new Error('Lab configuration is not ready');
        return this.config;
    }

    /** Return the native catalogue after bootstrap has installed it. */
    public get catalogue(): LabCatalogue {
        if (!this.native_catalogue) throw new Error('Lab catalogue is not ready');
        return this.native_catalogue;
    }

    /** Expose the immutable list of definition scripts selected by native bootstrap. */
    public get definitionScripts(): readonly string[] { return this.definition_scripts; }

    /** Retain the resize event source; construction alone installs no browser subscriptions. */
    public constructor(private readonly resizeTarget: EventTarget = window) {}

    /** Load a classic script and reject load or observed execution errors, removing temporary listeners. */
    public async loadScript(path: string): Promise<void> {
        this.lifetime.signal.throwIfAborted();
        await new Promise<void>((resolve, reject) => {
            const script = document.createElement('script');
            const cleanup = () => {
                window.removeEventListener('error', executionError);
                this.lifetime.signal.removeEventListener('abort', cancelled);
                script.onload = null;
                script.onerror = null;
            };
            const cancelled = () => {
                cleanup();
                script.remove();
                reject(this.lifetime.signal.reason);
            };
            const executionError = (event: ErrorEvent) => {
                cleanup();
                reject(new Error(event.message || `Could not execute ${path}`));
            };
            script.src = path;
            this.lifetime.signal.addEventListener('abort', cancelled, {once: true});
            window.addEventListener('error', executionError);
            script.onload = () => { cleanup(); resolve(); };
            script.onerror = () => { cleanup(); reject(new Error(`Could not load ${path}`)); };
            document.head.appendChild(script);
        });
    }

    /** Start transport then run native bootstrap, loading definition scripts before worksheet initialisation. */
    public async start(): Promise<void> {
        await this.loadScript('/js/transport.js');
        this.lifetime.signal.throwIfAborted();
        this.cleanup_callbacks.push(() => labDOM.dispose(), () => labWire.dispose());
        await labWire.start();
        this.lifetime.signal.throwIfAborted();
        await labDOM.flow<void>(64, {}, {}, {
            fetch: (url: string) => labHttp.fetch(url),
            decode: (response: MarsTransportResponse) => response.labData(),
            native: (entry: string, ...args: unknown[]) => labDOM.call(entry, ...args),
            config: (value: LabConfiguration) => this.config = value,
            catalogue: (value: LabCatalogue) => this.native_catalogue = value,
            definitions: (names: string[]) => {
                this.definition_scripts = Object.freeze([...names]);
                return Promise.all(names.map(name => this.loadScript(`/js/${name}.js`)));
            },
            script: (path: string) => this.loadScript(path),
            events: () => this.bind_events(),
            widgets: () => this.initialise_widgets(),
            worksheet: () => {
                this.initialise_worksheet();
                return window.labInitialEvaluation;
            },
            fail: (message: string) => { throw new Error(message); }
        }, this.lifetime.signal);
    }

    /** Retain a resize observer until disposal; ignore absent elements and require ResizeObserver support. */
    public observe(element: Element | null, callback: ResizeObserverCallback): void {
        if (!element) return;
        const observer = new ResizeObserver(callback);
        observer.observe(element);
        this.observers.push(observer);
    }

    /** Replace any existing mobile-access refresh interval with a five-second subscription. */
    public startRefresh(): void {
        if (this.refreshTimer !== undefined) window.clearInterval(this.refreshTimer);
        this.refreshTimer = window.setInterval(WorksheetRequests.calendarRequests.refreshMobileAccess, 5000);
    }

    /** Subscribe a resize callback once by identity and retain it for disposal. */
    public listenResize(callback: () => void): void {
        if (this.resizeCallbacks.has(callback)) return;
        this.resizeCallbacks.add(callback);
        this.resizeTarget.addEventListener('resize', callback);
    }

    /** Disconnect owned observers, timers and resize listeners without unloading scripts or native state. */
    public dispose(): void {
        this.lifetime.abort();
        this.observers.forEach(observer => observer.disconnect());
        this.observers.length = 0;
        window.clearInterval(this.refreshTimer);
        this.refreshTimer = undefined;
        for (const callback of this.resizeCallbacks)
            this.resizeTarget.removeEventListener('resize', callback);
        this.resizeCallbacks.clear();
        for (const cleanup of this.cleanup_callbacks.splice(0).reverse()) cleanup();
    }

    /** Bind generic host services; C owns control subscriptions and dispatch policy. */
    private bind_events(): void {
        this.cleanup_callbacks.push(() => labSurfacePreview.close(), () => labSurfaceVolume.dispose());
        this.cleanup_callbacks.push(() => {
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
        });
        labDOM.bind_presentation({
            card_id: element => WorksheetContext.resultCardIds.get(element) ?? -1,
            schedule_fit: () => WorksheetResults.renderedFit.schedule()
        });
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
            resetPlotPreview: () => {
                WorksheetRequests.labRequests.cancel('plot');
                labSurfaceVolume.cancel();
                labSurfacePreview.dispose();
            },
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
            markDatetimeOffsetTouched: () => WorksheetLocations.calendar.markDatetimeOffsetTouched(),
            setAlmanacVisibility: WorksheetResults.calendarResults.setAlmanacVisibility,
            saveLastAlmanacState: () => WorksheetPersistence.persistence.saveCalendar('almanac'),
            renderAlmanacWorksheet: WorksheetResults.calendarResults.renderAlmanacWorksheet,
            refreshAlmanacLandTotality: WorksheetRequests.calendarRequests.refreshAlmanacLandTotality,
            evaluateAlmanac: WorksheetRequests.evaluation.evaluateAlmanac,
            applyAlmanacTotalityAction: WorksheetLocations.calendarActions.applyAlmanacTotalityAction,
            setStatus: WorksheetWorkspace.presentation.setStatus
        }));
        labDOM.call('lab_events_install', document, window);
        labDOM.call('lab_widget_events_install', document, window, WorksheetContext.marsDatePickerState);
        if (typeof ResizeObserver === 'function') {
            WorksheetContext.labTextareas.forEach(textarea => this.observe(textarea, WorksheetWorkspace.layout.scheduleEditorResizeGrip));
        }
    }

    /** Browser observers and host callbacks for controls whose policies are native. */
    private initialise_widgets(): void {
        labDOM.services(labDOM.call<NativeServicePlan>('lab_bootstrap_widgets'), {enhance: WorksheetLocations.selects.enhance});
        if (typeof ResizeObserver === 'function') {
            this.observe(WorksheetContext.rendered, () => WorksheetResults.renderedFit.schedule());
            this.observe(document.getElementById('plotRendered'), () => WorksheetResults.renderedFit.schedule());
            this.observe(WorksheetContext.functionStyle, () => WorksheetResults.solverFit.schedule());
        } else {
            this.listenResize(() => WorksheetResults.renderedFit.schedule());
            this.listenResize(() => WorksheetResults.solverFit.schedule());
        }
    }

    /** Initialise worksheet controls and periodic services, exposing the initial calculation promise on window. */
    private initialise_worksheet(): void {
        WorksheetWorkspace.presentation.syncModeTabs();
        WorksheetWorkspace.presentation.syncModeUI();
        WorksheetWorkspace.presentation.setStatus('Ready');
        WorksheetRequests.calendarRequests.refreshMobileAccess();
        this.startRefresh();
        window.labInitialEvaluation = WorksheetRequests.runtime.continue(62, {});
    }


}

const labApplication: LabApplication = new LabApplication();

window.labReady = labApplication.start().catch((error: unknown) => {
    const status = document.getElementById('status');
    const output = document.getElementById('rendered');
    const button = document.getElementById('run');
    if (status) status.textContent = 'Startup failed';
    if (output) output.textContent = error instanceof Error ? error.message : String(error);
    if (button instanceof HTMLButtonElement) button.disabled = true;
    throw error;
});
window.addEventListener('pagehide', event => { if (!event.persisted) labApplication.dispose(); });
