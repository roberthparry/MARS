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
window.checkLabMobileLayout = async function checkLabMobileLayout(state: string): Promise<void> {
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
        for (const source of document.querySelectorAll<HTMLElement>('body > header, body > main'))
            doc.body.appendChild(source.cloneNode(true));
        const cards = [...doc.querySelectorAll<HTMLElement>(selectors)];
        check(cards.length === originals.length, 'complete card inventory copied');
        const viewports: MobileLayoutViewport[] = [
            {width: 320, height: 568},
            {width: 390, height: 844},
            {width: 430, height: 932},
            {width: 768, height: 1024},
            {width: 844, height: 390}
        ];
        for (const {width, height} of viewports) {
            frame.style.width = width + 'px';
            frame.style.height = height + 'px';
            const viewport = doc.documentElement.clientWidth;
            /** @brief Check horizontal bounds against this viewport, retaining the measured coordinates. */
            function inside(node: Element, name: string): void {
                const box = node.getBoundingClientRect();
                check(box.left >= -1 && box.right <= viewport + 1,
                      `${width}px: ${name} outside viewport (${box.left}, ${box.right}; ${viewport})`);
            }
            check(doc.documentElement.scrollWidth <= viewport + 1, `${width}px: page scrolls sideways`);
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
                    const compact = width <= 560 && !node.matches('.mars-date-button');
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
                    const month = doc.getElementById('marsDatePickerMonth')!.getBoundingClientRect();
                    const yearInput = doc.getElementById('marsDatePickerYear')!;
                    const yearBox = yearInput.getBoundingClientRect();
                    check(yearBox.top >= month.bottom && yearBox.width >= 80,
                          `${width}px: year entry needs its own usable row below the month`);
                    check(parseFloat(frame.contentWindow!.getComputedStyle(yearInput).fontSize) >= 16,
                          `${width}px: year entry must not trigger mobile focus zoom`);
                    for (const control of picker.querySelectorAll<HTMLElement>('.mars-date-picker-head button, ' +
                                                                  '.mars-date-picker-head input, ' +
                                                                  '.mars-date-picker-head select')) {
                        const box = control.getBoundingClientRect();
                        check(box.left >= popup.left && box.right <= popup.right && box.height >= 43,
                              `${width}px: calendar header control is clipped or too small: ${control.id}`);
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
            if (width <= 560) {
                const slider = doc.getElementById('precisionSlider')! as HTMLInputElement;
                const track = slider.getBoundingClientRect();
                const expressionTab = doc.getElementById('modeTabExpression')!.getBoundingClientRect();
                const precision = doc.querySelector<HTMLElement>('.precision-toolbar')!.getBoundingClientRect();
                check(precision.right < expressionTab.left &&
                      Math.abs((precision.top + precision.bottom) - (expressionTab.top + expressionTab.bottom)) < 2,
                      `${width}px: precision controls should sit to the left of Expression on the first row`);
                const otherTabs = [...doc.querySelectorAll<HTMLElement>('.mode-tab')].slice(1);
                const rowTops = new Set<number>();
                let belowExpression = true;
                for (const tab of otherTabs) {
                    const top = tab.getBoundingClientRect().top;
                    rowTops.add(Math.round(top));
                    if (!(top >= expressionTab.bottom))
                        belowExpression = false;
                }
                check(rowTops.size === 2 && belowExpression,
                      `${width}px: remaining modes should form two rows below Expression`);
                check(track.width >= precision.width - 2 && track.height <= 24 && precision.height <= 52,
                      `${width}px: precision slider fills its compact two-column slot`);
                check(slider.type === 'range' && slider.labels?.[0]?.textContent === 'Precision' &&
                      slider.getAttribute('aria-describedby') === 'precisionValue',
                      `${width}px: precision slider retains its label and value description`);
                const precisionValue = doc.getElementById('precisionValue')!;
                precisionValue.textContent = '2004 digits';
                inside(precisionValue, 'maximum precision readout');
                check(precisionValue.getBoundingClientRect().left >= doc.querySelector<HTMLElement>('.precision-label')!.getBoundingClientRect().right,
                      `${width}px: maximum precision readout does not overlap its label`);
                check(doc.querySelector<HTMLElement>('.lab-topbar')!.getBoundingClientRect().bottom < 290,
                      `${width}px: header/navigation takes too much vertical space`);
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
                    check(!visible(almanac.querySelector<HTMLElement>('.almanac-briefing')!),
                          `${width}px: repeated Almanac introduction takes up phone space`);
                    const globe = bounds('almanacGlobe');
                    check(date.top >= globe.bottom && date.bottom - globe.bottom < 160,
                          `${width}px: Almanac moment controls should sit compactly below the globe`);
                    check(zone.bottom - date.top < 124, `${width}px: moment controls are too tall`);
                }
            }
            const mobile = doc.querySelector<HTMLDetailsElement>('#mobileAccess')!;
            check(!visible(mobile), `${width}px: Wi-Fi access card should be hidden on mobile`);
            mobile.open = true;
            check(!visible(mobile.querySelector<HTMLElement>('.mobile-panel')!),
                  `${width}px: an open Wi-Fi access card should remain hidden on mobile`);
            mobile.open = false;
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
        }
    } finally {
        frame.remove();
    }
};
