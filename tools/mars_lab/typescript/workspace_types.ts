/**
 * @file workspace_types.ts
 * @brief Shared contracts for worksheet domain adapters.
 *
 * Describes bindings, native result snapshots, editor metadata and browser controls.
 * These declarations carry no runtime state; native payloads remain opaque unless an
 * adapter consumes their fields.
 */
/**
 * Shared contracts for the classic worksheet scripts.
 * Native payloads remain opaque unless an adapter consumes their fields.
 * These declarations introduce no runtime module wrapper or imports.
 */
type WorksheetMode = 'expression' | 'equation' | 'diffequation' | 'matrix' | 'integrator' | 'datetime' | 'almanac';
type WorksheetGuard = () => boolean;
type WorksheetSourceKey = 'fullText' | 'displayText' | 'lastInput' | 'goalSource' | 'goalTarget';
type WorksheetSources = Record<WorksheetSourceKey, string>;
interface WorksheetSaveOptions { keepalive?: boolean; debounce?: boolean; }
interface WorksheetModeOptions { force?: boolean; }
interface WorksheetEvaluationOptions { skipHistoryUpdate?: boolean; reuseLastInput?: boolean; }
interface WorksheetBinding {
    name: string;
    value?: string;
    kind?: string;
    [field: string]: unknown;
}
interface WorksheetEditorMetadata {
    expression: string;
    bindings: WorksheetBinding[];
    [field: string]: unknown;
}
interface WorksheetIntegratorRow {
    kind: 'free' | 'bound';
    name: string;
    lo: string;
    hi: string;
}
interface WorksheetRowPlan { bounds: WorksheetIntegratorRow[]; rows: WorksheetIntegratorRow[]; }
interface WorksheetRowsMetadata { expression: string; [field: string]: unknown; }
interface WorksheetSnapshot {
    mode: WorksheetMode;
    text: string;
    [field: string]: unknown;
}
interface WorksheetResultValues {
    lastTex: string;
    lastDerivativeExpression: string;
    currentVariables: string[];
    currentDifferentiable: boolean;
    resultInputBindings: WorksheetBinding[];
}
interface WorksheetResultSnapshot extends WorksheetResultValues { surfaceView?: MarsSurfaceSnapshot | null; }
interface WorksheetDisplayResult { TeX: string; bindings: WorksheetBinding[]; }
interface WorksheetEvaluationData {
    editor: WorksheetEditorMetadata;
    rows?: WorksheetIntegratorRow[];
    binding_values?: WorksheetBinding[];
    evaluation_ready?: boolean;
    [field: string]: unknown;
}
interface WorksheetTextCards {
    expression: string;
    full_expression: string;
    input: string;
    function: string;
    full_function: string;
    value: string;
    differentiable: boolean;
}
interface WorksheetGoalContext {
    editorExpression: string;
    editorBody: string;
    cards: WorksheetTextCards;
    seek: { expression: string };
    target: string;
}
interface WorksheetCalendarState {
    jurisdiction?: string;
    town?: string;
    latitude?: string;
    longitude?: string;
    [field: string]: unknown;
}
interface WorksheetTownControls {
    townSelect?: WorksheetSelect;
    dateInput?: HTMLInputElement;
    zoneInput?: HTMLInputElement;
    latitudeInput?: HTMLInputElement;
    longitudeInput?: HTMLInputElement;
    elevationInput?: HTMLInputElement;
    resetOffsetTouched?: boolean;
}
interface WorksheetSelect extends HTMLSelectElement {
    __marsRebuildRoundedSelect?: () => unknown;
    __marsSyncRoundedSelect?: () => unknown;
}
interface WorksheetCopyButton extends HTMLButtonElement { copyResetTimer?: number; }
interface WorksheetPickerState {
    input: HTMLInputElement | null;
    button: HTMLButtonElement | null;
    shell: HTMLElement | null;
    readonly year: number;
    readonly month: number;
}
interface WorksheetBindingCard { input: HTMLInputElement; binding: WorksheetBinding; message: string; }
interface WorksheetBindingRendered { values: [string, string][]; kinds: [string, string][]; }
interface WorksheetBindingCapture { mode: WorksheetMode; modeId: number; source: string; text: string; editor: HTMLTextAreaElement; }
interface WorksheetBindingCommitFrame {
    before: WorksheetBindingCapture;
    inputs: HTMLInputElement[];
    isCurrent: WorksheetGuard;
}
interface WorksheetCalendarOptions { updateCoordinates?: boolean; }
interface WorksheetAutoEvaluationOptions { refreshJurisdiction?: boolean; refreshCoordinates?: boolean; }
interface WorksheetTownOptions { selectDefault?: boolean; isCurrent?: WorksheetGuard; }
interface WorksheetSavePlan { index: number; delay: number; [field: string]: unknown; }
interface WorksheetCalendarCard { element: HTMLElement; button: WorksheetCopyButton | null; sections: unknown; text: string; }
interface WorksheetPrecisionPlan { source: string; target: string; action: number; }

interface WorksheetReferences {
    expr: HTMLTextAreaElement;
    subtitle: HTMLElement;
    leftPaneTitle: HTMLElement;
    matrixControls: HTMLElement;
    matrixOperation: WorksheetSelect;
    matrixOperand: HTMLTextAreaElement;
    matrixOperandLabel: HTMLElement;
    equationControls: HTMLElement;
    diffequationControls: HTMLElement;
    integratorControls: HTMLElement;
    integratorBoundStack: HTMLElement;
    integratorIntervalCap: HTMLInputElement;
    datetimeControls: HTMLElement;
    datetimeDate: HTMLInputElement;
    datetimeJdn: HTMLInputElement;
    datetimeStart: HTMLInputElement;
    datetimeYear: HTMLInputElement;
    datetimeJurisdiction: WorksheetSelect;
    datetimeTown: WorksheetSelect;
    datetimeLatitude: HTMLInputElement;
    datetimeLongitude: HTMLInputElement;
    datetimeGmtOffset: HTMLInputElement;
    datetimeLocal: HTMLElement;
    datetimeLocalBody: HTMLElement;
    almanacControls: HTMLElement;
    almanacDate: HTMLInputElement;
    almanacTime: HTMLInputElement;
    almanacZone: HTMLInputElement;
    almanacJurisdiction: WorksheetSelect;
    almanacTown: WorksheetSelect;
    almanacLatitude: HTMLInputElement;
    almanacLongitude: HTMLInputElement;
    almanacElevation: HTMLInputElement;
    marsDatePicker: HTMLElement;
    marsDatePickerMonth: WorksheetSelect;
    marsDatePickerYear: HTMLInputElement;
    marsDatePickerWeekdays: HTMLElement;
    marsDatePickerGrid: HTMLElement;
    marsDatePickerToday: WorksheetCopyButton;
    marsDatePickerClose: WorksheetCopyButton;
    run: WorksheetCopyButton;
    back: WorksheetCopyButton;
    forward: WorksheetCopyButton;
    help: WorksheetCopyButton;
    goalSeek: WorksheetCopyButton;
    clear: WorksheetCopyButton;
    targetRow: HTMLElement;
    goalTarget: HTMLInputElement;
    precisionSlider: HTMLInputElement;
    precisionValue: HTMLOutputElement;
    derivativeButtons: HTMLElement;
    variableValues: HTMLElement;
    mobileAccess: HTMLElement;
    mobileTitle: HTMLElement;
    mobileHint: HTMLElement;
    mobileUrl: HTMLElement;
    mobileQr: HTMLElement;
    statusEl: HTMLElement;
    inputCopy: WorksheetCopyButton;
    labWorkspace: HTMLElement;
    rightPaneTitle: HTMLElement;
    resultUseInput: WorksheetCopyButton;
    resultPane: HTMLElement;
    helpPane: HTMLElement;
    rendered: HTMLElement;
    renderedTitle: HTMLElement;
    renderedMore: WorksheetCopyButton;
    parsed: HTMLElement;
    parsedMore: WorksheetCopyButton;
    functionStyle: HTMLElement;
    functionTitle: HTMLElement;
    functionMore: WorksheetCopyButton;
    functionRun: WorksheetCopyButton;
    functionRunResult: HTMLElement;
    functionRunOutput: HTMLElement;
    valueCard: HTMLElement;
    valueNoteCard: HTMLElement;
    valueNote: HTMLElement;
    value: HTMLElement;
    valueTitle: HTMLElement;
    valueMore: WorksheetCopyButton;
    labTextareas: HTMLTextAreaElement[];
    modeTabs: HTMLElement[];
    helpCards: HTMLElement[];
    copyButtons: WorksheetCopyButton[];
    moreDigitButtons: WorksheetCopyButton[];
    resultCards: HTMLElement[];
}


type WorksheetRequest = WorksheetRequests.MarsApiRequest;
type WorksheetResponse = Awaited<ReturnType<typeof labHttp.fetch>>;
interface WorksheetCardEvent {
    action: string;
    prevent?: boolean;
    card: WorksheetBindingCard;
}
interface WorksheetRowEvent {
    action: string;
    prevent?: boolean;
    item: HTMLElement;
    index: number;
    operation: number;
}
