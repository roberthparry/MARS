/**
 * @file mobile_layout_checks.ts
 * @brief Responsive worksheet and result-card geometry regressions.
 *
 * Inventories visible cards before resizing an isolated iframe running production CSS, so mobile layout cannot silently hide desktop content.
 */

/** @brief Check responsive geometry using snapshots of the live, native-populated worksheet.
 * Every result/help card is inventoried before resizing; mobile CSS may not hide
 * a desktop-visible card. The iframe runs production CSS without a second app.
 */
window.checkLabMobileLayout = async function checkLabMobileLayout(state: string, desktopOnly = false): Promise<void> {
    /** @brief Report a layout failure with the captured worksheet state. */
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error(`Mobile layout (${state}): ${message}`);
    }

    /** @brief Read layout visibility without changing the fixture's DOM. */
    function visible(node: Element): boolean {
        return node.getClientRects().length > 0;
    }

    /** @brief Identify the button stylesheet in the production stylesheet's small import list. */
    function isButtonStylesheet(rule: CSSRule): boolean {
        return (rule as CSSImportRule).href?.endsWith('/buttons.css') === true;
    }

    /** @brief Identify the production touch rule without depending on the runner's pointer hardware. */
    function isTouchRule(rule: CSSRule): boolean {
        return (rule as CSSMediaRule).conditionText?.includes('hover: none') === true;
    }

    const selectors = '.result-card, .help-card, #datetimeLocal, .datetime-section, #functionRunResult';
    const originals = [...document.querySelectorAll<HTMLElement>(selectors)];
    const expected = originals.map(visible);
    const frame = document.createElement('iframe');
    frame.style.cssText = 'position:fixed;left:-10000px;top:0;width:390px;height:844px;border:0';
    try {
        const loaded = new Promise<void>(function waitForLoad(resolve) {
            /** @brief Release the fixture after its production CSS has loaded. */
            frame.onload = function frameLoaded(): void {
                resolve();
            };
        });
        frame.srcdoc = '<!doctype html><html><head><meta name="viewport" ' +
            'content="width=device-width, initial-scale=1"><link rel="stylesheet" href="/index.css">' +
            '</head><body></body></html>';
        document.body.appendChild(frame);
        await loaded;
        const doc = frame.contentDocument!;
        doc.body.className = document.body.className;
        for (const source of document.querySelectorAll<HTMLElement>('body > header, body > main, body > #marsDatePicker'))
            doc.body.appendChild(source.cloneNode(true));
        const modeMenu = doc.getElementById('modeMenu') as HTMLDetailsElement;
        modeMenu.open = true;
        await doc.fonts.ready;
        const cards = [...doc.querySelectorAll<HTMLElement>(selectors)];
        check(cards.length === originals.length, 'complete card inventory copied');
        const precision_toolbar = doc.querySelector<HTMLElement>('.precision-toolbar')!;
        const precision_class = precision_toolbar.className;
        precision_toolbar.classList.remove('hidden');
        for (const {width, height} of [
            ...[320, 390, 560, 768, 900, 901, 1280, 1500, 1870, 1900].map(width => ({width, height: 844})),
            {width: 932, height: 430}
        ]) {
            frame.style.width = width + 'px';
            frame.style.height = height + 'px';
            const compact_modes = width <= 560 || height <= 500;
            modeMenu.open = !compact_modes;
            check(visible(doc.getElementById('modeMenuToggle')!) === compact_modes,
                  `${width}x${height}: phone orientations use the mode dropdown and desktop uses tabs`);
            const modes = modeMenu.getBoundingClientRect();
            const precision = precision_toolbar.getBoundingClientRect();
            check(precision.left >= modes.right && precision.top < modes.bottom && modes.top < precision.bottom,
                  `${width}px: precision stays beside the mode selector without wrapping underneath`);
            check(doc.documentElement.scrollWidth <= doc.documentElement.clientWidth + 1,
                  `${width}px: keeping precision beside modes does not widen the page`);
            if (width >= 1500) {
                const tabs = [...doc.querySelectorAll('.mode-tab')].map(tab => tab.getBoundingClientRect());
                check(tabs.every(tab => Math.abs(tab.top - tabs[0].top) < 1),
                      `${width}px: all desktop mode buttons remain on one row beside precision`);
            }
        }
        precision_toolbar.className = precision_class;
        frame.style.height = '844px';
        modeMenu.open = true;
        const originalClasses = doc.body.className;
        for (const width of [390, 900, 1280, 1870]) {
            frame.style.width = width + 'px';
            for (const note of doc.querySelectorAll<HTMLElement>('#almanacGlobeStatus, .globe-credit'))
                check(frame.contentWindow!.getComputedStyle(note).display === 'none',
                      `${width}px: globe footer notes occupy no space on desktop or mobile`);
            for (const mode of ['', 'equation-mode', 'diffequation-mode', 'matrix-mode', 'integrator-mode',
                                'datetime-mode', 'almanac-mode']) {
                doc.body.className = mode;
                const header = doc.querySelector('header')!;
                const padding = parseFloat(frame.contentWindow!.getComputedStyle(header).paddingTop);
                const titleTop = doc.querySelector('h1')!.getBoundingClientRect().top;
                check(Math.abs(titleTop - header.getBoundingClientRect().top - padding) < 1,
                      `${width}px: ${mode || 'expression'} title keeps the same top padding`);
                const sky = doc.querySelector('.sky-status-strip');
                if (width <= 900 && sky)
                    check(sky.getBoundingClientRect().height === 0,
                          `${width}px: ${mode || 'expression'} keeps the home-sky card out of the mobile header`);
            }
        }
        doc.body.className = originalClasses;
        if (doc.body.matches('.datetime-mode, .almanac-mode')) {
            frame.style.width = '1870px';
            const sky = doc.querySelector('.sky-status-strip');
            const result = doc.getElementById('resultWorkspacePanel');
            if (sky && result && visible(result)) {
                const skyBox = sky.getBoundingClientRect();
                const mobile = doc.querySelector('#mobileAccess summary')!.getBoundingClientRect();
                const gap = result.getBoundingClientRect().top - skyBox.bottom;
                check(Math.abs(skyBox.top - mobile.top) < 1, 'wide calendar header remains top-aligned');
                check(gap >= 8 && gap <= 20, `wide calendar result gap is compact without overlap (${gap})`);
                check(skyBox.left > doc.querySelector('.lab-tabs')!.getBoundingClientRect().right,
                      'wide calendar sky card does not overlap the mode tabs');
            }
        }
        if (desktopOnly) {
            for (const width of [320, 560, 900]) {
                frame.style.width = width + 'px';
                const sky = doc.querySelector('.sky-status-strip');
                check(!sky || sky.getBoundingClientRect().height === 0,
                      `${width}px: mobile calendar layout omits the home-sky card`);
            }
            return;
        }
        const viewports: MobileLayoutViewport[] = [
            {width: 320, height: 568},
            {width: 325, height: 672},
            {width: 390, height: 844},
            {width: 430, height: 932},
            {width: 560, height: 844},
            {width: 768, height: 1024},
            {width: 667, height: 280},
            {width: 693, height: 320},
            {width: 844, height: 390}
        ];
        for (const {width, height} of viewports) {
            frame.style.width = width + 'px';
            frame.style.height = height + 'px';
            const compact_modes = width <= 560 || height <= 500;
            modeMenu.open = !compact_modes;
            if (compact_modes) {
                const toggle = doc.getElementById('modeMenuToggle')!;
                const before = doc.querySelector('.lab-topbar')!.getBoundingClientRect();
                check(!visible(doc.querySelector('.lab-tabs')!) && toggle.getBoundingClientRect().height >= 36,
                    `${width}px: mobile modes are collapsed behind a readable Mode button`);
                toggle.click();
                check(modeMenu.open && doc.querySelector('.lab-topbar')!.getBoundingClientRect().height === before.height,
                    `${width}px: mode choices expand over the worksheet without pushing it down`);
                const selector = toggle.getBoundingClientRect();
                const menu = doc.querySelector<HTMLElement>('.lab-tabs')!;
                const popup = menu.getBoundingClientRect();
                check(Math.abs(popup.top - selector.bottom) < 1 && Math.abs(popup.left - selector.left) < 1 &&
                    Math.abs(popup.right - before.right) < 1,
                    `${width}px: mode choices drop beneath the selector across the toolbar width`);
                const first = menu.querySelector<HTMLElement>('.mode-tab')!.getBoundingClientRect();
                check(menu.contains(doc.elementFromPoint(first.left + first.width / 2, first.top + first.height / 2)),
                    `${width}px: the mode menu stays above the precision controls`);
                if (width > height && height <= 500) {
                    const tabs = [...menu.querySelectorAll<HTMLElement>('.mode-tab')];
                    check(popup.bottom <= height && tabs.length === 7 && tabs.every(tab => {
                        const box = tab.getBoundingClientRect();
                        return box.top >= popup.top && box.bottom <= popup.bottom &&
                            tab.contains(doc.elementFromPoint(box.left + box.width / 2, box.top + box.height / 2));
                    }), `${width}x${height}: all seven landscape mode choices fit below the dropdown: ` +
                        JSON.stringify({selector, popup, tabs: tabs.map(tab => tab.getBoundingClientRect())}));
                }
            }
            const viewport = doc.documentElement.clientWidth;
            /** @brief Check horizontal bounds against this viewport, retaining the measured coordinates. */
            function inside(node: Element, name: string): void {
                const box = node.getBoundingClientRect();
                check(box.left >= -1 && box.right <= viewport + 1,
                      `${width}px: ${name} outside viewport (${box.left}, ${box.right}; ${viewport})`);
            }
            check(doc.documentElement.scrollWidth <= viewport + 1, `${width}px: page scrolls sideways`);
            if (width <= 700 || height <= 500) {
                const picker = doc.getElementById('marsDatePicker')!;
                const originalStyle = picker.getAttribute('style');
                const days = doc.getElementById('marsDatePickerGrid')!;
                if (!days.children.length) {
                    for (let day = 1; day <= 42; ++day) {
                        const button = doc.createElement('button');
                        button.className = 'mars-date-picker-day';
                        button.textContent = String(day);
                        days.append(button);
                    }
                }
                picker.classList.remove('hidden');
                picker.style.cssText = 'width:276px;left:12px;top:900px;max-height:8px';
                const popup = picker.getBoundingClientRect();
                check(picker.parentElement === doc.body &&
                    [...doc.querySelectorAll('body > header, body > main')].every(node =>
                        frame.contentWindow!.getComputedStyle(node).visibility === 'hidden'),
                    `${width}x${height}: the calendar is a separate view with the worksheet and header hidden`);
                const month_select = doc.getElementById('marsDatePickerMonth')!;
                const month_control = month_select.closest('.select-shell')?.querySelector('.select-button') ?? month_select;
                const monthBox = month_control.getBoundingClientRect();
                check(picker.contains(doc.elementFromPoint(monthBox.left + monthBox.width / 2,
                    monthBox.top + monthBox.height / 2)),
                    `${width}x${height}: the title and skin selector cannot cover the calendar month`);
                check(popup.left === 0 && popup.top === 0 && Math.abs(popup.width - width) < 1 &&
                    Math.abs(popup.height - height) < 1,
                    `${width}x${height}: mobile date picker fills the viewport despite an off-screen anchor ` +
                    JSON.stringify({left: popup.left, top: popup.top, width: popup.width, height: popup.height, viewport}));
                const footer = picker.querySelector('.mars-date-picker-foot')!.getBoundingClientRect();
                const landscape = width > height && height <= 500;
                const grid_bounds = days.getBoundingClientRect();
                days.scrollTop = days.scrollHeight;
                check(footer.bottom <= height &&
                    grid_bounds.bottom <= footer.top &&
                    picker.scrollHeight <= picker.clientHeight + 1,
                    `${width}x${height}: calendar and actions remain inside the viewport without overlap`);
                if (landscape) {
                    check(days.children.length === 42 && days.scrollHeight <= days.clientHeight + 1 &&
                        days.scrollWidth <= days.clientWidth + 1 && days.scrollTop === 0,
                        `${width}x${height}: all six calendar weeks fit without scrolling`);
                    const head = picker.querySelector('.mars-date-picker-head')!.getBoundingClientRect();
                    const weekdays = picker.querySelector('.mars-date-picker-weekdays')!.getBoundingClientRect();
                    check(head.bottom <= weekdays.top && weekdays.bottom <= grid_bounds.top &&
                        Math.abs(grid_bounds.left - weekdays.left) < 1 &&
                        Math.abs(grid_bounds.right - weekdays.right) < 1 && grid_bounds.width > width * .9,
                        `${width}x${height}: a single compact header sits above the full-width calendar`);
                    const previous = doc.getElementById('marsDatePickerPrev')!.getBoundingClientRect();
                    const next = doc.getElementById('marsDatePickerNext')!.getBoundingClientRect();
                    const year = picker.querySelector('.mars-date-picker-year-group')!.getBoundingClientRect();
                    check(previous.right <= monthBox.left && monthBox.right <= next.left && next.right <= year.left &&
                        Math.abs(previous.top - next.top) < 1 && Math.abs(next.top - year.top) < 1,
                        `${width}x${height}: the month arrows bracket the month before the separate year controls`);
                    const bottom_padding = parseFloat(frame.contentWindow!.getComputedStyle(picker).paddingBottom);
                    check(Math.abs(footer.bottom - height + bottom_padding) < 1 &&
                        Math.abs(footer.left - grid_bounds.left) < 1 && Math.abs(footer.right - grid_bounds.right) < 1,
                        `${width}x${height}: Now and Close stay at the bottom below all six weeks`);
                    for (const button of picker.querySelectorAll<HTMLElement>('.mars-date-picker-foot button'))
                        check(button.getBoundingClientRect().width < 96 && button.getBoundingClientRect().height <= 38,
                            `${width}x${height}: landscape actions stay compact`);
                    for (const button of days.querySelectorAll<HTMLElement>('.mars-date-picker-day')) {
                        const bounds = button.getBoundingClientRect();
                        check(bounds.top >= grid_bounds.top && bounds.bottom <= grid_bounds.bottom + 1,
                            `${width}x${height}: every date is fully visible`);
                    }
                }
                for (const button of picker.querySelectorAll<HTMLElement>('.mars-date-picker-day, .mars-date-picker-foot button')) {
                    const minimum_height = landscape ? (button.matches('.mars-date-picker-day') ? 24 : 36) : 44;
                    check(button.getBoundingClientRect().height >= minimum_height && button.scrollWidth <= button.clientWidth + 1,
                        `${width}x${height}: date choices and footer controls remain readable touch targets`);
                }
                for (const button of days.querySelectorAll<HTMLElement>('.mars-date-picker-day'))
                    check(button.getBoundingClientRect().height <= 48,
                        `${width}x${height}: calendar dates stay compact instead of stretching to fill the screen`);
                if (days.scrollHeight <= days.clientHeight + 1) {
                    const dates = days.querySelectorAll<HTMLElement>('.mars-date-picker-day');
                    const first = dates[0].getBoundingClientRect();
                    const last = dates[dates.length - 1].getBoundingClientRect();
                    check(Math.abs(first.top - grid_bounds.top - (grid_bounds.bottom - last.bottom)) <= 1,
                        `${width}x${height}: calendar rows use the available height with balanced outer spacing`);
                    if (!landscape)
                        check(last.bottom >= grid_bounds.bottom - 1,
                            `${width}x${height}: portrait dates extend towards the actions instead of clustering at the top`);
                    const row_step = dates[7].getBoundingClientRect().top - first.top;
                    for (let row = 2; row < 6; ++row)
                        check(Math.abs(dates[row * 7].getBoundingClientRect().top - first.top - row * row_step) <= 1,
                            `${width}x${height}: calendar rows have evenly distributed spacing`);
                }
                days.scrollTop = 0;
                picker.classList.add('hidden');
                check(frame.contentWindow!.getComputedStyle(doc.querySelector('body > header')!).visibility === 'visible',
                    `${width}x${height}: closing the calendar restores the worksheet header`);
                if (originalStyle === null) picker.removeAttribute('style');
                else picker.setAttribute('style', originalStyle);
            }
            const toolbar = doc.querySelector<HTMLElement>('.almanac-globe-toolbar');
            if (toolbar && visible(toolbar)) {
                const toolbar_height = toolbar.getBoundingClientRect().height;
                const history_notice = doc.getElementById('historyPersistenceStatus') ?? doc.createElement('p');
                history_notice.id = 'historyPersistenceStatus';
                history_notice.setAttribute('role', 'status');
                history_notice.textContent = 'A history status message that would otherwise wrap across several lines. '.repeat(4);
                toolbar.querySelector('.workspace-history')!.append(history_notice);
                check(!visible(history_notice) && toolbar.getBoundingClientRect().height === toolbar_height,
                    `${width}px: history status prose takes no mobile toolbar space`);
                const title = toolbar.querySelector('strong')!.getBoundingClientRect();
                const options = toolbar.querySelector<HTMLDetailsElement>('.almanac-globe-options')!;
                // The live globe applies the same breakpoint through matchMedia; this iframe only runs CSS.
                options.open = width > 560;
                const controls = toolbar.querySelector('.almanac-globe-view-controls')!;
                inside(toolbar, 'globe toolbar');
                check(width <= 560 || controls.getBoundingClientRect().top >= title.bottom,
                      `${width}px: globe view controls have their own row below the title and navigation`);
                if (width <= 560) {
                    const history = toolbar.querySelector<HTMLElement>('.workspace-history')!;
                    const navigation = [...history.querySelectorAll<HTMLButtonElement>('button')];
                    const heightBefore = toolbar.getBoundingClientRect().height;
                    const mapBefore = doc.querySelector('.almanac-globe-view')!.getBoundingClientRect();
                    const summary = options.querySelector<HTMLElement>('summary')!;
                    check(title.height === 1 && heightBefore <= 54 && !visible(controls),
                        `${width}px: the phone toolbar defaults to one compact row with map controls collapsed`);
                    check(navigation.length === 3 && navigation.every(button => {
                        const box = button.getBoundingClientRect(), first = navigation[0].getBoundingClientRect();
                        return Math.abs(box.top - first.top) < 1 && Math.abs(box.width - first.width) < 1;
                    }), `${width}px: Back, Forward and History share an evenly spaced row`);
                    check(Math.abs(summary.getBoundingClientRect().top - navigation[0].getBoundingClientRect().top) < 1,
                        `${width}px: Map shares the navigation row`);
                    inside(summary, 'Map control');
                    summary.click();
                    check(options.open && visible(controls) && toolbar.getBoundingClientRect().height === heightBefore &&
                        doc.querySelector('.almanac-globe-view')!.getBoundingClientRect().top === mapBefore.top,
                        `${width}px: opening Map reveals the controls without shrinking or moving the globe`);
                    const grid = controls.querySelector<HTMLElement>('[data-globe-action="grid"]')!.getBoundingClientRect();
                    const borders = controls.querySelector<HTMLElement>('[data-globe-action="borders"]')!.getBoundingClientRect();
                    const zoom = controls.querySelector<HTMLElement>('[data-globe-action="out"]')!.getBoundingClientRect();
                    check(Math.abs(grid.top - borders.top) < 1 && Math.abs(grid.width - borders.width) < 1 &&
                        zoom.top >= grid.bottom && zoom.width >= 36,
                        `${width}px: map toggles align above the grouped zoom and centring controls`);
                    for (const button of toolbar.querySelectorAll<HTMLButtonElement>('button'))
                        check(button.getBoundingClientRect().height >= 36 && button.scrollWidth <= button.clientWidth + 1,
                            `${width}px: compact globe buttons retain readable labels and usable touch targets`);
                }
                for (const button of toolbar.querySelectorAll('button'))
                    inside(button, 'globe control ' + button.textContent);
                if (width <= 560) {
                    options.querySelector<HTMLElement>('summary')!.click();
                    check(!options.open && !visible(controls), `${width}px: Map closes without leaving an empty panel`);
                }
            }
            for (const row of doc.querySelectorAll<HTMLElement>('.almanac-summary-row')) {
                if (!visible(row))
                    continue;
                const label = row.querySelector('dt')!, value = row.querySelector('dd')!;
                inside(row, 'Almanac summary row');
                check(row.scrollWidth <= row.clientWidth + 1, `${width}px: Almanac summary overflows`);
                const labelBox = label.getBoundingClientRect(), valueBox = value.getBoundingClientRect();
                check(valueBox.top >= labelBox.bottom,
                      `${width}px: Almanac summary values should sit below their labels`);
                const columns = frame.contentWindow!.getComputedStyle(row.parentElement!).gridTemplateColumns.split(' ');
                check(columns.length === (width <= 480 ? 2 : 3),
                      `${width}px: Almanac summary should use two phone columns or three wider columns`);
            }
            for (let i = 0; i < cards.length; ++i) {
                const card = cards[i];
                check(visible(card) === expected[i], `${width}px: visibility changed for card ${i} ` +
                      (card.querySelector<HTMLElement>('.card-title, .help-kicker')?.textContent?.trim() || card.id));
                if (visible(card))
                    inside(card, `card ${i}`);
            }
            for (const node of doc.querySelectorAll<HTMLElement>('.datetime-row, .datetime-row-value')) {
                if (visible(node))
                    inside(node, node.className);
            }
            for (const node of doc.querySelectorAll<HTMLElement>('.datetime-section-grid, .datetime-row')) {
                if (visible(node))
                    check(node.scrollWidth <= node.clientWidth + 1, `${width}px: ${node.className} clips content`);
            }
            if (width <= 560) {
                for (const table of doc.querySelectorAll<HTMLElement>('.almanac-body-table')) {
                    if (!visible(table))
                        continue;
                    check(table.scrollWidth <= table.clientWidth + 1, `${width}px: bodies need sideways scrolling`);
                    for (const cell of table.querySelectorAll<HTMLElement>('tbody td')) {
                        inside(cell, cell.dataset.label || 'body name');
                        check(cell.scrollWidth <= cell.clientWidth + 1,
                              `${width}px: clipped body field ${cell.dataset.label}`);
                    }
                }
            }
            for (const node of doc.querySelectorAll<HTMLElement>(
                     '.mode-tab, .precision-toolbar button, .panel-head button, .card-actions button, ' +
                     '.controls button, .variable-value-actions button, .plot-update, .select-button, .mars-date-button, ' +
                     'input:not([type="hidden"]):not(.select-native-source), textarea')) {
                if (!visible(node))
                    continue;
                inside(node, node.id || node.className);
                if (node.matches('.mars-date-button')) {
                    const style = frame.contentWindow!.getComputedStyle(node);
                    const icon = frame.contentWindow!.getComputedStyle(node, '::before');
                    check(parseFloat(style.borderTopRightRadius) <= 10 && style.boxShadow === 'none',
                          `${width}px: date button should be flat rather than a raised circular badge`);
                    check(parseFloat(icon.width) >= 22 && parseFloat(icon.height) >= 22,
                          `${width}px: calendar icon is too small to recognise`);
                }
                const card = node.closest('.result-card')!;
                if (card && node.matches('button')) {
                    const bounds = card.getBoundingClientRect(), box = node.getBoundingClientRect();
                    check(box.left >= bounds.left && box.right <= bounds.right,
                          `${width}px: button clipped by its card: ${node.id || node.textContent}`);
                }
                if (node.matches('.mode-tab, .card-action, .controls button, .mars-date-button')) {
                    const compact = (width <= 560 && !node.matches('.mars-date-button')) ||
                        (compact_modes && node.matches('.mode-tab, .precision-toolbar button'));
                    const precision = compact && node.closest('.precision-toolbar')!;
                    const worksheet = compact && node.matches('.controls button');
                    let minimumHeight = 43;
                    if (precision)
                        minimumHeight = 27;
                    else if (worksheet)
                        minimumHeight = 31;
                    else if (compact)
                        minimumHeight = 35;
                    check(node.getBoundingClientRect().height >= minimumHeight,
                          `${width}px: small touch target ${node.id}`);
                    if (compact && node.matches('.controls button, .panel-head button, .variable-value-actions button'))
                        check(node.getBoundingClientRect().height <= (worksheet ? 33 : 38),
                              `${width}px: mobile action has regained oversized padding`);
                }
                if (node.matches('textarea, input:not([type="radio"]):not([type="checkbox"]):not([type="range"])'))
                    check(parseFloat(frame.contentWindow!.getComputedStyle(node).fontSize) >= 16,
                          `${width}px: small editor font ${node.id}`);
            }
            let visibleModes = 0;
            for (const tab of doc.querySelectorAll<HTMLElement>('.mode-tab')) {
                if (visible(tab))
                    ++visibleModes;
            }
            check(visibleModes === 7, `${width}px: a mode is missing`);
            if (width <= 560) {
                const tabs = [...doc.querySelectorAll<HTMLElement>('.mode-tab')];
                const tabWidth = tabs[0].getBoundingClientRect().width;
                for (const tab of tabs)
                    check(Math.abs(tab.getBoundingClientRect().width - tabWidth) <= 1,
                          `${width}px: all mobile mode tabs should have the same width`);
                for (const node of doc.querySelectorAll<HTMLElement>(
                         '.mode-tab, :is(.datetime-controls, .almanac-controls) label, ' +
                         ':is(.datetime-controls, .almanac-controls) .datetime-field-group-title')) {
                    if (visible(node))
                        check(parseFloat(frame.contentWindow!.getComputedStyle(node).fontSize) === 12,
                              `${width}px: navigation and calendar labels should share a compact type size`);
                }
                for (const button of doc.querySelectorAll<HTMLElement>(
                         ':is(.datetime-controls, .almanac-controls) .select-button')) {
                    if (visible(button))
                        check(parseFloat(frame.contentWindow!.getComputedStyle(button).fontSize) === 14,
                              `${width}px: calendar selector text should be compact`);
                }
            }
            for (const id of ['equationControls', 'diffequationControls']) {
                const hint = doc.getElementById(id)!;
                if (hint.classList.contains('hidden'))
                    continue;
                check(visible(hint) === (width > 560 && visible(document.getElementById(id)!)),
                      `${width}px: introductory equation hint should appear only on larger screens`);
                const mode = id.replace('Controls', '');
                check(doc.querySelector<HTMLElement>(`.help-card[data-help-modes="${mode}"]`)?.textContent?.trim().length! > 0,
                      `${width}px: equation guidance must remain in Help`);
            }
            if (width <= 560) {
                for (const box of doc.querySelectorAll<HTMLElement>('.variable-value-box')) {
                    if (!visible(box))
                        continue;
                    const field = box.querySelector<HTMLElement>('.binding-value-field, .variable-value-text')!;
                    const actions = box.querySelector<HTMLElement>('.variable-value-actions')!;
                    if (field && actions && visible(actions)) {
                        const inputBox = field.getBoundingClientRect(), actionBox = actions.getBoundingClientRect();
                        check(actionBox.left >= inputBox.right && actionBox.top < inputBox.bottom,
                              `${width}px: binding actions should sit alongside the value`);
                    }
                }
            }
            const integrator = doc.getElementById('integratorControls')!;
            if (visible(integrator)) {
                check(visible(integrator.querySelector<HTMLElement>('.integrator-bound-guidance')!) === (width > 560),
                      `${width}px: Bound/Free guidance should appear only on larger screens`);
                check(visible(integrator.querySelector<HTMLElement>('.integrator-examples')!) === (width > 560),
                      `${width}px: integrator examples should appear only on larger screens`);
                check(visible(doc.getElementById('integratorBoundStack')!),
                      `${width}px: integrator rows must remain available`);
                check(visible(integrator.querySelector<HTMLElement>('label[for="integratorIntervalCap"]')!),
                      `${width}px: integrator work budget must remain available`);
            }
            const calendar = doc.getElementById('datetimeControls')!;
            if (visible(calendar)) {
                check(visible(calendar.querySelector<HTMLElement>('.datetime-offset-hint')!) === (width > 560),
                      `${width}px: GMT offset explanation should appear only on larger screens`);
                check(visible(doc.getElementById('datetimeGmtOffset')!),
                      `${width}px: GMT offset control must remain available`);
                const date = doc.getElementById('datetimeDate')!.getBoundingClientRect();
                const year = doc.getElementById('datetimeYear')!.getBoundingClientRect();
                const jdn = doc.getElementById('datetimeJdn')!.getBoundingClientRect();
                check(!calendar.querySelector<HTMLElement>('.datetime-briefing, .datetime-orbit')!,
                      'decorative calendar banner has returned');
                check(calendar.querySelector<HTMLElement>('.datetime-field-group input')?.id === 'datetimeDate',
                      'date must be first in reading and keyboard order');
                for (const selector of ['.selected-jdn-field', '.selected-year-field'])
                    check(visible(calendar.querySelector<HTMLElement>(selector)!) === (width > 560),
                          `${width}px: secondary date fields should appear only on larger screens`);
                check(visible(doc.getElementById('datetimeDate')!),
                      `${width}px: selected date must remain editable`);
                if (width > 560)
                    check(Math.abs(year.top - jdn.top) < 1 && date.bottom <= year.top,
                          `${width}px: Julian day and year should share a row below the date`);
                for (const group of calendar.querySelectorAll<HTMLElement>('.datetime-field-group'))
                    check(frame.contentWindow!.getComputedStyle(group).borderRadius === '0px',
                          `${width}px: nested decorative calendar card`);
                for (const shell of calendar.querySelectorAll<HTMLElement>('.mars-date-shell')) {
                    const button = shell.querySelector<HTMLElement>('.mars-date-button')!;
                    const icon = frame.contentWindow!.getComputedStyle(button, '::before');
                    check(button.getAttribute('aria-label')?.includes('date picker') &&
                          icon.maskImage.includes('svg') && icon.content === '""',
                          `${width}px: date picker needs a labelled vector calendar icon`);
                    check(Math.abs(button.getBoundingClientRect().right - shell.getBoundingClientRect().right) <= 2,
                          `${width}px: date picker is not integrated with the date input`);
                }
                if (width <= 560) {
                    check(date.bottom < 460, `${width}px: date input is buried below the introduction`);
                    const picker = doc.getElementById('marsDatePicker')!;
                    const pickerStyle = picker.getAttribute('style');
                    picker.classList.remove('hidden');
                    picker.style.cssText = `width:${viewport - 24}px;left:12px;top:12px;max-height:${height - 24}px`;
                    const popup = picker.getBoundingClientRect();
                    const monthSelect = doc.getElementById('marsDatePickerMonth')!;
                    const monthButton = monthSelect.closest('.select-shell')?.querySelector('.select-button') ?? monthSelect;
                    const month = monthButton.getBoundingClientRect();
                    const yearInput = doc.getElementById('marsDatePickerYear')!;
                    const yearBox = yearInput.getBoundingClientRect();
                    check(yearBox.top >= month.bottom && yearBox.width >= 80,
                          `${width}px: year entry needs its own usable row below the month`);
                    check(parseFloat(frame.contentWindow!.getComputedStyle(yearInput).fontSize) >= 16,
                          `${width}px: year entry must not trigger mobile focus zoom`);
                    for (const control of picker.querySelectorAll<HTMLElement>('.mars-date-picker-head button, ' +
                                                                  '.mars-date-picker-head input, ' +
                                                                  '.mars-date-picker-head select:not(.select-native-source)')) {
                        if (!visible(control))
                            continue;
                        const box = control.getBoundingClientRect();
                        check(box.left >= popup.left && box.right <= popup.right && box.height >= 43,
                              `${width}px: calendar header control is clipped or too small: ` +
                              JSON.stringify({id: control.id, className: control.className, box, popup}));
                    }
                    const down = doc.getElementById('marsDatePickerYearDown')!.getBoundingClientRect();
                    const up = doc.getElementById('marsDatePickerYearUp')!.getBoundingClientRect();
                    check(down.right < yearBox.left && yearBox.right < up.left,
                          `${width}px: year step buttons overlap the editable year`);
                    picker.classList.add('hidden');
                    if (pickerStyle === null)
                        picker.removeAttribute('style');
                    else
                        picker.setAttribute('style', pickerStyle);
                    const from = doc.getElementById('datetimeStart')!.closest('.integrator-bound-field')!;
                    const to = doc.getElementById('datetimeEnd')!.closest('.integrator-bound-field')!;
                    check(to.getBoundingClientRect().bottom - from.getBoundingClientRect().top <= 108,
                          `${width}px: date range should be two compact rows`);
                    for (const field of [from, to]) {
                        const label = field.querySelector<HTMLElement>('label')!.getBoundingClientRect();
                        const input = field.querySelector<HTMLElement>('input')!.getBoundingClientRect();
                        check(label.right < input.left && label.top >= input.top && label.bottom <= input.bottom,
                              `${width}px: range label should sit beside its input`);
                    }
                }
            }
            if (compact_modes) {
                const slider = doc.getElementById('precisionSlider')! as HTMLInputElement;
                const track = slider.getBoundingClientRect();
                const modeToggle = doc.getElementById('modeMenuToggle')!.getBoundingClientRect();
                const precisionControl = doc.querySelector<HTMLElement>('.precision-toolbar')!;
                const precision = precisionControl.getBoundingClientRect();
                const precisionVisible = visible(precisionControl);
                check(precisionVisible === !doc.body.matches('.datetime-mode, .almanac-mode'),
                      `${width}px: calendar modes hide the precision controls`);
                if (precisionVisible) {
                    check(precision.left >= modeToggle.right && precision.top < modeToggle.bottom &&
                        modeToggle.top < precision.bottom,
                          `${width}px: precision controls stay beside Mode`);
                }
                if (precisionVisible) {
                    check(track.width >= precision.width - 2 && track.height <= 24 && precision.height <= 52,
                          `${width}px: precision slider fills its compact two-column slot`);
                }
                check(slider.type === 'range' && slider.labels?.[0]?.textContent === 'Precision' &&
                      slider.getAttribute('aria-describedby') === 'precisionValue',
                      `${width}px: precision slider retains its label and value description`);
                const precisionValue = doc.getElementById('precisionValue')!;
                precisionValue.textContent = '2004 digits';
                if (precisionVisible) {
                    inside(precisionValue, 'maximum precision readout');
                    check(precisionValue.getBoundingClientRect().right <=
                        doc.getElementById('precisionCustomToggle')!.getBoundingClientRect().left,
                          `${width}px: maximum precision readout does not overlap its action`);
                }
                const headerBottom = doc.querySelector<HTMLElement>('.lab-topbar')!.getBoundingClientRect().bottom;
                check(headerBottom < 290,
                      `${width}px: header/navigation takes too much vertical space (${headerBottom}px)`);
            }
            const almanac = doc.getElementById('almanacControls')!;
            if (visible(almanac)) {
                /** @brief Measure one Almanac control within the isolated document. */
                function bounds(id: string): DOMRect {
                    return doc.getElementById(id)!.getBoundingClientRect();
                }
                const date = bounds('almanacDate'), time = bounds('almanacTime'), zone = bounds('almanacZone');
                check(Math.abs(time.top - zone.top) < 1 && time.right < zone.left && date.bottom <= time.top,
                      `${width}px: GMT time and zone should share a row below the date`);
                const latitude = bounds('almanacLatitude'), longitude = bounds('almanacLongitude');
                check(Math.abs(latitude.top - longitude.top) < 1 && latitude.right < longitude.left,
                      `${width}px: observer coordinates should share a row`);
                for (const group of almanac.querySelectorAll<HTMLElement>('.datetime-field-group'))
                    check(frame.contentWindow!.getComputedStyle(group).borderRadius === '0px',
                          `${width}px: Almanac has nested decorative panels`);
                for (const id of ['almanacDate', 'almanacTime', 'almanacZone', 'almanacJurisdiction',
                                  'almanacTown', 'almanacLatitude', 'almanacLongitude', 'almanacElevation'])
                    check(visible(doc.getElementById(id)!), `${width}px: missing Almanac field ${id}`);
                check(visible(almanac.querySelector<HTMLElement>('.almanac-accuracy-note')!) === (width > 560),
                      `${width}px: ephemeris coverage and accuracy note should appear only on larger screens`);
                if (width <= 560) {
                    check(!almanac.querySelector('.almanac-briefing'),
                          `${width}px: repeated Almanac introduction takes up phone space`);
                    const globe = bounds('almanacGlobe');
                    const actions = bounds('almanacActions');
                    const help = bounds('help');
                    check(actions.top >= globe.bottom && actions.top - globe.bottom < 16 &&
                        Math.abs(actions.height - help.height) < 1,
                          `${width}px: Help should sit compactly below the globe ` +
                          `(gap ${actions.top - globe.bottom}px, row ${actions.height}px, button ${help.height}px)`);
                    check(bounds('workspaceActionsHome').height === 0,
                          `${width}px: relocated actions should leave no empty mobile spacer`);
                    check(doc.getElementById('back')!.closest('.almanac-globe-toolbar') &&
                          doc.getElementById('forward')!.closest('.almanac-globe-toolbar'),
                          `${width}px: history navigation belongs in the globe toolbar`);
                    check(date.top >= actions.bottom && date.bottom - actions.bottom < 160,
                          `${width}px: Almanac moment controls should sit compactly below Help`);
                    check(zone.bottom - date.top < 124, `${width}px: moment controls are too tall`);
                }
            }
            const mobile = doc.querySelector<HTMLDetailsElement>('#mobileAccess')!;
            check(!visible(mobile), `${width}px: Wi-Fi access card should be hidden on mobile`);
            mobile.open = true;
            check(!visible(mobile.querySelector<HTMLElement>('.mobile-panel')!),
                  `${width}px: an open Wi-Fi access card should remain hidden on mobile`);
            mobile.open = false;
            if (compact_modes) {
                doc.getElementById('modeMenuToggle')!.click();
                check(!modeMenu.open && !visible(doc.querySelector('.lab-tabs')!), `${width}px: Mode collapses again`);
            }
        }
        if (state === 'all calendar result cards') {
            // Exercise the production touch rule even when the runner has a mouse.
            const imports = [...doc.styleSheets[0].cssRules];
            const buttons = (imports.find(isButtonStylesheet) as CSSImportRule).styleSheet!;
            const touch = [...buttons.cssRules].find(isTouchRule) as CSSMediaRule;
            check(touch?.conditionText.includes('pointer: coarse'), 'touch tooltip media rule missing');
            const style = doc.createElement('style');
            const touchRules: string[] = [];
            for (const rule of touch.cssRules)
                touchRules.push(rule.cssText);
            style.textContent = touchRules.join('\n');
            doc.head.appendChild(style);
            const tooltip = doc.createElement('div');
            tooltip.className = 'mars-button-tooltip visible';
            tooltip.textContent = 'Open date picker';
            doc.body.appendChild(tooltip);
            check(frame.contentWindow!.getComputedStyle(tooltip).display === 'none',
                  'touch hover tooltip must not cover subsequent controls');
            const button = doc.querySelector<HTMLElement>('[data-date-target="datetimeDate"]')!;
            button.focus();
            check(button.matches(':focus-visible') &&
                  frame.contentWindow!.getComputedStyle(tooltip).display !== 'none',
                  'keyboard-focused buttons must retain their tooltip on touch layouts');
            const longName = 'Llanfairpwllgwyngyllgogerychwyrndrobwllllantysiliogogogoch, Wales';
            tooltip.textContent = longName;
            tooltip.style.left = '8px';
            tooltip.style.top = '8px';
            const tooltipRect = tooltip.getBoundingClientRect();
            check(tooltip.textContent === longName && tooltip.scrollWidth <= tooltip.clientWidth + 1,
                  'the complete unbroken town name must wrap inside its tooltip');
            check(tooltipRect.right <= frame.contentWindow!.innerWidth &&
                  tooltip.scrollHeight <= tooltip.clientHeight + 1,
                  'the full tooltip must fit without horizontal or vertical cropping');
        }
    } finally {
        frame.remove();
    }
};
