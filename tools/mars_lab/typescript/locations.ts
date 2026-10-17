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

    // Resolve browser-owned controls and application callbacks only when a native continuation executes them.

    /** @brief Native location protocol table; callbacks resolve their owners only when invoked. */
    export const locationServices = Object.freeze({
            locForms: (request: WorksheetLocationFormsRequest) => WorksheetRequests.presentation.requestLabForms(request),
            locCurrent: (callback: WorksheetGuard) => callback(),
            locAwait: (promise: Promise<unknown>) => promise,
            locTowns: (jurisdiction: string | undefined) => WorksheetLocations.townSelection.townsForJurisdiction(jurisdiction),
            locPopulate: (...args: Parameters<typeof WorksheetLocations.townSelection.populateTownSelect>) => WorksheetLocations.townSelection.populateTownSelect(...args),
            locRestore: (...args: Parameters<typeof WorksheetLocations.townSelection.restoreTownSelection>) => WorksheetLocations.townSelection.restoreTownSelection(...args),
            locApplyTown: (controls: WorksheetTownControls) => townSelection.applySelectedTown(controls),
            locElements: (mode: WorksheetMode) => WorksheetLocations.calendar.calendarElements(mode),
            locControls: (...args: Parameters<typeof calendar.calendarTownControls>) => calendar.calendarTownControls(...args),
            locApplyState: (...args: Parameters<typeof WorksheetLocations.calendar.applyCalendarState>) => WorksheetLocations.calendar.applyCalendarState(...args),
            locState: (...args: Parameters<typeof WorksheetLocations.calendar.calendarState>) => WorksheetLocations.calendar.calendarState(...args),
            locRead: (mode: WorksheetMode) => WorksheetLocations.calendar.readCalendarControls(mode),
            locContext: () => WorksheetLocations.calendar.calendarLocationContext(),
            locAcceptContext: (context: ReturnType<typeof WorksheetLocations.calendar.calendarLocationContext>) => WorksheetLocations.calendar.acceptCalendarLocationContext(context),
            locBegin: (...args: Parameters<typeof WorksheetRequests.labRequests.begin>) => WorksheetRequests.labRequests.begin(...args),
            locFinish: (request: WorksheetRequest) => WorksheetRequests.labRequests.finish(request),
            locRestoreTotality: (state: WorksheetCalendarState, request: WorksheetRequest) => WorksheetLocations.townSelection.restoreTownSelection(
                WorksheetContext.almanacTown, state.jurisdiction, state.town, state.latitude, state.longitude,
                () => WorksheetRequests.labRequests.current(request)),
            locSaveAlmanac: () => WorksheetPersistence.persistence.saveCalendar('almanac'),
            locSaveDatetime: () => WorksheetPersistence.persistence.saveCalendar('datetime'),
            locEvaluateCurrent: () => WorksheetWorkspace.mode.evaluateCurrentMode(),
            locStatus: (status: string) => WorksheetWorkspace.presentation.setStatus(status),
            locError: (message: string) => WorksheetResults.cards.setRenderedError(message),
            locCoordinates: (...args: Parameters<typeof WorksheetLocations.townSelection.selectTownByCoordinates>) => WorksheetLocations.townSelection.selectTownByCoordinates(...args),
            locResponse: (mode: WorksheetMode, data: WorksheetEvaluationData, context: ReturnType<typeof WorksheetLocations.calendar.calendarLocationContext>, update: boolean, applied: boolean) => labDOM.call<void>(
                'lab_location_response', WorksheetWorkspace.mode.workspaceModeId(mode), data, context, Number(update), Number(!!applied)),
            locClearAlmanac: () => WorksheetContext.worksheetSession.calendar.almanacLastWorksheetData = null,
            locRefreshAlmanac: (options: WorksheetCalendarOptions = {}) => calendarActions.refreshAlmanacJurisdictionLocation(options),
            locRefreshDatetime: (options: WorksheetCalendarOptions = {}) => WorksheetLocations.calendarActions.refreshDatetimeJurisdictionLocation(options),
            locEvaluateAlmanac: (options: WorksheetEvaluationOptions = {}) => WorksheetRequests.evaluation.evaluateAlmanac(options),
            locEvaluateDatetime: (options: WorksheetEvaluationOptions = {}) => WorksheetRequests.evaluation.evaluateDatetime(options),
            locTriggerAlmanac: (options: WorksheetAutoEvaluationOptions) => WorksheetLocations.calendarActions.triggerAlmanacAutoEvaluation(options),
            locTriggerDatetime: (options: WorksheetAutoEvaluationOptions) => WorksheetLocations.calendarActions.triggerDatetimeAutoEvaluation(options),
            locTimeSnapshot: () => ({authored: WorksheetContext.almanacTime.value, context: WorksheetRequests.labRequests.context(),
                                    main: WorksheetRequests.labRequests.latestMain(), element: WorksheetContext.almanacTime}),
            locLatestMain: () => WorksheetRequests.labRequests.latestMain(),
            locFormatTime: (text: string) => WorksheetLocations.calendarActions.formatAlmanacTimeInput(text),
            locTimeApply: (input: HTMLInputElement, text: string) => {
                input.value = text;
                input.setSelectionRange(text.length, text.length);
            },
            locClearCustom: (...args: Parameters<typeof WorksheetLocations.townSelection.clearTownForCustomCoordinates>) => WorksheetLocations.townSelection.clearTownForCustomCoordinates(...args),
            locRenderLocal: (node: HTMLElement, sections: unknown, text: string) => WorksheetResults.calendarResults.renderDatetimeSections(node, null, sections, text),
            locLocalVisible: (card: HTMLElement) => labDOM.call<void>('lab_flow_location_visibility', card, WorksheetWorkspace.mode.currentMode()),
            locLocalHidden: (card: HTMLElement) => card.classList.add('hidden'),
            locSelectRebuild: (select: WorksheetSelect) => select.__marsRebuildRoundedSelect?.(),
            locSelectSync: (select: WorksheetSelect) => select.__marsSyncRoundedSelect?.(),
            locRenderPicker: () => WorksheetLocations.picker.render()
    });

    interface WorksheetRoundedSelectOptions {
        searchable?: boolean;
        details?: boolean;
        placeholder?: string;
        emptyText?: string;
        searchPlaceholder?: string;
    }

    /** Browser callback ownership for one native-projected select menu. */
    export class WorksheetRoundedSelect {
        /** @brief Retain one menu's node, options and native projection capability. */
        constructor(
            private readonly select: WorksheetSelect,
            private readonly options: WorksheetRoundedSelectOptions,
            private readonly native: () => Pick<typeof labDOM, 'call'>,
            private readonly document: () => Document
        ) {
            this.install = this.install.bind(this);
            this.dispose = this.dispose.bind(this);
        }

        /** @brief Synchronise native menu selection; safe as a detached element callback. */
        readonly sync = (): void => this.native().call<void>('lab_select_sync', this.select);
        /** @brief Close this menu without affecting any other controller. */
        readonly close = (): number => this.native().call<number>('lab_select_event', this.select, 3, 0, null);
        /** @brief Rebuild this menu using its original enhancement options. */
        readonly rebuild = (): void => this.native().call<void>('lab_select_refresh', this.select, this.options);

        /** @brief Install callbacks only when native enhancement accepts the node. */
        install(): boolean {
            if (!this.native().call<number>('lab_select_enhance', this.select, this.options, this.document()))
                return false;
            this.select.__marsSyncRoundedSelect = this.sync;
            this.select.__marsRebuildRoundedSelect = this.rebuild;
            return true;
        }

        /** @brief Close the menu and remove only callbacks installed by this controller. */
        dispose(): void {
            this.close();
            if (this.select.__marsSyncRoundedSelect === this.sync) delete this.select.__marsSyncRoundedSelect;
            if (this.select.__marsRebuildRoundedSelect === this.rebuild) delete this.select.__marsRebuildRoundedSelect;
        }
    }

    /** @brief Own the lifetime of successfully enhanced select menus. */
    export class RoundedSelectRegistry {
        private readonly owned = new Set<WorksheetRoundedSelect>();

        /** @brief Inject native services and the document used to create menus. */
        constructor(
            private readonly native: () => Pick<typeof labDOM, 'call' | 'services'>,
            private readonly document: () => Document,
            private readonly services: () => NativeServiceRegistry
        ) {
            this.enhance = this.enhance.bind(this);
            this.sync = this.sync.bind(this);
            this.dispose = this.dispose.bind(this);
        }

        /** @brief Enhance one select, retaining only successfully installed controllers. */
        enhance(select: WorksheetSelect, options: WorksheetRoundedSelectOptions = {}): WorksheetRoundedSelect | null {
            const controller = new WorksheetRoundedSelect(select, options, this.native, this.document);
            if (!controller.install()) return null;
            this.owned.add(controller);
            return controller;
        }

        /** @brief Execute native-selected synchronisation or rebuilding for a menu. */
        sync(select: WorksheetSelect): void {
            const capabilities = {
                get rebuild() { return typeof select.__marsRebuildRoundedSelect === 'function'; },
                get sync() { return typeof select.__marsSyncRoundedSelect === 'function'; }
            };
            const native = this.native();
            native.services(native.call<NativeServicePlan>('lab_flow_location_select', select, capabilities), this.services());
        }

        /** @brief Release only menus installed through this registry. */
        dispose(): void {
            for (const controller of this.owned) controller.dispose();
            this.owned.clear();
        }
    }

    /** @brief Page-owned native menu controllers with explicit shutdown. */
    export const selects: RoundedSelectRegistry = new RoundedSelectRegistry(() => labDOM, () => document, () => locationServices);

    // Browser date conversion and clock access; C owns picker policy and DOM projection.
    // Browser form controls perform text conversion; C receives structured numbers only.

    function marsIsoDate(year: number, month: number, day: number) {
        return `${String(year).padStart(4, '0')}-${String(month).padStart(2, '0')}-${String(day).padStart(2, '0')}`;
    }

    function marsClockTime(hours: number, minutes: number, seconds: number) {
        return `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
    }

    interface WorksheetPickerCloseOptions { restoreFocus?: boolean; }
    interface WorksheetPickerMoveOptions { commit?: boolean; }

    interface DatePickerDependencies {
        state: () => WorksheetPickerState;
        native: () => Pick<typeof labDOM, 'call' | 'services'>;
        dates: () => Pick<BrowserDateAdapter, 'parseMarsIsoDate' | 'marsFormNumber' | 'marsTodayIsoDate' | 'marsCurrentGmtMoment'>;
        viewport: () => {width: number; height: number};
        clock: () => Date;
        services: () => NativeServiceRegistry;
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
            return this.dependencies.native().call<string>('lab_widgets_picker_today_date', input, dates.marsTodayIsoDate(), dates.marsCurrentGmtMoment().date);
        }

        /** @brief Commit authored date text, reporting whether native validation changed the value. */
        commit(input: HTMLInputElement, value: unknown): boolean {
            return !!this.dependencies.native().call<number>('lab_widgets_date_commit', input, String(value));
        }

        /** @brief Commit today's native-selected date and any associated UTC time. */
        commitToday(input: HTMLInputElement): void {
            const dates = this.dependencies.dates();
            this.dependencies.native().call<void>('lab_widgets_picker_today', input, dates.marsTodayIsoDate(), dates.marsCurrentGmtMoment());
        }

        /** @brief Close the owned popup, optionally restoring focus. */
        close({restoreFocus = false}: WorksheetPickerCloseOptions = {}): void {
            this.dependencies.native().call<void>('lab_widgets_picker_close', this.dependencies.state(), Number(restoreFocus));
        }

        /** @brief Position the popup using native geometry and the live viewport. */
        place(shell: HTMLElement): void {
            this.dependencies.native().call<void>('lab_widgets_picker_place', this.dependencies.state(), shell, this.dependencies.viewport().width, this.dependencies.viewport().height);
        }

        /** @brief Project the current native picker without reopening a closed popup. */
        render(): void {
            const state = this.dependencies.state();
            const input = state.input;
            this.dependencies.native().call<void>(
                'lab_widgets_picker_render', state, this.dependencies.dates().parseMarsIsoDate(input?.value),
                this.dependencies.dates().parseMarsIsoDate(this.todayForInput(input)), this.dependencies.viewport().width, this.dependencies.viewport().height);
        }

        /** @brief Transfer popup ownership to the supplied input and button. */
        open(input: HTMLInputElement, button: WorksheetCopyButton): void {
            this.dependencies.native().call<void>(
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
            const moved = this.dependencies.native().call<number>(
                'lab_widgets_picker_move', state, operation, first, second,
                this.dependencies.dates().parseMarsIsoDate(state.input?.value), this.dependencies.clock().getFullYear(), Number(!!commit));
            this.dependencies.native().services(this.dependencies.native().call<NativeServicePlan>('lab_flow_location_picker', Number(!!moved)), this.dependencies.services());
        }
    }
    // The script is loaded before workspace state; resolve it only when an action runs.
    /** @brief Shared popup owner; calendar policy and date validation remain native. */
    export const picker: WorksheetDatePickerController = new WorksheetDatePickerController({
        state: () => WorksheetContext.marsDatePickerState,
        native: () => labDOM,
        dates: () => dates,
        viewport: () => ({width: window.innerWidth, height: window.innerHeight}),
        clock: () => new Date(),
        services: () => locationServices
    });

    interface TownSelectionControllerDependencies {
        flow: () => Pick<WorksheetRequests.NativeFlowRuntime, 'continue'>;
        calendar: () => Pick<CalendarStateController, 'calendarLocationContext' | 'acceptCalendarLocationContext'>;
        dates: () => Pick<BrowserDateAdapter, 'formatOffsetHours' | 'timeZoneOffsetHours'>;
        native: () => Pick<typeof labDOM, 'call'>;
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
            return this.dependencies.native().call<unknown[]>('lab_location_towns', this.dependencies.calendar().calendarLocationContext(), {value: jurisdiction});
        }

        /** @brief Select a town using the native coordinate-matching policy. */
        selectTownByCoordinates(select: WorksheetSelect, latitude: unknown, longitude: unknown) {
            return Boolean(this.dependencies.native().call<number>('lab_location_coordinates', select, {latitude, longitude}));
        }

        /** @brief Restore a saved town; return null when ownership has changed. */
        restoreTownSelection(select: WorksheetSelect, jurisdiction: string | undefined, townValue: string | undefined, latitude: string | undefined, longitude: string | undefined, isCurrent = () => true): Promise<boolean | null> {
            return this.dependencies.flow().continue<boolean | null>(27, {select, jurisdiction, selection: {value: townValue, latitude, longitude}, isCurrent});
        }

        /** @brief Populate a town menu under the supplied ownership guard. */
        populateTownSelect(select: WorksheetSelect, jurisdiction: string | undefined, {selectDefault = true, isCurrent = () => true}: WorksheetTownOptions = {}): Promise<unknown[]> {
            return this.dependencies.flow().continue<unknown[]>(28, {select, jurisdiction, selectDefault, isCurrent});
        }

        /** @brief Return the native-selected option, or null for a custom location. */
        selectedTownOption(select: WorksheetSelect | undefined) {
            return this.dependencies.native().call<HTMLOptionElement | null>('lab_location_selected', select);
        }

        /** @brief Clear named-town selection after authored coordinate changes. */
        clearTownForCustomCoordinates(townSelect: WorksheetSelect, latitudeInput: HTMLInputElement, longitudeInput: HTMLInputElement, elevationInput: HTMLInputElement | undefined) {
            this.dependencies.native().call<void>('lab_location_clear_custom', townSelect, latitudeInput, longitudeInput, elevationInput);
        }

        /** @brief Apply a selected town and its browser time-zone offset, accepting native flag changes. */
        applySelectedTown(controls: WorksheetTownControls = {}) {
            const option = this.selectedTownOption(controls.townSelect);
            const offset = this.dependencies.dates().formatOffsetHours(this.dependencies.dates().timeZoneOffsetHours(option?.dataset.timezone, controls.dateInput?.value));
            const context = this.dependencies.calendar().calendarLocationContext();
            const applied = this.dependencies.native().call<number>('lab_location_apply_town', controls, offset, context);
            this.dependencies.calendar().acceptCalendarLocationContext(context);
            return Boolean(applied);
        }

        /** @brief Populate both calendar town menus through the native continuation. */
        syncTownSelectors({selectDefault = false}: Pick<WorksheetTownOptions, 'selectDefault'> = {}): Promise<void> {
            return this.dependencies.flow().continue<void>(29, {selectDefault});
        }
    }

    /** @brief Town menus and guarded selection; native code owns matching and restoration. */
    export const townSelection: TownSelectionController = new TownSelectionController({flow: () => WorksheetRequests.runtime, calendar: () => calendar, dates: () => dates, native: () => labDOM});

    interface CalendarStateControllerDependencies {
        context: () => Pick<typeof WorksheetContext, 'HOLIDAY_JURISDICTION_SET' | 'JURISDICTION_TOWN_OPTIONS' | 'worksheetSession' | 'ALMANAC_WORKSHEET_TITLE'>;
        mode: () => Pick<WorksheetWorkspace.WorksheetModeSession, 'workspaceModeId'>;
        configuration: () => LabConfiguration;
        native: () => Pick<typeof labDOM, 'call'>;
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
            this.dependencies.context().worksheetSession.calendar.datetimeGmtOffsetTouched = true;
        }

        /** @brief Capture the catalogue and current browser-owned calendar flags for native projection. */
        calendarLocationContext() {
            return {
                config: this.dependencies.configuration(),
                jurisdictions: this.dependencies.context().HOLIDAY_JURISDICTION_SET,
                towns: this.dependencies.context().JURISDICTION_TOWN_OPTIONS,
                visibility: this.dependencies.context().worksheetSession.calendar.almanacVisibilityMode,
                automaticOffset: this.dependencies.context().worksheetSession.calendar.datetimeAutoGmtOffset,
                offsetTouched: this.dependencies.context().worksheetSession.calendar.datetimeGmtOffsetTouched
            };
        }

        /** @brief Accept calendar flags updated by a completed native projection. */
        acceptCalendarLocationContext(context: ReturnType<CalendarStateController['calendarLocationContext']>) {
            this.dependencies.context().worksheetSession.calendar.almanacVisibilityMode = context.visibility;
            this.dependencies.context().worksheetSession.calendar.datetimeAutoGmtOffset = context.automaticOffset;
            this.dependencies.context().worksheetSession.calendar.datetimeGmtOffsetTouched = context.offsetTouched;
        }

        /** @brief Read the named worksheet controls using the native calendar schema. */
        readCalendarControls(mode: WorksheetMode) {
            return this.dependencies.native().call<WorksheetCalendarState>('lab_location_read', this.dependencies.mode().workspaceModeId(mode), this.calendarLocationContext());
        }

        /** @brief Normalise calendar state without changing controls; reject unsupported operations. */
        calendarState(mode: WorksheetMode, source: unknown, operation: number, prefix = '') {
            const state = this.dependencies.native().call<WorksheetCalendarState | null>(
                'lab_location_state', this.dependencies.mode().workspaceModeId(mode), operation, source, prefix, this.calendarLocationContext(), 0);
            if (!state)
                throw new Error('Invalid calendar state operation');
            return state;
        }

        /** @brief Project native-normalised calendar state and accept its updated context. */
        applyCalendarState(mode: WorksheetMode, source: unknown, operation = 0, prefix = '') {
            const context = this.calendarLocationContext();
            const state = this.dependencies.native().call<WorksheetCalendarState | null>('lab_location_state', this.dependencies.mode().workspaceModeId(mode), operation, source, prefix, context, 1);
            if (!state)
                throw new Error('Invalid calendar state operation');
            this.acceptCalendarLocationContext(context);
            return state;
        }

        /** @brief Resolve browser controls using the native worksheet schema. */
        calendarElements(mode: WorksheetMode) {
            return this.dependencies.native().call<Record<string, HTMLElement>>('lab_location_elements', this.dependencies.mode().workspaceModeId(mode));
        }

        /** @brief Resolve the town-control group selected by the native calendar schema. */
        calendarTownControls(mode: WorksheetMode, coordinates = true) {
            return this.dependencies.native().call<WorksheetTownControls>('lab_location_controls', this.dependencies.mode().workspaceModeId(mode), Number(coordinates));
        }

        /** @brief Read current native calendar state and accept its updated flags. */
        currentCalendarState(mode: WorksheetMode) {
            const context = this.calendarLocationContext();
            const state = this.dependencies.native().call<WorksheetCalendarState>('lab_location_current', this.dependencies.mode().workspaceModeId(mode), context);
            this.acceptCalendarLocationContext(context);
            return state;
        }

        /** @brief Apply native-validated response fields without interpreting their values. */
        applyCalendarEvaluationFields(mode: WorksheetMode, data: Partial<WorksheetEvaluationData>) {
            const context = this.calendarLocationContext();
            this.dependencies.native().call<void>('lab_location_evaluation', this.dependencies.mode().workspaceModeId(mode), data, context);
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
            return this.dependencies.native().call<string>('lab_location_summary', 6, state, this.dependencies.context().ALMANAC_WORKSHEET_TITLE);
        }

        /** @brief Obtain native summary text for the supplied DateTime state. */
        datetimeSummaryText(state = this.currentDatetimeState()) {
            return this.dependencies.native().call<string>('lab_location_summary', 5, state, '');
        }
    }

    /** @brief Live calendar controls and session flags; captures native contexts without duplicating state. */
    export const calendar: CalendarStateController = new CalendarStateController({context: () => WorksheetContext, mode: () => WorksheetWorkspace.mode, configuration: () => labApplication.configuration, native: () => labDOM});

    interface CalendarActionControllerDependencies {
        flow: () => Pick<WorksheetRequests.NativeFlowRuntime, 'continue'>;
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
            return this.dependencies.flow().continue<string>(26, {action: 0, value});
        }

        /** @brief Restore calendar controls and town selection under an ownership guard. */
        restoreCalendarHistory(mode: WorksheetMode, source: unknown, isCurrent = () => true): Promise<void> {
            return this.dependencies.flow().continue<void>(30, {mode, source, isCurrent});
        }

        /** @brief Run the native location and evaluation continuation for a totality action. */
        applyAlmanacTotalityAction(button: WorksheetCopyButton): Promise<void> {
            return this.dependencies.flow().continue<void>(31, {button});
        }

        /** @brief Refresh location data through native policy, optionally replacing coordinates. */
        refreshCalendarJurisdictionLocation(mode: WorksheetMode, {updateCoordinates = true}: WorksheetCalendarOptions = {}): Promise<void> {
            return this.dependencies.flow().continue<void>(32, {mode, updateCoordinates});
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
            return this.dependencies.flow().continue<void>(33, {mode: 'datetime', options: {refreshJurisdiction, refreshCoordinates}});
        }

        /** @brief Delegate almanac refresh, persistence and evaluation ordering to C. */
        triggerAlmanacAutoEvaluation({refreshJurisdiction = false, refreshCoordinates = false}: WorksheetAutoEvaluationOptions = {}): Promise<void> {
            return this.dependencies.flow().continue<void>(33, {mode: 'almanac', options: {refreshJurisdiction, refreshCoordinates}});
        }

        /** @brief Format time through native services, retaining request and authored-input ownership. */
        formatAlmanacTimeFromEvent(): Promise<void> {
            return this.dependencies.flow().continue<void>(34, {});
        }

        /** @brief Deliver native calendar actions to asynchronous location and evaluation services. */
        applyCalendarControlEvent(mode: WorksheetMode, actions: number): void {
            void this.dependencies.flow().continue<void>(35, {mode, actions});
        }
    }

    /** @brief Calendar commands delegated to native continuations, preserving request ownership and ordering. */
    export const calendarActions: CalendarActionController = new CalendarActionController({flow: () => WorksheetRequests.runtime});

    interface BrowserDateAdapterDependencies {
        wire: () => Pick<typeof labWire, 'exports'>;
        document: () => Pick<Document, 'createElement'>;
        clock: () => Date;
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
                const parts = Object.fromEntries(formatter.formatToParts(probe).map((part: Intl.DateTimeFormatPart) => [part.type, part.value]));
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

        /** @brief Return the current local calendar date in ISO notation. */
        marsTodayIsoDate() {
            const now = this.dependencies.clock();
            return marsIsoDate(now.getFullYear(), now.getMonth() + 1, now.getDate());
        }

        /** @brief Return the UTC date and time for the supplied browser clock value. */
        marsCurrentGmtMoment(now = this.dependencies.clock()) {
            return {
                date: marsIsoDate(now.getUTCFullYear(), now.getUTCMonth() + 1, now.getUTCDate()),
                time: marsClockTime(now.getUTCHours(), now.getUTCMinutes(), now.getUTCSeconds())
            };
        }
    }

    /** @brief Browser date conversion and clock access with native validation and arithmetic. */
    export const dates: BrowserDateAdapter = new BrowserDateAdapter({wire: () => labWire, document: () => document, clock: () => new Date(), dateTimeFormat: Intl.DateTimeFormat});

}
