/**
 * Mode selection, workspace visibility, precision and action controls.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function createEmptyModeHistory() {
  return {
    expression: [],
    equation: [],
    diffequation: [],
    matrix: [],
    integrator: [],
    datetime: [],
    almanac: []
  };
}

function precisionDigitsForBits(bits) {
  if (bits <= DOUBLE_PRECISION_BITS)
    return DOUBLE_PRECISION_DIGITS;
  return Math.ceil(bits * Math.LOG10E * Math.LN2);
}

function requestedPrecisionBits() {
  const mode = currentMode();
  const bits = modePrecisionBits[mode] ?? workingPrecisionBits;
  return Math.max(DOUBLE_PRECISION_BITS, Math.min(MAX_PRECISION_BITS, bits));
}

function precisionStatusText() {
  const bits = requestedPrecisionBits();
  const digits = requestedValuePrecision();
  return `${digits} digits / ${bits} bits`;
}

function setStatus(text) {
  statusEl.textContent = `${text} · ${precisionStatusText()}`;
}

function currentMode() {
  return currentLabMode;
}

function syncModeTabs() {
  modeTabs.forEach((tab) => {
    const active = tab.dataset.mode === currentLabMode;
    tab.classList.toggle('active', active);
    tab.setAttribute('aria-selected', active ? 'true' : 'false');
    tab.tabIndex = active ? 0 : -1;
  });
}

function setMode(mode, options = {}) {
  const nextMode = mode === 'equation' || mode === 'diffequation' || mode === 'matrix' || mode === 'integrator' || mode === 'datetime' || mode === 'almanac'
    ? mode
    : 'expression';
  const changed = nextMode !== currentLabMode;
  if (changed && datetimeWeatherAbortController) {
    datetimeWeatherAbortController.abort();
    datetimeWeatherAbortController = null;
  }
  if (changed)
    resetEditorManualSize();
  currentLabMode = nextMode;
  workingPrecisionBits = modePrecisionBits[currentLabMode] || workingPrecisionBits;
  syncModeTabs();
  if (!changed && !options.force)
    return false;
  return true;
}

function captureCurrentModeEditor() {
  commitVisibleBindingInputs();
  const mode = currentMode();
  if (mode === 'expression') {
    modeEditorText.expression = currentExpressionText() || expr.value.trim() || modeEditorText.expression;
    saveLastExpression(modeEditorText.expression);
  } else if (mode === 'equation') {
    modeEditorText.equation = currentExpressionText() || expr.value.trim() || modeEditorText.equation;
    saveLastEquationState();
  }
  else if (mode === 'diffequation') {
    modeEditorText.diffequation = currentExpressionText() || expr.value.trim() || modeEditorText.diffequation;
    saveLastDiffequationState();
  }
  else if (mode === 'matrix') {
    modeEditorText.matrix = currentExpressionText() || expr.value.trim() || modeEditorText.matrix;
    saveLastMatrixState();
  } else if (mode === 'integrator') {
    modeEditorText.integrator = currentExpressionText() || expr.value.trim() || modeEditorText.integrator;
    saveLastIntegratorState();
  } else if (mode === 'datetime') {
    modeEditorText.datetime = DEFAULT_DATETIME_TEXT;
    saveLastDatetimeState();
  } else {
    modeEditorText.almanac = DEFAULT_ALMANAC_TEXT;
    saveLastAlmanacState();
  }
}

function restoreModeEditor(mode) {
  if (mode === 'expression') {
    setExpressionEditor(modeEditorText.expression || DEFAULT_EXPRESSION_TEXT);
  } else if (mode === 'equation') {
    setExpressionEditor(modeEditorText.equation || DEFAULT_EQUATION_TEXT);
  } else if (mode === 'diffequation') {
    setExpressionEditor(modeEditorText.diffequation || DEFAULT_DIFFEQUATION_TEXT);
  } else if (mode === 'matrix') {
    const text = modeEditorText.matrix || DEFAULT_MATRIX_TEXT;
    if (bindingParts(text))
      setExpressionEditor(text);
    else {
      expr.value = text;
      clearExpressionSource();
      clearVariableValues();
    }
  } else {
    if (mode === 'datetime') {
      expr.value = DEFAULT_DATETIME_TEXT;
      clearExpressionSource();
      clearVariableValues();
      return;
    }
    if (mode === 'almanac') {
      expr.value = DEFAULT_ALMANAC_TEXT;
      clearExpressionSource();
      clearVariableValues();
      return;
    }
    const text = modeEditorText.integrator || DEFAULT_INTEGRATOR_TEXT;
    if (bindingParts(text))
      setExpressionEditor(text);
    else {
      expr.value = text;
      clearExpressionSource();
      clearVariableValues();
    }
  }
}

function setResultTitles(renderedText, parsedText, functionText, valueText) {
  renderedTitle.textContent = renderedText;
  parsedTitle.textContent = parsedText;
  functionTitle.textContent = functionText;
  functionRun.classList.toggle('hidden', functionText !== 'Function');
  clearFunctionRun();
  valueTitle.textContent = valueText;
}

function setAuxResultCardsVisible(visible) {
  [parsed, functionStyle, value].filter(Boolean).forEach((element) => {
    const card = element.closest('.result-card');
    if (!card)
      return;
    if (!visible && card.classList.contains('expanded-card'))
      collapseResultCards();
    card.classList.toggle('hidden', !visible);
  });
}

function setValueCardVisible(visible) {
  if (!valueCard)
    return;
  if (!visible && valueCard.classList.contains('expanded-card'))
    collapseResultCards();
  valueCard.classList.toggle('hidden', !visible);
  valueCard.toggleAttribute('hidden', !visible);
  // Visibility must not override the selected card's expansion state.
  valueCard.style.removeProperty('display');
}

function snapshotElementState(element) {
  return {
    className: element.className,
    style: element.style.cssText,
    innerHTML: element.innerHTML,
    dataset: {...element.dataset}
  };
}

function restoreElementState(element, state) {
  element.className = state.className || '';
  element.style.cssText = state.style || '';
  element.innerHTML = state.innerHTML || '';
  Object.keys(element.dataset).forEach((key) => {
    delete element.dataset[key];
  });
  Object.entries(state.dataset || {}).forEach(([key, value]) => {
    element.dataset[key] = value;
  });
}

function snapshotButtonState(button) {
  return {
    className: button.className,
    textContent: button.textContent,
    disabled: !!button.disabled,
    dataset: {...button.dataset}
  };
}

function restoreButtonState(button, state) {
  button.className = state.className || '';
  button.textContent = state.textContent || '';
  button.disabled = !!state.disabled;
  Object.keys(button.dataset).forEach((key) => {
    delete button.dataset[key];
  });
  Object.entries(state.dataset || {}).forEach(([key, value]) => {
    button.dataset[key] = value;
  });
}

function hasResultContent() {
  return Boolean(
    rendered.innerHTML.trim() ||
    parsed.textContent.trim() ||
    functionStyle.textContent.trim() ||
    value.textContent.trim()
  );
}

function saveCurrentModeResultState(mode = currentMode()) {
  if (!hasResultContent()) {
    modeResultState[mode] = null;
    return;
  }

  modeResultState[mode] = {
    rendered: snapshotElementState(rendered),
    parsed: snapshotElementState(parsed),
    functionStyle: snapshotElementState(functionStyle),
    value: snapshotElementState(value),
    renderedMore: snapshotButtonState(renderedMore),
    parsedMore: snapshotButtonState(parsedMore),
    functionMore: snapshotButtonState(functionMore),
    valueMore: snapshotButtonState(valueMore),
    resultInputText: resultUseInput.dataset.inputText || '',
    lastTex,
    lastDerivativeExpression,
    currentVariables: [...currentVariables],
    currentDifferentiable
  };
}

function restoreModeResultState(mode = currentMode()) {
  clearFunctionRun();
  const state = modeResultState[mode];
  if (!state) {
    clearResultPane();
    return;
  }

  collapseResultCards();
  restoreElementState(rendered, state.rendered);
  restoreElementState(parsed, state.parsed);
  restoreElementState(functionStyle, state.functionStyle);
  restoreElementState(value, state.value);
  restoreButtonState(renderedMore, state.renderedMore);
  restoreButtonState(parsedMore, state.parsedMore);
  restoreButtonState(functionMore, state.functionMore);
  restoreButtonState(valueMore, state.valueMore);
  setResultInputText(state.resultInputText || '');
  lastTex = state.lastTex || '';
  lastDerivativeExpression = state.lastDerivativeExpression || '';
  currentVariables = Array.isArray(state.currentVariables) ? [...state.currentVariables] : [];
  currentDifferentiable = state.currentDifferentiable !== false;
  renderDerivativeButtons(currentVariables);
  scheduleRenderedTeXFit();
}

function syncMatrixControls() {
  syncRoundedSelect(matrixOperation);
  const needsOperand = currentMode() === 'matrix' && (matrixOperation.value === 'solve' || matrixOperation.value === 'multiply');
  matrixOperand.classList.toggle('hidden', !needsOperand);
  matrixOperandLabel.classList.toggle('hidden', !needsOperand);
  scheduleEditorResizeGrip();
}

function syncModeUI() {
  const mode = currentMode();
  const expressionMode = mode === 'expression';
  const equationMode = mode === 'equation';
  const diffequationMode = mode === 'diffequation';
  const matrixMode = mode === 'matrix';
  const integratorMode = mode === 'integrator';
  const datetimeMode = mode === 'datetime';
  const almanacMode = mode === 'almanac';

  document.body.classList.toggle('datetime-mode', datetimeMode);
  document.body.classList.toggle('almanac-mode', almanacMode);
  document.body.classList.toggle('diffequation-mode', diffequationMode);
  document.body.classList.toggle('matrix-mode', matrixMode);
  matrixControls.classList.toggle('hidden', !matrixMode);
  equationControls.classList.toggle('hidden', !equationMode);
  diffequationControls.classList.toggle('hidden', !diffequationMode);
  integratorControls.classList.toggle('hidden', !integratorMode);
  datetimeControls.classList.toggle('hidden', !datetimeMode);
  almanacControls.classList.toggle('hidden', !almanacMode);
  if (datetimeLocal)
    datetimeLocal.classList.toggle('hidden', !datetimeMode || !String(datetimeLocalBody?.textContent || '').trim());
  targetRow.classList.toggle('hidden', !expressionMode || targetRow.classList.contains('hidden'));
  derivativeButtons.classList.toggle('hidden', !expressionMode && !matrixMode);
  goalSeek.classList.toggle('hidden', !expressionMode);

  if (expressionMode) {
    leftPaneTitle.textContent = 'Expression';
    subtitle.textContent = 'Switch between expression, equation, differential-equation, matrix, and integrator experiments. Each mode runs through a local MARS scratch binary and shows the result on the right.';
    setResultTitles('Rendered TeX', 'Expression', 'Function', 'Value');
    setAuxResultCardsVisible(true);
    setValueCardVisible(Boolean(String(value.textContent || '').trim()));
  } else if (equationMode) {
    leftPaneTitle.textContent = 'Equation';
    subtitle.textContent = 'Enter an equation on the left. The lab tries symbolic isolation first, then numeric solving for all variable bindings.';
    setResultTitles('Rendered TeX', 'Equation', 'Function', 'Solutions');
    setAuxResultCardsVisible(true);
    setValueCardVisible(true);
  } else if (diffequationMode) {
    leftPaneTitle.textContent = 'Differential Equation';
    subtitle.textContent = 'Enter an ordinary differential equation and optional initial or boundary conditions. MARS selects a symbolic solver family and preserves arbitrary constants when conditions are absent.';
    setResultTitles('Solution', 'Differential Equation', 'Solver', 'Solutions');
    setAuxResultCardsVisible(true);
    setValueCardVisible(true);
  } else if (matrixMode) {
    leftPaneTitle.textContent = 'Matrix';
    subtitle.textContent = 'Enter a complete matrix expression on the left, press Evaluate, and inspect its TeX, expression, function, and numerical value.';
    setResultTitles('Rendered TeX', 'Expression', 'Function', 'Value');
    setAuxResultCardsVisible(true);
    setValueCardVisible(false);
  } else if (integratorMode) {
    leftPaneTitle.textContent = 'Integrator';
    subtitle.textContent = 'Enter an integrand expression on the left, stack one or more integral rows, and use Free when a symbol should stay as a parameter. Leave both bounds blank for an antiderivative, or leave lower blank and fill upper to evaluate it there.';
    setResultTitles('Rendered TeX', 'Integrand', 'Exact result', 'Integral');
    setAuxResultCardsVisible(true);
    setValueCardVisible(true);
  } else if (almanacMode) {
    leftPaneTitle.textContent = 'Almanac';
    subtitle.textContent = `Enter date and time in GMT, then zone, latitude, and longitude. The live almanac engine covers ${ALMANAC_COVERAGE_TEXT}.`;
    setResultTitles('Worksheet', '', '', '');
    setAuxResultCardsVisible(false);
  } else {
    leftPaneTitle.textContent = 'Datetime';
    subtitle.textContent = 'Choose dates, a year, and a location. MARS datetime calculates calendar observances, moon phase, solar times, and optional local weather, with jurisdiction holidays added when available.';
    setResultTitles('Overview', 'Date Range', 'Calendar', 'Solar And Moon');
    setAuxResultCardsVisible(true);
    setValueCardVisible(true);
  }

  syncMatrixControls();
  syncHelpCards();
  updateHistoryButtons();
  scheduleWorkspacePanelFit();
}

function textareaCanUseConditionalResize(textarea) {
  return textarea.isConnected &&
    textarea.clientHeight > 0 &&
    textarea.getClientRects().length > 0;
}

function resetEditorManualSize() {
  labTextareas.forEach((textarea) => {
    textarea.classList.remove('editor-manual-size', 'editor-space-limited');
    textarea.style.removeProperty('height');
    textarea.style.removeProperty('max-height');
    delete textarea.dataset.automaticHeight;
  });
}

function syncEditorResizeGrip() {
  editorResizeFrame = 0;
  const visibleTextareas = labTextareas.filter(textareaCanUseConditionalResize);
  const maximumTotalExtraHeight = Math.max(96, Math.min(320, window.innerHeight * 0.35));
  const maximumExtraHeight = maximumTotalExtraHeight / Math.max(1, visibleTextareas.length);

  labTextareas.forEach((textarea) => {
    if (!textareaCanUseConditionalResize(textarea)) {
      textarea.classList.remove('editor-manual-size', 'editor-space-limited');
      textarea.style.removeProperty('height');
      textarea.style.removeProperty('max-height');
      delete textarea.dataset.automaticHeight;
      return;
    }

    const spaceLimited = textarea.scrollHeight > textarea.clientHeight + 1;
    if (spaceLimited && !textarea.classList.contains('editor-manual-size')) {
      const automaticHeight = textarea.getBoundingClientRect().height;
      textarea.dataset.automaticHeight = String(automaticHeight);
      textarea.style.height = `${automaticHeight}px`;
      textarea.style.maxHeight = `${automaticHeight + maximumExtraHeight}px`;
      textarea.classList.add('editor-manual-size');
    }

    if (textarea.classList.contains('editor-manual-size')) {
      const automaticHeight = Number(textarea.dataset.automaticHeight || 0);
      const currentHeight = textarea.getBoundingClientRect().height;
      const manuallyResized = Math.abs(currentHeight - automaticHeight) > 2;
      textarea.style.maxHeight = `${automaticHeight + maximumExtraHeight}px`;
      if (!spaceLimited && !manuallyResized) {
        textarea.classList.remove('editor-manual-size', 'editor-space-limited');
        textarea.style.removeProperty('height');
        textarea.style.removeProperty('max-height');
        delete textarea.dataset.automaticHeight;
      } else {
        textarea.classList.add('editor-space-limited');
      }
    } else {
      textarea.classList.toggle('editor-space-limited', spaceLimited);
    }
  });

}

function scheduleEditorResizeGrip() {
  if (editorResizeFrame)
    cancelAnimationFrame(editorResizeFrame);
  editorResizeFrame = requestAnimationFrame(syncEditorResizeGrip);
}

function scheduleWorkspacePanelFit() {
  scheduleEditorResizeGrip();
}

function syncHelpCards() {
  const mode = currentMode();

  helpCards.forEach((card) => {
    const modes = String(card.dataset.helpModes || '')
      .split(',')
      .map((item) => item.trim())
      .filter(Boolean);
    const visible = !modes.length || modes.includes(mode);
    card.classList.toggle('hidden', !visible);
  });
}

function applyLabMode(mode) {
  setMode(validLabMode(mode), {force: true});
  restoreModeEditor(currentMode());
  if (currentMode() === 'integrator')
    renderIntegratorRows(activeIntegratorRows());
  if (currentMode() === 'datetime') {
    restoreDatetimeDefaultsIfBlank();
    refreshDatetimeJurisdictionLocation().then(() => {
      if (currentMode() === 'datetime')
        saveLastDatetimeState();
    });
  }
  if (currentMode() === 'almanac') {
    restoreAlmanacDefaultsIfBlank();
    saveLastAlmanacState();
  }
  syncModeUI();
  if (currentMode() === 'integrator' && currentIntegratorBoundRows().length === 0)
    resetIntegratorBoundsToDefault();
  if (currentMode() === 'integrator' && integratorIntervalCap)
    integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorIntervalCap.value));
}

function showResults() {
  resultPane.classList.remove('hidden');
  helpPane.classList.add('hidden');
  rightPaneTitle.textContent = 'Result';
  resultUseInput.classList.toggle('hidden', !resultUseInput.dataset.inputText);
  help.textContent = 'Help';
}

function showHelp() {
  resultPane.classList.add('hidden');
  helpPane.classList.remove('hidden');
  rightPaneTitle.textContent = 'Help';
  resultUseInput.classList.add('hidden');
  help.textContent = 'Result';
  setStatus('Help');
}

function toggleHelp() {
  if (helpPane.classList.contains('hidden'))
    showHelp();
  else {
    showResults();
    setStatus('Ready');
  }
}

function variableNamesFromBindings(bindings) {
  return (Array.isArray(bindings) ? bindings : [])
    .filter((binding) => String(binding.kind || 'variable') !== 'constant')
    .map((binding) => String(binding.name || '').trim())
    .filter(Boolean);
}

function visibleBindingsForCurrentMode(bindings) {
  if (currentMode() !== 'integrator')
    return Array.isArray(bindings) ? bindings : [];
  return integratorEditableBindings(bindings);
}

function showTargetEntry() {
  targetRow.classList.remove('hidden');
  goalSeek.textContent = 'Run goal seek';
  goalTarget.focus();
  goalTarget.select();
  setStatus('Enter target');
}

function hideTargetEntry() {
  targetRow.classList.add('hidden');
  goalSeek.textContent = 'Goal seek';
}

async function evaluateCurrentMode(options = {}) {
  if (currentMode() === 'equation') {
    await evaluateEquation(options);
    return;
  }
  if (currentMode() === 'diffequation') {
    await evaluateDiffequation(options);
    return;
  }
  if (currentMode() === 'matrix') {
    await evaluateMatrix(options);
    return;
  }
  if (currentMode() === 'integrator') {
    await evaluateIntegrator(options);
    return;
  }
  if (currentMode() === 'datetime') {
    await evaluateDatetime(options);
    return;
  }
  if (currentMode() === 'almanac') {
    await evaluateAlmanac(options);
    return;
  }
  await evaluateExpression(options);
}

function setBusy(isBusy) {
  const expressionMode = currentMode() === 'expression';
  run.disabled = isBusy || !expressionReadyToEvaluate();
  back.disabled = isBusy || currentHistoryLength() === 0;
  forward.disabled = isBusy || currentForwardHistoryLength() === 0;
  goalSeek.disabled = isBusy || !expressionMode || !canGoalSeek();
  goalSeek.title = goalSeek.disabled && !isBusy && expressionMode
    ? 'Goal seek needs at least one variable binding'
    : '';
  goalTarget.disabled = isBusy;
  lessPrecision.disabled = isBusy || atMinimumPrecision();
  morePrecision.disabled = isBusy || atMaximumPrecision();
  morePrecision.title = !isBusy && atMaximumPrecision()
    ? 'Already at the current maximum precision setting'
    : '';
  Array.from((integratorBoundStack || document.createElement('div')).querySelectorAll('input, button')).forEach((control) => {
    if (isBusy) {
      if (!control.disabled)
        control.dataset.busyDisabled = '1';
      control.disabled = true;
    } else if (control.dataset.busyDisabled === '1') {
      control.disabled = false;
      delete control.dataset.busyDisabled;
    }
  });
  Array.from((datetimeControls || document.createElement('div')).querySelectorAll('input, button, select')).forEach((control) => {
    if (isBusy) {
      if (!control.disabled)
        control.dataset.busyDisabled = '1';
      control.disabled = true;
    } else if (control.dataset.busyDisabled === '1') {
      control.disabled = false;
      delete control.dataset.busyDisabled;
    }
  });
  Array.from((almanacControls || document.createElement('div')).querySelectorAll('input, button, select')).forEach((control) => {
    if (isBusy) {
      if (!control.disabled)
        control.dataset.busyDisabled = '1';
      control.disabled = true;
    } else if (control.dataset.busyDisabled === '1') {
      control.disabled = false;
      delete control.dataset.busyDisabled;
    }
  });
  if (equationVariable)
    equationVariable.disabled = isBusy;
  if (integratorIntervalCap)
    integratorIntervalCap.disabled = isBusy;
  copyButtons.forEach((button) => {
    button.disabled = isBusy;
  });
  moreDigitButtons.forEach((button) => {
    button.disabled = isBusy;
  });
  Array.from(variableValues.querySelectorAll('button')).forEach((button) => {
    button.disabled = isBusy;
  });
  Array.from(variableValues.querySelectorAll('input')).forEach((input) => {
    input.disabled = isBusy;
  });
  Array.from(derivativeButtons.querySelectorAll('button')).forEach((button) => {
    button.disabled = isBusy;
  });
}

function updateHistoryButtons() {
  const expressionMode = currentMode() === 'expression';
  run.disabled = !expressionReadyToEvaluate();
  back.disabled = currentHistoryLength() === 0;
  forward.disabled = currentForwardHistoryLength() === 0;
  lessPrecision.disabled = atMinimumPrecision();
  morePrecision.disabled = atMaximumPrecision();
  goalSeek.disabled = !expressionMode || !canGoalSeek();
  goalSeek.title = goalSeek.disabled && expressionMode
    ? 'Goal seek needs at least one variable binding'
    : '';
  morePrecision.title = atMaximumPrecision()
    ? 'Already at the current maximum precision setting'
    : '';
}

function pushExpressionHistory(entry) {
  const snapshot = typeof entry === 'string'
    ? historyStateForMode(currentMode(), entry)
    : (entry || historyStateForMode());
  const stack = modeHistoryStack(expressionHistory, snapshot.mode);
  const previous = stack[stack.length - 1];

  if (snapshot && snapshot.text && !historyStatesEqual(snapshot, previous))
    stack.push(snapshot);
  clearForwardHistory(snapshot.mode);
  updateHistoryButtons();
}

function renderDerivativeButtons(variables) {
  derivativeButtons.replaceChildren();
  if (!currentDifferentiable) return;
  variables.forEach((name) => {
    const derivativeButton = document.createElement('button');
    derivativeButton.className = 'secondary';
    derivativeButton.type = 'button';
    const variableName = document.createElement('i');
    variableName.textContent = bindingDisplayName(name);
    derivativeButton.append(variableName, ' derivative');
    derivativeButton.addEventListener('click', () => takeDerivative(name, derivativeButton));
    derivativeButtons.appendChild(derivativeButton);
  });
  variables.forEach((name) => {
    const integralButton = document.createElement('button');
    integralButton.className = 'secondary';
    integralButton.type = 'button';
    const variableName = document.createElement('i');
    variableName.textContent = bindingDisplayName(name);
    integralButton.append(variableName, ' integral');
    integralButton.addEventListener('click', () => takeIntegral(name, integralButton));
    derivativeButtons.appendChild(integralButton);
  });
}

function setActionRunning(button, running) {
  if (!button)
    return;
  button.classList.toggle('action-running', running);
  if (running)
    button.setAttribute('aria-busy', 'true');
  else
    button.removeAttribute('aria-busy');
}

function estimateValuePrecision() {
  const style = getComputedStyle(value);
  const canvas = estimateValuePrecision.canvas || document.createElement('canvas');
  const context = canvas.getContext('2d');
  const padLeft = parseFloat(style.paddingLeft) || 0;
  const padRight = parseFloat(style.paddingRight) || 0;
  let charWidth = 9;

  estimateValuePrecision.canvas = canvas;
  if (context) {
    context.font = style.font;
    charWidth = context.measureText('0123456789'.repeat(8)).width / 80 || charWidth;
  }

  const usableWidth = Math.max(0, value.clientWidth - padLeft - padRight);
  const chars = Math.floor(usableWidth / charWidth);

  return Math.max(96, Math.min(220, chars - 3));
}

function requestedValuePrecision() {
  return precisionDigitsForBits(requestedPrecisionBits());
}

function atMinimumPrecision() {
  return requestedPrecisionBits() <= DOUBLE_PRECISION_BITS;
}

function atMaximumPrecision() {
  return requestedPrecisionBits() >= MAX_PRECISION_BITS;
}

function setRequestedPrecisionBits(bits) {
  const mode = currentMode();
  const clamped = Math.max(DOUBLE_PRECISION_BITS, Math.min(MAX_PRECISION_BITS, bits));
  modePrecisionBits[mode] = clamped;
  workingPrecisionBits = clamped;
}

function nextPrecisionStepBits(current) {
  if (current < QFLOAT_PRECISION_BITS)
    return QFLOAT_PRECISION_BITS;
  if (current < 256)
    return 256;
  return Math.min(MAX_PRECISION_BITS, Math.ceil((current + 1) / 128) * 128);
}

function previousPrecisionStepBits(current) {
  if (current <= QFLOAT_PRECISION_BITS)
    return DOUBLE_PRECISION_BITS;
  if (current <= 256)
    return QFLOAT_PRECISION_BITS;
  return Math.max(256, Math.floor((current - 1) / 128) * 128);
}

function evaluateFromKeyboard() {
  if (!expressionReadyToEvaluate()) {
    updateHistoryButtons();
    return;
  }
  clearForwardHistory();
  if (currentMode() === 'equation')
    evaluateEquation();
  else if (currentMode() === 'diffequation')
    evaluateDiffequation();
  else if (currentMode() === 'matrix')
    evaluateMatrix();
  else if (currentMode() === 'integrator')
    evaluateIntegrator();
  else if (currentMode() === 'datetime')
    evaluateDatetime();
  else
    evaluateExpression();
}
