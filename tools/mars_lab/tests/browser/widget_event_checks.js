/** Picker and tooltip event policy, deferred service arguments and scoped subscription regressions. */
window.checkLabWidgetEvents = function checkLabWidgetEvents() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Widget events: ' + message);
    };
    const same = (actual, expected) => {
        if (Object.is(actual, expected))
            return true;
        if (!actual || !expected || typeof actual !== 'object' || typeof expected !== 'object' ||
            actual instanceof Node || expected instanceof Node)
            return false;
        const keys = Object.keys(expected);
        return Object.keys(actual).length === keys.length && keys.every(key => same(actual[key], expected[key]));
    };
    // Re-reading the reference catalogue must never repeat workspace/card initialisation.
    const native = labWire.exports();
    const savedMode = native.lab_workspace_mode(), savedPrecision = native.lab_workspace_precision(6);
    const savedZoom = native.lab_view_card_zoom(0), savedExpansion = native.lab_view_card_expanded();
    const referenceFixture = document.createElement('textarea');
    referenceFixture.className = 'result-card';
    try {
        native.lab_workspace_select(6);
        native.lab_workspace_precision_set(6, 333);
        native.lab_view_card_set_zoom(0, 1);
        native.lab_view_cards_collapse();
        native.lab_view_card_toggle(0);
        const refs = labDOM.call('lab_workspace_dom_references');
        for (const [name, id] of [
            ['expr', 'expr'], ['statusEl', 'status'], ['run', 'run'], ['datetimeDate', 'datetimeDate'],
            ['marsDatePicker', 'marsDatePicker'], ['matrixOperation', 'matrixOperation'], ['value', 'value']]) {
            check(refs[name] instanceof Element && refs[name] === document.getElementById(id),
                'catalogue returns the actual control for ' + name);
        }
        const arrays = {
            labTextareas: 'textarea', modeTabs: '.mode-tab', helpCards: '#helpPane .help-card',
            copyButtons: '.copy-result', moreDigitButtons: '.more-digits', resultCards: '.result-card'
        };
        for (const [name, selector] of Object.entries(arrays)) {
            check(Array.isArray(refs[name]) && same(refs[name], [...document.querySelectorAll(selector)]),
                'catalogue returns ordered array snapshots for ' + name);
        }
        const length = refs.labTextareas.length, cardCount = refs.resultCards.length;
        document.body.append(referenceFixture);
        const fresh = labDOM.call('lab_workspace_dom_references');
        check(fresh !== refs && Object.keys(arrays).every(name => fresh[name] !== refs[name]),
            'each catalogue call owns independent records and arrays');
        check(refs.labTextareas.length === length && refs.resultCards.length === cardCount &&
            fresh.labTextareas.length === length + 1 && fresh.labTextareas.at(-1) === referenceFixture &&
            fresh.resultCards.at(-1) === referenceFixture, 'old arrays are snapshots while new calls see DOM changes');
        referenceFixture.remove();
        check(fresh.labTextareas.at(-1) === referenceFixture, 'snapshot retains the actual detached node');
        fresh.labTextareas.length = 0;
        labDOM.call('lab_widget_events_dispatch', 255, {target: referenceFixture}, {});
        const again = labDOM.call('lab_workspace_dom_references');
        check(again.expr === refs.expr && again.statusEl === refs.statusEl &&
            same(again.labTextareas, refs.labTextareas) && same(again.resultCards, refs.resultCards),
            'array mutation and recycled handles cannot alias another result');
        check(native.lab_workspace_mode() === 6 && native.lab_workspace_precision(6) === 333 &&
            native.lab_view_card_zoom(0) === 1 && native.lab_view_card_expanded() === 0 &&
            native.lab_view_card_zoom(cardCount) === -1,
            'reference discovery preserves workspace state and does not reset or register cards');
    } finally {
        referenceFixture.remove();
        native.lab_workspace_select(savedMode);
        native.lab_workspace_precision_set(6, savedPrecision);
        native.lab_view_card_set_zoom(0, savedZoom);
        native.lab_view_cards_collapse();
        if (savedExpansion >= 0)
            native.lab_view_card_toggle(savedExpansion);
    }
    const call = (service, ...args) => ({service, args});
    const services = [
        'openMarsDatePicker', 'closeMarsDatePicker', 'commitMarsDateValue', 'shiftMarsDatePickerMonth',
        'shiftMarsDatePickerYear', 'setMarsDatePickerMonthYear', 'commitMarsTodayValue', 'placeMarsDatePicker',
        'showButtonTooltip', 'hideButtonTooltip', 'markDatetimeOffsetTouched'
    ];
    const input = datetimeDate, otherInput = almanacDate;
    const opener = document.querySelector('[data-date-target="datetimeDate"]');
    const shell = input.closest('.mars-date-shell');
    const state = {input, button: opener, shell, year: 2031, month: 2};
    const replacement = {input: otherInput, button: null, shell: otherInput.closest('.mars-date-shell'), year: 2042, month: 7};
    const plan = (action, event = {}, context = state) =>
        labDOM.call('lab_widget_events_dispatch', action, event, context);
    const expect = (action, event, calls = [], context = state, prevent = false) => {
        const result = plan(action, event, context);
        check(same(result.calls, calls), 'ordered service arguments for action ' + action);
        check(!!result.prevent === prevent && !result.stop, 'cancellation policy for action ' + action);
        return result;
    };
    const focusClose = call('closeMarsDatePicker', {restoreFocus: true});
    const outsideClose = call('closeMarsDatePicker', {restoreFocus: false});
    const hide = call('hideButtonTooltip');
    const fixture = document.createElement('div');
    fixture.innerHTML = '<button data-date-target="  datetimeDate  "><span>open</span></button>' +
        '<button><span>tooltip</span><i>inside</i></button><span>outside</span>';
    const fixtureOpener = fixture.children[0], tooltipButton = fixture.children[1], outside = fixture.children[2];
    const day = document.createElement('button');
    day.dataset.isoDate = '2031-02-03';
    day.innerHTML = '<span>3</span>';
    const foreignDay = day.cloneNode(true);
    const savedPickerClass = marsDatePicker.getAttribute('class');
    const savedYear = marsDatePickerYear.value, savedMonth = marsDatePickerMonth.value;
    const savedMonthChildren = [...marsDatePickerMonth.childNodes];
    const savedOffsetTouched = datetimeGmtOffsetTouched;
    const savedContext = {
        input: marsDatePickerState.input, button: marsDatePickerState.button, shell: marsDatePickerState.shell
    };
    const records = [], reentryErrors = [];
    const spies = Object.fromEntries(services.map(service => [service, (...args) => {
        records.push(call(service, ...args));
        try {
            check(plan(255).calls.length === 0, 'service re-enters after handle scope closes');
        } catch (error) {
            reentryErrors.push(error);
        }
    }]));
    const send = (target, type, expected = [], options = {}, eventTarget = null) => {
        const event = type === 'keydown' ? new KeyboardEvent(type, {cancelable: true, ...options}) :
            new MouseEvent(type, {cancelable: true, ...options});
        if (eventTarget)
            Object.defineProperty(event, 'target', {value: eventTarget});
        records.length = 0;
        const accepted = target.dispatchEvent(event);
        check(same(records, expected), 'live ' + type + ' delivers exact service arguments once');
        return accepted;
    };
    // Keep grid and fixture bubbling local; document subscriptions are exercised on separate targets below.
    const stop = event => event.stopPropagation();
    fixture.addEventListener('click', stop);
    marsDatePickerGrid.addEventListener('click', stop);
    const savedRegistry = labDOM.bindEvents(Object.freeze(spies));
    try {
        check(savedRegistry && services.every(service => typeof savedRegistry[service] === 'function'),
            'bootstrap binds every widget service');
        document.body.append(fixture);
        marsDatePickerGrid.append(day);
        marsDatePickerMonth.append(new Option('Fixture February', '2'));
        marsDatePickerMonth.value = '2';
        marsDatePickerYear.value = '2033';

        marsDatePicker.classList.add('hidden');
        expect(0, {currentTarget: fixtureOpener}, [call('openMarsDatePicker', input, fixtureOpener)]);
        for (const targetId of ['', ' ', 'missing-widget-event-input']) {
            const invalid = document.createElement('button');
            invalid.dataset.dateTarget = targetId;
            expect(0, {currentTarget: invalid});
        }
        marsDatePicker.classList.remove('hidden');
        expect(0, {currentTarget: opener}, [focusClose]);
        expect(0, {currentTarget: opener}, [call('openMarsDatePicker', input, opener)], replacement);
        const retained = expect(1, {target: day.firstChild}, [call('commitMarsDateValue', input, '2031-02-03'), focusClose]);
        plan(255, {target: otherInput}, replacement);
        check(retained.calls[0].args[0] === input, 'returned DOM values outlive their original handle slots');
        day.disabled = true;
        expect(1, {target: day.firstChild});
        day.disabled = false;
        expect(1, {target: foreignDay.firstChild});
        expect(1, {target: marsDatePickerGrid});
        expect(1, {target: day.firstChild}, [call('commitMarsDateValue', null, '2031-02-03'), focusClose],
            {...state, input: null});
        expect(2, {}, [call('shiftMarsDatePickerMonth', -1)]);
        expect(3, {}, [call('shiftMarsDatePickerMonth', 1)]);
        for (const [modifiers, step] of [[{}, 1], [{shiftKey: true}, 10], [{ctrlKey: true}, 100],
            [{ctrlKey: true, shiftKey: true}, 100], [{metaKey: true}, 1]]) {
            expect(4, modifiers, [call('shiftMarsDatePickerYear', -step)]);
            expect(5, modifiers, [call('shiftMarsDatePickerYear', step)]);
        }
        expect(6, {}, [call('setMarsDatePickerMonthYear', 2031, '2', {commit: true})]);
        expect(7, {}, [call('setMarsDatePickerMonthYear', '2033', 2, {commit: true})]);
        expect(8, {key: 'Enter'}, [call('setMarsDatePickerMonthYear', '2033', 2, {commit: true})], state, true);
        for (const key of ['Escape', 'Tab', 'ArrowDown']) expect(8, {key});
        expect(9, {}, [call('commitMarsTodayValue', input), focusClose]);
        expect(9, {}, [], {...state, input: null});
        expect(10, {}, [focusClose]);
        expect(11, {target: outside}, [outsideClose]);
        expect(11, {target: day.firstChild});
        expect(11, {target: opener});
        expect(12, {}, [call('placeMarsDatePicker', shell)]);
        marsDatePicker.classList.add('hidden');
        expect(11, {target: outside});
        expect(12);
        for (const action of [13, 14]) {
            const expected = action === 13 ? call('showButtonTooltip', tooltipButton) : hide;
            expect(action, {target: tooltipButton.firstChild, relatedTarget: null}, [expected]);
            expect(action, {target: tooltipButton.firstChild, relatedTarget: outside}, [expected]);
            expect(action, {target: tooltipButton.firstChild, relatedTarget: tooltipButton.lastChild});
            expect(action, {target: tooltipButton.firstChild, relatedTarget: tooltipButton});
            expect(action, {target: outside});
        }
        expect(15, {target: tooltipButton.firstChild}, [call('showButtonTooltip', tooltipButton)]);
        expect(15, {target: outside}, [call('showButtonTooltip', null)]);
        expect(15, {target: document.createTextNode('text')}, [call('showButtonTooltip', null)]);
        expect(16, {}, [hide]);
        expect(17, {key: 'Escape'}, [hide]);
        expect(17, {key: 'Enter'});
        expect(18, {}, [call('markDatetimeOffsetTouched')]);
        expect(19);
        expect(255);

        // Install on isolated document/window targets while exercising the real page controls.
        const testDocument = new EventTarget(), testWindow = new EventTarget();
        const add = EventTarget.prototype.addEventListener, subscriptions = [];
        EventTarget.prototype.addEventListener = function(type, listener, options) {
            subscriptions.push({target: this, type, options});
            return add.call(this, type, listener, options);
        };
        try {
            labDOM.call('lab_widget_events_install', testDocument, testWindow, marsDatePickerState);
            const count = subscriptions.length;
            check(count > 0, 'new targets acquire subscriptions');
            labDOM.call('lab_widget_events_install', testDocument, testWindow, marsDatePickerState);
            labDOM.call('lab_widget_events_install', testDocument, testWindow, marsDatePickerState);
            check(subscriptions.length === count, 'reinstallation does not duplicate the same context');
        } finally {
            EventTarget.prototype.addEventListener = add;
        }
        const scroll = subscriptions.find(entry => entry.target === testWindow && entry.type === 'scroll');
        check(scroll && scroll.options.passive === true, 'tooltip scroll listener remains passive');
        Object.assign(marsDatePickerState, {input: otherInput, button: null, shell: replacement.shell});
        marsDatePicker.classList.remove('hidden');
        send(marsDatePickerToday, 'click', [call('commitMarsTodayValue', otherInput), focusClose]);
        marsDatePickerState.input = input;
        send(marsDatePickerToday, 'click', [call('commitMarsTodayValue', input), focusClose]);
        marsDatePickerState.input = null;
        send(marsDatePickerToday, 'click');
        marsDatePickerState.input = otherInput;
        send(opener, 'click', [call('openMarsDatePicker', input, opener)]);
        send(fixtureOpener.firstChild, 'click', [call('openMarsDatePicker', input, fixtureOpener)], {bubbles: true});
        send(day.firstChild, 'click', [call('commitMarsDateValue', otherInput, '2031-02-03'), focusClose], {bubbles: true});
        day.disabled = true;
        send(marsDatePickerGrid, 'click', [], {}, day);
        day.disabled = false;
        send(document.getElementById('marsDatePickerPrev'), 'click', [call('shiftMarsDatePickerMonth', -1)]);
        send(document.getElementById('marsDatePickerNext'), 'click', [call('shiftMarsDatePickerMonth', 1)]);
        send(document.getElementById('marsDatePickerYearDown'), 'click', [call('shiftMarsDatePickerYear', -100)],
            {ctrlKey: true, shiftKey: true});
        send(document.getElementById('marsDatePickerYearUp'), 'click', [call('shiftMarsDatePickerYear', 10)], {shiftKey: true});
        send(marsDatePickerMonth, 'change', [call('setMarsDatePickerMonthYear', marsDatePickerState.year, '2', {commit: true})]);
        const yearCommit = call('setMarsDatePickerMonthYear', '2033', marsDatePickerState.month, {commit: true});
        send(marsDatePickerYear, 'change', [yearCommit]);
        check(!send(marsDatePickerYear, 'keydown', [yearCommit],
            {key: 'Enter'}), 'year Enter prevents the real default');
        check(send(marsDatePickerYear, 'keydown', [], {key: 'Tab'}), 'other year keys keep browser behaviour');
        send(marsDatePickerClose, 'click', [focusClose]);
        send(datetimeGmtOffset, 'input', [call('markDatetimeOffsetTouched')]);
        send(testDocument, 'click', [hide, outsideClose], {}, outside);
        send(testDocument, 'click', [hide], {}, day);
        send(testDocument, 'click', [hide], {}, opener);
        // Resize preserves the original tooltip-hide then picker-placement order.
        send(testWindow, 'resize', [hide, call('placeMarsDatePicker', replacement.shell)]);
        marsDatePicker.classList.add('hidden');
        send(testWindow, 'resize', [hide]);
        send(testWindow, 'scroll', [hide]);
        send(testDocument, 'click', [hide], {}, outside);
        send(testDocument, 'pointerover', [call('showButtonTooltip', tooltipButton)], {relatedTarget: outside},
            tooltipButton.firstChild);
        send(testDocument, 'pointerover', [], {relatedTarget: tooltipButton.lastChild}, tooltipButton.firstChild);
        send(testDocument, 'pointerout', [hide], {relatedTarget: outside}, tooltipButton.firstChild);
        send(testDocument, 'pointerout', [], {relatedTarget: tooltipButton.lastChild}, tooltipButton.firstChild);
        send(testDocument, 'focusin', [call('showButtonTooltip', tooltipButton)], {}, tooltipButton.firstChild);
        send(testDocument, 'focusin', [call('showButtonTooltip', null)], {}, outside);
        send(testDocument, 'focusout', [hide]);
        send(testDocument, 'keydown', [hide], {key: 'Escape'});
        send(testDocument, 'keydown', [], {key: 'Enter'});
        check(reentryErrors.length === 0, 'deferred services never run within a native scope: ' + reentryErrors);
    } finally {
        fixture.remove();
        day.remove();
        marsDatePickerGrid.removeEventListener('click', stop);
        marsDatePicker.setAttribute('class', savedPickerClass);
        marsDatePickerMonth.replaceChildren(...savedMonthChildren);
        marsDatePickerMonth.value = savedMonth;
        marsDatePickerYear.value = savedYear;
        datetimeGmtOffsetTouched = savedOffsetTouched;
        Object.assign(marsDatePickerState, savedContext);
        try {
            // Restore retained listener contexts as well as the service registry, including on assertion failure.
            labDOM.call('lab_widget_events_install', document, window, marsDatePickerState);
        } finally {
            labDOM.bindEvents(savedRegistry);
        }
    }
    return true;
};
