/**
 * Derivative and integral actions delegated to native calculation workers.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function matrixCalculusInput(text, wrt, action) {
  const source = String(text || '').trim();

  return action === 'integral'
    ? `@S${source}d${wrt}`
    : `D${wrt}(${source})`;
}

async function takeMatrixCalculus(wrt, action, actionButton = null) {
  commitVisibleBindingInputs();
  await pendingExpressionBindingCommit;
  if (lastMatrixScalarExpression) {
    await takeMatrixScalarCalculus(wrt, action, actionButton);
    return;
  }
  const sourceText = String(currentExpressionText() || '').trim();
  const text = expressionBodyForEditor(sourceText);
  if (!text || !wrt)
    return;

  const variables = [...currentVariables];
  const differentiable = currentDifferentiable;
  const actionLabel = action === 'integral' ? 'integral' : 'derivative';
  const calculusBody = matrixCalculusInput(text, wrt, action);
  const calculusText = expressionWithBindings(calculusBody, compactExpressionForEditor(sourceText).bindings);

  setActionRunning(actionButton, true);
  showResults();
  rightPaneTitle.textContent = action === 'integral'
    ? `${wrt} integral RESULT`
    : `${wrt} ${actionLabel} RESULT`;
  setBusy(true);
  setStatus(action === 'integral'
    ? `Integrating matrix with respect to ${wrt}...`
    : `Differentiating matrix d/d${wrt}...`);
  try {
    const {response, data} = await fetchMatrixEvaluation({
      matrixText: calculusText,
      operation: 'eval',
      operand: '',
      skipSave: true
    });

    if (!response.ok || !data.ok) {
      clearResultDetails({keepBindings: true});
      currentVariables = variables;
      currentDifferentiable = differentiable;
      renderDerivativeButtons(currentVariables);
      setRenderedError(data.error || `No matrix ${actionLabel} for ${wrt}`);
      resetMoreDigitsButton(renderedMore, false);
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
    setRenderedContent(data.svg || '', data.render_error || (data.tex || 'No rendered TeX available'));
    resetMoreDigitsButton(
      renderedMore,
      !!data.full_TeX && !!data.tex && data.full_TeX !== data.tex
    );
    setMatrixExpressionResult(data);
    setResultInputText(data.result || '', data.binding_values || []);
    setExpandableText(
      functionStyle,
      functionMore,
      data.display_function || data.function || '',
      data.full_display_function || data.function || ''
    );
    valueTitle.textContent = data.value ? 'Value' : 'Summary';
    setMatrixValueResult(data);
    setValueCardVisible(!!data.value);
    currentVariables = variables;
    currentDifferentiable = differentiable;
    renderDerivativeButtons(currentVariables);
    commitModeState();
    setStatus('Ready');
  } catch (err) {
    clearResultDetails({keepBindings: true});
    currentVariables = variables;
    currentDifferentiable = differentiable;
    renderDerivativeButtons(currentVariables);
    setRenderedError(String(err));
    resetMoreDigitsButton(renderedMore, false);
    setStatus('Error');
  } finally {
    setBusy(false);
    setActionRunning(actionButton, false);
  }
}

async function takeMatrixScalarCalculus(wrt, action, actionButton = null) {
  const source = lastMatrixScalarExpression;
  const actionLabel = action === 'integral' ? 'integral' : 'derivative';
  const variables = [...currentVariables];
  const differentiable = currentDifferentiable;

  if (!source || !wrt)
    return;
  setActionRunning(actionButton, true);
  showResults();
  rightPaneTitle.textContent = action === 'integral'
    ? `∫ f(${wrt}) d${wrt} RESULT`
    : `${wrt} derivative RESULT`;
  setBusy(true);
  setStatus(action === 'integral'
    ? `Integrating scalar result with respect to ${wrt}...`
    : `Differentiating scalar result d/d${wrt}...`);
  try {
    const {response, data} = await fetchEvaluation(
      source,
      wrt,
      action === 'integral' ? 'integral' : ''
    );
    const resultExpression = action === 'integral'
      ? integralExpressionFromLine(data.integral)
      : derivativeExpressionFromLine(data.derivative);
    const resultTex = action === 'integral' ? data.integral_TeX : data.derivative_TeX;
    const resultSvg = action === 'integral' ? data.integral_svg : data.derivative_svg;
    const resultValue = action === 'integral' ? data.integral_value : data.derivative_value;
    const renderError = action === 'integral'
      ? data.integral_render_error
      : data.derivative_render_error;

    if (!response.ok || !data.ok || !resultExpression)
      throw new Error(data.error || data.raw || `No scalar ${actionLabel} for ${wrt}`);

    clearResultDetails({keepBindings: true});
    clearRenderedError();
    setExpandableText(parsed, parsedMore, resultExpression, resultExpression);
    setResultInputText(resultExpression);
    setMatrixPrettyResult(resultExpression, '');
    setValueText(resultValue || '');
    valueTitle.textContent = data.derivative_values ? 'Values' : 'Value';
    setValueCardVisible(!!resultValue);
    lastTex = resultTex || '';
    rendered.dataset.displayTex = resultTex || '';
    rendered.dataset.fullTex = resultTex || '';
    rendered.dataset.displaySvg = resultSvg || '';
    rendered.dataset.fullSvg = '';
    rendered.dataset.renderError = renderError || '';
    setRenderedContent(resultSvg || '', renderError || resultTex || resultExpression);
    resetMoreDigitsButton(renderedMore, false);
    currentVariables = variables;
    currentDifferentiable = differentiable;
    renderDerivativeButtons(currentVariables);
    setStatus('Ready');
  } catch (err) {
    clearResultDetails({keepBindings: true});
    currentVariables = variables;
    currentDifferentiable = differentiable;
    renderDerivativeButtons(currentVariables);
    setRenderedError(String(err));
    resetMoreDigitsButton(renderedMore, false);
    setStatus('Error');
  } finally {
    setBusy(false);
    setActionRunning(actionButton, false);
  }
}

async function takeDerivative(wrt, actionButton = null) {
  if (currentMode() === 'matrix') {
    await takeMatrixCalculus(wrt, 'derivative', actionButton);
    return;
  }

  const enteredBindings = visibleBindingValues();
  commitVisibleBindingInputs();
  await pendingExpressionBindingCommit;
  const text = expressionWithVisibleBindings(currentExpressionText(), enteredBindings);
  if (!text || !wrt) return;

  setActionRunning(actionButton, true);
  showResults();
  rightPaneTitle.textContent = `${wrt} derivative RESULT`;
  setBusy(true);
  setStatus(`Differentiating d/d${wrt}...`);
  try {
    const {response, data} = await fetchEvaluation(text, wrt);
    const derivativeExpression = derivativeExpressionFromLine(data.derivative);
    const derivativeTex = data.derivative_TeX || '';
    const derivativeSvg = data.derivative_svg || '';
    const derivativeFunction = data.display_derivative_function || data.derivative_function || derivativeExpression || '';
    const fullDerivativeFunction = data.full_display_derivative_function || data.derivative_function || derivativeExpression || '';

    if (!response.ok || !data.ok || !derivativeExpression) {
      clearResultDetails({keepBindings: true});
      setRenderedError(data.error || data.raw || `No derivative for ${wrt}`);
      resetMoreDigitsButton(renderedMore, false);
      setStatus('Error');
      return;
    }

    clearResultDetails({keepBindings: true});
    clearRenderedError();
    setExpandableText(
      parsed,
      parsedMore,
      derivativeExpression,
      derivativeExpression
    );
    setResultInputText(derivativeExpression);
    setExpandableText(
      functionStyle,
      functionMore,
      derivativeFunction,
      fullDerivativeFunction
    );
    setValueText(data.derivative_value || '');
    valueTitle.textContent = data.derivative_values ? 'Values' : 'Value';
    setValueCardVisible(!!data.derivative_value);
    lastDerivativeExpression = derivativeExpression;
    {
      const variableBindings = variableNamesFromBindings(data.binding_values || []);
      currentVariables = variableBindings;
    }
    currentDifferentiable = String(data.differentiable || 'yes').trim().toLowerCase() !== 'no';
    renderDerivativeButtons(currentVariables);
    if (derivativeTex) {
      lastTex = derivativeTex;
      rendered.dataset.displayTex = derivativeTex;
      rendered.dataset.fullTex = derivativeTex;
      rendered.dataset.displaySvg = derivativeSvg;
      rendered.dataset.fullSvg = '';
      rendered.dataset.renderError = data.derivative_render_error || '';
      setRenderedContent(
        derivativeSvg,
        data.derivative_render_error || derivativeTex
      );
      resetMoreDigitsButton(renderedMore, false);
    } else {
      setRenderedContent('', derivativeExpression);
      resetMoreDigitsButton(renderedMore, false);
    }
    setStatus('Ready');
  } catch (err) {
    clearResultDetails({keepBindings: true});
    setRenderedError(String(err));
    resetMoreDigitsButton(renderedMore, false);
    setStatus('Error');
  } finally {
    setBusy(false);
    setActionRunning(actionButton, false);
  }
}

async function takeIntegral(wrt, actionButton = null) {
  if (currentMode() === 'matrix') {
    await takeMatrixCalculus(wrt, 'integral', actionButton);
    return;
  }

  const enteredBindings = visibleBindingValues();
  commitVisibleBindingInputs();
  await pendingExpressionBindingCommit;
  const text = expressionWithVisibleBindings(currentExpressionText(), enteredBindings);
  if (!text || !wrt) return;

  setActionRunning(actionButton, true);
  showResults();
  rightPaneTitle.textContent = `${wrt} integral RESULT`;
  setBusy(true);
  setStatus(`Integrating with respect to ${wrt}...`);
  try {
    const {response, data} = await fetchEvaluation(text, wrt, 'integral');
    const integralExpression = integralExpressionFromLine(data.integral);
    const integralTex = data.integral_TeX || '';
    const integralSvg = data.integral_svg || '';
    const integralWrappedTex = data.integral_wrapped_TeX || integralTex;
    const integralWrappedSvg = data.integral_wrapped_svg || '';
    const integralFunction = data.display_integral_function || data.integral_function || integralExpression || '';
    const fullIntegralFunction = data.full_display_integral_function || data.integral_function || integralExpression || '';

    if (!response.ok || !data.ok || !integralExpression) {
      clearResultDetails({keepBindings: true});
      setRenderedError(data.error || data.raw || `No integral for ${wrt}`);
      resetMoreDigitsButton(renderedMore, false);
      setStatus('Error');
      return;
    }

    clearResultDetails({keepBindings: true});
    clearRenderedError();
    setExpandableText(
      parsed,
      parsedMore,
      integralExpression,
      integralExpression
    );
    setResultInputText(integralExpression);
    setExpandableText(
      functionStyle,
      functionMore,
      integralFunction,
      fullIntegralFunction
    );
    setValueText(data.integral_value || '');
    setValueCardVisible(!!data.integral_value);
    lastDerivativeExpression = '';
    {
      const variableBindings = variableNamesFromBindings(data.binding_values || []);
      currentVariables = variableBindings;
    }
    currentDifferentiable = String(data.differentiable || 'yes').trim().toLowerCase() !== 'no';
    renderDerivativeButtons(currentVariables);
    if (integralTex) {
      lastTex = integralTex;
      rendered.dataset.displayTex = integralTex;
      rendered.dataset.fullTex = integralTex;
      rendered.dataset.displaySvg = integralSvg;
      rendered.dataset.fullSvg = '';
      rendered.dataset.renderError = data.integral_render_error || '';
      rendered.dataset.compactTex = integralTex;
      rendered.dataset.wrappedTex = integralWrappedTex;
      rendered.dataset.compactSvg = integralSvg;
      rendered.dataset.wrappedSvg = integralWrappedSvg;
      rendered.dataset.responsiveFallback =
        data.integral_render_error || integralTex;
      delete rendered.dataset.responsiveVariant;
      setRenderedContent(
        integralSvg,
        data.integral_render_error || integralTex
      );
      scheduleRenderedTeXFit();
      resetMoreDigitsButton(renderedMore, false);
    } else {
      setRenderedContent('', integralExpression);
      resetMoreDigitsButton(renderedMore, false);
    }
    setStatus('Ready');
  } catch (err) {
    clearResultDetails({keepBindings: true});
    setRenderedError(String(err));
    resetMoreDigitsButton(renderedMore, false);
    setStatus('Error');
  } finally {
    setBusy(false);
    setActionRunning(actionButton, false);
  }
}
