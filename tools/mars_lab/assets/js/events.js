/** Browser event wiring and initial evaluation; loaded last by app.js. */

run.addEventListener('click', async () => {
  setActionRunning(run, true);
  if (currentMode() === 'expression') {
    clearForwardHistory();
    clearGoalSeekRequest();
    hideTargetEntry();
  }
  try {
    await evaluateCurrentMode();
  } finally {
    setActionRunning(run, false);
  }
});

back.addEventListener('click', () => {
  commitVisibleBindingInputs();
  const previous = modeHistoryStack(expressionHistory).pop();
  if (!previous) {
    updateHistoryButtons();
    return;
  }

  const current = historyStateForMode();
  if (current.text)
    modeHistoryStack(forwardHistory).push(current);
  restoreHistoryState(previous);
  evaluateCurrentMode({skipHistoryUpdate: true});
});

forward.addEventListener('click', () => {
  commitVisibleBindingInputs();
  const next = modeHistoryStack(forwardHistory).pop();
  if (!next) {
    updateHistoryButtons();
    return;
  }

  const current = historyStateForMode();
  if (current.text)
    modeHistoryStack(expressionHistory).push(current);
  restoreHistoryState(next);
  evaluateCurrentMode({skipHistoryUpdate: true});
});

expr.addEventListener('keydown', (event) => {
  if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
    event.preventDefault();
    if (currentMode() === 'expression')
      evaluateFromKeyboard();
    else if (currentMode() === 'equation')
      evaluateEquation();
    else if (currentMode() === 'diffequation')
      evaluateDiffequation();
    else if (currentMode() === 'matrix')
      evaluateMatrix();
    else if (currentMode() === 'integrator')
      evaluateIntegrator();
    else
      evaluateDatetime();
  }
});

expr.addEventListener('input', () => {
  if (currentMode() === 'datetime') {
    modeEditorText.datetime = expr.value.trim() || DEFAULT_DATETIME_TEXT;
    updateHistoryButtons();
    return;
  }
  if (currentMode() === 'equation') {
    if (!bindingParts(expr.value)) {
      fullExpressionText = expr.value.trim();
      displayedExpressionText = expr.value.trim();
      expr.dataset.fullExpression = fullExpressionText;
      expr.dataset.displayExpression = displayedExpressionText;
    }
    refreshVariableValuesFromEditor();
    saveLastEquationState({debounce: true});
    updateHistoryButtons();
    return;
  }
  if (currentMode() === 'diffequation') {
    fullExpressionText = expr.value.trim();
    displayedExpressionText = expr.value.trim();
    expr.dataset.fullExpression = fullExpressionText;
    expr.dataset.displayExpression = displayedExpressionText;
    updateHistoryButtons();
    return;
  }
  if (currentMode() === 'matrix' || currentMode() === 'integrator') {
    if (!bindingParts(expr.value)) {
      fullExpressionText = expr.value.trim();
      displayedExpressionText = expr.value.trim();
      expr.dataset.fullExpression = fullExpressionText;
      expr.dataset.displayExpression = displayedExpressionText;
    }
    refreshVariableValuesFromEditor();
    updateHistoryButtons();
    return;
  }
  if (currentMode() !== 'expression') {
    updateHistoryButtons();
    return;
  }
  saveLastExpression(expr.value.trim(), {debounce: true});
  if (expr.value.trim() === (expr.dataset.displayExpression || displayedExpressionText)) {
    clearTimeout(expressionBindingRefreshTimer);
    expressionBindingRefreshSequence++;
    expr.dataset.bindingRefreshValid = 'true';
    updateHistoryButtons();
    return;
  }
  clearGoalSeekRequest();
  lastEvaluationInputText = '';
  scheduleEditedExpressionBindingRefresh();
});

clear.addEventListener('click', () => {
  const current = historyStateForMode();
  if (current.text)
    pushExpressionHistory(current);
  clearForwardHistory();
  expr.value = '';
  if (currentMode() === 'matrix') {
    matrixOperand.value = '';
    matrixOperation.value = 'eval';
  }
  if (currentMode() === 'equation' && equationVariable)
    equationVariable.value = DEFAULT_EQUATION_VARIABLE_TEXT;
  if (currentMode() === 'integrator') {
    resetIntegratorBoundsToBlank();
    if (integratorIntervalCap)
      integratorIntervalCap.value = String(DEFAULT_INTEGRATOR_INTERVAL_CAP);
  }
  if (currentMode() === 'datetime') {
    if (datetimeDate)
      datetimeDate.value = DEFAULT_DATETIME_DATE;
    if (datetimeJdn)
      datetimeJdn.value = '';
    if (datetimeStart)
      datetimeStart.value = DEFAULT_DATETIME_DATE;
    if (datetimeEnd)
      datetimeEnd.value = DEFAULT_DATETIME_DATE;
    if (datetimeYear)
      datetimeYear.value = DEFAULT_DATETIME_DATE.slice(0, 4);
    if (datetimeJurisdiction)
      setSelectValue(datetimeJurisdiction, DEFAULT_DATETIME_JURISDICTION);
    if (datetimeLatitude)
      datetimeLatitude.value = DEFAULT_DATETIME_LATITUDE;
    if (datetimeLongitude)
      datetimeLongitude.value = DEFAULT_DATETIME_LONGITUDE;
    if (datetimeElevation)
      datetimeElevation.value = DEFAULT_DATETIME_ELEVATION;
    if (datetimeGmtOffset) {
      datetimeGmtOffset.value = DEFAULT_DATETIME_GMT_OFFSET;
      datetimeAutoGmtOffset = String(datetimeGmtOffset.value || '').trim();
      datetimeGmtOffsetTouched = false;
    }
    expr.value = DEFAULT_DATETIME_TEXT;
  }
  if (currentMode() === 'almanac') {
    if (almanacDate)
      almanacDate.value = DEFAULT_ALMANAC_DATE;
    if (almanacTime)
      almanacTime.value = DEFAULT_ALMANAC_TIME;
    if (almanacZone)
      almanacZone.value = DEFAULT_ALMANAC_ZONE;
    if (almanacLatitude)
      almanacLatitude.value = DEFAULT_ALMANAC_LATITUDE;
    if (almanacLongitude)
      almanacLongitude.value = DEFAULT_ALMANAC_LONGITUDE;
    if (almanacElevation)
      almanacElevation.value = DEFAULT_ALMANAC_ELEVATION;
    almanacVisibilityMode = DEFAULT_ALMANAC_VISIBILITY;
    expr.value = DEFAULT_ALMANAC_TEXT;
  }
  captureCurrentModeEditor();
  clearExpressionSource();
  hideTargetEntry();
  clearResultPane();
  saveCurrentModeResultState();
  commitModeState();
  updateHistoryButtons();
  setStatus('Ready');
  expr.focus();
});

help.addEventListener('click', toggleHelp);

goalSeek.addEventListener('click', async () => {
  if (currentMode() !== 'expression')
    return;
  commitVisibleBindingInputs();
  const text = currentExpressionText();
  if (!text) return;

  if (targetRow.classList.contains('hidden')) {
    showTargetEntry();
    return;
  }

  showResults();
  setBusy(true);
  setStatus('Goal seeking...');
  try {
    const target = goalTarget.value.trim() || '0';
    await runGoalSeek(text, target);
  } catch (err) {
    setRenderedError(String(err));
    resetMoreDigitsButton(renderedMore, false);
    clearResultDetails({keepBindings: true});
    setStatus('Error');
  } finally {
    setBusy(false);
  }
});

if (modeTabs.length) {
  modeTabs.forEach((tab) => tab.addEventListener('click', () => {
    captureCurrentModeEditor();
    saveCurrentModeResultState();
    if (!setMode(tab.dataset.mode))
      return;
    saveLastLabMode(currentMode());
    hideTargetEntry();
    restoreModeEditor(currentMode());
    syncModeUI();
    restoreModeResultState(currentMode());
    if (currentMode() === 'integrator' && currentIntegratorBoundRows().length === 0)
      resetIntegratorBoundsToDefault();
    if (currentMode() === 'integrator') {
      if (integratorIntervalCap)
        integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorIntervalCap.value));
    }
    if (currentMode() === 'datetime') {
      restoreDatetimeDefaultsIfBlank();
      datetimeDate?.focus();
    } else if (currentMode() === 'almanac') {
      restoreAlmanacDefaultsIfBlank();
      almanacDate?.focus();
    } else {
      expr.focus();
    }
  }));
}

if (matrixOperation)
  matrixOperation.addEventListener('change', () => {
    matrixOperation.value = validMatrixOperation(matrixOperation.value);
    syncMatrixControls();
    if (currentMode() === 'matrix')
      saveLastMatrixState();
  });

if (equationVariable)
  equationVariable.addEventListener('change', () => {
    equationVariable.value = String(equationVariable.value || DEFAULT_EQUATION_VARIABLE_TEXT).trim() ||
      DEFAULT_EQUATION_VARIABLE_TEXT;
    if (currentMode() === 'equation')
      saveLastEquationState();
  });

if (integratorIntervalCap)
  integratorIntervalCap.addEventListener('change', () => {
    integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorIntervalCap.value));
    if (currentMode() === 'integrator')
      saveLastIntegratorState();
  });

[datetimeDate, datetimeJdn, datetimeStart, datetimeEnd, datetimeYear, datetimeJurisdiction, datetimeTown, datetimeLatitude, datetimeLongitude, datetimeElevation, datetimeGmtOffset]
  .filter(Boolean)
  .forEach((control) => {
    if (control === datetimeDate || control === datetimeStart || control === datetimeEnd) {
      control.addEventListener('keydown', (event) => {
        const shell = control.closest('.mars-date-shell');
        const button = shell ? shell.querySelector('[data-date-target]') : null;
        if (!button)
          return;
        if (event.key === 'ArrowDown' || event.key === 'Enter') {
          event.preventDefault();
          openMarsDatePicker(control, button);
        } else if (event.key === 'Escape') {
          closeMarsDatePicker();
        }
      });
    }
    control.addEventListener('change', () => {
      if (control === datetimeDate) {
        if (datetimeYear && datetimeDate.value)
          datetimeYear.value = datetimeDate.value.slice(0, 4);
        if (datetimeJdn)
          datetimeJdn.value = '';
      }
      if (control === datetimeTown) {
        applySelectedTown({
          townSelect: datetimeTown,
          latitudeInput: datetimeLatitude,
          longitudeInput: datetimeLongitude,
          elevationInput: datetimeElevation,
          zoneInput: datetimeGmtOffset,
          dateInput: datetimeDate,
          resetOffsetTouched: true
        });
      } else if (control === datetimeLatitude || control === datetimeLongitude || control === datetimeElevation) {
        clearTownForCustomCoordinates(
          datetimeTown,
          datetimeLatitude,
          datetimeLongitude,
          datetimeElevation
        );
      }
      if (currentMode() === 'datetime') {
        triggerDatetimeAutoEvaluation({
          refreshJurisdiction: control === datetimeJurisdiction || control === datetimeDate,
	              refreshCoordinates: control === datetimeJurisdiction
	            });
	          }
    });
  });

if (almanacTime)
  almanacTime.addEventListener('input', () => {
    const formatted = formatAlmanacTimeInput(almanacTime.value);
    if (formatted === almanacTime.value)
      return;
    almanacTime.value = formatted;
    almanacTime.setSelectionRange(formatted.length, formatted.length);
  });

[almanacDate, almanacTime, almanacZone, almanacJurisdiction, almanacTown, almanacLatitude, almanacLongitude, almanacElevation]
  .filter(Boolean)
  .forEach((control) => {
    if (control === almanacDate) {
      control.addEventListener('keydown', (event) => {
        const shell = control.closest('.mars-date-shell');
        const button = shell ? shell.querySelector('[data-date-target]') : null;
        if (!button)
          return;
        if (event.key === 'ArrowDown' || event.key === 'Enter') {
          event.preventDefault();
          openMarsDatePicker(control, button);
        } else if (event.key === 'Escape') {
          closeMarsDatePicker();
        }
      });
    }
    control.addEventListener('change', () => {
      if (control === almanacTown) {
        applySelectedTown({
          townSelect: almanacTown,
          latitudeInput: almanacLatitude,
          longitudeInput: almanacLongitude,
          elevationInput: almanacElevation,
          zoneInput: almanacZone,
          dateInput: almanacDate
        });
      } else if (control === almanacLatitude || control === almanacLongitude || control === almanacElevation) {
        clearTownForCustomCoordinates(
          almanacTown,
          almanacLatitude,
          almanacLongitude,
          almanacElevation
        );
      }
      if (currentMode() === 'almanac') {
	            triggerAlmanacAutoEvaluation({
	              refreshJurisdiction: control === almanacJurisdiction || control === almanacDate,
	              refreshCoordinates: control === almanacJurisdiction
	            });
	          }
    });
  });

goalTarget.addEventListener('keydown', (event) => {
  if (event.key === 'Enter') {
    event.preventDefault();
    goalSeek.click();
  } else if (event.key === 'Escape') {
    event.preventDefault();
    hideTargetEntry();
    expr.focus();
    setStatus('Ready');
  }
});

morePrecision.addEventListener('click', async () => {
  commitVisibleBindingInputs();
  setRequestedPrecisionBits(nextPrecisionStepBits(requestedPrecisionBits()));
  savePrecisionState();
  setStatus('Precision changed');
  try {
    if (currentMode() === 'expression') {
      const goalSeekSource = currentGoalSeekSource();
      const goalSeekTarget = expr.dataset.goalSeekTarget || '';

      if (goalSeekSource && goalSeekTarget) {
        const solvedExpression = fullExpressionText || currentExpressionText();
        const start = solvedStartValuesForGoalSeek(goalSeekSource, solvedExpression);
        await runGoalSeek(goalSeekSource, goalSeekTarget, start, {skipHistoryUpdate: true});
      } else {
        await evaluateExpression({skipHistoryUpdate: true, reuseLastInput: true});
      }
    }
    else if (currentMode() === 'equation')
      await evaluateEquation();
    else if (currentMode() === 'diffequation')
      await evaluateDiffequation();
    else if (currentMode() === 'matrix')
      await evaluateMatrix();
    else
      await evaluateIntegrator();
  } finally {
    updateHistoryButtons();
  }
});

lessPrecision.addEventListener('click', async () => {
  commitVisibleBindingInputs();
  setRequestedPrecisionBits(previousPrecisionStepBits(requestedPrecisionBits()));
  savePrecisionState();
  setStatus('Precision changed');
  try {
    if (currentMode() === 'expression') {
      const goalSeekSource = currentGoalSeekSource();
      const goalSeekTarget = expr.dataset.goalSeekTarget || '';

      if (goalSeekSource && goalSeekTarget) {
        const solvedExpression = fullExpressionText || currentExpressionText();
        const start = solvedStartValuesForGoalSeek(goalSeekSource, solvedExpression);
        await runGoalSeek(goalSeekSource, goalSeekTarget, start, {skipHistoryUpdate: true});
      } else {
        await evaluateExpression({skipHistoryUpdate: true, reuseLastInput: true});
      }
    }
    else if (currentMode() === 'equation')
      await evaluateEquation();
    else if (currentMode() === 'diffequation')
      await evaluateDiffequation();
    else if (currentMode() === 'matrix')
      await evaluateMatrix();
    else
      await evaluateIntegrator();
  } finally {
    updateHistoryButtons();
  }
});

renderedMore.addEventListener('click', () => {
  toggleRenderedDigits();
});

parsedMore.addEventListener('click', () => {
  toggleTextDigits(parsed, parsedMore);
});

functionMore.addEventListener('click', () => {
  toggleTextDigits(functionStyle, functionMore);
});

functionRun.addEventListener('click', () => {
  void runFunctionCard();
});

valueMore.addEventListener('click', () => {
  toggleTextDigits(value, valueMore);
});

resultUseInput.addEventListener('click', () => {
  void sendResultExpressionToInput();
});

inputCopy.addEventListener('click', async () => {
  commitVisibleBindingInputs();
  const text = currentMode() === 'datetime'
    ? datetimeSummaryText()
    : String(expr.value || '').trim();
  if (!text)
    return;
  try {
    await writeClipboardText(text);
    flashCopyButton(inputCopy, true);
    setStatus('Copied input');
    setTimeout(() => setStatus('Ready'), 1000);
  } catch (err) {
    flashCopyButton(inputCopy, false);
    setStatus(String(err));
  }
});

copyButtons.forEach((button) => {
  button.addEventListener('click', async () => {
    const text = copyTextForTarget(button.dataset.copyTarget);
    if (!text) return;
    try {
      await writeClipboardText(text);
      flashCopyButton(button, true);
      setStatus('Copied');
      setTimeout(() => setStatus('Ready'), 1000);
    } catch (err) {
      flashCopyButton(button, false);
      setStatus(String(err));
    }
  });
});

zoomButtons.forEach((button) => {
  button.addEventListener('click', (event) => {
    event.preventDefault();
    event.stopPropagation();
    const card = button.closest('.result-card');
    if (!card)
      return;
    if (button.hasAttribute('data-zoom-reset'))
      setResultZoom(card, RESULT_ZOOM_DEFAULT_INDEX);
    else
      stepResultZoom(card, Number(button.dataset.zoomStep || 1));
    setStatus(`Zoom ${Math.round(RESULT_ZOOM_LEVELS[resultZoomIndex(card)] * 100)}%`);
  });
});

resultCards.forEach((card) => {
  applyResultZoom(card);
  card.addEventListener('wheel', (event) => {
    if (!event.ctrlKey && !event.metaKey)
      return;
    event.preventDefault();
    stepResultZoom(card, event.deltaY < 0 ? 1 : -1);
  }, {passive: false});
});

window.addEventListener('pagehide', () => {
  if (currentMode() === 'expression')
    saveLastExpression(currentExpressionText() || expr.value.trim(), {keepalive: true});
  else if (currentMode() === 'equation')
    saveLastEquationState({keepalive: true});
});

document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'hidden') {
    if (currentMode() === 'expression')
      saveLastExpression(currentExpressionText() || expr.value.trim(), {keepalive: true});
    else if (currentMode() === 'equation')
      saveLastEquationState({keepalive: true});
  }
});

window.addEventListener('resize', () => {
  resultCards.forEach((card) => applyResultZoom(card));
  scheduleWorkspacePanelFit();
});
window.addEventListener('scroll', scheduleWorkspacePanelFit, {passive: true});

labTextareas.forEach((textarea) => textarea.addEventListener('input', scheduleEditorResizeGrip));
if (typeof ResizeObserver === 'function') {
  const expressionEditorResizeObserver = new ResizeObserver(scheduleEditorResizeGrip);
  labTextareas.forEach((textarea) => expressionEditorResizeObserver.observe(textarea));
}

expandCardButtons.forEach((button) => {
  button.setAttribute('aria-expanded', 'false');
  button.addEventListener('click', () => toggleResultCardExpansion(button));
});

syncModeTabs();
syncModeUI();
restoreIntegratorBoundsText(DEFAULT_INTEGRATOR_BOUNDS_TEXT);
setStatus('Ready');
refreshMobileAccess();
setInterval(refreshMobileAccess, 5000);
window.labInitialEvaluation = loadLastState().finally(() => evaluateActiveModeOnLoad());
