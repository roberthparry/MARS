/** Native date-picker projection and deferred browser-effect checks; the parent runs these sequentially. */
window.checkLabPickerDom = function checkLabPickerDom() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error(`Picker DOM: ${message}`);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const native = labWire.exports();
    const saved = {
        input: marsDatePickerState.input,
        button: marsDatePickerState.button,
        shell: marsDatePickerState.shell,
        packed: native.lab_forms_picker_date(),
        today: marsTodayIsoDate,
        moment: marsCurrentGmtMoment,
        focus: document.activeElement,
        date: almanacDate.value,
        time: almanacTime.value
    };
    const snapshots = [
        marsDatePicker, marsDatePickerGrid, marsDatePickerWeekdays, marsDatePickerMonth, marsDatePickerYear
    ].map(node => ({
              node,
              children: [...node.childNodes],
              value: node.value,
              attributes: [...node.attributes].map(attribute => [attribute.name, attribute.value])
          }));
    const root = document.createElement('div');
    document.body.appendChild(root);
    const makeShell = () => {
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
    };
    const first = makeShell(), second = makeShell();
    const box = (node, left, top, width, height) => node.getBoundingClientRect = () =>
        ({left, top, width, height, right: left + width, bottom: top + height});
    const moment = {date: '2024-03-01', time: '00:01:02'};
    const almanacEvents = [];
    const intercept = event => {
        if (event.target !== almanacDate && event.target !== almanacTime)
            return;
        almanacEvents.push({target: event.target, date: almanacDate.value, time: almanacTime.value});
        event.stopImmediatePropagation();
        // Deferred events must be allowed to start another native DOM call.
        marsDatePickerAnchorRect(first.shell);
    };
    document.addEventListener('change', intercept, true);
    const oldShellClass = saved.shell?.getAttribute('class');
    try {
        marsTodayIsoDate = () => '2024-02-29';
        marsCurrentGmtMoment = () => moment;
        equal(marsTodayIsoDateForInput(first.input), '2024-02-29', 'ordinary inputs use local date');
        equal(marsTodayIsoDateForInput(almanacDate), '2024-03-01', 'almanac uses GMT date');
        equal(marsDatePickerAnchorRect(null), null, 'missing shell has no anchor');
        box(first.shell, 20, 30, 500, 80);
        box(first.input, 30, 40, 300, 30);
        box(first.button, 350, 45, 40, 20);
        let rect = marsDatePickerAnchorRect(first.shell);
        equal(rect.left, 20, 'visible shell takes precedence');
        equal(rect.width, 500, 'visible shell width');
        box(first.shell, 0, 0, 0, 0);
        rect = marsDatePickerAnchorRect(first.shell);
        equal(rect.left, 30, 'display:contents union left');
        equal(rect.right, 390, 'display:contents union right');
        equal(rect.top, 40, 'display:contents union top');
        equal(rect.bottom, 70, 'display:contents union bottom');
        equal(rect.width, 360, 'display:contents union width');
        equal(rect.height, 30, 'display:contents union height');
        box(first.input, 30, 40, 1, 30);
        equal(marsDatePickerAnchorRect(first.shell).width, 40, 'degenerate input excluded');
        box(first.button, 0, 0, 0, 0);
        equal(marsDatePickerAnchorRect(first.shell), null, 'no visible controls means no anchor');
        box(first.input, 700, 100, 100, 30);
        box(first.button, 810, 110, 40, 40);
        marsDatePickerState.button = first.button;
        labDOM.call('lab_widgets_picker_place', marsDatePickerState, first.shell, 800, 600);
        equal(marsDatePicker.style.width, '448px', 'native minimum width');
        equal(marsDatePicker.style.left, '340px', 'right viewport clamp');
        equal(marsDatePicker.style.top, '158px', 'button bottom anchors popup');
        equal(marsDatePicker.style.maxHeight, '430px', 'available height');
        labDOM.call('lab_widgets_picker_place', marsDatePickerState, first.shell, 200, 120);
        equal(marsDatePicker.style.width, '176px', 'narrow viewport width');
        equal(marsDatePicker.style.left, '12px', 'narrow viewport margin');
        equal(marsDatePicker.style.maxHeight, '8px', 'minimum scrollable height');
        const style = marsDatePicker.getAttribute('style');
        labDOM.call('lab_widgets_picker_place', marsDatePickerState, first.shell, NaN, 600);
        equal(marsDatePicker.getAttribute('style'), style, 'invalid viewport preserves placement');
        placeMarsDatePicker(null);
        equal(marsDatePicker.getAttribute('style'), style, 'missing anchor preserves placement');

        first.input.value = '2024-01-31';
        openMarsDatePicker(first.input, first.button);
        equal(marsDatePickerState.input, first.input, 'open retains host input reference');
        equal(marsDatePickerState.shell, first.shell, 'open retains host shell reference');
        check(
            first.shell.classList.contains('open') && !marsDatePicker.classList.contains('hidden'), 'open projection');
        equal(marsDatePickerState.year, 2024, 'native selected year');
        equal(marsDatePickerState.month, 1, 'native selected month');
        check(
            marsDatePickerGrid.querySelector('[data-iso-date="2024-01-31"]').classList.contains('selected'),
            'native selected-day projection');
        let changes = 0;
        first.input.addEventListener('change', () => {
            ++changes;
            marsDatePickerAnchorRect(first.shell);
        });
        equal(commitMarsDateValue(first.input, '2024-01-31'), false, 'unchanged commit');
        equal(changes, 0, 'unchanged commit emits no event');
        shiftMarsDatePickerMonth(1);
        equal(first.input.value, '2024-02-29', 'month navigation clamps end-of-month');
        equal(changes, 1, 'month navigation emits one deferred change');
        shiftMarsDatePickerYear(1);
        equal(first.input.value, '2025-02-28', 'year navigation clamps leap day');
        setMarsDatePickerMonthYear(2026, 3);
        equal(first.input.value, '2025-02-28', 'noncommitting navigation preserves authored input');
        equal(marsDatePickerState.year, 2026, 'noncommitting navigation updates native view');
        setMarsDatePickerMonthYear(2024, 2, {commit: true});
        equal(first.input.value, '2024-02-28', 'committing month/year preserves current input day');

        second.input.value = '';
        openMarsDatePicker(second.input, second.button);
        check(
            !first.shell.classList.contains('open') && second.shell.classList.contains('open'),
            'opening another input transfers shell ownership');
        equal(marsDatePickerState.month, 2, 'empty input starts at local today');
        equal(marsDatePickerState.year, 2024, 'empty input uses local today year');
        openMarsDatePicker(null, second.button);
        equal(marsDatePickerState.input, second.input, 'invalid open preserves ownership');
        let focusSawClosed = false;
        second.button.addEventListener('focus', () => {
            focusSawClosed = marsDatePickerState.input === null && native.lab_forms_picker_date() === 0;
            marsDatePickerAnchorRect(second.shell);
        });
        second.input.focus();
        closeMarsDatePicker({restoreFocus: true});
        equal(document.activeElement, second.button, 'close restores button focus');
        check(focusSawClosed, 'focus event sees fully closed native and host state');
        check(
            marsDatePicker.classList.contains('hidden') && !second.shell.classList.contains('open'),
            'closed projection');
        equal(marsDatePickerState.button, null, 'closed button reference released');
        equal(marsDatePickerState.shell, null, 'closed shell reference released');
        shiftMarsDatePickerMonth(1);
        renderMarsDatePicker();
        check(marsDatePicker.classList.contains('hidden'), 'stale navigation/render cannot reopen picker');

        commitMarsTodayValue(first.input);
        equal(first.input.value, '2024-02-29', 'ordinary Today commits local date');
        almanacDate.value = '2024-02-28';
        almanacTime.value = '12:00:00';
        commitMarsTodayValue(almanacDate);
        equal(almanacEvents.length, 1, 'changed GMT date and time emit one event');
        equal(almanacEvents[0].target, almanacDate, 'date event takes precedence');
        equal(almanacEvents[0].date, moment.date, 'event observes new GMT date');
        equal(almanacEvents[0].time, moment.time, 'event observes new GMT time');
        commitMarsTodayValue(almanacDate);
        equal(almanacEvents.length, 1, 'unchanged GMT moment emits no event');
        almanacTime.value = '12:00:00';
        commitMarsTodayValue(almanacDate);
        equal(almanacEvents.length, 2, 'time-only Today emits one event');
        equal(almanacEvents[1].target, almanacTime, 'time-only change targets time input');
        commitMarsTodayValue(null);
        equal(almanacEvents.length, 2, 'missing Today input is inert');
    } finally {
        document.removeEventListener('change', intercept, true);
        marsTodayIsoDate = saved.today;
        marsCurrentGmtMoment = saved.moment;
        almanacDate.value = saved.date;
        almanacTime.value = saved.time;
        hideButtonTooltip();
        root.remove();
        marsDatePickerState.input = saved.input;
        marsDatePickerState.button = saved.button;
        marsDatePickerState.shell = saved.shell;
        if (saved.packed)
            native.lab_forms_picker_open(
                native.lab_forms_date_year(saved.packed), native.lab_forms_date_month(saved.packed), 2024);
        else
            native.lab_forms_picker_close();
        if (saved.shell) {
            if (oldShellClass === null)
                saved.shell.removeAttribute('class');
            else
                saved.shell.setAttribute('class', oldShellClass);
        }
        for (const {node, children, value, attributes} of snapshots) {
            for (const attribute of [...node.attributes]) node.removeAttribute(attribute.name);
            for (const [name, content] of attributes) node.setAttribute(name, content);
            node.replaceChildren(...children);
            if (value !== undefined)
                node.value = value;
        }
        saved.focus?.focus();
    }
};
