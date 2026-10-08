/**
 * Mode-specific evaluation and installation of native response cards.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

async function evaluateExpression(options = {}) {
  const enteredBindings = visibleBindingValues();
  commitVisibleBindingInputs();
  await pendingExpressionBindingCommit;
  const editorText = currentExpressionText();
  const editorBodyText = String(expr.value || '').trim();
  const enteredExpression = expressionWithVisibleBindings(editorText, enteredBindings);
  const text = options.reuseLastInput && lastEvaluationInputText
    ? lastEvaluationInputText
    : (enteredExpression || editorText);
  saveLastExpression(editorText || text);
  const nextState = historyStateForMode(currentMode(), text);
  const previousState = !options.skipHistoryUpdate
    ? previousModeStateForHistory(nextState)
    : null;
  if (!text) return;
  showResults();
  setBusy(true);
  setStatus('Evaluating...');
  try {
    if (previousState)
      pushExpressionHistory(previousState);
    const {response, data} = await fetchEvaluation(text);

    if (!response.ok || !data.ok) {
      setRenderedError(data.error || 'Evaluation failed');
      resetMoreDigitsButton(renderedMore, false);
      clearResultDetails({keepBindings: true});
      commitModeState();
      setStatus('Error');
      return;
    }

    if (data.partial_error) {
      setRenderedError(data.error || 'Evaluation failed');
      resetMoreDigitsButton(renderedMore, false);
    } else {
      setRenderedResult(data);
    }
    if (data.expression && !data.partial_error)
      setExpressionEditor(
        editorText || text,
        bindingsWithAuthoredValues(data.binding_values, editorText || text, enteredBindings),
        editorBodyText || null,
        data.evaluation_ready
      );
    else if (data.binding_values)
      renderVariableValues(data.binding_values || []);
    setExpandableText(
      parsed,
      parsedMore,
      data.display_expression || (data.expression ? compactExpressionForEditor(data.expression).display : ''),
      data.full_display_expression || data.expression || ''
    );
    setResultInputText(data.editor_expression || data.full_display_expression || data.expression || '');
    setExpandableText(
      functionStyle,
      functionMore,
      data.display_function || data.function || '',
      data.full_display_function || data.function || ''
    );
    setValueText(data.value || '');
    valueNote.textContent = data.value_note || '';
    valueNoteCard.classList.toggle('hidden', !valueNote.textContent.trim());
    valueTitle.textContent = data.root_value ? 'Values' : 'Value';
    setValueCardVisible(Boolean(String(value.textContent || '').trim()));
    lastEvaluationInputText = text;
    if (!data.partial_error)
      saveLastExpression(editorText || fullExpressionText || expr.value.trim());
    lastDerivativeExpression = derivativeExpressionFromLine(data.derivative);
    {
      const variableBindings = variableNamesFromBindings(data.binding_values || []);
      currentVariables = variableBindings;
    }
    currentDifferentiable = String(data.differentiable || 'yes').trim().toLowerCase() !== 'no';
    renderDerivativeButtons(currentVariables);
    commitModeState();
    setStatus(data.partial_error ? 'Error' : 'Ready');
  } catch (err) {
    setRenderedError(String(err));
    resetMoreDigitsButton(renderedMore, false);
    clearResultDetails({keepBindings: true});
    commitModeState();
    setStatus('Error');
  } finally {
    setBusy(false);
    if (!options.skipHistoryUpdate)
      updateHistoryButtons();
  }
}

async function evaluateMatrix(options = {}) {
  commitVisibleBindingInputs();
  const text = currentExpressionText() || expr.value.trim();
  const nextState = historyStateForMode(currentMode(), text);
  const previousState = !options.skipHistoryUpdate
    ? previousModeStateForHistory(nextState)
    : null;
  if (!text)
    return;
  showResults();
  setBusy(true);
  setStatus('Evaluating matrix...');
  try {
    if (previousState)
      pushExpressionHistory(previousState);
    const {response, data} = await fetchMatrixEvaluation();
    if (!response.ok || !data.ok) {
      lastMatrixScalarExpression = '';
      setRenderedError(data.error || 'Matrix evaluation failed');
      resetMoreDigitsButton(renderedMore, false);
      clearResultDetails({keepBindings: true});
      commitModeState();
      setStatus('Error');
      return;
    }

    clearResultDetails({keepBindings: true});
    clearRenderedError();
    lastTex = data.full_TeX || data.tex || '';
    rendered.dataset.displayTex = data.tex || '';
    rendered.dataset.fullTex = data.full_TeX || data.tex || '';
    rendered.dataset.displaySvg = data.svg || '';
    rendered.dataset.fullSvg = '';
    rendered.dataset.renderError = data.render_error || '';
    setRenderedContent(
      data.svg || '',
      data.render_error || (data.tex || 'No rendered TeX available')
    );
    resetMoreDigitsButton(
      renderedMore,
      !!data.full_TeX && !!data.tex && data.full_TeX !== data.tex
    );
    setMatrixExpressionResult(data);
    lastMatrixScalarExpression = data.scalar ? (data.result || '') : '';
    setResultInputText(data.result || '', data.binding_values || []);
    setExpandableText(
      functionStyle,
      functionMore,
      data.display_function || data.function || '',
      data.full_display_function || data.function || ''
    );
    if (data.operation && matrixOperation) {
      matrixOperation.value = validMatrixOperation(data.operation);
      syncRoundedSelect(matrixOperation);
      syncMatrixControls();
    }
    valueTitle.textContent = 'Value';
    setMatrixValueResult(data);
    setValueCardVisible(!!data.value);
    if (Array.isArray(data.binding_values) && data.binding_values.length) {
      const matrixBindings = bindingsWithAuthoredValues(data.binding_values, text);
      const editorBody = expressionBodyForEditor(text);

      setExpressionEditor(editorBody, matrixBindings, editorBody);
    } else {
      clearVariableValues();
    }
    modeEditorText.matrix = currentExpressionText() || text;
    saveLastMatrixState();
    currentVariables = variableNamesFromBindings(data.binding_values || []);
    currentDifferentiable = currentVariables.length > 0;
    renderDerivativeButtons(currentVariables);
    commitModeState();
    setStatus('Ready');
  } catch (err) {
    setRenderedError(String(err));
    resetMoreDigitsButton(renderedMore, false);
    clearResultDetails({keepBindings: true});
    commitModeState();
    setStatus('Error');
  } finally {
    setBusy(false);
    if (!options.skipHistoryUpdate)
      updateHistoryButtons();
  }
}

function equationSolutionText(data) {
  if (data.status === 'no solutions')
    return 'No solutions';
  const valueLines = [];
  const solutionLines = String(data.solutions || '')
    .split('\n')
    .map((line) => line.trim())
    .filter(Boolean);
  const displaySolutionLines = String(data.display_solutions || data.solutions || '')
    .split('\n')
    .map((line) => line.trim())
    .filter(Boolean);
  const numericSolutionLines = Array.isArray(data.numeric_solutions)
    ? data.numeric_solutions.map((line) => String(line).trim()).filter(Boolean)
    : [];
  if (data.interpretation_note) {
    valueLines.push(String(data.interpretation_note));
    valueLines.push('');
  }
  displaySolutionLines.forEach((line) => valueLines.push(line));
  numericSolutionLines.forEach((line, index) => {
    if (!solutionLineIsNumericLiteral(solutionLines[index] || '')) {
      if (valueLines.length && !valueLines.includes(''))
        valueLines.push('');
      valueLines.push(line);
    }
  });
  if (!valueLines.length && data.status)
    valueLines.push(data.status);
  for (const note of [data.search_note, data.family_note]) {
    if (!note)
      continue;
    if (valueLines.length)
      valueLines.push('');
    valueLines.push(String(note));
  }
  return valueLines.join('\n');
}

async function evaluateEquation(options = {}) {
  commitVisibleBindingInputs();
  const text = String(currentExpressionText() || expr.value || '').trim();
  const nextState = historyStateForMode(currentMode(), text);
  const previousState = !options.skipHistoryUpdate
    ? previousModeStateForHistory(nextState)
    : null;
  if (!text)
    return;
  showResults();
  setBusy(true);
  setStatus('Solving equation...');
  try {
    if (previousState)
      pushExpressionHistory(previousState);
    const {response, data} = await fetchEquationEvaluation();
    if (!response.ok || !data.ok) {
      setRenderedError(data.error || 'Equation solving failed');
      resetMoreDigitsButton(renderedMore, false);
      clearResultDetails({keepBindings: true});
      commitModeState();
      setStatus('Error');
      return;
    }

    clearResultDetails({keepBindings: true});
    clearRenderedError();
    lastTex = data.tex || '';
    rendered.dataset.displayTex = data.display_TeX || data.tex || '';
    rendered.dataset.fullTex = data.full_display_TeX || data.tex || '';
    rendered.dataset.displaySvg = data.svg || '';
    rendered.dataset.fullSvg = '';
    rendered.dataset.renderError = data.render_error || '';
    setRenderedContent(data.svg || '', data.render_error || (data.tex || 'No rendered TeX available'));
    resetMoreDigitsButton(
      renderedMore,
      !!data.full_display_TeX &&
        !!data.display_TeX &&
        data.full_display_TeX !== data.display_TeX &&
        hasAbbreviatedValue(data.display_TeX)
    );
    setExpandableText(
      parsed,
      parsedMore,
      data.display_equation || data.equation || '',
      data.full_display_equation || data.equation || ''
    );
    setResultInputText(parsedExpressionText());
    setExpandableText(
      functionStyle,
      functionMore,
      data.function || '',
      data.function || ''
    );
    setValueText(equationSolutionText(data));
    if (Array.isArray(data.binding_values))
      renderVariableValues(data.binding_values);
    else
      clearVariableValues();
    modeEditorText.equation = text;
    saveLastEquationState();
    currentVariables = [];
    currentDifferentiable = false;
    renderDerivativeButtons(currentVariables);
    commitModeState();
    setStatus('Ready');
  } catch (err) {
    setRenderedError(String(err));
    resetMoreDigitsButton(renderedMore, false);
    clearResultDetails({keepBindings: true});
    commitModeState();
    setStatus('Error');
  } finally {
    setBusy(false);
    if (!options.skipHistoryUpdate)
      updateHistoryButtons();
  }
}

async function evaluateDiffequation(options = {}) {
  const text = String(currentExpressionText() || expr.value || '').trim();
  const nextState = historyStateForMode(currentMode(), text);
  const previousState = !options.skipHistoryUpdate
    ? previousModeStateForHistory(nextState)
    : null;
  if (!text)
    return;
  showResults();
  setBusy(true);
  setStatus('Solving differential equation...');
  try {
    if (previousState)
      pushExpressionHistory(previousState);
    const {response, data} = await fetchDiffequationEvaluation();
    if (!response.ok || !data.ok) {
      setRenderedError(data.error || 'Differential-equation solving failed');
      resetMoreDigitsButton(renderedMore, false);
      clearResultDetails({keepBindings: true});
      commitModeState();
      setStatus('Error');
      return;
    }

    clearResultDetails({keepBindings: true});
    clearRenderedError();
    renderedTitle.textContent = data.status === 'series' ? 'Equation and local series' :
      data.status === 'solved' ? 'Equation and solutions' : 'Reduction';
    lastTex = data.display_TeX || data.solutions_TeX || data.problem_TeX || '';
    rendered.dataset.displayTex = lastTex;
    rendered.dataset.fullTex = lastTex;
    rendered.dataset.displaySvg = data.svg || '';
    rendered.dataset.fullSvg = '';
    rendered.dataset.renderError = data.render_error || '';
    rendered.dataset.compactTex = lastTex;
    rendered.dataset.wrappedTex = data.display_wrapped_TeX || lastTex;
    rendered.dataset.compactSvg = data.svg || '';
    rendered.dataset.wrappedSvg = data.wrapped_svg || '';
    rendered.dataset.responsiveFallback =
      data.render_error || lastTex || data.diagnostic || 'No symbolic solution available';
    delete rendered.dataset.responsiveVariant;
    setRenderedContent(
      data.svg || '',
      data.render_error || lastTex || data.diagnostic || 'No symbolic solution available'
    );
    scheduleRenderedTeXFit();
    resetMoreDigitsButton(renderedMore, false);
    setExpandableText(parsed, parsedMore, data.problem || text, data.problem || text);
    setResultInputText(data.input || text);
    const solverDetails = (data.symmetry || data.steps) ? [
      data.symmetry ? `Symmetry: ${data.symmetry}` : '',
      data.steps || ''
    ].filter(Boolean).join('\n\n') : [
      data.solver ? `solver: ${data.solver}` : '',
      data.status ? `status: ${data.status}` : '',
      data.diagnostic || ''
    ].filter(Boolean).join('\n');
    setExpandableText(functionStyle, functionMore, solverDetails, solverDetails);
    functionStyle.classList.remove('equation-function');
    const solverTexSource = data.steps_left_TeX || data.steps_TeX ||
      solverTextToTex(solverDetails);
    if (solverTexSource) {
      try {
        const solverTex = await renderTexSvg(solverTexSource);
        if (solverTex.svg) {
          functionStyle.classList.add('equation-function');
          functionStyle.dataset.solverCompactTex = solverTexSource;
          functionStyle.dataset.solverWrappedTex =
            data.steps_wrapped_TeX || solverTexSource;
          functionStyle.dataset.solverCompactSvg = solverTex.svg;
          functionStyle.dataset.solverWrappedSvg = '';
          delete functionStyle.dataset.solverVariant;
          installSolverTexSvg(solverTex.svg, 'compact');
          scheduleSolverTexFit();
          functionStyle.dataset.fullText = solverDetails;
          functionStyle.dataset.displayText = solverDetails;
        }
      } catch (_err) {
        // Keep the plain-text solver derivation as the rendering fallback.
      }
    }
    setValueText(data.solutions || data.diagnostic || data.status || '');
    setValueCardVisible(true);
    clearVariableValues();
    modeEditorText.diffequation = text;
    saveLastDiffequationState();
    currentVariables = [];
    currentDifferentiable = false;
    renderDerivativeButtons(currentVariables);
    commitModeState();
    setStatus(data.status === 'series' ? 'Local series' : data.status === 'solved' ? 'Ready' : 'Not solved');
  } catch (err) {
    setRenderedError(String(err));
    resetMoreDigitsButton(renderedMore, false);
    clearResultDetails({keepBindings: true});
    commitModeState();
    setStatus('Error');
  } finally {
    setBusy(false);
    if (!options.skipHistoryUpdate)
      updateHistoryButtons();
  }
}

async function evaluateIntegrator(options = {}) {
  commitVisibleBindingInputs();
  const text = currentExpressionText() || expr.value.trim();
  const nextState = historyStateForMode(currentMode(), text);
  const previousState = !options.skipHistoryUpdate
    ? previousModeStateForHistory(nextState)
    : null;
  if (!text)
    return;
  showResults();
  setBusy(true);
  setStatus('Integrating...');
  try {
    if (previousState)
      pushExpressionHistory(previousState);
    const {response, data} = await fetchIntegratorEvaluation();
    if (!response.ok || !data.ok) {
      const errorText = String(data.error || '').trim();
      const rawError = String(data.raw_error || '').trim();
      setRenderedError(
        errorText && errorText !== 'Integration failed'
          ? errorText
          : (rawError || errorText || 'Integration failed')
      );
      resetMoreDigitsButton(renderedMore, false);
      clearResultDetails({keepBindings: true});
      applyIntegratorBindingState(data, text);
      applyIntegratorResultBound(data);
      saveLastIntegratorState();
      commitModeState();
      setStatus('Error');
      return;
    }

    clearResultDetails({keepBindings: true});
    clearRenderedError();
    lastTex = data.full_TeX || data.tex || '';
    rendered.dataset.displayTex = data.tex || '';
    rendered.dataset.fullTex = data.full_TeX || data.tex || '';
    rendered.dataset.displaySvg = data.svg || '';
    rendered.dataset.fullSvg = '';
    rendered.dataset.renderError = data.render_error || '';
    setRenderedContent(
      data.svg || '',
      data.render_error || (data.tex || 'No rendered TeX available')
    );
    resetMoreDigitsButton(
      renderedMore,
      !!data.full_TeX && !!data.tex && data.full_TeX !== data.tex
    );
    setExpandableText(parsed, parsedMore, data.expression || '', data.expression || '');
    setResultInputText(data.antiderivative || '');
    const workUnits = data.work_units || data.intervals || '';
    const workCap = data.work_cap || data.max_intervals || '';
    const statusText = String(data.status || '');
    const symbolicStatus = /symbolic|antiderivative|closed-form|fast path/i.test(statusText);
    const antiderivativeStatus = /antiderivative/i.test(statusText);
    const stoppedEarly = !symbolicStatus && workUnits && workCap && String(workUnits) !== String(workCap);
    const workText = workUnits && workCap
      ? `work used: ${workUnits} / ${workCap}${stoppedEarly ? ' (precision reached)' : ''}`
      : (workUnits ? `work used: ${workUnits}` : '');
    const detailLines = [];
    if (data.antiderivative)
      detailLines.push(`Antiderivative:\n${data.antiderivative}`);
    if (data.symbolic && !antiderivativeStatus)
      detailLines.push(`Definite result:\n${data.symbolic}`);
    const domainText = [data.bound, data.status ? `status: ${data.status}` : '', symbolicStatus ? '' : workText]
      .filter(Boolean)
      .join('\n');
    if (domainText)
      detailLines.push(domainText);
    const detailText = detailLines.join('\n\n');
    setExpandableText(functionStyle, functionMore, detailText, detailText);
    {
      const valueLines = [];
      if (data.value)
        valueLines.push(data.value);
      if (data.error)
        valueLines.push(`error ≈ ${data.error}`);
      if (!valueLines.length && data.status)
        valueLines.push(data.status);
      setValueText(valueLines.join('\n'));
    }
    applyIntegratorBindingState(data, text);
    applyIntegratorResultBound(data);
    saveLastIntegratorState();
    currentVariables = [];
    currentDifferentiable = false;
    renderDerivativeButtons(currentVariables);
    commitModeState();
    setStatus('Ready');
  } catch (err) {
    setRenderedError(String(err));
    resetMoreDigitsButton(renderedMore, false);
    clearResultDetails({keepBindings: true});
    commitModeState();
    setStatus('Error');
  } finally {
    setBusy(false);
    if (!options.skipHistoryUpdate)
      updateHistoryButtons();
  }
}

async function refreshDatetimeWeather(evaluationId, state, overviewData) {
  if (datetimeWeatherAbortController)
    datetimeWeatherAbortController.abort();
  const controller = new AbortController();
  datetimeWeatherAbortController = controller;
  setStatus('Loading weather...');

  try {
    const {response, data} = await fetchDatetimeWeather(state, controller.signal);
    if (evaluationId !== datetimeEvaluationSequence || currentMode() !== 'datetime' || controller.signal.aborted)
      return;
    if (!response.ok || !data.ok)
      throw new Error(data.error || 'Weather request failed');

    const baseSections = (Array.isArray(overviewData.overview_sections) ? overviewData.overview_sections : [])
      .filter((section) => String(section && section.title || '') !== 'Weather');
    const weatherSections = Array.isArray(data.overview_sections) ? data.overview_sections : [];
    const overviewText = [overviewData.overview || '', data.overview || '']
      .filter(Boolean)
      .join('\n');
    renderDatetimeSections(rendered, null, [...baseSections, ...weatherSections], overviewText);
    setStatus('Ready');
  } catch (err) {
    if (evaluationId !== datetimeEvaluationSequence || currentMode() !== 'datetime' || controller.signal.aborted)
      return;
    setStatus('Weather unavailable');
  } finally {
    if (datetimeWeatherAbortController === controller)
      datetimeWeatherAbortController = null;
  }
}

async function evaluateDatetime(options = {}) {
  const evaluationId = ++datetimeEvaluationSequence;
  const state = currentDatetimeState();
  const snapshotText = datetimeSummaryText(state);
  const nextState = historyStateForMode(currentMode(), snapshotText);
  const previousState = !options.skipHistoryUpdate
    ? previousModeStateForHistory(nextState)
    : null;
  showResults();
  setBusy(true);
  setStatus('Calculating dates...');
  try {
    if (previousState)
      pushExpressionHistory(previousState);
    const {response, data} = await fetchDatetimeEvaluation();
    if (evaluationId !== datetimeEvaluationSequence || currentMode() !== 'datetime')
      return;
    if (!response.ok || !data.ok) {
      setRenderedError(data.error || 'Datetime calculation failed');
      resetMoreDigitsButton(renderedMore, false);
      setDatetimeLocalText('');
      clearResultDetails({keepBindings: true});
      commitModeState();
      setStatus('Error');
      return;
    }

    clearResultDetails({keepBindings: true});
    clearRenderedError();
    renderDatetimeSections(rendered, null, data.overview_sections || [], data.overview || '');
    resetMoreDigitsButton(renderedMore, false);
    renderDatetimeSections(parsed, parsedMore, data.range_sections || [], data.range || '');
    renderDatetimeSections(functionStyle, functionMore, data.calendar_sections || [], data.calendar || '');
    renderDatetimeSections(value, null, data.solar_sections || [], data.solar || '');
    setDatetimeLocalText(data.local || '', data.local_sections || []);
    if (data.fields) {
      if (datetimeDate && data.fields.date)
        datetimeDate.value = validDateText(data.fields.date, datetimeDate.value || DEFAULT_DATETIME_DATE);
      if (datetimeJdn)
        datetimeJdn.value = String(data.fields.julian_day_number || '');
      if (datetimeYear && datetimeDate && datetimeDate.value)
        datetimeYear.value = datetimeDate.value.slice(0, 4);
      if (datetimeGmtOffset) {
        const currentOffset = String(datetimeGmtOffset.value || '').trim();
        const returnedOffset = String(data.fields.gmt_offset || '').trim();
        if (returnedOffset && (!datetimeGmtOffsetTouched || !currentOffset || currentOffset === datetimeAutoGmtOffset)) {
          datetimeGmtOffset.value = returnedOffset;
          datetimeAutoGmtOffset = returnedOffset;
          datetimeGmtOffsetTouched = false;
        }
      }
    }
    setResultInputText('');
    clearVariableValues();
    currentVariables = [];
    currentDifferentiable = false;
    renderDerivativeButtons(currentVariables);
    saveLastDatetimeState();
    commitModeState();
    setStatus('Ready');
    void refreshDatetimeWeather(evaluationId, {
      ...state,
      date: String(data.fields && data.fields.date || state.date),
      latitude: String(data.fields && data.fields.latitude || state.latitude),
      longitude: String(data.fields && data.fields.longitude || state.longitude),
    }, data);
  } catch (err) {
    if (evaluationId !== datetimeEvaluationSequence || currentMode() !== 'datetime')
      return;
    setRenderedError(String(err));
    resetMoreDigitsButton(renderedMore, false);
    setDatetimeLocalText('');
    clearResultDetails({keepBindings: true});
    commitModeState();
    setStatus('Error');
  } finally {
    if (evaluationId === datetimeEvaluationSequence)
      setBusy(false);
    if (!options.skipHistoryUpdate)
      updateHistoryButtons();
  }
}

async function evaluateAlmanac(options = {}) {
  const evaluationId = ++almanacEvaluationSequence;
  const state = currentAlmanacState();
  const snapshotText = almanacSummaryText(state);
  const nextState = historyStateForMode(currentMode(), snapshotText);
  const previousState = !options.skipHistoryUpdate
    ? previousModeStateForHistory(nextState)
    : null;
  showResults();
  setBusy(true);
  setStatus('Working the almanac...');
  try {
    if (previousState)
      pushExpressionHistory(previousState);
    const {response, data} = await fetchAlmanacEvaluation();
    if (evaluationId !== almanacEvaluationSequence || currentMode() !== 'almanac')
      return;
    if (!response.ok || !data.ok) {
      setRenderedError(data.error || 'Almanac calculation failed');
      resetMoreDigitsButton(renderedMore, false);
      clearResultDetails({keepBindings: true});
      commitModeState();
      setStatus('Error');
      return;
    }

    clearResultDetails({keepBindings: true});
    clearRenderedError();
    almanacLastWorksheetData = data;
    renderAlmanacWorksheet(rendered, data);
    refreshAlmanacLandTotality(data);
    resetMoreDigitsButton(renderedMore, false);
    setExpandableText(parsed, parsedMore, '', '');
    setExpandableText(functionStyle, functionMore, '', '');
    setValueText('');
    if (data.fields) {
      if (almanacZone && data.fields.zone)
        almanacZone.value = String(data.fields.zone || '').trim();
      if (almanacLatitude && data.fields.latitude)
        almanacLatitude.value = String(data.fields.latitude || '').trim();
      if (almanacLongitude && data.fields.longitude)
        almanacLongitude.value = String(data.fields.longitude || '').trim();
      if (almanacElevation && data.fields.elevation)
        almanacElevation.value = String(data.fields.elevation || '').trim();
      if (almanacJurisdiction && data.fields.jurisdiction)
        setSelectValue(almanacJurisdiction, validDatetimeJurisdiction(data.fields.jurisdiction, DEFAULT_DATETIME_JURISDICTION));
    }
    setResultInputText('');
    clearVariableValues();
    currentVariables = [];
    currentDifferentiable = false;
    renderDerivativeButtons(currentVariables);
    saveLastAlmanacState();
    commitModeState();
    setStatus('Ready');
  } catch (err) {
    if (evaluationId !== almanacEvaluationSequence || currentMode() !== 'almanac')
      return;
    setRenderedError(String(err));
    resetMoreDigitsButton(renderedMore, false);
    clearResultDetails({keepBindings: true});
    commitModeState();
    setStatus('Error');
  } finally {
    if (evaluationId === almanacEvaluationSequence)
      setBusy(false);
    if (!options.skipHistoryUpdate)
      updateHistoryButtons();
  }
}

async function evaluateActiveModeOnLoad() {
  if (currentMode() === 'matrix') {
    return evaluateMatrix();
  }
  if (currentMode() === 'equation') {
    return evaluateEquation();
  }
  if (currentMode() === 'diffequation') {
    return evaluateDiffequation();
  }
  if (currentMode() === 'integrator') {
    return evaluateIntegrator();
  }
  if (currentMode() === 'datetime') {
    await refreshDatetimeJurisdictionLocation();
    return evaluateDatetime();
  }
  if (currentMode() === 'almanac') {
    return evaluateAlmanac();
  }
  return evaluateExpression();
}

async function runGoalSeek(sourceText, target, start = {}, options = {}) {
  const request = goalSeekExpressionAndStarts(sourceText, start);
  const response = await fetch('/goal_seek', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({
      expression: expressionForEvaluation(request.expression),
      target,
      start: request.start,
      precision: requestedValuePrecision()
    })
  });
  const data = await response.json();

  if (!response.ok || !data.ok) {
    setRenderedError(data.error || 'Goal seek failed');
    resetMoreDigitsButton(renderedMore, false);
    clearResultDetails({keepBindings: true});
    setStatus('Error');
    return false;
  }

  if (!options.skipHistoryUpdate)
    pushExpressionHistory(currentExpressionText());

  const solvedExpression = data.expression || sourceText;
  const solvedWithoutNan = expressionForEditor(solvedExpression).trim();
  const sourceWithoutNan = expressionForEditor(sourceText).trim();
  const unchanged = solvedWithoutNan === sourceWithoutNan;
  const editorBody = expressionBodyForEditor(sourceText);
  const editorExpression = expressionWithBindings(editorBody, data.binding_values || []) || editorBody;
  setRenderedResult(data);
  setExpressionEditor(
    editorExpression,
    data.binding_values || null,
    editorBody,
    data.evaluation_ready
  );
  setExpandableText(
    parsed,
    parsedMore,
    data.display_expression || compactExpressionForEditor(solvedExpression).display,
    data.full_display_expression || solvedExpression
  );
  setResultInputText(data.full_display_expression || solvedExpression);
  setExpandableText(
    functionStyle,
    functionMore,
    data.display_function || data.function || '',
    data.full_display_function || data.function || ''
  );
  setValueText(data.value || '');
  lastEvaluationInputText = editorExpression;
  lastDerivativeExpression = '';
  {
    const variableBindings = variableNamesFromBindings(data.binding_values || []);
    currentVariables = variableBindings;
  }
  currentDifferentiable = String(data.differentiable || 'yes').trim().toLowerCase() !== 'no';
  renderDerivativeButtons(currentVariables);
  expr.dataset.goalSeekSource = expressionForEditor(request.expression).trim();
  expr.dataset.goalSeekTarget = target;
  hideTargetEntry();
  setStatus(unchanged ? 'Goal already reached' : 'Goal reached');
  return true;
}
