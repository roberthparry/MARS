/**
 * @file select_checks.ts
 * @brief Selection-menu DOM and keyboard regressions.
 *
 * Checks native-controlled menu behaviour, retired control routes and themed inline plot choices against the live page.
 */

/** @brief Verify registry ownership without installing page-global controllers or DOM callback properties. */
function checkSelectRegistryOwnership(check: LabTestCheck): void {
    const select = document.createElement('select');
    const other = document.createElement('select');
    const events: string[] = [];
    let accepted = false;
    let attempts = 0;
    let installFailure = false;
    const failure = new Error('synthetic enhancement failure');
    const native = {
        call(entry: string, ...args: unknown[]): unknown {
            if (entry === 'lab_select_enhance') {
                ++attempts;
                if (installFailure)
                    throw failure;
                return accepted;
            }
            if (entry === 'lab_flow_location_select')
                return labDOM.call(entry, ...args);
            events.push(entry + ':' + (args[0] === select ? 'first' : 'other'));
            return 0;
        },
        services(plan: NativeServicePlan, services: NativeServiceRegistry): void {
            labDOM.services(plan, services);
        }
    };
    function nativeProvider() { return native; }
    function documentProvider() { return document; }
    const first = new WorksheetLocations.RoundedSelectRegistry(nativeProvider, documentProvider);
    const second = new WorksheetLocations.RoundedSelectRegistry(nativeProvider, documentProvider);
    try {
        check(first.enhance(select) === null, 'rejected enhancement does not acquire ownership');
        first.rebuild(select);
        first.syncSelection(select);
        check(events.length === 0, 'failed enhancement leaves registry operations inert');
        installFailure = true;
        let caught: unknown;
        try {
            first.enhance(select);
        } catch (error) {
            caught = error;
        }
        check(caught === failure, 'enhancement exceptions propagate unchanged');
        installFailure = false;
        accepted = true;
        const controller = first.enhance(select);
        const otherController = second.enhance(other);
        if (!controller || !otherController)
            throw new Error('Selection registry: fixture installation failed');
        const installedAttempts = attempts;
        check(first.enhance(select) === null && attempts === installedAttempts,
            'duplicate enhancement cannot invoke native installation or replace the owner');
        check(!Object.hasOwn(select, '__marsRebuildRoundedSelect') && !Object.hasOwn(select, '__marsSyncRoundedSelect'),
            'enhancement does not attach callback properties to DOM nodes');
        second.rebuild(select);
        second.syncSelection(select);
        second.sync(select);
        check(events.length === 0, 'another registry cannot dispatch an unowned menu');
        first.rebuild(select);
        first.syncSelection(select);
        first.sync(select);
        check(events.join(',') === 'lab_select_refresh:first,lab_select_sync:first,lab_select_refresh:first',
            'direct and native-planned dispatch use the registered controller');
        events.length = 0;
        const dispose = controller.dispose;
        dispose();
        dispose();
        first.rebuild(select);
        first.syncSelection(select);
        first.sync(select);
        check(events.join(',') === 'lab_select_event:first', 'individual disposal closes once and unregisters the menu');
        const replacement = first.enhance(select);
        if (!replacement)
            throw new Error('Selection registry: replacement installation failed');
        dispose();
        events.length = 0;
        first.rebuild(select);
        check(events.join(',') === 'lab_select_refresh:first', 'old disposal cannot unregister a replacement controller');
        events.length = 0;
        first.dispose();
        first.dispose();
        first.rebuild(select);
        first.syncSelection(select);
        first.sync(select);
        check(events.join(',') === 'lab_select_event:first', 'registry disposal is idempotent and clears dispatch ownership');
        second.syncSelection(other);
        check(events.at(-1) === 'lab_select_sync:other', 'disposing one registry preserves another registry');
        events.length = 0;
        second.dispose();
        second.dispose();
        second.rebuild(other);
        second.syncSelection(other);
        check(events.join(',') === 'lab_select_event:other', 'global disposal releases the remaining owner exactly once');
    } finally {
        first.dispose();
        second.dispose();
    }
}

/** Real DOM and keyboard regressions for C/WASM selection menus. */
window.checkLabSelects = async function checkLabSelects() {
    function check(ok: unknown, message: string): void {
        if (!ok)
            throw new Error('C selection menus: ' + message);
    }
    checkSelectRegistryOwnership(check);
    check(!labApplication.definitionScripts.includes('controls'), 'retired control script remains in the loader');
    check((await fetch('/js/controls.js')).status === 404, 'retired control route remains available');
    check(!document.querySelector<HTMLElement>('#plotRange select')!, 'plot settings use inline choices, not selection menus');
    for (const id of ['plotKindChoices', 'plotScaleChoices']) {
        const radios = [...document.getElementById(id)!.querySelectorAll('input[type="radio"]')];
        check(radios.length === 3, 'plot choices include Auto alongside their two overrides');
        for (const radio of radios) {
            const style = getComputedStyle(radio);
            check(style.colorScheme === 'dark' && style.accentColor !== 'auto',
                  'plot radio controls must use the dark theme and explicit accent colour');
        }
    }
    const inputIds = ['plotLower', 'plotUpper', 'plotYLower', 'plotYUpper'];
    const inputReference = getComputedStyle(document.getElementById('plotLower')!);
    for (const id of inputIds) {
        const inputStyle = getComputedStyle(document.getElementById(id)!);
        for (const property of ['color', 'backgroundColor', 'borderRadius', 'fontFamily'] as const)
            check(inputStyle[property] === inputReference[property], id + ' input theme mismatch: ' + property);
        check(inputStyle.colorScheme === 'dark', id + ' must request dark native input states');
    }
    const placeholder = getComputedStyle(document.getElementById('plotYLower')!, '::placeholder');
    check(placeholder.color === 'rgba(233, 244, 239, 0.14)', 'manual limit placeholder theme');
    // Autofill cannot be triggered by assigning .value. Exercise the shipped declarations
    // through a test-only selector, without claiming to trigger the browser's autofill service.
    function isFormsImport(rule: CSSRule): rule is CSSImportRule {
        return Boolean((rule as CSSImportRule).href?.endsWith('/css/forms.css'));
    }

    function isAutofillRule(rule: CSSRule): rule is CSSStyleRule {
        return Boolean((rule as CSSStyleRule).selectorText?.includes(':autofill'));
    }

    const entry = Array.from(document.styleSheets).find(sheet => sheet.href?.endsWith('/index.css'))!;
    const formsImport = Array.from(entry.cssRules).find(isFormsImport)!;
    const autofillRules = Array.from(formsImport.styleSheet!.cssRules).filter(isAutofillRule);
    check(autofillRules.length === 2, 'normal and focused autofill safeguards must exist');
    const autofillStyle = document.createElement('style');
    const fixtureRules: string[] = [];
    for (const rule of autofillRules) {
        const selector = rule.selectorText.replace(/:-webkit-autofill|:autofill/g, '[data-test-autofill]');
        fixtureRules.push(selector + '{' + rule.style.cssText + '}');
    }
    autofillStyle.textContent = fixtureRules.join('\n');
    document.head.appendChild(autofillStyle);
    const active = document.activeElement;
    try {
        for (const id of inputIds) {
            const input = document.getElementById(id)!;
            input.setAttribute('data-test-autofill', '');
            const normal = getComputedStyle(input);
            check(normal.boxShadow.includes('rgb(9, 31, 23)') && normal.boxShadow.includes('1000px'),
                  id + ' must cover autofill paint with opaque panel colour');
            check(normal.webkitTextFillColor === inputReference.color, id + ' autofill text must stay light');
            input.focus();
            check(getComputedStyle(input).boxShadow.includes('1000px'), id + ' focus removed autofill protection');
            input.removeAttribute('data-test-autofill');
        }
    } finally {
        for (const id of inputIds)
            document.getElementById(id)!.removeAttribute('data-test-autofill');
        autofillStyle.remove();
        (active as HTMLElement | null)?.focus();
    }
    const root = document.createElement('div');
    root.style.cssText = 'position:fixed;left:0;top:0;z-index:10000;width:400px';
    document.body.appendChild(root);
    function key(node: HTMLElement, text: string): boolean {
        const event = new KeyboardEvent('keydown', {key: text, bubbles: true, cancelable: true});
        node.dispatchEvent(event);
        return event.defaultPrevented;
    }
    try {
        const label = document.createElement('label');
        const select = document.createElement('select');
        select.id = 'nativeSelectFixture';
        label.htmlFor = select.id;
        label.id = 'nativeSelectFixtureLabel';
        label.textContent = 'Places';
        root.append(label, select);
        for (const [name, value] of [['Málaga', 'es'], ['Disabled', 'no'], ['Shrewsbury', 'gb'], ['東京', 'jp']]) {
            const option = new Option(name, value);
            option.dataset.detail = value === 'es' ? '36.7204, -4.4203' : '';
            option.disabled = value === 'no';
            select.appendChild(option);
        }
        select.value = 'gb';
        const widget = WorksheetLocations.selects.enhance(select, {searchable: true, details: true});
        const shell = select.closest<HTMLElement>('.select-shell')!;
        const button = shell.querySelector<HTMLButtonElement>('.select-button')!;
        const search = shell.querySelector<HTMLInputElement>('.select-search')!;
        const menu = shell.querySelector<HTMLElement>('.select-menu')!;
        function items(): HTMLButtonElement[] {
            return [...shell.querySelectorAll<HTMLButtonElement>('.select-option')];
        }
        check(
            button.textContent === 'Shrewsbury' && button.getAttribute('aria-labelledby') === label.id,
            'initial selection or accessible label lost');
        check(
            items()[0].querySelector<HTMLButtonElement>('.select-option-detail')!.textContent === '36.7204, -4.4203',
            'town second column missing');
        check(
            items()[1].disabled && items()[2].getAttribute('aria-selected') === 'true',
            'disabled or selected option state lost');
        check(WorksheetLocations.selects.enhance(select) === null, 'select enhanced twice');
        check(
            key(button, 'ArrowDown') && document.activeElement === search && !menu.classList.contains('hidden'),
            'keyboard opening does not focus search');
        check(
            key(search, 'ArrowDown') && document.activeElement === items()[2],
            'search ArrowDown bubbled and skipped the selected option');
        key(items()[2], 'ArrowUp');
        check(document.activeElement === items()[0], 'keyboard navigation did not skip disabled option');
        key(items()[0], 'ArrowUp');
        check(document.activeElement === items()[3], 'backwards wrap failed');
        key(items()[3], 'ArrowDown');
        check(document.activeElement === items()[0], 'forward wrap failed');
        search.value = ' MALAGA ';
        search.dispatchEvent(new Event('input', {bubbles: true}));
        check(
            items().filter(item => !item.classList.contains('hidden')).length === 1 &&
                !items()[0].classList.contains('hidden'),
            'accent/case-insensitive filtering failed');
        let changes = 0;
        select.addEventListener('change', function recordSelection() {
            ++changes;
            labDOM.call('lab_select_sync', select);
        });
        items()[0].click();
        check(
            select.value === 'es' && changes === 1 && button.textContent === 'Málaga' &&
                document.activeElement === button && !shell.classList.contains('open'),
            'delegated selection/focus/change sequence failed');
        button.click();
        items()[0].click();
        check(changes === 1, 'unchanged selection emitted change');
        button.click();
        search.value = '-4.4203';
        search.dispatchEvent(new Event('input', {bubbles: true}));
        check(!items()[0].classList.contains('hidden'), 'town details cannot be searched');
        search.value = '東京';
        search.dispatchEvent(new Event('input', {bubbles: true}));
        check(
            !items()[3].classList.contains('hidden') && items()[0].classList.contains('hidden'),
            'Unicode label search failed');
        search.value = 'missing';
        search.dispatchEvent(new Event('input', {bubbles: true}));
        check(shell.querySelector<HTMLElement>('.select-empty')!.classList.contains('visible'), 'empty search message missing');
        key(search, 'ArrowDown');
        check(document.activeElement === search, 'empty navigation did not retain search focus');
        key(search, 'Escape');
        check(document.activeElement === button && menu.classList.contains('hidden'), 'Escape focus restore failed');
        button.click();
        check(search.value === '' && items().every(item => !item.classList.contains('hidden')), 'open retains filter');
        root.click();
        check(menu.classList.contains('hidden'), 'outside click did not close menu');
        select.disabled = true;
        widget!.sync();
        key(button, 'Enter');
        check(button.disabled && menu.classList.contains('hidden'), 'disabled select can open');
        select.disabled = false;

        // Rendering a large catalogue must not exhaust the scoped 4095-handle table.
        const fragment = document.createDocumentFragment();
        for (let i = 0; i < 1500; ++i)
            fragment.appendChild(new Option(`Town ${i}`, String(i)));
        select.replaceChildren(fragment);
        select.value = '1499';
        WorksheetLocations.selects.rebuild(select);
        check(items().length === 1500 && button.textContent === 'Town 1499', 'large bounded rebuild failed');
        button.click();
        search.value = 'Town 1499';
        search.dispatchEvent(new Event('input', {bubbles: true}));
        check(items().filter(item => !item.classList.contains('hidden')).length === 1, 'large catalogue filter failed');
        widget!.close();

        // Plain selectors focus the chosen option, and all dynamic labels remain text.
        const plain = document.createElement('select');
        plain.id = 'nativePlainFixture';
        plain.append(new Option('<img src=x onerror=alert(1)>', 'safe'));
        root.appendChild(plain);
        WorksheetLocations.selects.enhance(plain);
        const plainShell = plain.closest<HTMLElement>('.select-shell')!;
        const plainButton = plainShell.querySelector<HTMLButtonElement>('.select-button')!;
        key(plainButton, ' ');
        check(
            document.activeElement === plainShell.querySelector<HTMLButtonElement>('.select-option')! && !plainShell.querySelector<HTMLElement>('img')!,
            'plain selector focus or escaped text failed');
        plain.replaceChildren();
        WorksheetLocations.selects.rebuild(plain);
        key(plainButton, 'Enter');
        check(
            document.activeElement === plainButton && plainButton.textContent === 'Select option',
            'empty selector placeholder/focus failed');
        const group = document.createElement('optgroup');
        group.disabled = true;
        group.appendChild(new Option('Disabled group choice', 'disabled-group'));
        plain.append(group, new Option('Available', 'available'));
        plain.value = 'available';
        WorksheetLocations.selects.rebuild(plain);
        check(plainShell.querySelector<HTMLButtonElement>('.select-option')!.disabled, 'disabled optgroup state was not copied');

        // Failed native calls must not publish queued events; the next call must remain usable.
        const exports = labWire.exports;
        let discarded = 0;
        plain.addEventListener('change', () => ++discarded);
        try {
            labWire.exports = function failedEffectExports() {
                return {
                    ...exports(),
                    fixture_failed_effect(node: number) {
                        labDOM.env.lab_dom_effect(node, 2);
                        throw new Error('fixture failure');
                    }
                };
            };
            let failed = false;
            try {
                labDOM.call('fixture_failed_effect', plain);
            } catch (error) {
                failed = (error as Error).message === 'fixture failure';
            }
            check(failed && discarded === 0, 'failed native call published a queued change');
        } finally {
            labWire.exports = exports;
        }
        labDOM.call('lab_select_sync', plain);
        check(plainButton.textContent === 'Available', 'DOM bridge did not recover after a failed call');
    } finally {
        WorksheetResults.tooltip.hide();
        root.remove();
    }
};
