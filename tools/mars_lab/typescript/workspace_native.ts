/**
 * @file workspace_native.ts
 * @brief Native worksheet buffers, state views and browser session owners.
 *
 * Provides validated UTF-8 transfer, snapshot reading and concrete views of C-owned state.
 * Native C validates snapshot envelopes and their mode catalogue. Property descriptors
 * connect browser properties directly to native storage, selecting their field kind once.
 * Separate classes own presentation, editor and calendar resources; no mathematical
 * expression parsing or simplification is performed in the browser.
 */

/** @brief Source keys in lab_workspace_source_get/set ABI order; also defines WorksheetSources. */
const WORKSHEET_SOURCE_NAMES = Object.freeze(['fullText', 'displayText', 'lastInput', 'goalSource', 'goalTarget'] as const);

/** Native operations used by worksheet form and snapshot adapters. */
interface WorksheetNativeCalls {
    /** @brief Invoke a native operation within one scoped browser-handle lifetime. */
    call(name: string, ...args: unknown[]): unknown;
}

/** Access to native worksheet memory and state exports. */
interface WorksheetNativeExports {
    /** @brief Borrow the initialised native module's exports. */
    exports(): MarsTransportExports;
}

/** Binary codec capabilities required by the worksheet buffer owner. */
interface WorksheetNativeCodec extends WorksheetNativeExports {
    /** @brief Encode a browser value into independently owned bytes. */
    encode(value: unknown): Uint8Array<ArrayBuffer>;
    /** @brief Decode a copied binary message. */
    decode(buffer: import('./codec_types.js').CodecBuffer): unknown;
}

/** Native text transfer used by live state accessors. */
interface WorksheetNativeText {
    /** @brief Copy and decode native output. */
    readText(length: number): string;
    /** @brief Validate and stage text in a native input buffer. */
    stageText(text: string, index?: number): number;
}

/** Live precision values for the seven worksheet modes. */
interface WorksheetModePrecisions {
    expression: number;
    equation: number;
    diffequation: number;
    matrix: number;
    integrator: number;
    datetime: number;
    almanac: number;
}

/** Live editor text for the seven worksheet modes. */
interface WorksheetModeEditors {
    expression: string;
    equation: string;
    diffequation: string;
    matrix: string;
    integrator: string;
    datetime: string;
    almanac: string;
}

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
        private readonly dom: WorksheetNativeCalls = labDOM,
        private readonly wire: WorksheetNativeExports = labWire
    ) {
        marsBindCallbacks(this, ['nextRevision', 'install', 'text', 'plan', 'edit']);
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
        this.metadata = this.dom.call('lab_binding_rows_metadata', expression, result) as WorksheetRowsMetadata | null;
    }

    /** Obtain native row text, rejecting invalid Unicode or native capacity overflow. */
    public text(rows: WorksheetIntegratorRow[]): string {
        const text = this.dom.call('lab_binding_rows_text', rows) as string | null;
        if (text === null)
            throw new Error('Integrator rows exceed the native limits or contain invalid Unicode');
        return text;
    }

    /** Plan rows against the installed metadata; reject a native validation failure. */
    public plan(rows: WorksheetIntegratorRow[], expression: string): WorksheetRowPlan {
        const plan = this.dom.call('lab_binding_rows_plan', rows, this.metadata, expression) as WorksheetRowPlan | null;
        if (!plan)
            throw new Error('Invalid integrator row plan');
        return plan;
    }

    /** Request a native row edit, preserving the minimum-bound and maximum-row constraints. */
    public edit(rows: WorksheetIntegratorRow[], index: number, operation: number): WorksheetIntegratorRow[] {
        const plan = this.dom.call('lab_binding_rows_edit', rows, index, operation) as WorksheetIntegratorRow[] | null;
        if (!plan)
            throw new Error('This edit would remove the last bound or exceed the 256-row form limit');
        return plan;
    }
}

/** Owns UTF-8 transfer buffers and submits decoded snapshots for native validation. */
class WorksheetNativeBuffer {
    /** Retain the native transport; copied outputs never alias its growable memory. */
    public constructor(
        private readonly wire: WorksheetNativeCodec = labWire,
        private readonly modeNames: readonly WorksheetMode[] = WorksheetWorkspace.WORKSPACE_MODE_NAMES,
        private readonly native: WorksheetNativeCalls = labDOM
    ) {
        marsBindCallbacks(this, ['stageSnapshot', 'stage', 'stageText', 'readBytes', 'readText', 'readSnapshot']);
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
        if (length === 0) return null;
        const bytes = this.readBytes(length);
        const envelope = this.wire.decode(bytes.buffer);
        const state = this.native.call('lab_workspace_snapshot_validate', envelope, this.modeNames);
        if (state === -1)
            throw new Error('Invalid native worksheet snapshot envelope');
        if (state === -2)
            throw new Error('Invalid native worksheet snapshot');
        return state as WorksheetSnapshot | null;
    }
}

/**
 * @brief Create one enumerable, non-configurable accessor onto native state.
 * Select the field kind once; every access borrows the current exports, without caching
 * native values or functions. Text writes preserve staging validation and native errors.
 */
function worksheetNativeFieldDescriptor(wire: WorksheetNativeExports, buffer: WorksheetNativeText,
                                           kind: 'precision' | 'editor' | 'source', index: number): PropertyDescriptor {
    if (kind === 'precision') {
        return {
            enumerable: true,
            get(): number { return wire.exports().lab_workspace_precision(index); },
            set(value: unknown): void { wire.exports().lab_workspace_precision_set(index, Number(value)); }
        };
    }
    if (kind === 'editor') {
        return {
            enumerable: true,
            get(): string { return buffer.readText(wire.exports().lab_workspace_editor_get(index)); },
            set(value: unknown): void {
                const native = wire.exports();
                const length = buffer.stageText(String(value));
                if (!native.lab_workspace_editor_set(index, length))
                    throw new Error('Could not retain worksheet editor text');
            }
        };
    }
    return {
        enumerable: true,
        get(): string { return buffer.readText(wire.exports().lab_workspace_source_get(index)); },
        set(value: unknown): void {
            const native = wire.exports();
            const length = buffer.stageText(String(value));
            if (!native.lab_workspace_source_set(index, length))
                throw new Error('Could not retain editor source text');
        }
    };
}

/** Typed accessors onto C-owned state; no duplicate JavaScript worksheet model. */
class WorksheetNativeViews {
    /** Retain native state capabilities and a stable mode catalogue, without creating duplicate state. */
    public constructor(
        private readonly wire: WorksheetNativeExports,
        private readonly buffer: WorksheetNativeText,
        private readonly modeNames: readonly WorksheetMode[]
    ) {
        marsBindCallbacks(this, ['mode', 'sources']);
    }

    /** Create a live per-mode precision view whose reads and writes access native state. */
    public mode(kind: 'precision'): WorksheetModePrecisions;

    /** Create a live per-mode editor-text view; failed native text writes throw. */
    public mode(kind: 'editor'): WorksheetModeEditors;

    /** Build non-extensible accessors without retaining a duplicate of native worksheet state. */
    public mode(kind: 'precision' | 'editor'): WorksheetModePrecisions | WorksheetModeEditors {
        const view = Object.create(null) as WorksheetModePrecisions | WorksheetModeEditors;
        for (let index = 0; index < this.modeNames.length; ++index) {
            Object.defineProperty(view, this.modeNames[index],
                worksheetNativeFieldDescriptor(this.wire, this.buffer, kind, index));
        }
        return Object.preventExtensions(view);
    }

    /** Create live source-text accessors; native storage owns their values and validates writes. */
    public sources(): WorksheetSources {
        const view = Object.create(null) as WorksheetSources;
        for (let index = 0; index < WORKSHEET_SOURCE_NAMES.length; ++index) {
            Object.defineProperty(view, WORKSHEET_SOURCE_NAMES[index],
                worksheetNativeFieldDescriptor(this.wire, this.buffer, 'source', index));
        }
        return Object.preventExtensions(view);
    }

}
