/** Native Function/matrix metadata and DOM checks; run sequentially after Lab readiness. */
window.checkLabSyntax = async function checkLabSyntax() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error(`Function syntax: ${message}`);
    };
    const savedMode = currentMode();
    const host = document.createElement('div');
    document.body.appendChild(host);
    try {
        const removed = await labFetch('/js/result_text.js');
        check(
            removed.status === 404 && !labDefinitionScripts.includes('result_text'),
            'obsolete result-text script remains');
        const removedEditor = await labFetch('/js/editor.js');
        check(
            removedEditor.status === 404 && !labDefinitionScripts.includes('editor'), 'obsolete editor script remains');
        setMode('expression');
        labRequests.modeChanged('expression');
        const originalFetch = labFetch;
        const queued = [], preparations = [];
        const fixture = 'editor context regression fixture';
        const reply = () =>
            ({ok: true, labData: async () => ({ok: true, editor: {text: fixture, body: 'new context'}})});
        try {
            labFetch = (url) => {
                check(url === '/presentation', 'unexpected request during editor fixture');
                return new Promise(resolve => queued.push(resolve));
            };
            const old = prepareLabEditor(fixture).catch(error => error);
            preparations.push(old);
            setMode('matrix');
            setMode('expression');
            const fresh = prepareLabEditor(fixture);
            preparations.push(fresh);
            check(queued.length === 2, 'new context reused stale editor preparation');
            queued[0](reply());
            check((await old)?.name === 'AbortError', 'old preparation was not rejected');
            check(!labPresentationEditors.has(fixture), 'stale preparation populated the editor cache');
            const shared = prepareLabEditor(fixture);
            preparations.push(shared);
            check(queued.length === 2, 'old finaliser removed the fresh pending request');
            queued[1](reply());
            const [first, second] = await Promise.all([fresh, shared]);
            check(first === second && first.body === 'new context', 'same-context preparation was not shared');
            check(!labPresentationEditorRequests.has(fixture), 'completed preparation retained its request');
        } finally {
            queued.forEach(resolve => resolve(reply()));
            await Promise.allSettled(preparations);
            labFetch = originalFetch;
            labPresentationEditors.delete(fixture);
        }
        await labRequests.run('evaluate', 'expression', async request => {
            const {response, data} = await fetchEvaluation('{ x+2 | x = 3 }', '', '', '', '', request);
            check(labRequests.outcome(request, response, data) === 2, 'native evaluation failed');
            const sources =
                [data.display_function || data.function || '', data.full_display_function || data.function || ''];
            for (const source of sources) {
                check(
                    source && !!labDOM.call('lab_result_is_function', labFunctionSyntax, String(source || '')),
                    'exact combined Function source has no lexical metadata');
                labDOM.call('lab_result_text_render', labResultCaches, host, String(source || ''), 1);
                check(host.textContent === source, 'highlighting changed full native text, declarations or whitespace');
                check(host.querySelector('.function-token-keyword'), 'native keyword span missing');
                check(host.querySelector('.function-token-number'), 'native number span missing');
                const entry = labFunctionSyntax.get(source);
                check(
                    typeof entry.html === 'string' && host.innerHTML === entry.html,
                    'browser did not install the native Function markup');
                check(
                    entry.spans.map(span => span.text).join('') === source,
                    'native spans do not cover the entire card');
            }
        });
        const {response, data} = await fetchMatrixEvaluation(
            {matrixText: '(1,0;0,2)', operation: 'eigendecompose', operand: '', skipSave: true});
        check(response.ok && data.ok !== false, 'native matrix evaluation failed');
        const entries = data.presentation?.matrix_headings || [];
        const headingEntry = entries.find(entry => entry.spans.some(span => span.kind === 'heading'));
        check(headingEntry, 'native eigendecomposition lacks section heading metadata');
        for (const entry of entries) {
            labDOM.call('lab_result_text_render', labResultCaches, host, String(entry.source || ''), 2);
            check(
                host.textContent ===
                    entry.spans.map(span => span.kind === 'heading' ? span.display : span.text).join(''),
                'matrix DOM differs from native presentation spans');
            check(
                host.querySelectorAll('.matrix-section-heading').length ===
                    entry.spans.filter(span => span.kind === 'heading').length,
                'matrix heading classes differ from native metadata');
        }
        const unknown = ' EIGENVALUES\n<em>not native metadata</em>';
        await setMatrixPrettyResult('1.5·(1/3, f(x,y); 1e-20, [a+b]) + (2, 3; 4, 5)', '', host);
        check(
            host.querySelectorAll('.matrix-term-display').length === 2 &&
                host.querySelectorAll('.matrix-cell').length === 8 &&
                host.querySelector('.matrix-factor').textContent === '1.5' &&
                host.querySelectorAll('.matrix-cell')[1].textContent === 'f(x,y)',
            'native matrix markup lost factors, sums or exact cells');
        check(
            host.querySelector('.matrix-grid').style.getPropertyValue('--matrix-columns') === '2' &&
                getComputedStyle(host.querySelector('.matrix-grid')).gridTemplateColumns.split(' ').length === 2,
            'native matrix dimensions did not reach CSS layout');
        await setMatrixPrettyResult('(<img src=x>, &; μ, 4)', '', host);
        check(
            !host.querySelector('img') && host.querySelector('.matrix-cell').textContent === '<img src=x>',
            'matrix cells were interpreted as HTML');
        await setMatrixPrettyResult('(1,2;3)', 'ragged fallback', host);
        check(
            host.textContent === 'ragged fallback' && !host.querySelector('.matrix-grid'),
            'ragged matrix lost fallback');
        labDOM.call('lab_result_text_render', labResultCaches, host, String(unknown || ''), 2);
        check(
            host.textContent === unknown && !host.querySelector('span, em'),
            'unknown text must stay literal and unparsed');
        const saved = labMatrixHeadings.get(headingEntry.source);
        try {
            labMatrixHeadings.set(
                headingEntry.source, {source: headingEntry.source, spans: [{text: 'stale', kind: 'heading'}]});
            labDOM.call('lab_result_text_render', labResultCaches, host, String(headingEntry.source || ''), 2);
            check(
                host.textContent === headingEntry.source && !host.querySelector('span'),
                'stale spans must fall back to exact text');
        } finally {
            labMatrixHeadings.set(headingEntry.source, saved);
        }
        const integralResponse = await labFetch('/integrator-eval', {
            method: 'POST',
            body: labWire.encode({expression: 'x', bounds: [{name: 'x', lo: '0', hi: '1'}], precision: 17})
        });
        const integralData = await integralResponse.labData();
        check(
            integralResponse.ok && integralData.ok !== false,
            `native integrator evaluation failed: ${integralData.error || integralResponse.status}`);
        const integralText = integralData.presentation?.integrator;
        check(
            typeof integralText?.detail_text === 'string' && typeof integralText?.value_text === 'string',
            'native integrator card text metadata missing');
        check(
            integralText.detail_text.length > 0 && integralText.value_text.length > 0,
            'native integrator card text empty');
        labDOM.call('lab_result_text_render', labResultCaches, host, String(integralText.detail_text || ''), 0);
        check(host.textContent === integralText.detail_text, 'integrator detail DOM changed native text');
        labDOM.call('lab_result_text_render', labResultCaches, host, String(integralText.value_text || ''), 0);
        check(host.textContent === integralText.value_text, 'integrator value DOM changed native text');
    } finally {
        host.remove();
        setMode(savedMode);
        labRequests.modeChanged(savedMode);
        syncModeUI();
    }
};
