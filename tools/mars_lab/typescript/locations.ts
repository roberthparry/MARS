/**
 * @file locations.ts
 * @brief Typed browser capabilities for native calendar locations, menus and date pickers.
 *
 * WorksheetLocations exposes calendar actions to the application and native event bridge.
 * Instance controllers retain browser references and callbacks; C owns location matching,
 * navigation, validation and workflow ordering. Intl and form controls provide browser date
 * conversion. Requests use WorksheetRequests and live state belongs to worksheetSession.
 * This classic definition script resolves shared state lazily because bootstrap loads it
 * before the workspace. Menu and picker owners expose explicit disposal, and controller
 * hosts use narrow ports. Callers address the owning instance rather than forwarding functions.
 * No mathematical interpretation or duplicate calendar state lives here.
 */

namespace WorksheetLocations {

    type WorksheetLocationFormsRequest =
        | {action: 'time'; text: string}
        | {action: 'town'; value: string; candidates?: string[]};

    /** @brief Native DOM calls required by location controllers. */
    interface LocationNative {
        call(entry: string, ...args: unknown[]): unknown;
    }

    /** @brief Native projection and service dispatch required by browser menus. */
    interface LocationProjection extends LocationNative {
        services(plan: NativeServicePlan, services: NativeServiceRegistry): void;
    }

    /** @brief Lazy native call capability, resolved after bootstrap. */
    interface LocationNativeProvider { (): LocationNative; }

    /** @brief Lazy native projection capability, resolved after bootstrap. */
    interface LocationProjectionProvider { (): LocationProjection; }

    /** @brief Lazy document capability for menu ownership. */
    interface LocationDocumentProvider { (): Document; }

    /** @brief Native continuation capability used by calendar actions. */
    interface LocationFlow {
        continue(entry: number, state: object): Promise<unknown>;
    }

    /** @brief Calendar flags and catalogue passed through native projection. */
    interface CalendarLocationContext {
        config: LabConfiguration;
        jurisdictions: typeof WorksheetContext.HOLIDAY_JURISDICTION_SET;
        towns: typeof WorksheetContext.JURISDICTION_TOWN_OPTIONS;
        visibility: string;
        automaticOffset: string;
        offsetTouched: boolean;
    }

    /** @brief Calendar context capability shared with town selection. */
    interface LocationCalendar {
        calendarLocationContext(): CalendarLocationContext;
        acceptCalendarLocationContext(context: CalendarLocationContext): void;
    }

    /** @brief Browser date parts validated by native calendar arithmetic. */
    interface CalendarDate { year: number; month: number; day: number; }

    /** @brief Browser UTC date and time text. */
    interface CalendarMoment { date: string; time: string; }

    /** @brief Browser date conversion needed by the picker. */
    interface PickerDates {
        parseMarsIsoDate(text: unknown): CalendarDate | null;
        marsFormNumber(value: unknown): number;
        marsTodayIsoDate(): string;
        marsCurrentGmtMoment(now?: Date): CalendarMoment;
    }

    /** @brief Browser time-zone conversion needed by town selection. */
    interface LocationDates {
        formatOffsetHours(offset: number | null): string;
        timeZoneOffsetHours(zone: string | undefined, date: string | undefined): number | null;
    }

    /** @brief Calendar-only session flags; editor and result state are not exposed through this port. */
    interface CalendarSessionState {
        calendar: {
            almanacVisibilityMode: string;
            datetimeAutoGmtOffset: string;
            datetimeGmtOffsetTouched: boolean;
        };
    }

    /** @brief Live browser state required by calendar projection. */
    interface CalendarHostContext {
        HOLIDAY_JURISDICTION_SET: typeof WorksheetContext.HOLIDAY_JURISDICTION_SET;
        JURISDICTION_TOWN_OPTIONS: typeof WorksheetContext.JURISDICTION_TOWN_OPTIONS;
        worksheetSession: CalendarSessionState;
        ALMANAC_WORKSHEET_TITLE: string;
    }

    /** @brief Native mode identifier capability. */
    interface CalendarMode { workspaceModeId(mode: WorksheetMode): number; }

    /** @brief Calendar controls indexed by the native schema. */
    interface CalendarElements { [name: string]: HTMLElement; }

    /** @brief Optional evaluation fields passed unchanged to native projection. */
    interface CalendarEvaluation {
        editor?: WorksheetEditorMetadata;
        rows?: WorksheetIntegratorRow[];
        binding_values?: WorksheetBinding[];
        evaluation_ready?: boolean;
        [field: string]: unknown;
    }

    /** @brief Selection policy for synchronising both town menus. */
    interface TownSyncOptions { selectDefault?: boolean; }

    /** @brief Browser input creation capability for date conversion. */
    interface DateDocument { createElement(tag: 'input'): HTMLInputElement; }

    /** @brief Native arithmetic capability for browser date conversion. */
    interface DateWire { exports(): MarsTransportExports; }

    /** @brief Shared stateless ownership guard for unrestricted calendar actions. */
    const locationOwnership = {
        /** @brief Accept an action without an external ownership constraint. */
        current(): boolean { return true; },

        /** @brief Resolve request ownership at invocation, not at definition time. */
        requestCurrent(request: WorksheetRequest): boolean {
            return WorksheetRequests.labRequests.current(request);
        },
    };

    /** @brief Browser text parts indexed by Intl's field names. */
    interface DateTextParts { [name: string]: string; }

    /** @brief Lazy browser capabilities shared by the page-owned controllers. */
    const locationHosts = {
        /** @brief Resolve native projection only after bootstrap has supplied it. */
        native(): LocationProjection { return labDOM; },

        /** @brief Resolve the current browser document. */
        document(): Document { return document; },

        /** @brief Resolve the own-property native service table. */
        services(): NativeServiceRegistry { return locationServices; },

        /** @brief Resolve the shared popup state only when an action runs. */
        state(): WorksheetPickerState { return WorksheetContext.marsDatePickerState; },

        /** @brief Resolve the browser date adapter. */
        dates(): BrowserDateAdapter { return dates; },

        /** @brief Read the current viewport dimensions. */
        viewport(): {width: number; height: number} {
            return {width: window.innerWidth, height: window.innerHeight};
        },

        /** @brief Read the browser clock at invocation time. */
        clock(): Date { return new Date(); },

        /** @brief Resolve the native continuation owner. */
        flow(): LocationFlow { return WorksheetRequests.runtime; },

        /** @brief Resolve the calendar context owner. */
        calendar(): CalendarStateController { return calendar; },

        /** @brief Resolve live worksheet state after bootstrap. */
        context(): CalendarHostContext { return WorksheetContext; },

        /** @brief Resolve the workspace mode owner. */
        mode(): CalendarMode { return WorksheetWorkspace.mode; },

        /** @brief Resolve the native-supplied configuration. */
        configuration(): LabConfiguration { return labApplication.configuration; },

        /** @brief Resolve the native browser arithmetic exports. */
        wire(): DateWire { return labWire; },

        /** @brief Browser date formatter constructor retained for injectable adapters. */
        dateTimeFormat: Intl.DateTimeFormat,
    };

    /** @brief Native location protocol table; callbacks resolve their owners only when invoked. */
    export const locationServices = Object.freeze({
        // --- Forms and guards ---
        /** @brief Request native form formatting through the live presentation owner. */
        locForms(request: WorksheetLocationFormsRequest) {
            return WorksheetRequests.presentation.requestLabForms(request);
        },

        /** @brief Invoke the supplied ownership guard. */
        locCurrent(callback: WorksheetGuard) {
            return callback();
        },

        /** @brief Retain the supplied asynchronous result unchanged. */
        locAwait(promise: Promise<unknown>) {
            return promise;
        },

        // --- Town selection ---
        /** @brief Resolve the current town catalogue provider. */
        locTowns(jurisdiction: string | undefined) {
            return WorksheetLocations.townSelection.townsForJurisdiction(jurisdiction);
        },

        /** @brief Populate a town menu under the supplied ownership guard. */
        locPopulate(select: WorksheetSelect, jurisdiction: string | undefined, options: WorksheetTownOptions = {}) {
            return WorksheetLocations.townSelection.populateTownSelect(select, jurisdiction, options);
        },

        /** @brief Restore town selection through the current controller. */
        locRestore(select: WorksheetSelect, jurisdiction: string | undefined, town: string | undefined, latitude: string | undefined, longitude: string | undefined, current: WorksheetGuard = locationOwnership.current) {
            return WorksheetLocations.townSelection.restoreTownSelection(select, jurisdiction, town, latitude, longitude, current);
        },

        /** @brief Apply the selected town to its controls. */
        locApplyTown(controls: WorksheetTownControls) {
            return townSelection.applySelectedTown(controls);
        },

        /** @brief Select a town using native coordinate matching. */
        locCoordinates(select: WorksheetSelect, latitude: unknown, longitude: unknown) {
            return WorksheetLocations.townSelection.selectTownByCoordinates(select, latitude, longitude);
        },

        /** @brief Clear named-town selection after custom coordinates. */
        locClearCustom(select: WorksheetSelect, latitude: HTMLInputElement, longitude: HTMLInputElement, elevation: HTMLInputElement | undefined) {
            return WorksheetLocations.townSelection.clearTownForCustomCoordinates(select, latitude, longitude, elevation);
        },

        /** @brief Restore town selection while the supplied request remains current. */
        locRestoreTotality(state: WorksheetCalendarState, request: WorksheetRequest) {
            return WorksheetLocations.townSelection.restoreTownSelection(
                WorksheetContext.almanacTown, state.jurisdiction, state.town, state.latitude, state.longitude,
                locationOwnership.requestCurrent.bind(null, request));
        },

        // --- Calendar state ---
        /** @brief Resolve the controls for a calendar mode. */
        locElements(mode: WorksheetMode) {
            return WorksheetLocations.calendar.calendarElements(mode);
        },

        /** @brief Resolve the town controls for a calendar mode. */
        locControls(mode: WorksheetMode, coordinates = true) {
            return calendar.calendarTownControls(mode, coordinates);
        },

        /** @brief Project calendar state through native validation. */
        locApplyState(mode: WorksheetMode, source: unknown, operation = 0, prefix = '') {
            return WorksheetLocations.calendar.applyCalendarState(mode, source, operation, prefix);
        },

        /** @brief Normalise calendar state without projecting controls. */
        locState(mode: WorksheetMode, source: unknown, operation: number, prefix = '') {
            return WorksheetLocations.calendar.calendarState(mode, source, operation, prefix);
        },

        /** @brief Read authored calendar controls. */
        locRead(mode: WorksheetMode) {
            return WorksheetLocations.calendar.readCalendarControls(mode);
        },

        /** @brief Capture the current native calendar context. */
        locContext() {
            return WorksheetLocations.calendar.calendarLocationContext();
        },

        /** @brief Accept flags updated by native calendar projection. */
        locAcceptContext(context: CalendarLocationContext) {
            return WorksheetLocations.calendar.acceptCalendarLocationContext(context);
        },

        /** @brief Project a calendar response without interpreting its values. */
        locResponse(mode: WorksheetMode, data: WorksheetEvaluationData, context: CalendarLocationContext, update: boolean, applied: boolean) {
            return labDOM.call(
                'lab_location_response', WorksheetWorkspace.mode.workspaceModeId(mode), data, context, Number(update), Number(!!applied));
        },

        /** @brief Clear the retained almanac response. */
        locClearAlmanac() {
            return WorksheetContext.worksheetSession.calendar.almanacLastWorksheetData = null;
        },

        // --- Request lifecycle ---
        /** @brief Begin a guarded request using the current request owner. */
        locBegin(operation: string, mode?: string, options: WorksheetRequests.MarsApiOptions = {}) {
            return WorksheetRequests.labRequests.begin(operation, mode, options);
        },

        /** @brief Finish the supplied guarded request. */
        locFinish(request: WorksheetRequest) {
            return WorksheetRequests.labRequests.finish(request);
        },

        // --- Refresh, evaluation and persistence ---
        /** @brief Refresh almanac location controls. */
        locRefreshAlmanac(options: WorksheetCalendarOptions = {}) {
            return calendarActions.refreshAlmanacJurisdictionLocation(options);
        },

        /** @brief Refresh DateTime location controls. */
        locRefreshDatetime(options: WorksheetCalendarOptions = {}) {
            return WorksheetLocations.calendarActions.refreshDatetimeJurisdictionLocation(options);
        },

        /** @brief Evaluate the almanac through the current request owner. */
        locEvaluateAlmanac(options: WorksheetEvaluationOptions = {}) {
            return WorksheetRequests.evaluation.evaluateAlmanac(options);
        },

        /** @brief Evaluate DateTime through the current request owner. */
        locEvaluateDatetime(options: WorksheetEvaluationOptions = {}) {
            return WorksheetRequests.evaluation.evaluateDatetime(options);
        },

        /** @brief Delegate automatic almanac evaluation ordering. */
        locTriggerAlmanac(options: WorksheetAutoEvaluationOptions) {
            return WorksheetLocations.calendarActions.triggerAlmanacAutoEvaluation(options);
        },

        /** @brief Delegate automatic DateTime evaluation ordering. */
        locTriggerDatetime(options: WorksheetAutoEvaluationOptions) {
            return WorksheetLocations.calendarActions.triggerDatetimeAutoEvaluation(options);
        },

        /** @brief Evaluate the current worksheet mode. */
        locEvaluateCurrent() {
            return WorksheetWorkspace.mode.evaluateCurrentMode();
        },

        /** @brief Persist the current almanac controls. */
        locSaveAlmanac() {
            return WorksheetPersistence.persistence.saveCalendar('almanac');
        },

        /** @brief Persist the current DateTime controls. */
        locSaveDatetime() {
            return WorksheetPersistence.persistence.saveCalendar('datetime');
        },

        // --- UI projection ---
        /** @brief Present native workflow status. */
        locStatus(status: string) {
            return WorksheetWorkspace.presentation.setStatus(status);
        },

        /** @brief Present native workflow failure. */
        locError(message: string) {
            return WorksheetResults.cards.setRenderedError(message);
        },

        /** @brief Render native DateTime sections. */
        locRenderLocal(node: HTMLElement, sections: unknown, text: string) {
            return WorksheetResults.calendarResults.renderDatetimeSections(node, null, sections, text);
        },

        /** @brief Apply native local-card visibility policy. */
        locLocalVisible(card: HTMLElement) {
            return labDOM.call('lab_flow_location_visibility', card, WorksheetWorkspace.mode.currentMode());
        },

        /** @brief Hide the supplied local card. */
        locLocalHidden(card: HTMLElement) {
            return card.classList.add('hidden');
        },

        /** @brief Rebuild the menu through its registry-owned controller. */
        locSelectRebuild(select: WorksheetSelect) {
            return selects.rebuild(select);
        },

        /** @brief Synchronise the menu through its registry-owned controller. */
        locSelectSync(select: WorksheetSelect) {
            return selects.syncSelection(select);
        },

        /** @brief Render the current date picker. */
        locRenderPicker() {
            return WorksheetLocations.picker.render();
        },

        // --- Time formatting ---
        /** @brief Capture authored time and current request ownership. */
        locTimeSnapshot() {
            return {
                authored: WorksheetContext.almanacTime.value,
                context: WorksheetRequests.labRequests.context(),
                main: WorksheetRequests.labRequests.latestMain(),
                element: WorksheetContext.almanacTime
            };
        },

        /** @brief Read current main-request ownership. */
        locLatestMain() {
            return WorksheetRequests.labRequests.latestMain();
        },

        /** @brief Request native authored-time formatting. */
        locFormatTime(text: string) {
            return WorksheetLocations.calendarActions.formatAlmanacTimeInput(text);
        },

        /** @brief Apply formatted time and place the caret at its end. */
        locTimeApply(input: HTMLInputElement, text: string) {
            input.value = text;
            input.setSelectionRange(text.length, text.length);
        }
    });

    /** @brief Native menu enhancement options retained for rebuilding. */
    interface WorksheetRoundedSelectOptions {
        searchable?: boolean;
        details?: boolean;
        placeholder?: string;
        emptyText?: string;
        searchPlaceholder?: string;
    }

    /** Browser callback ownership for one native-projected select menu. */
    export class WorksheetRoundedSelect {
        private installed = false;

        /** @brief Retain a menu, native capabilities and the registry's ownership-release callback. */
        constructor(
            private readonly select: WorksheetSelect,
            private readonly options: WorksheetRoundedSelectOptions,
            private readonly native: LocationNativeProvider,
            private readonly document: LocationDocumentProvider,
            private readonly release: (select: WorksheetSelect, controller: WorksheetRoundedSelect) => void
        ) {
            this.sync = this.sync.bind(this);
            this.close = this.close.bind(this);
            this.rebuild = this.rebuild.bind(this);
            this.install = this.install.bind(this);
            this.dispose = this.dispose.bind(this);
        }

        /** @brief Synchronise native menu selection; safe as a detached controller callback. */
        sync(): void { this.native().call('lab_select_sync', this.select); }

        /** @brief Close this menu without affecting any other controller. */
        close(): number { return this.native().call('lab_select_event', this.select, 3, 0, null) as number; }

        /** @brief Rebuild this menu using its original enhancement options. */
        rebuild(): void { this.native().call('lab_select_refresh', this.select, this.options); }

        /** @brief Install native enhancement without adding callback properties to the select element. */
        install(): boolean {
            if (!this.native().call('lab_select_enhance', this.select, this.options, this.document()))
                return false;
            this.installed = true;
            return true;
        }

        /** @brief Close this menu and release its registry entry; repeated disposal is harmless. */
        dispose(): void {
            if (!this.installed) return;
            this.installed = false;
            try {
                this.close();
            } finally {
                this.release(this.select, this);
            }
        }
    }

    /** @brief Own enhanced menus, indexed by element without storing callbacks on the DOM. */
    export class RoundedSelectRegistry {
        private readonly controllers = new WeakMap<WorksheetSelect, WorksheetRoundedSelect>();
        // WeakMap provides direct lookup; the owned set permits deterministic shutdown.
        private readonly owned = new Set<WorksheetRoundedSelect>();
        private readonly services: NativeServiceRegistry;

        /** @brief Inject native projection and the document used to create menus. */
        constructor(
            private readonly native: LocationProjectionProvider,
            private readonly document: LocationDocumentProvider
        ) {
            this.enhance = this.enhance.bind(this);
            this.sync = this.sync.bind(this);
            this.syncSelection = this.syncSelection.bind(this);
            this.rebuild = this.rebuild.bind(this);
            this.dispose = this.dispose.bind(this);
            this.release = this.release.bind(this);
            // Plans must return to this registry, not the page-wide instance.
            this.services = {locSelectRebuild: this.rebuild, locSelectSync: this.syncSelection};
        }

        /** @brief Enhance one select, retaining only successfully installed controllers. */
        enhance(select: WorksheetSelect, options: WorksheetRoundedSelectOptions = {}): WorksheetRoundedSelect | null {
            if (this.controllers.has(select)) return null;
            const controller = new WorksheetRoundedSelect(select, options, this.native, this.document, this.release);
            if (!controller.install()) return null;
            this.controllers.set(select, controller);
            this.owned.add(controller);
            return controller;
        }

        /** @brief Execute native-selected synchronisation or rebuilding for a registered menu. */
        sync(select: WorksheetSelect): void {
            const available = this.controllers.has(select);
            const native = this.native();
            const plan = native.call('lab_flow_location_select', select, {rebuild: available, sync: available});
            native.services(plan as NativeServicePlan, this.services);
        }

        /** @brief Synchronise selection directly; unregistered or disposed menus are ignored. */
        syncSelection(select: WorksheetSelect): void {
            this.controllers.get(select)?.sync();
        }

        /** @brief Rebuild directly with the retained options; unregistered or disposed menus are ignored. */
        rebuild(select: WorksheetSelect): void {
            this.controllers.get(select)?.rebuild();
        }

        /** @brief Release only menus installed through this registry. */
        dispose(): void {
            for (const controller of this.owned) controller.dispose();
        }

        private release(select: WorksheetSelect, controller: WorksheetRoundedSelect): void {
            if (this.controllers.get(select) === controller) this.controllers.delete(select);
            this.owned.delete(controller);
        }
    }

    /** @brief Page-owned native menu controllers with explicit shutdown. */
    export const selects: RoundedSelectRegistry = new RoundedSelectRegistry(locationHosts.native, locationHosts.document);

    // Browser date conversion and clock access; C owns picker policy and DOM projection.
    // Browser form controls perform text conversion; C receives structured numbers only.

    /** @brief Focus restoration requested when closing the picker. */
    interface WorksheetPickerCloseOptions { restoreFocus?: boolean; }

    /** @brief Commit policy for picker navigation. */
    interface WorksheetPickerMoveOptions { commit?: boolean; }

    /** @brief Injectable browser capabilities, resolved only when used. */
    interface DatePickerDependencies {
        state(): WorksheetPickerState;
        native(): LocationProjection;
        dates(): PickerDates;
        viewport(): {width: number; height: number};
        clock(): Date;
        services(): NativeServiceRegistry;
    }

    /** Browser references for the native popup; navigation and commit policy remain in C. */
    export class WorksheetDatePickerController {
        /** @brief Retain lazy popup state and browser capabilities without assuming bootstrap order. */
        constructor(private readonly dependencies: DatePickerDependencies) {
            this.todayForInput = this.todayForInput.bind(this);
            this.commit = this.commit.bind(this);
            this.commitToday = this.commitToday.bind(this);
            this.close = this.close.bind(this);
            this.place = this.place.bind(this);
            this.render = this.render.bind(this);
            this.open = this.open.bind(this);
            this.setMonthYear = this.setMonthYear.bind(this);
            this.shift = this.shift.bind(this);
            this.dispose = this.dispose.bind(this);
            this.shiftYear = this.shiftYear.bind(this);
            this.shiftMonth = this.shiftMonth.bind(this);
        }

        /** @brief Choose the local or UTC date using the native input policy. */
        todayForInput(input: HTMLInputElement | null): string {
            const dates = this.dependencies.dates();
            return this.dependencies.native().call('lab_widgets_picker_today_date', input, dates.marsTodayIsoDate(), dates.marsCurrentGmtMoment().date) as string;
        }

        /** @brief Commit authored date text, reporting whether native validation changed the value. */
        commit(input: HTMLInputElement, value: unknown): boolean {
            return !!this.dependencies.native().call('lab_widgets_date_commit', input, String(value));
        }

        /** @brief Commit today's native-selected date and any associated UTC time. */
        commitToday(input: HTMLInputElement): void {
            const dates = this.dependencies.dates();
            this.dependencies.native().call('lab_widgets_picker_today', input, dates.marsTodayIsoDate(), dates.marsCurrentGmtMoment());
        }

        /** @brief Close the owned popup, optionally restoring focus. */
        close({restoreFocus = false}: WorksheetPickerCloseOptions = {}): void {
            this.dependencies.native().call('lab_widgets_picker_close', this.dependencies.state(), Number(restoreFocus));
        }

        /** @brief Position the popup using native geometry and the live viewport. */
        place(shell: HTMLElement): void {
            this.dependencies.native().call('lab_widgets_picker_place', this.dependencies.state(), shell, this.dependencies.viewport().width, this.dependencies.viewport().height);
        }

        /** @brief Project the current native picker without reopening a closed popup. */
        render(): void {
            const state = this.dependencies.state();
            const input = state.input;
            this.dependencies.native().call(
                'lab_widgets_picker_render', state, this.dependencies.dates().parseMarsIsoDate(input?.value),
                this.dependencies.dates().parseMarsIsoDate(this.todayForInput(input)), this.dependencies.viewport().width, this.dependencies.viewport().height);
        }

        /** @brief Transfer popup ownership to the supplied input and button. */
        open(input: HTMLInputElement, button: WorksheetCopyButton): void {
            this.dependencies.native().call(
                'lab_widgets_picker_open', this.dependencies.state(), input, button, this.dependencies.dates().parseMarsIsoDate(input?.value),
                this.dependencies.dates().parseMarsIsoDate(this.todayForInput(input)), this.dependencies.clock().getFullYear(), this.dependencies.viewport().width,
                this.dependencies.viewport().height);
        }

        /** @brief Select a month and year with optional native commit. */
        setMonthYear(year: number, month: number, {commit = false}: WorksheetPickerMoveOptions = {}): void {
            this.move(0, this.dependencies.dates().marsFormNumber(year), this.dependencies.dates().marsFormNumber(month), commit);
        }

        /** @brief Move by a month or year delta using native clamping. */
        shift(delta: number, years: boolean): void {
            this.move(years ? 2 : 1, delta, 0, true);
        }

        /** @brief Close this controller's popup during host shutdown. */
        dispose(): void { this.close(); }

        /** @brief Shift the native picker by a year delta. */
        shiftYear(delta: number): void { this.shift(delta, true); }

        /** @brief Shift the native picker by a month delta. */
        shiftMonth(delta: number): void { this.shift(delta, false); }

        private move(operation: 0 | 1 | 2, first: number, second: number, commit: boolean): void {
            const state = this.dependencies.state();
            const moved = this.dependencies.native().call(
                'lab_widgets_picker_move', state, operation, first, second,
                this.dependencies.dates().parseMarsIsoDate(state.input?.value), this.dependencies.clock().getFullYear(), Number(!!commit)) as number;
            this.dependencies.native().services(this.dependencies.native().call('lab_flow_location_picker', Number(!!moved)) as NativeServicePlan, this.dependencies.services());
        }
    }

    // The script is loaded before workspace state; resolve it only when an action runs.

    /** @brief Shared popup owner; calendar policy and date validation remain native. */
    export const picker: WorksheetDatePickerController = new WorksheetDatePickerController(locationHosts);

    /** @brief Injectable browser capabilities, resolved only when used. */
    interface TownSelectionControllerDependencies {
        flow(): LocationFlow;
        calendar(): LocationCalendar;
        dates(): LocationDates;
        native(): LocationNative;
    }

    /** @brief Town menus and guarded selection; native code owns matching and restoration. */
    export class TownSelectionController {
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: TownSelectionControllerDependencies) {
            this.townsForJurisdiction = this.townsForJurisdiction.bind(this);
            this.selectTownByCoordinates = this.selectTownByCoordinates.bind(this);
            this.restoreTownSelection = this.restoreTownSelection.bind(this);
            this.populateTownSelect = this.populateTownSelect.bind(this);
            this.selectedTownOption = this.selectedTownOption.bind(this);
            this.clearTownForCustomCoordinates = this.clearTownForCustomCoordinates.bind(this);
            this.applySelectedTown = this.applySelectedTown.bind(this);
            this.syncTownSelectors = this.syncTownSelectors.bind(this);
        }

        /** @brief Read the native-selected town catalogue for a jurisdiction. */
        townsForJurisdiction(jurisdiction: string | undefined) {
            return this.dependencies.native().call('lab_location_towns', this.dependencies.calendar().calendarLocationContext(), {value: jurisdiction}) as unknown[];
        }

        /** @brief Select a town using the native coordinate-matching policy. */
        selectTownByCoordinates(select: WorksheetSelect, latitude: unknown, longitude: unknown) {
            return Boolean(this.dependencies.native().call('lab_location_coordinates', select, {latitude, longitude}) as number);
        }

        /** @brief Restore a saved town; return null when ownership has changed. */
        restoreTownSelection(select: WorksheetSelect, jurisdiction: string | undefined, townValue: string | undefined, latitude: string | undefined, longitude: string | undefined, isCurrent: WorksheetGuard = locationOwnership.current): Promise<boolean | null> {
            return this.dependencies.flow().continue(27, {select, jurisdiction, selection: {value: townValue, latitude, longitude}, isCurrent}) as Promise<boolean | null>;
        }

        /** @brief Populate a town menu under the supplied ownership guard. */
        populateTownSelect(select: WorksheetSelect, jurisdiction: string | undefined, {selectDefault = true, isCurrent = locationOwnership.current}: WorksheetTownOptions = {}): Promise<unknown[]> {
            return this.dependencies.flow().continue(28, {select, jurisdiction, selectDefault, isCurrent}) as Promise<unknown[]>;
        }

        /** @brief Return the native-selected option, or null for a custom location. */
        selectedTownOption(select: WorksheetSelect | undefined) {
            return this.dependencies.native().call('lab_location_selected', select) as HTMLOptionElement | null;
        }

        /** @brief Clear named-town selection after authored coordinate changes. */
        clearTownForCustomCoordinates(townSelect: WorksheetSelect, latitudeInput: HTMLInputElement, longitudeInput: HTMLInputElement, elevationInput: HTMLInputElement | undefined) {
            this.dependencies.native().call('lab_location_clear_custom', townSelect, latitudeInput, longitudeInput, elevationInput);
        }

        /** @brief Apply a selected town and its browser time-zone offset, accepting native flag changes. */
        applySelectedTown(controls: WorksheetTownControls = {}) {
            const option = this.selectedTownOption(controls.townSelect);
            const offset = this.dependencies.dates().formatOffsetHours(this.dependencies.dates().timeZoneOffsetHours(option?.dataset.timezone, controls.dateInput?.value));
            const context = this.dependencies.calendar().calendarLocationContext();
            const applied = this.dependencies.native().call('lab_location_apply_town', controls, offset, context) as number;
            this.dependencies.calendar().acceptCalendarLocationContext(context);
            return Boolean(applied);
        }

        /** @brief Populate both calendar town menus through the native continuation. */
        syncTownSelectors({selectDefault = false}: TownSyncOptions = {}): Promise<void> {
            return this.dependencies.flow().continue(29, {selectDefault}) as Promise<void>;
        }
    }

    /** @brief Town menus and guarded selection; native code owns matching and restoration. */
    export const townSelection: TownSelectionController = new TownSelectionController(locationHosts);

    /** @brief Injectable browser capabilities, resolved only when used. */
    interface CalendarStateControllerDependencies {
        context(): CalendarHostContext;
        mode(): CalendarMode;
        configuration(): LabConfiguration;
        native(): LocationNative;
    }

    /** @brief Live calendar controls and session flags; captures native contexts without duplicating state. */
    export class CalendarStateController {
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: CalendarStateControllerDependencies) {
            this.markDatetimeOffsetTouched = this.markDatetimeOffsetTouched.bind(this);
            this.calendarLocationContext = this.calendarLocationContext.bind(this);
            this.acceptCalendarLocationContext = this.acceptCalendarLocationContext.bind(this);
            this.readCalendarControls = this.readCalendarControls.bind(this);
            this.calendarState = this.calendarState.bind(this);
            this.applyCalendarState = this.applyCalendarState.bind(this);
            this.calendarElements = this.calendarElements.bind(this);
            this.calendarTownControls = this.calendarTownControls.bind(this);
            this.currentCalendarState = this.currentCalendarState.bind(this);
            this.applyCalendarEvaluationFields = this.applyCalendarEvaluationFields.bind(this);
            this.restoreDatetimeDefaultsIfBlank = this.restoreDatetimeDefaultsIfBlank.bind(this);
            this.restoreAlmanacDefaultsIfBlank = this.restoreAlmanacDefaultsIfBlank.bind(this);
            this.currentDatetimeState = this.currentDatetimeState.bind(this);
            this.currentAlmanacState = this.currentAlmanacState.bind(this);
            this.almanacSummaryText = this.almanacSummaryText.bind(this);
            this.datetimeSummaryText = this.datetimeSummaryText.bind(this);
        }

        /** @brief Preserve an authored offset against subsequent native location defaults. */
        markDatetimeOffsetTouched(): void {
            const calendar = this.dependencies.context().worksheetSession.calendar;
            calendar.datetimeGmtOffsetTouched = true;
        }

        /** @brief Capture the catalogue and current browser-owned calendar flags for native projection. */
        calendarLocationContext(): CalendarLocationContext {
            const config = this.dependencies.configuration();
            const context = this.dependencies.context();
            const calendar = context.worksheetSession.calendar;
            return {
                config,
                jurisdictions: context.HOLIDAY_JURISDICTION_SET,
                towns: context.JURISDICTION_TOWN_OPTIONS,
                visibility: calendar.almanacVisibilityMode,
                automaticOffset: calendar.datetimeAutoGmtOffset,
                offsetTouched: calendar.datetimeGmtOffsetTouched
            };
        }

        /** @brief Accept calendar flags updated by a completed native projection. */
        acceptCalendarLocationContext(context: CalendarLocationContext) {
            const calendar = this.dependencies.context().worksheetSession.calendar;
            calendar.almanacVisibilityMode = context.visibility;
            calendar.datetimeAutoGmtOffset = context.automaticOffset;
            calendar.datetimeGmtOffsetTouched = context.offsetTouched;
        }

        /** @brief Read the named worksheet controls using the native calendar schema. */
        readCalendarControls(mode: WorksheetMode) {
            return this.dependencies.native().call('lab_location_read', this.dependencies.mode().workspaceModeId(mode), this.calendarLocationContext()) as WorksheetCalendarState;
        }

        /** @brief Normalise calendar state without changing controls; reject unsupported operations. */
        calendarState(mode: WorksheetMode, source: unknown, operation: number, prefix = '') {
            const state = this.dependencies.native().call(
                'lab_location_state', this.dependencies.mode().workspaceModeId(mode), operation, source, prefix, this.calendarLocationContext(), 0) as WorksheetCalendarState | null;
            if (!state)
                throw new Error('Invalid calendar state operation');
            return state;
        }

        /** @brief Project native-normalised calendar state and accept its updated context. */
        applyCalendarState(mode: WorksheetMode, source: unknown, operation = 0, prefix = '') {
            const context = this.calendarLocationContext();
            const state = this.dependencies.native().call('lab_location_state', this.dependencies.mode().workspaceModeId(mode), operation, source, prefix, context, 1) as WorksheetCalendarState | null;
            if (!state)
                throw new Error('Invalid calendar state operation');
            this.acceptCalendarLocationContext(context);
            return state;
        }

        /** @brief Resolve browser controls using the native worksheet schema. */
        calendarElements(mode: WorksheetMode) {
            return this.dependencies.native().call('lab_location_elements', this.dependencies.mode().workspaceModeId(mode)) as CalendarElements;
        }

        /** @brief Resolve the town-control group selected by the native calendar schema. */
        calendarTownControls(mode: WorksheetMode, coordinates = true) {
            return this.dependencies.native().call('lab_location_controls', this.dependencies.mode().workspaceModeId(mode), Number(coordinates)) as WorksheetTownControls;
        }

        /** @brief Read current native calendar state and accept its updated flags. */
        currentCalendarState(mode: WorksheetMode) {
            const context = this.calendarLocationContext();
            const state = this.dependencies.native().call('lab_location_current', this.dependencies.mode().workspaceModeId(mode), context) as WorksheetCalendarState;
            this.acceptCalendarLocationContext(context);
            return state;
        }

        /** @brief Apply native-validated response fields without interpreting their values. */
        applyCalendarEvaluationFields(mode: WorksheetMode, data: CalendarEvaluation) {
            const context = this.calendarLocationContext();
            this.dependencies.native().call('lab_location_evaluation', this.dependencies.mode().workspaceModeId(mode), data, context);
            this.acceptCalendarLocationContext(context);
        }

        /** @brief Apply native DateTime defaults only to blank controls. */
        restoreDatetimeDefaultsIfBlank() {
            this.applyCalendarState('datetime', this.readCalendarControls('datetime'), 2);
        }

        /** @brief Apply native almanac defaults only to blank controls. */
        restoreAlmanacDefaultsIfBlank() {
            this.applyCalendarState('almanac', this.readCalendarControls('almanac'), 2);
        }

        /** @brief Capture the native-normalised current DateTime worksheet. */
        currentDatetimeState() {
            return this.currentCalendarState('datetime');
        }

        /** @brief Capture the native-normalised current almanac worksheet. */
        currentAlmanacState() {
            return this.currentCalendarState('almanac');
        }

        /** @brief Obtain native summary text for the supplied almanac state. */
        almanacSummaryText(state = this.currentAlmanacState()) {
            return this.dependencies.native().call('lab_location_summary', 6, state, this.dependencies.context().ALMANAC_WORKSHEET_TITLE) as string;
        }

        /** @brief Obtain native summary text for the supplied DateTime state. */
        datetimeSummaryText(state = this.currentDatetimeState()) {
            return this.dependencies.native().call('lab_location_summary', 5, state, '') as string;
        }
    }

    /** @brief Live calendar controls and session flags; captures native contexts without duplicating state. */
    export const calendar: CalendarStateController = new CalendarStateController(locationHosts);

    /** @brief Injectable browser capabilities, resolved only when used. */
    interface CalendarActionControllerDependencies {
        flow(): LocationFlow;
    }

    /** @brief Calendar commands delegated to native continuations, preserving request ownership and ordering. */
    export class CalendarActionController {
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: CalendarActionControllerDependencies) {
            this.formatAlmanacTimeInput = this.formatAlmanacTimeInput.bind(this);
            this.restoreCalendarHistory = this.restoreCalendarHistory.bind(this);
            this.applyAlmanacTotalityAction = this.applyAlmanacTotalityAction.bind(this);
            this.refreshCalendarJurisdictionLocation = this.refreshCalendarJurisdictionLocation.bind(this);
            this.refreshDatetimeJurisdictionLocation = this.refreshDatetimeJurisdictionLocation.bind(this);
            this.refreshAlmanacJurisdictionLocation = this.refreshAlmanacJurisdictionLocation.bind(this);
            this.triggerDatetimeAutoEvaluation = this.triggerDatetimeAutoEvaluation.bind(this);
            this.triggerAlmanacAutoEvaluation = this.triggerAlmanacAutoEvaluation.bind(this);
            this.formatAlmanacTimeFromEvent = this.formatAlmanacTimeFromEvent.bind(this);
            this.applyCalendarControlEvent = this.applyCalendarControlEvent.bind(this);
        }

        /** @brief Request native normalisation of an authored almanac time. */
        formatAlmanacTimeInput(value: unknown): Promise<string> {
            return this.dependencies.flow().continue(26, {action: 0, value}) as Promise<string>;
        }

        /** @brief Restore calendar controls and town selection under an ownership guard. */
        restoreCalendarHistory(mode: WorksheetMode, source: unknown, isCurrent: WorksheetGuard = locationOwnership.current): Promise<void> {
            return this.dependencies.flow().continue(30, {mode, source, isCurrent}) as Promise<void>;
        }

        /** @brief Run the native location and evaluation continuation for a totality action. */
        applyAlmanacTotalityAction(button: WorksheetCopyButton): Promise<void> {
            return this.dependencies.flow().continue(31, {button}) as Promise<void>;
        }

        /** @brief Refresh location data through native policy, optionally replacing coordinates. */
        refreshCalendarJurisdictionLocation(mode: WorksheetMode, {updateCoordinates = true}: WorksheetCalendarOptions = {}): Promise<void> {
            return this.dependencies.flow().continue(32, {mode, updateCoordinates}) as Promise<void>;
        }

        /** @brief Refresh DateTime location data with optional coordinate replacement. */
        refreshDatetimeJurisdictionLocation(options: WorksheetCalendarOptions = {}) {
            return this.refreshCalendarJurisdictionLocation('datetime', options);
        }

        /** @brief Refresh the almanac location without duplicating the native request policy. */
        refreshAlmanacJurisdictionLocation(options: WorksheetCalendarOptions = {}) {
            return this.refreshCalendarJurisdictionLocation('almanac', options);
        }

        /** @brief Delegate DateTime refresh, persistence and evaluation ordering to C. */
        triggerDatetimeAutoEvaluation({refreshJurisdiction = false, refreshCoordinates = false}: WorksheetAutoEvaluationOptions = {}): Promise<void> {
            return this.dependencies.flow().continue(33, {mode: 'datetime', options: {refreshJurisdiction, refreshCoordinates}}) as Promise<void>;
        }

        /** @brief Delegate almanac refresh, persistence and evaluation ordering to C. */
        triggerAlmanacAutoEvaluation({refreshJurisdiction = false, refreshCoordinates = false}: WorksheetAutoEvaluationOptions = {}): Promise<void> {
            return this.dependencies.flow().continue(33, {mode: 'almanac', options: {refreshJurisdiction, refreshCoordinates}}) as Promise<void>;
        }

        /** @brief Format time through native services, retaining request and authored-input ownership. */
        formatAlmanacTimeFromEvent(): Promise<void> {
            return this.dependencies.flow().continue(34, {}) as Promise<void>;
        }

        /** @brief Deliver native calendar actions to asynchronous location and evaluation services. */
        applyCalendarControlEvent(mode: WorksheetMode, actions: number): void {
            void this.dependencies.flow().continue(35, {mode, actions});
        }
    }

    /** @brief Calendar commands delegated to native continuations, preserving request ownership and ordering. */
    export const calendarActions: CalendarActionController = new CalendarActionController(locationHosts);

    /** @brief Injectable browser capabilities, resolved only when used. */
    interface BrowserDateAdapterDependencies {
        wire(): DateWire;
        document(): DateDocument;
        clock(): Date;
        dateTimeFormat: typeof Intl.DateTimeFormat;
    }

    /** @brief Browser date conversion and clock access with native validation and arithmetic. */
    export class BrowserDateAdapter {
        /** @brief Retain narrow capabilities; live browser state is resolved only when used. */
        constructor(private readonly dependencies: BrowserDateAdapterDependencies) {
            this.timeZoneOffsetHours = this.timeZoneOffsetHours.bind(this);
            this.formatOffsetHours = this.formatOffsetHours.bind(this);
            this.marsFormNumber = this.marsFormNumber.bind(this);
            this.parseMarsIsoDate = this.parseMarsIsoDate.bind(this);
            this.marsTodayIsoDate = this.marsTodayIsoDate.bind(this);
            this.marsCurrentGmtMoment = this.marsCurrentGmtMoment.bind(this);
        }

        /** @brief Use Intl and native date arithmetic to obtain an offset, or null when unavailable. */
        timeZoneOffsetHours(timeZone: string | undefined, dateText: string | undefined): number | null {
            if (!timeZone)
                return null;
            const parsed = this.parseMarsIsoDate(dateText);
            if (!parsed)
                return null;
            const forms = this.dependencies.wire().exports();
            const probe = new Date(forms.lab_forms_utc_milliseconds(parsed.year, parsed.month, parsed.day, 12, 0, 0));
            if (Number.isNaN(probe.getTime()))
                return null;
            try {
                const formatter = new this.dependencies.dateTimeFormat('en-GB', {
                    timeZone,
                    hour12: false,
                    year: 'numeric',
                    month: '2-digit',
                    day: '2-digit',
                    hour: '2-digit',
                    minute: '2-digit',
                    second: '2-digit'
                });
                const parts: DateTextParts = {};
                for (const part of formatter.formatToParts(probe))
                    parts[part.type] = part.value;
                const localAsUtc = forms.lab_forms_utc_milliseconds(
                    this.marsFormNumber(parts.year), this.marsFormNumber(parts.month), this.marsFormNumber(parts.day),
                    this.marsFormNumber(parts.hour), this.marsFormNumber(parts.minute), this.marsFormNumber(parts.second));
                const offset = forms.lab_forms_offset_hours(localAsUtc, probe.getTime());
                return Number.isFinite(offset) ? offset : null;
            } catch (_) {
                return null;
            }
        }

        /** @brief Format a finite offset after native rounding; absent offsets produce empty text. */
        formatOffsetHours(offset: number | null): string {
            if (offset === null || !Number.isFinite(offset))
                return '';
            const rounded = this.dependencies.wire().exports().lab_forms_round_offset(offset);
            return Number.isFinite(rounded) ? String(rounded) : '';
        }

        /** @brief Convert a browser form value to a number using an input control. */
        marsFormNumber(value: unknown) {
            if (typeof value === 'number')
                return value;
            // Match number-input parsing: blank or invalid text is NaN, unlike Number('').
            const input = this.dependencies.document().createElement('input');
            input.type = 'number';
            input.value = String(value ?? '').trim();
            return input.valueAsNumber;
        }

        /** @brief Parse a browser date value and validate its structured parts in C. */
        parseMarsIsoDate(text: unknown) {
            const input = this.dependencies.document().createElement('input');
            input.type = 'date';
            input.value = String(text || '').trim();
            const date = input.valueAsDate;
            if (!date)
                return null;
            const year = date.getUTCFullYear(), month = date.getUTCMonth() + 1, day = date.getUTCDate();
            return this.dependencies.wire().exports().lab_forms_date_valid(year, month, day) ? {year, month, day} : null;
        }

        private isoDate(year: number, month: number, day: number): string {
            return `${String(year).padStart(4, '0')}-${String(month).padStart(2, '0')}-${String(day).padStart(2, '0')}`;
        }

        private clockTime(hours: number, minutes: number, seconds: number): string {
            return `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
        }

        /** @brief Return the current local calendar date in ISO notation. */
        marsTodayIsoDate() {
            const now = this.dependencies.clock();
            return this.isoDate(now.getFullYear(), now.getMonth() + 1, now.getDate());
        }

        /** @brief Return the UTC date and time for the supplied browser clock value. */
        marsCurrentGmtMoment(now = this.dependencies.clock()) {
            return {
                date: this.isoDate(now.getUTCFullYear(), now.getUTCMonth() + 1, now.getUTCDate()),
                time: this.clockTime(now.getUTCHours(), now.getUTCMinutes(), now.getUTCSeconds())
            };
        }
    }

    /** @brief Browser date conversion and clock access with native validation and arithmetic. */
    export const dates: BrowserDateAdapter = new BrowserDateAdapter(locationHosts);

}
