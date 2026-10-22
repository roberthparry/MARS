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
    /** @brief Initial selected-jurisdiction loading; workspace startup does not await it. */
    public townsReady: Promise<void> = Promise.resolve();
    private readonly townJurisdictions = new Set<string>();
    private readonly failedTownCodes = new Set<string>();
    private readonly townJobs = new Map<string, {controller: AbortController; promise: Promise<void>; ready: Promise<void>}>();
    private nearbyCentre: [number, number] | null = null;
    private nearbyComplete = false;
    private nearbyTowns?: AbortController;
    private readonly observers: ResizeObserver[] = [];
    private readonly resizeCallbacks = new Set<LabLifetimeCallback>();
    private readonly cleanupCallbacks: LabLifetimeCallback[] = [];
    private readonly lifetime = new AbortController();
    private refreshTimer: number | undefined;
    private config: LabConfiguration | undefined;
    private nativeCatalogue: LabCatalogue | undefined;
    private definitionScriptNames: readonly string[] = [];
    private readonly pendingTownCodes = new Set<string>();
    private townRefresh: Promise<void> | null = null;

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
        this.townsReady = this.loadTowns().catch(error => {
            if (!this.lifetime.signal.aborted) {
                this.townNotice(`Town loading incomplete: ${String(error)}. Loaded towns and observer coordinates remain usable.`);
            }
        });
    }

    private townNotice(message: string): void {
        if (!message && this.failedTownCodes.size)
            message = `Some towns for ${Array.from(this.failedTownCodes).join(', ')} could not be loaded. ` +
                'Loaded towns remain usable; reselect to retry.';
        for (const id of ['datetimeJurisdiction', 'almanacJurisdiction']) {
            const panel = document.getElementById(id)?.closest('.mode-panel');
            if (!panel) continue;
            let notice = panel.querySelector<HTMLElement>('[data-jurisdiction-notice]');
            if (!notice) {
                notice = document.createElement('p');
                notice.className = 'mode-hint';
                notice.dataset.jurisdictionNotice = 'true';
                panel.prepend(notice);
            }
            notice.textContent = message;
            notice.classList.toggle('hidden', !message);
        }
    }

    private async fetchResource(url: string) {
        if (url !== '/jurisdictions') return labHttp.fetch(url, {signal: this.lifetime.signal});
        // Geography must not prevent the mathematical workspace from starting.
        let data: LabCatalogue;
        try {
            const response = await labHttp.fetch(url, {
                signal: AbortSignal.any([this.lifetime.signal, AbortSignal.timeout(3000)])
            });
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            data = await response.labData() as LabCatalogue;
        } catch {
            this.lifetime.signal.throwIfAborted();
            data = {available: false, options: [], towns: {}, error: 'Jurisdiction catalogue unavailable'};
        }
        return {ok: true, status: 200, labData: async () => data};
    }

    private async loadTowns(): Promise<void> {
        if (!this.catalogue.available) return;
        for (const [code, rows] of Object.entries(this.catalogue.towns)) {
            if (labDOM.call<number>('lab_town_store_add', code, rows) < 0) throw new Error('Cannot retain town catalogue');
            delete this.catalogue.towns[code];
        }
        Object.defineProperty(this.catalogue.towns, 'nativeStore', {value: true, configurable: true});
        document.dispatchEvent(new CustomEvent('lab-town-catalogue-ready'));
        const refresh = () => { void this.loadSelectedTowns(); };
        for (const id of ['datetimeJurisdiction', 'almanacJurisdiction'])
            document.getElementById(id)?.addEventListener('change', refresh, {signal: this.lifetime.signal});
        document.addEventListener('mars-almanac-observer-committed', refresh, {signal: this.lifetime.signal});
        const latitude = Number((document.getElementById('almanacLatitude') as HTMLInputElement).value);
        const longitude = Number((document.getElementById('almanacLongitude') as HTMLInputElement).value);
        void this.loadNearbyTowns(latitude, longitude);
        await this.loadSelectedTowns();
    }

    private async loadSelectedTowns(): Promise<void> {
        const pending: Promise<void>[] = [];
        const codes = new Set(['almanacJurisdiction', 'datetimeJurisdiction'].map(id =>
            (document.getElementById(id) as HTMLSelectElement)?.value || '').filter(code => /^[A-Z0-9-]{2,32}$/.test(code)));
        // Include parent fallback only when its subdivision is selected, never the whole world.
        for (const code of Array.from(codes)) if (code.includes('-')) codes.add(code.split('-')[0]);
        for (const code of this.failedTownCodes) if (!codes.has(code)) this.failedTownCodes.delete(code);
        for (const [code, job] of this.townJobs) if (!codes.has(code)) job.controller.abort();
        for (const code of codes) {
            if (this.lifetime.signal.aborted) return;
            const selected = ['datetimeJurisdiction', 'almanacJurisdiction'].some(id => {
                const value = (document.getElementById(id) as HTMLSelectElement)?.value || '';
                return value === code || value.startsWith(code + '-');
            });
            if (!selected || this.townJurisdictions.has(code)) continue;
            let job = this.townJobs.get(code);
            if (job?.controller.signal.aborted) job = undefined;
            if (!job) {
                const controller = new AbortController();
                let firstPage!: () => void;
                const ready = new Promise<void>(resolve => { firstPage = resolve; });
                const promise = this.loadJurisdictionTowns(code, controller.signal, firstPage).catch(() => {
                    if (!controller.signal.aborted && !this.lifetime.signal.aborted) {
                        this.failedTownCodes.add(code);
                        this.townNotice('');
                    }
                }).finally(() => {
                    firstPage();
                    if (this.townJobs.get(code)?.controller === controller) this.townJobs.delete(code);
                });
                job = {controller, promise, ready};
                this.townJobs.set(code, job);
            }
            pending.push(job.promise);
        }
        await Promise.all(pending);
    }

    /** Await only the first country page; the remaining catalogue continues in the background. */
    public async prepareCountrySelection(code: string): Promise<void> {
        if (!this.catalogue?.available || this.townJurisdictions.has(code)) return;
        void this.loadSelectedTowns();
        await this.townJobs.get(code)?.ready;
    }

    private async loadJurisdictionTowns(code: string, controller: AbortSignal, firstPage: () => void): Promise<void> {
        this.failedTownCodes.delete(code);
        this.townNotice('');
        let cursor = '0';
        do {
            const signal = AbortSignal.any([controller, this.lifetime.signal, AbortSignal.timeout(10000)]);
            const response = await labHttp.fetch(`/towns?after=${cursor}&jurisdiction=${code}`, {signal});
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            const batch = await response.labData() as LabCatalogue;
            signal.throwIfAborted();
            if (!batch.available || !batch.next || !/^\d+$/.test(batch.next))
                throw new Error(batch.error || 'Invalid town catalogue page');
            if (batch.next !== '0' && BigInt(batch.next) <= BigInt(cursor))
                throw new Error('Town catalogue cursor did not advance');
            await this.publishTowns(batch.towns);
            firstPage();
            this.townNotice('');
            cursor = batch.next;
        } while (cursor !== '0');
        this.lifetime.signal.throwIfAborted();
        this.townJurisdictions.add(code);
        this.townNotice('');
    }

    /** Load towns near the current view without downloading the world catalogue. */
    public async loadNearbyTowns(latitude: number, longitude: number): Promise<void> {
        if (!this.catalogue?.available ||
            !Number.isFinite(latitude) || !Number.isFinite(longitude)) return;
        if (this.nearbyComplete && this.nearbyCentre && Math.abs(latitude - this.nearbyCentre[0]) < 1 &&
            Math.abs(longitude - this.nearbyCentre[1]) < 1) return;
        this.nearbyTowns?.abort();
        this.nearbyCentre = [latitude, longitude];
        this.nearbyComplete = false;
        const controller = this.nearbyTowns = new AbortController();
        const signal = AbortSignal.any([controller.signal, this.lifetime.signal, AbortSignal.timeout(10000)]);
        let cursor = '0';
        try {
            for (let page = 0; page < 8; ++page) {
                const response = await labHttp.fetch(`/towns?after=${cursor}&lat=${latitude}&lon=${longitude}`, {signal});
                if (!response.ok) throw new Error(`HTTP ${response.status}`);
                const batch = await response.labData() as LabCatalogue;
                signal.throwIfAborted();
                if (!batch.available || !batch.next || !/^\d+$/.test(batch.next) ||
                    (batch.next !== '0' && BigInt(batch.next) <= BigInt(cursor))) throw new Error('Invalid local town page');
                await this.publishTowns(batch.towns);
                this.townNotice('');
                cursor = batch.next;
                if (cursor === '0') break;
            }
            if (this.nearbyTowns === controller) this.nearbyComplete = true;
        } catch {
            if (this.nearbyTowns === controller) this.nearbyCentre = null;
            // Local loading is optional; selected-country menus load independently.
        }
    }

    private async publishTowns(towns: LabTownCatalogue): Promise<void> {
        let added = 0;
        let count = 0;
        for (const rows of Object.values(towns)) {
            if (!Array.isArray(rows) || (count += rows.length) > 512) throw new Error('Invalid town catalogue batch');
        }
        for (const [code, rows] of Object.entries(towns)) {
            const inserted = labDOM.call<number>('lab_town_store_add', code, rows);
            if (inserted < 0) throw new Error('Cannot retain town catalogue batch');
            if (inserted) {
                added += inserted;
                this.pendingTownCodes.add(code);
            }
        }
        if (!added) return;
        document.dispatchEvent(new CustomEvent('lab-town-catalogue-ready'));
        // Local and jurisdiction pages share one refresh queue to avoid concurrent menu rebuilds.
        if (!this.townRefresh) {
            this.townRefresh = this.refreshTownMenus();
        }
        await this.townRefresh;
    }

    private async refreshTownMenus(): Promise<void> {
        try {
            while (this.pendingTownCodes.size) {
                // Yield to browser input/painting even when catalogue fetches are satisfied immediately.
                await new Promise<void>(resolve => window.setTimeout(resolve, 0));
                this.lifetime.signal.throwIfAborted();
                const changed = new Set(this.pendingTownCodes);
                this.pendingTownCodes.clear();
                for (const mode of ['datetime', 'almanac']) {
                    const jurisdiction = document.getElementById(`${mode}Jurisdiction`) as WorksheetSelect;
                    const code = jurisdiction.value;
                    const selection = WorksheetLocations.townSelection;
                    if (!changed.has(code) && !changed.has(code.split('-')[0])) continue;
                    const select = document.getElementById(`${mode}Town`) as WorksheetSelect;
                    const current = () => !this.lifetime.signal.aborted && jurisdiction.value === code;
                    await selection.populateTownSelect(select, code, {selectDefault: false, isCurrent: current});
                    if (!current()) continue;
                    const latitude = document.getElementById(`${mode}Latitude`) as HTMLInputElement;
                    const longitude = document.getElementById(`${mode}Longitude`) as HTMLInputElement;
                    if (!selection.selectTownByCoordinates(select, latitude.value, longitude.value)) select.value = '';
                    WorksheetLocations.selects.syncSelection(select);
                    await new Promise<void>(resolve => window.setTimeout(resolve, 0));
                }
            }
        } finally { this.townRefresh = null; }
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
        if (this.nativeCatalogue) {
            labDOM.call('lab_town_store_clear');
            labDOM.call('lab_country_cache_clear');
            labDOM.call('lab_menu_index_clear');
            labDOM.call('lab_globe_borders_clear');
        }
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
