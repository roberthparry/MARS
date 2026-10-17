/**
 * @file editor_checks.ts
 * @brief Native editor metadata and lazy source-selection regressions.
 *
 * Exercises metadata lookup against an independent fixture oracle, including malformed inputs and getter evaluation boundaries.
 */

/** Exact native editor metadata selection, scoped source resolution and lazy getter boundaries. */
window.checkLabEditor = function checkLabEditor() {
    const check: LabTestCheck = (condition, message) => {
        if (!condition)
            throw new Error('Editor metadata: ' + message);
    };
    const equal = (actual: unknown, expected: unknown, message: string) => check(Object.is(actual, expected), message);
    function native(kind: 5, value: unknown, editors: Map<string, unknown>): EditorFixtureCompact;
    function native(kind: number, value: unknown, editors: Map<string, unknown>): unknown;
    function native(kind: number, value: unknown, editors: Map<string, unknown>): unknown {
        return labDOM.call('lab_editor_metadata', kind, [value], editors);
    }
    // Independent baseline oracle; the production client no longer contains these policy branches.
    const lookup = (value: unknown, editors: Map<string, EditorFixtureMetadata>) => editors.get(String(value || '').trim()) || null;
    const expected: ((value: unknown, editors: Map<string, EditorFixtureMetadata>) => unknown)[] = [
        lookup, (value, editors) => lookup(value, editors)?.expression || String(value || '').trim(),
        (value, editors) => {
            const text = String(value || '').trim();
            return lookup(text, editors)?.text || text;
        },
        (value, editors) => lookup(value, editors)?.body ?? String(value || '').trim(),
        (value, editors) => {
            const record = lookup(value, editors);
            return record?.wrapped ? record : null;
        },
        (value, editors) => {
            const source = String(value || ''), editor = lookup(source, editors);
            return editor ? {
                display: editor.display,
                bindings: editor.bindings || [],
                shortened: editor.display !== editor.expression
            } :
                            {display: source, bindings: [], shortened: false};
        }
    ];
    const values = [undefined, null, false, 0, -0, NaN, 17, true, '', '  μ  ', '\uFEFFμ\u00a0', 'a\u0000b', '\ud800'];
    const fields = [undefined, null, false, 0, '', ' opaque source ', {opaque: true}];
    for (const input of values) {
        const key = String(input || '').trim();
        for (const field of fields) {
            const record =
                {expression: field, text: field, body: field, display: field, wrapped: field, bindings: field};
            const editors = new Map([[key, record]]);
            for (let kind = 0; kind < expected.length; ++kind) {
                const actual = native(kind, input, editors), reference = expected[kind](input, editors);
                if (kind !== 5) {
                    equal(actual, reference, 'boxed values and falsey/nullish selection for projection ' + kind);
                } else {
                    const compactActual = actual as EditorFixtureCompact;
                    const compactReference = reference as EditorFixtureCompact;
                    equal(compactActual.display, compactReference.display, 'compact display retains raw property type');
                    equal(compactActual.shortened, compactReference.shortened, 'compact shortened uses strict equality');
                    if (field)
                        equal(compactActual.bindings, compactReference.bindings, 'truthy binding identity retained');
                    else
                        check(
                            Array.isArray(compactActual.bindings) && !compactActual.bindings.length,
                            'falsey bindings become a new array');
                    equal(Object.keys(compactActual).join(), 'display,bindings,shortened', 'compact field order');
                }
            }
        }
        for (const absent of [undefined, null, false, 0, '']) {
            const editors = new Map([[key, absent]]);
            equal(native(0, input, editors), null, 'falsey metadata is absent');
            equal(native(1, input, editors), key, 'canonical fallback is trimmed');
            equal(native(2, input, editors), key, 'source fallback is trimmed');
            equal(native(3, input, editors), key, 'body fallback is trimmed');
            equal(native(4, input, editors), null, 'absent metadata is never wrapped');
            const compact = native(5, input, editors);
            equal(compact.display, String(input || ''), 'compact fallback preserves authored whitespace');
            equal(compact.shortened, false, 'unknown text is never reconstructed or abbreviated');
        }
    }
    for (const display of [undefined, null, NaN, 0, '']) {
        for (const expression of [undefined, null, NaN, 0, '']) {
            const projected = native(5, 'key', new Map([['key', {display, expression}]]));
            equal(projected.display, display, 'undefined, null and NaN remain distinct display values');
            equal(projected.shortened, display !== expression, 'strict inequality includes NaN and undefined/null');
        }
    }
    for (const kind of [-1, 6, 100]) equal(native(kind, 'key', new Map()), null, 'invalid projection is inert');

    const reads: string[] = [];
    let displays = 0;
    const record = {
        get display() {
            reads.push('display');
            return ++displays === 1 ? 'first visible text' : 'second visible text';
        },
        get bindings() {
            reads.push('bindings');
            return [];
        },
        get expression() {
            reads.push('expression');
            return 'second visible text';
        }
    };
    const projected = native(5, 'key', new Map([['key', record]]));
    equal(reads.join(), 'display,bindings,display,expression', 'observable metadata getter order preserved');
    equal(projected.display, 'first visible text', 'display is copied before later getters run');
    equal(projected.shortened, false, 'comparison uses the second display read');
    for (const kind of [1, 3]) {
        let conversions = 0;
        const value = {toString: () => ++conversions === 1 ? 'first' : 'fallback'};
        equal(native(kind, value, new Map()), 'fallback', 'fallback preserves repeated source conversion');
        equal(conversions, 2, 'lookup and fallback convert separately');
    }
    let conversions = 0;
    equal(native(2, {toString: () => (++conversions, ' source ')}, new Map()), 'source', 'restoration trims once');
    equal(conversions, 1, 'restoration captures its converted source once');

    const fieldsToSave = ['fullText', 'displayText', 'lastInput'] as const;
    const saved = Object.fromEntries(fieldsToSave.map(field => [field, WorksheetContext.labEditorState[field]]));
    const savedBody = WorksheetContext.expr.value;
    const key = '__native_editor_metadata_fixture__';
    const hadKey = WorksheetResults.labEvaluationView.caches.editors.has(key), savedMetadata = WorksheetResults.labEvaluationView.caches.editors.get(key);
    const errorMessage = (callback: () => unknown) => {
        try {
            callback();
        } catch (error) {
            return (error as Error).message;
        }
        throw new Error('Editor metadata: expected a bridge validation error');
    };
    try {
        const metadata = {
            expression: 'canonical native expression',
            text: 'restored full source',
            body: '',
            wrapped: true,
            bindings: [{name: 'μ', value: 'π/7'}],
            display: key
        };
        WorksheetResults.labEvaluationView.caches.editors.set(key, metadata);
        equal(WorksheetBindings.editor.labEditorData('  ' + key + '  '), metadata, 'public lookup adapter');
        equal(WorksheetBindings.editor.expressionWithSortedConstants(key), metadata.expression, 'public canonical adapter');
        equal(restoreCompactBindingValues(key), metadata.text, 'public source adapter');
        equal(WorksheetBindings.editor.expressionBodyForEditor(key), '', 'empty native body is not replaced by its source');
        equal(WorksheetBindings.editor.bindingParts(key), metadata, 'public binding adapter');
        equal(WorksheetBindings.editor.compactExpressionForEditor(key).bindings, metadata.bindings, 'public compact adapter retains bindings');

        WorksheetContext.labEditorState.displayText = key;
        WorksheetContext.labEditorState.fullText = '\uFEFFfull retained μ\u0000β';
        WorksheetContext.expr.value = ' \t' + key + '\n';
        equal(WorksheetBindings.editor.currentExpressionText(), WorksheetContext.labEditorState.fullText, 'matched source retains BOM and embedded NUL');
        const noMetadata = {
            get rawEditor() {
                return WorksheetBindings.controller.labEditorSnapshot();
            },
            get editors() {
                throw new Error('retained source must skip metadata lookup');
            }
        };
        equal(labDOM.call('lab_editor_current', noMetadata), WorksheetContext.labEditorState.fullText, 'retained-source lookup is lazy');
        WorksheetContext.labEditorState.displayText = 'different';
        equal(WorksheetBindings.editor.currentExpressionText(), metadata.text, 'changed display resolves complete native mapping');
        WorksheetContext.labEditorState.fullText = 'unrelated full source';
        WorksheetContext.expr.value = ' untouched opaque input ';
        equal(WorksheetBindings.editor.currentExpressionText(), 'untouched opaque input', 'unknown editor text is only trimmed');
        equal(WorksheetContext.expr.value, ' untouched opaque input ', 'reading never changes the editor');

        let customReads = 0;
        const custom = {
            get expressionText() {
                ++customReads;
                return 'custom source';
            }
        };
        equal(labDOM.call('lab_editor_current', custom), 'custom source', 'custom getter fallback remains supported');
        equal(customReads, 1, 'custom source read exactly once');
        for (const mode of ['equation', 'matrix', 'integrator', 'datetime', 'almanac', 'unknown']) {
            equal(
                labDOM.call('lab_editor_ready', [mode], {
                    get rawEditor() {
                        throw new Error('unexpected editor read');
                    },
                    get expressionText() {
                        throw new Error('unexpected editor read');
                    }
                }),
                true, 'non-Expression readiness never inspects editor text');
        }
        for (const value of ['', null, false, 0, 'source'])
            equal(
                labDOM.call('lab_editor_ready', ['expression'], {expressionText: value}), Boolean(value),
                'Expression readiness uses current source truthiness');

        WorksheetContext.expr.value = '\ud800';
        equal(
            errorMessage(WorksheetBindings.editor.currentExpressionText), 'Worksheet text contains an unpaired Unicode surrogate',
            'current-source bridge preserves invalid UTF-16 diagnostics');
        WorksheetContext.expr.value = 'x'.repeat(labWire.exports().lab_workspace_capacity() + 1);
        equal(
            errorMessage(WorksheetBindings.editor.currentExpressionText), 'Worksheet snapshot exceeds the native 4 MiB limit',
            'current-source bridge preserves the byte-size limit');
        equal(
            errorMessage(() => labDOM.call('lab_evaluation_setup', 0, 1, {}, {}, WorksheetResults.labEvaluationView)),
            'Worksheet snapshot exceeds the native 4 MiB limit', 'in-scope setup uses the same validation boundary');
    } finally {
        WorksheetContext.expr.value = savedBody;
        for (const field of fieldsToSave) WorksheetContext.labEditorState[field] = saved[field];
        if (hadKey)
            WorksheetResults.labEvaluationView.caches.editors.set(key, savedMetadata!);
        else
            WorksheetResults.labEvaluationView.caches.editors.delete(key);
    }
};
