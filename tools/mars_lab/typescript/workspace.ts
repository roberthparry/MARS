/**
 * @file workspace.ts
 * @brief Worksheet coordination and browser-session composition.
 *
 * Creates the controllers defined by workspace_controllers.ts, then initialises
 * WorksheetContext's DOM references and native-backed editor, precision and calendar
 * views. Loaded after the definition scripts and native catalogue, before browser
 * subscriptions and the first evaluation. Dependency getters keep construction from
 * reading context fields before they exist. Native C owns workspace projection,
 * mathematical state and policy; this file supplies browser resources and startup order.
 */

namespace WorksheetWorkspace {
    /** Stable native mode indices shared by typed state views. */
    export const WORKSPACE_MODE_NAMES = Object.freeze([
        'expression', 'equation', 'diffequation', 'matrix', 'integrator', 'datetime', 'almanac'
    ] as const);

    /** Session-owned native transfer buffer. */
    export const labWorkspaceBuffer: WorksheetNativeBuffer = new WorksheetNativeBuffer(labWire, WORKSPACE_MODE_NAMES);

    /** Live native views share the session buffer and immutable mode catalogue. */
    export const nativeViews: WorksheetNativeViews = new WorksheetNativeViews(labWire, labWorkspaceBuffer, WORKSPACE_MODE_NAMES);

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
        locations: {
            /** @brief Synchronise the browser's rounded select widget. */
            syncRoundedSelect(select) { WorksheetLocations.selects.sync(select); }
        },
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

        /** @brief Cancel this layout owner's pending browser frame. */
        cancelFrame(handle) { cancelAnimationFrame(handle); },

        /** @brief Schedule the next browser geometry update. */
        requestFrame(callback) { return requestAnimationFrame(callback); },
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
    /** @brief Named controls returned by the native DOM catalogue; see WorksheetReferences for their types. */
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
    } = labDOM.call('lab_workspace_dom_references') as WorksheetReferences;

    /** @brief Legacy equation-variable control; the current worksheet has no separate input. */
    export const equationVariable = null;

    /** @brief Native bootstrap token used by authenticated control requests. */
    export const controlToken = labApplication.configuration.CONTROL_TOKEN;

    /** @brief Browser-only presentation, calendar and editor resources for this page. */
    export const worksheetSession: WorksheetSession = new WorksheetSession();

    /** @brief Direct node-to-index lookup shared with the native result-view controller. */
    export const resultCardIds = new WeakMap<HTMLElement, number>();
    for (let index = 0; index < resultCards.length; ++index) resultCardIds.set(resultCards[index], index);
    if (!labWire.exports().lab_view_cards_reset(resultCards.length))
        throw new Error('Too many result cards for the native view controller');

    labWire.exports().lab_workspace_reset();
    export const expressionHistory = 0;
    export const forwardHistory = 1;
    export const labEditorState = WorksheetWorkspace.nativeViews.sources();

    labDOM.services(
        labDOM.call('lab_bootstrap_workspace', 0, {
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
        }) as NativeServicePlan,
        {
            /** @brief Apply the native token-cleanup URL without navigation. */
            replaceLocation(url: string) { window.history.replaceState(null, '', url); }
        });
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
        labDOM.call('lab_bootstrap_workspace', 1, {
            control: datetimeJurisdiction, fallback: DEFAULT_DATETIME_JURISDICTION
        }) as NativeServicePlan,
        {
            /** @brief Install the native-selected initial jurisdiction value. */
            writeValue(control: HTMLInputElement, value: unknown) { return control.value = String(value); }
        });
    worksheetSession.calendar.datetimeAutoGmtOffset =
        labDOM.call('lab_bootstrap_workspace', 2, {
            control: datetimeGmtOffset, fallback: DEFAULT_DATETIME_GMT_OFFSET
        }) as string;
    worksheetSession.calendar.almanacVisibilityMode = DEFAULT_ALMANAC_VISIBILITY;

    /** @brief Browser membership index built once from native jurisdiction codes. */
    export const HOLIDAY_JURISDICTION_SET = new Set<string>();
    for (const [code] of labApplication.catalogue.options) HOLIDAY_JURISDICTION_SET.add(code);

    /** @brief Native town records indexed by jurisdiction; the browser does not reinterpret them. */
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
        labDOM.call('lab_workspace_dom_result_states') as WorksheetWorkspace.WorkspaceResultStates;

    /** @brief Browser date-picker references, with date components read from native state. */
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
