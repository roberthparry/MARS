/**
 * Binding controls, authored values and editor synchronisation.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function expressionWithBindings(bodyText, bindings) {
  const source = String(bodyText || '').trim();
  if (!source)
    return '';
  if (!Array.isArray(bindings) || !bindings.length)
    return source;
  const body = expressionBodyForEditor(source);

  const variableAssignments = [];
  const constantAssignments = [];
  bindings.forEach((binding) => {
    const name = String(binding && binding.name || '').trim();
    if (!name)
      return;

    let valueText = String(binding && (binding.value ?? binding.display) || '').trim();
    if (!valueText || /^NAN$/i.test(valueText))
      valueText = '?';

    const assignment = `${name} = ${valueText}`;
    if (String(binding && binding.kind || 'variable').trim() === 'constant')
      constantAssignments.push(assignment);
    else
      variableAssignments.push(assignment);
  });

  constantAssignments.sort(compareBindingNames);
  let bindingText = variableAssignments.join(', ');
  if (constantAssignments.length) {
    const constants = constantAssignments.join(', ');
    bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;
  }
  return bindingText ? `{ ${body} | ${bindingText} }` : body;
}

function visibleBindingValues() {
  return Array.from(variableValues.querySelectorAll('.binding-value-input'))
    .map((input) => ({
      name: String(input.dataset.bindingName || '').trim(),
      kind: String(input.dataset.bindingKind || 'variable').trim(),
      value: normalisedBindingInputValue(input),
      display: normalisedBindingInputValue(input)
    }))
    .filter((binding) => binding.name);
}

function expressionWithVisibleBindings(sourceExpression, visibleBindings) {
  return expressionWithBindings(
    expressionBodyForEditor(sourceExpression),
    visibleBindings
  ) || sourceExpression;
}

function bindingsWithAuthoredValues(bindings, sourceExpression, visibleBindings = []) {
  const discovered = Array.isArray(bindings) ? bindings : [];
  const authored = compactExpressionForEditor(sourceExpression).bindings || [];
  const authoredByName = new Map(
    authored.map((binding) => [String(binding.name || '').trim(), binding])
  );
  const visibleByName = new Map(
    (Array.isArray(visibleBindings) ? visibleBindings : [])
      .map((binding) => [String(binding.name || '').trim(), binding])
  );

  return discovered.map((binding) => {
    const name = String(binding && binding.name || '').trim();
    const sourceBinding = visibleByName.get(name) || authoredByName.get(name);
    if (!sourceBinding)
      return binding;
    return {
      ...binding,
      value: sourceBinding.value,
      display: sourceBinding.display
    };
  });
}

function replaceBindingValueInExpression(sourceExpression, kind, targetName, valueText) {
  const parts = bindingParts(sourceExpression);
  if (!parts || !targetName)
    return sourceExpression;

  let changed = false;
  function replaceAssignments(assignmentsText, shouldReplace) {
    return splitTopLevel(assignmentsText, ',')
      .map((part) => {
        const eq = indexOfTopLevel(part, '=');
        if (eq < 0)
          return part.trim();

        const name = part.slice(0, eq).trim();
        if (!shouldReplace || name !== targetName)
          return part.trim();

        changed = true;
        return `${name} = ${valueText}`;
      })
      .filter(Boolean)
      .join(', ');
  }

  const variables = replaceAssignments(parts.variables, kind !== 'constant');
  const constants = replaceAssignments(parts.constants, kind === 'constant');
  if (!changed)
    return sourceExpression;

  let bindingText = variables;
  if (constants)
    bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;
  return `{ ${parts.body} | ${bindingText} }`;
}

function replaceBindingKindInExpression(sourceExpression, targetName, nextKind) {
  const parts = bindingParts(sourceExpression);
  if (!parts || !targetName)
    return sourceExpression;

  let movedAssignment = '';
  function removeAssignment(assignmentsText, shouldRemove) {
    return splitTopLevel(assignmentsText, ',')
      .map((part) => part.trim())
      .filter(Boolean)
      .filter((part) => {
        const eq = indexOfTopLevel(part, '=');
        const name = eq >= 0 ? part.slice(0, eq).trim() : part.trim();
        if (!shouldRemove || name !== targetName)
          return true;
        movedAssignment = part;
        return false;
      })
      .join(', ');
  }

  const variables = removeAssignment(parts.variables, nextKind === 'constant');
  const constants = removeAssignment(parts.constants, nextKind !== 'constant');
  if (!movedAssignment)
    return sourceExpression;

  const nextVariables = nextKind === 'constant'
    ? variables
    : [variables, movedAssignment].filter(Boolean).join(', ');
  const nextConstants = nextKind === 'constant'
    ? sortedAssignmentParts(
      [...splitTopLevel(constants, ','), movedAssignment]
        .map((part) => part.trim())
        .filter(Boolean)
    ).join(', ')
    : constants;
  let bindingText = nextVariables;
  if (nextConstants)
    bindingText = bindingText ? `${bindingText}; ${nextConstants}` : `; ${nextConstants}`;
  return `{ ${parts.body} | ${bindingText} }`;
}

function isIntegrationConstantName(name) {
  return /^C(?:_\d+|[₀₁₂₃₄₅₆₇₈₉]+)?$/.test(String(name || '').trim());
}

function splitTopLevelAddSubTerms(text) {
  const terms = [];
  let depth = 0;
  let start = 0;
  let sign = '+';
  const source = String(text || '');

  for (let i = 0; i < source.length; i++) {
    const ch = source[i];
    if (ch === '(' || ch === '[' || ch === '{') depth++;
    else if (ch === ')' || ch === ']' || ch === '}') depth = Math.max(0, depth - 1);
    else if ((ch === '+' || ch === '-') && depth === 0 && i > 0) {
      const term = source.slice(start, i).trim();
      if (term)
        terms.push({sign, text: term});
      sign = ch;
      start = i + 1;
    }
  }

  const tail = source.slice(start).trim();
  if (tail)
    terms.push({sign, text: tail});
  return terms;
}

function joinTopLevelAddSubTerms(terms) {
  return (terms || []).map((term, index) => {
    const sign = term.sign === '-' ? '-' : '+';
    const text = String(term.text || '').trim();
    if (!text)
      return '';
    if (index === 0)
      return sign === '-' ? `-${text}` : text;
    return sign === '-' ? ` - ${text}` : ` + ${text}`;
  }).filter(Boolean).join('');
}

function removeBindingFromExpression(sourceExpression, kind, targetName) {
  const parts = bindingParts(sourceExpression);
  if (!parts || !targetName)
    return sourceExpression;

  let changed = false;
  function keepAssignments(assignmentsText, shouldRemove) {
    return splitTopLevel(assignmentsText, ',')
      .map((part) => part.trim())
      .filter(Boolean)
      .filter((part) => {
        const eq = indexOfTopLevel(part, '=');
        const name = eq >= 0 ? part.slice(0, eq).trim() : part.trim();
        if (!shouldRemove || name !== targetName)
          return true;
        changed = true;
        return false;
      })
      .join(', ');
  }

  const variables = keepAssignments(parts.variables, kind !== 'constant');
  const constants = keepAssignments(parts.constants, kind === 'constant');
  let body = parts.body;
  if (kind === 'constant' && isIntegrationConstantName(targetName)) {
    const terms = splitTopLevelAddSubTerms(body);
    const filteredTerms = terms.filter((term) => term.text !== targetName);
    if (filteredTerms.length !== terms.length) {
      body = joinTopLevelAddSubTerms(filteredTerms) || '0';
      changed = true;
    }
  }

  if (!changed)
    return sourceExpression;

  let bindingText = variables;
  if (constants)
    bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;
  return bindingText ? `{ ${body} | ${bindingText} }` : body;
}

function applyUpdatedBindingExpression(updated) {
  if (currentMode() === 'expression' || currentMode() === 'equation' || currentMode() === 'diffequation') {
    setExpressionEditor(updated);
    return;
  }

  if (bindingParts(updated))
    setExpressionEditor(updated);
  else {
    expr.value = expressionForEditor(updated).trim();
    clearExpressionSource();
    clearVariableValues();
  }
}

async function applyMarsBindingExpression(updated, editorBodyText = null) {
  setBusy(true);
  setStatus('Updating bindings...');
  try {
    const {response, data} = await fetchEvaluation(updated, '', 'bindings');
    if (!response.ok || !data.ok)
      throw new Error(data.error || 'MARS could not update the bindings');

    const bindings = bindingsWithAuthoredValues(
      Array.isArray(data.binding_values) ? data.binding_values : [],
      updated
    );
    if (editorBodyText !== null && editorBodyText !== undefined) {
      const editorBody = expressionBodyForEditor(editorBodyText);
      setExpressionEditor(
        expressionWithBindings(editorBody, bindings) || editorBody,
        bindings,
        editorBody,
        data.evaluation_ready
      );
    } else {
      setExpressionEditor(
        updated,
        bindings,
        null,
        data.evaluation_ready
      );
    }
    updateHistoryButtons();
    saveCurrentModeEditorState();
    setStatus('Ready');
    return true;
  } catch (err) {
    setStatus(String(err));
    return false;
  } finally {
    setBusy(false);
  }
}

function applyMarsBindingsToEditedExpression(editedBody, sourceExpression, data) {
  const inlineBindings = bindingParts(editedBody);
  const editorBody = expressionBodyForEditor(editedBody);
  const authoredBindingSource = inlineBindings ? editedBody : sourceExpression;
  const bindings = bindingsWithAuthoredValues(
    data && data.binding_values,
    authoredBindingSource
  );
  fullExpressionText = expressionForEditor(
    expressionWithBindings(editorBody, bindings) || editorBody
  ).trim();
  displayedExpressionText = editorBody;
  expr.dataset.fullExpression = fullExpressionText;
  expr.dataset.displayExpression = displayedExpressionText;
  expr.dataset.bindingRefreshValid = 'true';
  expr.dataset.evaluationReady =
    String(data && data.evaluation_ready || 'no').trim().toLowerCase() === 'yes'
      ? 'true'
      : 'false';
  expr.value = displayedExpressionText;
  renderVariableValues(bindings);
  currentVariables = variableNamesFromBindings(bindings);
  currentDifferentiable =
    String(data && data.differentiable || 'yes').trim().toLowerCase() !== 'no';
  renderDerivativeButtons(currentVariables);
  saveLastExpression(fullExpressionText, {debounce: true});
}

async function refreshEditedExpressionBindings(editedBody, sourceExpression, sequence) {
  try {
    const {response, data} = await fetchEvaluation(
      editedBody,
      '',
      'bindings',
      sourceExpression
    );
    if (sequence !== expressionBindingRefreshSequence ||
        currentMode() !== 'expression' ||
        expr.value.trim() !== editedBody)
      return;

    if (!response.ok || !data.ok) {
      expr.dataset.bindingRefreshValid = 'false';
      updateHistoryButtons();
      return;
    }

    applyMarsBindingsToEditedExpression(editedBody, sourceExpression, data);
    updateHistoryButtons();
  } catch (err) {
    if (sequence !== expressionBindingRefreshSequence ||
        currentMode() !== 'expression' ||
        expr.value.trim() !== editedBody)
      return;
    expr.dataset.bindingRefreshValid = 'false';
    updateHistoryButtons();
  }
}

function scheduleEditedExpressionBindingRefresh() {
  const editedBody = expr.value.trim();
  const sourceExpression = expr.dataset.fullExpression || fullExpressionText;
  const sequence = ++expressionBindingRefreshSequence;

  clearTimeout(expressionBindingRefreshTimer);
  expr.dataset.bindingRefreshValid = 'pending';
  updateHistoryButtons();
  expressionBindingRefreshTimer = setTimeout(() => {
    void refreshEditedExpressionBindings(
      editedBody,
      sourceExpression,
      sequence
    );
  }, 300);
}

function saveCurrentModeEditorState() {
  if (currentMode() === 'expression')
    saveLastExpression(currentExpressionText() || expr.value.trim());
  else if (currentMode() === 'equation')
    saveLastEquationState();
  else if (currentMode() === 'diffequation')
    saveLastDiffequationState();
  else if (currentMode() === 'matrix')
    saveLastMatrixState();
  else
    saveLastIntegratorState();
}

function normalisedBindingInputValue(input) {
  let text = String(input.value || '').trim();
  // Recover values contaminated by an older result-envelope parser.
  const conditionStart = indexOfTopLevel(text, ';');
  if (conditionStart >= 0)
    text = text.slice(0, conditionStart).trim();
  return text || '?';
}

async function commitBindingInput(input) {
  const name = input.dataset.bindingName || '';
  const kind = input.dataset.bindingKind || 'variable';
  const valueText = normalisedBindingInputValue(input);
  const current = currentExpressionText();

  if (currentMode() === 'expression') {
    let updatedSource = replaceBindingValueInExpression(
      current,
      kind,
      name,
      valueText
    );
    if (updatedSource === current)
      updatedSource = replaceBindingValueInExpression(
        current,
        kind === 'constant' ? 'variable' : 'constant',
        name,
        valueText
      );
    if (!updatedSource || updatedSource === current)
      return;

    fullExpressionText = expressionForEditor(updatedSource).trim();
    expr.dataset.fullExpression = fullExpressionText;
    expr.dataset.bindingRefreshValid = 'true';
    input.value = isUnsetBindingValue(valueText) ? '' : valueText;
    input.title = valueText;
    if (isUnsetBindingValue(valueText))
      bindingValueCache.delete(name);
    else
      bindingValueCache.set(name, valueText);
    updateHistoryButtons();
    saveCurrentModeEditorState();
    return;
  }

  const removesIntegrationConstant =
    kind === 'constant' && valueText === '?' && isIntegrationConstantName(name);
  const updated = removesIntegrationConstant
    ? removeBindingFromExpression(current, kind, name)
    : replaceBindingValueInExpression(current, kind, name, valueText);

  input.value = (valueText === '?' || /^NAN$/i.test(valueText)) ? '' : valueText;
  input.title = valueText;

  if (updated === current)
    return;

  if (removesIntegrationConstant) {
    await applyMarsBindingExpression(updated);
    return;
  }

  applyUpdatedBindingExpression(updated);
  refreshVariableValuesFromEditor();
  updateHistoryButtons();
  saveCurrentModeEditorState();
}

function commitVisibleBindingInputs() {
  const inputs = Array.from(variableValues.querySelectorAll('.binding-value-input'));
  if (!inputs.length)
    return false;

  let current = currentExpressionText();
  let updated = current;

  if (currentMode() === 'expression') {
    if (!bindingParts(current)) {
      const enteredBindings = inputs.map((input) => ({
        name: input.dataset.bindingName || '',
        kind: input.dataset.bindingKind || 'variable',
        value: normalisedBindingInputValue(input)
      })).filter((binding) => binding.name);
      updated = expressionWithBindings(current, enteredBindings);
    } else {
      inputs.forEach((input) => {
        const name = input.dataset.bindingName || '';
        const kind = input.dataset.bindingKind || 'variable';
        const valueText = normalisedBindingInputValue(input);
        const previous = updated;
        updated = replaceBindingValueInExpression(previous, kind, name, valueText);
        if (updated === previous)
          updated = replaceBindingValueInExpression(
            previous,
            kind === 'constant' ? 'variable' : 'constant',
            name,
            valueText
          );
      });
    }

    if (!updated || updated === current)
      return false;

    fullExpressionText = expressionForEditor(updated).trim();
    expr.dataset.fullExpression = fullExpressionText;
    expr.dataset.bindingRefreshValid = 'true';
    inputs.forEach((input) => {
      const name = input.dataset.bindingName || '';
      const valueText = normalisedBindingInputValue(input);
      input.value = isUnsetBindingValue(valueText) ? '' : valueText;
      input.title = valueText;
      if (isUnsetBindingValue(valueText))
        bindingValueCache.delete(name);
      else
        bindingValueCache.set(name, valueText);
    });
    updateHistoryButtons();
    saveCurrentModeEditorState();
    return true;
  }

  if (currentMode() === 'matrix' && !bindingParts(current)) {
    const enteredBindings = inputs
      .map((input) => ({
        name: input.dataset.bindingName || '',
        kind: input.dataset.bindingKind || 'variable',
        value: String(input.value || '').trim()
      }))
      .filter((binding) => binding.name && binding.value);

    if (!enteredBindings.length)
      return false;
    updated = expressionWithBindings(current, enteredBindings);
  } else {
    inputs.forEach((input) => {
      const name = input.dataset.bindingName || '';
      const kind = input.dataset.bindingKind || 'variable';
      const valueText = normalisedBindingInputValue(input);
      updated = kind === 'constant' && valueText === '?' && isIntegrationConstantName(name)
        ? removeBindingFromExpression(updated, kind, name)
        : replaceBindingValueInExpression(updated, kind, name, valueText);
    });
  }

  if (!updated || updated === current)
    return false;

  applyUpdatedBindingExpression(updated);
  refreshVariableValuesFromEditor();
  updateHistoryButtons();
  saveCurrentModeEditorState();
  return true;
}

async function toggleBindingKind(binding) {
  const current = currentExpressionText();
  const name = String(binding && binding.name || '').trim();
  const currentKind = String(binding && binding.kind || 'variable').trim() || 'variable';
  if (!current || !name)
    return;

  const nextKind = currentKind === 'constant' ? 'variable' : 'constant';
  const updated = replaceBindingKindInExpression(current, name, nextKind);
  if (updated === current)
    return;

  if (currentMode() === 'expression') {
    await applyMarsBindingExpression(updated, expr.value.trim());
    return;
  }

  applyUpdatedBindingExpression(updated);
  refreshVariableValuesFromEditor();
  updateHistoryButtons();
  saveCurrentModeEditorState();
}

function displayValueForBinding(binding) {
  const value = String(binding.value || binding.display || '').trim();
  return (value === '?' || /^NAN$/i.test(value)) ? '' : value;
}

function solutionLineIsNumericLiteral(line) {
  const match = String(line || '').match(/^[^=≈]+(?:=|≈)\s*(.+)$/);
  if (!match)
    return false;

  const rhs = match[1].replace(/\s+/g, '');
  const number = '(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[Ee][+-]?\\d+)?';
  const fraction = '(?:\\d+/\\d+|[⁰¹²³⁴⁵⁶⁷⁸⁹]+⁄[₀₁₂₃₄₅₆₇₈₉]+|[¼½¾⅐⅑⅒⅓⅔⅕⅖⅗⅘⅙⅚⅛⅜⅝⅞])';
  const scalar = `(?:${number}|${fraction})`;
  const numeric = new RegExp(
    `^(?:[+-]?${scalar}|[+-]?(?:${scalar})?i|[+-]?${scalar}[+-](?:${scalar})?i)$`
  );
  return numeric.test(rhs);
}

function fullValueForBinding(binding) {
  const value = String(binding.value || binding.display || '').trim();
  return (value === '?' || /^NAN$/i.test(value)) ? '' : value;
}

function clearVariableValues() {
  variableValues.replaceChildren();
  variableValues.classList.add('hidden');
  currentBindingKinds = new Map();
}

function refreshVariableValuesFromEditor() {
  if (currentMode() === 'expression') {
    scheduleEditedExpressionBindingRefresh();
    return;
  }
  const compact = compactExpressionForEditor(currentExpressionText());
  const bindings = visibleBindingsForCurrentMode(compact.bindings || []);
  renderVariableValues(bindings);
  currentVariables = variableNamesFromBindings(bindings);
  renderDerivativeButtons(currentVariables);
}

function queueBindingInputCommit(input) {
  const isExpression = currentMode() === 'expression';
  const pending = isExpression
    ? pendingExpressionBindingCommit.then(() => commitBindingInput(input))
    : commitBindingInput(input);
  const handled = pending.catch((err) => setStatus(String(err)));
  if (isExpression)
    pendingExpressionBindingCommit = handled;
  return handled;
}

function bindingDisplayName(name) {
  const text = String(name || '');
  // Brackets remain part of the native identifier, but are not needed on UI labels.
  return text.startsWith('[') && text.endsWith(']') ? text.slice(1, -1) : text;
}

function renderVariableValues(bindings) {
  variableValues.replaceChildren();
  bindingValueCache = new Map();
  currentBindingKinds = new Map();
  if (!bindings.length) {
    variableValues.classList.add('hidden');
    return;
  }

  const variableBindings = [];
  const constantBindings = [];
  bindings.forEach((binding) => {
    const kind = binding.kind || 'variable';
    if (kind === 'constant')
      constantBindings.push(binding);
    else
      variableBindings.push(binding);
  });
  constantBindings.sort(compareBindingNames);

  [...variableBindings, ...constantBindings].forEach((binding) => {
    const kind = binding.kind || 'variable';
    const displayName = bindingDisplayName(binding.name);
    currentBindingKinds.set(binding.name, kind);
    const displayValue = displayValueForBinding(binding);
    const fullValue = fullValueForBinding(binding);
    if (fullValue)
      bindingValueCache.set(binding.name, fullValue);

    const box = document.createElement('div');
    box.className = kind === 'constant'
      ? 'variable-value-box constant-value-box'
      : 'variable-value-box';

    const name = document.createElement('span');
    name.className = kind === 'constant'
      ? 'variable-value-name constant-value-name'
      : 'variable-value-name';
    name.textContent = displayName;

    const field = document.createElement('div');
    field.className = 'binding-value-field';

    const text = document.createElement('input');
    text.className = 'variable-value-text binding-value-input';
    text.type = 'text';
    text.value = displayValue;
    text.title = fullValue || binding.value || '?';
    text.dataset.bindingName = binding.name;
    text.dataset.bindingKind = kind;
    text.autocomplete = 'off';
    text.spellcheck = false;
    // A blank placeholder lets CSS hide the clear button whenever the value is empty.
    text.placeholder = ' ';
    text.setAttribute('aria-label', `Value of ${displayName}`);
    text.addEventListener('keydown', (event) => {
      if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
        event.preventDefault();
        commitBindingInput(text).then(() => evaluateFromKeyboard());
      } else if (event.key === 'Enter') {
        event.preventDefault();
        text.blur();
      } else if (event.key === 'Escape') {
        event.preventDefault();
        text.value = displayValue;
        text.blur();
      }
    });
    text.addEventListener('change', () => {
      void queueBindingInputCommit(text);
    });
    text.addEventListener('input', () => updateHistoryButtons());

    const clear = document.createElement('button');
    clear.className = 'binding-value-clear';
    clear.type = 'button';
    clear.textContent = '×';
    clear.title = `Clear ${displayName}`;
    clear.setAttribute('aria-label', clear.title);
    // Do not blur and re-render the field before the click can clear it.
    clear.addEventListener('pointerdown', (event) => event.preventDefault());
    clear.addEventListener('click', async () => {
      text.value = '';
      text.focus();
      updateHistoryButtons();
      await queueBindingInputCommit(text);
      // Other modes may replace the binding controls while committing.
      const replacement = Array.from(variableValues.querySelectorAll('.binding-value-input'))
        .find((input) => input.dataset.bindingName === binding.name && input.dataset.bindingKind === kind);
      (replacement || expr).focus();
    });
    field.append(text, clear);

    const actions = document.createElement('div');
    actions.className = 'variable-value-actions';

    const copy = document.createElement('button');
    copy.className = 'card-action variable-copy';
    copy.type = 'button';
    copy.textContent = 'Copy';
    copy.addEventListener('click', async () => {
      try {
        await writeClipboardText(text.value);
        flashCopyButton(copy, true);
        setStatus(`Copied ${displayName}`);
        setTimeout(() => setStatus('Ready'), 1000);
      } catch (err) {
        flashCopyButton(copy, false);
        setStatus(String(err));
      }
    });

    const toggle = document.createElement('button');
    toggle.className = 'card-action variable-toggle';
    toggle.type = 'button';
    toggle.textContent = kind === 'constant' ? 'Variable' : 'Constant';
    toggle.title = kind === 'constant'
      ? `Treat ${displayName} as a variable`
      : `Treat ${displayName} as a constant`;
    toggle.addEventListener('click', () => {
      void toggleBindingKind(binding);
    });

    actions.append(toggle, copy);
    box.append(name, field, actions);
    variableValues.appendChild(box);
  });

  variableValues.classList.remove('hidden');
}

function assignmentValuesByName(assignmentsText) {
  const values = new Map();
  splitTopLevel(assignmentsText || '', ',').forEach((part) => {
    const eq = indexOfTopLevel(part, '=');
    if (eq < 0)
      return;

    const name = part.slice(0, eq).trim();
    const valueText = part.slice(eq + 1).trim();
    if (name && valueText)
      values.set(name, valueText);
  });
  return values;
}

function solvedStartValuesForGoalSeek(sourceExpression, solvedExpression, providedStart = {}) {
  const start = {...providedStart};
  const sourceParts = bindingParts(sourceExpression);
  const solvedParts = bindingParts(solvedExpression);
  if (!sourceParts || !solvedParts)
    return start;

  const sourceVariables = new Set(assignmentValuesByName(sourceParts.variables).keys());
  const solvedVariables = assignmentValuesByName(solvedParts.variables);
  sourceVariables.forEach((name) => {
    const cachedValue = bindingValueCache.get(name);
    const solvedValue = cachedValue || solvedVariables.get(name) || '';
    if (!solvedValue || solvedValue === '?' || /^NAN$/i.test(solvedValue))
      return;
    start[name] = solvedValue;
  });

  return start;
}

function goalSeekExpressionAndStarts(sourceExpression, providedStart = {}) {
  const parts = bindingParts(sourceExpression);
  const start = {...providedStart};

  if (!parts)
    return {expression: sourceExpression, start};

  let changed = false;
  const variables = splitTopLevel(parts.variables, ',')
    .map((part) => {
      const eq = indexOfTopLevel(part, '=');
      if (eq < 0)
        return part.trim();

      const name = part.slice(0, eq).trim();
      const valueText = part.slice(eq + 1).trim();
      if (!name)
        return part.trim();

      if (valueText && valueText !== '?' && !/^NAN$/i.test(valueText) && !start[name])
        start[name] = valueText;

      changed = changed || valueText !== '?';
      return `${name} = ?`;
    })
    .filter(Boolean)
    .join(', ');

  let bindingText = variables;
  if (parts.constants)
    bindingText = bindingText ? `${bindingText}; ${parts.constants}` : `; ${parts.constants}`;

  return {
    expression: changed ? `{ ${parts.body} | ${bindingText} }` : sourceExpression,
    start
  };
}

function setExpressionEditor(
  fullText,
  evaluatedBindings = null,
  editorBodyText = null,
  evaluationReady = null
) {
  const compact = currentMode() === 'expression'
    ? null
    : compactExpressionForEditor(fullText);
  const hasEvaluatedBindings = Array.isArray(evaluatedBindings);
  const editorBindings = hasEvaluatedBindings ? evaluatedBindings : [];
  const defaultEditorBody = expressionBodyForEditor(fullText);
  let editorBody = editorBodyText === null || editorBodyText === undefined
    ? defaultEditorBody
    : expressionBodyForEditor(editorBodyText);
  const fullEditorText = expressionForEditor(fullText).trim();
  fullExpressionText = fullEditorText;
  displayedExpressionText = editorBody;
  expr.dataset.fullExpression = fullExpressionText;
  expr.dataset.displayExpression = displayedExpressionText;
  expr.dataset.bindingRefreshValid = 'true';
  if (evaluationReady !== null && evaluationReady !== undefined) {
    expr.dataset.evaluationReady =
      String(evaluationReady).trim().toLowerCase() === 'yes'
        ? 'true'
        : 'false';
  } else {
    delete expr.dataset.evaluationReady;
  }
  expr.value = displayedExpressionText;
  scheduleEditorResizeGrip();
  const bindings = visibleBindingsForCurrentMode(
    hasEvaluatedBindings
      ? editorBindings
      : (compact ? compact.bindings : [])
  );
  renderVariableValues(bindings || []);
  currentVariables = variableNamesFromBindings(bindings || []);
  renderDerivativeButtons(currentVariables);
  if (currentMode() === 'expression' &&
      (evaluationReady === null || evaluationReady === undefined)) {
    scheduleEditedExpressionBindingRefresh();
  }
}

function integratorEditableBindings(bindings) {
  const boundNames = currentIntegratorBoundNames();
  return (Array.isArray(bindings) ? bindings : [])
    .filter((binding) => !boundNames.has(String(binding && binding.name || '').trim()));
}

function applyIntegratorBindingState(data, fallbackExpression) {
  const bindingExpression = expressionWithSortedConstants(
    String(data && data.binding_expression || fallbackExpression || '').trim()
  );
  const editorBody = String(data && data.expression || expr.value || '').trim();
  const editableBindings = integratorEditableBindings(data && data.binding_values);

  if (bindingExpression && bindingParts(bindingExpression)) {
    setExpressionEditor(
      bindingExpression,
      editableBindings,
      editorBody || null
    );
    if (!editableBindings.length)
      clearVariableValues();
    modeEditorText.integrator = bindingExpression;
  } else if (editableBindings.length) {
    renderVariableValues(editableBindings);
  } else {
    clearVariableValues();
  }
}
