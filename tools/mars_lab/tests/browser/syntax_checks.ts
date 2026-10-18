/**
 * @file syntax_checks.ts
 * @brief Function and matrix metadata presentation regressions.
 *
 * Exercises native syntax metadata and browser rendering after Lab readiness, including retired-script route checks.
 */

/** @brief Check native Function/matrix metadata and DOM sequentially after Lab readiness. */
window.checkLabSyntax = async function checkLabSyntax(): Promise<void> {
    /** @brief Report a failed syntax presentation assertion. */
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error(`Function syntax: ${message}`);
    }

    /** @brief Join every native span, optionally using its displayed matrix heading. */
    function spanText(spans: SyntaxFixtureSpan[], displayHeadings: boolean): string {
        let text = '';
        for (const span of spans) {
            const value = displayHeadings && span.kind === 'heading' ? span.display : span.text;
            text += value ?? '';
        }
        return text;
    }

    /** @brief Count headings while visiting each span of one small native fixture. */
    function headingCount(entry: SyntaxFixtureEntry): number {
        let count = 0;
        for (const span of entry.spans) {
            if (span.kind === 'heading')
                ++count;
        }
        return count;
    }

    /** @brief Select a heading-bearing entry from the small eigendecomposition fixture. */
    function hasHeading(entry: SyntaxFixtureEntry): boolean {
        return headingCount(entry) > 0;
    }

    /** @brief Retain a rejected preparation for its explicit stale-context assertion. */
    function preparationError(error: unknown): unknown {
        return error;
    }
    const savedMode = WorksheetWorkspace.mode.currentMode();
    const host = document.createElement('div');
    document.body.appendChild(host);
    try {
        const removed = await labHttp.fetch('/js/result_text.js');
        check(
            removed.status === 404 && !labApplication.definitionScripts.includes('result_text'),
            'obsolete result-text script remains');
        const removedEditor = await labHttp.fetch('/js/editor.js');
        check(
            removedEditor.status === 404 && !labApplication.definitionScripts.includes('editor'), 'obsolete editor script remains');
        WorksheetWorkspace.mode.setMode('expression');
        WorksheetRequests.labRequests.modeChanged('expression');
        const originalFetch = labHttp.fetch;
        const queued: ((response: MarsTransportResponse) => void)[] = [];
        const preparations: Promise<unknown>[] = [];
        const fixture = 'editor context regression fixture';
        /** @brief Return a fresh response whose payload is decoded asynchronously. */
        function reply(): MarsTransportResponse {
            return Object.assign(new Response(), {
                /** @brief Supply the fixed editor metadata after transport completion. */
                async labData() {
                    return {ok: true, editor: {text: fixture, body: 'new context'}};
                }
            });
        }

        try {
            /** @brief Hold presentation responses until the scenario chooses their order. */
            labHttp.fetch = function fetchPresentation(url): Promise<MarsTransportResponse> {
                check(url === '/presentation', 'unexpected request during editor fixture');
                return new Promise(function queueResponse(resolve) {
                    queued.push(resolve);
                });
            };
            const old = WorksheetRequests.presentation.prepareLabEditor(fixture).catch(preparationError);
            preparations.push(old);
            WorksheetWorkspace.mode.setMode('matrix');
            WorksheetWorkspace.mode.setMode('expression');
            const fresh = WorksheetRequests.presentation.prepareLabEditor(fixture);
            preparations.push(fresh);
            check(queued.length === 2, 'new context reused stale editor preparation');
            queued[0](reply());
            check(((await old) as Error | undefined)?.name === 'AbortError', 'old preparation was not rejected');
            check(!WorksheetResults.labEvaluationView.caches.editors.has(fixture), 'stale preparation populated the editor cache');
            const shared = WorksheetRequests.presentation.prepareLabEditor(fixture);
            preparations.push(shared);
            check(queued.length === 2, 'old finaliser removed the fresh pending request');
            queued[1](reply());
            const [first, second] = await Promise.all([fresh, shared]);
            check(first === second && first.body === 'new context', 'same-context preparation was not shared');
            check(!WorksheetRequests.presentation.pending.has(fixture), 'completed preparation retained its request');
        } finally {
            for (const resolve of queued)
                resolve(reply());
            await Promise.allSettled(preparations);
            labHttp.fetch = originalFetch;
            WorksheetResults.labEvaluationView.caches.editors.delete(fixture);
        }
        /** @brief Check exact function text, native token coverage and installed markup. */
        async function checkFunction(request: WorksheetRequests.MarsApiRequest): Promise<void> {
            const {response, data} = await WorksheetRequests.evaluationTransport.fetchEvaluation('{ x+2 | x = 3 }', '', '', '', '', request);
            check(WorksheetRequests.labRequests.outcome(request, response, data) === 2, 'native evaluation failed');
            const sources =
                [data.display_function || data.function || '', data.full_display_function || data.function || ''];
            for (const source of sources) {
                check(
                    source && !!labDOM.call('lab_result_is_function', WorksheetResults.labEvaluationView.caches.functions, String(source || '')),
                    'exact combined Function source has no lexical metadata');
                labDOM.call('lab_result_text_render', WorksheetResults.labEvaluationView.caches, host, String(source || ''), 1);
                check(host.textContent === source, 'highlighting changed full native text, declarations or whitespace');
                check(host.querySelector('.function-token-keyword'), 'native keyword span missing');
                check(host.querySelector('.function-token-number'), 'native number span missing');
                const entry = WorksheetResults.labEvaluationView.caches.functions.get(source as string) as SyntaxFixtureEntry;
                check(
                    typeof entry.html === 'string' && host.innerHTML === entry.html,
                    'browser did not install the native Function markup');
                check(
                    spanText(entry.spans, false) === source,
                    'native spans do not cover the entire card');
            }
        }
        await WorksheetRequests.labRequests.run('evaluate', 'expression', checkFunction);

        /** @brief Check native small-value notation without changing the source metadata. */
        async function checkSmallValue(request: WorksheetRequests.MarsApiRequest): Promise<void> {
            const {response, data} = await WorksheetRequests.evaluationTransport.fetchEvaluation('exp(-20)', '', '', '', '', request);
            check(WorksheetRequests.labRequests.outcome(request, response, data) === 2, 'small-value evaluation failed');
            labDOM.call('lab_result_text_render', WorksheetResults.labEvaluationView.caches, host, data.value, 2);
            check(host.textContent.includes('×10⁻⁹'), 'Value card retained machine exponent notation');
            check(!/[eE][+-]?\d/.test(host.textContent), 'Value card contains an e/E exponent');
            check((WorksheetResults.labEvaluationView.caches.headings.get(data.value as string) as SyntaxFixtureEntry | undefined)?.source === data.value, 'raw numeric source was changed');
        }
        await WorksheetRequests.labRequests.run('evaluate', 'expression', checkSmallValue);
        const {response, data} = await WorksheetRequests.evaluationTransport.fetchMatrixEvaluation(
            {matrixText: '(1,0;0,2)', operation: 'eigendecompose', operand: '', skipSave: true}) as SyntaxFixtureResult;
        check(response.ok && data.ok !== false, 'native matrix evaluation failed');
        const entries = data.presentation?.matrix_headings || [];
        const headingEntry = entries.find(hasHeading)!;
        check(headingEntry, 'native eigendecomposition lacks section heading metadata');
        for (const entry of entries) {
            labDOM.call('lab_result_text_render', WorksheetResults.labEvaluationView.caches, host, String(entry.source || ''), 2);
            check(
                host.textContent === spanText(entry.spans, true),
                'matrix DOM differs from native presentation spans');
            check(
                host.querySelectorAll('.matrix-section-heading').length ===
                    headingCount(entry),
                'matrix heading classes differ from native metadata');
        }
        const unknown = ' EIGENVALUES\n<em>not native metadata</em>';
        await WorksheetResults.cards.setMatrixPrettyResult('1.5·(1/3, f(x,y); 1e-20, [a+b]) + (2, 3; 4, 5)', '', host);
        check(
            host.querySelectorAll('.matrix-term-display').length === 2 &&
                host.querySelectorAll('.matrix-cell').length === 8 &&
                host.querySelector<HTMLElement>('.matrix-factor')!.textContent === '1.5' &&
                host.querySelectorAll('.matrix-cell')[1].textContent === 'f(x,y)',
            'native matrix markup lost factors, sums or exact cells');
        check(
            host.querySelector<HTMLElement>('.matrix-grid')!.style.getPropertyValue('--matrix-columns') === '2' &&
                getComputedStyle(host.querySelector<HTMLElement>('.matrix-grid')!).gridTemplateColumns.split(' ').length === 2,
            'native matrix dimensions did not reach CSS layout');
        await WorksheetResults.cards.setMatrixPrettyResult('(<img src=x>, &; μ, 4)', '', host);
        check(
            !host.querySelector('img') && host.querySelector<HTMLElement>('.matrix-cell')!.textContent === '<img src=x>',
            'matrix cells were interpreted as HTML');
        await WorksheetResults.cards.setMatrixPrettyResult('(1,2;3)', 'ragged fallback', host);
        check(
            host.textContent === 'ragged fallback' && !host.querySelector<HTMLElement>('.matrix-grid')!,
            'ragged matrix lost fallback');
        labDOM.call('lab_result_text_render', WorksheetResults.labEvaluationView.caches, host, String(unknown || ''), 2);
        check(
            host.textContent === unknown && !host.querySelector('span, em'),
            'unknown text must stay literal and unparsed');
        const saved = WorksheetResults.labEvaluationView.caches.headings.get(headingEntry.source);
        try {
            WorksheetResults.labEvaluationView.caches.headings.set(
                headingEntry.source, {source: headingEntry.source, spans: [{text: 'stale', kind: 'heading'}]});
            labDOM.call('lab_result_text_render', WorksheetResults.labEvaluationView.caches, host, String(headingEntry.source || ''), 2);
            check(
                host.textContent === headingEntry.source && !host.querySelector('span'),
                'stale spans must fall back to exact text');
        } finally {
            WorksheetResults.labEvaluationView.caches.headings.set(headingEntry.source, saved);
        }
        const integralResponse = await labHttp.fetch('/integrator-eval', {
            method: 'POST',
            body: labWire.encode({expression: 'x', bounds: [{name: 'x', lo: '0', hi: '1'}], precision: 17})
        });
        const integralData = await integralResponse.labData() as SyntaxFixtureData;
        check(
            integralResponse.ok && integralData.ok !== false,
            `native integrator evaluation failed: ${integralData.error || integralResponse.status}`);
        const integralText = integralData.presentation?.integrator!;
        check(
            typeof integralText?.detail_text === 'string' && typeof integralText?.value_text === 'string',
            'native integrator card text metadata missing');
        check(
            integralText.detail_text.length > 0 && integralText.value_text.length > 0,
            'native integrator card text empty');
        labDOM.call('lab_result_text_render', WorksheetResults.labEvaluationView.caches, host, String(integralText.detail_text || ''), 0);
        check(host.textContent === integralText.detail_text, 'integrator detail DOM changed native text');
        labDOM.call('lab_result_text_render', WorksheetResults.labEvaluationView.caches, host, String(integralText.value_text || ''), 0);
        check(host.textContent === integralText.value_text, 'integrator value DOM changed native text');
    } finally {
        host.remove();
        WorksheetWorkspace.mode.setMode(savedMode);
        WorksheetRequests.labRequests.modeChanged(savedMode);
        WorksheetWorkspace.presentation.syncModeUI();
    }
};
