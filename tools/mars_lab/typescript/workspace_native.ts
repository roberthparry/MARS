/**
 * @file workspace_native.ts
 * @brief Native worksheet buffers, state views and browser session owners.
 *
 * Provides validated UTF-8 transfer, snapshot reading and typed views of C-owned state.
 * Separate classes own presentation, editor and calendar resources; no mathematical
 * expression parsing or simplification is performed in the browser.
 */
/** Native worksheet capabilities. Definitions load before workspace state is initialised. */

/** Browser-only presentation state. Algebra and plot state remain native-owned. */
class WorksheetPresentationState {
    lastTex = '';
    resultInputBindings: WorksheetBinding[] = [];
    lastDerivativeExpression = '';
    lastMatrixScalarExpression = '';
}

/** Instance-owned editor activity metadata; binding caches and timers belong to controllers. */
class WorksheetEditorSession {
    lastExpressionUpdatedAt = 0;
}

/** Calendar control ownership is independent of expression presentation. */
class WorksheetCalendarSession {
    datetimeAutoGmtOffset = '';
    datetimeGmtOffsetTouched = false;
    almanacVisibilityMode = '';
    almanacLastWorksheetData: WorksheetEvaluationData | null = null;
}

/** Independent residual editor and calendar state; domain resources live in their controllers. */
class WorksheetSession {
    readonly editor = new WorksheetEditorSession();
    readonly calendar = new WorksheetCalendarSession();
}

/** Owns reference metadata; C remains the authority for row validation and edits. */
class WorksheetIntegratorForms {
    /** Retain native capabilities; row policy and revision storage remain native-owned. */
    public constructor(
        private readonly dom: Pick<typeof labDOM, 'call'> = labDOM,
        private readonly wire: Pick<typeof labWire, 'exports'> = labWire
    ) {
        // Preserve the detachable callback contract of the former namespace functions.
        this.nextRevision = this.nextRevision.bind(this);
        this.install = this.install.bind(this);
        this.text = this.text.bind(this);
        this.plan = this.plan.bind(this);
        this.edit = this.edit.bind(this);
    }

    public metadata: WorksheetRowsMetadata | null = null;

    /** Advance the native form revision; exhaustion requires reloading the Lab. */
    public nextRevision(): number {
        const revision = this.wire.exports().lab_rows_revision_next();
        if (!revision)
            throw new Error('Integrator form revision limit reached; reload the Lab');
        return revision;
    }

    /** Replace cached reference metadata with the native interpretation of an evaluation. */
    public install(expression: unknown, result: WorksheetEvaluationData): void {
        this.metadata = this.dom.call<WorksheetRowsMetadata | null>('lab_binding_rows_metadata', expression, result);
    }

    /** Obtain native row text, rejecting invalid Unicode or native capacity overflow. */
    public text(rows: WorksheetIntegratorRow[]): string {
        const text = this.dom.call<string | null>('lab_binding_rows_text', rows);
        if (text === null)
            throw new Error('Integrator rows exceed the native limits or contain invalid Unicode');
        return text;
    }

    /** Plan rows against the installed metadata; reject a native validation failure. */
    public plan(rows: WorksheetIntegratorRow[], expression: string): WorksheetRowPlan {
        const plan = this.dom.call<WorksheetRowPlan | null>('lab_binding_rows_plan', rows, this.metadata, expression);
        if (!plan)
            throw new Error('Invalid integrator row plan');
        return plan;
    }

    /** Request a native row edit, preserving the minimum-bound and maximum-row constraints. */
    public edit(rows: WorksheetIntegratorRow[], index: number, operation: number): WorksheetIntegratorRow[] {
        const plan = this.dom.call<WorksheetIntegratorRow[] | null>('lab_binding_rows_edit', rows, index, operation);
        if (!plan)
            throw new Error('This edit would remove the last bound or exceed the 256-row form limit');
        return plan;
    }
}

/** Owns UTF-8 transfer buffers and validates the native snapshot envelope. */
class WorksheetNativeBuffer {
    /** Retain the native transport; copied outputs never alias its growable memory. */
    public constructor(
        private readonly wire: Pick<typeof labWire, 'exports' | 'encode' | 'decode'> = labWire,
        private readonly modeNames: readonly WorksheetMode[] = WorksheetWorkspace.WORKSPACE_MODE_NAMES
    ) {
        // Preserve the detachable callback contract of the former namespace functions.
        this.stageSnapshot = this.stageSnapshot.bind(this);
        this.stage = this.stage.bind(this);
        this.stageText = this.stageText.bind(this);
        this.readBytes = this.readBytes.bind(this);
        this.readText = this.readText.bind(this);
        this.readSnapshot = this.readSnapshot.bind(this);
    }

    private readonly encoder = new TextEncoder();
    private readonly decoder = new TextDecoder('utf-8', {fatal: true, ignoreBOM: true});

    /** Encode and stage a nullable snapshot in the selected native input buffer. */
    public stageSnapshot(state: WorksheetSnapshot | null, index = 0): number {
        return this.stage(this.wire.encode({state: state || null}), index);
    }

    /** Copy bytes into the selected native input buffer and return their length, enforcing capacity. */
    public stage(bytes: Uint8Array, index = 0): number {
        const native = this.wire.exports();
        if (bytes.length > native.lab_workspace_capacity())
            throw new Error('Worksheet snapshot exceeds the native 4 MiB limit');
        new Uint8Array(native.memory.buffer, native.lab_workspace_input(index), bytes.length).set(bytes);
        return bytes.length;
    }

    /** Stage UTF-8 text, rejecting unpaired surrogates rather than silently replacing them. */
    public stageText(text: string, index = 0): number {
        const source = String(text);
        const bytes = this.encoder.encode(source);
        if (this.decoder.decode(bytes) !== source)
            throw new Error('Worksheet text contains an unpaired Unicode surrogate');
        return this.stage(bytes, index);
    }

    /** Copy native output into independently owned bytes; reject a negative native result. */
    public readBytes(length: number): Uint8Array<ArrayBuffer> {
        if (length < 0)
            throw new Error('Invalid native worksheet operation');
        const native = this.wire.exports();
        return new Uint8Array(native.memory.buffer, native.lab_workspace_output(), length).slice();
    }

    /** Decode copied native output as strict UTF-8, preserving a leading byte-order mark. */
    public readText(length: number): string {
        return this.decoder.decode(this.readBytes(length));
    }

    /** Decode a snapshot and validate its envelope and mode; empty or null state returns null. */
    public readSnapshot(length: number): WorksheetSnapshot | null {
        const bytes = this.readBytes(length);
        if (!length) return null;
        const envelope = this.wire.decode(bytes.buffer);
        if (!envelope || typeof envelope !== 'object' || !('state' in envelope))
            throw new Error('Invalid native worksheet snapshot envelope');
        const state = envelope.state;
        if (state === null) return null;
        if (!state || typeof state !== 'object' || !('mode' in state) || !('text' in state) ||
            typeof state.mode !== 'string' || typeof state.text !== 'string' ||
            !this.modeNames.some(mode => mode === state.mode))
            throw new Error('Invalid native worksheet snapshot');
        return state as WorksheetSnapshot;
    }
}

/** Typed accessors onto C-owned state; no duplicate JavaScript worksheet model. */
class WorksheetNativeViews {
    /** Retain native state capabilities and a stable mode catalogue, without creating duplicate state. */
    public constructor(
        private readonly wire: Pick<typeof labWire, 'exports'>,
        private readonly buffer: Pick<WorksheetNativeBuffer, 'readText' | 'stageText'>,
        private readonly modeNames: readonly WorksheetMode[]
    ) {
        // Preserve the detachable callback contract of the former namespace functions.
        this.mode = this.mode.bind(this);
        this.sources = this.sources.bind(this);
    }

    /** Create a live per-mode precision view whose reads and writes access native state. */
    public mode(kind: 'precision'): Record<WorksheetMode, number>;
    /** Create a live per-mode editor-text view; failed native text writes throw. */
    public mode(kind: 'editor'): Record<WorksheetMode, string>;
    /** Build non-extensible accessors without retaining a duplicate of native worksheet state. */
    public mode(kind: 'precision' | 'editor'): Record<WorksheetMode, string | number> {
        const wire = this.wire, buffer = this.buffer;
        const view = Object.create(null) as Record<WorksheetMode, string | number>;
        this.modeNames.forEach((mode, index) => {
            Object.defineProperty(view, mode, {
                enumerable: true,
                get() {
                    const native = wire.exports();
                    if (kind === 'precision')
                        return native.lab_workspace_precision(index);
                    return buffer.readText(native.lab_workspace_editor_get(index));
                },
                set(value: unknown) {
                    const native = wire.exports();
                    if (kind === 'precision')
                        native.lab_workspace_precision_set(index, Number(value));
                    else if (!native.lab_workspace_editor_set(index, buffer.stageText(String(value))))
                        throw new Error('Could not retain worksheet editor text');
                }
            });
        });
        return Object.preventExtensions(view);
    }

    /** Create live source-text accessors; native storage owns their values and validates writes. */
    public sources(): WorksheetSources {
        const wire = this.wire, buffer = this.buffer;
        const view = Object.create(null) as WorksheetSources;
        ['fullText', 'displayText', 'lastInput', 'goalSource', 'goalTarget'].forEach((name, field) => {
            Object.defineProperty(view, name, {
                enumerable: true,
                get() {
                    return buffer.readText(wire.exports().lab_workspace_source_get(field));
                },
                set(value: unknown) {
                    if (!wire.exports().lab_workspace_source_set(field, buffer.stageText(String(value))))
                        throw new Error('Could not retain editor source text');
                }
            });
        });
        return Object.preventExtensions(view);
    }

}
