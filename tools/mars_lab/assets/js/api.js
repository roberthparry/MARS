/**
 * Requests to the native mathematical and calendar endpoints.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

async function fetchEvaluation(
  text,
  wrt = '',
  action = '',
  bindingSource = '',
  bindingValue = ''
) {
  const precision = requestedValuePrecision();
  const response = await fetch('/eval', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({
      expression: expressionForEvaluation(text),
      binding_source: expressionForEvaluation(bindingSource),
      binding_value: bindingValue,
      wrt,
      precision,
      action,
      expression_updated_at: lastExpressionUpdatedAt,
      persist_expression: currentMode() === 'expression'
    })
  });
  const data = await response.json();
  return {response, data};
}

async function fetchMatrixEvaluation(options = {}) {
  if (!options.skipSave)
    saveLastMatrixState();
  const matrixText = String(options.matrixText || currentExpressionText() || expr.value.trim()).trim();
  const response = await fetch('/matrix-eval', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({
      matrix: matrixText,
      operation: options.operation || matrixOperation.value,
      operand: options.operand === undefined ? matrixOperand.value.trim() : String(options.operand || '').trim(),
      precision: requestedValuePrecision(),
      transient: !!options.skipSave
    })
  });
  const data = await response.json();
  return {response, data};
}

async function fetchEquationEvaluation() {
  saveLastEquationState();
  const equationText = currentExpressionText() || expr.value.trim();
  const response = await fetch('/equation-eval', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({
      equation: equationText,
      precision: requestedValuePrecision()
    })
  });
  const data = await response.json();
  return {response, data};
}

async function fetchDiffequationEvaluation() {
  saveLastDiffequationState();
  const diffequationText = currentExpressionText() || expr.value.trim();
  const response = await fetch('/diffequation-eval', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({
      diffequation: diffequationText,
      precision: requestedValuePrecision()
    })
  });
  const data = await response.json();
  return {response, data};
}

async function fetchIntegratorEvaluation() {
  const expressionText = currentExpressionText() || expr.value.trim();
  const rows = currentIntegratorRows();
  const activeRows = activeIntegratorRows(rows, expressionText);
  const bounds = activeIntegratorBoundRows(rows, expressionText);
  renderIntegratorRows(activeRows);
  bounds.forEach((bound) => {
    if (bound.lo && !bound.hi)
      throw new Error(`A one-sided bound for ${bound.name} should be entered as an upper bound. Leave lower blank and put the value in upper.`);
  });
  saveLastIntegratorState();
  const response = await fetch('/integrator-eval', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({
      expression: expressionText,
      bounds,
      precision: requestedValuePrecision(),
      max_intervals: requestedIntegratorIntervalCap()
    })
  });
  const data = await response.json();
  return {response, data};
}

async function fetchDatetimeEvaluation() {
  const state = currentDatetimeState();
  saveLastDatetimeState();
  const response = await fetch('/datetime-eval', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(state)
  });
  const raw = await response.text();
  let data;
  try {
    data = raw ? JSON.parse(raw) : {};
  } catch (_) {
    throw new Error(raw || `Datetime request failed with HTTP ${response.status}`);
  }
  return {response, data};
}

async function fetchDatetimeWeather(state, signal) {
  const response = await fetch('/datetime-weather', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({
      date: state.date,
      latitude: state.latitude,
      longitude: state.longitude,
    }),
    signal,
  });
  const raw = await response.text();
  let data;
  try {
    data = raw ? JSON.parse(raw) : {};
  } catch (_) {
    throw new Error(raw || `Weather request failed with HTTP ${response.status}`);
  }
  return {response, data};
}

async function fetchAlmanacEvaluation() {
  const state = currentAlmanacState();
  saveLastAlmanacState();
  const response = await fetch('/almanac-eval', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(state)
  });
  const raw = await response.text();
  let data;
  try {
    data = raw ? JSON.parse(raw) : {};
  } catch (_) {
    throw new Error(raw || `Almanac request failed with HTTP ${response.status}`);
  }
  return {response, data};
}
