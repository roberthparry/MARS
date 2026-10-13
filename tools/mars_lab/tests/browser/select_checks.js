/** Real DOM and keyboard regressions for C/WASM selection menus. */
window.checkLabSelects = async function checkLabSelects() {
    const check = (ok, message) => {
        if (!ok)
            throw new Error('C selection menus: ' + message);
    };
    check(!labDefinitionScripts.includes('controls'), 'retired control script remains in the loader');
    check((await fetch('/js/controls.js')).status === 404, 'retired control route remains available');
    check(!document.querySelector('#plotRange select'), 'plot settings use inline choices, not selection menus');
    for (const id of ['plotKindChoices', 'plotScaleChoices']) {
        const radios = [...document.getElementById(id).querySelectorAll('input[type="radio"]')];
        check(radios.length === 2, 'plot override groups contain only the two explicit choices');
        for (const radio of radios) {
            const style = getComputedStyle(radio);
            check(style.colorScheme === 'dark' && style.accentColor !== 'auto',
                  'plot radio controls must use the dark theme and explicit accent colour');
        }
    }
    const inputIds = ['plotLower', 'plotUpper', 'plotYLower', 'plotYUpper'];
    const inputReference = getComputedStyle(document.getElementById('plotLower'));
    for (const id of inputIds) {
        const inputStyle = getComputedStyle(document.getElementById(id));
        for (const property of ['color', 'backgroundColor', 'borderRadius', 'fontFamily'])
            check(inputStyle[property] === inputReference[property], id + ' input theme mismatch: ' + property);
        check(inputStyle.colorScheme === 'dark', id + ' must request dark native input states');
    }
    const placeholder = getComputedStyle(document.getElementById('plotYLower'), '::placeholder');
    check(placeholder.color === 'rgba(233, 244, 239, 0.14)', 'manual limit placeholder theme');
    // Autofill cannot be triggered by assigning .value. Exercise the shipped declarations
    // through a test-only selector, without claiming to trigger the browser's autofill service.
    const entry = Array.from(document.styleSheets).find(sheet => sheet.href?.endsWith('/index.css'));
    const forms = Array.from(entry.cssRules).find(rule => rule.href?.endsWith('/css/forms.css')).styleSheet;
    const autofillRules = Array.from(forms.cssRules).filter(rule => rule.selectorText?.includes(':autofill'));
    check(autofillRules.length === 2, 'normal and focused autofill safeguards must exist');
    const autofillStyle = document.createElement('style');
    autofillStyle.textContent = autofillRules.map(rule =>
        rule.selectorText.replace(/:-webkit-autofill|:autofill/g, '[data-test-autofill]') +
        '{' + rule.style.cssText + '}').join('\n');
    document.head.appendChild(autofillStyle);
    const active = document.activeElement;
    try {
        for (const id of inputIds) {
            const input = document.getElementById(id);
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
            document.getElementById(id).removeAttribute('data-test-autofill');
        autofillStyle.remove();
        active?.focus();
    }
    const root = document.createElement('div');
    root.style.cssText = 'position:fixed;left:0;top:0;z-index:10000;width:400px';
    document.body.appendChild(root);
    const key = (node, text) => {
        const event = new KeyboardEvent('keydown', {key: text, bubbles: true, cancelable: true});
        node.dispatchEvent(event);
        return event.defaultPrevented;
    };
    try {
        const label = document.createElement('label'), select = document.createElement('select');
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
        const widget = enhanceRoundedSelect(select, {searchable: true, details: true});
        const shell = select.closest('.select-shell'), button = shell.querySelector('.select-button');
        const search = shell.querySelector('.select-search'), menu = shell.querySelector('.select-menu');
        const items = () => [...shell.querySelectorAll('.select-option')];
        check(
            button.textContent === 'Shrewsbury' && button.getAttribute('aria-labelledby') === label.id,
            'initial selection or accessible label lost');
        check(
            items()[0].querySelector('.select-option-detail').textContent === '36.7204, -4.4203',
            'town second column missing');
        check(
            items()[1].disabled && items()[2].getAttribute('aria-selected') === 'true',
            'disabled or selected option state lost');
        check(enhanceRoundedSelect(select) === null, 'select enhanced twice');
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
        select.addEventListener('change', () => {
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
        check(shell.querySelector('.select-empty').classList.contains('visible'), 'empty search message missing');
        key(search, 'ArrowDown');
        check(document.activeElement === search, 'empty navigation did not retain search focus');
        key(search, 'Escape');
        check(document.activeElement === button && menu.classList.contains('hidden'), 'Escape focus restore failed');
        button.click();
        check(search.value === '' && items().every(item => !item.classList.contains('hidden')), 'open retains filter');
        root.click();
        check(menu.classList.contains('hidden'), 'outside click did not close menu');
        select.disabled = true;
        widget.sync();
        key(button, 'Enter');
        check(button.disabled && menu.classList.contains('hidden'), 'disabled select can open');
        select.disabled = false;

        // Rendering a large catalogue must not exhaust the scoped 4095-handle table.
        const fragment = document.createDocumentFragment();
        for (let i = 0; i < 1500; ++i) fragment.appendChild(new Option(`Town ${i}`, String(i)));
        select.replaceChildren(fragment);
        select.value = '1499';
        select.__marsRebuildRoundedSelect();
        check(items().length === 1500 && button.textContent === 'Town 1499', 'large bounded rebuild failed');
        button.click();
        search.value = 'Town 1499';
        search.dispatchEvent(new Event('input', {bubbles: true}));
        check(items().filter(item => !item.classList.contains('hidden')).length === 1, 'large catalogue filter failed');
        widget.close();

        // Plain selectors focus the chosen option, and all dynamic labels remain text.
        const plain = document.createElement('select');
        plain.id = 'nativePlainFixture';
        plain.append(new Option('<img src=x onerror=alert(1)>', 'safe'));
        root.appendChild(plain);
        enhanceRoundedSelect(plain);
        const plainShell = plain.closest('.select-shell'), plainButton = plainShell.querySelector('.select-button');
        key(plainButton, ' ');
        check(
            document.activeElement === plainShell.querySelector('.select-option') && !plainShell.querySelector('img'),
            'plain selector focus or escaped text failed');
        plain.replaceChildren();
        plain.__marsRebuildRoundedSelect();
        key(plainButton, 'Enter');
        check(
            document.activeElement === plainButton && plainButton.textContent === 'Select option',
            'empty selector placeholder/focus failed');
        const group = document.createElement('optgroup');
        group.disabled = true;
        group.appendChild(new Option('Disabled group choice', 'disabled-group'));
        plain.append(group, new Option('Available', 'available'));
        plain.value = 'available';
        plain.__marsRebuildRoundedSelect();
        check(plainShell.querySelector('.select-option').disabled, 'disabled optgroup state was not copied');

        // Failed native calls must not publish queued events; the next call must remain usable.
        const exports = labWire.exports;
        let discarded = 0;
        plain.addEventListener('change', () => ++discarded);
        try {
            labWire.exports = () => ({
                ...exports(),
                fixture_failed_effect(node) {
                    labDOM.env.lab_dom_effect(node, 2);
                    throw new Error('fixture failure');
                }
            });
            let failed = false;
            try {
                labDOM.call('fixture_failed_effect', plain);
            } catch (error) {
                failed = error.message === 'fixture failure';
            }
            check(failed && discarded === 0, 'failed native call published a queued change');
        } finally {
            labWire.exports = exports;
        }
        labDOM.call('lab_select_sync', plain);
        check(plainButton.textContent === 'Available', 'DOM bridge did not recover after a failed call');
    } finally {
        hideButtonTooltip();
        root.remove();
    }
};
