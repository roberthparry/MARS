/**
 * @file select_event_checks.ts
 * @brief Selection-menu subscriptions and event-dispatch regressions.
 *
 * Builds searchable and ordinary selection fixtures to exercise focus, keyboard behaviour and native event ownership.
 */

/** Native select subscriptions and browser-event dispatch checks; the parent runs these sequentially. */
window.checkLabSelectEvents = function checkLabSelectEvents() {
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error(`Select events: ${message}`);
    }

    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }
    const focus = document.activeElement;
    const root = document.createElement('section');
    root.style.cssText = 'position:fixed;left:0;top:0;z-index:10000;width:400px;height:80px';
    document.body.appendChild(root);
    const fixtures: BindingSelectFixture[] = [];
    function create(id: string, searchable: boolean): BindingSelectFixture {
        const label = document.createElement('label');
        const select = document.createElement('select');
        select.id = id;
        label.htmlFor = id;
        label.id = id + 'Label';
        label.textContent = 'Native places';
        for (const [name, value, disabled] of [
                 ['Alpha', 'a', false], ['Disabled', 'disabled', true], ['Málaga', 'm', false], ['東京', 'j', false]] as const) {
            const option = new Option(name, value);
            option.disabled = disabled;
            option.dataset.detail = value === 'm' ? 'native coordinates' : '';
            select.appendChild(option);
        }
        select.value = 'm';
        root.append(label, select);
        const options = {searchable, details: true};
        equal(labDOM.call('lab_select_enhance', select, options, document), 1, 'native enhancement');
        const shell = select.closest<HTMLElement>('.select-shell')!;
        const fixture: BindingSelectFixture = {
            select, shell, label, options,
            button: shell.querySelector<HTMLButtonElement>('.select-button')!,
            menu: shell.querySelector<HTMLElement>('.select-menu')!,
            search: shell.querySelector<HTMLInputElement>('.select-search')!,
            items() {
                return [...shell.querySelectorAll<HTMLButtonElement>('.select-option')];
            }
        };
        fixtures.push(fixture);
        return fixture;
    }

    function key(node: EventTarget, name: string): KeyboardEvent {
        const event = new KeyboardEvent('keydown', {key: name, bubbles: true, cancelable: true});
        node.dispatchEvent(event);
        return event;
    }

    function open(fixture: BindingSelectFixture) {
        return labDOM.call('lab_select_event', fixture.select, 0, 0, null);
    }
    try {
        equal(labDOM.call('lab_select_enhance', null, {}, document), 0, 'missing select is inert');
        equal(labDOM.call('lab_select_register', null), 0, 'missing registration is inert');
        const detached = document.createElement('select');
        equal(labDOM.call('lab_select_register', detached), 0, 'unbuilt select cannot register');
        const first = create('selectEventFirst', true);
        const second = create('selectEventSecond', false);
        equal(first.button.getAttribute('aria-labelledby'), first.label.id, 'native label association');
        equal(first.items()[2].querySelector('.select-option-detail')!.textContent, 'native coordinates',
              'initial native enhancement includes detail projection');
        first.options.details = false;
        labDOM.call('lab_select_refresh', first.select, first.options);
        check(!first.items()[2].querySelector('.select-option-detail'), 'native rebuild reads changed options');
        first.options.details = true;
        labDOM.call('lab_select_refresh', first.select, first.options);
        equal(first.items()[2].querySelector('.select-option-detail')!.textContent, 'native coordinates',
              'native rebuild restores requested detail column');
        equal(labDOM.call('lab_select_register', first.select), 0, 'registration is idempotent');
        equal(labDOM.call('lab_select_register', first.select), 0, 'repeated registration remains inert');
        equal(labDOM.call('lab_select_enhance', first.select, {}, document), 0, 'enhancement is idempotent');
        first.button.click();
        check(first.shell.classList.contains('open'), 'one click opens once after repeated registration');
        equal(getComputedStyle(root).overflowY, 'visible', 'open menu escapes its clipping card');
        const outsideRow = first.items()[3];
        const outsideRect = outsideRow.getBoundingClientRect();
        check(outsideRect.top > root.getBoundingClientRect().bottom, 'test row lies below the card edge');
        const hit = document.elementFromPoint(outsideRect.left + outsideRect.width / 2,
                                             outsideRect.top + outsideRect.height / 2);
        check(hit && outsideRow.contains(hit), 'row beyond the card remains visible to pointer hit-testing');
        first.button.click();
        check(!first.shell.classList.contains('open'), 'one subsequent click closes once');
        equal(getComputedStyle(root).overflowY, 'hidden', 'closed card restores its original clipping');

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
        first.select.addEventListener('change', function recordSelection() {
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

        const welsh = new Option('Llanfairpwllgwyngyll', 'welsh-long-name');
        welsh.dataset.detail = '53.2214, -4.2033';
        first.select.append(welsh);
        labDOM.call('lab_select_rebuild', first.select, 1);
        open(first);
        first.search.value = 'llanf';
        first.search.dispatchEvent(new Event('input', {bubbles: true}));
        const filtered = first.items().filter(item => !item.classList.contains('hidden'));
        equal(filtered.length, 1, 'Welsh town search has one visible result');
        function hasStationaryRule(sheet: CSSStyleSheet): boolean {
            return Array.from(sheet.cssRules).some(rule =>
                rule instanceof CSSImportRule ? Boolean(rule.styleSheet && hasStationaryRule(rule.styleSheet)) :
                rule instanceof CSSStyleRule && rule.selectorText === '.select-option:active:not(:disabled)' &&
                rule.style.transform === 'none');
        }
        const stationary = Array.from(document.styleSheets).some(hasStationaryRule);
        check(stationary, 'pressed menu results override the shared moving-button style');
        filtered[0].querySelector<HTMLElement>('.select-option-label')!.click();
        equal(first.select.value, 'welsh-long-name', 'one click on filtered town text selects its option');
        check(!first.shell.classList.contains('open'), 'filtered selection closes the menu');
        first.select.value = 'j';
        first.select.dispatchEvent(new Event('change', {bubbles: true}));

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
        const plan = labDOM.call<BindingTestPlan>('lab_select_dispatch', 99, {target: first.button}, first.select);
        equal(plan.calls.length, 0, 'unknown action yields an empty service plan');
        check(!plan.prevent && !plan.stop, 'unknown action does not consume event');
        const foreign = labDOM.call<BindingTestPlan>('lab_select_dispatch', 3, {target: second.items()[0]}, first.select);
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

        // A country-sized catalogue must not rewrite every row when one town changes.
        const large = create('selectEventLarge', true);
        const towns = Array.from({length: 5000}, (_, index) => new Option(`Town ${index}`, `town-${index}`));
        towns[2500] = new Option('Colchester', 'colchester');
        towns[2501] = new Option('West Mersea', 'west-mersea');
        large.select.replaceChildren(...towns);
        labDOM.call('lab_select_rebuild', large.select, 0);
        open(large);
        const rows = large.items();
        const mutations = new MutationObserver(() => {});
        mutations.observe(large.shell.querySelector('.select-options')!, {attributes: true, subtree: true});
        let largeChanges = 0;
        large.select.addEventListener('change', () => ++largeChanges);
        rows[2500].click();
        equal(large.select.value, 'colchester', 'large catalogue selects Colchester');
        equal(largeChanges, 1, 'large catalogue dispatches one change');
        check(!large.shell.classList.contains('open'), 'large catalogue closes immediately after selection');
        check(mutations.takeRecords().length <= 4, 'selection updates at most two rows, without re-filtering 5000 towns');
        mutations.disconnect();
        open(large);
        large.search.value = 'West Mersea';
        large.search.dispatchEvent(new Event('input', {bubbles: true}));
        rows[2501].click();
        equal(large.select.value, 'west-mersea', 'large catalogue filtered selection remains usable');
    } finally {
        for (const fixture of fixtures)
            labDOM.call('lab_select_event', fixture.select, 3, 0, null);
        WorksheetResults.tooltip.hide();
        root.remove();
        if (focus && 'focus' in focus && typeof focus.focus === 'function')
            focus.focus();
    }
};
