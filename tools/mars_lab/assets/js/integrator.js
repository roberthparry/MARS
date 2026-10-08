/**
 * Integration-bound rows and free-parameter controls.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function cleanIntegratorBoundValue(value) {
  const text = String(value || '').trim();
  return /^blank\s+for\s+(none|antiderivative)$/i.test(text) ? '' : text;
}

function normaliseIntegratorRowKind(kind) {
  return String(kind || '').trim().toLowerCase() === 'free' ? 'free' : 'bound';
}

function sanitizeIntegratorRow(row, fallbackName = 'x') {
  const safe = row || {};
  return {
    kind: normaliseIntegratorRowKind(safe.kind),
    name: String(safe.name || fallbackName).trim() || fallbackName,
    lo: cleanIntegratorBoundValue(safe.lo),
    hi: cleanIntegratorBoundValue(safe.hi),
  };
}

function integratorDefaultVariableName(rows = []) {
  const taken = new Set(
    (Array.isArray(rows) ? rows : [])
      .map((row) => String(row && row.name || '').trim())
      .filter(Boolean)
  );
  const preferred = ['x', 'y', 'z', 't', 'u', 'v', 'w', 'r', 's'];
  for (const name of preferred) {
    if (!taken.has(name))
      return name;
  }
  for (let i = 1; i < 100; i += 1) {
    const name = `x${i}`;
    if (!taken.has(name))
      return name;
  }
  return 'x';
}

function integratorRowText(row) {
  const safe = sanitizeIntegratorRow(row);
  if (safe.kind === 'free')
    return `free ${safe.name}`;
  if (safe.lo && safe.hi)
    return `${safe.name} = ${safe.lo} .. ${safe.hi}`;
  if (safe.hi)
    return `${safe.name} = ${safe.hi}`;
  return safe.name;
}

function integratorBoundsTextFromRows(rows) {
  return (Array.isArray(rows) ? rows : [])
    .map((row) => integratorRowText(row))
    .filter(Boolean)
    .join('\n');
}

function parseIntegratorBoundsText(text) {
  const rows = [];
  for (const rawLine of String(text || '').split(/\n+/)) {
    const line = rawLine.trim();
    if (!line)
      continue;
    let match = line.match(/^free\s*(?::|\s)\s*(.+)$/i);
    if (match) {
      rows.push(sanitizeIntegratorRow({
        kind: 'free',
        name: match[1].trim(),
        lo: '',
        hi: '',
      }));
      continue;
    }
    match = line.match(/^([^:=]+?)\s*(?:=|:)\s*(.+?)\s*\.\.\s*(.+)$/);
    if (match) {
      rows.push(sanitizeIntegratorRow({
        kind: 'bound',
        name: match[1].trim(),
        lo: match[2].trim(),
        hi: match[3].trim(),
      }));
      continue;
    }
    match = line.match(/^([^:=]+?)\s*(?:=|:)\s*(.+)$/);
    if (match) {
      rows.push(sanitizeIntegratorRow({
        kind: 'bound',
        name: match[1].trim(),
        lo: '',
        hi: match[2].trim(),
      }));
      continue;
    }
    if (!/[=:]/.test(line) && !line.includes('..')) {
      rows.push(sanitizeIntegratorRow({
        kind: 'bound',
        name: line.trim(),
        lo: '',
        hi: '',
      }));
      continue;
    }
    throw new Error(`Bad bound line: ${line}`);
  }
  if (!rows.length)
    rows.push({kind: 'bound', name: 'x', lo: '0', hi: '1'});
  return rows;
}

function integratorFallbackRows() {
  return [{kind: 'bound', name: 'x', lo: '0', hi: '1'}];
}

function integratorBlankRows() {
  return [{kind: 'bound', name: 'x', lo: '', hi: ''}];
}

function currentIntegratorRows() {
  const rows = Array.from((integratorBoundStack || document.createElement('div')).querySelectorAll('.integrator-bound-row'))
    .map((row) => sanitizeIntegratorRow({
      kind: row.dataset.kind || 'bound',
      name: row.querySelector('[data-integrator-name]')?.value || '',
      lo: row.querySelector('[data-integrator-lower]')?.value || '',
      hi: row.querySelector('[data-integrator-upper]')?.value || '',
    }));
  return rows.length ? rows : integratorFallbackRows();
}

function escapeRegexLiteral(text) {
  return String(text || '').replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function integratorExpressionReferencesName(expressionText, name) {
  const needle = String(name || '').trim();
  if (!needle)
    return false;
  const body = expressionBodyForEditor(expressionText || currentExpressionText() || expr.value || '');
  if (!body)
    return false;
  if (needle.startsWith('[') && needle.endsWith(']'))
    return body.includes(needle);
  const pattern = new RegExp(`(^|[^A-Za-z0-9_])${escapeRegexLiteral(needle)}(?=$|[^A-Za-z0-9_])`);
  return pattern.test(body);
}

function activeIntegratorBoundRows(rows = currentIntegratorRows(), expressionText = '') {
  const boundRows = (Array.isArray(rows) ? rows : [])
    .filter((row) => normaliseIntegratorRowKind(row.kind) === 'bound');
  const activeRows = boundRows.filter((row) =>
    row.lo ||
    row.hi ||
    boundRows.length === 1 ||
    integratorExpressionReferencesName(expressionText, row.name)
  );
  return activeRows.length ? activeRows : integratorFallbackRows();
}

function activeIntegratorRows(rows = currentIntegratorRows(), expressionText = '') {
  const activeBounds = activeIntegratorBoundRows(rows, expressionText);
  const activeBoundSet = new Set(activeBounds);
  const activeRows = (Array.isArray(rows) ? rows : [])
    .filter((row) => {
      if (normaliseIntegratorRowKind(row.kind) === 'free')
        return integratorExpressionReferencesName(expressionText, row.name);
      return activeBoundSet.has(row);
    });
  return activeRows.length ? activeRows : integratorFallbackRows();
}

function currentIntegratorBoundRows() {
  return activeIntegratorBoundRows();
}

function currentIntegratorBoundNames() {
  return new Set(currentIntegratorBoundRows().map((row) => row.name));
}

function renderIntegratorRows(rows) {
  const sourceRows = Array.isArray(rows) && rows.length ? rows : integratorFallbackRows();
  const safeRows = sourceRows.map((row, index) =>
    sanitizeIntegratorRow(row, integratorDefaultVariableName(sourceRows.slice(0, index)))
  );
  integratorBoundStack.replaceChildren();

  safeRows.forEach((row, index) => {
    const boundCount = safeRows.filter((entry) => entry.kind !== 'free').length;
    const item = document.createElement('div');
    item.className = 'integrator-bound-row';
    item.dataset.kind = row.kind;
    item.dataset.index = String(index);

    const toggle = document.createElement('button');
    toggle.className = 'card-action integrator-bound-toggle';
    toggle.type = 'button';
    toggle.textContent = row.kind === 'free' ? 'Bound' : 'Free';
    toggle.title = row.kind === 'free'
      ? `Integrate with respect to ${row.name}`
      : `Leave ${row.name} free`;
    toggle.addEventListener('click', () => {
      commitVisibleBindingInputs();
      const nextRows = currentIntegratorRows();
      const target = nextRows[index];
      if (!target)
        return;
      target.kind = target.kind === 'free' ? 'bound' : 'free';
      target.lo = target.kind === 'free' ? '' : target.lo;
      target.hi = target.kind === 'free' ? '' : target.hi;
      if (nextRows.filter((entry) => entry.kind !== 'free').length === 0)
        nextRows.push({kind: 'bound', name: integratorDefaultVariableName(nextRows), lo: '', hi: ''});
      renderIntegratorRows(nextRows);
      refreshVariableValuesFromEditor();
      updateHistoryButtons();
      if (currentMode() === 'integrator')
        saveLastIntegratorState();
    });

    const makeField = (labelText, value, datasetKey, placeholder = '', disabled = false) => {
      const field = document.createElement('div');
      field.className = disabled ? 'integrator-bound-field disabled' : 'integrator-bound-field';
      const label = document.createElement('label');
      label.textContent = labelText;
      const input = document.createElement('input');
      input.spellcheck = false;
      input.autocomplete = 'off';
      input.value = value;
      input.placeholder = placeholder;
      input.dataset[datasetKey] = '1';
      input.disabled = disabled;
      input.addEventListener('keydown', (event) => {
        if (event.key === 'Enter') {
          event.preventDefault();
          input.blur();
        } else if (event.key === 'Escape') {
          event.preventDefault();
          input.value = value;
          input.blur();
        }
      });
      input.addEventListener('change', () => {
        if (datasetKey !== 'integratorName')
          input.value = cleanIntegratorBoundValue(input.value);
        else
          input.value = String(input.value || row.name || 'x').trim() || row.name || 'x';
        if (currentMode() === 'integrator') {
          refreshVariableValuesFromEditor();
          updateHistoryButtons();
          saveLastIntegratorState();
        }
      });
      field.append(label, input);
      return field;
    };

    const nameField = makeField('Variable', row.name, 'integratorName');
    const lowerField = makeField('Lower bound', row.lo, 'integratorLower', 'blank for none', row.kind === 'free');
    const upperField = makeField('Upper bound', row.hi, 'integratorUpper', 'blank for none', row.kind === 'free');

    const add = document.createElement('button');
    add.className = 'card-action integrator-bound-add';
    add.type = 'button';
    add.textContent = '+';
    add.title = 'Add another integral row';
    add.addEventListener('click', () => {
      commitVisibleBindingInputs();
      const nextRows = currentIntegratorRows();
      nextRows.splice(index + 1, 0, {
        kind: 'bound',
        name: integratorDefaultVariableName(nextRows),
        lo: '',
        hi: '',
      });
      renderIntegratorRows(nextRows);
      refreshVariableValuesFromEditor();
      updateHistoryButtons();
      if (currentMode() === 'integrator')
        saveLastIntegratorState();
    });

    const remove = document.createElement('button');
    remove.className = 'card-action integrator-bound-remove';
    remove.type = 'button';
    remove.textContent = '−';
    remove.title = 'Remove this row';
    remove.disabled = safeRows.length === 1 || (row.kind !== 'free' && boundCount === 1);
    remove.addEventListener('click', () => {
      commitVisibleBindingInputs();
      const nextRows = currentIntegratorRows();
      nextRows.splice(index, 1);
      if (!nextRows.length)
        nextRows.push({kind: 'bound', name: 'x', lo: '', hi: ''});
      if (nextRows.filter((entry) => entry.kind !== 'free').length === 0)
        nextRows.push({kind: 'bound', name: integratorDefaultVariableName(nextRows), lo: '', hi: ''});
      renderIntegratorRows(nextRows);
      refreshVariableValuesFromEditor();
      updateHistoryButtons();
      if (currentMode() === 'integrator')
        saveLastIntegratorState();
    });

    item.append(toggle, nameField, lowerField, upperField, add, remove);
    integratorBoundStack.appendChild(item);
  });
}

function applyIntegratorResultBound(data) {
  const responseBounds = Array.isArray(data && data.bounds) ? data.bounds : [];
  if (!responseBounds.length)
    return;
  const parameterNames = new Set(
    variableNamesFromBindings(data && data.binding_values)
      .map((name) => String(name || '').trim())
      .filter(Boolean)
  );
  const previousRows = currentIntegratorRows();
  const mergedRows = [];
  let boundIndex = 0;

  previousRows.forEach((row) => {
    if (row.kind === 'free') {
      if (parameterNames.has(String(row.name || '').trim()))
        mergedRows.push(row);
      return;
    }
    if (boundIndex < responseBounds.length)
      mergedRows.push(responseBounds[boundIndex++]);
  });

  while (boundIndex < responseBounds.length)
    mergedRows.push(responseBounds[boundIndex++]);

  renderIntegratorRows(mergedRows);
}

function restoreIntegratorBoundsText(text) {
  try {
    renderIntegratorRows(parseIntegratorBoundsText(text || DEFAULT_INTEGRATOR_BOUNDS_TEXT));
  } catch (_) {
    renderIntegratorRows(integratorFallbackRows());
  }
}

function currentIntegratorBoundsText() {
  return integratorBoundsTextFromRows(activeIntegratorRows());
}

function resetIntegratorBoundsToDefault() {
  restoreIntegratorBoundsText(DEFAULT_INTEGRATOR_BOUNDS_TEXT);
}

function resetIntegratorBoundsToBlank() {
  renderIntegratorRows(integratorBlankRows());
}

function requestedIntegratorIntervalCap() {
  const raw = parseInt(String(integratorIntervalCap && integratorIntervalCap.value || DEFAULT_INTEGRATOR_INTERVAL_CAP), 10);
  if (!Number.isFinite(raw))
    return DEFAULT_INTEGRATOR_INTERVAL_CAP;
  return raw;
}
