/** Native almanac event plans and live subscriptions; exact server markup remains opaque. */
window.checkLabAlmanacEvents = function checkLabAlmanacEvents() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Almanac events: ' + message);
    };
    const names = plan => plan.calls.map(call => call.service).join(',');
    const dispatch = (action, button, context = null) => labDOM.call(
        'lab_almanac_events_dispatch', action, {currentTarget: button, target: button?.firstChild}, context);
    const target = document.createElement('div');
    const controls = '<button data-almanac-visibility="all"><span>All</span></button>' +
        '<button data-almanac-visibility="visible"><span>Visible</span></button>' +
        '<button data-almanac-visibility="invalid">Invalid</button>' +
        '<button data-almanac-use-totality="" data-date="2031-02-03" data-latitude="1.25"><span>Use</span></button>';
    const data = {
        visibility: 'all',
        marker: {opaque: 'unchanged'},
        almanac_presentation: {
            all: {html: controls + '<p data-native="all">Exact native all</p>', copy_text: 'Native ALL\nα'},
            visible:
                {html: controls + '<p data-native="visible">Exact native visible</p>', copy_text: 'Native VISIBLE\nβ'}
        }
    };
    const backing = {visibility: 'all', worksheet: data};
    const state = Object.freeze({
        get visibility() {
            return backing.visibility;
        },
        get worksheet() {
            return backing.worksheet;
        }
    });
    const context = {target, state};
    const button = document.createElement('button');
    button.dataset.almanacVisibility = 'visible';
    const savedVisibility = almanacVisibilityMode, savedWorksheet = almanacLastWorksheetData;
    const services = [
        'applyAlmanacTotalityAction', 'setAlmanacVisibility', 'saveLastAlmanacState', 'renderAlmanacWorksheet',
        'refreshAlmanacLandTotality', 'setStatus', 'evaluateAlmanac'
    ];
    const calls = [], reentryErrors = [];
    let project = false;
    const spies = Object.fromEntries(services.map(
        service => [service, (...args) => {
            if (service === 'setAlmanacVisibility')
                setAlmanacVisibility(...args);
            calls.push({service, args, visibility: almanacVisibilityMode});
            try {
                check(dispatch(99, null).calls.length === 0, 'service can re-enter after the native scope ends');
            } catch (error) {
                reentryErrors.push(error);
            }
            if (project && service === 'renderAlmanacWorksheet')
                renderAlmanacWorksheet(...args);
        }]));
    const savedRegistry = labDOM.bindEvents(Object.freeze(spies));
    const send = (node, expected, bubbles = false) => {
        calls.length = 0;
        const event = new MouseEvent('click', {bubbles, cancelable: true});
        check(node.dispatchEvent(event) && !event.defaultPrevented, 'almanac actions preserve browser defaults');
        check(names({calls}) === expected, 'live ordered services: ' + names({calls}));
    };
    try {
        check(
            savedRegistry && services.every(service => typeof savedRegistry[service] === 'function'),
            'bootstrap provides every deferred almanac service');
        let plan = dispatch(1, button, context);
        check(
            names(plan) ===
                'setAlmanacVisibility,saveLastAlmanacState,renderAlmanacWorksheet,refreshAlmanacLandTotality,setStatus',
            'cached visibility change commits, saves, renders, refreshes, then reports Ready');
        check(
            !plan.prevent && !plan.stop && state.visibility === 'all' && plan.calls[0].args[0] === 'visible',
            'native policy defers assignment and leaves getter-only state unchanged');
        check(
            plan.calls[2].args[0] === target && plan.calls[2].args[1] !== data &&
                plan.calls[2].args[1].visibility === 'visible' && plan.calls[2].args[1].marker === data.marker &&
                plan.calls[2].args[1].almanac_presentation === data.almanac_presentation && data.visibility === 'all',
            'rerender receives a shallow response copy without changing native metadata');
        check(
            plan.calls[3].args[0] === data && plan.calls[4].args[0] === 'Ready',
            'totality refresh receives the original last response');
        dispatch(99, document.createElement('div'));
        check(
            plan.calls[2].args[0] === target && plan.calls[3].args[0] === data,
            'returned references survive scoped-handle recycling');
        backing.visibility = 'visible';
        for (const text of ['visible', ' VISIBLE ', '\tvisible\n', 'invalid', '']) {
            button.dataset.almanacVisibility = text;
            check(dispatch(1, button, context).calls.length === 0, 'unchanged or invalid visibility is inert');
        }
        delete button.dataset.almanacVisibility;
        check(dispatch(1, button, context).calls.length === 0, 'missing visibility retains the current selection');
        for (const worksheet of [null, false, undefined]) {
            backing.worksheet = worksheet;
            backing.visibility = 'all';
            button.dataset.almanacVisibility = ' VISIBLE ';
            plan = dispatch(1, button, context);
            check(
                names(plan) === 'setAlmanacVisibility,saveLastAlmanacState,evaluateAlmanac' &&
                    state.visibility === 'all' && plan.calls[0].args[0] === 'visible',
                'missing cache requests evaluation after saving normalised visibility');
            check(
                plan.calls[2].args.length === 1 && plan.calls[2].args[0].skipHistoryUpdate === true,
                'fallback evaluation does not add history');
        }
        check(
            dispatch(1, button, null).calls.length === 0 && dispatch(1, null, context).calls.length === 0 &&
                dispatch(99, button, context).calls.length === 0,
            'missing context/target and unknown actions are inert');
        plan = dispatch(0, button);
        check(
            names(plan) === 'applyAlmanacTotalityAction' && plan.calls[0].args[0] === button,
            'totality forwards its registered button unchanged');
        check(dispatch(0, null).calls.length === 0, 'missing totality target is inert');

        // Native rendering validates first, preserves exact HTML/copy text and uses the visibility fallback.
        backing.visibility = 'all';
        backing.worksheet = data;
        check(
            labDOM.call('lab_almanac_events_render', target, data, state) === 'all',
            'native render returns resolved visibility');
        check(
            target.innerHTML === data.almanac_presentation.all.html &&
                target.dataset.copyText === data.almanac_presentation.all.copy_text,
            'native all markup and copy stay exact');
        for (const invalid
                 of [null, {}, {visibility: 'visible'},
                     {visibility: 'visible', almanac_presentation: {visible: {html: ' \n\t '}}},
                     {visibility: 'visible', almanac_presentation: {visible: {html: 42}}}]) {
            const html = target.innerHTML, copy = target.dataset.copyText;
            check(
                labDOM.call('lab_almanac_events_render', target, invalid, state) === null,
                'malformed native variant rejected');
            check(
                target.innerHTML === html && target.dataset.copyText === copy && state.visibility === 'all',
                'invalid response cannot clear prior markup, copy text or visibility');
        }
        check(
            labDOM.call('lab_almanac_events_render', target, {...data, visibility: ' VISIBLE '}, state) === 'visible' &&
                state.visibility === 'all' && target.innerHTML === data.almanac_presentation.visible.html,
            'render returns normalised visibility without overwriting the getter');
        backing.visibility = 'visible';
        check(
            labDOM.call('lab_almanac_events_render', target, {...data, visibility: 'bad'}, state) === 'visible' &&
                target.dataset.copyText === data.almanac_presentation.visible.copy_text,
            'invalid visibility uses live fallback');
        check(
            labDOM.call('lab_almanac_events_render', null, data, state) === null, 'missing render target is rejected');
        check(
            Object.isFrozen(state) && typeof Object.getOwnPropertyDescriptor(state, 'visibility').get === 'function',
            'native dispatch and rendering preserve non-configurable live accessors');

        // Reinstalling on the same root updates its live state without adding a second context/listener.
        const replacement = Object.freeze({
            get visibility() {
                return almanacVisibilityMode;
            },
            get worksheet() {
                return null;
            }
        });
        const add = EventTarget.prototype.addEventListener;
        let added = 0;
        EventTarget.prototype.addEventListener = function(type, listener, options) {
            ++added;
            return add.call(this, type, listener, options);
        };
        try {
            labDOM.call('lab_almanac_events_install', target, replacement);
            labDOM.call('lab_almanac_events_install', target, replacement);
            bindAlmanacTotalityActions(target);
            check(added === 0, 'repeat installation and async totality rebinding do not duplicate listeners');
        } finally {
            EventTarget.prototype.addEventListener = add;
        }
        backing.visibility = 'all';
        almanacVisibilityMode = 'all';
        send(
            target.querySelector('[data-almanac-visibility="visible"]'),
            'setAlmanacVisibility,saveLastAlmanacState,evaluateAlmanac');
        check(
            replacement.visibility === 'visible' && state.visibility === 'all', 'subscription reads replacement state');
        almanacVisibilityMode = 'all';
        send(
            target.querySelector('[data-almanac-visibility="visible"]'),
            'setAlmanacVisibility,saveLastAlmanacState,evaluateAlmanac');
        check(replacement.visibility === 'visible', 'context is read live after subscription');
        const totality = target.querySelector('[data-almanac-use-totality]');
        send(totality.firstChild, 'applyAlmanacTotalityAction', true);
        check(calls[0].args[0] === totality, 'nested click forwards currentTarget, not the child');
        check(
            totality.dataset.date === '2031-02-03' && totality.dataset.latitude === '1.25',
            'totality payload remains opaque and unchanged');
        const later = totality.cloneNode(true);
        target.append(later);
        bindAlmanacTotalityActions(target);
        bindAlmanacTotalityActions(target);
        send(later, 'applyAlmanacTotalityAction');
        check(calls[0].args[0] === later, 'asynchronously inserted totality action is bound once');

        // Exercise the unchanged public wrappers, global state access and repeated markup replacement.
        almanacVisibilityMode = 'all';
        almanacLastWorksheetData = data;
        project = true;
        renderAlmanacWorksheet(target, data);
        send(
            target.querySelector('[data-almanac-visibility="visible"]'),
            'setAlmanacVisibility,saveLastAlmanacState,renderAlmanacWorksheet,refreshAlmanacLandTotality,setStatus');
        check(
            calls.every(call => call.visibility === 'visible') &&
                target.innerHTML === data.almanac_presentation.visible.html &&
                target.dataset.copyText === data.almanac_presentation.visible.copy_text,
            'global visibility is committed before services and native markup is rerendered');
        send(target.querySelector('[data-almanac-visibility="visible"]'), '');
        send(target.querySelector('[data-almanac-visibility="invalid"]'), '');
        send(
            target.querySelector('[data-almanac-visibility="all"]'),
            'setAlmanacVisibility,saveLastAlmanacState,renderAlmanacWorksheet,refreshAlmanacLandTotality,setStatus');
        check(
            target.innerHTML === data.almanac_presentation.all.html && almanacLastWorksheetData === data,
            'new buttons work after rerender without replacing the stored response');
        almanacLastWorksheetData = null;
        send(
            target.querySelector('[data-almanac-visibility="visible"]'),
            'setAlmanacVisibility,saveLastAlmanacState,evaluateAlmanac');
        const previous = target.innerHTML;
        let rejected = false;
        try {
            renderAlmanacWorksheet(target, {visibility: 'all'});
        } catch (error) {
            rejected =
                error.message.includes('Almanac presentation') && error.message.includes('make mars-lab-restart');
        }
        check(
            rejected && target.innerHTML === previous && almanacVisibilityMode === 'visible',
            'public wrapper preserves the startup-mismatch diagnostic and prior output');

        // Default-root binding still supports totality fragments delivered outside the worksheet.
        const fragment = document.createElement('div');
        fragment.append(totality.cloneNode(true));
        document.body.append(fragment);
        try {
            bindAlmanacTotalityActions();
            bindAlmanacTotalityActions();
            send(fragment.firstChild, 'applyAlmanacTotalityAction');
        } finally {
            fragment.remove();
        }
        check(reentryErrors.length === 0, 'all host services execute outside the scoped native call: ' + reentryErrors);
    } finally {
        almanacVisibilityMode = savedVisibility;
        almanacLastWorksheetData = savedWorksheet;
        labDOM.bindEvents(savedRegistry);
        target.remove();
    }
    return true;
};
