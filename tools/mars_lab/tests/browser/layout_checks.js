/**
 * Browser-only result geometry and deferred-render regressions.
 * The parent invokes checkLabLayout after labReady, with no live requests.
 * Mock transport controls completion order; native request-policy checks remain
 * in request_checks.js. No mathematical expressions are interpreted here.
 */
window.checkLabLayout = async function checkLabLayout() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error(`Result layout: ${message}`);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const view = labWire.exports();
    const hints = document.createElement('section');
    hints.className = 'result-card';
    const heading = document.createElement('div');
    heading.className = 'card-title';
    const title = document.createElement('span');
    title.textContent = '  Résultat Ω  ';
    heading.appendChild(title);
    hints.appendChild(heading);
    document.body.appendChild(hints);
    const makeButton = (text = '') => {
        const button = document.createElement('button');
        button.textContent = text;
        hints.appendChild(button);
        return button;
    };
    const hint = button => {
        showButtonTooltip(button);
        const node = document.getElementById('marsButtonTooltip');
        check(node && node.classList.contains('visible'), 'tooltip visible');
        equal(node.getAttribute('role'), 'tooltip', 'tooltip role');
        return node.textContent;
    };
    try {
        hideButtonTooltip();
        const button = makeButton('  Choose\n\t something  ');
        equal(hint(button), 'Choose something', 'collapsed fallback label');
        equal(button.getAttribute('aria-describedby'), 'marsButtonTooltip', 'temporary description');
        equal(hint(button), 'Choose something', 'same-button refresh');
        hideButtonTooltip();
        check(!button.hasAttribute('aria-describedby'), 'refresh must not preserve its own description');
        equal(hint(makeButton()), 'Activate this control', 'empty fallback');
        button.setAttribute('aria-label', '  日本語\u00a0');
        equal(hint(button), '日本語', 'Unicode accessible label');
        button.className = 'select-button';
        button.textContent = 'Shrewsbury';
        equal(hint(button), 'Choose Shrewsbury', 'selection hint');
        button.textContent = '';
        equal(hint(button), 'Choose an option', 'empty selection hint');
        button.className = 'variable-copy';
        equal(hint(button), 'Copy this binding value', 'binding hint');
        button.className = 'copy-result';
        equal(hint(button), 'Copy the résultat ω', 'Unicode card name');
        button.dataset.copyTarget = 'mobile';
        equal(hint(button), 'Copy the private mobile-access URL', 'mobile copy precedence');
        button.classList.add('more-digits');
        equal(hint(button), 'Show the full value in the résultat ω card', 'more-digits precedence');
        button.dataset.expandCard = '';
        button.textContent = 'Collapse';
        equal(hint(button), 'Collapse the résultat ω card', 'expansion precedence');
        button.classList.add('mode-tab');
        equal(hint(button), 'Switch to Collapse mode', 'mode precedence');
        const labels = {
            back: 'Return to the previous input in this mode',
            clear: 'Clear the current input and its results',
            forward: 'Move to the next input in this mode',
            goalSeek: 'Find a variable value that reaches the requested target',
            help: 'Show or hide help for the current mode',
            inputCopy: 'Copy the current input',
            lessPrecision: 'Calculate and display fewer significant digits',
            marsDatePickerClose: 'Close the date picker',
            marsDatePickerToday: 'Use the current date',
            morePrecision: 'Calculate and display more significant digits',
            resultUseInput: 'Put this result into the current mode as a new input',
            run: 'Evaluate the current input'
        };
        for (const [id, text] of Object.entries(labels)) {
            button.id = id;
            equal(hint(button), text, 'fixed label overrides class: ' + id);
        }
        button.id = 'unknown-control';
        button.setAttribute('title', '  <b>Authored Ω</b>\u00a0');
        equal(hint(button), '<b>Authored Ω</b>', 'authored title precedence');
        check(!document.getElementById('marsButtonTooltip').querySelector('b'), 'hint cannot inject HTML');
        check(!button.hasAttribute('title'), 'native browser title suppressed');
        equal(hint(button), '<b>Authored Ω</b>', 'cached title retained');
        button.setAttribute('title', 'Changed');
        equal(hint(button), 'Changed', 'new title replaces cache');
        button.setAttribute('title', ' ');
        equal(hint(button), 'Switch to Collapse mode', 'explicit empty title clears cache');
        check(!button.hasAttribute('data-mars-tooltip-title'), 'cleared cache removed');
        hideButtonTooltip();
        for (const description of ['', ' first\t second ', 'first marsButtonTooltip second']) {
            button.setAttribute('aria-describedby', description);
            hint(button);
            hint(button);
            equal(
                button.getAttribute('aria-describedby').split(/\s+/).filter(x => x === 'marsButtonTooltip').length, 1,
                'description token appears once');
            hideButtonTooltip();
            equal(button.getAttribute('aria-describedby'), description, 'description restored byte-for-byte');
            check(!button.hasAttribute('data-mars-tooltip-description'), 'saved description released');
        }
        button.setAttribute('aria-describedby', 'original');
        button.dispatchEvent(new Event('pointerover', {bubbles: true}));
        button.dispatchEvent(new Event('focusin', {bubbles: true}));
        button.dispatchEvent(new Event('pointerout', {bubbles: true}));
        equal(button.getAttribute('aria-describedby'), 'original', 'hover plus focus restores original');
        hint(button);
        const other = makeButton('Other');
        equal(hint(other), 'Other', 'tooltip switches controls');
        equal(button.getAttribute('aria-describedby'), 'original', 'switch restores previous');
        button.classList.add('hidden');
        showButtonTooltip(button);
        equal(activeTooltipButton, other, 'hidden candidate does not replace active node');
        showButtonTooltip(null);
        equal(activeTooltipButton, other, 'absent candidate does not replace active node');
        other.remove();
        hideButtonTooltip();
        check(!other.hasAttribute('aria-describedby'), 'detached active node is restored');
        check(!document.getElementById('marsButtonTooltip').classList.contains('visible'), 'tooltip hidden');
    } finally {
        hideButtonTooltip();
        hints.remove();
    }
    const retired = await labFetch('/js/result_layout.js');
    check(
        retired.status === 404 && !labDefinitionScripts.includes('result_layout'), 'retired layout script unavailable');
    const savedCardZooms = resultCards.map(card => resultZoomIndex(card));
    const savedCardHidden = resultCards.map(card => card.classList.contains('hidden'));
    const savedExpanded = view.lab_view_card_expanded();
    try {
        equal(view.lab_view_cards_reset(64), 1, 'maximum card registration');
        for (let card = 0; card < 64; ++card) equal(view.lab_view_card_zoom(card), 3, 'registered cards start at 100%');
        equal(view.lab_view_card_set_zoom(63, 10), 10, 'last card owns zoom');
        equal(view.lab_view_card_zoom(0), 3, 'card zoom isolation');
        equal(view.lab_view_card_step_zoom(63, 1), 10, 'upper zoom bound');
        equal(view.lab_view_card_set_zoom(63, -100), 0, 'lower zoom clamp');
        equal(view.lab_view_card_step_zoom(63, -1), 0, 'lower zoom bound');
        equal(view.lab_view_card_set_zoom(63, NaN), 3, 'non-finite zoom resets');
        equal(view.lab_view_card_set_zoom(63, 4.5), 5, 'owned zoom rounds');
        equal(view.lab_view_card_toggle(63), 63, 'expand last card');
        for (const invalid of [64, -1]) {
            equal(view.lab_view_card_zoom(invalid), -1, 'invalid card read');
            equal(view.lab_view_card_set_zoom(invalid, 0), -1, 'invalid zoom set');
            equal(view.lab_view_card_step_zoom(invalid, 1), -1, 'invalid zoom step');
            equal(view.lab_view_card_toggle(invalid), -2, 'invalid expansion');
            equal(view.lab_view_card_expanded(), 63, 'invalid action retains expansion');
            equal(view.lab_view_card_zoom(63), 5, 'invalid action retains zoom');
        }
        equal(view.lab_view_cards_reset(65), 0, 'excessive registration rejected');
        equal(view.lab_view_card_expanded(), 63, 'invalid registration preserves expansion');
        equal(view.lab_view_card_toggle(0), 0, 'expansion transfers exclusively');
        equal(view.lab_view_card_toggle(0), -1, 'same card collapses');
        view.lab_view_card_toggle(63);
        view.lab_view_cards_collapse();
        equal(view.lab_view_card_expanded(), -1, 'explicit collapse');
        equal(view.lab_view_card_zoom(63), 5, 'collapse preserves zoom');
        equal(view.lab_view_cards_reset(0), 1, 'release all cards');
        equal(view.lab_view_card_zoom(0), -1, 'released card is unavailable');
        equal(view.lab_view_card_toggle(0), -2, 'released card cannot expand');

        view.lab_view_cards_reset(resultCards.length);
        const first = resultCards[0], second = resultCards[1];
        setResultZoom(first, 5);
        stepResultZoom(first, 1);
        equal(resultZoomIndex(first), 6, 'DOM zoom adapter changes native state');
        equal(resultZoomIndex(second), 3, 'DOM zoom adapter preserves other cards');
        equal(first.style.getPropertyValue('--result-zoom'), '2', 'DOM applies native zoom');
        toggleResultCardExpansion(first.querySelector('[data-expand-card]'));
        check(resultCardExpanded(first) && first.classList.contains('expanded-card'), 'DOM expands selected card');
        toggleResultCardExpansion(second.querySelector('[data-expand-card]'));
        check(!first.classList.contains('expanded-card') && resultCardExpanded(second), 'DOM transfers expansion');
        equal(
            first.querySelector('[data-expand-card]').getAttribute('aria-expanded'), 'false', 'old card accessibility');
        equal(
            second.querySelector('[data-expand-card]').getAttribute('aria-expanded'), 'true', 'new card accessibility');
        // Hiding must consult native selection, even if the browser projection has been disturbed.
        second.classList.remove('expanded-card');
        labDOM.call('lab_workspace_dom_aux', Number(!!false));
        equal(view.lab_view_card_expanded(), -1, 'hiding expanded card consults native state');
        collapseResultCards();
        check(!resultPane.classList.contains('card-expanded') && !resultCardExpanded(second), 'DOM collapses cards');
        equal(resultZoomIndex(first), 6, 'DOM collapse retains native zoom');
    } finally {
        view.lab_view_cards_reset(resultCards.length);
        savedCardZooms.forEach((zoom, card) => view.lab_view_card_set_zoom(card, zoom));
        if (savedExpanded >= 0)
            view.lab_view_card_toggle(savedExpanded);
        renderResultCardExpansion();
        resultCards.forEach((card, index) => card.classList.toggle('hidden', savedCardHidden[index]));
        resultCards.forEach(card => applyResultZoom(card));
    }
    const resize = (...args) => {
        const pointer = view.lab_view_editor_resize(...args);
        check(pointer !== 0, 'valid editor geometry');
        return new Float64Array(view.memory.buffer, pointer, 3).slice().join(',');
    };
    equal(resize(800, 2, 300, 100, 120, 0, false), '3,120,260', 'overflow enables resizing and shares budget');
    equal(resize(800, 2, 101, 100, 120, 0, false), '0,120,260', 'one-pixel overflow tolerance');
    equal(resize(800, 2, 100, 100, 122, 120, true), '0,120,260', 'two-pixel manual tolerance resets');
    equal(resize(800, 2, 100, 100, 123, 120, true), '3,120,260', 'larger manual height is retained');
    equal(resize(800, 2, 100, 100, 117, 120, true), '3,120,260', 'smaller manual height is retained');
    equal(resize(0, 0, 300, 100, 120, 0, false), '3,120,216', 'minimum resize budget and zero-count fallback');
    equal(resize(2000, 1, 300, 100, 120, 0, false), '3,120,440', 'maximum resize budget');
    const resizePointer = view.lab_view_editor_resize(800, 2, 300, 100, 120, 0, false);
    const resizeSaved = new Float64Array(view.memory.buffer, resizePointer, 3).slice().join(',');
    for (const index of [0, 2, 3, 4, 5]) {
        for (const invalid of [NaN, Infinity, -1]) {
            const args = [800, 2, 300, 100, 120, 0, false];
            args[index] = invalid;
            equal(view.lab_view_editor_resize(...args), 0, 'invalid editor measurement rejected');
            equal(
                new Float64Array(view.memory.buffer, resizePointer, 3).join(','), resizeSaved,
                'invalid editor measurement leaves result unchanged');
        }
    }
    const tooltip = (...args) => {
        const pointer = view.lab_view_tooltip_rect(...args);
        check(pointer !== 0, 'valid tooltip geometry');
        return new Float64Array(view.memory.buffer, pointer, 2).slice().join(',');
    };
    equal(tooltip(1000, 800, 100, 100, 100, 130, 80, 20), '110,138', 'tooltip centred below');
    equal(tooltip(1000, 800, 980, 760, 20, 780, 80, 20), '912,732', 'tooltip right clamp and above placement');
    equal(tooltip(100, 10, -50, 0, 20, 10, 200, 30), '8,8', 'oversized tooltip keeps minimum margins');
    equal(tooltip(1000, 166, 100, 100, 100, 130, 80, 20), '110,138', 'tooltip fits exactly at lower margin');
    const tooltipPointer = view.lab_view_tooltip_rect(1000, 800, 100, 100, 100, 130, 80, 20);
    const tooltipSaved = new Float64Array(view.memory.buffer, tooltipPointer, 2).slice().join(',');
    for (let index = 0; index < 8; ++index) {
        const args = [1000, 800, 100, 100, 100, 130, 80, 20];
        args[index] = NaN;
        equal(view.lab_view_tooltip_rect(...args), 0, 'invalid tooltip measurement rejected');
        equal(
            new Float64Array(view.memory.buffer, tooltipPointer, 2).join(','), tooltipSaved,
            'invalid tooltip measurement leaves result unchanged');
    }
    // Exercise the real DOM adapter with controlled measurements, without changing existing editors.
    const savedEditors = labTextareas.slice();
    const testEditor = document.createElement('textarea');
    let contentHeight = 300;
    let measuredHeight = 120;
    Object.defineProperties(testEditor, {clientHeight: {get: () => 100}, scrollHeight: {get: () => contentHeight}});
    testEditor.getBoundingClientRect = () => ({height: measuredHeight});
    testEditor.getClientRects = () => [{}];
    document.body.appendChild(testEditor);
    labTextareas.splice(0, labTextareas.length, testEditor);
    try {
        syncEditorResizeGrip();
        equal(testEditor.dataset.automaticHeight, '120', 'adapter retains automatic height');
        check(testEditor.classList.contains('editor-manual-size'), 'adapter enables resizing');
        contentHeight = 100;
        measuredHeight = 150;
        syncEditorResizeGrip();
        check(testEditor.classList.contains('editor-manual-size'), 'adapter preserves resized editor');
        measuredHeight = 120;
        syncEditorResizeGrip();
        check(!testEditor.classList.contains('editor-manual-size'), 'adapter resets unneeded resizing');
        equal(testEditor.style.height, '', 'adapter clears fixed height');
        equal(testEditor.style.maxHeight, '', 'adapter clears maximum height');
        equal(testEditor.dataset.automaticHeight, undefined, 'adapter clears stored measurement');
    } finally {
        labTextareas.splice(0, labTextareas.length, ...savedEditors);
        testEditor.remove();
    }
    const pickerRect = (...args) => {
        const pointer = view.lab_view_picker_rect(...args);
        check(pointer !== 0, 'valid picker geometry');
        return new Float64Array(view.memory.buffer, pointer, 4).slice().join(',');
    };
    equal(pickerRect(1000, 800, 100, 200, 100), '448,100,108,680', 'desktop picker minimum width');
    equal(pickerRect(320, 600, 250, 200, 100), '296,12,108,480', 'mobile picker fits viewport');
    equal(pickerRect(1000, 800, -100, 500, 790), '500,12,798,8', 'offscreen anchor clamps');
    equal(pickerRect(10, 10, 0, 200, 10), '0,12,18,8', 'tiny viewport stays non-negative');
    const pickerPointer = view.lab_view_picker_rect(1000, 800, 100, 200, 100);
    const pickerSaved = new Float64Array(view.memory.buffer, pickerPointer, 4).slice().join(',');
    for (const args
             of [[-1, 800, 0, 10, 0], [1000, -1, 0, 10, 0], [1000, 800, 0, -1, 0], [NaN, 800, 0, 10, 0],
                 [1000, 800, Infinity, 10, 0], [1000, 800, 0, 10, -Infinity]]) {
        equal(view.lab_view_picker_rect(...args), 0, 'invalid picker geometry rejected');
        equal(
            new Float64Array(view.memory.buffer, pickerPointer, 4).join(','), pickerSaved,
            'invalid geometry preserves borrowed rectangle');
    }
    const scales = [0.5, 0.67, 0.8, 1, 1.25, 1.5, 2, 3, 4, 6, 8];
    equal(view.lab_view_zoom_count(), scales.length, 'C owns every zoom level');
    scales.forEach((scale, index) => equal(view.lab_view_zoom(index), scale, `zoom ${index}`));
    for (const invalid of [NaN, Infinity, -Infinity]) {
        equal(view.lab_view_zoom_index(invalid), 3, 'invalid zoom selects 100%');
        equal(view.lab_view_wrapped(invalid, 1, 100, true), 0, 'invalid width cannot request wrapping');
        equal(view.lab_view_wrapped(200, invalid, 100, true), 0, 'invalid scale cannot request wrapping');
        equal(view.lab_view_matrix_scale(1, 1, invalid, 100), 1, 'invalid fitting geometry is safe');
    }
    equal(view.lab_view_zoom_index(-100), 0, 'zoom lower clamp');
    equal(view.lab_view_zoom_index(1e200), 10, 'zoom upper clamp before conversion');
    equal(view.lab_view_zoom_index(2.5), 3, 'zoom rounds half up');
    equal(view.lab_view_zoom_step(0, -1), 0, 'zoom cannot step below minimum');
    equal(view.lab_view_zoom_step(10, 1), 10, 'zoom cannot step above maximum');
    equal(view.lab_view_matrix_scale(1.35, 2, 100, 200), 1, 'matrix fits before applying zoom');
    equal(view.lab_view_matrix_scale(1, 2, 1000, 200), 2, 'matrix fitting does not enlarge base');
    equal(view.lab_view_wrapped(101, 1, 100, true), 0, 'one pixel fitting tolerance');
    equal(view.lab_view_wrapped(102, 1, 100, true), 1, 'overflow selects wrapped variant');
    equal(view.lab_view_wrapped(102, 1, 100, false), 0, 'missing variant cannot wrap');
    equal(view.lab_view_mode_flags(0, false), 29, 'empty expression value hidden');
    equal(view.lab_view_mode_flags(0, true), 31, 'expression value shown when supplied');
    [29, 3, 3, 9, 3, 3, 0].forEach(
        (flags, mode) => equal(view.lab_view_mode_flags(mode, false), flags, `mode ${mode} card policy`));
    equal(labDOM.call('lab_workspace_dom_labels', 2)[0], 'Differential Equation', 'mode title from C');
    equal(labDOM.call('lab_workspace_dom_labels', 4)[4], 'Exact result', 'integrator card label from C');
    equal(view.lab_view_text_length(0, 99), 0, 'invalid label is empty');
    for (let mode = 0; mode < 7; ++mode) {
        equal(view.lab_view_controls(mode, true, true, 1, 1, true, false, false), 0, 'busy disables controls');
        equal(
            view.lab_view_controls(mode, false, true, 1, 1, true, false, false), mode ? 55 : 63,
            'only expression mode enables goal seek');
        equal(view.lab_view_controls(mode, false, false, 0, 0, false, true, true), 0, 'unavailable controls disabled');
    }
    for (const [length, pixels] of [
             ['96px', 96], ['72pt', 96], ['6pc', 96], ['1in', 96], ['2.54cm', 96], ['25.4mm', 96], ['200', 200],
             ['50%', 0], ['2em', 0], ['nonsense', 0]])
        check(Math.abs(labDOM.svgLength(length) - pixels) < 0.001, `browser SVG units: ${length}`);
    equal(svgMarkupIntrinsicWidth('<svg viewBox="0 0 300 20"></svg>'), 300, 'browser parses viewBox width');
    check(!labSolverRenderRequest, 'requires an idle solver renderer');
    const saved = {
        currentMode,
        solverFitFrame,
        renderedTeXFitFrame,
        requests: {...labRequests},
        title: rightPaneTitle.textContent,
        zoom: resultZoomIndex(rendered.closest('.result-card')),
        renderScale: rendered.closest('.result-card').style.getPropertyValue('--render-base-scale')
    };
    const elements = [functionStyle, rendered].map(
        element => ({
            element,
            attributes: [...element.attributes].map(attribute => [attribute.name, attribute.value]),
            children: [...element.childNodes],
            clientWidth: Object.getOwnPropertyDescriptor(element, 'clientWidth')
        }));
    const compact = '<svg xmlns="http://www.w3.org/2000/svg" width="2000" height="20"></svg>';
    const wrapped = '<svg xmlns="http://www.w3.org/2000/svg" width="100" height="40"></svg>';
    const other = '<svg xmlns="http://www.w3.org/2000/svg" width="2100" height="20"></svg>';
    let mode = 'diffequation';
    let width = 300;
    let epoch = 1;
    let active = null;
    let allowBegin = true;
    const shown = () => rendered.querySelector('.rendered-zoom-frame')?.innerHTML || '';
    const posts = [];
    const finished = [];
    const tasks = [];
    const start = () => {
        const task = fitSolverTexToCard();
        tasks.push(task);
        return task;
    };
    const startPending = step => {
        const count = posts.length;
        const task = start();
        check(posts.length === count + 1, `${step}: expected one new pending transport request`);
        const post = posts[count];
        check(labSolverRenderRequest?.request === post.request, `${step}: pending layout request owns transport`);
        return {task, post};
    };
    const complete = (post, svg = wrapped, ok = true, httpOk = true) => {
        check(post && typeof post.resolve === 'function', 'completion requires a pending transport request');
        post.resolve({response: {ok: httpOk}, data: {ok, svg}});
    };
    const card = (svg = compact, text = 'wrapped-A') => {
        functionStyle.classList.add('equation-function');
        functionStyle.dataset.solverCompactSvg = svg;
        functionStyle.dataset.solverCompactTex = 'compact-source';
        functionStyle.dataset.solverWrappedTex = text;
        functionStyle.dataset.solverWrappedSvg = '';
        functionStyle.dataset.solverParentToken = String(epoch);
        delete functionStyle.dataset.solverVariant;
        installSolverTexSvg(svg, 'compact');
    };
    try {
        currentMode = () => mode;
        for (const element of [functionStyle, rendered])
            Object.defineProperty(element, 'clientWidth', {configurable: true, get: () => width});
        functionStyle.style.setProperty('--solver-tex-scale', '1');
        const renderCard = rendered.closest('.result-card');
        renderCard.style.setProperty('--render-base-scale', '1');
        view.lab_view_card_set_zoom(resultCardIds.get(renderCard), 3);
        rendered.replaceChildren();
        labRequests.latestMain = () => epoch;
        labRequests.begin = (operation, requestedMode, options) => {
            equal(operation, 'solverRender', 'uses a separate solver-render operation');
            equal(requestedMode, 'diffequation', 'solver request mode');
            equal(
                options.parent, Number(functionStyle.dataset.solverParentToken) || 0,
                'request belongs to the displayed solver card');
            equal(options.input, true, 'nonempty wrapped source is supplied');
            if (!allowBegin || !options.parent || options.parent !== epoch)
                return null;
            active = {epoch};
            return active;
        };
        labRequests.current = request =>
            !!request && request === active && request.epoch === epoch && mode === 'diffequation';
        labRequests.post = (request, payload, endpoint) => new Promise((resolve, reject) => {
            equal(endpoint, '/render_TeX', 'render endpoint');
            equal(payload.tex, functionStyle.dataset.solverWrappedTex, 'native TeX forwarded unchanged');
            posts.push({request, resolve, reject});
        });
        labRequests.finish = request => {
            finished.push(request);
            if (active === request)
                active = null;
        };

        equal(hasAbbreviatedValue(true), true, 'native abbreviation enabled');
        for (const value of [false, undefined, '', 'literal...', 'true'])
            equal(hasAbbreviatedValue(value), false, 'text does not imply abbreviation');

        rendered.dataset.compactSvg = compact;
        rendered.dataset.wrappedSvg = wrapped;
        rendered.dataset.compactTex = 'compact';
        rendered.dataset.wrappedTex = 'wrapped';
        delete rendered.dataset.responsiveVariant;
        delete rendered.dataset.responsiveFit;
        rightPaneTitle.textContent = 'Integral result';
        fitRenderedTeXToCard();
        equal(shown(), '', 'differential-equation mode does not imply responsive policy');
        mode = 'expression';
        fitRenderedTeXToCard();
        equal(shown(), '', 'English integral title does not imply responsive policy');
        rendered.dataset.responsiveFit = 'true';
        rightPaneTitle.textContent = 'Résultat';
        fitRenderedTeXToCard();
        equal(shown(), wrapped, 'native responsive policy is independent of title');
        equal(rendered.dataset.displayTex, 'wrapped', 'wrapped source follows geometry');
        width = 3000;
        fitRenderedTeXToCard();
        equal(shown(), compact, 'wide viewport uses compact native rendering');
        equal(rendered.dataset.displayTex, 'compact', 'compact source follows geometry');
        rendered.dataset.responsiveFit = 'false';
        width = 300;
        fitRenderedTeXToCard();
        equal(shown(), compact, 'disabled responsive metadata is respected');

        mode = 'diffequation';
        // Native preparation returns ordinary snapshot values and performs every freshness decision.
        card();
        const prepare = (pending = null, live = 0, owner = null) =>
            labDOM.call('lab_solver_view_prepare', mode, pending, live, owner);
        const current = (snapshot, active = snapshot, live = 1, owner = null) =>
            labDOM.call('lab_solver_view_current', mode, snapshot, active, live, owner);
        const publish = (snapshot, active = snapshot, live = 1, owner = null, failed = 0) => labDOM.call(
            'lab_solver_view_publish', mode, snapshot, active, live, owner, {ok: true}, {ok: true, svg: wrapped},
            failed);
        const snapshot = Object.freeze(prepare());
        equal(snapshot.node, functionStyle, 'snapshot stores the actual solver card');
        equal(snapshot.compactSvg, compact, 'snapshot preserves exact compact SVG');
        equal(snapshot.wrappedTex, 'wrapped-A', 'snapshot preserves exact wrapped TeX');
        equal(snapshot.parentToken, String(epoch), 'snapshot keeps parent token text for exact comparison');
        equal(snapshot.restoredOwner, null, 'evaluated snapshot has no restored owner');
        equal(snapshot.operation, 'solverRender', 'native preparation selects evaluated-card operation');
        equal(snapshot.mode, 'diffequation', 'native request mode');
        equal(snapshot.options.parent, epoch, 'native preparation uses displayed parent identity');
        equal(snapshot.options.input, true, 'native request input flag');
        equal(snapshot.payload.tex, 'wrapped-A', 'request payload retains native TeX');
        equal(snapshot.endpoint, '/render_TeX', 'native wrapping endpoint');
        equal(current(snapshot), 1, 'fresh snapshot accepted');
        equal(prepare(snapshot, 1), null, 'identical live snapshot deduplicates');
        const fresh = prepare(snapshot, 0);
        check(
            fresh && fresh !== snapshot && fresh.options !== snapshot.options && fresh.payload !== snapshot.payload,
            'expired work yields independent request objects');
        equal(snapshot.node, functionStyle, 'later calls cannot recycle the saved node into a handle alias');
        equal(current(snapshot, fresh), 0, 'same inputs do not replace browser request identity');
        equal(current(snapshot, snapshot, 0), 0, 'request liveness is mandatory');
        equal(publish(snapshot, fresh), 0, 'stale browser owner cannot publish');
        equal(functionStyle.dataset.solverWrappedSvg, '', 'stale publication leaves the cache untouched');
        for (const [field, replacement] of [
                 ['solverParentToken', '0' + epoch], ['solverCompactSvg', other],
                 ['solverWrappedTex', ' wrapped-A ']]) {
            const previous = functionStyle.dataset[field];
            functionStyle.dataset[field] = replacement;
            equal(current(snapshot), 0, 'changed snapshot field rejected: ' + field);
            equal(publish(snapshot), 0, 'changed snapshot cannot publish: ' + field);
            functionStyle.dataset[field] = previous;
        }
        const duplicate = functionStyle.cloneNode(true);
        functionStyle.before(duplicate);
        try {
            equal(current(snapshot), 0, 'replacement node with identical metadata is a different card');
            equal(publish(snapshot), 0, 'replacement card cannot receive another node snapshot');
        } finally {
            duplicate.remove();
        }
        const restoredOwner = Object.freeze({context: 91});
        const restoredSnapshot = Object.freeze(prepare(null, 0, restoredOwner));
        equal(restoredSnapshot.operation, 'solverRestoreRender', 'native preparation selects restored operation');
        equal(restoredSnapshot.options.parent, 0, 'restored owner does not borrow a main token');
        equal(restoredSnapshot.restoredOwner, restoredOwner, 'restored snapshot retains actual owner identity');
        equal(current(restoredSnapshot, restoredSnapshot, 1, restoredOwner), 1, 'same restored owner accepted');
        equal(
            current(restoredSnapshot, restoredSnapshot, 1, {context: 91}), 0,
            'matching context numbers do not replace restored owner identity');
        equal(publish(restoredSnapshot), 0, 'invalidated restored owner cannot publish');
        equal(publish(snapshot), 1, 'current native publication succeeds');
        equal(functionStyle.dataset.solverWrappedSvg, wrapped, 'native completion caches exact wrapped SVG');
        equal(functionStyle.dataset.solverVariant, 'wrapped', 'native completion projects selected layout');
        width = 3000;
        equal(prepare(), null, 'wide cached card needs no asynchronous request');
        equal(functionStyle.dataset.solverVariant, 'compact', 'cached preparation uses latest viewport');
        width = 300;
        equal(prepare(), null, 'narrow cached card reuses wrapping');
        equal(functionStyle.dataset.solverVariant, 'wrapped', 'narrow preparation projects cached wrapping');
        equal(publish(snapshot, snapshot, 1, null, 1), 1, 'current network failure can project compact fallback');
        equal(functionStyle.dataset.solverWrappedSvg, wrapped, 'network failure does not erase an existing cache');
        equal(functionStyle.dataset.solverVariant, 'compact', 'network failure uses compact fallback');
        for (const [response, data] of [
                 [{ok: false}, {ok: true, svg: wrapped}], [{ok: true}, {ok: false, svg: wrapped}]]) {
            equal(
                labDOM.call('lab_solver_view_publish', mode, snapshot, snapshot, 1, null, response, data, 0), 1,
                'current HTTP/application failure is handled');
            equal(functionStyle.dataset.solverWrappedSvg, '', 'HTTP/application failure clears wrapped cache');
        }
        mode = 'expression';
        equal(prepare(), null, 'other modes cannot prepare solver wrapping');
        equal(current(snapshot), 0, 'other modes cannot accept a solver snapshot');
        mode = 'diffequation';
        functionStyle.classList.remove('equation-function');
        equal(prepare(), null, 'missing solver class cannot prepare wrapping');
        card('', 'wrapped-A');
        equal(prepare(), null, 'missing compact SVG cannot prepare wrapping');
        card(compact, '');
        equal(prepare(), null, 'missing wrapped input needs no transport');
        card(compact, 'compact-source');
        equal(prepare(), null, 'identical compact and wrapped TeX needs no wrapping request');

        card();
        const {task: first, post: firstPost} = startPending('first result');
        check(labSolverRenderRequest.current(), 'public pending.current delegates fresh ownership to C');
        ++epoch;
        card(other, 'wrapped-B');
        const {task: second, post: secondPost} = startPending('replacement result');
        complete(firstPost);
        await first;
        equal(functionStyle.dataset.solverWrappedSvg, '', 'old response cannot populate new cache');
        equal(functionStyle.dataset.solverVariant, 'compact', 'old response cannot replace new card');
        const count = posts.length;
        await start();
        equal(posts.length, count, 'old completion cannot clear newer pending render');
        complete(secondPost);
        await second;
        equal(functionStyle.dataset.solverWrappedSvg, wrapped, 'current response populates current cache');
        equal(functionStyle.dataset.solverVariant, 'wrapped', 'current response installs wrapped SVG');
        equal(functionStyle.querySelector('svg')?.getAttribute('width'), '100', 'wrapped SVG reaches the DOM');

        // Identical source text is not a substitute for native result identity.
        card();
        const {task: sameOld, post: sameOldPost} = startPending('identical source original identity');
        ++epoch;
        card();
        const {task: sameNew, post: sameNewPost} = startPending('identical source replacement identity');
        check(sameOldPost !== sameNewPost, 'identical source with a new identity starts a new render');
        sameOldPost.reject(new Error('obsolete render failure'));
        await sameOld;
        equal(functionStyle.dataset.solverWrappedSvg, '', 'obsolete failure leaves replacement cache alone');
        const pending = labSolverRenderRequest;
        await start();
        equal(labSolverRenderRequest, pending, 'obsolete failure preserves replacement pending render');
        complete(sameNewPost);
        await sameNew;

        card();
        const {task: changedMode, post: modePost} = startPending('mode change');
        mode = 'expression';
        complete(modePost);
        await changedMode;
        equal(functionStyle.dataset.solverWrappedSvg, '', 'mode change rejects old render');
        equal(functionStyle.dataset.solverVariant, 'compact', 'mode change does not install old SVG');
        mode = 'diffequation';

        card();
        const {task: removed, post: removedPost} = startPending('removed solver card');
        functionStyle.classList.remove('equation-function');
        complete(removedPost);
        await removed;
        equal(functionStyle.dataset.solverWrappedSvg, '', 'removed solver card rejects render');
        check(!functionStyle.classList.contains('equation-function'), 'late render cannot restore removed solver card');

        card();
        const {task: replacedSource, post: replacedPost} = startPending('replaced DOM source');
        card(other, 'replacement-source');
        complete(replacedPost);
        await replacedSource;
        equal(functionStyle.dataset.solverWrappedSvg, '', 'changed DOM source rejects render even with live request');
        equal(functionStyle.querySelector('svg')?.getAttribute('width'), '2100', 'replacement DOM remains untouched');

        card();
        const {task: resized, post: resizePost} = startPending('viewport resize');
        width = 3000;
        complete(resizePost);
        await resized;
        equal(functionStyle.dataset.solverVariant, 'compact', 'completion uses latest viewport width');
        equal(functionStyle.dataset.solverWrappedSvg, wrapped, 'offscreen wrapped variant remains cached');
        const cachedCount = posts.length;
        width = 300;
        await start();
        equal(functionStyle.dataset.solverVariant, 'wrapped', 'later resize reuses cached SVG');
        equal(posts.length, cachedCount, 'cached layout needs no request');

        for (const failure of ['network', 'application', 'http']) {
            card();
            const {task: failed, post: failedPost} = startPending(`${failure} failure`);
            if (failure === 'network')
                failedPost.reject(new Error('render unavailable'));
            else
                complete(failedPost, wrapped, failure !== 'application', failure !== 'http');
            await failed;
            equal(functionStyle.dataset.solverVariant, 'compact', `${failure} failure retains compact SVG`);
            equal(functionStyle.dataset.solverWrappedSvg, '', `${failure} failure does not cache SVG`);
            equal(labSolverRenderRequest, null, `${failure} failure releases pending render`);
        }
        card();
        const identityCount = posts.length;
        delete functionStyle.dataset.solverParentToken;
        await start();
        equal(posts.length, identityCount, 'missing card identity cannot borrow latest main identity');
        card();
        ++epoch;
        await start();
        equal(posts.length, identityCount, 'old displayed card cannot borrow newer evaluation identity');
        equal(functionStyle.dataset.solverVariant, 'compact', 'obsolete card retains compact output');
        card();
        allowBegin = false;
        const refusedCount = posts.length;
        await start();
        equal(posts.length, refusedCount, 'native refusal never sends render');
        equal(functionStyle.dataset.solverVariant, 'compact', 'native refusal keeps compact output');
        equal(finished.length, posts.length, 'every started request finishes exactly once');
        equal(new Set(finished).size, posts.length, 'finished requests have distinct identities');
    } finally {
        // Drain all deferred continuations before restoring real request methods.
        ++epoch;
        for (const post of posts) complete(post);
        await Promise.allSettled(tasks);
        Object.assign(labRequests, saved.requests);
        currentMode = saved.currentMode;
        solverFitFrame = saved.solverFitFrame;
        renderedTeXFitFrame = saved.renderedTeXFitFrame;
        labSolverRenderRequest = null;
        rightPaneTitle.textContent = saved.title;
        rendered.closest('.result-card').style.setProperty('--render-base-scale', saved.renderScale);
        view.lab_view_card_set_zoom(resultCardIds.get(rendered.closest('.result-card')), saved.zoom);
        for (const {element, attributes, children, clientWidth} of elements) {
            for (const attribute of [...element.attributes]) element.removeAttribute(attribute.name);
            for (const [name, value] of attributes) element.setAttribute(name, value);
            element.replaceChildren(...children);
            if (clientWidth)
                Object.defineProperty(element, 'clientWidth', clientWidth);
            else
                delete element.clientWidth;
        }
    }
    await checkLabRestoredSolverOwnership();
};

/** Exercise restored cards with real native request ownership and deferred transport. */
async function checkLabRestoredSolverOwnership() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error(`Restored solver: ${message}`);
    };
    const native = labWire.exports();
    const saved = {currentMode, labFetch, setBusy, setActionRunning, solverFitFrame, mode: native.lab_workspace_mode()};
    const ids =
        ['rendered', 'parsed', 'functionStyle', 'value', 'renderedMore', 'parsedMore', 'functionMore', 'valueMore'];
    const elements = ids.map(id => {
        const element = document.getElementById(id);
        return {
            element,
            attributes: [...element.attributes].map(attribute => [attribute.name, attribute.value]),
            children: [...element.childNodes],
            disabled: element.disabled,
            clientWidth: Object.getOwnPropertyDescriptor(element, 'clientWidth')
        };
    });
    const oldOwner = functionStyle.labSolverRestoreOwner;
    const oldRenderOwner = rendered.labResultRenderRequest;
    const compact = '<svg xmlns="http://www.w3.org/2000/svg" width="2000" height="20"></svg>';
    const wrapped = '<svg xmlns="http://www.w3.org/2000/svg" width="100" height="40"></svg>';
    const posts = [], tasks = [];
    let width = 3000, mode = 'diffequation';
    const select = id => {
        mode = id === 2 ? 'diffequation' : 'expression';
        native.lab_workspace_select(id);
        labRequests.modeChanged(mode);
    };
    const start = () => {
        const count = posts.length;
        const task = fitSolverTexToCard();
        tasks.push(task);
        check(posts.length === count + 1, 'one pending fetch expected');
        return {task, post: posts[count], pending: labSolverRenderRequest};
    };
    const complete = pending => pending.post.resolve({ok: true, labData: async () => ({ok: true, svg: wrapped})});
    try {
        currentMode = () => mode;
        setBusy = () => {};
        setActionRunning = () => {};
        labFetch = (url, options) => new Promise(resolve => {
            check(url === '/render_TeX', 'native render endpoint');
            posts.push({resolve, signal: options.signal});
        });
        select(2);
        labDOM.call('lab_result_solver_invalidate');
        Object.defineProperty(functionStyle, 'clientWidth', {configurable: true, get: () => width});
        functionStyle.style.setProperty('--solver-tex-scale', '1');
        const main = labRequests.begin('evaluate', mode);
        check(main, 'initial evaluation starts');
        functionStyle.classList.add('equation-function');
        Object.assign(functionStyle.dataset, {
            solverCompactSvg: compact,
            solverCompactTex: 'compact-source',
            solverWrappedTex: 'wrapped-source',
            solverWrappedSvg: '',
            solverParentToken: String(main.token)
        });
        delete functionStyle.dataset.solverVariant;
        installSolverTexSvg(compact, 'compact');
        labRequests.finish(main);
        const snapshot = labDOM.call('lab_workspace_dom_result_save', {currentVariables: []});
        const restore = () => labDOM.call('lab_workspace_dom_result_restore', snapshot);

        // An original live-parent request must also lose ownership on same-mode restoration.
        width = 300;
        const original = start();
        restore();
        const originalReplacement = start();
        check(original.post.signal.aborted, 'restoration replacement cancels original transport');
        complete(original);
        await original.task;
        check(functionStyle.dataset.solverWrappedSvg === '', 'original reply cannot write a restored card');
        check(labSolverRenderRequest === originalReplacement.pending, 'old finaliser preserves replacement');
        complete(originalReplacement);
        await originalReplacement.task;

        select(0);
        select(2);
        restore();
        const owner = labDOM.call('lab_result_solver_owner');
        check(owner && !functionStyle.dataset.solverParentToken, 'restoration replaces obsolete main ownership');
        width = 3000;
        const before = posts.length;
        await fitSolverTexToCard();
        check(posts.length === before, 'wide restored card needs no wrapping');
        width = 300;
        const first = start();
        check(
            native.lab_request_deadline(first.pending.request.channel, first.pending.request.token) === 45000,
            'restored wrapping has a native deadline');
        await fitSolverTexToCard();
        check(posts.length === before + 1, 'same owner deduplicates wrapping');
        complete(first);
        await first.task;
        check(
            functionStyle.dataset.solverWrappedSvg === wrapped && functionStyle.dataset.solverVariant === 'wrapped',
            'restored card can fetch and display wrapping after mode return');

        restore();
        const repeated = start();
        const repeatedOwner = labDOM.call('lab_result_solver_owner');
        restore();
        check(labDOM.call('lab_result_solver_owner') !== repeatedOwner, 'identical restoration gets fresh identity');
        const replacement = start();
        check(repeated.post.signal.aborted, 'replacement aborts earlier restored fetch');
        complete(repeated);
        await repeated.task;
        check(functionStyle.dataset.solverWrappedSvg === '', 'identical stale restore cannot populate cache');
        check(labSolverRenderRequest === replacement.pending, 'stale restore cannot clear replacement');
        complete(replacement);
        await replacement.task;

        restore();
        const superseded = start();
        const newerMain = labRequests.begin('evaluate', mode);
        check(newerMain && !labDOM.call('lab_result_solver_owner'), 'same-mode main invalidates before computation');
        check(superseded.post.signal.aborted, 'new main aborts restored fetch');
        labRequests.cancel('evaluate');
        complete(superseded);
        await superseded.task;
        check(functionStyle.dataset.solverWrappedSvg === '', 'cancelled newer main cannot revive old response');
        restore();
        const cancelled = start();
        labRequests.cancel('evaluate');
        check(
            !labDOM.call('lab_result_solver_owner') && cancelled.post.signal.aborted,
            'explicit main cancellation retires restored owner and transport');
        complete(cancelled);
        await cancelled.task;
        check(functionStyle.dataset.solverWrappedSvg === '', 'cancelled restored response is ignored');

        restore();
        const away = start();
        select(0);
        select(2);
        check(!labDOM.call('lab_result_solver_owner') && away.post.signal.aborted, 'away/back invalidates ownership');
        complete(away);
        await away.task;
        check(functionStyle.dataset.solverWrappedSvg === '', 'away/back response is ignored');
        restore();
        const timedOut = start();
        const request = timedOut.pending.request;
        check(native.lab_request_timeout(request.channel, request.token), 'native timeout is recorded');
        complete(timedOut);
        await timedOut.task;
        check(functionStyle.dataset.solverWrappedSvg === '', 'late successful timeout response is ignored');
    } finally {
        labRequests.cancel('evaluate');
        for (const post of posts) post.resolve({ok: true, labData: async () => ({ok: true, svg: wrapped})});
        await Promise.allSettled(tasks);
        labSolverRenderRequest = null;
        native.lab_workspace_select(saved.mode);
        currentMode = saved.currentMode;
        labRequests.modeChanged(currentMode());
        labFetch = saved.labFetch;
        setBusy = saved.setBusy;
        setActionRunning = saved.setActionRunning;
        solverFitFrame = saved.solverFitFrame;
        for (const {element, attributes, children, disabled, clientWidth} of elements) {
            for (const attribute of [...element.attributes]) element.removeAttribute(attribute.name);
            for (const [name, value] of attributes) element.setAttribute(name, value);
            element.replaceChildren(...children);
            if (disabled !== undefined)
                element.disabled = disabled;
            if (clientWidth)
                Object.defineProperty(element, 'clientWidth', clientWidth);
            else
                delete element.clientWidth;
        }
        if (oldOwner === undefined)
            delete functionStyle.labSolverRestoreOwner;
        else
            functionStyle.labSolverRestoreOwner = oldOwner;
        if (oldRenderOwner === undefined)
            delete rendered.labResultRenderRequest;
        else
            rendered.labResultRenderRequest = oldRenderOwner;
    }
}
