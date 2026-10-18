/**
 * @file widget_event_checks.ts
 * @brief Date-picker and tooltip event-policy regressions.
 *
 * Checks deferred service arguments, scoped subscriptions and repeated catalogue access without repeating application initialisation.
 */

/** @brief Browser references and array snapshots inspected by the event fixture. */
interface BindingReferenceFixture {
    [name: string]: unknown;
    labTextareas: HTMLTextAreaElement[];
    resultCards: Element[];
}

/** @brief Keyboard and pointer fields accepted by the synthetic event helper. */
interface BindingWidgetEventOptions extends MouseEventInit, KeyboardEventInit {}

/** Picker and tooltip event policy, deferred service arguments and scoped subscription regressions. */
window.checkLabWidgetEvents = function checkLabWidgetEvents() {
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('Widget events: ' + message);
    }

    function same(actual: unknown, expected: unknown): boolean {
        if (Object.is(actual, expected))
            return true;
        if (!actual || !expected || typeof actual !== 'object' || typeof expected !== 'object' ||
            actual instanceof Node || expected instanceof Node)
            return false;
        const keys = Object.keys(expected);
        if (Object.keys(actual).length !== keys.length)
            return false;
        for (const key of keys) {
            if (!same(Reflect.get(actual, key), Reflect.get(expected, key)))
                return false;
        }
        return true;
    }
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
        const refs = labDOM.call<BindingReferenceFixture>('lab_workspace_dom_references');
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
        const fresh = labDOM.call<BindingReferenceFixture>('lab_workspace_dom_references');
        check(fresh !== refs && Object.keys(arrays).every(name => fresh[name] !== refs[name]),
            'each catalogue call owns independent records and arrays');
        check(refs.labTextareas.length === length && refs.resultCards.length === cardCount &&
            fresh.labTextareas.length === length + 1 && fresh.labTextareas.at(-1) === referenceFixture &&
            fresh.resultCards.at(-1) === referenceFixture, 'old arrays are snapshots while new calls see DOM changes');
        referenceFixture.remove();
        check(fresh.labTextareas.at(-1) === referenceFixture, 'snapshot retains the actual detached node');
        fresh.labTextareas.length = 0;
        labDOM.call<BindingTestPlan>('lab_widget_events_dispatch', 255, {target: referenceFixture}, {});
        const again = labDOM.call<BindingReferenceFixture>('lab_workspace_dom_references');
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
    function call(service: string, ...args: unknown[]): MarsTransportServiceCall {
        return {service, args};
    }
    const services = [
        'openMarsDatePicker', 'closeMarsDatePicker', 'commitMarsDateValue', 'shiftMarsDatePickerMonth',
        'shiftMarsDatePickerYear', 'setMarsDatePickerMonthYear', 'commitMarsTodayValue', 'placeMarsDatePicker',
        'showButtonTooltip', 'hideButtonTooltip', 'markDatetimeOffsetTouched'
    ];
    const input = WorksheetContext.datetimeDate, otherInput = WorksheetContext.almanacDate;
    const opener = document.querySelector('[data-date-target="datetimeDate"]');
    const shell = input.closest('.mars-date-shell');
    const state = {input, button: opener, shell, year: 2031, month: 2};
    const replacement = {input: otherInput, button: null, shell: otherInput.closest('.mars-date-shell'), year: 2042, month: 7};
    function plan(action: number, event: object = {}, context: BindingWidgetContext = state): BindingTestPlan {
        return labDOM.call<BindingTestPlan>('lab_widget_events_dispatch', action, event, context);
    }

    function expect(
        action: number, event: object = {}, calls: MarsTransportServiceCall[] = [],
        context: BindingWidgetContext = state, prevent = false
    ): BindingTestPlan {
        const result = plan(action, event, context);
        check(same(result.calls, calls), 'ordered service arguments for action ' + action);
        check(!!result.prevent === prevent && !result.stop, 'cancellation policy for action ' + action);
        return result;
    }
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
    const savedPickerClass = WorksheetContext.marsDatePicker.getAttribute('class');
    const savedYear = WorksheetContext.marsDatePickerYear.value, savedMonth = WorksheetContext.marsDatePickerMonth.value;
    const savedMonthChildren = [...WorksheetContext.marsDatePickerMonth.childNodes];
    const savedOffsetTouched = WorksheetContext.worksheetSession.calendar.datetimeGmtOffsetTouched;
    const savedContext = {
        input: WorksheetContext.marsDatePickerState.input, button: WorksheetContext.marsDatePickerState.button, shell: WorksheetContext.marsDatePickerState.shell
    };
    const records: MarsTransportServiceCall[] = [];
    const reentryErrors: unknown[] = [];
    const spies: {[service: string]: MarsTransportService} = {};
    for (const service of services) {
        spies[service] = function recordService(...args: unknown[]) {
            records.push(call(service, ...args));
            try {
                check(plan(255).calls.length === 0, 'service re-enters after handle scope closes');
            } catch (error) {
                reentryErrors.push(error);
            }
        };
    }

    function send(
        target: EventTarget | null, type: string, expected: MarsTransportServiceCall[] = [],
        options: BindingWidgetEventOptions = {}, eventTarget: EventTarget | null = null
    ): boolean {
        let event: KeyboardEvent | MouseEvent;
        if (type === 'keydown')
            event = new KeyboardEvent(type, {cancelable: true, ...options});
        else
            event = new MouseEvent(type, {cancelable: true, ...options});
        if (eventTarget)
            Object.defineProperty(event, 'target', {value: eventTarget});
        records.length = 0;
        if (!target)
            throw new Error('Missing widget event fixture target');
        const accepted = target.dispatchEvent(event);
        check(same(records, expected), 'live ' + type + ' delivers exact service arguments once');
        return accepted;
    }
    // Keep grid and fixture bubbling local; document subscriptions are exercised on separate targets below.
    function stop(event: Event): void {
        event.stopPropagation();
    }
    fixture.addEventListener('click', stop);
    WorksheetContext.marsDatePickerGrid.addEventListener('click', stop);
    const savedRegistry = labDOM.bindEvents(Object.freeze(spies));
    try {
        check(savedRegistry && services.every(service => typeof savedRegistry[service] === 'function'),
            'bootstrap binds every widget service');
        document.body.append(fixture);
        WorksheetContext.marsDatePickerGrid.append(day);
        WorksheetContext.marsDatePickerMonth.append(new Option('Fixture February', '2'));
        WorksheetContext.marsDatePickerMonth.value = '2';
        WorksheetContext.marsDatePickerYear.value = '2033';

        WorksheetContext.marsDatePicker.classList.add('hidden');
        expect(0, {currentTarget: fixtureOpener}, [call('openMarsDatePicker', input, fixtureOpener)]);
        for (const targetId of ['', ' ', 'missing-widget-event-input']) {
            const invalid = document.createElement('button');
            invalid.dataset.dateTarget = targetId;
            expect(0, {currentTarget: invalid});
        }
        WorksheetContext.marsDatePicker.classList.remove('hidden');
        expect(0, {currentTarget: opener}, [focusClose]);
        expect(0, {currentTarget: opener}, [call('openMarsDatePicker', input, opener)], replacement);
        const retained = expect(1, {target: day.firstChild}, [call('commitMarsDateValue', input, '2031-02-03'), focusClose]);
        plan(255, {target: otherInput}, replacement);
        check(retained.calls[0].args[0] === input, 'returned DOM values outlive their original handle slots');
        day.disabled = true;
        expect(1, {target: day.firstChild});
        day.disabled = false;
        expect(1, {target: foreignDay.firstChild});
        expect(1, {target: WorksheetContext.marsDatePickerGrid});
        expect(1, {target: day.firstChild}, [call('commitMarsDateValue', null, '2031-02-03'), focusClose],
            {...state, input: null});
        expect(2, {}, [call('shiftMarsDatePickerMonth', -1)]);
        expect(3, {}, [call('shiftMarsDatePickerMonth', 1)]);
        for (const [modifiers, step] of [[{}, 1], [{shiftKey: true}, 10], [{ctrlKey: true}, 100],
            [{ctrlKey: true, shiftKey: true}, 100], [{metaKey: true}, 1]] as const) {
            expect(4, modifiers, [call('shiftMarsDatePickerYear', -step)]);
            expect(5, modifiers, [call('shiftMarsDatePickerYear', step)]);
        }
        expect(6, {}, [call('setMarsDatePickerMonthYear', 2031, '2', {commit: true})]);
        expect(7, {}, [call('setMarsDatePickerMonthYear', '2033', 2, {commit: true})]);
        expect(8, {key: 'Enter'}, [call('setMarsDatePickerMonthYear', '2033', 2, {commit: true})], state, true);
        for (const key of ['Escape', 'Tab', 'ArrowDown'])
            expect(8, {key});
        expect(9, {}, [call('commitMarsTodayValue', input), focusClose]);
        expect(9, {}, [], {...state, input: null});
        expect(10, {}, [focusClose]);
        expect(11, {target: outside}, [outsideClose]);
        expect(11, {target: day.firstChild});
        expect(11, {target: opener});
        expect(12, {}, [call('placeMarsDatePicker', shell)]);
        WorksheetContext.marsDatePicker.classList.add('hidden');
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
        const testDocument = new EventTarget();
        const testWindow = new EventTarget();
        const add = EventTarget.prototype.addEventListener;
        const subscriptions: {target: EventTarget; type: string; options?: boolean | AddEventListenerOptions}[] = [];
        EventTarget.prototype.addEventListener = function recordSubscription(type, listener, options) {
            subscriptions.push({target: this, type, options});
            return add.call(this, type, listener, options);
        };
        try {
            labDOM.call('lab_widget_events_install', testDocument, testWindow, WorksheetContext.marsDatePickerState);
            const count = subscriptions.length;
            check(count > 0, 'new targets acquire subscriptions');
            labDOM.call('lab_widget_events_install', testDocument, testWindow, WorksheetContext.marsDatePickerState);
            labDOM.call('lab_widget_events_install', testDocument, testWindow, WorksheetContext.marsDatePickerState);
            check(subscriptions.length === count, 'reinstallation does not duplicate the same context');
        } finally {
            EventTarget.prototype.addEventListener = add;
        }
        const scroll = subscriptions.find(entry => entry.target === testWindow && entry.type === 'scroll');
        check(scroll && typeof scroll.options === 'object' && scroll.options.passive === true, 'tooltip scroll listener remains passive');
        Object.assign(WorksheetContext.marsDatePickerState, {input: otherInput, button: null, shell: replacement.shell});
        WorksheetContext.marsDatePicker.classList.remove('hidden');
        send(WorksheetContext.marsDatePickerToday, 'click', [call('commitMarsTodayValue', otherInput), focusClose]);
        WorksheetContext.marsDatePickerState.input = input;
        send(WorksheetContext.marsDatePickerToday, 'click', [call('commitMarsTodayValue', input), focusClose]);
        WorksheetContext.marsDatePickerState.input = null;
        send(WorksheetContext.marsDatePickerToday, 'click');
        WorksheetContext.marsDatePickerState.input = otherInput;
        send(opener, 'click', [call('openMarsDatePicker', input, opener)]);
        send(fixtureOpener.firstChild, 'click', [call('openMarsDatePicker', input, fixtureOpener)], {bubbles: true});
        send(day.firstChild, 'click', [call('commitMarsDateValue', otherInput, '2031-02-03'), focusClose], {bubbles: true});
        day.disabled = true;
        send(WorksheetContext.marsDatePickerGrid, 'click', [], {}, day);
        day.disabled = false;
        send(document.getElementById('marsDatePickerPrev'), 'click', [call('shiftMarsDatePickerMonth', -1)]);
        send(document.getElementById('marsDatePickerNext'), 'click', [call('shiftMarsDatePickerMonth', 1)]);
        send(document.getElementById('marsDatePickerYearDown'), 'click', [call('shiftMarsDatePickerYear', -100)],
            {ctrlKey: true, shiftKey: true});
        send(document.getElementById('marsDatePickerYearUp'), 'click', [call('shiftMarsDatePickerYear', 10)], {shiftKey: true});
        send(WorksheetContext.marsDatePickerMonth, 'change', [call('setMarsDatePickerMonthYear', WorksheetContext.marsDatePickerState.year, '2', {commit: true})]);
        const yearCommit = call('setMarsDatePickerMonthYear', '2033', WorksheetContext.marsDatePickerState.month, {commit: true});
        send(WorksheetContext.marsDatePickerYear, 'change', [yearCommit]);
        check(!send(WorksheetContext.marsDatePickerYear, 'keydown', [yearCommit],
            {key: 'Enter'}), 'year Enter prevents the real default');
        check(send(WorksheetContext.marsDatePickerYear, 'keydown', [], {key: 'Tab'}), 'other year keys keep browser behaviour');
        send(WorksheetContext.marsDatePickerClose, 'click', [focusClose]);
        send(WorksheetContext.datetimeGmtOffset, 'input', [call('markDatetimeOffsetTouched')]);
        send(testDocument, 'click', [hide, outsideClose], {}, outside);
        send(testDocument, 'click', [hide], {}, day);
        send(testDocument, 'click', [hide], {}, opener);
        // Resize preserves the original tooltip-hide then picker-placement order.
        send(testWindow, 'resize', [hide, call('placeMarsDatePicker', replacement.shell)]);
        WorksheetContext.marsDatePicker.classList.add('hidden');
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
        WorksheetContext.marsDatePickerGrid.removeEventListener('click', stop);
        WorksheetContext.marsDatePicker.setAttribute('class', savedPickerClass!);
        WorksheetContext.marsDatePickerMonth.replaceChildren(...savedMonthChildren);
        WorksheetContext.marsDatePickerMonth.value = savedMonth;
        WorksheetContext.marsDatePickerYear.value = savedYear;
        WorksheetContext.worksheetSession.calendar.datetimeGmtOffsetTouched = savedOffsetTouched;
        Object.assign(WorksheetContext.marsDatePickerState, savedContext);
        try {
            // Restore retained listener contexts as well as the service registry, including on assertion failure.
            labDOM.call('lab_widget_events_install', document, window, WorksheetContext.marsDatePickerState);
        } finally {
            labDOM.bindEvents(savedRegistry);
        }
    }
    return true;
};
