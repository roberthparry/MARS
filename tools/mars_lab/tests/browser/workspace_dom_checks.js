/** Workspace projection regressions against the live C/WASM controller and scoped DOM bridge. */
window.checkLabWorkspaceDom = function checkLabWorkspaceDom() {
    const check = (ok, message) => {
        if (!ok)
            throw new Error('C workspace DOM: ' + message);
    };
    const hidden = node => node.classList.contains('hidden');
    const native = labWire.exports();
    const savedMode = native.lab_workspace_mode();
    const savedPrecision = native.lab_workspace_precision(savedMode);
    const savedExpansion = native.lab_view_card_expanded();
    const savedDerivative = takeDerivative, savedIntegral = takeIntegral;
    const savedDifferentiable = currentDifferentiable;
    const savedMetadata = {lastTex, lastDerivativeExpression, currentVariables, resultInputBindings};
    const savedFocus = document.activeElement;
    const savedEditors = labTextareas.slice();
    const savedSelection = [goalTarget.selectionStart, goalTarget.selectionEnd, goalTarget.selectionDirection];
    const savedFrames = [editorResizeFrame, renderedTeXFitFrame];
    const savedResultState = modeResultState[currentMode()];
    const snapshots = [document.body, ...document.body.querySelectorAll('*')].map(
        node => ({
            node,
            attributes:
                [...node.attributes].map(attribute => [attribute.namespaceURI, attribute.name, attribute.value]),
            children: [...node.childNodes],
            value: node.matches('input, select, textarea') ? node.value : undefined,
            selected: node.tagName === 'OPTION' ? node.selected : undefined
        }));
    try {
        const panels = [
            null, equationControls, diffequationControls, matrixControls, integratorControls, datetimeControls,
            almanacControls
        ];
        const calculus = [true, false, false, true, false, false, false];
        const valueVisible = [false, true, true, false, true, true, false];
        const classes = ['datetime', 'almanac', 'diffequation', 'matrix'];
        value.textContent = ' \n\t ';
        datetimeLocalBody.textContent = 'Local weather';
        for (let mode = 0; mode < 7; ++mode) {
            native.lab_workspace_select(mode);
            syncModeTabs();
            for (const tab of modeTabs) {
                const selected = tab.dataset.mode === WORKSPACE_MODE_NAMES[mode];
                check(tab.classList.contains('active') === selected, 'exact mode tab selection');
                check(tab.getAttribute('aria-selected') === String(selected), 'tab accessibility selection');
                check(tab.tabIndex === (selected ? 0 : -1), 'only selected tab is in the tab order');
            }
            labDOM.call('lab_workspace_dom_mode', mode, 'fixture coverage');
            for (let index = 1; index < panels.length; ++index)
                check(hidden(panels[index]) === (index !== mode), 'only selected mode panel is shown');
            for (const name of classes)
                check(
                    document.body.classList.contains(name + '-mode') === (WORKSPACE_MODE_NAMES[mode] === name),
                    'mode-specific body class');
            check(hidden(derivativeButtons) === !calculus[mode], 'calculus visibility');
            check(hidden(goalSeek) === (mode !== 0), 'goal seek is expression-only');
            check(hidden(valueCard) === !valueVisible[mode], 'value policy without a numerical result');
            check(valueCard.hasAttribute('hidden') === !valueVisible[mode], 'value hidden attribute');
            check(hidden(datetimeLocal) === (mode !== 5), 'local weather belongs to Datetime');
            const labels = labDOM.call('lab_workspace_dom_labels', mode);
            check(leftPaneTitle.textContent === labels[0], 'native workspace heading');
            check(
                renderedTitle.textContent === labels[2] && valueTitle.textContent === labels[5], 'native card titles');
            check(hidden(functionRun) === (labels[4] !== 'Function'), 'Function execution visibility');
            if (mode === 6)
                check(subtitle.textContent.endsWith('fixture coverage.'), 'almanac coverage suffix');
        }
        native.lab_workspace_select(0);
        targetRow.classList.remove('hidden');
        value.textContent = '1';
        labDOM.call('lab_workspace_dom_mode', 0, '');
        check(!hidden(targetRow) && !hidden(valueCard), 'expression preserves open target and numerical value');
        labDOM.call('lab_workspace_dom_mode', 2, '');
        labDOM.call('lab_workspace_dom_mode', 0, '');
        check(hidden(targetRow), 'switching back does not reopen a closed target');
        datetimeLocalBody.textContent = '\u2003';
        labDOM.call('lab_workspace_dom_mode', 5, '');
        check(hidden(datetimeLocal), 'Unicode-whitespace local data is empty');

        const tags = ['', ' , \u2003, ', ' expression , matrix ', 'diffequation', 'express', 'Expression'];
        const cards = tags.map(text => {
            const card = document.createElement('section');
            card.dataset.helpModes = text;
            return card;
        });
        labDOM.call('lab_workspace_dom_help_cards', cards, 'expression');
        cards.forEach((card, index) => check(hidden(card) === (index > 2), 'help tags match exact trimmed tokens'));
        cards[0].dataset.helpModes = Array(1500).fill('unknown').join(',') + ', expression';
        labDOM.call('lab_workspace_dom_help_cards', cards, 'expression');
        check(!hidden(cards[0]), 'long help metadata releases temporary handles');

        for (const operation of ['eval', 'solve', 'multiply', 'det']) {
            matrixOperation.value = operation;
            labDOM.call('lab_workspace_dom_matrix', 3);
            const needed = operation === 'solve' || operation === 'multiply';
            check(hidden(matrixOperand) === !needed && hidden(matrixOperandLabel) === !needed, 'matrix operand policy');
            labDOM.call('lab_workspace_dom_matrix', 0);
            check(hidden(matrixOperand) && hidden(matrixOperandLabel), 'matrix operation cannot expose other modes');
        }

        native.lab_workspace_select(savedMode);
        native.lab_workspace_request_precision(savedMode, 53);
        check(precisionStatusText() === '17 digits / 53 bits', 'native precision status');
        setStatus('Ready μ');
        check(statusEl.textContent === 'Ready μ · 17 digits / 53 bits', 'opaque Unicode status text');
        resultUseInput.dataset.inputText = 'x';
        showHelp();
        check(hidden(resultPane) && !hidden(helpPane) && hidden(resultUseInput), 'help pane and reuse visibility');
        check(help.textContent === 'Result' && rightPaneTitle.textContent === 'Help', 'help navigation labels');
        toggleHelp();
        check(!hidden(resultPane) && hidden(helpPane) && !hidden(resultUseInput), 'toggle returns to reusable result');
        check(statusEl.textContent.startsWith('Ready · '), 'returning from help reports Ready');
        resultUseInput.dataset.inputText = '';
        setStatus('Preserved');
        showResults();
        check(
            hidden(resultUseInput) && statusEl.textContent.startsWith('Preserved · '), 'showResults preserves status');

        goalTarget.disabled = false;
        goalTarget.value = '3.14159';
        showTargetEntry();
        check(!hidden(targetRow) && goalSeek.textContent === 'Run goal seek', 'goal entry is revealed');
        check(document.activeElement === goalTarget, 'goal focus effect runs after the DOM call');
        check(
            goalTarget.selectionStart === 0 && goalTarget.selectionEnd === goalTarget.value.length,
            'goal text selection');
        hideTargetEntry();
        check(hidden(targetRow) && goalSeek.textContent === 'Goal seek', 'goal entry reset');

        for (let mode = 0; mode < 7; ++mode) {
            labDOM.call('lab_workspace_dom_controls', mode, 0, 1, 1, 1, 1, 0, 0);
            check(!run.disabled && !back.disabled && !forward.disabled, 'ready history controls');
            check(goalSeek.disabled === (mode !== 0), 'goal availability follows mode');
            labDOM.call('lab_workspace_dom_controls', mode, 1, 1, 1, 1, 1, 0, 0);
            check(
                [run, back, forward, goalSeek, lessPrecision, morePrecision].every(button => button.disabled),
                'busy disables every worksheet action');
            check(goalSeek.title === '' && morePrecision.title === '', 'busy does not imply unavailable precision');
        }
        labDOM.call('lab_workspace_dom_controls', 0, 0, 0, 0, 0, 0, 1, 1);
        check(
            goalSeek.title.includes('variable binding') && morePrecision.title.includes('maximum'), 'disabled reasons');

        const enabled = document.createElement('input'), disabled = document.createElement('input');
        disabled.disabled = true;
        integratorBoundStack.append(enabled, disabled);
        const detachedCopy = document.createElement('button');
        labDOM.call('lab_workspace_dom_busy', 1, null, [detachedCopy], []);
        labDOM.call('lab_workspace_dom_busy', 1, null, [detachedCopy], []);
        check(enabled.disabled && disabled.disabled && detachedCopy.disabled, 'busy is idempotent for repeated starts');
        check(
            enabled.dataset.busyDisabled === '1' && !disabled.dataset.busyDisabled,
            'only newly disabled controls owned');
        labDOM.call('lab_workspace_dom_busy', 0, null, [detachedCopy], []);
        check(
            !enabled.disabled && disabled.disabled && !detachedCopy.disabled,
            'busy completion preserves prior disabled');
        check(!enabled.hasAttribute('data-busy-disabled'), 'busy ownership removed on completion');
        labDOM.call('lab_workspace_dom_busy', 0, null, [detachedCopy], []);
        check(disabled.disabled, 'repeated completion preserves pre-existing disabled state');

        const action = document.createElement('button');
        setActionRunning(action, true);
        check(
            action.classList.contains('action-running') && action.getAttribute('aria-busy') === 'true',
            'running state');
        setActionRunning(action, false);
        setActionRunning(null, true);
        check(
            !action.classList.contains('action-running') && !action.hasAttribute('aria-busy'), 'running state cleared');

        collapseResultCards();
        native.lab_view_card_toggle(resultCardIds.get(valueCard));
        renderResultCardExpansion();
        valueCard.style.display = 'block';
        setValueCardVisible(true);
        check(resultCardExpanded(valueCard) && valueCard.style.display === '', 'showing value respects expansion');
        setValueCardVisible(false);
        check(native.lab_view_card_expanded() === -1 && hidden(valueCard), 'hiding expanded value collapses it');
        const parsedCard = parsed.closest('.result-card');
        native.lab_view_card_toggle(resultCardIds.get(parsedCard));
        renderResultCardExpansion();
        labDOM.call('lab_workspace_dom_aux', Number(!!false));
        check(
            native.lab_view_card_expanded() === -1 && hidden(parsedCard),
            'hiding auxiliary results collapses selection');

        const element = document.createElement('div');
        element.className = 'native-result';
        element.style.height = '25px';
        element.innerHTML = '<i>μ &amp; σ</i>';
        element.dataset.fullText = '\u0000exact α\n';
        element.dataset.empty = '';
        const elementState = labDOM.call('lab_workspace_dom_snapshot', element, 0);
        element.dataset.fullText = 'changed';
        element.dataset.stale = 'obsolete';
        element.innerHTML = '<b>replacement</b>';
        check(elementState.dataset.fullText === '\u0000exact α\n', 'snapshot dataset is independent');
        labDOM.call('lab_workspace_dom_restore', element, elementState, 0);
        check(
            element.innerHTML === '<i>μ &amp; σ</i>' && element.style.height === '25px',
            'element markup/style round trip');
        check(
            element.dataset.fullText === '\u0000exact α\n' && !('stale' in element.dataset),
            'exact data and stale-key removal');
        check(element.hasAttribute('data-empty'), 'empty dataset attributes survive');
        action.className = 'more-digits';
        action.textContent = 'Show fewer digits';
        action.disabled = true;
        action.dataset.expanded = 'true';
        const buttonState = labDOM.call('lab_workspace_dom_snapshot', action, 1);
        action.disabled = false;
        action.dataset.obsolete = 'old';
        labDOM.call('lab_workspace_dom_restore', action, buttonState, 1);
        check(action.disabled && action.textContent === 'Show fewer digits', 'button snapshot restores disabled/label');
        check(
            action.dataset.expanded === 'true' && !('obsolete' in action.dataset),
            'button snapshot restores exact dataset');
        labDOM.call('lab_workspace_dom_restore', element, {}, 0);
        check(
            element.className === '' && element.innerHTML === '' && !Object.keys(element.dataset).length,
            'empty snapshot clears');

        rendered.textContent = 'rendered fixture';
        parsed.textContent = 'parsed fixture';
        resultUseInput.dataset.inputText = 'saved input';
        const variables = ['μ', 'x'];
        const saved = labDOM.call('lab_workspace_dom_result_save', {
            lastTex: 'native TeX',
            lastDerivativeExpression: 'native expression',
            currentVariables: variables,
            currentDifferentiable: false
        });
        variables.push('new variable');
        check(
            saved.currentVariables.length === 2 && saved.resultInputText === 'saved input',
            'result snapshot owns metadata copy');
        rendered.textContent = 'changed result';
        parsed.dataset.stale = 'stale';
        const restored = labDOM.call('lab_workspace_dom_result_restore', saved);
        check(
            rendered.textContent === 'rendered fixture' && parsed.textContent === 'parsed fixture',
            'result snapshot restores cards');
        check(!('stale' in parsed.dataset), 'result snapshot removes stale presentation');
        check(
            restored.lastTex === 'native TeX' && restored.lastDerivativeExpression === 'native expression' &&
                restored.currentDifferentiable === false,
            'native restoration returns opaque metadata and explicit differentiability');
        check(
            restored.currentVariables !== saved.currentVariables && restored.currentVariables.join(',') === 'μ,x',
            'restore owns a fresh variable list');
        restored.currentVariables.push('restored-only');
        check(saved.currentVariables.length === 2, 'editing restored variables cannot mutate the saved mode');
        check(
            Array.isArray(restored.resultInputBindings) && restored.resultInputBindings.length === 0,
            'restoration drops prior reusable-input bindings');
        check(
            resultUseInput.dataset.inputText === 'saved input' && !resultUseInput.disabled && !hidden(resultUseInput),
            'native restore projects reusable input before returning metadata');
        const initialResults = labDOM.call('lab_workspace_dom_result_states');
        check(
            Object.keys(initialResults).join(',') === WORKSPACE_MODE_NAMES.join(',') &&
                Object.values(initialResults).every(state => state === null),
            'native result table initialises exactly the seven absent mode snapshots');
        initialResults.expression = saved;
        check(labDOM.call('lab_workspace_dom_result_states').expression === null, 'mode result tables are independent');

        for (const differentiable of [undefined, null, 0, '', 'false', true, false]) {
            const legacy = labDOM.call('lab_workspace_dom_result_restore', {
                ...saved,
                lastTex: null,
                lastDerivativeExpression: false,
                currentVariables: {0: 'not-an-array', length: 1},
                currentDifferentiable: differentiable,
                resultInputText: ' \u2003 '
            });
            check(legacy.lastTex === '' && legacy.lastDerivativeExpression === '', 'native metadata text defaults');
            check(
                Array.isArray(legacy.currentVariables) && !legacy.currentVariables.length,
                'array-like variables rejected');
            check(legacy.currentDifferentiable === (differentiable !== false), 'only boolean false disables calculus');
            check(
                resultUseInput.dataset.inputText === '' && resultUseInput.disabled && hidden(resultUseInput),
                'blank restored input is unavailable');
        }
        const longVariables = Array.from({length: 5000}, (_, index) => 'opaque-' + index);
        const largeResult = labDOM.call('lab_workspace_dom_result_save', {currentVariables: longVariables});
        const largeRestore = labDOM.call('lab_workspace_dom_result_restore', largeResult);
        check(
            largeRestore.currentVariables.length === 5000 && largeRestore.currentVariables[4999] === 'opaque-4999' &&
                largeRestore.currentVariables !== longVariables &&
                largeRestore.currentVariables !== largeResult.currentVariables,
            'large metadata copies survive bounded scoped handle release');
        const defaultCapture = labDOM.call('lab_workspace_dom_result_save', {currentVariables: 'not an array'});
        check(
            defaultCapture.lastTex === '' && defaultCapture.lastDerivativeExpression === '' &&
                defaultCapture.currentDifferentiable === true && defaultCapture.currentVariables.length === 0,
            'capture and restoration share metadata defaults');

        const lifecycle = [];
        const savedLifecycle =
            {clearFunctionRun, clearResultPane, renderDerivativeButtons, scheduleRenderedTeXFit, scheduleSolverTexFit};
        try {
            clearFunctionRun = () => lifecycle.push('cancel');
            clearResultPane = () => lifecycle.push('clear');
            renderDerivativeButtons = names => {
                lifecycle.push('derivatives');
                check(
                    names === currentVariables && names !== modeResultState[currentMode()].currentVariables &&
                        lastTex === 'native TeX' && lastDerivativeExpression === 'native expression' &&
                        currentDifferentiable === false && resultInputBindings.length === 0,
                    'globals are assigned before derivative refresh');
                check(
                    rendered.textContent === 'rendered fixture' && resultUseInput.dataset.inputText === 'saved input',
                    'cards and reusable input are restored before derivative refresh');
                savedLifecycle.renderDerivativeButtons(names);
            };
            scheduleRenderedTeXFit = () => {
                lifecycle.push('rendered-fit');
                check(!derivativeButtons.children.length, 'calculus is refreshed before fitting');
            };
            scheduleSolverTexFit = () => lifecycle.push('solver-fit');
            modeResultState[currentMode()] = saved;
            lastTex = 'stale TeX';
            currentDifferentiable = true;
            resultInputBindings = [{name: 'stale'}];
            restoreModeResultState();
            check(
                lifecycle.join(',') === 'cancel,derivatives,rendered-fit,solver-fit',
                'restoration cancels function work, refreshes calculus, then schedules both fits');
            currentVariables.push('live-only');
            check(saved.currentVariables.length === 2, 'JS globals never alias the saved variable list');
            saveCurrentModeResultState();
            check(
                modeResultState[currentMode()].currentVariables.length === 3 &&
                    modeResultState[currentMode()].currentVariables !== currentVariables,
                'JS save adapter captures an independent native metadata snapshot');
            lifecycle.length = 0;
            modeResultState[currentMode()] = null;
            restoreModeResultState();
            check(lifecycle.join(',') === 'cancel,clear', 'absent snapshot clears without scheduling either fit');
        } finally {
            ({
                clearFunctionRun,
                clearResultPane,
                renderDerivativeButtons,
                scheduleRenderedTeXFit,
                scheduleSolverTexFit
            } = savedLifecycle);
        }
        for (const node of [rendered, parsed, functionStyle, value]) node.textContent = '\u2003';
        check(!labDOM.call('lab_workspace_dom_has_result'), 'Unicode-whitespace result is empty');
        check(labDOM.call('lab_workspace_dom_result_save', {}) === null, 'empty result is not saved');
        rendered.innerHTML = '<svg></svg>';
        check(!!labDOM.call('lab_workspace_dom_has_result'), 'rendered markup is content even without text');

        let measuredHeight = 120, scrollHeight = 300;
        const editor = document.createElement('textarea');
        Object.defineProperties(editor, {clientHeight: {get: () => 100}, scrollHeight: {get: () => scrollHeight}});
        editor.getClientRects = () => [{}];
        editor.getBoundingClientRect = () => ({height: measuredHeight});
        editor.style.color = 'red';
        document.body.append(editor);
        labTextareas.splice(0, labTextareas.length, editor);
        check(!!labDOM.call('lab_workspace_dom_editor_visible', editor), 'visible fixture editor is eligible');
        syncEditorResizeGrip();
        check(
            editor.dataset.automaticHeight === '120' && editor.style.height === '120px',
            'overflow acquires resize grip');
        scrollHeight = 100;
        measuredHeight = 155;
        syncEditorResizeGrip();
        check(editor.classList.contains('editor-manual-size'), 'user resize survives cleared overflow');
        measuredHeight = 120;
        syncEditorResizeGrip();
        check(
            !editor.classList.contains('editor-manual-size') && editor.style.height === '',
            'unneeded resize grip clears');
        scrollHeight = 300;
        syncEditorResizeGrip();
        editor.remove();
        check(!labDOM.call('lab_workspace_dom_editor_visible', editor), 'detached editor cannot retain layout');
        syncEditorResizeGrip();
        check(
            !editor.hasAttribute('data-automatic-height') && editor.style.maxHeight === '',
            'detached editor sizing reset');
        resetEditorManualSize();
        check(editor.style.color === 'red', 'sizing reset preserves unrelated styles');

        const calls = [];
        takeDerivative = (name, button) => calls.push(['derivative', name, button]);
        takeIntegral = (name, button) => calls.push(['integral', name, button]);
        currentDifferentiable = true;
        const names = ['μ', '<literal & name>'];
        renderDerivativeButtons(names);
        const buttons = [...derivativeButtons.querySelectorAll('button')];
        check(buttons.length === 4, 'each variable has derivative and integral actions');
        check(
            buttons[0].textContent.endsWith(' derivative') && buttons[2].textContent.endsWith(' integral'),
            'calculus ordering');
        check(
            buttons[1].dataset.variable === names[1] && !buttons[1].querySelector('literal'), 'names stay opaque text');
        buttons.forEach(button => button.click());
        check(
            calls.length === 4 && calls[0][0] === 'derivative' && calls[0][1] === names[0] &&
                calls[3][0] === 'integral' && calls[3][1] === names[1] && calls[3][2] === buttons[3],
            'browser callbacks forward C action metadata and actual button');
        renderDerivativeButtons(['x']);
        derivativeButtons.querySelector('button').click();
        check(calls.length === 5, 'rerender does not duplicate event callbacks');
        currentDifferentiable = false;
        renderDerivativeButtons(names);
        check(!derivativeButtons.children.length, 'non-differentiable results clear all calculus buttons');
        const manyNames = Array.from({length: 600}, (_, index) => `v${index}`);
        labDOM.call('lab_workspace_dom_derivatives', manyNames, manyNames, 1);
        check(derivativeButtons.children.length === 1200, 'large action list stays within scoped handle budget');
    } finally {
        takeDerivative = savedDerivative;
        takeIntegral = savedIntegral;
        currentDifferentiable = savedDifferentiable;
        ({lastTex, lastDerivativeExpression, currentVariables, resultInputBindings} = savedMetadata);
        native.lab_workspace_select(savedMode);
        native.lab_workspace_precision_set(savedMode, savedPrecision);
        modeResultState[currentMode()] = savedResultState;
        labTextareas.splice(0, labTextareas.length, ...savedEditors);
        if (editorResizeFrame !== savedFrames[0])
            cancelAnimationFrame(editorResizeFrame);
        if (renderedTeXFitFrame !== savedFrames[1])
            cancelAnimationFrame(renderedTeXFitFrame);
        [editorResizeFrame, renderedTeXFitFrame] = savedFrames;
        native.lab_view_cards_collapse();
        if (savedExpansion >= 0)
            native.lab_view_card_toggle(savedExpansion);
        for (const {node, attributes, children} of snapshots) {
            for (const attribute of [...node.attributes]) node.removeAttribute(attribute.name);
            for (const [namespace, name, value] of attributes) node.setAttributeNS(namespace, name, value);
            node.replaceChildren(...children);
        }
        for (const {node, value: savedValue, selected} of snapshots) {
            if (savedValue !== undefined)
                node.value = savedValue;
            if (selected !== undefined)
                node.selected = selected;
        }
        goalTarget.setSelectionRange(...savedSelection);
        savedFocus?.focus();
    }
};
