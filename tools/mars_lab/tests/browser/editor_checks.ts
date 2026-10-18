/**
 * @file editor_checks.ts
 * @brief Native editor metadata and lazy source-selection regressions.
 *
 * Exercises metadata lookup against an independent fixture oracle, including malformed inputs and getter evaluation boundaries.
 */

/** @brief Check exact metadata selection, scoped source resolution and lazy getter boundaries. */
window.checkLabEditor = function checkLabEditor(): void {
    /** @brief Report a failed editor metadata assertion. */
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('Editor metadata: ' + message);
    }

    /** @brief Compare values without changing their identity or falsey distinctions. */
    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }

    /** @brief Run one native projection, including malformed fixture inputs. */
    function native(kind: number, value: unknown, editors: Map<string, unknown>): unknown {
        return labDOM.call('lab_editor_metadata', kind, [value], editors);
    }

    /** @brief Read the concrete result shape of the compact projection. */
    function nativeCompact(value: unknown, editors: Map<string, unknown>): EditorFixtureCompact {
        return native(5, value, editors) as EditorFixtureCompact;
    }

    // Independent baseline oracle; the production client no longer contains these policy branches.
    /** @brief Find an oracle record using the original falsey and whitespace rules. */
    function lookup(value: unknown, editors: Map<string, EditorFixtureMetadata>): EditorFixtureMetadata | null {
        return editors.get(String(value || '').trim()) || null;
    }

    /** @brief Select canonical text with a separately converted fallback. */
    function expectedExpression(value: unknown, editors: Map<string, EditorFixtureMetadata>): unknown {
        return lookup(value, editors)?.expression || String(value || '').trim();
    }

    /** @brief Capture source text once before looking up its restoration metadata. */
    function expectedSource(value: unknown, editors: Map<string, EditorFixtureMetadata>): unknown {
        const text = String(value || '').trim();
        return lookup(text, editors)?.text || text;
    }

    /** @brief Preserve an explicitly empty body while falling back for nullish values. */
    function expectedBody(value: unknown, editors: Map<string, EditorFixtureMetadata>): unknown {
        return lookup(value, editors)?.body ?? String(value || '').trim();
    }

    /** @brief Admit only records with truthy wrapped metadata. */
    function expectedWrapped(value: unknown, editors: Map<string, EditorFixtureMetadata>): EditorFixtureMetadata | null {
        const record = lookup(value, editors);
        return record?.wrapped ? record : null;
    }

    /** @brief Reproduce compact field order and observable getter evaluation order. */
    function expectedCompact(value: unknown, editors: Map<string, EditorFixtureMetadata>): EditorFixtureCompact {
        const source = String(value || '');
        const editor = lookup(source, editors);
        if (!editor)
            return {display: source, bindings: [], shortened: false};
        return {
            display: editor.display,
            bindings: editor.bindings || [],
            shortened: editor.display !== editor.expression
        };
    }

    const expected: EditorFixtureProjection[] = [
        lookup, expectedExpression, expectedSource, expectedBody, expectedWrapped, expectedCompact
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
            const compact = nativeCompact(input, editors);
            equal(compact.display, String(input || ''), 'compact fallback preserves authored whitespace');
            equal(compact.shortened, false, 'unknown text is never reconstructed or abbreviated');
        }
    }
    for (const display of [undefined, null, NaN, 0, '']) {
        for (const expression of [undefined, null, NaN, 0, '']) {
            const projected = nativeCompact('key', new Map([['key', {display, expression}]]));
            equal(projected.display, display, 'undefined, null and NaN remain distinct display values');
            equal(projected.shortened, display !== expression, 'strict inequality includes NaN and undefined/null');
        }
    }
    for (const kind of [-1, 6, 100])
        equal(native(kind, 'key', new Map()), null, 'invalid projection is inert');

    const reads: string[] = [];
    let displays = 0;
    const record = {
        /** @brief Expose distinct display values so repeated reads remain observable. */
        get display() {
            reads.push('display');
            return ++displays === 1 ? 'first visible text' : 'second visible text';
        },
        /** @brief Record when compact projection reads the binding list. */
        get bindings() {
            reads.push('bindings');
            return [];
        },
        /** @brief Record the expression read used by the shortened comparison. */
        get expression() {
            reads.push('expression');
            return 'second visible text';
        }
    };
    const projected = nativeCompact('key', new Map([['key', record]]));
    equal(reads.join(), 'display,bindings,display,expression', 'observable metadata getter order preserved');
    equal(projected.display, 'first visible text', 'display is copied before later getters run');
    equal(projected.shortened, false, 'comparison uses the second display read');
    for (const kind of [1, 3]) {
        let conversions = 0;
        const value = {
            /** @brief Return different text for lookup and fallback conversions. */
            toString(): string {
                ++conversions;
                return conversions === 1 ? 'first' : 'fallback';
            }
        };
        equal(native(kind, value, new Map()), 'fallback', 'fallback preserves repeated source conversion');
        equal(conversions, 2, 'lookup and fallback convert separately');
    }
    let conversions = 0;
    const sourceValue = {
        /** @brief Count source conversions while retaining authored whitespace. */
        toString(): string {
            ++conversions;
            return ' source ';
        }
    };
    equal(native(2, sourceValue, new Map()), 'source', 'restoration trims once');
    equal(conversions, 1, 'restoration captures its converted source once');

    const fieldsToSave = ['fullText', 'displayText', 'lastInput'] as const;
    const saved = {
        fullText: WorksheetContext.labEditorState.fullText,
        displayText: WorksheetContext.labEditorState.displayText,
        lastInput: WorksheetContext.labEditorState.lastInput
    };
    const savedBody = WorksheetContext.expr.value;
    const key = '__native_editor_metadata_fixture__';
    const hadKey = WorksheetResults.labEvaluationView.caches.editors.has(key), savedMetadata = WorksheetResults.labEvaluationView.caches.editors.get(key);
    /** @brief Capture the bridge error without swallowing an unexpected success. */
    function errorMessage(callback: () => unknown): string {
        try {
            callback();
        } catch (error) {
            return (error as Error).message;
        }
        throw new Error('Editor metadata: expected a bridge validation error');
    }

    /** @brief Exercise setup validation with the same editor snapshot as public reads. */
    function setupEditor(): unknown {
        return labDOM.call('lab_evaluation_setup', 0, 1, {}, {}, WorksheetResults.labEvaluationView);
    }
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
            /** @brief Supply the retained source through the real editor snapshot. */
            get rawEditor() {
                return WorksheetBindings.controller.labEditorSnapshot();
            },
            /** @brief Fail if retained source unnecessarily consults metadata. */
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
            /** @brief Count custom source reads without changing the returned text. */
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
                    /** @brief Fail if a non-Expression readiness check inspects the editor. */
                    get rawEditor() {
                        throw new Error('unexpected editor read');
                    },
                    /** @brief Fail if a non-Expression readiness check resolves source text. */
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
            errorMessage(setupEditor),
            'Worksheet snapshot exceeds the native 4 MiB limit', 'in-scope setup uses the same validation boundary');
    } finally {
        WorksheetContext.expr.value = savedBody;
        for (const field of fieldsToSave)
            WorksheetContext.labEditorState[field] = saved[field];
        if (hadKey)
            WorksheetResults.labEvaluationView.caches.editors.set(key, savedMetadata!);
        else
            WorksheetResults.labEvaluationView.caches.editors.delete(key);
    }
};
