/**
 * Worksheet persistence, history snapshots and restoration.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function validPrecisionBits(bits, fallback) {
  const parsed = parseInt(String(bits), 10);
  if (!Number.isFinite(parsed))
    return fallback;
  return Math.max(DOUBLE_PRECISION_DIGITS, Math.min(MAX_PRECISION_BITS, parsed));
}

function validIntegratorIntervalCap(value) {
  const parsed = parseInt(String(value), 10);
  if (!Number.isFinite(parsed))
    return DEFAULT_INTEGRATOR_INTERVAL_CAP;
  const allowed = [500, 5000, 20000, 50000, 100000];
  return allowed.includes(parsed) ? parsed : DEFAULT_INTEGRATOR_INTERVAL_CAP;
}

function validMatrixOperation(value) {
  const operation = String(value || '').trim();
  const allowed = Array.from(matrixOperation.options).map((option) => option.value);
  return allowed.includes(operation) ? operation : 'eval';
}

function validLabMode(value) {
  const mode = String(value || '').trim();
  return mode === 'equation' || mode === 'diffequation' || mode === 'matrix' || mode === 'integrator' || mode === 'datetime' || mode === 'almanac' ? mode : 'expression';
}

function applySavedState(data) {
  const saved = String(data.expression || '').trim();
  if (saved) {
    lastExpressionUpdatedAt = Number(data.expression_updated_at || 0);
    modeEditorText.expression = saved;
    setExpressionEditor(saved);
  }

  const savedMatrix = String(data.matrix || '').trim();
  if (savedMatrix && !savedMatrix.includes('...'))
    modeEditorText.matrix = savedMatrix;

  const savedEquation = String(data.equation || '').trim();
  if (savedEquation)
    modeEditorText.equation = expressionWithSortedConstants(savedEquation);

  const savedDiffequation = String(data.diffequation || '').trim();
  if (savedDiffequation && !savedDiffequation.includes('...'))
    modeEditorText.diffequation = savedDiffequation;

  const savedEquationVariable = String(data.equation_variable || '').trim();
  if (equationVariable)
    equationVariable.value = savedEquationVariable || DEFAULT_EQUATION_VARIABLE_TEXT;

  const savedMatrixOperation = validMatrixOperation(data.matrix_operation);
  if (matrixOperation)
    matrixOperation.value = savedMatrixOperation;

  const savedMatrixOperand = String(data.matrix_operand || '').trim();
  if (matrixOperand)
    matrixOperand.value = savedMatrixOperand;

  const savedIntegrator = String(data.integrator_expression || '').trim();
  if (savedIntegrator && !savedIntegrator.includes('...')) {
    modeEditorText.integrator = savedIntegrator;
    expr.dataset.savedIntegratorExpression = savedIntegrator;
  }

  const savedBounds = String(data.integrator_bounds || '').trim();
  if (savedBounds)
    restoreIntegratorBoundsText(savedBounds);

  const savedCap = validIntegratorIntervalCap(data.integrator_interval_cap);
  if (integratorIntervalCap)
    integratorIntervalCap.value = String(savedCap);

  if (datetimeDate)
    datetimeDate.value = validDateText(data.datetime_date, DEFAULT_DATETIME_DATE);
  if (datetimeJdn)
    datetimeJdn.value = String(data.datetime_jdn || '');
  if (datetimeStart)
    datetimeStart.value = validDateText(data.datetime_start, datetimeDate?.value || DEFAULT_DATETIME_DATE);
  if (datetimeEnd)
    datetimeEnd.value = validDateText(data.datetime_end, datetimeDate?.value || DEFAULT_DATETIME_DATE);
  if (datetimeYear)
    datetimeYear.value = String(data.datetime_year || (datetimeDate?.value || DEFAULT_DATETIME_DATE).slice(0, 4));
  if (datetimeJurisdiction)
    setSelectValue(datetimeJurisdiction, validDatetimeJurisdiction(data.datetime_jurisdiction, DEFAULT_DATETIME_JURISDICTION));
  if (datetimeLatitude)
    datetimeLatitude.value = String(data.datetime_latitude || DEFAULT_DATETIME_LATITUDE);
  if (datetimeLongitude)
    datetimeLongitude.value = String(data.datetime_longitude || DEFAULT_DATETIME_LONGITUDE);
  if (datetimeElevation)
    datetimeElevation.value = String(data.datetime_elevation || DEFAULT_DATETIME_ELEVATION);
  if (datetimeGmtOffset) {
    datetimeGmtOffset.value = String(data.datetime_gmt_offset || DEFAULT_DATETIME_GMT_OFFSET);
    datetimeAutoGmtOffset = String(datetimeGmtOffset.value || '').trim();
    datetimeGmtOffsetTouched = false;
  }
  if (almanacDate)
    almanacDate.value = validDateText(data.almanac_date, DEFAULT_ALMANAC_DATE);
  if (almanacTime)
    almanacTime.value = String(data.almanac_time || DEFAULT_ALMANAC_TIME).trim() || DEFAULT_ALMANAC_TIME;
  if (almanacZone)
    almanacZone.value = String(data.almanac_zone || DEFAULT_ALMANAC_ZONE).trim();
  if (almanacJurisdiction)
    setSelectValue(almanacJurisdiction, validDatetimeJurisdiction(data.almanac_jurisdiction, DEFAULT_DATETIME_JURISDICTION));
  if (almanacLatitude)
    almanacLatitude.value = String(data.almanac_latitude || DEFAULT_ALMANAC_LATITUDE).trim();
  if (almanacLongitude)
    almanacLongitude.value = String(data.almanac_longitude || DEFAULT_ALMANAC_LONGITUDE).trim();
  if (almanacElevation)
    almanacElevation.value = String(data.almanac_elevation || DEFAULT_ALMANAC_ELEVATION).trim();
  almanacVisibilityMode = validAlmanacVisibility(data.almanac_visibility, DEFAULT_ALMANAC_VISIBILITY);

  if (data.precision_bits && typeof data.precision_bits === 'object') {
    Object.entries(data.precision_bits).forEach(([mode, bits]) => {
      if (modePrecisionBits[mode] !== undefined)
        modePrecisionBits[mode] = validPrecisionBits(bits, modePrecisionBits[mode]);
    });
  } else if (data.precision_bits !== undefined) {
    modePrecisionBits.expression = validPrecisionBits(data.precision_bits, modePrecisionBits.expression);
  }
	      workingPrecisionBits = modePrecisionBits[currentMode()] || workingPrecisionBits;
	      syncTownSelectors({selectDefault: false});
  restoreTownSelection(
    datetimeTown,
    datetimeJurisdiction && datetimeJurisdiction.value,
    data.datetime_town,
    datetimeLatitude && datetimeLatitude.value,
    datetimeLongitude && datetimeLongitude.value
  );
  restoreTownSelection(
    almanacTown,
    almanacJurisdiction && almanacJurisdiction.value,
    data.almanac_town,
    almanacLatitude && almanacLatitude.value,
    almanacLongitude && almanacLongitude.value
  );

	      applyLabMode(validLabMode(data.lab_mode));
}

async function loadLastState() {
  try {
    const response = await fetch('/state');
    const data = await response.json();
    applySavedState(data || {});
    try {
      const serverExpression = String(data.expression || '').trim();
      const serverExpressionUpdatedAt = Number(data.expression_updated_at || 0);
      const localExpression = String(localStorage.getItem('mars.exprLab.lastExpression') || '').trim();
      const localExpressionUpdatedAt = Number(localStorage.getItem(EXPRESSION_TIMESTAMP_STORAGE_KEY) || 0);
      if (localExpression && (
        localExpressionUpdatedAt > serverExpressionUpdatedAt ||
        !serverExpression ||
        serverExpression === DEFAULT_EXPRESSION_TEXT
      )) {
        lastExpressionUpdatedAt = localExpressionUpdatedAt || Date.now();
        modeEditorText.expression = localExpression;
        if (currentMode() === 'expression')
          setExpressionEditor(localExpression);
        saveLabState({
          expression: localExpression,
          expression_updated_at: localExpressionUpdatedAt || Date.now()
        });
      }

      const serverEquation = String(data.equation || '').trim();
      const serverEquationUpdatedAt = Number(data.equation_updated_at || 0);
      const localEquation = String(localStorage.getItem('mars.exprLab.lastEquation') || '').trim();
      const localEquationUpdatedAt = Number(localStorage.getItem(EQUATION_TIMESTAMP_STORAGE_KEY) || 0);
      if (localEquation && (
        localEquationUpdatedAt > serverEquationUpdatedAt ||
        !serverEquation ||
        serverEquation === DEFAULT_EQUATION_TEXT
      )) {
        modeEditorText.equation = expressionWithSortedConstants(localEquation);
        if (currentMode() === 'equation')
          restoreModeEditor('equation');
        saveLabState({
          equation: modeEditorText.equation,
          equation_updated_at: localEquationUpdatedAt || Date.now()
        });
      }
    } catch (_) {
      // The server copy remains authoritative when localStorage is unavailable.
    }
    return;
  } catch (_) {
    // Fall back to localStorage below.
  }

  try {
    const saved = localStorage.getItem('mars.exprLab.lastExpression');
    if (saved) {
      lastExpressionUpdatedAt = Number(localStorage.getItem(EXPRESSION_TIMESTAMP_STORAGE_KEY) || Date.now());
      modeEditorText.expression = saved;
      setExpressionEditor(saved);
    }
    const matrixText = localStorage.getItem('mars.exprLab.lastMatrix');
    if (matrixText && !matrixText.includes('...'))
      modeEditorText.matrix = matrixText;
    const matrixOperationText = localStorage.getItem('mars.exprLab.lastMatrixOperation');
    if (matrixOperation && matrixOperationText)
      matrixOperation.value = validMatrixOperation(matrixOperationText);
    const matrixOperandText = localStorage.getItem('mars.exprLab.lastMatrixOperand');
    if (matrixOperand && matrixOperandText !== null)
      matrixOperand.value = matrixOperandText;
    const equationText = localStorage.getItem('mars.exprLab.lastEquation');
    if (equationText)
      modeEditorText.equation = expressionWithSortedConstants(equationText);
    const diffequationText = localStorage.getItem('mars.exprLab.lastDiffequation');
    if (diffequationText && !diffequationText.includes('...'))
      modeEditorText.diffequation = diffequationText;
    const equationVariableText = localStorage.getItem('mars.exprLab.lastEquationVariable');
    if (equationVariable && equationVariableText)
      equationVariable.value = equationVariableText;
    const integratorExpression = localStorage.getItem('mars.exprLab.lastIntegratorExpression');
    if (integratorExpression && !integratorExpression.includes('...'))
      modeEditorText.integrator = integratorExpression;
    const integratorBoundsText = localStorage.getItem('mars.exprLab.lastIntegratorBounds');
    if (integratorBoundsText)
      restoreIntegratorBoundsText(integratorBoundsText);
    const integratorCap = localStorage.getItem('mars.exprLab.lastIntegratorIntervalCap');
    if (integratorIntervalCap && integratorCap)
      integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorCap));
    const datetimeStateText = localStorage.getItem('mars.exprLab.lastDatetimeState');
    if (datetimeStateText) {
      const state = JSON.parse(datetimeStateText);
      if (datetimeDate)
        datetimeDate.value = validDateText(state.date, DEFAULT_DATETIME_DATE);
      if (datetimeJdn)
        datetimeJdn.value = String(state.jdn || '');
      if (datetimeStart)
        datetimeStart.value = validDateText(state.start, datetimeDate?.value || DEFAULT_DATETIME_DATE);
      if (datetimeEnd)
        datetimeEnd.value = validDateText(state.end, datetimeDate?.value || DEFAULT_DATETIME_DATE);
      if (datetimeYear)
        datetimeYear.value = String(state.year || (datetimeDate?.value || DEFAULT_DATETIME_DATE).slice(0, 4));
    if (datetimeJurisdiction)
      setSelectValue(datetimeJurisdiction, validDatetimeJurisdiction(state.jurisdiction, DEFAULT_DATETIME_JURISDICTION));
    if (datetimeLatitude)
      datetimeLatitude.value = String(state.latitude || DEFAULT_DATETIME_LATITUDE);
    if (datetimeLongitude)
      datetimeLongitude.value = String(state.longitude || DEFAULT_DATETIME_LONGITUDE);
    if (datetimeElevation)
      datetimeElevation.value = String(state.elevation || DEFAULT_DATETIME_ELEVATION);
    if (datetimeGmtOffset) {
      datetimeGmtOffset.value = String(state.gmt_offset || DEFAULT_DATETIME_GMT_OFFSET);
      datetimeAutoGmtOffset = String(datetimeGmtOffset.value || '').trim();
      datetimeGmtOffsetTouched = false;
    }
    }
    const almanacStateText = localStorage.getItem('mars.exprLab.lastAlmanacState');
    if (almanacStateText) {
      const state = JSON.parse(almanacStateText);
      if (almanacDate)
        almanacDate.value = validDateText(state.date, DEFAULT_ALMANAC_DATE);
      if (almanacTime)
        almanacTime.value = String(state.time || DEFAULT_ALMANAC_TIME).trim() || DEFAULT_ALMANAC_TIME;
      if (almanacZone)
        almanacZone.value = String(state.zone || DEFAULT_ALMANAC_ZONE).trim();
      if (almanacJurisdiction)
        setSelectValue(almanacJurisdiction, validDatetimeJurisdiction(state.jurisdiction, DEFAULT_DATETIME_JURISDICTION));
      if (almanacLatitude)
        almanacLatitude.value = String(state.latitude || DEFAULT_ALMANAC_LATITUDE).trim();
      if (almanacLongitude)
        almanacLongitude.value = String(state.longitude || DEFAULT_ALMANAC_LONGITUDE).trim();
      if (almanacElevation)
        almanacElevation.value = String(state.elevation || DEFAULT_ALMANAC_ELEVATION).trim();
      almanacVisibilityMode = validAlmanacVisibility(state.visibility, DEFAULT_ALMANAC_VISIBILITY);
    }
	        const labMode = localStorage.getItem(LAB_MODE_STORAGE_KEY);
	        syncTownSelectors({selectDefault: false});
    restoreTownSelection(
      datetimeTown,
      datetimeJurisdiction && datetimeJurisdiction.value,
      datetimeStateText ? JSON.parse(datetimeStateText).town : '',
      datetimeLatitude && datetimeLatitude.value,
      datetimeLongitude && datetimeLongitude.value
    );
    restoreTownSelection(
      almanacTown,
      almanacJurisdiction && almanacJurisdiction.value,
      almanacStateText ? JSON.parse(almanacStateText).town : '',
      almanacLatitude && almanacLatitude.value,
      almanacLongitude && almanacLongitude.value
    );
	        if (labMode)
	          applyLabMode(labMode);
  } catch (_) {
    // Private browsing or locked-down webviews can disable localStorage.
  }
}

function saveLabState(patch, options = {}) {
  const payload = {...patch};
  fetch('/state', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(payload),
    keepalive: !!options.keepalive
  }).catch(() => {
    // Persistence is helpful, not essential.
  });
}

function savePrecisionState() {
  saveLabState({precision_bits: modePrecisionBits});
}

function saveLastLabMode(mode = currentMode()) {
  const labMode = validLabMode(mode);
  try {
    localStorage.setItem(LAB_MODE_STORAGE_KEY, labMode);
  } catch (_) {
    // The lab still works fine without persistence.
  }
  saveLabState({lab_mode: labMode});
}

function saveLastExpression(text, options = {}) {
  text = String(text || '').trim();
  const updatedAt = Date.now();
  lastExpressionUpdatedAt = updatedAt;
  if (text)
    modeEditorText.expression = text;

  try {
    if (text)
      localStorage.setItem('mars.exprLab.lastExpression', text);
    localStorage.setItem(EXPRESSION_TIMESTAMP_STORAGE_KEY, String(updatedAt));
  } catch (_) {
    // The lab still works fine without persistence.
  }

  if (!text)
    return;

  const patch = {
    expression: text,
    expression_updated_at: updatedAt,
    precision_bits: modePrecisionBits
  };
  clearTimeout(expressionStateSaveTimer);
  expressionStateSaveTimer = null;
  if (options.debounce) {
    expressionStateSaveTimer = setTimeout(() => {
      expressionStateSaveTimer = null;
      saveLabState(patch);
    }, 250);
  } else {
    saveLabState(patch, {keepalive: !!options.keepalive});
  }
}

function saveLastMatrixState() {
  const text = String(currentExpressionText() || expr.value || '').trim();
  const operation = validMatrixOperation(matrixOperation && matrixOperation.value);
  const operand = String(matrixOperand && matrixOperand.value || '').trim();
  if (text)
    modeEditorText.matrix = text;

  try {
    if (text)
      localStorage.setItem('mars.exprLab.lastMatrix', text);
    localStorage.setItem('mars.exprLab.lastMatrixOperation', operation);
    localStorage.setItem('mars.exprLab.lastMatrixOperand', operand);
  } catch (_) {
    // The lab still works fine without persistence.
  }

  saveLabState({
    matrix: text,
    matrix_operation: operation,
    matrix_operand: operand,
    precision_bits: modePrecisionBits
  });
}

function saveLastEquationState(options = {}) {
  const text = expressionWithSortedConstants(String(currentExpressionText() || expr.value || '').trim());
  const updatedAt = Date.now();
  if (text)
    modeEditorText.equation = text;

  try {
    if (text)
      localStorage.setItem('mars.exprLab.lastEquation', text);
    localStorage.setItem(EQUATION_TIMESTAMP_STORAGE_KEY, String(updatedAt));
  } catch (_) {
    // The lab still works fine without persistence.
  }

  const patch = {
    equation: text,
    equation_updated_at: updatedAt,
    precision_bits: modePrecisionBits
  };
  clearTimeout(equationStateSaveTimer);
  equationStateSaveTimer = null;
  if (options.debounce) {
    equationStateSaveTimer = setTimeout(() => {
      equationStateSaveTimer = null;
      saveLabState(patch);
    }, 250);
  } else {
    saveLabState(patch, {keepalive: !!options.keepalive});
  }
}

function saveLastDiffequationState() {
  const text = String(currentExpressionText() || expr.value || '').trim();
  if (text)
    modeEditorText.diffequation = text;

  try {
    if (text)
      localStorage.setItem('mars.exprLab.lastDiffequation', text);
  } catch (_) {
    // The lab still works fine without persistence.
  }

  saveLabState({
    diffequation: text,
    precision_bits: modePrecisionBits
  });
}

function saveLastIntegratorState() {
  const text = expressionWithSortedConstants(String(currentExpressionText() || expr.value || '').trim());
  const bounds = currentIntegratorBoundsText();
  const cap = requestedIntegratorIntervalCap();
  if (text)
    modeEditorText.integrator = text;

  try {
    if (text)
      localStorage.setItem('mars.exprLab.lastIntegratorExpression', text);
    if (bounds)
      localStorage.setItem('mars.exprLab.lastIntegratorBounds', bounds);
    localStorage.setItem('mars.exprLab.lastIntegratorIntervalCap', String(cap));
  } catch (_) {
    // The lab still works fine without persistence.
  }

  saveLabState({
    integrator_expression: text,
    integrator_bounds: bounds,
    integrator_interval_cap: cap,
    precision_bits: modePrecisionBits
  });
}

function saveLastDatetimeState() {
  const state = currentDatetimeState();
  modeEditorText.datetime = DEFAULT_DATETIME_TEXT;

  try {
    localStorage.setItem('mars.exprLab.lastDatetimeState', JSON.stringify(state));
  } catch (_) {
    // The lab still works fine without persistence.
  }

  saveLabState({
    datetime_date: state.date,
    datetime_jdn: state.jdn,
    datetime_start: state.start,
    datetime_end: state.end,
    datetime_year: state.year,
    datetime_jurisdiction: state.jurisdiction,
    datetime_town: state.town,
    datetime_latitude: state.latitude,
    datetime_longitude: state.longitude,
    datetime_elevation: state.elevation,
    datetime_gmt_offset: state.gmt_offset,
    precision_bits: modePrecisionBits
  });
}

function saveLastAlmanacState() {
  const state = currentAlmanacState();
  modeEditorText.almanac = DEFAULT_ALMANAC_TEXT;

  try {
    localStorage.setItem('mars.exprLab.lastAlmanacState', JSON.stringify(state));
  } catch (_) {
    // The lab still works fine without persistence.
  }

  saveLabState({
    almanac_date: state.date,
    almanac_time: state.time,
    almanac_zone: state.zone,
    almanac_jurisdiction: state.jurisdiction,
    almanac_town: state.town,
    almanac_latitude: state.latitude,
    almanac_longitude: state.longitude,
    almanac_elevation: state.elevation,
    almanac_visibility: state.visibility,
    precision_bits: modePrecisionBits
  });
}

function modeHistoryStack(store, mode = currentMode()) {
  return store[mode] || [];
}

function currentHistoryLength() {
  return modeHistoryStack(expressionHistory).length;
}

function currentForwardHistoryLength() {
  return modeHistoryStack(forwardHistory).length;
}

function historyStateForMode(mode = currentMode(), textOverride = null) {
  let text = String(
    textOverride === null || textOverride === undefined
      ? (currentExpressionText() || expr.value || '')
      : textOverride
  ).trim();
  const state = {mode, text};

  if (mode === 'equation' && equationVariable) {
    state.variable = String(equationVariable.value || DEFAULT_EQUATION_VARIABLE_TEXT).trim() ||
      DEFAULT_EQUATION_VARIABLE_TEXT;
  } else if (mode === 'matrix') {
    state.operation = matrixOperation.value;
    state.operand = String(matrixOperand.value || '').trim();
  } else if (mode === 'integrator') {
    state.bounds = currentIntegratorBoundsText();
    state.intervalCap = String(validIntegratorIntervalCap(
      integratorIntervalCap && integratorIntervalCap.value
    ));
  } else if (mode === 'datetime') {
    state.datetime = currentDatetimeState();
    if (textOverride === null || textOverride === undefined)
      text = datetimeSummaryText(state.datetime);
    state.text = text || DEFAULT_DATETIME_TEXT;
  } else if (mode === 'almanac') {
    state.almanac = currentAlmanacState();
    if (textOverride === null || textOverride === undefined)
      text = almanacSummaryText(state.almanac);
    state.text = text || DEFAULT_ALMANAC_TEXT;
  }

  return state;
}

function historyStatesEqual(left, right) {
  return JSON.stringify(left || null) === JSON.stringify(right || null);
}

function previousModeStateForHistory(nextState) {
  const previous = modeCommittedState[nextState && nextState.mode || currentMode()];

  if (!previous || historyStatesEqual(previous, nextState))
    return null;
  return previous;
}

function commitModeState(mode = currentMode(), textOverride = null) {
  modeCommittedState[mode] = historyStateForMode(mode, textOverride);
}

function restoreHistoryState(state) {
  if (!state)
    return;

  if (state.mode === 'equation' && equationVariable) {
    equationVariable.value = String(state.variable || DEFAULT_EQUATION_VARIABLE_TEXT).trim() ||
      DEFAULT_EQUATION_VARIABLE_TEXT;
  } else if (state.mode === 'matrix') {
    matrixOperation.value = state.operation || 'eval';
    matrixOperand.value = String(state.operand || '').trim();
  } else if (state.mode === 'integrator') {
    restoreIntegratorBoundsText(state.bounds || DEFAULT_INTEGRATOR_BOUNDS_TEXT);
    if (integratorIntervalCap)
      integratorIntervalCap.value = String(validIntegratorIntervalCap(state.intervalCap));
  } else if (state.mode === 'datetime') {
    const datetimeState = state.datetime || {};
    if (datetimeDate)
      datetimeDate.value = validDateText(datetimeState.date, DEFAULT_DATETIME_DATE);
    if (datetimeJdn)
      datetimeJdn.value = String(datetimeState.jdn || '');
    if (datetimeStart)
      datetimeStart.value = validDateText(datetimeState.start, datetimeDate?.value || DEFAULT_DATETIME_DATE);
    if (datetimeEnd)
      datetimeEnd.value = validDateText(datetimeState.end, datetimeDate?.value || DEFAULT_DATETIME_DATE);
    if (datetimeYear)
      datetimeYear.value = String(datetimeState.year || (datetimeDate?.value || DEFAULT_DATETIME_DATE).slice(0, 4));
    if (datetimeJurisdiction)
      setSelectValue(datetimeJurisdiction, validDatetimeJurisdiction(datetimeState.jurisdiction, DEFAULT_DATETIME_JURISDICTION));
    if (datetimeLatitude)
      datetimeLatitude.value = String(datetimeState.latitude || DEFAULT_DATETIME_LATITUDE);
    if (datetimeLongitude)
      datetimeLongitude.value = String(datetimeState.longitude || DEFAULT_DATETIME_LONGITUDE);
    if (datetimeElevation)
      datetimeElevation.value = String(datetimeState.elevation || DEFAULT_DATETIME_ELEVATION);
    if (datetimeGmtOffset) {
      datetimeGmtOffset.value = String(datetimeState.gmt_offset || DEFAULT_DATETIME_GMT_OFFSET);
      datetimeAutoGmtOffset = String(datetimeGmtOffset.value || '').trim();
      datetimeGmtOffsetTouched = false;
    }
    restoreTownSelection(
      datetimeTown,
      datetimeJurisdiction && datetimeJurisdiction.value,
      datetimeState.town,
      datetimeLatitude && datetimeLatitude.value,
      datetimeLongitude && datetimeLongitude.value
    );
  } else if (state.mode === 'almanac') {
    const almanacState = state.almanac || {};
    if (almanacDate)
      almanacDate.value = validDateText(almanacState.date, DEFAULT_ALMANAC_DATE);
    if (almanacTime)
      almanacTime.value = String(almanacState.time || DEFAULT_ALMANAC_TIME).trim() || DEFAULT_ALMANAC_TIME;
    if (almanacZone)
      almanacZone.value = String(almanacState.zone || DEFAULT_ALMANAC_ZONE).trim();
    if (almanacJurisdiction)
      setSelectValue(almanacJurisdiction, validDatetimeJurisdiction(almanacState.jurisdiction, DEFAULT_DATETIME_JURISDICTION));
    if (almanacLatitude)
      almanacLatitude.value = String(almanacState.latitude || DEFAULT_ALMANAC_LATITUDE).trim();
    if (almanacLongitude)
      almanacLongitude.value = String(almanacState.longitude || DEFAULT_ALMANAC_LONGITUDE).trim();
    if (almanacElevation)
      almanacElevation.value = String(almanacState.elevation || DEFAULT_ALMANAC_ELEVATION).trim();
    almanacVisibilityMode = validAlmanacVisibility(almanacState.visibility, DEFAULT_ALMANAC_VISIBILITY);
    restoreTownSelection(
      almanacTown,
      almanacJurisdiction && almanacJurisdiction.value,
      almanacState.town,
      almanacLatitude && almanacLatitude.value,
      almanacLongitude && almanacLongitude.value
    );
  }

  if (state.mode === 'datetime') {
    expr.value = DEFAULT_DATETIME_TEXT;
    clearExpressionSource();
    clearVariableValues();
  } else if (state.mode === 'almanac') {
    expr.value = DEFAULT_ALMANAC_TEXT;
    clearExpressionSource();
    clearVariableValues();
  } else {
    applyUpdatedBindingExpression(state.text || '');
  }
}

function clearForwardHistory(mode = currentMode()) {
  forwardHistory[mode] = [];
}
