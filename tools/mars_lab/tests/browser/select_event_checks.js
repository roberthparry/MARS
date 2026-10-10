/** Native select subscriptions and browser-event dispatch checks; the parent runs these sequentially. */
window.checkLabSelectEvents = function checkLabSelectEvents() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error(`Select events: ${message}`);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const focus = document.activeElement;
    const root = document.createElement('div');
    root.style.cssText = 'position:fixed;left:0;top:0;z-index:10000;width:400px';
    document.body.appendChild(root);
    const fixtures = [];
    const create = (id, searchable) => {
        const label = document.createElement('label'), select = document.createElement('select');
        select.id = id;
        label.htmlFor = id;
        label.id = id + 'Label';
        label.textContent = 'Native places';
        for (const [name, value, disabled] of [
                 ['Alpha', 'a', false], ['Disabled', 'disabled', true], ['Málaga', 'm', false], ['東京', 'j', false]]) {
            const option = new Option(name, value);
            option.disabled = disabled;
            option.dataset.detail = value === 'm' ? 'native coordinates' : '';
            select.appendChild(option);
        }
        select.value = 'm';
        root.append(label, select);
        const options = {searchable, details: true};
        equal(labDOM.call('lab_select_enhance', select, options, document), 1, 'native enhancement');
        const shell = select.closest('.select-shell');
        const fixture = {
            select, shell, label, options,
            button: shell.querySelector('.select-button'), menu: shell.querySelector('.select-menu'),
            search: shell.querySelector('.select-search'), items: () => [...shell.querySelectorAll('.select-option')]
        };
        fixtures.push(fixture);
        return fixture;
    };
    const key = (node, name) => {
        const event = new KeyboardEvent('keydown', {key: name, bubbles: true, cancelable: true});
        node.dispatchEvent(event);
        return event;
    };
    const open = fixture => labDOM.call('lab_select_event', fixture.select, 0, 0, null);
    try {
        equal(labDOM.call('lab_select_enhance', null, {}, document), 0, 'missing select is inert');
        equal(labDOM.call('lab_select_register', null), 0, 'missing registration is inert');
        const detached = document.createElement('select');
        equal(labDOM.call('lab_select_register', detached), 0, 'unbuilt select cannot register');
        const first = create('selectEventFirst', true), second = create('selectEventSecond', false);
        equal(first.button.getAttribute('aria-labelledby'), first.label.id, 'native label association');
        equal(first.items()[2].querySelector('.select-option-detail').textContent, 'native coordinates',
              'initial native enhancement includes detail projection');
        first.options.details = false;
        labDOM.call('lab_select_refresh', first.select, first.options);
        check(!first.items()[2].querySelector('.select-option-detail'), 'native rebuild reads changed options');
        first.options.details = true;
        labDOM.call('lab_select_refresh', first.select, first.options);
        equal(first.items()[2].querySelector('.select-option-detail').textContent, 'native coordinates',
              'native rebuild restores requested detail column');
        equal(labDOM.call('lab_select_register', first.select), 0, 'registration is idempotent');
        equal(labDOM.call('lab_select_register', first.select), 0, 'repeated registration remains inert');
        equal(labDOM.call('lab_select_enhance', first.select, {}, document), 0, 'enhancement is idempotent');
        first.button.click();
        check(first.shell.classList.contains('open'), 'one click opens once after repeated registration');
        first.button.click();
        check(!first.shell.classList.contains('open'), 'one subsequent click closes once');

        let bubbled = 0;
        root.addEventListener('keydown', () => ++bubbled);
        check(key(first.button, 'ArrowDown').defaultPrevented, 'button opening cancels browser default');
        equal(bubbled, 0, 'handled button key stops propagation');
        equal(document.activeElement, first.search, 'search receives keyboard-opening focus');
        check(key(first.search, 'ArrowDown').defaultPrevented, 'search navigation handled');
        equal(document.activeElement, first.items()[2], 'search key does not bubble and skip selected item');
        equal(bubbled, 0, 'handled search key stops propagation');
        key(first.items()[2], 'ArrowUp');
        equal(document.activeElement, first.items()[0], 'menu navigation skips disabled options');
        key(first.items()[0], 'ArrowUp');
        equal(document.activeElement, first.items()[3], 'menu navigation wraps backwards');
        check(!key(first.search, 'Home').defaultPrevented, 'unknown browser key remains unhandled');
        equal(bubbled, 1, 'unknown key retains bubbling');
        check(key(first.search, 'Escape').defaultPrevented, 'search Escape handled');
        equal(document.activeElement, first.button, 'search Escape restores trigger focus');
        check(first.menu.classList.contains('hidden'), 'search Escape closes menu');
        first.button.click();
        check(!key(first.button, 'Escape').defaultPrevented, 'trigger Escape preserves original cancellation policy');
        check(first.menu.classList.contains('hidden'), 'trigger Escape still closes menu');

        first.button.click();
        first.search.value = 'malaga';
        first.search.dispatchEvent(new Event('input', {bubbles: true}));
        equal(first.items().filter(item => !item.classList.contains('hidden')).length, 1, 'native search input dispatch');
        check(!first.items()[2].classList.contains('hidden'), 'native Unicode filtering retained');
        first.search.value = '';
        first.search.dispatchEvent(new Event('input', {bubbles: true}));
        let changes = 0;
        first.select.addEventListener('change', () => {
            ++changes;
            // A queued change must reach the browser only after the original native scope closes.
            labDOM.call('lab_select_sync', first.select);
        });
        const child = document.createElement('span');
        child.textContent = 'Nested native option';
        first.items()[0].appendChild(child);
        child.click();
        equal(first.select.value, 'a', 'delegated nested option click resolves owning option');
        equal(changes, 1, 'one changed selection emits one event');
        equal(first.button.textContent, 'Alpha', 'deferred native change listener synchronises label');
        first.button.click();
        child.click();
        equal(changes, 1, 'unchanged selection emits no duplicate event');
        first.select.value = 'j';
        first.select.dispatchEvent(new Event('change', {bubbles: true}));
        equal(first.button.textContent, '東京', 'external native select changes synchronise custom control');

        // These menus share a document target/action but require distinct select contexts.
        open(first);
        open(second);
        check(first.shell.classList.contains('open') && second.shell.classList.contains('open'), 'both fixture menus open');
        first.search.click();
        check(first.shell.classList.contains('open'), 'inside click preserves its own menu');
        check(!second.shell.classList.contains('open'), 'same click closes the other context');
        open(second);
        root.click();
        check(!first.shell.classList.contains('open') && !second.shell.classList.contains('open'),
              'outside listeners are registered for every select context');
        const labelClick = new MouseEvent('click', {bubbles: true, cancelable: true});
        first.label.dispatchEvent(labelClick);
        check(labelClick.defaultPrevented, 'label suppresses hidden native select activation');
        equal(document.activeElement, first.button, 'label focuses custom button');

        first.select.disabled = true;
        first.select.dispatchEvent(new Event('change', {bubbles: true}));
        check(first.button.disabled, 'native disabled state synchronised through change subscription');
        key(first.button, 'Enter');
        check(!first.shell.classList.contains('open'), 'synthetic keyboard event cannot open disabled select');
        first.select.disabled = false;
        first.select.dispatchEvent(new Event('change', {bubbles: true}));
        const plan = labDOM.call('lab_select_dispatch', 99, {target: first.button}, first.select);
        equal(plan.calls.length, 0, 'unknown action yields an empty service plan');
        check(!plan.prevent && !plan.stop, 'unknown action does not consume event');
        const foreign = labDOM.call('lab_select_dispatch', 3, {target: second.items()[0]}, first.select);
        equal(foreign.calls.length, 0, 'synchronous select policy requests no application services');
        equal(first.select.value, 'j', 'foreign menu option cannot change context select');

        // Callbacks retain the select, so rebuilding may replace all option nodes safely.
        first.select.replaceChildren(new Option('Replacement', 'replacement'));
        labDOM.call('lab_select_rebuild', first.select, 0);
        first.button.click();
        first.items()[0].click();
        equal(first.select.value, 'replacement', 'delegated subscription survives option replacement');
        check(first.menu.classList.contains('hidden'), 'rebuilt option closes its menu');
        key(second.button, ' ');
        equal(document.activeElement, second.items()[2], 'plain select uses native keyboard focus policy');
    } finally {
        for (const fixture of fixtures)
            labDOM.call('lab_select_event', fixture.select, 3, 0, null);
        hideButtonTooltip();
        root.remove();
        focus?.focus();
    }
};
