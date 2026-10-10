/** Native event-plan and live subscription regressions; browser services are isolated and restored. */
window.checkLabEvents = function checkLabEvents() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Events: ' + message);
    };
    const native = labWire.exports();
    const plan = (action, event = {}) => labDOM.call('lab_events_dispatch', action, event);
    const names = result => result.calls.map(call => call.service).join(',');
    const expect = (action, event, services, args = []) => {
        const result = plan(action, event);
        check(names(result) === services, 'service order for action ' + action);
        if (services && !services.includes(',')) {
            check(result.calls[0].args.length === args.length, 'argument count for action ' + action);
            args.forEach((arg, index) => check(Object.is(result.calls[0].args[index], arg), 'argument identity'));
        }
        return result;
    };
    const services = [
        'clearForwardHistory',
        'clearGoalSeekRequest',
        'hideTargetEntry',
        'evaluateCurrentMode',
        'navigateHistoryFromEvent',
        'evaluateFromKeyboard',
        'selectWorksheetMode',
        'normaliseWorksheetControl',
        'startGoalSeekFromEvent',
        'changeWorksheetPrecision',
        'toggleTextDigits',
        'copyResultFromEvent',
        'flushWorksheetState',
        'scheduleWorkspacePanelFit',
        'openMarsDatePicker',
        'closeMarsDatePicker',
        'applyCalendarControlEvent',
        'refreshWorksheetFromInput',
        'clearWorksheetFromEvent',
        'toggleHelp',
        'formatAlmanacTimeFromEvent',
        'toggleRenderedDigits',
        'runFunctionCard',
        'sendResultExpressionToInput',
        'copyInputFromEvent',
        'scheduleEditorResizeGrip'
    ];
    const calls = [], reentryErrors = [];
    const registry = Object.fromEntries(
        services.map(service => [service, (...args) => {
                         calls.push({service, args});
                         // Every host service must run after the native handle scope has been released.
                         try {
                             check(plan(255).calls.length === 0, 'service can re-enter the DOM bridge');
                         } catch (error) {
                             reentryErrors.push(error);
                         }
                     }]));
    const savedMode = native.lab_workspace_mode();
    const savedExpansion = native.lab_view_card_expanded();
    const savedZoom = resultCards.map(card => {
        const id = resultCardIds.get(card);
        return [id, native.lab_view_card_zoom(id)];
    });
    const savedRequestFrame = window.requestAnimationFrame, savedCancelFrame = window.cancelAnimationFrame;
    const savedFrames = [editorResizeFrame, renderedTeXFitFrame, solverFitFrame];
    const snapshots = [document.body, ...document.body.querySelectorAll('*')].map(
        node => ({
            node,
            attributes:
                [...node.attributes].map(attribute => [attribute.namespaceURI, attribute.name, attribute.value]),
            children: [...node.childNodes],
            value: node.matches('input, select, textarea') ? node.value : undefined,
            selected: node.tagName === 'OPTION' ? node.selected : undefined
        }));
    const fixture = document.createElement('div');
    fixture.innerHTML = '<textarea></textarea><button class="mode-tab" data-mode="matrix"><span>tab</span></button>';
    // Exercise nested tab bubbling without invoking unrelated document picker/tooltip handlers.
    fixture.addEventListener('click', event => event.stopPropagation());
    const send = (target, event, expected) => {
        check(!!target, 'registered target exists');
        calls.length = 0;
        const accepted = target.dispatchEvent(event);
        check(names({calls}) === expected, 'live ' + event.type + ' routing: ' + names({calls}));
        return accepted;
    };
    const click = () => new MouseEvent('click', {cancelable: true});
    const key = options => new KeyboardEvent('keydown', {cancelable: true, ...options});
    const savedRegistry = labDOM.bindEvents(Object.freeze(registry));
    try {
        window.requestAnimationFrame = () => 0;
        window.cancelAnimationFrame = () => {};
        const refreshEditor = document.createElement('textarea');
        refreshEditor.value = '  opaque μ {x = ?}  ';
        refreshEditor.dataset.bindingRefreshValid = 'pending';
        refreshEditor.dataset.evaluationReady = 'keep';
        const refreshPlan = (mode, bound, unchanged) => labDOM.call(
            'lab_events_refresh', mode, Number(bound), Number(unchanged),
            {editor: refreshEditor, defaultDatetimeText: 'calendar default'});
        const expectedRefresh = [
            'saveState,clearGoalSeekRequest,setSource,scheduleEditedExpressionBindingRefresh',
            'refreshVariableValuesFromEditor,saveState,updateHistoryButtons',
            'setSource,setSource,updateHistoryButtons', 'refreshVariableValuesFromEditor,updateHistoryButtons',
            'refreshVariableValuesFromEditor,updateHistoryButtons', 'setModeEditor,updateHistoryButtons',
            'updateHistoryButtons'
        ];
        for (let mode = 0; mode < 7; ++mode) {
            for (const bound of [false, true]) {
                for (const unchanged of [false, true]) {
                    const refresh = refreshPlan(mode, bound, unchanged);
                    let expected = expectedRefresh[mode];
                    if (!mode && unchanged)
                        expected = 'saveState,cancelBindingRefresh,markBindingRefresh,updateHistoryButtons';
                    if (!bound && [1, 3, 4].includes(mode))
                        expected = 'setSource,setSource,' + expected;
                    check(names(refresh) === expected, 'native refresh service order for mode ' + mode);
                    const save = refresh.calls.find(call => call.service === 'saveState');
                    if (save) {
                        check(save.args[0] === WORKSPACE_MODE_NAMES[mode], 'save uses the exact mode token');
                        check(save.args[1].options.debounce === true, 'native save policy requests debounce');
                        check(
                            mode ? !Object.hasOwn(save.args[1], 'text') : save.args[1].text === 'opaque μ {x = ?}',
                            'expression saves trimmed text; equation preserves its live default');
                    }
                    const source = refresh.calls.filter(call => call.service === 'setSource');
                    if (source.length === 2)
                        check(
                            source[0].args[0] === 'fullText' && source[1].args[0] === 'displayText' &&
                                source.every(call => call.args[1] === 'opaque μ {x = ?}'),
                            'native source writes remain ordered and opaque');
                    check(
                        refreshEditor.dataset.bindingRefreshValid === 'pending',
                        'plan construction cannot mark reuse before cancelling its timer');
                }
            }
        }
        check(names(refreshPlan(99, false, false)) === '', 'invalid refresh mode has no service effects');
        refreshEditor.value = ' \u2003 ';
        check(refreshPlan(5, false, false).calls[0].args[1] === 'calendar default', 'blank Datetime editor fallback');
        refreshEditor.value = ' authored calendar ';
        check(refreshPlan(5, false, false).calls[0].args[1] === 'authored calendar', 'authored calendar text survives');
        const reused = refreshPlan(0, false, true);
        refreshPlan(6, true, false);
        check(reused.calls[2].args[0] === refreshEditor, 'deferred projection retains a node, not a scoped handle');
        labDOM.call('lab_events_refresh_mark', refreshEditor);
        check(
            refreshEditor.dataset.bindingRefreshValid === 'true' && refreshEditor.dataset.evaluationReady === 'keep',
            'native reuse projection changes only binding validity');

        const refreshSaved = {
            cancel: labRequests.cancel,
            clearTimeout: window.clearTimeout,
            bindingParts,
            refreshVariableValuesFromEditor,
            saveWorksheetState,
            clearGoalSeekRequest,
            scheduleEditedExpressionBindingRefresh,
            updateHistoryButtons,
            source: Object.fromEntries(Object.keys(labEditorState).map(key => [key, labEditorState[key]])),
            editors: Object.fromEntries(WORKSPACE_MODE_NAMES.map(mode => [mode, modeEditorText[mode]]))
        };
        const refreshCalls = [];
        const sourceDescriptor = Object.getOwnPropertyDescriptor(labEditorState, 'fullText');
        const modeDescriptor = Object.getOwnPropertyDescriptor(modeEditorText, 'datetime');
        let refreshBound = false;
        try {
            labRequests.cancel = operation => refreshCalls.push('cancel:' + operation);
            bindingParts = () => {
                check(
                    refreshCalls.join(',') === 'cancel:evaluate,cancel:bindings',
                    'requests cancel before flag discovery');
                return refreshBound ? {} : null;
            };
            refreshVariableValuesFromEditor = () => {
                refreshCalls.push('bindings');
                check(
                    labEditorState.fullText === (refreshBound ? 'previous full' : 'opaque input'),
                    'source setter precedes binding refresh without redefining its view');
            };
            saveWorksheetState = (mode, text, options) => {
                refreshCalls.push('save');
                check(
                    options.debounce === true && text === (mode === 'expression' ? 'opaque input' : undefined),
                    'host adapter preserves omitted save text and native debounce policy');
                check(expr.dataset.bindingRefreshValid === 'pending', 'save precedes reuse projection');
            };
            clearGoalSeekRequest = () => {
                refreshCalls.push('goal');
                check(labEditorState.lastInput === 'previous input', 'goal reset precedes last-input reset');
            };
            scheduleEditedExpressionBindingRefresh = () => {
                refreshCalls.push('schedule');
                check(labEditorState.lastInput === '', 'last input resets before the binding timer is scheduled');
            };
            window.clearTimeout = timer => {
                refreshCalls.push('timer');
                check(
                    timer === expressionBindingRefreshTimer && expr.dataset.bindingRefreshValid === 'pending',
                    'pending refresh timer is cancelled before native reuse projection');
            };
            updateHistoryButtons = () => {
                refreshCalls.push('history');
                check(
                    labDOM.call('lab_events_refresh', 99, 0, 0, {}).calls.length === 0,
                    'refresh services can re-enter a released native scope');
                if (currentMode() === 'expression')
                    check(expr.dataset.bindingRefreshValid === 'true', 'reuse is projected before history refresh');
            };
            const hostOrder = [
                'save,goal,schedule', 'bindings,save,history', 'history', 'bindings,history', 'bindings,history',
                'history', 'history'
            ];
            for (let mode = 0; mode < 7; ++mode) {
                for (const bound of [false, true]) {
                    for (const unchanged of mode === 0 ? [false, true] : [false]) {
                        native.lab_workspace_select(mode);
                        refreshBound = bound;
                        expr.value = '  opaque input  ';
                        expr.dataset.bindingRefreshValid = 'pending';
                        labEditorState.fullText = 'previous full';
                        labEditorState.displayText = unchanged ? 'opaque input' : 'previous display';
                        labEditorState.lastInput = 'previous input';
                        refreshCalls.length = 0;
                        refreshWorksheetFromInput();
                        const expected = unchanged ? 'save,timer,history' : hostOrder[mode];
                        check(
                            refreshCalls.join(',') === 'cancel:evaluate,cancel:bindings,' + expected,
                            'real refresh adapter preserves service order for mode ' + mode);
                        if (mode === 2 || (!bound && [1, 3, 4].includes(mode)))
                            check(
                                labEditorState.fullText === 'opaque input' &&
                                    labEditorState.displayText === 'opaque input',
                                'real source view setters receive native text');
                        if (mode === 5)
                            check(
                                modeEditorText.datetime === 'opaque input',
                                'real calendar view setter receives native text');
                    }
                }
            }
            check(
                Object.getOwnPropertyDescriptor(labEditorState, 'fullText').get === sourceDescriptor.get &&
                    Object.getOwnPropertyDescriptor(labEditorState, 'fullText').set === sourceDescriptor.set &&
                    Object.getOwnPropertyDescriptor(modeEditorText, 'datetime').get === modeDescriptor.get,
                'native plans leave source and mode view descriptors intact');
        } finally {
            labRequests.cancel = refreshSaved.cancel;
            window.clearTimeout = refreshSaved.clearTimeout;
            ({
                bindingParts,
                refreshVariableValuesFromEditor,
                saveWorksheetState,
                clearGoalSeekRequest,
                scheduleEditedExpressionBindingRefresh,
                updateHistoryButtons
            } = refreshSaved);
            for (const [key, text] of Object.entries(refreshSaved.source)) labEditorState[key] = text;
            for (const [mode, text] of Object.entries(refreshSaved.editors)) modeEditorText[mode] = text;
            native.lab_workspace_select(savedMode);
        }
        check(
            savedRegistry && services.every(service => typeof savedRegistry[service] === 'function'),
            'bootstrap supplies every native service');
        native.lab_workspace_select(0);
        const runServices = 'clearForwardHistory,clearGoalSeekRequest,hideTargetEntry,evaluateCurrentMode';
        expect(0, {}, runServices);
        // Check bootstrap registration before explicitly installing anything in this suite.
        send(document.getElementById('run'), click(), runServices);
        for (let mode = 1; mode < 7; ++mode) {
            native.lab_workspace_select(mode);
            expect(0, {}, 'evaluateCurrentMode');
            const result = expect(3, {key: 'Enter', metaKey: true}, 'evaluateCurrentMode');
            check(result.prevent && !result.stop, 'non-expression shortcut cancellation');
        }
        native.lab_workspace_select(0);
        for (const modifier of ['ctrlKey', 'metaKey']) {
            const result = expect(3, {key: 'Enter', [modifier]: true}, 'evaluateFromKeyboard');
            check(result.prevent && !result.stop, 'expression shortcut cancellation');
            check(
                !send(expr, key({key: 'Enter', [modifier]: true}), 'evaluateFromKeyboard'),
                'real editor shortcut prevents the default');
        }
        for (const event of [{key: 'Enter'}, {key: 'Escape', ctrlKey: true}, {key: 'x', metaKey: true}]) {
            check(!expect(3, event, '').prevent, 'unrelated key retains browser behaviour');
            check(send(expr, key(event), ''), 'unrelated real key is not cancelled');
        }
        expect(1, {}, 'navigateHistoryFromEvent', [0]);
        expect(2, {}, 'navigateHistoryFromEvent', [1]);
        for (let action = 9; action <= 11; ++action) expect(action, {}, 'normaliseWorksheetControl', [action - 9]);
        // Equation variable is intentionally not a DOM control; its native action remains valid.
        expect(10, {currentTarget: null}, 'normaliseWorksheetControl', [1]);
        expect(14, {}, 'changeWorksheetPrecision', [1]);
        expect(15, {}, 'changeWorksheetPrecision', [-1]);
        const button = document.getElementById('parsedMore');
        for (const [action, target] of [[17, parsed], [18, functionStyle], [20, value]])
            expect(action, {currentTarget: button}, 'toggleTextDigits', [target, button]);
        const retained = expect(23, {currentTarget: button}, 'copyResultFromEvent', [button]);
        plan(255, {currentTarget: expr});
        check(retained.calls[0].args[0] === button, 'returned arguments are nodes, not recycled handle numbers');
        check(expect(13, {key: 'Enter'}, 'startGoalSeekFromEvent').prevent, 'goal Enter is cancelled');
        check(!expect(13, {key: 'Tab'}, '').prevent, 'goal Tab is untouched');
        for (const action of [32, 255, 256, (4 << 8), (5 << 8) | 255, (6 << 8) | 255, (7 << 8)])
            check(!expect(action, {}, '').prevent, 'unknown action or calendar field is harmless');

        const routes = [
            ['back', 'click', 1, 'navigateHistoryFromEvent', [0]],
            ['forward', 'click', 2, 'navigateHistoryFromEvent', [1]],
            ['clear', 'click', 5, 'clearWorksheetFromEvent', []], ['help', 'click', 6, 'toggleHelp', []],
            ['goalSeek', 'click', 7, 'startGoalSeekFromEvent', []],
            ['matrixOperation', 'change', 9, 'normaliseWorksheetControl', [0]],
            ['integratorIntervalCap', 'change', 11, 'normaliseWorksheetControl', [2]],
            ['almanacTime', 'input', 12, 'formatAlmanacTimeFromEvent', []],
            ['morePrecision', 'click', 14, 'changeWorksheetPrecision', [1]],
            ['lessPrecision', 'click', 15, 'changeWorksheetPrecision', [-1]],
            ['renderedMore', 'click', 16, 'toggleRenderedDigits', []],
            ['functionRun', 'click', 19, 'runFunctionCard', []],
            ['resultUseInput', 'click', 21, 'sendResultExpressionToInput', []],
            ['inputCopy', 'click', 22, 'copyInputFromEvent', []]
        ];
        for (const [id, type, action, service, args] of routes) {
            const target = document.getElementById(id);
            expect(action, {currentTarget: target}, service, args);
            send(target, new Event(type, {cancelable: true}), service);
            check(
                calls[0].args.length === args.length && args.every((arg, index) => calls[0].args[index] === arg),
                'live service arguments for ' + id);
        }
        send(expr, new Event('input'), 'refreshWorksheetFromInput,scheduleEditorResizeGrip');
        for (const [id, target] of [['parsedMore', parsed], ['functionMore', functionStyle], ['valueMore', value]]) {
            const control = document.getElementById(id);
            send(control, click(), 'toggleTextDigits');
            check(calls[0].args[0] === target && calls[0].args[1] === control, 'real digit-button node identity');
        }
        const copyButton = document.querySelector('[data-copy-target]');
        send(copyButton, click(), 'copyResultFromEvent');
        check(calls[0].args[0] === copyButton, 'copy button identity survives native return');

        const calendarFlags = {
            datetime: {
                date: 19,
                jdn: 0,
                start: 1,
                end: 1,
                year: 0,
                jurisdiction: 48,
                town: 4,
                latitude: 8,
                longitude: 8,
                elevation: 8,
                gmt_offset: 0
            },
            almanac: {
                date: 17,
                time: 0,
                zone: 0,
                jurisdiction: 48,
                town: 4,
                latitude: 8,
                longitude: 8,
                elevation: 8,
                visibility: 0
            }
        };
        for (const [mode, flags] of Object.entries(calendarFlags)) {
            for (const field of calendarSchema(mode)) {
                const target = field.element ? document.getElementById(field.element) : null;
                check(Object.hasOwn(flags, field.key), 'calendar field has independent expected policy');
                const action = (field.id << 8) | field.field;
                if (target)
                    expect(
                        action, {type: 'change', currentTarget: target}, 'applyCalendarControlEvent',
                        [mode, flags[field.key]]);
                if (!(flags[field.key] & 1)) {
                    expect(action, {type: 'keydown', key: 'Enter', currentTarget: target}, '');
                    continue;
                }
                const pickerButton = target.closest('.mars-date-shell').querySelector('[data-date-target]');
                for (const name of ['ArrowDown', 'Enter']) {
                    const result = expect(
                        action, {type: 'keydown', key: name, currentTarget: target}, 'openMarsDatePicker',
                        [target, pickerButton]);
                    check(result.prevent && !result.stop, 'calendar open prevents default only');
                    check(!send(target, key({key: name}), 'openMarsDatePicker'), 'real date shortcut cancellation');
                    check(calls[0].args[0] === target && calls[0].args[1] === pickerButton, 'real picker arguments');
                }
                check(
                    !expect(action, {type: 'keydown', key: 'Escape', currentTarget: target}, 'closeMarsDatePicker')
                         .prevent,
                    'calendar Escape preserves default');
                expect(action, {type: 'keydown', key: 'Enter', currentTarget: document.createElement('input')}, '');
            }
        }
        const dateField = calendarSchema('datetime').find(field => field.key === 'date');
        const dateInput = document.getElementById(dateField.element);
        dateInput.value = '2031-02-03';
        send(dateInput, new Event('change'), 'applyCalendarControlEvent');
        check(datetimeYear.value === '2031' && datetimeJdn.value === '', 'date change projects year and clears JDN');
        dateInput.value = '';
        datetimeYear.value = '1984';
        datetimeJdn.value = '2451545';
        send(dateInput, new Event('change'), 'applyCalendarControlEvent');
        check(datetimeYear.value === '1984' && datetimeJdn.value === '', 'empty date preserves year but clears JDN');

        // Observe actual listener options, including new targets and repeated installation.
        document.body.append(fixture);
        const testDocument = new EventTarget(), testWindow = new EventTarget();
        const subscriptions = [];
        const add = EventTarget.prototype.addEventListener;
        EventTarget.prototype.addEventListener = function(type, listener, options) {
            subscriptions.push({target: this, type, options});
            return add.call(this, type, listener, options);
        };
        try {
            labDOM.call('lab_events_install', testDocument, testWindow);
            const count = subscriptions.length;
            check(count > 0, 'new targets receive subscriptions');
            labDOM.call('lab_events_install', testDocument, testWindow);
            check(subscriptions.length === count, 'installation deduplicates target/type/action');
        } finally {
            EventTarget.prototype.addEventListener = add;
        }
        const scroll = subscriptions.find(entry => entry.target === testWindow && entry.type === 'scroll');
        check(scroll?.options.passive === true, 'scroll listener is passive');
        check(
            subscriptions.filter(entry => entry !== scroll).every(entry => entry.options.passive === false),
            'cancellable listeners explicitly remain non-passive');
        send(fixture.querySelector('textarea'), new Event('input'), 'scheduleEditorResizeGrip');
        send(fixture.querySelector('span'), new MouseEvent('click', {bubbles: true}), 'selectWorksheetMode');
        check(calls[0].args[0] === 'matrix', 'tab uses currentTarget rather than nested clicked element');
        send(testWindow, new Event('pagehide'), 'flushWorksheetState');
        send(testWindow, new Event('resize'), 'scheduleWorkspacePanelFit');
        send(testWindow, new Event('scroll'), 'scheduleWorkspacePanelFit');
        testDocument.visibilityState = 'visible';
        send(testDocument, new Event('visibilitychange'), '');
        testDocument.visibilityState = 'hidden';
        send(testDocument, new Event('visibilitychange'), 'flushWorksheetState');
        send(document.getElementById('run'), click(), runServices);

        const card = resultCards.find(
            candidate => candidate.querySelector('[data-expand-card]') && candidate.querySelector('[data-zoom-step]') &&
                candidate.querySelector('[data-zoom-reset]'));
        check(!!card, 'a registered card has expansion and zoom controls');
        const cardId = resultCardIds.get(card), initialZoom = native.lab_view_zoom_index(NaN);
        native.lab_view_card_set_zoom(cardId, initialZoom);
        check(send(card, new WheelEvent('wheel', {deltaY: -1, cancelable: true}), ''), 'plain wheel is not captured');
        check(native.lab_view_card_zoom(cardId) === initialZoom, 'plain wheel does not zoom');
        check(
            !send(card, new WheelEvent('wheel', {deltaY: -1, ctrlKey: true, cancelable: true}), ''),
            'modified wheel is cancellable despite listener options');
        check(native.lab_view_card_zoom(cardId) === initialZoom + 1, 'modified wheel zooms upwards');
        check(
            !send(card, new WheelEvent('wheel', {deltaY: 1, metaKey: true, cancelable: true}), ''),
            'meta wheel is cancelled');
        check(native.lab_view_card_zoom(cardId) === initialZoom, 'positive wheel zooms downwards');
        const zoom = card.querySelector('[data-zoom-step]'), reset = card.querySelector('[data-zoom-reset]');
        check(zoom && reset, 'zoom controls are present');
        let bubbled = false;
        const observe = () => {
            bubbled = true;
        };
        card.addEventListener('click', observe);
        try {
            check(!send(zoom, new MouseEvent('click', {bubbles: true, cancelable: true}), ''), 'zoom cancels click');
            check(!bubbled, 'zoom prevents ancestor click propagation');
        } finally {
            card.removeEventListener('click', observe);
        }
        send(reset, click(), '');
        check(native.lab_view_card_zoom(cardId) === initialZoom, 'reset restores native default zoom');
        native.lab_view_cards_collapse();
        const expand = card.querySelector('[data-expand-card]');
        send(expand, click(), '');
        check(
            native.lab_view_card_expanded() === cardId && expand.getAttribute('aria-expanded') === 'true',
            'expand subscription projects native state');
        let addedWhileExpanded = 0;
        EventTarget.prototype.addEventListener = function(type, listener, options) {
            ++addedWhileExpanded;
            return add.call(this, type, listener, options);
        };
        try {
            labDOM.call('lab_events_install', testDocument, testWindow);
            labDOM.call('lab_events_install', testDocument, testWindow);
        } finally {
            EventTarget.prototype.addEventListener = add;
        }
        check(addedWhileExpanded === 0, 'expanded-card reinstallation adds no duplicate listeners');
        check(
            native.lab_view_card_expanded() === cardId && expand.getAttribute('aria-expanded') === 'true' &&
                card.classList.contains('expanded-card') && labWorkspace.classList.contains('result-card-expanded') &&
                resultPane.classList.contains('card-expanded') && expand.textContent === 'Collapse',
            'reinstallation preserves native expansion and projects its accessibility and layout state');
        send(document.getElementById('run'), click(), runServices);
        send(testWindow, new Event('pagehide'), 'flushWorksheetState');
        send(expand, click(), '');
        check(
            native.lab_view_card_expanded() === -1 && expand.getAttribute('aria-expanded') === 'false',
            'second expand click collapses once');
        check(reentryErrors.length === 0, 'host services run outside the synchronous WASM scope: ' + reentryErrors);
    } finally {
        fixture.remove();
        native.lab_workspace_select(savedMode);
        for (const [id, zoom] of savedZoom) native.lab_view_card_set_zoom(id, zoom);
        native.lab_view_cards_collapse();
        if (savedExpansion >= 0)
            native.lab_view_card_toggle(savedExpansion);
        for (const {node, attributes, children} of snapshots) {
            for (const attribute of [...node.attributes]) node.removeAttribute(attribute.name);
            for (const [namespace, name, text] of attributes) node.setAttributeNS(namespace, name, text);
            node.replaceChildren(...children);
        }
        for (const {node, value: savedValue, selected} of snapshots) {
            if (savedValue !== undefined)
                node.value = savedValue;
            if (selected !== undefined)
                node.selected = selected;
        }
        [editorResizeFrame, renderedTeXFitFrame, solverFitFrame] = savedFrames;
        window.requestAnimationFrame = savedRequestFrame;
        window.cancelAnimationFrame = savedCancelFrame;
        labDOM.bindEvents(savedRegistry);
    }
    return true;
};
