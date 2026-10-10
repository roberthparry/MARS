/** Exact native editor metadata selection, scoped source resolution and lazy getter boundaries. */
window.checkLabEditor = function checkLabEditor() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Editor metadata: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const native = (kind, value, editors) => labDOM.call('lab_editor_metadata', kind, [value], editors);
    // Independent baseline oracle; the production client no longer contains these policy branches.
    const lookup = (value, editors) => editors.get(String(value || '').trim()) || null;
    const expected = [
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
                    equal(actual.display, reference.display, 'compact display retains raw property type');
                    equal(actual.shortened, reference.shortened, 'compact shortened uses strict equality');
                    if (field)
                        equal(actual.bindings, reference.bindings, 'truthy binding identity retained');
                    else
                        check(
                            Array.isArray(actual.bindings) && !actual.bindings.length,
                            'falsey bindings become a new array');
                    equal(Object.keys(actual).join(), 'display,bindings,shortened', 'compact field order');
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

    const reads = [];
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

    const fieldsToSave = ['fullText', 'displayText', 'lastInput'];
    const saved = Object.fromEntries(fieldsToSave.map(field => [field, labEditorState[field]]));
    const savedBody = expr.value;
    const key = '__native_editor_metadata_fixture__';
    const hadKey = labPresentationEditors.has(key), savedMetadata = labPresentationEditors.get(key);
    const errorMessage = callback => {
        try {
            callback();
        } catch (error) {
            return error.message;
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
        labPresentationEditors.set(key, metadata);
        equal(labEditorData('  ' + key + '  '), metadata, 'public lookup adapter');
        equal(expressionWithSortedConstants(key), metadata.expression, 'public canonical adapter');
        equal(restoreCompactBindingValues(key), metadata.text, 'public source adapter');
        equal(expressionBodyForEditor(key), '', 'empty native body is not replaced by its source');
        equal(bindingParts(key), metadata, 'public binding adapter');
        equal(compactExpressionForEditor(key).bindings, metadata.bindings, 'public compact adapter retains bindings');

        labEditorState.displayText = key;
        labEditorState.fullText = '\uFEFFfull retained μ\u0000β';
        expr.value = ' \t' + key + '\n';
        equal(currentExpressionText(), labEditorState.fullText, 'matched source retains BOM and embedded NUL');
        const noMetadata = {
            get rawEditor() {
                return labEditorSnapshot();
            },
            get editors() {
                throw new Error('retained source must skip metadata lookup');
            }
        };
        equal(labDOM.call('lab_editor_current', noMetadata), labEditorState.fullText, 'retained-source lookup is lazy');
        labEditorState.displayText = 'different';
        equal(currentExpressionText(), metadata.text, 'changed display resolves complete native mapping');
        labEditorState.fullText = 'unrelated full source';
        expr.value = ' untouched opaque input ';
        equal(currentExpressionText(), 'untouched opaque input', 'unknown editor text is only trimmed');
        equal(expr.value, ' untouched opaque input ', 'reading never changes the editor');

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

        expr.value = '\ud800';
        equal(
            errorMessage(currentExpressionText), 'Worksheet text contains an unpaired Unicode surrogate',
            'current-source bridge preserves invalid UTF-16 diagnostics');
        expr.value = 'x'.repeat(labWire.exports().lab_workspace_capacity() + 1);
        equal(
            errorMessage(currentExpressionText), 'Worksheet snapshot exceeds the native 4 MiB limit',
            'current-source bridge preserves the byte-size limit');
        equal(
            errorMessage(() => labDOM.call('lab_evaluation_setup', 0, 1, {}, {}, labEvaluationView)),
            'Worksheet snapshot exceeds the native 4 MiB limit', 'in-scope setup uses the same validation boundary');
    } finally {
        expr.value = savedBody;
        for (const field of fieldsToSave) labEditorState[field] = saved[field];
        if (hadKey)
            labPresentationEditors.set(key, savedMetadata);
        else
            labPresentationEditors.delete(key);
    }
};
