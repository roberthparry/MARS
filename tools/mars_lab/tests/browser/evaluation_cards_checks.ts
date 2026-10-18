/**
 * @file evaluation_cards_checks.ts
 * @brief Native evaluation-card selection and DOM projection regressions.
 *
 * Supplies presentation caches and native result fixtures to verify card visibility, content and browser projection boundaries.
 */

/** Native evaluation-card selection and real DOM projection checks; run sequentially after Lab readiness. */
window.checkLabEvaluationCards = async function checkLabEvaluationCards() {
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error(`Evaluation cards: ${message}`);
    }
    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }
    const caches = {
        editors: new Map([['native full', {display: 'native compact'}]]),
        expansions: new Map([['short TeX', new Map([['full TeX', true]])]]),
        solver: new Map([['solver: native solver\nstatus: reduced\nreason', 'cached solver TeX']])
    };
    function plan(mode: number, data: unknown = {}, text = ''): EvaluationCardsFixture {
        return labDOM.call<EvaluationCardsFixture>('lab_evaluation_cards', mode, data, text, caches);
    }
    const selectors = [
        '#rendered', '#renderedMore', '#parsed', '#parsedMore', '#functionStyle', '#functionMore', '#value',
        '#valueMore', '#valueCard', '#valueTitle', '#valueNote', '#valueNoteCard', '#resultUseInput', '#targetRow',
        '#goalTarget', '#goalSeek', '#resultPane', '#helpPane', '#rightPaneTitle', '#help', '#status'
    ];
    const snapshots: EvaluationCardsSnapshot[] = [];
    for (const selector of selectors) {
        const node = document.querySelector<HTMLElement & {value?: string}>(selector);
        if (node === null)
            continue;
        const attributes: [string, string][] = [];
        for (const attribute of node.attributes)
            attributes.push([attribute.name, attribute.value]);
        snapshots.push({node, children: [...node.childNodes], value: node.value, attributes});
    }
    const saved = {
        lastTex: WorksheetWorkspace.presentation.state.lastTex, resultInputBindings: WorksheetWorkspace.presentation.state.resultInputBindings, clearFunctionRun: WorksheetRequests.functionCard.clearFunctionRun, scheduleRenderedTeXFit: WorksheetResults.renderedFit.schedule,
        requestAnimationFrame: window.requestAnimationFrame, focus: document.activeElement,
        currentMode: WorksheetWorkspace.mode.currentMode, currentExpressionText: WorksheetBindings.editor.currentExpressionText, runGoalSeek: WorksheetRequests.goals.runGoalSeek,
        expanded: labWire.exports().lab_view_card_expanded(),
        renderRequest: Object.getOwnPropertyDescriptor(WorksheetContext.rendered, 'labResultRenderRequest'),
        matrixRequests: [WorksheetContext.parsed, WorksheetContext.functionStyle, WorksheetContext.value].map(node =>
            [node, Object.getOwnPropertyDescriptor(node, 'labMatrixPresentationRequest')] as const)
    };
    const host = document.createElement('div');
    try {
        window.requestAnimationFrame = () => 0;
        WorksheetResults.renderedFit.schedule = () => {};
        WorksheetRequests.functionCard.clearFunctionRun = () => {};
        const data = {
            expression: 'native full', editor_expression: 'native editor', function: 'native function', value: '1',
            full_display_function: 'native full function', differentiable: ' NO ', value_note: 'native note',
            presentation: {calculus: {derivative: {expression: 'native derivative'}}}
        };
        let cards = plan(0, data);
        equal(cards.expression, 'native compact', 'native editor metadata supplies compact display');
        equal(cards.full_expression, 'native full', 'full symbolic expression retained');
        equal(cards.input, 'native editor', 'explicit editor source has reuse precedence');
        equal(cards.derivative, 'native derivative', 'native derivative metadata retained');
        equal(cards.differentiable, false, 'native differentiability flag normalised');
        WorksheetResults.cards.installEvaluationTextCards(cards);
        labDOM.call('lab_evaluation_cards_present', 0, data);
        equal(WorksheetContext.parsed.textContent, 'native compact', 'selected display reaches real expression card');
        equal(WorksheetContext.functionStyle.textContent, 'native function', 'native Function card text');
        equal(WorksheetResults.clipboard.copyTextForTarget('function'), 'native full function', 'full Function source remains available');
        equal(WorksheetContext.value.textContent, '1', 'binding-independent value reaches numerical card');
        equal(WorksheetContext.valueNote.textContent, 'native note', 'note installed after value clearing');
        check(!WorksheetContext.valueCard.classList.contains('hidden'), 'non-empty numerical value shown');
        await window.checkLabMobileLayout('numerical value and evaluation note');
        cards = plan(0, {...data, value: '  ', root_value: true});
        WorksheetResults.cards.installEvaluationTextCards(cards);
        labDOM.call('lab_evaluation_cards_present', 0, {...data, root_value: true});
        equal(WorksheetContext.valueTitle.textContent, 'Values', 'root metadata controls numerical title');
        check(WorksheetContext.valueCard.classList.contains('hidden'), 'blank numerical output hidden');
        await window.checkLabMobileLayout('no numerical value');
        WorksheetResults.cards.setRenderedError('native partial error');
        WorksheetResults.cards.installEvaluationTextCards(plan(0, data));
        equal(WorksheetContext.rendered.textContent, 'native partial error', 'text installation preserves partial rendered error');
        check(WorksheetContext.rendered.classList.contains('error'), 'partial rendered-error class preserved');

        cards = plan(1, {
            equation: 'native equation', display_equation: 'short equation', full_display_equation: 'full equation',
            function: 'equation function', display_TeX: 'short TeX', full_display_TeX: 'full TeX',
            presentation: {equation_solution_text: 'native solutions'}
        });
        equal(cards.expandable, true, 'equation TeX expansion uses exact native cache pair');
        WorksheetResults.cards.installEvaluationTextCards(cards);
        equal(WorksheetContext.parsed.textContent, 'short equation', 'equation short representation');
        equal(WorksheetContext.resultUseInput.dataset.inputText, 'full equation', 'equation reuse selects full source');
        equal(WorksheetContext.value.textContent, 'native solutions', 'equation solutions come from presentation metadata');
        equal(plan(1, {display_TeX: 'literal...', full_display_TeX: 'full'}).expandable, false,
              'text does not imply native abbreviation');

        cards = plan(2, {symmetry: 'Ω', steps: 'native steps', steps_left_TeX: 'left TeX',
                         steps_TeX: 'other TeX', solutions: 'solutions', diagnostic: 'reason'}, 'authored problem');
        equal(cards.function, 'Symmetry: Ω\n\nnative steps', 'solver composition preserves paragraph separation');
        equal(cards.expression, 'authored problem', 'missing native problem retains authored source');
        equal(cards.solver_source, 'left TeX', 'left solver TeX has precedence');
        equal(cards.value, 'solutions', 'native solutions precede diagnostics');
        cards = plan(2, {solver: 'native solver', status: 'reduced', diagnostic: 'reason'});
        equal(cards.function, 'solver: native solver\nstatus: reduced\nreason', 'diagnostic fallback composition');
        equal(cards.solver_source, 'cached solver TeX', 'exact composed diagnostic looks up native solver rendering');
        WorksheetContext.functionStyle.classList.add('equation-function');
        WorksheetResults.cards.installEvaluationTextCards(cards);
        labDOM.call('lab_evaluation_cards_present', 2, {});
        check(!WorksheetContext.functionStyle.classList.contains('equation-function'), 'plain solver replacement removes old SVG class');
        equal(plan(3, {scalar: true, result: 'native scalar'}).scalar, 'native scalar', 'matrix scalar source');
        equal(plan(3, {scalar: false, result: 'matrix'}).scalar, '', 'non-scalar matrix clears scalar reuse');
        cards = plan(4, {expression: 'integrand', antiderivative: 'primitive', presentation: {
            integrator: {detail_text: 'native integration details', value_text: 'exact integral value'}
        }});
        WorksheetResults.cards.installEvaluationTextCards(cards);
        equal(WorksheetContext.functionStyle.textContent, 'native integration details', 'integrator native detail card');
        equal(WorksheetContext.value.textContent, 'exact integral value', 'integrator native value card');
        equal(WorksheetContext.resultUseInput.dataset.inputText, 'primitive', 'integrator reuse selects antiderivative');

        cards = plan(5, {overview: 'native overview', overview_sections: [{rows: [{}], html: '<b>Overview</b>'}],
                         range: 'range text', calendar: 'calendar text', solar: 'solar text'});
        equal(cards.calendar.length, 4, 'calendar plan owns four result cards');
        for (const card of cards.calendar)
            WorksheetResults.calendarResults.renderDatetimeSections(card.element, card.button, card.sections, card.text);
        equal(WorksheetContext.rendered.querySelector('b')?.textContent, 'Overview', 'calendar plan supplies native markup');
        equal(WorksheetContext.parsed.textContent, 'range text', 'range fallback projected');
        equal(WorksheetContext.functionStyle.textContent, 'calendar text', 'calendar fallback projected');
        equal(WorksheetContext.value.textContent, 'solar text', 'solar fallback projected');
        WorksheetResults.cards.installEvaluationTextCards(plan(6, {expression: 'must not leak'}));
        equal(WorksheetContext.parsed.textContent + WorksheetContext.functionStyle.textContent + WorksheetContext.value.textContent, '', 'almanac auxiliary cards clear');

        const base = {overview: 'base copy', overview_sections: [
            {title: 'Summary', rows: [{}], html: '<b>Summary</b>'}, {title: 'Weather', html: '<b>Old weather</b>'},
            {title: 'weather', html: '<b>Lower-case heading</b>'}
        ]};
        const weather = {overview: 'weather copy', overview_sections: [{title: 'Weather', rows: [{}], html: '<b>New</b>'}]};
        const merged = labDOM.call<{sections: unknown[]; text: string}>('lab_evaluation_weather_cards', base, weather);
        equal(merged.sections.length, 3, 'weather replaces only exact matching section titles');
        equal(merged.sections[0], base.overview_sections[0], 'native base section identity retained');
        equal(merged.sections[2], weather.overview_sections[0], 'new weather section appended');
        equal(merged.text, 'base copy\nweather copy', 'weather copy text composition');
        equal(base.overview_sections.length, 3, 'weather merge does not mutate base response');
        WorksheetResults.calendarResults.renderDatetimeSections(host, null, merged.sections, merged.text);
        equal(host.dataset.fullText, merged.text, 'merged native copy source reaches calendar renderer');
        const malformed = labDOM.call<{sections: unknown[]; text: string}>('lab_evaluation_weather_cards', base, {overview_sections: [{rows: [{}]}]});
        let failed = false;
        try {
            WorksheetResults.calendarResults.renderDatetimeSections(host, null, malformed.sections, malformed.text);
        } catch (error) {
            failed = String(error).includes('DateTime presentation is missing');
        }
        check(failed, 'weather merging retains the strong native-markup guard');
        const state = {date: 'old', latitude: '1', longitude: '2', zone: 'retained'};
        const request = labDOM.call<LabTestTextFields>('lab_evaluation_weather_state', state, {fields: {
            date: 'new', latitude: '0', longitude: ''
        }});
        equal(request.date, 'new', 'weather date uses evaluated field');
        equal(request.latitude, '0', 'string zero latitude is retained');
        equal(request.longitude, '2', 'empty evaluated longitude falls back to captured state');
        equal(request.zone, 'retained', 'weather request retains unrelated state');
        equal(state.date, 'old', 'captured weather state remains untouched');

        cards = plan(7, {expression: ' native full ', full_display_expression: 'reusable goal'}, 'native full');
        equal(cards.unchanged, true, 'goal status compares trimmed opaque native source');
        equal(cards.input, 'reusable goal', 'goal reuse selects full displayed source');
        const goals: EvaluationCardsInvocation[] = [];
        WorksheetWorkspace.mode.currentMode = () => 'matrix';
        WorksheetBindings.editor.currentExpressionText = () => 'native source';
        WorksheetRequests.goals.runGoalSeek = async function captureCardAction(source, target, start, options) {
            goals.push({source, target, start, options});
        };
        await WorksheetRequests.goals.startGoalSeekFromEvent();
        equal(goals.length, 0, 'goal event is inactive outside Expression mode');
        WorksheetWorkspace.mode.currentMode = () => 'expression';
        WorksheetContext.targetRow.classList.add('hidden');
        await WorksheetRequests.goals.startGoalSeekFromEvent();
        equal(goals.length, 0, 'first goal click only reveals target');
        check(!WorksheetContext.targetRow.classList.contains('hidden'), 'native goal preparation reveals target entry');
        WorksheetContext.goalTarget.value = '   ';
        await WorksheetRequests.goals.startGoalSeekFromEvent();
        equal(goals[0].source, 'native source', 'goal event retains source');
        equal(goals[0].target, '0', 'blank target defaults to zero');
        equal(goals[0].options!.commitBindings, true, 'goal event requests binding commit');
        WorksheetBindings.editor.currentExpressionText = () => '';
        await WorksheetRequests.goals.startGoalSeekFromEvent();
        equal(goals.length, 1, 'empty source cannot start goal seeking');
    } finally {
        labDOM.call('lab_result_render_invalidate');
        WorksheetRequests.functionCard.clearFunctionRun = saved.clearFunctionRun;
        WorksheetResults.renderedFit.schedule = saved.scheduleRenderedTeXFit;
        window.requestAnimationFrame = saved.requestAnimationFrame;
        WorksheetWorkspace.mode.currentMode = saved.currentMode;
        WorksheetBindings.editor.currentExpressionText = saved.currentExpressionText;
        WorksheetRequests.goals.runGoalSeek = saved.runGoalSeek;
        WorksheetWorkspace.presentation.state.lastTex = saved.lastTex;
        WorksheetWorkspace.presentation.state.resultInputBindings = saved.resultInputBindings;
        for (const {node, children, value, attributes} of snapshots) {
            for (const attribute of [...node.attributes]) node.removeAttribute(attribute.name);
            for (const [name, content] of attributes) node.setAttribute(name, content);
            node.replaceChildren(...children);
            if (value !== undefined)
                node.value = value;
        }
        for (const [node, descriptor] of saved.matrixRequests) {
            if (descriptor)
                Object.defineProperty(node, 'labMatrixPresentationRequest', descriptor);
            else
                Reflect.deleteProperty(node, 'labMatrixPresentationRequest');
        }
        if (saved.renderRequest)
            Object.defineProperty(WorksheetContext.rendered, 'labResultRenderRequest', saved.renderRequest);
        labWire.exports().lab_view_cards_collapse();
        if (saved.expanded >= 0)
            labWire.exports().lab_view_card_toggle(saved.expanded);
        renderResultCardExpansion();
        (saved.focus as HTMLElement | null)?.focus();
    }
};
