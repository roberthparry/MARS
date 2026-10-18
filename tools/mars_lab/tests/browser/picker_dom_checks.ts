/**
 * @file picker_dom_checks.ts
 * @brief Date-picker projection and deferred browser-effect regressions.
 *
 * Exercises the native picker through live DOM fixtures and checks projection and service ordering.
 */

/** Native date-picker projection and deferred browser-effect checks; the parent runs these sequentially. */
window.checkLabPickerDom = function checkLabPickerDom() {
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error(`Picker DOM: ${message}`);
    }

    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }
    const native = labWire.exports();
    const saved = {
        input: WorksheetContext.marsDatePickerState.input,
        button: WorksheetContext.marsDatePickerState.button,
        shell: WorksheetContext.marsDatePickerState.shell,
        packed: native.lab_forms_picker_date(),
        today: WorksheetLocations.dates.marsTodayIsoDate,
        moment: WorksheetLocations.dates.marsCurrentGmtMoment,
        focus: document.activeElement,
        date: WorksheetContext.almanacDate.value,
        time: WorksheetContext.almanacTime.value
    };
    const snapshots: BindingElementSnapshot[] = [];
    const elements = [
        WorksheetContext.marsDatePicker, WorksheetContext.marsDatePickerGrid,
        WorksheetContext.marsDatePickerWeekdays, WorksheetContext.marsDatePickerMonth, WorksheetContext.marsDatePickerYear
    ];
    for (const element of elements) {
        const children = [...element.childNodes];
        const value = (element as LabWorksheetTestSnapshotElement).value;
        const attributes: [string, string][] = [];
        for (const attribute of element.attributes)
            attributes.push([attribute.name, attribute.value]);
        snapshots.push({element, children, value, attributes});
    }
    const root = document.createElement('div');
    document.body.appendChild(root);
    function makeShell() {
        const shell = document.createElement('div');
        shell.className = 'mars-date-shell';
        const input = document.createElement('input');
        input.type = 'date';
        const button = document.createElement('button');
        button.type = 'button';
        button.dataset.dateTarget = 'picker-dom-fixture';
        shell.append(input, button);
        root.appendChild(shell);
        return {shell, input, button};
    }
    const first = makeShell();
    const second = makeShell();
    function box(node: HTMLElement, left: number, top: number, width: number, height: number): void {
        node.getBoundingClientRect = function fixtureRect(): DOMRect {
            return {left, top, width, height, right: left + width, bottom: top + height} as DOMRect;
        };
    }
    const moment = {date: '2024-03-01', time: '00:01:02'};
    const almanacEvents: {target: EventTarget | null; date: string; time: string}[] = [];
    function intercept(event: Event): void {
        if (event.target !== WorksheetContext.almanacDate && event.target !== WorksheetContext.almanacTime)
            return;
        almanacEvents.push({target: event.target, date: WorksheetContext.almanacDate.value, time: WorksheetContext.almanacTime.value});
        event.stopImmediatePropagation();
        // Deferred events must be allowed to start another native DOM call.
        marsDatePickerAnchorRect(first.shell);
    }
    document.addEventListener('change', intercept, true);
    const oldShellClass = saved.shell?.getAttribute('class');
    try {
        Reflect.set(WorksheetLocations.dates, 'marsTodayIsoDate', () => '2024-02-29');
        Reflect.set(WorksheetLocations.dates, 'marsCurrentGmtMoment', () => moment);
        equal(WorksheetLocations.picker.todayForInput(first.input), '2024-02-29', 'ordinary inputs use local date');
        equal(WorksheetLocations.picker.todayForInput(WorksheetContext.almanacDate), '2024-03-01', 'almanac uses GMT date');
        equal(marsDatePickerAnchorRect(null), null, 'missing shell has no anchor');
        box(first.shell, 20, 30, 500, 80);
        box(first.input, 30, 40, 300, 30);
        box(first.button, 350, 45, 40, 20);
        let rect = marsDatePickerAnchorRect(first.shell)!;
        equal(rect.left, 20, 'visible shell takes precedence');
        equal(rect.width, 500, 'visible shell width');
        box(first.shell, 0, 0, 0, 0);
        rect = marsDatePickerAnchorRect(first.shell)!;
        equal(rect.left, 30, 'display:contents union left');
        equal(rect.right, 390, 'display:contents union right');
        equal(rect.top, 40, 'display:contents union top');
        equal(rect.bottom, 70, 'display:contents union bottom');
        equal(rect.width, 360, 'display:contents union width');
        equal(rect.height, 30, 'display:contents union height');
        box(first.input, 30, 40, 1, 30);
        equal(marsDatePickerAnchorRect(first.shell)!.width, 40, 'degenerate input excluded');
        box(first.button, 0, 0, 0, 0);
        equal(marsDatePickerAnchorRect(first.shell), null, 'no visible controls means no anchor');
        box(first.input, 700, 100, 100, 30);
        box(first.button, 810, 110, 40, 40);
        WorksheetContext.marsDatePickerState.button = first.button;
        labDOM.call('lab_widgets_picker_place', WorksheetContext.marsDatePickerState, first.shell, 800, 600);
        equal(WorksheetContext.marsDatePicker.style.width, '448px', 'native minimum width');
        equal(WorksheetContext.marsDatePicker.style.left, '340px', 'right viewport clamp');
        equal(WorksheetContext.marsDatePicker.style.top, '158px', 'button bottom anchors popup');
        equal(WorksheetContext.marsDatePicker.style.maxHeight, '430px', 'available height');
        labDOM.call('lab_widgets_picker_place', WorksheetContext.marsDatePickerState, first.shell, 200, 120);
        equal(WorksheetContext.marsDatePicker.style.width, '176px', 'narrow viewport width');
        equal(WorksheetContext.marsDatePicker.style.left, '12px', 'narrow viewport margin');
        equal(WorksheetContext.marsDatePicker.style.maxHeight, '8px', 'minimum scrollable height');
        const style = WorksheetContext.marsDatePicker.getAttribute('style');
        labDOM.call('lab_widgets_picker_place', WorksheetContext.marsDatePickerState, first.shell, NaN, 600);
        equal(WorksheetContext.marsDatePicker.getAttribute('style'), style, 'invalid viewport preserves placement');
        WorksheetLocations.picker.place(null as unknown as HTMLElement);
        equal(WorksheetContext.marsDatePicker.getAttribute('style'), style, 'missing anchor preserves placement');

        first.input.value = '2024-01-31';
        WorksheetLocations.picker.open(first.input, first.button);
        equal(WorksheetContext.marsDatePickerState.input, first.input, 'open retains host input reference');
        equal(WorksheetContext.marsDatePickerState.shell, first.shell, 'open retains host shell reference');
        check(
            first.shell.classList.contains('open') && !WorksheetContext.marsDatePicker.classList.contains('hidden'), 'open projection');
        equal(WorksheetContext.marsDatePickerState.year, 2024, 'native selected year');
        equal(WorksheetContext.marsDatePickerState.month, 1, 'native selected month');
        check(
            WorksheetContext.marsDatePickerGrid.querySelector<HTMLElement>('[data-iso-date="2024-01-31"]')!.classList.contains('selected'),
            'native selected-day projection');
        let changes = 0;
        first.input.addEventListener('change', function recordDateChange() {
            ++changes;
            marsDatePickerAnchorRect(first.shell);
        });
        equal(WorksheetLocations.picker.commit(first.input, '2024-01-31'), false, 'unchanged commit');
        equal(changes, 0, 'unchanged commit emits no event');
        WorksheetLocations.picker.shiftMonth(1);
        equal(first.input.value, '2024-02-29', 'month navigation clamps end-of-month');
        equal(changes, 1, 'month navigation emits one deferred change');
        WorksheetLocations.picker.shiftYear(1);
        equal(first.input.value, '2025-02-28', 'year navigation clamps leap day');
        WorksheetLocations.picker.setMonthYear(2026, 3);
        equal(first.input.value, '2025-02-28', 'noncommitting navigation preserves authored input');
        equal(WorksheetContext.marsDatePickerState.year, 2026, 'noncommitting navigation updates native view');
        WorksheetLocations.picker.setMonthYear(2024, 2, {commit: true});
        equal(first.input.value, '2024-02-28', 'committing month/year preserves current input day');
        document.getElementById('marsDatePickerYearUp')!.click();
        equal(first.input.value, '2025-02-28', 'year plus button commits through its event handler');
        WorksheetContext.marsDatePickerYear.value = '2030';
        WorksheetContext.marsDatePickerYear.dispatchEvent(new Event('change', {bubbles: true}));
        equal(first.input.value, '2030-02-28', 'typed year commits through its event handler');
        document.getElementById('marsDatePickerYearDown')!.click();
        equal(first.input.value, '2029-02-28', 'year minus button commits through its event handler');
        WorksheetLocations.picker.setMonthYear(2024, 2, {commit: true});

        second.input.value = '';
        WorksheetLocations.picker.open(second.input, second.button);
        check(
            !first.shell.classList.contains('open') && second.shell.classList.contains('open'),
            'opening another input transfers shell ownership');
        equal(WorksheetContext.marsDatePickerState.month, 2, 'empty input starts at local today');
        equal(WorksheetContext.marsDatePickerState.year, 2024, 'empty input uses local today year');
        WorksheetLocations.picker.open(null as unknown as HTMLInputElement, second.button);
        equal(WorksheetContext.marsDatePickerState.input, second.input, 'invalid open preserves ownership');
        let focusSawClosed = false;
        second.button.addEventListener('focus', function observeClosedPicker() {
            focusSawClosed = WorksheetContext.marsDatePickerState.input === null && native.lab_forms_picker_date() === 0;
            marsDatePickerAnchorRect(second.shell);
        });
        second.input.focus();
        WorksheetLocations.picker.close({restoreFocus: true});
        equal(document.activeElement, second.button, 'close restores button focus');
        check(focusSawClosed, 'focus event sees fully closed native and host state');
        check(
            WorksheetContext.marsDatePicker.classList.contains('hidden') && !second.shell.classList.contains('open'),
            'closed projection');
        equal(WorksheetContext.marsDatePickerState.button, null, 'closed button reference released');
        equal(WorksheetContext.marsDatePickerState.shell, null, 'closed shell reference released');
        WorksheetLocations.picker.shiftMonth(1);
        WorksheetLocations.picker.render();
        check(WorksheetContext.marsDatePicker.classList.contains('hidden'), 'stale navigation/render cannot reopen picker');

        WorksheetLocations.picker.commitToday(first.input);
        equal(first.input.value, '2024-02-29', 'ordinary Today commits local date');
        WorksheetContext.almanacDate.value = '2024-02-28';
        WorksheetContext.almanacTime.value = '12:00:00';
        WorksheetLocations.picker.commitToday(WorksheetContext.almanacDate);
        equal(almanacEvents.length, 1, 'changed GMT date and time emit one event');
        equal(almanacEvents[0].target, WorksheetContext.almanacDate, 'date event takes precedence');
        equal(almanacEvents[0].date, moment.date, 'event observes new GMT date');
        equal(almanacEvents[0].time, moment.time, 'event observes new GMT time');
        WorksheetLocations.picker.commitToday(WorksheetContext.almanacDate);
        equal(almanacEvents.length, 1, 'unchanged GMT moment emits no event');
        WorksheetContext.almanacTime.value = '12:00:00';
        WorksheetLocations.picker.commitToday(WorksheetContext.almanacDate);
        equal(almanacEvents.length, 2, 'time-only Today emits one event');
        equal(almanacEvents[1].target, WorksheetContext.almanacTime, 'time-only change targets time input');
        WorksheetLocations.picker.commitToday(null as unknown as HTMLInputElement);
        equal(almanacEvents.length, 2, 'missing Today input is inert');
    } finally {
        document.removeEventListener('change', intercept, true);
        Reflect.set(WorksheetLocations.dates, 'marsTodayIsoDate', saved.today);
        Reflect.set(WorksheetLocations.dates, 'marsCurrentGmtMoment', saved.moment);
        WorksheetContext.almanacDate.value = saved.date;
        WorksheetContext.almanacTime.value = saved.time;
        WorksheetResults.tooltip.hide();
        root.remove();
        WorksheetContext.marsDatePickerState.input = saved.input;
        WorksheetContext.marsDatePickerState.button = saved.button;
        WorksheetContext.marsDatePickerState.shell = saved.shell;
        if (saved.packed)
            native.lab_forms_picker_open(
                native.lab_forms_date_year(saved.packed), native.lab_forms_date_month(saved.packed), 2024);
        else
            native.lab_forms_picker_close();
        if (saved.shell) {
            if (oldShellClass === null)
                saved.shell.removeAttribute('class');
            else
                saved.shell.setAttribute('class', oldShellClass!);
        }
        for (const {element, children, value, attributes} of snapshots) {
            for (const attribute of [...element.attributes])
                element.removeAttribute(attribute.name);
            for (const [name, content] of attributes)
                element.setAttribute(name, content);
            element.replaceChildren(...children);
            if (value !== undefined)
                element.value = value;
        }
        (saved.focus as HTMLElement | null)?.focus();
    }
};
