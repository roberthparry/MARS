/**
 * Installing, copying and reusing native result representations.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function copyTextForTarget(target) {
  if (target === 'rendered') return rendered.classList.contains('error') ? rendered.textContent : lastTex;
  if (target === 'expression') return parsedExpressionText();
  if (target === 'function') return functionStyle.dataset.fullText || functionStyle.textContent;
  if (target === 'value') return value.dataset.fullText || value.textContent;
  if (target === 'mobile') {
    const url = mobileUrl ? mobileUrl.textContent.trim() : '';
    return /^https?:\/\//.test(url) ? url : '';
  }
  return '';
}

function parsedExpressionText() {
  return expressionForEditor(
    parsed.dataset.fullText ||
    parsed.dataset.displayText ||
    parsed.textContent ||
    ''
  ).trim();
}

function setResultInputText(text, bindings = null) {
  const inputText = expressionForEditor(String(text || '')).trim();
  resultUseInput.dataset.inputText = inputText;
  resultInputBindings = Array.isArray(bindings)
    ? bindings.map((binding) => ({...binding}))
    : [];
  resultUseInput.disabled = !inputText;
  resultUseInput.classList.toggle('hidden', !inputText);
  resultUseInput.title = inputText
    ? 'Send this result to the input pane'
    : 'No reusable result is available';
}

function resultExpressionTextForInput() {
  if (currentMode() === 'equation')
    return parsedExpressionText();
  return (resultUseInput.dataset.inputText || parsedExpressionText()).trim();
}

async function sendResultExpressionToInput() {
  const resultText = resultExpressionTextForInput();
  if (!resultText)
    return;

  const current = historyStateForMode();
  const next = historyStateForMode(currentMode(), resultText);
  if (current.text && !historyStatesEqual(current, next))
    pushExpressionHistory(current);

  clearGoalSeekRequest();
  hideTargetEntry();
  if (currentMode() === 'matrix') {
    const sourceBindings = compactExpressionForEditor(currentExpressionText()).bindings || [];
    const bindings = resultInputBindings.length
      ? bindingsWithAuthoredValues(resultInputBindings, expressionWithBindings(resultText, sourceBindings))
      : sourceBindings;
    setExpressionEditor(expressionWithBindings(resultText, bindings), bindings, resultText);
    matrixOperation.value = 'eval';
    matrixOperand.value = '';
    syncRoundedSelect(matrixOperation);
    syncMatrixControls();
  } else if (currentMode() === 'equation' || currentMode() === 'diffequation')
    setExpressionEditor(resultText);
  else if (!await applyMarsBindingExpression(resultText, resultText))
    return;
  saveCurrentModeEditorState();
  updateHistoryButtons();
  expr.focus();
  setStatus('Result sent to input');
}

function parseMatrixResultText(text) {
  const source = String(text || '').trim();
  if (!source.startsWith('(') || !source.endsWith(')'))
    return null;

  const body = source.slice(1, -1).trim();
  if (!body)
    return [[]];

  const rows = splitTopLevel(body, ';')
    .map((row) => splitTopLevel(row, ',').map((cell) => cell.trim()));
  if (!rows.length)
    return null;

  const cols = rows[0].length;
  if (!cols || rows.some((row) => row.length !== cols))
    return null;
  return rows;
}

function parseMatrixDisplayTerm(text) {
  const source = String(text || '').trim();
  const directRows = parseMatrixResultText(source);
  if (directRows)
    return {factor: '', rows: directRows};

  let depth = 0;
  for (let index = 0; index < source.length; index += 1) {
    const char = source[index];
    if ('([{'.includes(char)) {
      depth += 1;
      continue;
    }
    if (')]}'.includes(char)) {
      depth = Math.max(0, depth - 1);
      continue;
    }
    if (depth || (char !== '.' && char !== '·'))
      continue;

    const factor = source.slice(0, index).trim();
    const rows = parseMatrixResultText(source.slice(index + 1).trim());
    if (factor && rows)
      return {factor, rows};
  }
  return null;
}

function setMatrixPrettyResult(resultText, prettyText, element = functionStyle, moreButton = functionMore) {
  const terms = splitTopLevel(String(resultText || ''), '+')
    .map((term) => parseMatrixDisplayTerm(term));
  const matrixTerms = terms.length && terms.every((term) => term) ? terms : null;
  element.classList.add('matrix-pretty');
  element.dataset.displayText = prettyText || resultText || '';
  element.dataset.fullText = prettyText || resultText || '';
  if (moreButton)
    resetMoreDigitsButton(moreButton, false);

  if (!matrixTerms) {
    renderMatrixSectionHeadings(element, prettyText || resultText || '');
    return;
  }

  element.replaceChildren();
  const sum = document.createElement('span');
  sum.className = 'matrix-sum-display';
  matrixTerms.forEach((term, matrixIndex) => {
    if (matrixIndex) {
      const operator = document.createElement('span');
      operator.className = 'matrix-sum-operator';
      operator.textContent = '+';
      sum.appendChild(operator);
    }

    const termDisplay = document.createElement('span');
    termDisplay.className = 'matrix-term-display';
    if (term.factor) {
      const factor = document.createElement('span');
      factor.className = 'matrix-factor';
      factor.textContent = term.factor;
      termDisplay.appendChild(factor);

      const product = document.createElement('span');
      product.className = 'matrix-product-operator';
      product.textContent = '·';
      termDisplay.appendChild(product);
    }

    const display = document.createElement('span');
    display.className = 'matrix-display';

    const left = document.createElement('span');
    left.className = 'matrix-bracket';
    left.textContent = '(';
    display.appendChild(left);

    const grid = document.createElement('span');
    grid.className = 'matrix-grid';
    grid.style.gridTemplateColumns = element === value || element === parsed
      ? `repeat(${term.rows[0].length}, minmax(0, 1fr))`
      : `repeat(${term.rows[0].length}, max-content)`;
    term.rows.forEach((row) => {
      row.forEach((cellText) => {
        const cell = document.createElement('span');
        cell.className = 'matrix-cell';
        cell.textContent = cellText;
        grid.appendChild(cell);
      });
    });
    display.appendChild(grid);

    const right = document.createElement('span');
    right.className = 'matrix-bracket';
    right.textContent = ')';
    display.appendChild(right);
    termDisplay.appendChild(display);
    sum.appendChild(termDisplay);
  });
  element.appendChild(sum);
}

function setMatrixExpressionResult(data) {
  const fullExpression = data.expression_pretty || data.expression || data.result || '';
  const displayExpression = data.display_expression_pretty || fullExpression;

  parsed.classList.remove('matrix-pretty', 'matrix-expression-pretty');
  parsed.classList.add('matrix-expression-text');
  setExpandableText(parsed, parsedMore, displayExpression, fullExpression);
}

function setMatrixValueResult(data) {
  const fullText = String(data.value || '');
  const svg = String(data.value_svg || '');

  value.classList.remove('matrix-pretty');
  value.classList.toggle('matrix-tex-value', !!svg);
  value.dataset.displayText = fullText;
  value.dataset.fullText = fullText;
  resetMoreDigitsButton(valueMore, false);
  if (!svg) {
    setValueText(fullText);
    return;
  }

  value.replaceChildren();
  const frame = document.createElement('span');
  frame.className = 'rendered-zoom-frame';
  frame.innerHTML = svg;
  value.appendChild(frame);
  const card = value.closest('.result-card');
  if (card)
    requestAnimationFrame(() => applyResultZoom(card));
}

function setRenderedResult(data) {
  const displayTex = data.display_TeX || data.tex || '';
  const fullDisplayTex = data.full_display_TeX || data.tex || '';

  clearRenderedError();
  lastTex = data.tex || '';
  rendered.dataset.displayTex = displayTex;
  rendered.dataset.fullTex = fullDisplayTex;
  rendered.dataset.displaySvg = data.svg || '';
  rendered.dataset.fullSvg = '';
  rendered.dataset.renderError = data.render_error || '';
  rendered.dataset.compactTex = displayTex;
  rendered.dataset.wrappedTex = data.display_wrapped_TeX || displayTex;
  rendered.dataset.compactSvg = data.svg || '';
  rendered.dataset.wrappedSvg = data.display_wrapped_svg || '';
  rendered.dataset.responsiveFallback =
    data.render_error || data.display_expression || data.expression || 'Could not render result';
  rendered.dataset.responsiveFit = data.display_wrapped_svg ? 'true' : 'false';
  delete rendered.dataset.responsiveVariant;
  setRenderedContent(
    data.svg || '',
    data.display_expression || data.expression || 'Could not render result'
  );
  scheduleRenderedTeXFit();
  resetMoreDigitsButton(
    renderedMore,
    !!fullDisplayTex &&
      !!displayTex &&
      fullDisplayTex !== displayTex &&
      hasAbbreviatedValue(displayTex)
  );
}

async function renderTexSvg(tex) {
  const response = await fetch('/render_TeX', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({tex})
  });
  const data = await response.json();
  if (!response.ok || !data.ok)
    throw new Error(data.error || 'Could not render TeX');
  return data;
}

function solverTextToTex(text) {
  const escapeTex = value => String(value || '')
    .replaceAll('\\', String.raw`\textbackslash{}`)
    .replaceAll('&', String.raw`\&`)
    .replaceAll('%', String.raw`\%`)
    .replaceAll('$', String.raw`\$`)
    .replaceAll('#', String.raw`\#`)
    .replaceAll('_', String.raw`\_`)
    .replaceAll('{', String.raw`\{`)
    .replaceAll('}', String.raw`\}`)
    .replaceAll('^', String.raw`\textasciicircum{}`)
    .replaceAll('~', String.raw`\textasciitilde{}`);
  const rows = String(text || '').split('\n').map(line =>
    line.trim()
      ? String.raw`&\text{${escapeTex(line)}}`
      : String.raw`&\text{\phantom{X}}`
  );
  return String.raw`\begin{aligned}[t]${rows.join(String.raw`\\`)}\end{aligned}`;
}

function toggleTextDigits(element, button) {
  const expanded = button.dataset.expanded === 'true';
  if (expanded) {
    renderResultText(element, element.dataset.displayText || element.textContent);
    button.textContent = 'Show more digits';
    button.dataset.expanded = 'false';
  } else {
    renderResultText(element, element.dataset.fullText || element.textContent);
    button.textContent = 'Show fewer digits';
    button.dataset.expanded = 'true';
  }
}

async function toggleRenderedDigits() {
  const expanded = renderedMore.dataset.expanded === 'true';

  if (expanded) {
    setRenderedContent(rendered.dataset.displaySvg || '', rendered.dataset.renderError || '');
    renderedMore.textContent = 'Show more digits';
    renderedMore.dataset.expanded = 'false';
    return;
  }

  if (!rendered.dataset.fullSvg) {
    renderedMore.disabled = true;
    setStatus('Rendering full TeX...');
    try {
      const data = await renderTexSvg(rendered.dataset.fullTex || lastTex);
      rendered.dataset.fullSvg = data.svg || '';
      rendered.dataset.fullRenderError = data.render_error || '';
    } catch (err) {
      rendered.dataset.fullRenderError = String(err);
    } finally {
      renderedMore.disabled = false;
      setStatus('Ready');
    }
  }

  setRenderedContent(
    rendered.dataset.fullSvg || '',
    rendered.dataset.fullRenderError || 'No rendered TeX available'
  );
  renderedMore.textContent = 'Show fewer digits';
  renderedMore.dataset.expanded = 'true';
}

async function writeClipboardText(text) {
  if (navigator.clipboard && window.isSecureContext) {
    await navigator.clipboard.writeText(text);
    return;
  }

  const area = document.createElement('textarea');
  area.value = text;
  area.setAttribute('readonly', '');
  area.style.position = 'fixed';
  area.style.left = '-9999px';
  area.style.top = '0';
  document.body.appendChild(area);
  area.select();
  const ok = document.execCommand('copy');
  document.body.removeChild(area);
  if (!ok)
    throw new Error('Copy was blocked by the browser');
}

function flashCopyButton(button, ok) {
  const original = button.dataset.originalLabel || button.textContent;
  button.dataset.originalLabel = original;
  button.classList.remove('copied', 'copy-failed');
  button.classList.add(ok ? 'copied' : 'copy-failed');
  button.textContent = ok ? 'Copied' : 'Failed';

  clearTimeout(button.copyResetTimer);
  button.copyResetTimer = setTimeout(() => {
    button.textContent = original;
    button.classList.remove('copied', 'copy-failed');
  }, 1200);
}

function clearResultPane() {
  collapseResultCards();
  rendered.replaceChildren();
  rendered.textContent = '';
  clearRenderedError();
  setDatetimeLocalText('');
  resetMoreDigitsButton(renderedMore, false);
  clearResultDetails();
}

function clearResultDetails(options = {}) {
  clearFunctionRun();
  parsed.classList.remove('matrix-pretty');
  parsed.classList.remove('matrix-expression-pretty');
  parsed.classList.remove('matrix-expression-text');
  functionStyle.classList.remove('matrix-pretty');
  functionStyle.classList.remove('equation-function');
  value.classList.remove('matrix-pretty');
  value.classList.remove('matrix-tex-value');
  parsed.textContent = '';
  functionStyle.textContent = '';
  resetMoreDigitsButton(parsedMore, false);
  resetMoreDigitsButton(functionMore, false);
  resetMoreDigitsButton(valueMore, false);
  delete parsed.dataset.fullText;
  delete parsed.dataset.displayText;
  delete parsed.dataset.matrixExpression;
  delete parsed.dataset.matrixDisplayResult;
  delete parsed.dataset.matrixFullResult;
  delete parsed.dataset.matrixPretty;
  delete parsed.dataset.matrixBindings;
  delete functionStyle.dataset.fullText;
  delete functionStyle.dataset.displayText;
  delete rendered.dataset.compactTex;
  delete rendered.dataset.wrappedTex;
  delete rendered.dataset.compactSvg;
  delete rendered.dataset.wrappedSvg;
  delete rendered.dataset.responsiveFallback;
  delete rendered.dataset.responsiveFit;
  delete rendered.dataset.responsiveVariant;
  setResultInputText('');
  setValueText('');
  if (currentMode() === 'expression')
    setValueCardVisible(false);
  lastTex = '';
  lastDerivativeExpression = '';
  currentVariables = [];
  currentDifferentiable = true;
  renderDerivativeButtons(currentVariables);
  if (!options.keepBindings)
    clearVariableValues();
}
