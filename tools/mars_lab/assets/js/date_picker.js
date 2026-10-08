/**
 * Civil-date picker navigation, layout and date-entry controls.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function parseMarsIsoDate(text) {
  const match = String(text || '').trim().match(/^(\d{4})-(\d{2})-(\d{2})$/);
  if (!match)
    return null;
  const year = Number(match[1]);
  const month = Number(match[2]);
  const day = Number(match[3]);
  if (!Number.isInteger(year) || !Number.isInteger(month) || !Number.isInteger(day))
    return null;
  const probe = new Date(Date.UTC(year, month - 1, day));
  if (probe.getUTCFullYear() !== year || probe.getUTCMonth() !== month - 1 || probe.getUTCDate() !== day)
    return null;
  return {year, month, day};
}

function marsClampYear(year) {
  const number = Number.parseInt(String(year || ''), 10);
  if (!Number.isFinite(number))
    return new Date().getFullYear();
  return Math.max(MARS_DATE_MIN_YEAR, Math.min(MARS_DATE_MAX_YEAR, number));
}

function marsClampMonth(month) {
  const number = Number.parseInt(String(month || ''), 10);
  if (!Number.isFinite(number))
    return 1;
  return Math.max(1, Math.min(12, number));
}

function marsDaysInMonth(year, month) {
  return new Date(Date.UTC(year, month, 0)).getUTCDate();
}

function marsIsoDate(year, month, day) {
  return `${String(year).padStart(4, '0')}-${String(month).padStart(2, '0')}-${String(day).padStart(2, '0')}`;
}

function marsClockTime(hours, minutes, seconds) {
  return `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
}

function marsTodayIsoDate() {
  const now = new Date();
  return marsIsoDate(now.getFullYear(), now.getMonth() + 1, now.getDate());
}

function marsCurrentGmtMoment(now = new Date()) {
  return {
    date: marsIsoDate(now.getUTCFullYear(), now.getUTCMonth() + 1, now.getUTCDate()),
    time: marsClockTime(now.getUTCHours(), now.getUTCMinutes(), now.getUTCSeconds())
  };
}

function marsTodayIsoDateForInput(input) {
  if (input === almanacDate)
    return marsCurrentGmtMoment().date;
  return marsTodayIsoDate();
}

function initialiseMarsDatePickerControls() {
  if (!marsDatePickerMonth || marsDatePickerMonth.options.length)
    return;
  marsDatePickerMonth.replaceChildren(...MARS_DATE_MONTHS.map((name, index) => {
    const option = document.createElement('option');
    option.value = String(index + 1);
    option.textContent = name;
    return option;
  }));
}

function marsStartWeekdayIndex(year, month) {
  const weekday = new Date(Date.UTC(year, month - 1, 1)).getUTCDay();
  return (weekday + 6) % 7;
}

function closeMarsDatePicker({restoreFocus = false} = {}) {
  if (!marsDatePicker)
    return;
  marsDatePicker.classList.add('hidden');
  if (marsDatePickerState.shell)
    marsDatePickerState.shell.classList.remove('open');
  if (restoreFocus && marsDatePickerState.button)
    marsDatePickerState.button.focus();
  marsDatePickerState.input = null;
  marsDatePickerState.button = null;
  marsDatePickerState.shell = null;
}

function marsDatePickerAnchorRect(shell) {
  if (!shell)
    return null;
  const rect = shell.getBoundingClientRect();
  if (rect.width > 1 && rect.height > 1)
    return rect;

  const input = shell.querySelector('input');
  const button = shell.querySelector('[data-date-target]');
  const boxes = [input, button]
    .filter(Boolean)
    .map((node) => node.getBoundingClientRect())
    .filter((box) => box.width > 1 && box.height > 1);
  if (!boxes.length)
    return null;

  const left = Math.min(...boxes.map((box) => box.left));
  const right = Math.max(...boxes.map((box) => box.right));
  const top = Math.min(...boxes.map((box) => box.top));
  const bottom = Math.max(...boxes.map((box) => box.bottom));
  return {
    left,
    right,
    top,
    bottom,
    width: right - left,
    height: bottom - top
  };
}

function placeMarsDatePicker(shell) {
  if (!marsDatePicker || !shell)
    return;
  const rect = marsDatePickerAnchorRect(shell);
  if (!rect)
    return;
  const buttonRect = marsDatePickerState.button?.getBoundingClientRect();
  const anchorBottom = buttonRect && buttonRect.height > 1 ? buttonRect.bottom : rect.bottom;
  const margin = 12;
  const viewportWidth = window.innerWidth || document.documentElement.clientWidth || 0;
  const viewportHeight = window.innerHeight || document.documentElement.clientHeight || 0;
  const maxWidth = Math.max(0, viewportWidth - margin * 2);
  const minWidth = Math.min(448, maxWidth);
  const width = Math.min(maxWidth, Math.max(rect.width, minWidth));
  const left = Math.max(margin, Math.min(rect.left, viewportWidth - width - margin));
  const top = anchorBottom + 8;
  const availableHeight = Math.max(8, viewportHeight - top - margin);

  marsDatePicker.style.width = `${width}px`;
  marsDatePicker.style.left = `${left}px`;
  marsDatePicker.style.top = `${top}px`;
  marsDatePicker.style.maxHeight = `${availableHeight}px`;
}

function commitMarsDateValue(input, value) {
  if (!input)
    return;
  const changed = input.value !== value;
  input.value = value;
  if (changed)
    input.dispatchEvent(new Event('change', {bubbles: true}));
  return changed;
}

function commitMarsTodayValue(input) {
  if (input === almanacDate && almanacTime) {
    const moment = marsCurrentGmtMoment();
    const timeChanged = almanacTime.value !== moment.time;
    almanacTime.value = moment.time;
    if (!commitMarsDateValue(input, moment.date) && timeChanged)
      almanacTime.dispatchEvent(new Event('change', {bubbles: true}));
    return;
  }
  commitMarsDateValue(input, marsTodayIsoDate());
}

function renderMarsDatePicker() {
  if (!marsDatePicker || !marsDatePickerGrid || !marsDatePickerTitle || !marsDatePickerWeekdays)
    return;

  initialiseMarsDatePickerControls();
  const {input, year, month} = marsDatePickerState;
  const selected = parseMarsIsoDate(input && input.value);
  const today = parseMarsIsoDate(marsTodayIsoDateForInput(input));
  const daysInMonth = marsDaysInMonth(year, month);
  const startIndex = marsStartWeekdayIndex(year, month);
  const previousMonth = month === 1 ? 12 : month - 1;
  const previousYear = month === 1 ? year - 1 : year;
  const previousDays = marsDaysInMonth(previousYear, previousMonth);
  let nextDay = 1;

  if (marsDatePickerMonth)
    marsDatePickerMonth.value = String(month);
  if (marsDatePickerYear)
    marsDatePickerYear.value = String(year).padStart(4, '0');
  marsDatePickerWeekdays.replaceChildren(...MARS_DATE_WEEKDAYS.map((name) => {
    const cell = document.createElement('div');
    cell.className = 'mars-date-picker-weekday';
    cell.textContent = name;
    return cell;
  }));

  const dayButtons = [];
  for (let slot = 0; slot < 42; slot += 1) {
    const button = document.createElement('button');
    let displayDay;
    let buttonYear = year;
    let buttonMonth = month;

    button.type = 'button';
    button.className = 'mars-date-picker-day';

    if (slot < startIndex) {
      displayDay = previousDays - startIndex + slot + 1;
      buttonYear = previousYear;
      buttonMonth = previousMonth;
      button.classList.add('outside');
    } else if (slot >= startIndex + daysInMonth) {
      displayDay = nextDay++;
      buttonMonth = month === 12 ? 1 : month + 1;
      buttonYear = month === 12 ? year + 1 : year;
      button.classList.add('outside');
    } else {
      displayDay = slot - startIndex + 1;
    }

    const isoValue = marsIsoDate(buttonYear, buttonMonth, displayDay);
    button.textContent = String(displayDay);
    button.dataset.isoDate = isoValue;

    if (today && today.year === buttonYear && today.month === buttonMonth && today.day === displayDay)
      button.classList.add('today');
    if (selected && selected.year === buttonYear && selected.month === buttonMonth && selected.day === displayDay)
      button.classList.add('selected');

    button.addEventListener('click', () => {
      commitMarsDateValue(input, isoValue);
      closeMarsDatePicker({restoreFocus: true});
    });
    dayButtons.push(button);
  }

  marsDatePickerGrid.replaceChildren(...dayButtons);
  marsDatePicker.classList.remove('hidden');
  placeMarsDatePicker(marsDatePickerState.shell);
}

function openMarsDatePicker(input, button) {
  if (!input || !button || !marsDatePicker)
    return;

  const shell = button.closest('.mars-date-shell');
  const parsed = parseMarsIsoDate(input.value) || parseMarsIsoDate(marsTodayIsoDateForInput(input));
  marsDatePickerState.input = input;
  marsDatePickerState.button = button;
  marsDatePickerState.shell = shell;
  marsDatePickerState.year = marsClampYear(parsed.year);
  marsDatePickerState.month = marsClampMonth(parsed.month);
  if (shell)
    shell.classList.add('open');
  renderMarsDatePicker();
}

function commitMarsDatePickerMonthYear(year, month) {
  const input = marsDatePickerState.input;
  if (!input)
    return;
  const selected = parseMarsIsoDate(input.value);
  const day = selected ? selected.day : 1;
  const safeDay = Math.min(day, marsDaysInMonth(year, month));
  commitMarsDateValue(input, marsIsoDate(year, month, safeDay));
}

function setMarsDatePickerMonthYear(year, month, {commit = false} = {}) {
  if (!marsDatePickerState.input)
    return;
  marsDatePickerState.year = marsClampYear(year);
  marsDatePickerState.month = marsClampMonth(month);
  if (commit)
    commitMarsDatePickerMonthYear(marsDatePickerState.year, marsDatePickerState.month);
  renderMarsDatePicker();
}

function marsDatePickerYearStep(event) {
  if (event && event.ctrlKey)
    return 100;
  if (event && event.shiftKey)
    return 10;
  return 1;
}

function shiftMarsDatePickerYear(delta) {
  if (!marsDatePickerState.input)
    return;
  setMarsDatePickerMonthYear(marsDatePickerState.year + delta, marsDatePickerState.month, {commit: true});
}

function shiftMarsDatePickerMonth(delta) {
  if (!marsDatePickerState.input)
    return;
  let nextMonth = marsDatePickerState.month + delta;
  let nextYear = marsDatePickerState.year;
  while (nextMonth < 1) {
    nextMonth += 12;
    nextYear -= 1;
  }
  while (nextMonth > 12) {
    nextMonth -= 12;
    nextYear += 1;
  }
  setMarsDatePickerMonthYear(nextYear, nextMonth, {commit: true});
}
