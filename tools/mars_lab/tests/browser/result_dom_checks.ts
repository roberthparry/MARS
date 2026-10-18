/**
 * @file result_dom_checks.ts
 * @brief Result-card projection and asynchronous ownership regressions.
 *
 * Supplies native presentation metadata to the real scoped bridge and verifies DOM changes without parsing mathematics in the tests.
 */

/** @brief Exercise replaceable clipboard feedback using owned timers and detached public callbacks. */
async function checkResultClipboardLifetime(check: LabTestCheck): Promise<void> {
    const timers = new Map<number, () => void>();
    const cancelled: number[] = [];
    const writes: string[] = [];
    let timer = 0;
    const first = document.createElement('button');
    const second = document.createElement('button');
    first.textContent = 'Copy first';
    second.textContent = 'Copy second';
    function createClipboard() {
        return {
            async writeText(text: string): Promise<void> {
                writes.push(text);
            }
        };
    }
    const owner = new WorksheetResults.WorksheetClipboardController({
        native() { return labDOM; },
        document() { return document; },
        clipboard: createClipboard,
        secure() { return true; },
        schedule(callback, delay) {
            check(delay === 1200, 'copy feedback retains its expiry interval');
            timers.set(++timer, callback);
            return timer;
        },
        cancel(id) {
            cancelled.push(id);
            timers.delete(id);
        },
        surface() { return '<svg>fixture</svg>'; },
        lastTeX() { return ''; }
    });
    const {flash, write, copyTextForTarget, dispose} = owner;
    try {
        check(copyTextForTarget('plot') === '<svg>fixture</svg>', 'detached copy-source callback retains its surface provider');
        await write('authored text');
        check(writes.join(',') === 'authored text', 'detached clipboard write uses the injected owner');
        flash(first, true);
        const replaced = timer;
        flash(second, true);
        const other = timer;
        flash(first, false);
        const replacement = timer;
        check(!timers.has(replaced) && timers.has(other) && timers.has(replacement) &&
              cancelled.join(',') === String(replaced), 'replacement cancels only that button\'s previous feedback');
        check(first.textContent === 'Failed' && second.textContent === 'Copied', 'feedback remains independent per button');
        const expire = timers.get(replacement);
        if (!expire) throw new Error('Clipboard fixture: missing expiry');
        timers.delete(replacement);
        expire();
        check(first.textContent === 'Copy first' && second.textContent === 'Copied',
              'expiry restores the original label without resetting another button');
        first.textContent = 'After expiry';
        dispose();
        check(timers.size === 0 && second.textContent === 'Copy second' && first.textContent === 'After expiry',
              'disposal restores pending feedback but no longer owns expired feedback');
        const cancellations = cancelled.length;
        dispose();
        check(cancelled.length === cancellations && !cancelled.includes(replacement),
              'completed feedback is retired and repeated disposal is inert');
    } finally {
        dispose();
    }
}

/**
 * Result-card C/WASM projection and asynchronous ownership regressions.
 * The parent runs checkLabResultDom sequentially after Lab readiness. Fixtures
 * contain native presentation metadata; these checks never parse mathematics.
 */
window.checkLabResultDom = async function checkLabResultDom() {
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error(`Result DOM: ${message}`);
    }
    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }
    await checkResultClipboardLifetime(check);
    function call(name: string, ...args: unknown[]): unknown {
        return labDOM.call('lab_result_' + name, ...args);
    }
    const caches = {
        numeric: new Map<string, boolean>(),
        solver: new Map<string, string>(),
        compact: new Map<string, string>(),
        editors: new Map<string, unknown>(),
        expansions: new Map<string, Map<string, boolean>>(),
        functions: new Map<string, unknown>(),
        headings: new Map<string, unknown>()
    };
    const host = document.createElement('div');
    const button = document.createElement('button');
    const saved = {
        lastTex: WorksheetWorkspace.presentation.state.lastTex,
        resultInputBindings: WorksheetWorkspace.presentation.state.resultInputBindings,
        renderTexSvg: WorksheetResults.actions.renderTexSvg,
        requestLabPresentation: WorksheetRequests.presentation.requestLabPresentation,
        clearFunctionRun: WorksheetRequests.functionCard.clearFunctionRun,
        setStatus: WorksheetWorkspace.presentation.setStatus,
        scheduleRenderedTeXFit: WorksheetResults.renderedFit.schedule,
        requestAnimationFrame: window.requestAnimationFrame,
        expanded: labWire.exports().lab_view_card_expanded()
    };
    const nodes = [
        WorksheetContext.parsed, WorksheetContext.functionStyle, WorksheetContext.value, WorksheetContext.rendered, WorksheetContext.parsedMore, WorksheetContext.functionMore, WorksheetContext.valueMore, WorksheetContext.renderedMore, WorksheetContext.resultUseInput,
        WorksheetContext.valueNote, WorksheetContext.valueNoteCard, WorksheetContext.valueTitle, WorksheetContext.mobileUrl, WorksheetContext.value.closest<HTMLElement>('.result-card')!
    ].filter((node): node is HTMLElement => node !== null);
    const snapshots: ResultDomSnapshot[] = [];
    for (const node of nodes) {
        const attributes: ResultDomSnapshot['attributes'] = [];
        for (const attribute of node.attributes)
            attributes.push({name: attribute.name, value: attribute.value});
        snapshots.push({
            node,
            attributes,
            children: [...node.childNodes],
            disabled: (node as ResultDomElement).disabled,
            matrixRequest: Object.getOwnPropertyDescriptor(node, 'labMatrixPresentationRequest'),
            renderRequest: Object.getOwnPropertyDescriptor(node, 'labResultRenderRequest')
        });
    }
    const tasks: Promise<unknown>[] = [];
    const pending: ResultDomRenderRequest[] = [];
    const matrixRequests: ((value: ResultDomPresentationResponse) => void)[] = [];
    const statuses: string[] = [];
    const svg = '<svg xmlns="http://www.w3.org/2000/svg" width="100" height="20"><text>native</text></svg>';
    const fullSvg = '<svg xmlns="http://www.w3.org/2000/svg" width="200" height="20"><text>full</text></svg>';
    function result(source = 'native TeX') {
        return {
            tex: source,
            display_TeX: 'short TeX',
            full_display_TeX: 'full TeX',
            svg,
            display_expression: 'native expression',
            presentation: {responsive_fit: true}
        };
    }
    function startDigits() {
        const task = WorksheetResults.actions.toggleRenderedDigits();
        tasks.push(task);
        return task;
    }
    try {
        window.requestAnimationFrame = () => 0;
        WorksheetResults.renderedFit.schedule = () => {};
        WorksheetRequests.functionCard.clearFunctionRun = () => {};
        WorksheetWorkspace.presentation.setStatus = text => statuses.push(text);

        // More than one handle table's capacity proves per-entry scratch scopes are released.
        call('presentation_install', caches, {
            presentation: {
                solution_lines: Array.from({length: 1400}, (_, i) => ({line: `line-${i}`, numeric: i % 2 === 0})),
                compact_texts: Array.from({length: 1100}, (_, i) => ({text: `text-${i}`, display: `display-${i}`})),
                editors: Array.from({length: 1100}, (_, i) => ({text: ` editor-${i} `, expression: `expression-${i}`})),
                expansions:
                    Array.from({length: 1100}, (_, i) => ({display: `short-${i}`, full: `full-${i}`, can_expand: true}))
            }
        });
        equal(caches.numeric.size, 1024, 'numeric cache capacity');
        check(!caches.numeric.has('line-0') && caches.numeric.has('line-1399'), 'numeric insertion-order eviction');
        equal(caches.compact.size, 1024, 'compact cache capacity');
        equal(caches.editors.size, 1024, 'editor aliases share one bounded cache');
        equal(
            caches.editors.get('editor-1099'), caches.editors.get('expression-1099'),
            'trimmed aliases retain identity');
        equal(caches.expansions.size, 1024, 'expansion source capacity');
        call('presentation_install', caches, {
            presentation:
                {expansions: Array.from({length: 20}, (_, i) => ({display: 'π…', full: `π-${i}`, can_expand: true}))}
        });
        equal(caches.expansions.get('π…')!.size, 16, 'per-source variant capacity');
        equal(call('can_expand', caches.expansions, 'π…', 'π-0'), 0, 'evicted variant cannot expand');
        equal(call('can_expand', caches.expansions, 'π…', 'π-19'), 1, 'exact current variant expands');
        call('presentation_install', caches, {
            presentation: {
                expansions: [
                    {display: 'literal...', full: 'literal full', can_expand: 'true'},
                    {display: 'native short', full: 'native full', can_expand: true}, null,
                    {display: 3, full: 'invalid'}
                ]
            }
        });
        equal(
            call('can_expand', caches.expansions, 'literal...', 'literal full'), 0,
            'truthy text is not native Boolean');
        equal(call('can_expand', caches.expansions, 'native short', 'native full'), 1, 'native Boolean accepted');
        call('presentation_install', caches, {presentation: {editors: {}, expansions: 'bad', solution_lines: {}}});
        for (let i = 0; i < 70; ++i)
            call('presentation_install', caches, {presentation: {solver_text: `solver-${i}`, solver_TeX: `TeX-${i}`}});
        equal(caches.solver.size, 64, 'solver capacity');
        check(!caches.solver.has('solver-0'), 'solver oldest entry evicted');
        call('syntax_install', caches, {
            presentation: {
                function_syntax:
                    Array.from({length: 70}, (_, i) => ({source: `function-${i}`, html: '', is_function: true})),
                matrix_headings: [{source: 'native full', html: '<b>native full</b>'}, null]
            }
        });
        equal(caches.functions.size, 64, 'syntax cache capacity');
        equal(call('is_function', caches.functions, 'function-69'), 1, 'native function classification');
        equal(call('is_function', caches.functions, 'function-0'), 0, 'evicted classification unavailable');

        const unsafe = '<img src=x onerror="throw 1">';
        call('native_text', host, unsafe, {source: 'different', html: '<b>wrong metadata</b>'});
        equal(host.textContent, unsafe, 'mismatched source is plain text');
        check(!host.querySelector('img,b'), 'fallback does not parse HTML');
        call('expandable', caches, host, button, 'native short', 'native full');
        check(!button.classList.contains('hidden'), 'native expansion flag enables button');
        equal(host.textContent, 'native short', 'initial compact text');
        call('text_digits', caches, host, button);
        equal(host.innerHTML, '<b>native full</b>', 'expanded native markup');
        equal(button.textContent, 'Show fewer digits', 'expanded label');
        call('text_digits', caches, host, button);
        equal(host.textContent, 'native short', 'collapse restores compact text');
        equal(button.dataset.expanded, 'false', 'collapse state');

        host.textContent = 'previous DateTime result';
        host.dataset.fullText = 'previous copy';
        button.textContent = 'untouched';
        for (const html of [undefined, '', ' \n\t', 5, {}]) {
            let error = '';
            try {
                WorksheetResults.calendarResults.renderDatetimeSections(host, button, [{rows: [{}], html}], 'new fallback');
            } catch (failure) {
                error = String(failure);
            }
            check(
                error.includes('DateTime presentation is missing') && error.includes('reload'),
                'DateTime recovery diagnostic');
            equal(host.textContent, 'previous DateTime result', 'missing metadata preserves old result');
            equal(host.dataset.fullText, 'previous copy', 'missing metadata preserves copy source');
            equal(button.textContent, 'untouched', 'missing metadata preserves button');
        }
        WorksheetResults.calendarResults.renderDatetimeSections(host, button, [{rows: [{}], html: '<section>Native date</section>'}], '  exact copy  ');
        equal(host.querySelector('.datetime-section-grid section')?.textContent, 'Native date', 'native date grid');
        equal(host.dataset.fullText, 'exact copy', 'DateTime native fallback is copy source');
        WorksheetResults.calendarResults.renderDatetimeSections(host, null, null, '  <plain date>  ');
        equal(host.textContent, '<plain date>', 'missing optional sections use trimmed plain fallback');
        check(!host.children.length, 'DateTime fallback remains text');
        for (const variant of [null, {}, {html: ' '}, {html: 3}]) {
            let failed = false;
            try {
                almanacPresentationVariant({almanac_presentation: {all: variant}}, 'all');
            } catch (error) {
                failed = String(error).includes('Almanac presentation is missing');
            }
            check(failed, 'Almanac missing HTML remains a hard presentation error');
        }
        const variant = {html: '<section>Native almanac</section>', copy_text: 'Native worksheet copy'};
        equal(almanacPresentationVariant({almanac_presentation: {all: variant}}, 'all'), variant, 'variant identity');
        call('almanac_render', host, variant);
        equal(host.dataset.copyText, variant.copy_text, 'Almanac native copy source');
        equal(host.innerHTML, variant.html, 'Almanac native markup');

        WorksheetResults.cards.displayMatrixResult({
            tex: 'short matrix TeX',
            full_TeX: 'full matrix TeX',
            svg,
            result: 'matrix input',
            expression_pretty: 'full matrix expression',
            display_expression_pretty: 'short matrix expression',
            function: 'matrix function',
            value: '(1,2)',
            value_svg: fullSvg,
            binding_values: [{name: 'x', value: 'π', constant: true}]
        });
        equal(WorksheetContext.parsed.textContent, 'short matrix expression', 'matrix expression display');
        equal(WorksheetContext.parsed.dataset.fullText, 'full matrix expression', 'matrix expression full source');
        equal(WorksheetContext.value.querySelector('svg text')?.textContent, 'full', 'matrix SVG value');
        equal(WorksheetResults.clipboard.copyTextForTarget('value'), '(1,2)', 'matrix copying uses native source');
        equal(WorksheetResults.clipboard.copyTextForTarget('rendered'), 'full matrix TeX', 'matrix full TeX copy');
        equal(WorksheetWorkspace.presentation.state.resultInputBindings[0].value, 'π', 'exact binding value retained');
        check(!WorksheetContext.value.closest('.result-card')!.hasAttribute('hidden'), 'matrix value visible');
        WorksheetResults.cards.displayCalculusResult({
            expression: 'integral expression',
            function: 'integral function',
            full_function: 'full integral function',
            value: '',
            value_title: 'Exact result',
            TeX: 'integral TeX',
            svg,
            wrapped_TeX: 'wrapped integral',
            wrapped_svg: fullSvg
        });
        equal(WorksheetContext.valueTitle.textContent, 'Exact result', 'calculus native value title');
        equal(WorksheetResults.clipboard.copyTextForTarget('function'), 'full integral function', 'calculus full function copy');
        check(WorksheetContext.value.closest('.result-card')!.hasAttribute('hidden'), 'absent numerical value hidden');
        equal(WorksheetWorkspace.presentation.state.resultInputBindings.length, 0, 'calculus clears matrix reuse bindings');
        equal(WorksheetContext.rendered.dataset.responsiveFit, 'true', 'calculus enables its native wrapped representation');

        WorksheetResults.cards.setRenderedResult(result());
        equal(WorksheetWorkspace.presentation.state.lastTex, 'native TeX', 'Expression copy source stays independent of displayed TeX');
        equal(WorksheetContext.rendered.dataset.displayTex, 'short TeX', 'native abbreviated TeX installed');
        equal(WorksheetContext.rendered.dataset.responsiveFit, 'true', 'native responsive-fit flag');
        equal(WorksheetResults.clipboard.copyTextForTarget('rendered'), 'native TeX', 'rendered copy source');
        WorksheetContext.rendered.classList.add('error');
        WorksheetContext.rendered.textContent = 'native diagnostic';
        equal(WorksheetResults.clipboard.copyTextForTarget('rendered'), 'native diagnostic', 'error copy policy');
        WorksheetContext.parsed.dataset.fullText = '  exact expression  ';
        equal(parsedExpressionText(), 'exact expression', 'expression copy trimming');
        WorksheetResults.reuse.setResultInputText('  reusable expression  ', [{name: 'a', value: '2'}]);
        equal(call('input_get', 'equation'), 'exact expression', 'equation reuse follows parsed source');
        equal(call('input_get', 'matrix'), 'reusable expression', 'matrix reuse follows explicit source');
        WorksheetResults.reuse.setResultInputText('   ');
        check(WorksheetContext.resultUseInput.disabled && WorksheetContext.resultUseInput.classList.contains('hidden'), 'empty reuse disabled');
        equal(WorksheetResults.clipboard.copyTextForTarget('unknown'), '', 'unknown copy target');
        if (WorksheetContext.mobileUrl) {
            for (const url of ['javascript:alert(1)', 'file:///tmp/test', 'HTTPS://example.test']) {
                WorksheetContext.mobileUrl.textContent = url;
                equal(WorksheetResults.clipboard.copyTextForTarget('mobile'), '', 'mobile scheme policy');
            }
            WorksheetContext.mobileUrl.textContent = '  https://example.test/private  ';
            equal(WorksheetResults.clipboard.copyTextForTarget('mobile'), 'https://example.test/private', 'mobile HTTP URL accepted');
        }
        button.textContent = 'Copy';
        call('copy_flash', button, 1);
        equal(button.textContent, 'Copied', 'copy success feedback');
        call('copy_flash', button, 2);
        equal(button.textContent, 'Failed', 'copy failure feedback');
        call('copy_flash', button, 0);
        equal(button.textContent, 'Copy', 'copy feedback restores original label');

        WorksheetResults.actions.renderTexSvg = function deferRender(source: string): Promise<ResultDomRenderResponse> {
            return new Promise<ResultDomRenderResponse>((resolve, reject) => {
                pending.push({source, resolve, reject});
            });
        };
        WorksheetResults.cards.setRenderedResult(result('first'));
        const old = startDigits();
        equal(pending[0].source, 'full TeX', 'digit renderer requests full native source');
        check(WorksheetContext.renderedMore.disabled, 'pending full render disables toggle');
        await startDigits();
        equal(pending.length, 1, 'duplicate digit request suppressed');
        WorksheetResults.cards.setRenderedResult(result('replacement'));
        const fresh = startDigits();
        pending[0].resolve({svg: '<svg><text>obsolete</text></svg>'});
        await old;
        check(WorksheetContext.renderedMore.disabled, 'obsolete completion cannot enable a newer pending toggle');
        equal(WorksheetContext.rendered.dataset.fullSvg, '', 'obsolete completion cannot populate new cache');
        pending[1].resolve({svg: fullSvg});
        await fresh;
        equal(WorksheetContext.rendered.dataset.fullSvg, fullSvg, 'current completion populates cache');
        equal(WorksheetContext.renderedMore.dataset.expanded, 'true', 'current completion expands');
        check(!WorksheetContext.renderedMore.disabled, 'current completion releases button');
        await startDigits();
        equal(WorksheetContext.renderedMore.dataset.expanded, 'false', 'rendered collapse');
        await startDigits();
        equal(pending.length, 2, 'rendered re-expansion reuses cached SVG');
        WorksheetResults.cards.setRenderedResult(result('failure'));
        const failed = startDigits();
        pending[2].reject(new Error('native render unavailable'));
        await failed;
        check(WorksheetContext.rendered.textContent!.includes('native render unavailable'), 'digit failure fallback');
        check(!WorksheetContext.renderedMore.disabled, 'digit failure releases button');
        WorksheetResults.cards.setRenderedResult(result('clear'));
        const cleared = startDigits();
        call('pane_clear');
        call('details_clear', WorksheetResults.labEvaluationView.caches, 'expression');
        pending[3].resolve({svg: fullSvg});
        await cleared;
        equal(WorksheetContext.rendered.textContent, '', 'cleared pane rejects late digit completion');
        check(!WorksheetContext.rendered.dataset.fullTex && !WorksheetContext.parsed.dataset.fullText, 'clear removes stale source metadata');
        equal(call('input_get', 'matrix'), '', 'clear removes reusable source');

        WorksheetRequests.presentation.requestLabPresentation = function deferPresentation(): Promise<ResultDomPresentationResponse> {
            return new Promise<ResultDomPresentationResponse>(resolve => {
                matrixRequests.push(resolve);
            });
        };
        const firstMatrix = WorksheetResults.cards.setMatrixPrettyResult('same source', 'same display', host, button);
        const secondMatrix = WorksheetResults.cards.setMatrixPrettyResult('same source', 'same display', host, button);
        tasks.push(firstMatrix, secondMatrix);
        matrixRequests[0]({html: '<b>obsolete matrix</b>'});
        await firstMatrix;
        equal(host.textContent, 'same display', 'same-source old matrix request rejected by identity');
        matrixRequests[1]({html: '<b>current matrix</b>'});
        await secondMatrix;
        equal(host.innerHTML, '<b>current matrix</b>', 'current matrix request accepted');
        const request = call('pretty_begin', caches, WorksheetContext.parsed, WorksheetContext.parsedMore, 'pending matrix', 'pending pretty');
        call('details_clear', WorksheetResults.labEvaluationView.caches, 'expression');
        call('pretty_finish', WorksheetContext.parsed, request, {html: '<b>late matrix</b>'});
        equal(WorksheetContext.parsed.textContent, '', 'cleared matrix result rejects late presentation');
        const superseded = call('pretty_begin', caches, host, button, 'identical', 'identical');
        call('expandable', caches, host, button, 'identical', 'identical');
        call('pretty_finish', host, superseded, {html: '<b>superseded</b>'});
        equal(host.textContent, 'identical', 'ordinary text replacement invalidates pending matrix markup');
        check(statuses.includes('Ready'), 'accepted asynchronous digit completion reports ready');
    } finally {
        call('render_invalidate');
        for (const request of pending) request.resolve({svg});
        for (const resolve of matrixRequests) resolve({});
        await Promise.allSettled(tasks);
        WorksheetResults.actions.renderTexSvg = saved.renderTexSvg;
        WorksheetRequests.presentation.requestLabPresentation = saved.requestLabPresentation;
        WorksheetRequests.functionCard.clearFunctionRun = saved.clearFunctionRun;
        WorksheetWorkspace.presentation.setStatus = saved.setStatus;
        WorksheetResults.renderedFit.schedule = saved.scheduleRenderedTeXFit;
        window.requestAnimationFrame = saved.requestAnimationFrame;
        WorksheetWorkspace.presentation.state.lastTex = saved.lastTex;
        WorksheetWorkspace.presentation.state.resultInputBindings = saved.resultInputBindings;
        for (const {node, attributes, children, disabled, matrixRequest, renderRequest} of snapshots) {
            for (const attribute of [...node.attributes]) node.removeAttribute(attribute.name);
            for (const {name, value} of attributes) node.setAttribute(name, value);
            node.replaceChildren(...children);
            if (disabled !== undefined)
                (node as ResultDomElement).disabled = disabled;
            for (const [key, descriptor] of [
                     ['labMatrixPresentationRequest', matrixRequest], ['labResultRenderRequest', renderRequest]] as const) {
                if (descriptor)
                    Object.defineProperty(node, key, descriptor);
                else
                    Reflect.deleteProperty(node, key);
            }
        }
        labWire.exports().lab_view_cards_collapse();
        if (saved.expanded >= 0)
            labWire.exports().lab_view_card_toggle(saved.expanded);
        renderResultCardExpansion();
    }
};
