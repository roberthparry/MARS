/**
 * @file widgets_checks.ts
 * @brief DOM projection checks for native-owned widgets.
 *
 * Snapshots picker and result elements while exercising scoped browser capabilities, restoring their original state afterwards.
 */

/** Real DOM projection checks for C-owned widgets and scoped host capabilities. */
window.checkLabWidgets = function checkLabWidgets() {
    const check: LabTestCheck = (ok, message) => {
        if (!ok)
            throw new Error('C DOM widgets: ' + message);
    };
    const snapshots = [
        WorksheetContext.marsDatePickerGrid, WorksheetContext.marsDatePickerWeekdays, WorksheetContext.marsDatePickerMonth, WorksheetContext.marsDatePickerYear, WorksheetContext.marsDatePicker, WorksheetContext.rendered
    ].map(element => ({
              element,
              children: [...element.childNodes],
              attrs: [...element.attributes].map(attr => [attr.name, attr.value] as const),
              value: (element as LabWorksheetTestSnapshotElement).value
          }));
    const savedInput = WorksheetContext.marsDatePickerState.input;
    let savedServices = null;
    const native = labWire.exports();
    const calls: ([unknown, unknown] | {restoreFocus?: boolean})[] = [];
    try {
        const render = (...args: unknown[]) => labDOM.call('lab_widgets_picker', ...args);
        check(render(2024, 2, 2024, 2, 29, 2024, 2, 29) === 1, 'leap month rendered');
        check(
            WorksheetContext.marsDatePickerMonth.options.length === 12 && WorksheetContext.marsDatePickerMonth.value === '2',
            'month catalogue and selection');
        check(WorksheetContext.marsDatePickerWeekdays.textContent === 'MonTueWedThuFriSatSun', 'weekday headings');
        check(WorksheetContext.marsDatePickerGrid.children.length === 42, 'six complete weeks');
        const leap = WorksheetContext.marsDatePickerGrid.querySelector<HTMLElement>('[data-iso-date="2024-02-29"]')!;
        check(leap && leap.classList.contains('today') && leap.classList.contains('selected'), 'combined day classes');
        check((WorksheetContext.marsDatePickerGrid.firstChild as HTMLElement).dataset.isoDate === '2024-01-29', 'leading outside-month day');
        check((WorksheetContext.marsDatePickerGrid.firstChild as HTMLElement).classList.contains('outside'), 'leading day class');
        const before = WorksheetContext.marsDatePickerGrid.innerHTML;
        check(
            render(NaN, 2, 0, 0, 0, 0, 0, 0) === 0 && WorksheetContext.marsDatePickerGrid.innerHTML === before,
            'invalid month preserves DOM');
        savedServices = labDOM.bindEvents({});
        labDOM.bindEvents({
            ...savedServices,
            commitMarsDateValue: (input, value) => calls.push([input, value]),
            closeMarsDatePicker: options => calls.push(options)
        });
        WorksheetContext.marsDatePickerState.input = WorksheetContext.datetimeDate;
        leap.click();
        check(
            calls.length === 2 && (calls[0] as [unknown, unknown])[0] === WorksheetContext.datetimeDate && (calls[0] as [unknown, unknown])[1] === '2024-02-29' && (calls[1] as {restoreFocus?: boolean}).restoreFocus,
            'one delegated event commits and restores focus');
        render(1, 1, 1, 1, 1, 0, 0, 0);
        check(WorksheetContext.marsDatePickerYear.value === '0001', 'early year padded');
        render(9999, 12, 0, 0, 0, 0, 0, 0);
        const disabled = WorksheetContext.marsDatePickerGrid.lastChild as HTMLButtonElement;
        check(
            disabled.disabled && disabled.classList.contains('outside') && !disabled.dataset.isoDate,
            'out-of-range date disabled without an authored date');
        disabled.click();
        check(calls.length === 2, 'disabled boundary cannot commit');
        check(WorksheetContext.marsDatePickerMonth.options.length === 12, 'repeated renders do not duplicate month options');

        const button = document.createElement('button');
        WorksheetResults.cards.resetMoreDigitsButton(button, true);
        check(
            !button.classList.contains('hidden') && button.dataset.expanded === 'false' &&
                button.textContent === 'Show more digits',
            'C resets expandable controls');
        WorksheetResults.cards.resetMoreDigitsButton(button, false);
        check(button.classList.contains('hidden'), 'C hides unavailable expansion');
        WorksheetResults.cards.setRenderedError('<b>not markup</b>');
        check(WorksheetContext.rendered.textContent === '<b>not markup</b>' && !WorksheetContext.rendered.querySelector<HTMLElement>('b')!, 'errors remain plain text');
        check(
            WorksheetContext.rendered.classList.contains('error') && getComputedStyle(WorksheetContext.rendered).color === 'rgb(255, 217, 154)',
            'CSS supplies unchanged error colour');
        WorksheetResults.cards.clearRenderedError();
        check(!WorksheetContext.rendered.classList.contains('error'), 'C clears error state');
        for (const action
                 of [() => labDOM.call('missing_export'),
                    () => labDOM.call('lab_layout_more', ...Array(4096).fill(button))]) {
            let failed = false;
            try {
                action();
            } catch (_) {
                failed = true;
            }
            check(failed, 'invalid/oversized capability call rejected');
            WorksheetResults.cards.resetMoreDigitsButton(button, true);
            check(!button.classList.contains('hidden'), 'failed call releases scope for the next call');
        }
        const zoom = native.lab_view_card_zoom(0);
        labDOM.call('lab_layout_set_zoom', null, 8, 0);
        check(native.lab_view_card_zoom(0) === zoom, 'absent DOM card cannot change native state');
    } finally {
        if (savedServices)
            labDOM.bindEvents(savedServices);
        WorksheetContext.marsDatePickerState.input = savedInput;
        for (const {element, children, attrs, value} of snapshots) {
            for (const attribute of [...element.attributes]) element.removeAttribute(attribute.name);
            for (const [name, content] of attrs) element.setAttribute(name, content);
            element.replaceChildren(...children);
            if (value !== undefined)
                (element as LabWorksheetTestSnapshotElement).value = value;
        }
    }
};
