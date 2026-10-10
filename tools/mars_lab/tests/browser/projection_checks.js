/** Solver-card and persistence projection through the real scoped C/WASM bridge. */
window.checkLabProjection = async function checkLabProjection() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Native projection: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const nodes = [rendered, renderedTitle, renderedMore, functionStyle, valueNote, valueNoteCard, valueTitle];
    const saved = nodes.map(node => ({
                                html: node.innerHTML,
                                attributes: Array.from(node.attributes, attribute => [attribute.name, attribute.value])
                            }));
    const equationField = equationVariable || document.createElement('input');
    if (!equationVariable) {
        equationField.id = 'equationVariable';
        document.body.appendChild(equationField);
    }
    const equationName = equationField.value;
    const savedWidth = Object.getOwnPropertyDescriptor(rendered, 'clientWidth');
    try {
        const native = labWire.exports();
        for (const [mode, expected] of [[0, 72], [1, 28], [2, 18], [3, 20], [4, 20], [5, 17], [6, 16]]) {
            equal(native.lab_events_input(mode, 1, 0), expected, 'mode input action policy');
            equal(native.lab_events_flush(mode), Number(mode < 2), 'page inactivity save policy');
        }
        equal(native.lab_events_input(0, 0, 1), 56, 'matching source retains prepared bindings');
        equal(native.lab_events_input(1, 0, 0), 30, 'unbound equation updates source');
        equal(native.lab_events_input(4, 0, 0), 22, 'unbound integrator updates source');
        equal(native.lab_events_input(99, 0, 0), 0, 'invalid event mode ignored');
        equal(native.lab_events_precision(0, 1, 1), 1, 'precision preserves goal seek');
        equal(native.lab_events_precision(0, 1, 0), 2, 'incomplete goal uses expression');
        equal(native.lab_events_precision(3, 1, 1), 3, 'matrix precision reevaluates its own mode');
        equal(native.lab_events_precision(6, 0, 0), 0, 'calendar precision must not run an integrator');
        equal(labDOM.call('lab_evaluation_render', 0, {}, 0), null, 'invalid solver mode rejected');
        const equation = {tex: 'exact', display_TeX: 'short', full_display_TeX: 'full', render_error: 'native error'};
        equal(labDOM.call('lab_evaluation_render', 1, equation, 1), 'exact', 'equation retains native TeX');
        equal(rendered.dataset.displayTex, 'short', 'display TeX');
        equal(rendered.dataset.fullTex, 'full', 'full TeX');
        equal(rendered.textContent, 'native error', 'native render error fallback');
        check(!renderedMore.classList.contains('hidden'), 'metadata enables digit expansion');
        equal(
            labDOM.call('lab_evaluation_render', 4, {tex: 'short', full_TeX: 'full'}, 0), 'full',
            'integrator retains full TeX');
        check(!renderedMore.classList.contains('hidden'), 'different integral TeX can expand');
        labDOM.call('lab_evaluation_render', 4, {tex: 'same', full_TeX: 'same'}, 0);
        check(renderedMore.classList.contains('hidden'), 'identical integral TeX does not expand');
        for (const [status, title] of [
                 ['series', 'Equation and local series'], ['solved', 'Equation and solutions'],
                 ['reduced', 'Reduction']]) {
            const data = {
                status,
                display_TeX: 'display',
                solutions_TeX: 'solutions',
                problem_TeX: 'problem',
                display_wrapped_TeX: 'wrapped',
                diagnostic: 'diagnostic'
            };
            equal(labDOM.call('lab_evaluation_render', 2, data, 0), 'display', 'differential TeX precedence');
            equal(renderedTitle.textContent, title, 'solver state title');
            equal(rendered.dataset.wrappedTex, 'wrapped', 'wrapped representation retained');
            equal(rendered.dataset.responsiveFallback, 'display', 'responsive fallback');
            check(renderedMore.classList.contains('hidden'), 'differential digits disabled');
        }
        labDOM.call('lab_evaluation_render', 2, {diagnostic: '<img src=x>'}, 0);
        equal(rendered.textContent, '<img src=x>', 'diagnostic inserted as text');
        check(!rendered.querySelector('img'), 'diagnostic cannot inject HTML');
        labDOM.call('lab_evaluation_render', 2, {}, 0);
        equal(rendered.textContent, 'No symbolic solution available', 'missing solver fallback');
        Object.defineProperty(rendered, 'clientWidth', {configurable: true, value: 300});
        labDOM.call(
            'lab_evaluation_render', 2, {
                display_TeX: 'compact',
                display_wrapped_TeX: 'wrapped',
                svg: '<svg width="2000" height="20"></svg>',
                wrapped_svg: '<svg width="100" height="40"></svg>'
            },
            0);
        labDOM.call('lab_layout_fit');
        equal(rendered.dataset.responsiveVariant, 'wrapped', 'differential projection enables supplied wrapping');
        equal(rendered.dataset.displayTex, 'wrapped', 'differential wrapping selects exact native TeX');
        labDOM.call('lab_evaluation_render', 1, equation, 1);
        equal(rendered.dataset.responsiveFit, 'false', 'other solver cards clear responsive policy');
        check(!rendered.classList.contains('vertically-wrapped-tex'), 'replacement clears wrapped styling');
        labDOM.call(
            'lab_evaluation_solver', {steps_wrapped_TeX: 'wrapped'}, 'compact', 'plain',
            '<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10"></svg>', 73);
        equal(functionStyle.dataset.solverParentToken, '73', 'solver request owner retained');
        equal(functionStyle.dataset.solverWrappedTex, 'wrapped', 'solver wrapped TeX retained');
        equal(functionStyle.dataset.fullText, 'plain', 'copy source stays plain');
        check(functionStyle.querySelector('svg'), 'solver rendering installed');
        equal(
            labDOM.call('lab_evaluation_notes', {value_note: '<b>note</b>', root_value: true, differentiable: ' NO '}),
            0, 'explicit differentiability decision');
        equal(valueTitle.textContent, 'Values', 'root value title');
        equal(valueNote.textContent, '<b>note</b>', 'note is literal text');
        check(!valueNote.querySelector('b') && !valueNoteCard.classList.contains('hidden'), 'safe visible note');
        equal(labDOM.call('lab_evaluation_notes', {}), 1, 'default differentiability');
        check(valueNoteCard.classList.contains('hidden'), 'absent note hidden');

        const precision = {expression: 256};
        const records = labDOM.call('lab_persist_records', 4, ['source', 1000, '', '', '', 20000], precision);
        equal(records.server.integrator_interval_cap, 20000, 'server cap is numeric');
        equal(records.local['mars.exprLab.lastIntegratorIntervalCap'], 20000, 'local source preserves scalar type');
        check(!Object.hasOwn(records.local, 'mars.exprLab.lastIntegratorBounds'), 'empty local bounds omitted');
        equal(records.server.integrator_bounds, '', 'server receives empty bounds');
        equal(records.server.precision_bits, precision, 'trusted precision view passed through');
        equal(
            labWire.decode(labWire.encode(records.server)).integrator_interval_cap, 20000, 'record is valid Protobuf');
        equationField.value = '   ';
        const history = labDOM.call('lab_persist_history', 1, ' exact source ', null, '', '', labConfig);
        equal(history.variable, DEFAULT_EQUATION_VARIABLE_TEXT, 'history variable default');
        equal(history.text, 'exact source', 'history trims boundary whitespace only');
        equal(history.mode, 'equation', 'history mode from native index');
        equal(labDOM.call('lab_persist_history', -1, '', null, '', '', labConfig), null, 'invalid history mode');
        const calendar = {date: '2026-10-10', latitude: '52', zone: '1'};
        const patch = labDOM.call('lab_persist_calendar', 6, calendar, precision);
        equal(patch.almanac_date, calendar.date, 'calendar prefix');
        equal(patch.almanac_zone, '1', 'calendar schema field');
        check(!Object.hasOwn(patch, 'almanac_gmt_offset'), 'calendar-specific schema');
    } finally {
        if (savedWidth)
            Object.defineProperty(rendered, 'clientWidth', savedWidth);
        else
            delete rendered.clientWidth;
        await new Promise(resolve => requestAnimationFrame(resolve));
        nodes.forEach((node, index) => {
            node.innerHTML = saved[index].html;
            for (const attribute of Array.from(node.attributes)) node.removeAttribute(attribute.name);
            for (const [name, value] of saved[index].attributes) node.setAttribute(name, value);
        });
        equationField.value = equationName;
        if (!equationVariable)
            equationField.remove();
    }
};
