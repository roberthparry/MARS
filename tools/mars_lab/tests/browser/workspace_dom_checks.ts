/**
 * @file workspace_dom_checks.ts
 * @brief Workspace DOM projection and ownership regressions.
 *
 * Checks the live native workspace controller and scoped DOM bridge using browser fixtures that preserve the surrounding worksheet state.
 */

/** Workspace projection regressions against the live C/WASM controller and scoped DOM bridge. */
window.checkLabWorkspaceDom = function checkLabWorkspaceDom() {
    function check(ok: unknown, message: string): void {
        if (!ok)
            throw new Error('C workspace DOM: ' + message);
    }
    function hidden(node: HTMLElement): boolean {
        return node.classList.contains('hidden');
    }
    const native = labWire.exports();
    const savedMode = native.lab_workspace_mode();
    const savedPrecision = native.lab_workspace_precision(savedMode);
    const savedExpansion = native.lab_view_card_expanded();
    const savedDerivative = WorksheetRequests.plots.takeDerivative;
    const savedIntegral = WorksheetRequests.plots.takeIntegral;
    const savedPlot = WorksheetRequests.plots.takePlot;
    const savedDifferentiable = WorksheetBindings.panel.currentDifferentiable;
    const savedMetadata = {
        lastTex: WorksheetWorkspace.presentation.state.lastTex,
        lastDerivativeExpression: WorksheetWorkspace.presentation.state.lastDerivativeExpression,
        currentVariables: WorksheetBindings.panel.currentVariables,
        resultInputBindings: WorksheetWorkspace.presentation.state.resultInputBindings
    };
    const savedFocus = document.activeElement;
    const savedEditors = WorksheetContext.labTextareas.slice();
    const savedSelection = {
        start: WorksheetContext.goalTarget.selectionStart,
        end: WorksheetContext.goalTarget.selectionEnd,
        direction: WorksheetContext.goalTarget.selectionDirection
    };
    const savedEditorFrame = WorksheetWorkspace.layout.editorResizeFrame;
    const savedResultState = WorksheetWorkspace.presentation.resultStates[WorksheetWorkspace.mode.currentMode()];
    const snapshots: LabWorksheetTestNodeSnapshot[] = [];
    for (const node of [document.body, ...document.body.querySelectorAll<HTMLElement>('*')]) {
        const attributes: LabWorksheetTestNodeSnapshot['attributes'] = [];
        for (const attribute of node.attributes)
            attributes.push({namespace: attribute.namespaceURI, name: attribute.name, value: attribute.value});
        snapshots.push({
            node,
            attributes,
            children: [...node.childNodes],
            value: node.matches('input, select, textarea') ? (node as LabWorksheetTestSnapshotElement).value : undefined,
            selected: node.tagName === 'OPTION' ? (node as HTMLOptionElement).selected : undefined
        });
    }
    try {
        const panels = [
            null, WorksheetContext.equationControls, WorksheetContext.diffequationControls, WorksheetContext.matrixControls, WorksheetContext.integratorControls, WorksheetContext.datetimeControls,
            WorksheetContext.almanacControls
        ];
        const calculus = [true, true, true, true, false, false, false];
        const valueVisible = [false, true, true, false, true, true, false];
        const classes = ['datetime', 'almanac', 'diffequation', 'matrix'];
        WorksheetContext.value.textContent = ' \n\t ';
        WorksheetContext.datetimeLocalBody.textContent = 'Local weather';
        for (let mode = 0; mode < 7; ++mode) {
            native.lab_workspace_select(mode);
            WorksheetWorkspace.presentation.syncModeTabs();
            for (const tab of WorksheetContext.modeTabs) {
                const selected = tab.dataset.mode === WorksheetWorkspace.WORKSPACE_MODE_NAMES[mode];
                check(tab.classList.contains('active') === selected, 'exact mode tab selection');
                check(tab.getAttribute('aria-selected') === String(selected), 'tab accessibility selection');
                check(tab.tabIndex === (selected ? 0 : -1), 'only selected tab is in the tab order');
            }
            WorksheetContext.rightPaneTitle.textContent = 'x plot RESULT';
            labDOM.call('lab_workspace_dom_mode', mode, 'fixture coverage');
            check(WorksheetContext.rightPaneTitle.textContent === 'Result', 'mode projection clears the previous operation heading');
            for (let index = 1; index < panels.length; ++index)
                check(hidden(panels[index]!) === (index !== mode), 'only selected mode panel is shown');
            for (const name of classes)
                check(
                    document.body.classList.contains(name + '-mode') === (WorksheetWorkspace.WORKSPACE_MODE_NAMES[mode] === name),
                    'mode-specific body class');
            check(hidden(WorksheetContext.derivativeButtons) === !calculus[mode], 'calculus visibility');
            check(hidden(WorksheetContext.goalSeek) === (mode !== 0), 'goal seek is expression-only');
            check(hidden(WorksheetContext.run) === (mode === 6), 'automatic Almanac hides Evaluate; other modes retain it');
            const actions = document.getElementById('workspaceActions')!;
            const actionHome = document.getElementById(mode === 6 ? 'almanacActions' : 'workspaceActionsHome')!;
            check(actions.parentElement === actionHome && actions.contains(WorksheetContext.back),
                  'original action buttons move below the globe only in Almanac mode');
            if (mode === 6) {
                check(actionHome.previousElementSibling?.id === 'almanacGlobe' &&
                      actionHome.nextElementSibling?.classList.contains('almanac-briefing'),
                      'Almanac actions sit between globe and worksheet introduction');
                check(document.getElementById('workspaceActionsHome')!.getBoundingClientRect().height >= 48,
                      'former button area remains reserved for country and town dropdowns');
                const backDisabled = WorksheetContext.back.disabled;
                labDOM.call('lab_workspace_dom_busy', 1, null, [], []);
                check(WorksheetContext.back.disabled === backDisabled && !WorksheetContext.help.disabled &&
                      !WorksheetContext.clear.disabled, 'calendar busy handling does not take over shared actions');
                labDOM.call('lab_workspace_dom_busy', 0, null, [], []);
            }
            check(hidden(WorksheetContext.valueCard) === !valueVisible[mode], 'value policy without a numerical result');
            check(WorksheetContext.valueCard.hasAttribute('hidden') === !valueVisible[mode], 'value hidden attribute');
            check(hidden(WorksheetContext.datetimeLocal) === (mode !== 5), 'local weather belongs to Datetime');
            check(hidden(document.querySelector('.precision-toolbar') as HTMLElement) === (mode === 5 || mode === 6),
                  'precision toolbar is hidden only in Datetime and Almanac modes');
            check((getComputedStyle(document.querySelector('.precision-toolbar')!).display === 'none') ===
                  (mode === 5 || mode === 6), 'precision toolbar visibility survives layout styling');
            const labels = labDOM.call<string[]>('lab_workspace_dom_labels', mode);
            check(WorksheetContext.leftPaneTitle.textContent === labels[0], 'native workspace heading');
            check(
                WorksheetContext.renderedTitle.textContent === labels[2] && WorksheetContext.valueTitle.textContent === labels[5], 'native card titles');
            check(hidden(WorksheetContext.functionRun) === (labels[4] !== 'Function'), 'Function execution visibility');
            if (mode === 6)
                check(WorksheetContext.subtitle.textContent.endsWith('fixture coverage.'), 'almanac coverage suffix');
        }
        native.lab_workspace_select(0);
        labDOM.call('lab_workspace_dom_help', 1);
        labDOM.call('lab_workspace_dom_mode', 0, '');
        check(document.getElementById('workspaceActions')!.parentElement?.id === 'workspaceActionsHome',
              'leaving Almanac restores the original action-row position');
        check(WorksheetContext.rightPaneTitle.textContent === 'Help', 'mode projection retains the heading of visible Help');
        labDOM.call('lab_workspace_dom_help', 0);
        WorksheetContext.targetRow.classList.remove('hidden');
        WorksheetContext.value.textContent = '1';
        labDOM.call('lab_workspace_dom_mode', 0, '');
        check(!hidden(WorksheetContext.targetRow) && !hidden(WorksheetContext.valueCard), 'expression preserves open target and numerical value');
        labDOM.call('lab_workspace_dom_mode', 2, '');
        labDOM.call('lab_workspace_dom_mode', 0, '');
        check(hidden(WorksheetContext.targetRow), 'switching back does not reopen a closed target');
        WorksheetContext.datetimeLocalBody.textContent = '\u2003';
        labDOM.call('lab_workspace_dom_mode', 5, '');
        check(hidden(WorksheetContext.datetimeLocal), 'Unicode-whitespace local data is empty');

        const tags = ['', ' , \u2003, ', ' expression , matrix ', 'diffequation', 'express', 'Expression'];
        const cards: HTMLElement[] = [];
        for (const text of tags) {
            const card = document.createElement('section');
            card.dataset.helpModes = text;
            cards.push(card);
        }
        labDOM.call('lab_workspace_dom_help_cards', cards, 'expression');
        for (let index = 0; index < cards.length; ++index)
            check(hidden(cards[index]) === (index > 2), 'help tags match exact trimmed tokens');
        cards[0].dataset.helpModes = Array(1500).fill('unknown').join(',') + ', expression';
        labDOM.call('lab_workspace_dom_help_cards', cards, 'expression');
        check(!hidden(cards[0]), 'long help metadata releases temporary handles');

        for (const operation of ['eval', 'solve', 'multiply', 'det']) {
            WorksheetContext.matrixOperation.value = operation;
            labDOM.call('lab_workspace_dom_matrix', 3);
            const needed = operation === 'solve' || operation === 'multiply';
            check(hidden(WorksheetContext.matrixOperand) === !needed && hidden(WorksheetContext.matrixOperandLabel) === !needed, 'matrix operand policy');
            labDOM.call('lab_workspace_dom_matrix', 0);
            check(hidden(WorksheetContext.matrixOperand) && hidden(WorksheetContext.matrixOperandLabel), 'matrix operation cannot expose other modes');
        }

        native.lab_workspace_select(savedMode);
        native.lab_workspace_request_precision(savedMode, 53);
        for (const calendarMode of [5, 6]) {
            native.lab_workspace_select(calendarMode);
            WorksheetWorkspace.presentation.setStatus('Calculating');
            check(!WorksheetContext.statusEl.textContent!.includes('digits /'), 'calendar status omits numeric precision');
        }
        const skyTemplate = document.getElementById('homeSky') || document.createElement('span');
        skyTemplate.id = 'homeSky';
        skyTemplate.className = 'sky-status-template';
        skyTemplate.hidden = true;
        skyTemplate.innerHTML = '<span class="sky-status-strip"><svg aria-label="Moon"></svg>' +
            '<span>Sunset 18:20</span><span>Sunrise 07:10</span><span>12:00 GMT</span><img class="country-flag" alt="Finland"></span>';
        document.body.append(skyTemplate);
        for (const calendarMode of [5, 6]) {
            native.lab_workspace_select(calendarMode);
            labDOM.call('lab_workspace_dom_mode', calendarMode, '');
            WorksheetWorkspace.presentation.setStatus('Ready');
            check(Boolean(WorksheetContext.statusEl.querySelector('svg')) &&
                  WorksheetContext.statusEl.textContent!.includes('Sunset 18:20Sunrise 07:1012:00 GMT') &&
                  Boolean(WorksheetContext.statusEl.querySelector('img.country-flag')),
                  'calendar status installs the native Moon, sunset, sunrise, time and flag strip');
            if (window.innerWidth > 900) {
                const mobile = document.querySelector('#mobileAccess summary')!.getBoundingClientRect();
                const sky = WorksheetContext.statusEl.querySelector('.sky-status-strip')!.getBoundingClientRect();
                check(Math.abs(mobile.top - sky.top) < 1,
                      'DateTime and Almanac sky cards align with the top of the Mobile button');
            }
            WorksheetWorkspace.presentation.setStatus('Calculating');
            check(Boolean(WorksheetContext.statusEl.querySelector('svg')), 'worksheet loading preserves the independent home sky');
        }
        native.lab_workspace_select(6);
        const skySnapshot = labDOM.call('lab_workspace_dom_result_save', {});
        WorksheetContext.statusEl.textContent = 'Ready';
        labDOM.call('lab_workspace_dom_result_restore', skySnapshot);
        check(Boolean(WorksheetContext.statusEl.querySelector('.sky-status-strip svg')) &&
              WorksheetContext.statusEl.textContent!.includes('Sunset 18:20'),
              'restoring a retained calendar result restores its sky header without reevaluation');
        skyTemplate.remove();
        native.lab_workspace_select(6);
        labDOM.call('lab_workspace_dom_mode', 6, '');
        WorksheetContext.statusEl.innerHTML = '<span class="sky-status-strip">Moon: 87.1% illuminated</span>';
        native.lab_workspace_select(4);
        labDOM.call('lab_workspace_dom_mode', 4, '');
        check(!WorksheetContext.statusEl.querySelector('.sky-status-strip') &&
              WorksheetContext.statusEl.textContent!.startsWith('Ready · ') &&
              !WorksheetContext.statusEl.textContent!.includes('Moon'),
              'switching from Almanac to Integrator immediately clears calendar status before evaluation');
        native.lab_workspace_select(savedMode);
        check(precisionStatusText() === '17 digits / 53 bits', 'native precision status');
        WorksheetWorkspace.presentation.setStatus('Ready μ');
        check(WorksheetContext.statusEl.textContent === 'Ready μ · 17 digits / 53 bits', 'opaque Unicode status text');
        WorksheetContext.resultUseInput.dataset.inputText = 'x';
        showHelp();
        check(hidden(WorksheetContext.resultPane) && !hidden(WorksheetContext.helpPane) && hidden(WorksheetContext.resultUseInput), 'help pane and reuse visibility');
        check(WorksheetContext.help.textContent === 'Result' && WorksheetContext.rightPaneTitle.textContent === 'Help', 'help navigation labels');
        const helpClose = document.getElementById('helpClose')!;
        check(document.getElementById('labWorkspace')!.classList.contains('help-open'), 'full-width help state');
        check(!hidden(helpClose) && document.activeElement === helpClose, 'visible return action receives focus');
        WorksheetWorkspace.presentation.toggleHelp();
        check(!hidden(WorksheetContext.resultPane) && hidden(WorksheetContext.helpPane) && !hidden(WorksheetContext.resultUseInput), 'toggle returns to reusable result');
        check(WorksheetContext.statusEl.textContent.startsWith('Ready · '), 'returning from help reports Ready');
        check(hidden(helpClose) && document.activeElement === WorksheetContext.help, 'return restores focus to the Help button');
        check(!document.getElementById('labWorkspace')!.classList.contains('help-open'), 'two-panel state restored');
        WorksheetContext.resultUseInput.dataset.inputText = '';
        WorksheetWorkspace.presentation.setStatus('Preserved');
        WorksheetWorkspace.presentation.showResults();
        check(
            hidden(WorksheetContext.resultUseInput) && WorksheetContext.statusEl.textContent.startsWith('Preserved · '), 'showResults preserves status');

        WorksheetContext.goalTarget.disabled = false;
        WorksheetContext.goalTarget.value = '3.14159';
        showTargetEntry();
        check(!hidden(WorksheetContext.targetRow) && WorksheetContext.goalSeek.textContent === 'Run goal seek', 'goal entry is revealed');
        check(document.activeElement === WorksheetContext.goalTarget, 'goal focus effect runs after the DOM call');
        check(
            WorksheetContext.goalTarget.selectionStart === 0 && WorksheetContext.goalTarget.selectionEnd === WorksheetContext.goalTarget.value.length,
            'goal text selection');
        WorksheetWorkspace.presentation.hideTargetEntry();
        check(hidden(WorksheetContext.targetRow) && WorksheetContext.goalSeek.textContent === 'Goal seek', 'goal entry reset');

        for (let mode = 0; mode < 7; ++mode) {
            labDOM.call('lab_workspace_dom_controls', mode, 0, 1, 1, 1, 1, 0, 0);
            check(!WorksheetContext.run.disabled && !WorksheetContext.back.disabled && !WorksheetContext.forward.disabled, 'ready history controls');
            check(WorksheetContext.goalSeek.disabled === (mode !== 0), 'goal availability follows mode');
            labDOM.call('lab_workspace_dom_controls', mode, 1, 1, 1, 1, 1, 0, 0);
            check(
                [WorksheetContext.run, WorksheetContext.back, WorksheetContext.forward, WorksheetContext.goalSeek, WorksheetContext.precisionSlider].every(button => button.disabled),
                'busy disables every worksheet action');
            check(WorksheetContext.goalSeek.title === '' && WorksheetContext.precisionSlider.title.includes('bits'), 'busy retains the precision description');
        }
        labDOM.call('lab_workspace_dom_controls', 0, 0, 0, 0, 0, 0, 1, 1);
        check(
            WorksheetContext.goalSeek.title.includes('variable binding') && !WorksheetContext.precisionSlider.disabled,
            'precision remains adjustable at its endpoints');

        const enabled = document.createElement('input');
        const disabled = document.createElement('input');
        disabled.disabled = true;
        WorksheetContext.integratorBoundStack.append(enabled, disabled);
        const detachedCopy = document.createElement('button');
        labDOM.call('lab_workspace_dom_busy', 1, null, [detachedCopy], []);
        const busyMutations = new MutationObserver(() => {});
        busyMutations.observe(
            WorksheetContext.integratorBoundStack, {subtree: true, attributes: true, attributeFilter: ['disabled']});
        busyMutations.observe(detachedCopy, {attributes: true, attributeFilter: ['disabled']});
        try {
            labDOM.call('lab_workspace_dom_busy', 1, null, [detachedCopy], []);
            check(busyMutations.takeRecords().length === 0, 'repeated busy projection avoids disabled mutations');
            const inserted = document.createElement('input');
            WorksheetContext.integratorBoundStack.append(inserted);
            labDOM.call('lab_workspace_dom_busy', 1, null, [detachedCopy], []);
            check(inserted.disabled, 'unchanged busy state still disables newly inserted controls');
            inserted.remove();
        } finally {
            busyMutations.disconnect();
        }
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
        WorksheetWorkspace.presentation.setActionRunning(action, true);
        check(
            action.classList.contains('action-running') && action.getAttribute('aria-busy') === 'true',
            'running state');
        WorksheetWorkspace.presentation.setActionRunning(action, false);
        WorksheetWorkspace.presentation.setActionRunning(null as unknown as WorksheetCopyButton, true);
        check(
            !action.classList.contains('action-running') && !action.hasAttribute('aria-busy'), 'running state cleared');

        collapseResultCards();
        native.lab_view_card_toggle(WorksheetContext.resultCardIds.get(WorksheetContext.valueCard)!);
        renderResultCardExpansion();
        WorksheetContext.valueCard.style.display = 'block';
        WorksheetWorkspace.presentation.setValueCardVisible(true);
        check(resultCardExpanded(WorksheetContext.valueCard) && WorksheetContext.valueCard.style.display === '', 'showing value respects expansion');
        WorksheetWorkspace.presentation.setValueCardVisible(false);
        check(native.lab_view_card_expanded() === -1 && hidden(WorksheetContext.valueCard), 'hiding expanded value collapses it');
        const parsedCard = WorksheetContext.parsed.closest<HTMLElement>('.result-card')!;
        native.lab_view_card_toggle(WorksheetContext.resultCardIds.get(parsedCard)!);
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
        const elementState = labDOM.call<LabWorksheetDOMSnapshot>('lab_workspace_dom_snapshot', element, 0);
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
        const buttonState = labDOM.call<LabWorksheetDOMSnapshot>('lab_workspace_dom_snapshot', action, 1);
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

        WorksheetContext.rendered.textContent = 'rendered fixture';
        WorksheetContext.parsed.textContent = 'parsed fixture';
        WorksheetContext.rightPaneTitle.textContent = 'x plot RESULT';
        WorksheetContext.renderedTitle.textContent = 'Plot';
        WorksheetContext.resultUseInput.dataset.inputText = 'saved input';
        const variables = ['μ', 'x'];
        const saved = labDOM.call<LabWorksheetResultSnapshot>('lab_workspace_dom_result_save', {
            lastTex: 'native TeX',
            lastDerivativeExpression: 'native expression',
            currentVariables: variables,
            currentDifferentiable: false
        });
        variables.push('new variable');
        check(
            saved.currentVariables.length === 2 && saved.resultInputText === 'saved input',
            'result snapshot owns metadata copy');
        WorksheetContext.rendered.textContent = 'changed result';
        WorksheetContext.rightPaneTitle.textContent = 'Result';
        WorksheetContext.renderedTitle.textContent = 'Rendered TeX';
        WorksheetContext.parsed.dataset.stale = 'stale';
        const restored = labDOM.call<WorksheetResultValues>('lab_workspace_dom_result_restore', saved);
        check(
            WorksheetContext.rendered.textContent === 'rendered fixture' && WorksheetContext.parsed.textContent === 'parsed fixture',
            'result snapshot restores cards');
        check(WorksheetContext.rightPaneTitle.textContent === 'x plot RESULT' && WorksheetContext.renderedTitle.textContent === 'Plot',
              'mode snapshot restores its own result and card headings');
        labDOM.call('lab_workspace_dom_help', 1);
        labDOM.call<WorksheetResultValues>('lab_workspace_dom_result_restore', saved);
        check(WorksheetContext.rightPaneTitle.textContent === 'Help', 'saved plot heading does not overwrite visible Help');
        labDOM.call('lab_workspace_dom_help', 0);
        labDOM.call<WorksheetResultValues>('lab_workspace_dom_result_restore', saved);
        check(!('stale' in WorksheetContext.parsed.dataset), 'result snapshot removes stale presentation');
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
            WorksheetContext.resultUseInput.dataset.inputText === 'saved input' && !WorksheetContext.resultUseInput.disabled && !hidden(WorksheetContext.resultUseInput),
            'native restore projects reusable input before returning metadata');
        const initialResults = labDOM.call<LabWorksheetResultTable>('lab_workspace_dom_result_states');
        check(
            Object.keys(initialResults).join(',') === WorksheetWorkspace.WORKSPACE_MODE_NAMES.join(',') &&
                Object.values(initialResults).every(state => state === null),
            'native result table initialises exactly the seven absent mode snapshots');
        initialResults.expression = saved;
        check(labDOM.call<LabWorksheetResultTable>('lab_workspace_dom_result_states').expression === null, 'mode result tables are independent');

        for (const differentiable of [undefined, null, 0, '', 'false', true, false]) {
            const legacy = labDOM.call<WorksheetResultValues>('lab_workspace_dom_result_restore', {
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
                WorksheetContext.resultUseInput.dataset.inputText === '' && WorksheetContext.resultUseInput.disabled && hidden(WorksheetContext.resultUseInput),
                'blank restored input is unavailable');
        }
        const longVariables = Array.from({length: 5000}, (_, index) => 'opaque-' + index);
        const largeResult = labDOM.call<LabWorksheetResultSnapshot>('lab_workspace_dom_result_save', {currentVariables: longVariables});
        const largeRestore = labDOM.call<WorksheetResultValues>('lab_workspace_dom_result_restore', largeResult);
        check(
            largeRestore.currentVariables.length === 5000 && largeRestore.currentVariables[4999] === 'opaque-4999' &&
                largeRestore.currentVariables !== longVariables &&
                largeRestore.currentVariables !== largeResult.currentVariables,
            'large metadata copies survive bounded scoped handle release');
        const defaultCapture = labDOM.call<LabWorksheetResultSnapshot>('lab_workspace_dom_result_save', {currentVariables: 'not an array'});
        check(
            defaultCapture.lastTex === '' && defaultCapture.lastDerivativeExpression === '' &&
                defaultCapture.currentDifferentiable === true && defaultCapture.currentVariables.length === 0,
            'capture and restoration share metadata defaults');

        const lifecycle: string[] = [];
        const savedLifecycle = {
            clearFunctionRun: WorksheetRequests.functionCard.clearFunctionRun,
            clearResultPane: WorksheetResults.reset.clearResultPane,
            renderDerivativeButtons: WorksheetWorkspace.presentation.renderDerivativeButtons,
            scheduleRenderedTeXFit: WorksheetResults.renderedFit.schedule,
            scheduleSolverTexFit: WorksheetResults.solverFit.schedule
        };
        try {
            Reflect.set(WorksheetRequests.functionCard, 'clearFunctionRun', () => lifecycle.push('cancel'));
            Reflect.set(WorksheetResults.reset, 'clearResultPane', () => lifecycle.push('clear'));
            Reflect.set(WorksheetWorkspace.presentation, 'renderDerivativeButtons', function refreshActions(names: string[]) {
                lifecycle.push('derivatives');
                check(
                    names === WorksheetBindings.panel.currentVariables && names !== WorksheetWorkspace.presentation.resultStates[WorksheetWorkspace.mode.currentMode()]!.currentVariables &&
                        WorksheetWorkspace.presentation.state.lastTex === 'native TeX' && WorksheetWorkspace.presentation.state.lastDerivativeExpression === 'native expression' &&
                        WorksheetBindings.panel.currentDifferentiable === false && WorksheetWorkspace.presentation.state.resultInputBindings.length === 0,
                    'globals are assigned before derivative refresh');
                check(
                    WorksheetContext.rendered.textContent === 'rendered fixture' && WorksheetContext.resultUseInput.dataset.inputText === 'saved input',
                    'cards and reusable input are restored before derivative refresh');
                savedLifecycle.renderDerivativeButtons(names);
            });
            Reflect.set(WorksheetResults.renderedFit, 'schedule', function scheduleFit() {
                lifecycle.push('rendered-fit');
                check(!WorksheetContext.derivativeButtons.children.length, 'calculus is refreshed before fitting');
            });
            Reflect.set(WorksheetResults.solverFit, 'schedule', () => lifecycle.push('solver-fit'));
            WorksheetWorkspace.presentation.resultStates[WorksheetWorkspace.mode.currentMode()] = saved;
            WorksheetWorkspace.presentation.state.lastTex = 'stale TeX';
            WorksheetBindings.panel.currentDifferentiable = true;
            WorksheetWorkspace.presentation.state.resultInputBindings = [{name: 'stale'}];
            WorksheetWorkspace.presentation.restoreModeResultState();
            check(
                lifecycle.join(',') === 'cancel,derivatives,rendered-fit,solver-fit',
                'restoration cancels function work, refreshes calculus, then schedules both fits');
            WorksheetBindings.panel.currentVariables.push('live-only');
            check(saved.currentVariables.length === 2, 'JS globals never alias the saved variable list');
            WorksheetWorkspace.presentation.saveCurrentModeResultState();
            check(
                WorksheetWorkspace.presentation.resultStates[WorksheetWorkspace.mode.currentMode()]!.currentVariables.length === 3 &&
                    WorksheetWorkspace.presentation.resultStates[WorksheetWorkspace.mode.currentMode()]!.currentVariables !== WorksheetBindings.panel.currentVariables,
                'JS save adapter captures an independent native metadata snapshot');
            lifecycle.length = 0;
            WorksheetWorkspace.presentation.resultStates[WorksheetWorkspace.mode.currentMode()] = null as unknown as WorksheetResultSnapshot;
            WorksheetWorkspace.presentation.restoreModeResultState();
            check(lifecycle.join(',') === 'cancel,clear', 'absent snapshot clears without scheduling either fit');
            check(WorksheetContext.rightPaneTitle.textContent === 'Result', 'absent mode snapshot cannot retain a plot heading');
        } finally {
            for (const [name, callback] of Object.entries(savedLifecycle))
                Reflect.set(labTestModuleOwner(name), name, callback);
        }
        for (const node of [WorksheetContext.rendered, WorksheetContext.parsed, WorksheetContext.functionStyle, WorksheetContext.value]) node.textContent = '\u2003';
        check(!labDOM.call('lab_workspace_dom_has_result'), 'Unicode-whitespace result is empty');
        check(labDOM.call<LabWorksheetResultSnapshot>('lab_workspace_dom_result_save', {}) === null, 'empty result is not saved');
        WorksheetContext.rendered.innerHTML = '<svg></svg>';
        check(!!labDOM.call('lab_workspace_dom_has_result'), 'rendered markup is content even without text');

        let measuredHeight = 120;
        let scrollHeight = 300;
        const editor = document.createElement('textarea');
        Object.defineProperties(editor, {clientHeight: {get: () => 100}, scrollHeight: {get: () => scrollHeight}});
        editor.getClientRects = () => [{}] as unknown as DOMRectList;
        editor.getBoundingClientRect = () => ({height: measuredHeight} as DOMRect);
        editor.style.color = 'red';
        document.body.append(editor);
        WorksheetContext.labTextareas.splice(0, WorksheetContext.labTextareas.length, editor);
        check(!!labDOM.call('lab_workspace_dom_editor_visible', editor), 'visible fixture editor is eligible');
        WorksheetWorkspace.layout.syncEditorResizeGrip();
        check(
            editor.dataset.automaticHeight === '120' && editor.style.height === '120px',
            'overflow acquires resize grip');
        scrollHeight = 100;
        measuredHeight = 155;
        WorksheetWorkspace.layout.syncEditorResizeGrip();
        check(editor.classList.contains('editor-manual-size'), 'user resize survives cleared overflow');
        measuredHeight = 120;
        WorksheetWorkspace.layout.syncEditorResizeGrip();
        check(
            !editor.classList.contains('editor-manual-size') && editor.style.height === '',
            'unneeded resize grip clears');
        scrollHeight = 300;
        WorksheetWorkspace.layout.syncEditorResizeGrip();
        editor.remove();
        check(!labDOM.call('lab_workspace_dom_editor_visible', editor), 'detached editor cannot retain layout');
        WorksheetWorkspace.layout.syncEditorResizeGrip();
        check(
            !editor.hasAttribute('data-automatic-height') && editor.style.maxHeight === '',
            'detached editor sizing reset');
        WorksheetWorkspace.layout.resetEditorManualSize();
        check(editor.style.color === 'red', 'sizing reset preserves unrelated styles');

        const calls: [string, string, HTMLButtonElement | null][] = [];
        Reflect.set(WorksheetRequests.plots, 'takeDerivative', (name: string, button: HTMLButtonElement | null) => calls.push(['derivative', name, button]));
        Reflect.set(WorksheetRequests.plots, 'takeIntegral', (name: string, button: HTMLButtonElement | null) => calls.push(['integral', name, button]));
        Reflect.set(WorksheetRequests.plots, 'takePlot', (name: string, button: HTMLButtonElement | null) => calls.push(['plot', name, button]));
        document.getElementById('plotRange')!.classList.remove('hidden');
        WorksheetBindings.panel.currentDifferentiable = true;
        const names = ['μ', '<literal & name>'];
        WorksheetWorkspace.presentation.renderDerivativeButtons(names);
        const radioNames = [...document.querySelectorAll<HTMLInputElement>('#plotFirstChoices input')];
        check(radioNames.length === 2 && radioNames[0].value === 'μ' && radioNames[1].value === names[1] &&
              !document.getElementById('plotFirstChoices')!.querySelector<HTMLElement>('literal')!,
              'radio variables preserve native Unicode names and escape opaque display text');
        const buttons = [...WorksheetContext.derivativeButtons.querySelectorAll<HTMLButtonElement>('button')];
        check(buttons.length === 4 && !document.getElementById('expressionPlot')!,
              'variables retain calculus actions without a separate Plot button');
        check(
            buttons[0].textContent.endsWith(' derivative') && buttons[2].textContent.endsWith(' integral'),
            'calculus ordering');
        check(
            buttons[1].dataset.variable === names[1] && !buttons[1].querySelector<HTMLElement>('literal')!, 'names stay opaque text');
        for (const button of buttons)
            button.click();
        check(
            calls.length === 4 && calls[0][0] === 'derivative' && calls[0][1] === names[0] &&
                calls[3][0] === 'integral' && calls[3][1] === names[1] && calls[3][2] === buttons[3],
            'browser callbacks forward C action metadata and actual button');
        radioNames[1].click();
        check((document.getElementById('plotFirst') as HTMLInputElement).value === names[1],
              'the first-axis radio retains its selected variable');
        WorksheetWorkspace.presentation.renderDerivativeButtons(['x']);
        WorksheetContext.derivativeButtons.querySelector<HTMLButtonElement>('button')!.click();
        check(calls.length === 5, 'rerender does not duplicate event callbacks');
        buttons[0].querySelector<HTMLElement>('span')!.click();
        check(calls.length === 5, 'rerender removes listeners from detached calculus buttons');

        const nestedNames = ['[ψ]', '["quoted" & <literal>]'];
        WorksheetWorkspace.presentation.renderDerivativeButtons(nestedNames);
        const nestedButtons = [...WorksheetContext.derivativeButtons.querySelectorAll<HTMLButtonElement>('button')];
        Reflect.set(WorksheetRequests.plots, 'takeDerivative', function replacementDerivative(
            this: WorksheetRequests.PlotRequestController, name: string, button: HTMLButtonElement | null) {
            check(this === WorksheetRequests.plots, 'replacement derivative retains its controller receiver');
            calls.push(['replacement derivative', name, button]);
        });
        Reflect.set(WorksheetRequests.plots, 'takeIntegral', function replacementIntegral(
            this: WorksheetRequests.PlotRequestController, name: string, button: HTMLButtonElement | null) {
            check(this === WorksheetRequests.plots, 'replacement integral retains its controller receiver');
            calls.push(['replacement integral', name, button]);
        });
        nestedButtons[1].querySelector<HTMLElement>('span')!.click();
        nestedButtons[2].querySelector<HTMLElement>('span')!.click();
        check(
            calls.length === 7 && calls[5][0] === 'replacement derivative' && calls[5][1] === nestedNames[1] &&
                calls[5][2] === nestedButtons[1] && calls[6][0] === 'replacement integral' &&
                calls[6][1] === nestedNames[0] && calls[6][2] === nestedButtons[2],
            'nested span clicks use the owning button metadata and methods replaced after render');

        let unrelatedClicks = 0;
        const unrelatedListener = () => ++unrelatedClicks;
        const unrelatedButton = document.createElement('button');
        unrelatedButton.type = 'button';
        nestedButtons[1].addEventListener('click', unrelatedListener);
        unrelatedButton.addEventListener('click', unrelatedListener);
        WorksheetContext.derivativeButtons.append(unrelatedButton);
        document.body.append(nestedButtons[1]);
        try {
            WorksheetWorkspace.presentation.dispose();
            WorksheetWorkspace.presentation.dispose();
            nestedButtons[1].querySelector<HTMLElement>('span')!.click();
            nestedButtons[2].querySelector<HTMLElement>('span')!.click();
            unrelatedButton.click();
            check(calls.length === 7 && unrelatedClicks === 2,
                'disposal removes only owned listeners, including on moved buttons, and preserves unrelated listeners');
        } finally {
            nestedButtons[1].removeEventListener('click', unrelatedListener);
            unrelatedButton.removeEventListener('click', unrelatedListener);
            nestedButtons[1].remove();
            unrelatedButton.remove();
        }
        WorksheetBindings.panel.currentDifferentiable = false;
        WorksheetWorkspace.presentation.renderDerivativeButtons(names);
        check(WorksheetContext.derivativeButtons.children.length === 0,
            'non-differentiable expression results have no separate action buttons');
        const manyNames = Array.from({length: 600}, (_, index) => `v${index}`);
        labDOM.call('lab_workspace_dom_derivatives', manyNames, manyNames, 1);
        check(WorksheetContext.derivativeButtons.children.length === 1200, 'large action list contains only calculus actions');
        labDOM.call('lab_workspace_dom_derivatives', [], [], 1);
        check(WorksheetContext.derivativeButtons.children.length === 0, 'no plot action without a variable');
        document.getElementById('plotRange')!.classList.add('hidden');
        labDOM.call('lab_workspace_dom_derivatives', ['x'], ['x'], 1);
        check(WorksheetContext.derivativeButtons.children.length === 2, 'other modes retain only calculus actions');

        const labelCases = [
            ['x', 'x'], ['[μ]', 'μ'], ['[𐐀😀]', '𐐀😀'], ['[e\u0301]', 'e\u0301'],
            ['["quoted" & <literal>]', '"quoted" & <literal>'], ["['apostrophe']", "'apostrophe'"],
            ['', ''], ['[]', ''], ['[[x]]', '[x]'], ['[x', '[x'], ['x]', 'x]'], [' [x] ', ' [x] ']
        ];
        const exactNames = labelCases.map(([name]) => name);
        const nativeLabels = exactNames.map(name => labDOM.call<string>('lab_binding_label', [name]));
        check(nativeLabels.every((label, index) => label === labelCases[index][1]),
            'shared native labels preserve quotes, Unicode, empty names, whitespace and unmatched or inner brackets');
        const plotRange = document.getElementById('plotRange')!;
        const firstChoices = document.getElementById('plotFirstChoices')!;
        const secondChoices = document.getElementById('plotSecondChoices')!;
        const firstAxis = document.getElementById('plotFirst') as HTMLInputElement;
        const secondAxis = document.getElementById('plotSecond') as HTMLInputElement;
        function matchesChoice(radio: HTMLInputElement, index: number): boolean {
            return radio.value === exactNames[index] && radio.nextElementSibling!.textContent === nativeLabels[index];
        }
        function calculusSnapshot(): string {
            return JSON.stringify({
                buttons: WorksheetContext.derivativeButtons.innerHTML,
                plots: plotRange.innerHTML,
                first: firstAxis.value,
                second: secondAxis.value,
                checked: [...plotRange.querySelectorAll<HTMLInputElement>('input[type="radio"]')].map(radio => radio.checked)
            });
        }
        for (const plotsVisible of [false, true]) {
            plotRange.classList.toggle('hidden', !plotsVisible);
            for (const differentiable of [0, 1]) {
                firstAxis.value = exactNames[1];
                secondAxis.value = exactNames[2];
                labDOM.call('lab_workspace_dom_derivatives', exactNames, nativeLabels, differentiable);
                const legacy = calculusSnapshot();
                const previousPlots = plotRange.innerHTML;
                labDOM.call('lab_workspace_dom_calculus', exactNames, differentiable);
                check(calculusSnapshot() === legacy,
                    'native calculus entry matches the existing derivatives API for each plot and differentiability state');
                const controls = [...WorksheetContext.derivativeButtons.querySelectorAll<HTMLButtonElement>('button')];
                check(controls.length === differentiable * exactNames.length * 2,
                    'native calculus entry preserves differentiability and ordered action counts');
                for (let index = 0; index < controls.length; ++index) {
                    const button = controls[index];
                    const variable = index % exactNames.length;
                    check(button.dataset.variable === exactNames[variable] &&
                        button.dataset.calculusAction === String(Math.floor(index / exactNames.length)) &&
                        button.querySelector<HTMLElement>('i')!.textContent === nativeLabels[variable],
                        'native calculus buttons retain exact identifiers, shared labels and action metadata');
                }
                check(!WorksheetContext.derivativeButtons.querySelector('literal'), 'native labels remain text in calculus buttons');
                if (plotsVisible) {
                    const radios = [...firstChoices.querySelectorAll<HTMLInputElement>('input')];
                    check(radios.length === exactNames.length && radios.every(matchesChoice),
                        'visible plot choices use the same opaque names and labels even without calculus actions');
                    check(firstAxis.value === exactNames[1] && secondAxis.value === exactNames[2],
                        'native calculus entry retains valid plot selections');
                    check(!firstChoices.querySelector('literal') && !secondChoices.querySelector('literal'),
                        'quoted plot labels cannot create markup');
                } else {
                    check(plotRange.innerHTML === previousPlots, 'hidden plot controls remain untouched');
                }
            }
            labDOM.call('lab_workspace_dom_calculus', [], 1);
            check(WorksheetContext.derivativeButtons.childElementCount === 0,
                'native calculus entry clears previous actions for an empty name list');
            if (plotsVisible) {
                check(firstChoices.childElementCount === 0 && secondChoices.childElementCount === 1 &&
                    firstAxis.value === '' && secondAxis.value === '',
                    'empty native names clear plot variables and retain the automatic second-axis choice');
            }
        }
        check(JSON.stringify(exactNames) === JSON.stringify(labelCases.map(([name]) => name)),
            'native calculus rendering does not mutate the supplied variable array');

        // Bracketed labels require enough temporary handles to exceed the 4096-handle table without per-name release.
        const boundedNames = Array.from({length: 768}, (_, index) => `[μ${index} "quoted"]`);
        function matchesBoundedButton(button: HTMLButtonElement, index: number): boolean {
            const variable = index % boundedNames.length;
            return button.dataset.variable === boundedNames[variable] &&
                button.querySelector<HTMLElement>('i')!.textContent === `μ${variable} "quoted"`;
        }
        function matchesBoundedChoice(radio: HTMLInputElement, index: number): boolean {
            return radio.value === boundedNames[index] &&
                radio.nextElementSibling!.textContent === `μ${index} "quoted"`;
        }
        labDOM.call('lab_workspace_dom_calculus', boundedNames, 1);
        labDOM.call('lab_binding_label', ['[recycled]']);
        const boundedButtons = [...WorksheetContext.derivativeButtons.querySelectorAll<HTMLButtonElement>('button')];
        check(boundedButtons.length === boundedNames.length * 2 && boundedButtons.every(matchesBoundedButton),
            'large native calculus lists release temporary handles and retain labels after handle reuse');
        const boundedRadios = [...firstChoices.querySelectorAll<HTMLInputElement>('input')];
        check(boundedRadios.length === boundedNames.length && boundedRadios.every(matchesBoundedChoice),
            'large plot lists retain shared native labels after handle reuse');
        labDOM.call('lab_workspace_dom_calculus', ['[final]'], 0);
        check(WorksheetContext.derivativeButtons.childElementCount === 0 &&
            firstChoices.querySelector<HTMLElement>('span')!.textContent === 'final range',
            'later non-differentiable calls clear large action lists and preserve single-variable plot labels');
    } finally {
        WorksheetWorkspace.presentation.dispose();
        Reflect.set(WorksheetRequests.plots, 'takeDerivative', savedDerivative);
        Reflect.set(WorksheetRequests.plots, 'takeIntegral', savedIntegral);
        Reflect.set(WorksheetRequests.plots, 'takePlot', savedPlot);
        WorksheetBindings.panel.currentDifferentiable = savedDifferentiable;
        WorksheetWorkspace.presentation.state.lastTex = savedMetadata.lastTex;
        WorksheetWorkspace.presentation.state.lastDerivativeExpression = savedMetadata.lastDerivativeExpression;
        WorksheetBindings.panel.currentVariables = savedMetadata.currentVariables;
        WorksheetWorkspace.presentation.state.resultInputBindings = savedMetadata.resultInputBindings;
        native.lab_workspace_select(savedMode);
        native.lab_workspace_precision_set(savedMode, savedPrecision);
        WorksheetWorkspace.presentation.resultStates[WorksheetWorkspace.mode.currentMode()] = savedResultState;
        WorksheetContext.labTextareas.splice(0, WorksheetContext.labTextareas.length, ...savedEditors);
        if (WorksheetWorkspace.layout.editorResizeFrame !== savedEditorFrame)
            WorksheetWorkspace.layout.dispose();
        WorksheetWorkspace.layout.editorResizeFrame = savedEditorFrame;
        WorksheetResults.renderedFit.dispose();
        native.lab_view_cards_collapse();
        if (savedExpansion >= 0)
            native.lab_view_card_toggle(savedExpansion);
        for (const {node, attributes, children} of snapshots) {
            for (const attribute of [...node.attributes]) node.removeAttribute(attribute.name);
            for (const {namespace, name, value} of attributes) node.setAttributeNS(namespace, name, value);
            node.replaceChildren(...children);
        }
        for (const {node, value: savedValue, selected} of snapshots) {
            if (savedValue !== undefined)
                (node as LabWorksheetTestSnapshotElement).value = savedValue;
            if (selected !== undefined)
                (node as HTMLOptionElement).selected = selected;
        }
        WorksheetContext.goalTarget.setSelectionRange(
            savedSelection.start, savedSelection.end, savedSelection.direction ?? undefined);
        (savedFocus as HTMLElement | null)?.focus();
    }
};
