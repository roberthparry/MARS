/**
 * @file appearance_checks.ts
 * @brief Exercise skin selection, local artwork and optional browser storage in a real browser.
 *
 * The checks preserve the live worksheet and original preference. Detached controls exercise
 * unavailable storage, cross-tab events and listener disposal without changing application state.
 * Native result fixtures also check body cards and event tables at phone and desktop widths.
 */
/** @brief Exercise the live month menu without changing the active worksheet's date or mathematics. */
async function checkAppearanceMonthMenu(check: LabTestCheck): Promise<void> {
    const fixture = document.createElement('div');
    fixture.className = 'mars-date-shell';
    fixture.style.cssText = 'position:fixed;left:0;top:0';
    const input = document.createElement('input');
    input.value = '2026-11-13';
    const opener = document.createElement('button');
    opener.type = 'button';
    fixture.append(input, opener);
    document.body.append(fixture);
    const month = document.getElementById('marsDatePickerMonth') as HTMLSelectElement;
    const shell = month.closest<HTMLElement>('.select-shell')!;
    const button = shell?.querySelector<HTMLButtonElement>('.select-button')!;
    const picker = document.getElementById('marsDatePicker')!;
    try {
        WorksheetLocations.picker.open(input, opener);
        check(shell && button && month.classList.contains('select-native-source') &&
            month.getAttribute('aria-hidden') === 'true' && button.getAttribute('aria-label') === 'Calendar month' &&
            month.getBoundingClientRect().width <= 1 && month.getBoundingClientRect().height <= 1 &&
            getComputedStyle(month).pointerEvents === 'none',
            'the month picker exposes a labelled themed button instead of an operating-system select');
        button.click();
        check(shell.classList.contains('open') && shell.querySelectorAll('.select-option').length === 12 &&
            !shell.querySelector('.select-search') && button.textContent === 'November',
            'opening the month menu shows all twelve months without search');
        shell.querySelector<HTMLButtonElement>('[data-value="2"]')!.click();
        check(input.value === '2026-02-13' && button.textContent === 'February' && !shell.classList.contains('open'),
            'choosing a themed month commits the date and synchronises the label');
        document.getElementById('marsDatePickerYearDown')!.click();
        document.getElementById('marsDatePickerNext')!.click();
        check(input.value === '2025-03-13' && button.textContent === 'March',
            'year and month navigation keep the enhanced menu selection current');
        button.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true, cancelable: true}));
        await new Promise(requestAnimationFrame);
        check(document.activeElement === shell.querySelector('.select-option.selected'),
            'keyboard opening focuses the selected month');
        document.activeElement!.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape', bubbles: true, cancelable: true}));
        await new Promise(requestAnimationFrame);
        check(!shell.classList.contains('open') && !picker.classList.contains('hidden') && document.activeElement === button,
            'Escape closes the month menu and restores focus while retaining the calendar');
        button.click();
        WorksheetLocations.picker.close();
        check(!shell.classList.contains('open'), 'closing the calendar also dismisses its month menu');
        WorksheetLocations.picker.open(input, opener);
        check(button.textContent === 'March' && !shell.classList.contains('open'),
            'reopening restores the month label with its menu closed');
    } finally {
        WorksheetLocations.picker.close();
        fixture.remove();
    }
}

window.checkLabAppearance = async function checkLabAppearance(): Promise<void> {
    function check(ok: unknown, message: string): void {
        if (!ok) throw new Error('Appearance: ' + message);
    }
    const deadline = performance.now() + 30000;
    while (WorksheetRequests.labRequests.busy() && performance.now() < deadline)
        await new Promise(resolve => setTimeout(resolve, 20));
    check(!WorksheetRequests.labRequests.busy(), 'worksheet settles before checking decorative changes');
    await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    const names = ['Earth (Default)', 'Mars', 'Mercury', 'Jupiter', 'Venus', 'Saturn', 'Neptune',
        'Moon', 'Galaxy', 'Andromeda', 'Nebula', 'Quasar', 'Asteroids', 'Exoplanet'];
    const root = document.documentElement;
    const picker = document.getElementById('skinSelect') as HTMLSelectElement;
    const stored = localStorage.getItem(WorksheetAppearance.storageKey);
    const original = WorksheetAppearance.controller.current();
    const editor = WorksheetBindings.editor.currentExpressionText();
    const mode = WorksheetWorkspace.mode.currentMode();
    const rendered = WorksheetContext.rendered.innerHTML;
    const native = labWire.exports();
    const revisions = Array.from({length: 7}, (_, index) => native.lab_workspace_history_revision(index));
    const shell = picker.closest<HTMLElement>('.select-shell');
    const menuButton = shell?.querySelector<HTMLButtonElement>('.select-button');
    const settings_toggle = document.getElementById('settings_toggle') as HTMLButtonElement;
    const settings_panel = document.getElementById('settings_panel')!;
    const settings_close = document.getElementById('settings_close') as HTMLButtonElement;
    const paletteProbe = document.createElement('span');
    paletteProbe.hidden = true;
    document.body.append(paletteProbe);
    const almanacFrame = document.createElement('iframe');
    almanacFrame.style.cssText = 'position:fixed;left:-10000px;top:0;width:390px;height:844px;border:0';
    try {
        await checkAppearanceMonthMenu(check);
        check(!settings_panel.matches(':popover-open') && !menuButton!.getClientRects().length &&
            settings_toggle.getClientRects().length > 0 && settings_toggle.textContent!.trim() === '' &&
            settings_toggle.getAttribute('aria-label') === 'Settings',
            'only an accessible settings cog appears in the header before opening settings');
        settings_toggle.click();
        check(settings_panel.matches(':popover-open') && menuButton!.getClientRects().length > 0 &&
            settings_panel.contains(picker) && document.getElementById('skinSelectLabel')!.textContent === 'Theme',
            'the cog opens settings containing the labelled theme selector');
        const loaded = new Promise<void>(resolve => { almanacFrame.onload = () => resolve(); });
        // Start with an oversized fallback to check that the production theme owns its rem scale.
        almanacFrame.srcdoc = '<!doctype html><html><head><meta name="viewport" ' +
            'content="width=device-width, initial-scale=1">' +
            '<style>:root, button { font-size: 64px; }</style>' +
            '<link rel="stylesheet" href="/index.css">' +
            '</head><body><button id="font_scale_probe" type="button" hidden>Evaluate</button>' +
            '<div class="rendered" id="fixture"></div></body></html>';
        document.body.append(almanacFrame);
        await loaded;
        const almanacDoc = almanacFrame.contentDocument!;
        almanacDoc.body.prepend(document.querySelector('body > header')!.cloneNode(true));
        const frame_toggle = almanacDoc.getElementById('settings_toggle')!;
        const frame_panel = almanacDoc.getElementById('settings_panel')!;
        const frame_close = almanacDoc.getElementById('settings_close')!;
        const almanacTarget = almanacDoc.getElementById('fixture')!;
        const month_picker = document.getElementById('marsDatePicker')!.cloneNode(true) as HTMLElement;
        almanacDoc.body.append(month_picker);
        const month_shell = month_picker.querySelector<HTMLElement>('.select-shell')!;
        const month_menu = month_shell.querySelector<HTMLElement>('.select-menu')!;
        month_shell.classList.add('open');
        month_menu.classList.remove('hidden');
        WorksheetResults.calendarResults.renderAlmanacWorksheet(almanacTarget, window.labAlmanacFixtures()[0]);
        const rows = [...almanacDoc.querySelectorAll<HTMLElement>('.almanac-body-table tbody tr')];
        const tables = [...almanacDoc.querySelectorAll<HTMLElement>('.almanac-grid-table')];
        check(rows.length > 0 && tables.some(table => table.matches('.almanac-event-table')),
            'native fixture includes body cards and an event table for responsive skin checks');
        const almanacProbe = almanacDoc.createElement('span');
        almanacProbe.hidden = true;
        almanacDoc.body.append(almanacProbe);
        const almanacStyle = almanacFrame.contentWindow!.getComputedStyle.bind(almanacFrame.contentWindow!);
        check(almanacStyle(almanacDoc.documentElement).fontSize === '16px' &&
            almanacStyle(almanacDoc.getElementById('font_scale_probe')!).fontSize === '14px' &&
            parseFloat(almanacStyle(almanacDoc.querySelector('h1')!).fontSize) < 32 &&
            almanacDoc.querySelector('h1')!.getBoundingClientRect().right <= almanacDoc.documentElement.clientWidth,
            'an oversized fallback font cannot enlarge or clip the mobile heading and controls');
        check(shell && menuButton && picker.classList.contains('select-native-source'),
            'the skin menu uses native-projected page controls rather than a browser-painted popup');
        check(menuButton!.getAttribute('aria-labelledby') === 'skinSelectLabel', 'the theme selector has its visible label');
        check(picker.options.length === names.length, 'all requested skins have one selector entry');
        WorksheetAppearance.controller.select('13', false);
        for (let index = 0; index < names.length; ++index) {
            check(picker.options[index].value === String(index) &&
                picker.options[index].textContent === `${index}. ${names[index]}`, 'numbered skin: ' + names[index]);
            menuButton!.click();
            check(shell!.classList.contains('open'), 'the visible skin menu opens');
            const option = shell!.querySelector<HTMLButtonElement>(`.select-option[data-value="${index}"]`)!;
            check(option && option.getBoundingClientRect().height > 0, 'the skin choice is a visible page control');
            option.click();
            await new Promise(requestAnimationFrame);
            check(root.dataset.skin === String(index) &&
                localStorage.getItem(WorksheetAppearance.storageKey) === String(index), 'selection persists: ' + names[index]);
            const style = getComputedStyle(root);
            paletteProbe.style.color = style.getPropertyValue('--accent');
            check(!shell!.classList.contains('open') && menuButton!.textContent === picker.selectedOptions[0].textContent &&
                option.getAttribute('aria-selected') === 'true', 'visible menu selection follows the current skin');
            check(getComputedStyle(option).backgroundColor === getComputedStyle(paletteProbe).color,
                'the rendered selected row uses the current skin accent: ' + names[index]);
            check(document.querySelector<HTMLMetaElement>('meta[name="theme-color"]')?.content ===
                style.getPropertyValue('--paper').trim(), 'browser chrome follows the palette');
            almanacDoc.documentElement.dataset.skin = String(index);
            for (const width of [390, 560, 900, 1280]) {
                almanacFrame.style.width = width + 'px';
                check(frame_toggle.getBoundingClientRect().width >= 44 &&
                    frame_toggle.getBoundingClientRect().height >= 44 && !frame_panel.getClientRects().length,
                    `${width}px: a compact settings cog keeps the theme selector off the main page`);
                frame_toggle.click();
                const panel_bounds = frame_panel.getBoundingClientRect();
                check(frame_panel.matches(':popover-open') && panel_bounds.left >= 0 && panel_bounds.right <= width &&
                    frame_panel.querySelector('.select-button')!.getClientRects().length > 0,
                    `${width}px: settings and its theme selector fit the viewport`);
                frame_close.click();
                check(!frame_panel.matches(':popover-open'), `${width}px: the close button dismisses settings`);
                const summaryStyle = almanacStyle(almanacDoc.querySelector('.almanac-summary')!);
                for (const row of rows) {
                    const rowStyle = almanacStyle(row);
                    if (width <= 560) {
                        const expected = index ? summaryStyle.backgroundColor : 'rgba(8, 24, 19, 0.46)';
                        check(rowStyle.display === 'grid' && rowStyle.backgroundColor === expected,
                            `${width}px: native body card fills follow ${names[index]}`);
                        if (index)
                            check(rowStyle.borderTopColor === summaryStyle.borderTopColor,
                                `${width}px: native card borders follow ${names[index]}`);
                    }
                    if (index) {
                        almanacProbe.style.color = 'var(--ink)';
                        for (const name of row.querySelectorAll('.body-name, .reference-name'))
                            check(almanacStyle(name).color === almanacStyle(almanacProbe).color,
                                `${width}px: native card names follow ${names[index]}`);
                    }
                }
                if (index)
                    for (const table of tables) {
                        const mobileCards = width <= 560 && table.matches('.almanac-body-table');
                        check(almanacStyle(table).backgroundColor ===
                            (mobileCards ? 'rgba(0, 0, 0, 0)' : summaryStyle.backgroundColor),
                            `${width}px: body and event table surfaces follow ${names[index]}`);
                    }
            }
            for (const [width, height] of [[325, 672], [672, 325], [667, 280], [1280, 844]]) {
                almanacFrame.style.width = width + 'px';
                almanacFrame.style.height = height + 'px';
                month_picker.classList.remove('hidden');
                const options = [...month_menu.querySelectorAll<HTMLElement>('.select-option')];
                const menu_box = month_menu.getBoundingClientRect();
                const selector_box = month_shell.querySelector('.select-button')!.getBoundingClientRect();
                check(Math.abs(menu_box.top - selector_box.bottom - 4) < 1 &&
                    Math.abs(menu_box.left - selector_box.left) < 1 && Math.abs(menu_box.right - selector_box.right) < 1,
                    `${width}x${height}: the month dropdown opens directly beneath its selector with matching edges`);
                check(menu_box.left >= 0 && menu_box.right <= width && menu_box.top >= 0 && menu_box.bottom <= height &&
                    options.length === 12 && options.every(option => {
                        const box = option.getBoundingClientRect();
                        return box.top >= menu_box.top && box.bottom <= menu_box.bottom &&
                            option.contains(almanacDoc.elementFromPoint(box.left + box.width / 2, box.top + box.height / 2));
                    }), `${width}x${height}: all twelve themed months are visible and clickable in ${names[index]}`);
                almanacProbe.style.color = 'var(--ink)';
                check(almanacStyle(month_menu).color === almanacStyle(almanacProbe).color,
                    `${width}x${height}: the month menu text follows ${names[index]}`);
                if (index) {
                    almanacProbe.style.backgroundColor = 'var(--panel)';
                    almanacProbe.style.color = 'var(--accent)';
                    check(almanacStyle(month_menu).backgroundColor === almanacStyle(almanacProbe).backgroundColor &&
                        almanacStyle(month_menu.querySelector('.selected')!).backgroundColor === almanacStyle(almanacProbe).color,
                        `${width}x${height}: the month menu surface and selected month follow ${names[index]}`);
                }
                month_picker.classList.add('hidden');
            }
            almanacFrame.style.height = '844px';
            if (index) {
                const cardStyle = getComputedStyle(document.querySelector<HTMLElement>('.card')!);
                for (const card of document.querySelectorAll<HTMLElement>('.help-card')) {
                    const helpStyle = getComputedStyle(card);
                    check(helpStyle.backgroundColor === cardStyle.backgroundColor &&
                        helpStyle.borderTopColor === cardStyle.borderTopColor,
                        'Help cards share the worksheet palette: ' + names[index]);
                }
                check(getComputedStyle(document.querySelector('.help-kicker')!).color === getComputedStyle(paletteProbe).color,
                    'Help section labels use the selected accent: ' + names[index]);
                check(getComputedStyle(document.querySelector('.help-card a')!).color === getComputedStyle(paletteProbe).color,
                    'Help links use the selected accent: ' + names[index]);
                paletteProbe.style.backgroundColor = 'color-mix(in srgb, var(--accent-2) 12%, var(--panel))';
                for (const code of document.querySelectorAll<HTMLElement>('.help-card code'))
                    check(getComputedStyle(code).backgroundColor === getComputedStyle(paletteProbe).backgroundColor,
                        'Help examples use the selected control palette: ' + names[index]);
            } else {
                check(getComputedStyle(document.querySelector('.help-card')!).backgroundColor === 'rgba(8, 29, 22, 0.62)',
                    'Earth retains its original green Help cards');
            }
            for (const button of document.querySelectorAll<HTMLButtonElement>('.mars-date-picker-year-step')) {
                const yearStyle = getComputedStyle(button);
                if (index) {
                    const menuStyle = getComputedStyle(menuButton!);
                    check(yearStyle.backgroundImage === 'none' &&
                        yearStyle.backgroundColor === menuStyle.backgroundColor && yearStyle.color === menuStyle.color &&
                        yearStyle.borderTopColor === menuStyle.borderTopColor,
                        'calendar year buttons follow the control palette: ' + names[index]);
                } else {
                    check(yearStyle.backgroundImage.includes('linear-gradient'), 'Earth retains its green year buttons');
                }
            }
            for (const panel of document.querySelectorAll<HTMLElement>('#workspacePanel, #resultWorkspacePanel')) {
                const frame = getComputedStyle(panel);
                const head = getComputedStyle(panel.querySelector<HTMLElement>('.panel-head')!);
                const expected = parseFloat(frame.borderTopLeftRadius) - parseFloat(frame.borderLeftWidth);
                check(parseFloat(head.borderTopLeftRadius) === expected &&
                    parseFloat(head.borderTopRightRadius) === expected, 'header fill follows the rounded frame');
            }
            check(getComputedStyle(document.getElementById('resultWorkspacePanel')!).overflow === 'visible',
                'flag previews can still extend beyond the result panel');
            if (index) {
                const source = style.getPropertyValue('--skin-art').trim();
                const match = /^url\("(\/skins\/[a-z]+\.svg)"\)$/.exec(source);
                check(match, 'skin uses local vector artwork: ' + names[index]);
                const response = await fetch(match![1]);
                check(response.ok && response.headers.get('Content-Type')?.startsWith('image/svg+xml'),
                    'artwork route serves SVG: ' + names[index]);
                const svg = new DOMParser().parseFromString(await response.text(), 'image/svg+xml');
                check(svg.documentElement.localName === 'svg' &&
                    !svg.querySelector('parsererror, script, image, foreignObject, animate, animateTransform'),
                    'valid static vector artwork: ' + names[index]);
                const image = new Image();
                image.src = match![1];
                await image.decode();
                check(image.naturalWidth > 0 && image.naturalHeight > 0, 'artwork decodes: ' + names[index]);
            }
        }
        menuButton!.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true, cancelable: true}));
        await new Promise(requestAnimationFrame);
        check(shell!.classList.contains('open') && document.activeElement === shell!.querySelector('.select-option.selected'),
            'keyboard opening focuses the selected skin');
        document.activeElement!.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp', bubbles: true, cancelable: true}));
        await new Promise(requestAnimationFrame);
        const keyboardOption = document.activeElement as HTMLElement;
        check(keyboardOption.dataset.value === '12' && picker.value === '13', 'arrows move focus without committing a skin');
        check(getComputedStyle(keyboardOption).backgroundColor === getComputedStyle(paletteProbe).color,
            'the keyboard highlight uses the current palette');
        keyboardOption.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape', bubbles: true, cancelable: true}));
        await new Promise(requestAnimationFrame);
        check(!shell!.classList.contains('open') && document.activeElement === menuButton, 'Escape closes and restores focus');
        const beforeExternal = localStorage.getItem(WorksheetAppearance.storageKey);
        window.dispatchEvent(new StorageEvent('storage', {key: WorksheetAppearance.storageKey, newValue: '8'}));
        check(picker.value === '8' && menuButton!.textContent === '8. Galaxy' &&
            shell!.querySelector('.select-option.selected')?.getAttribute('data-value') === '8' &&
            localStorage.getItem(WorksheetAppearance.storageKey) === beforeExternal,
            'cross-tab skin changes update the visible menu without echoing storage writes');
        menuButton!.click();
        document.body.click();
        check(!shell!.classList.contains('open'), 'an outside click closes the menu');
        settings_close.click();
        check(!settings_panel.matches(':popover-open') && !menuButton!.getClientRects().length,
            'closing settings hides the theme name and restores the cog-only header');
        check(editor === WorksheetBindings.editor.currentExpressionText(), 'skin changes preserve the editor');
        check(mode === WorksheetWorkspace.mode.currentMode(), 'skin changes preserve the mode');
        check(rendered === WorksheetContext.rendered.innerHTML, 'skin changes preserve the rendered result');
        check(revisions.every((revision, index) => revision === native.lab_workspace_history_revision(index)),
            'skin changes preserve every worksheet timeline');

        const detached = document.createElement('div');
        const select = picker.cloneNode(true) as HTMLSelectElement;
        const events = new EventTarget();
        let saved: string | null = '8', writes = 0;
        const controller = new WorksheetAppearance.SkinController(detached, {
            getItem: () => saved,
            setItem: (_key, value) => { saved = value; ++writes; }
        });
        try {
            let projected = '';
            controller.restore();
            controller.mount(select, null, events);
            controller.connectMenu(() => { projected = select.value; });
            check(controller.current() === '8' && select.value === '8' && writes === 0, 'restore before mounting without rewriting');
            check(projected === '8', 'connecting a native menu projects the restored choice');
            select.value = '5';
            select.dispatchEvent(new Event('change'));
            check(controller.current() === '5' && saved === '5' && writes === 1, 'selector listener saves once');
            events.dispatchEvent(new StorageEvent('storage', {key: WorksheetAppearance.storageKey, newValue: '10'}));
            check(controller.current() === '10' && select.value === '10' && projected === '10' && writes === 1,
                'cross-tab changes project the menu without echoing writes');
            events.dispatchEvent(new StorageEvent('storage', {key: 'unrelated', newValue: '3'}));
            check(controller.current() === '10', 'unrelated storage ignored');
            events.dispatchEvent(new StorageEvent('storage', {key: null, newValue: null}));
            check(controller.current() === '0', 'cleared storage restores Earth');
            for (const invalid of [null, undefined, 5, -1, '14', '-1', '01', ' 1', '1.0', 'mars', '<svg>']) {
                controller.select(invalid, false);
                check(controller.current() === '0', 'invalid preference falls back to Earth');
            }
            controller.select('6', false);
            controller.dispose();
            select.value = '1';
            select.dispatchEvent(new Event('change'));
            events.dispatchEvent(new StorageEvent('storage', {key: WorksheetAppearance.storageKey, newValue: '5'}));
            check(controller.current() === '6' && writes === 1, 'disposed subscriptions cannot mutate the appearance');
        } finally {
            controller.dispose();
        }
        const denied = new WorksheetAppearance.SkinController(detached, {
            getItem: () => { throw new DOMException('Storage denied', 'SecurityError'); },
            setItem: () => { throw new DOMException('Storage denied', 'SecurityError'); }
        });
        try {
            denied.restore();
            check(denied.current() === '0', 'denied storage retains a usable default');
            denied.select('13');
            check(denied.current() === '13', 'live selection survives denied persistence');
        } finally {
            denied.dispose();
        }
    } finally {
        if (settings_panel.matches(':popover-open')) settings_close.click();
        almanacFrame.remove();
        paletteProbe.remove();
        WorksheetAppearance.controller.select(original, false);
        if (stored === null) localStorage.removeItem(WorksheetAppearance.storageKey);
        else localStorage.setItem(WorksheetAppearance.storageKey, stored);
    }
};
