/**
 * Function syntax highlighting and textual result sections.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function looksLikeMarsFunction(source) {
  return /(?:^|\n)\s*(?:array\s+)?(?:equation|expression|matrix)\s+[\p{L}_$][\p{L}\p{M}\p{N}_$]*\s*\(/u
    .test(String(source || ''));
}

function appendFunctionToken(fragment, text, className = '') {
  if (!className) {
    fragment.appendChild(document.createTextNode(text));
    return;
  }

  const token = document.createElement('span');
  token.className = className;
  token.textContent = text;
  fragment.appendChild(token);
}

function renderMarsFunctionSyntax(element, text) {
  const source = String(text || '');
  const fragment = document.createDocumentFragment();
  const identifierStart = character => /[\p{L}_]/u.test(character);
  const identifierPart = character => /[\p{L}\p{M}\p{N}_]/u.test(character);
  const functionDeclaration = source.match(
    /(?:^|\n)\s*(?:array\s+)?(?:equation|expression|matrix)\s+[\p{L}_$][\p{L}\p{M}\p{N}_$]*\s*\(([^)]*)\)/u
  );
  const functionArrayVariables = new Set();
  const functionOpeningBrackets = new Set();
  const functionBracketStack = [];
  let index = 0;

  if (functionDeclaration) {
    functionDeclaration[1].split(',').forEach(parameter => {
      const name = parameter.trim().match(/[\p{L}_][\p{L}\p{M}\p{N}_]*$/u);
      if (name && /(?:^|\s)array(?:\s|$)/u.test(parameter))
        functionArrayVariables.add(name[0]);
    });
  }

  for (const declaration of source.matchAll(
    /(?:^|\n)\s*array\s+(?:const\s+)?([\p{L}_][\p{L}\p{M}\p{N}_]*)\s*=/gu
  ))
    functionArrayVariables.add(declaration[1]);

  while (index < source.length) {
    if (source.startsWith('``', index)) {
      const end = source.indexOf('\n', index + 2);
      const next = end < 0 ? source.length : end;
      appendFunctionToken(fragment, source.slice(index, next), 'function-token-comment');
      index = next;
      continue;
    }

    if (source[index] === '`') {
      let next = index + 1;
      while (next < source.length) {
        if (source[next] === '`' && source[next - 1] !== '\\') {
          next += 1;
          break;
        }
        next += 1;
      }
      appendFunctionToken(fragment, source.slice(index, next), 'function-token-comment');
      index = next;
      continue;
    }

    if (source.startsWith('$[', index)) {
      let next = index + 2;
      let depth = 1;
      while (next < source.length && depth > 0) {
        if (source[next] === '[')
          depth += 1;
        else if (source[next] === ']')
          depth -= 1;
        next += 1;
      }
      appendFunctionToken(fragment, source.slice(index, next), 'function-token-variable');
      index = next;
      continue;
    }

    if (source[index] === '$') {
      appendFunctionToken(fragment, '$', 'function-token-keyword');
      index += 1;
      continue;
    }

    if (source[index] === '[' || source[index] === ']') {
      appendFunctionToken(fragment, source[index], 'function-token-keyword');
      index += 1;
      continue;
    }

    if (source[index] === '"' || source[index] === "'") {
      const quote = source[index];
      let next = index + 1;
      while (next < source.length) {
        if (source[next] === quote && source[next - 1] !== '\\') {
          next += 1;
          break;
        }
        next += 1;
      }
      appendFunctionToken(fragment, source.slice(index, next));
      index = next;
      continue;
    }

    const numberMatch = source.slice(index).match(/^(?:\d+(?:\.\d+)?|\.\d+)(?:[eE][+-]?\d+)?/u);
    if (numberMatch) {
      appendFunctionToken(fragment, numberMatch[0], 'function-token-number');
      index += numberMatch[0].length;
      continue;
    }

    const namedConstantMatch = source.slice(index).match(/^@(?:pi|phi|gamma|eulermascheroni|tau|inf|nan)\b/u);
    if (namedConstantMatch) {
      appendFunctionToken(fragment, namedConstantMatch[0], 'function-token-constant');
      index += namedConstantMatch[0].length;
      continue;
    }

    if (identifierStart(source[index])) {
      let next = index + 1;
      while (next < source.length && identifierPart(source[next]))
        next += 1;
      const identifier = source.slice(index, next);
      let following = next;
      while (following < source.length && /\s/u.test(source[following]))
        following += 1;
      const className = MARS_FUNCTION_KEYWORDS.has(identifier)
        ? 'function-token-keyword'
        : (source[following] === '('
          ? 'function-token-function'
          : (!MARS_FUNCTION_CONSTANTS.has(identifier)
            ? `function-token-variable${functionArrayVariables.has(identifier) ? ' function-token-array' : ''}`
            : ''));
      if (source[following] === '(')
        functionOpeningBrackets.add(following);
      appendFunctionToken(fragment, identifier, className);
      index = next;
      continue;
    }

    if (source[index] === '(') {
      const isFunctionBracket = functionOpeningBrackets.has(index);
      functionBracketStack.push(isFunctionBracket);
      appendFunctionToken(
        fragment,
        source[index],
        isFunctionBracket ? 'function-token-bracket' : ''
      );
      index += 1;
      continue;
    }

    if (source[index] === ')') {
      const isFunctionBracket = functionBracketStack.length
        ? functionBracketStack.pop()
        : false;
      appendFunctionToken(
        fragment,
        source[index],
        isFunctionBracket ? 'function-token-bracket' : ''
      );
      index += 1;
      continue;
    }

    appendFunctionToken(fragment, source[index]);
    index += 1;
  }

  element.replaceChildren(fragment);
}

function renderMatrixSectionHeadings(element, text) {
  const source = String(text || '');
  const lines = source.split('\n');
  const hasHeadings = lines.some((line) => /^(?:\s*)(eigenvalues|eigenvectors)(?:\s*)$/i.test(line));
  if (!hasHeadings) {
    element.textContent = source;
    return;
  }

  element.replaceChildren();
  lines.forEach((line, index) => {
    const match = line.match(/^(\s*)(eigenvalues|eigenvectors)(\s*)$/i);
    if (match) {
      element.appendChild(document.createTextNode(match[1]));
      const heading = document.createElement('span');
      heading.className = 'matrix-section-heading';
      heading.textContent = match[2].toLowerCase();
      element.appendChild(heading);
      element.appendChild(document.createTextNode(match[3]));
    } else {
      element.appendChild(document.createTextNode(line));
    }
    if (index + 1 < lines.length)
      element.appendChild(document.createTextNode('\n'));
  });
}

function renderResultText(element, text) {
  if (element === functionStyle && looksLikeMarsFunction(text)) {
    renderMarsFunctionSyntax(element, text);
    return;
  }
  renderMatrixSectionHeadings(element, text);
}

function clearFunctionRun() {
  ++functionRunSequence;
  if (functionRunController)
    functionRunController.abort();
  functionRunController = null;
  functionRun.disabled = false;
  setActionRunning(functionRun, false);
  functionRunOutput.textContent = '';
  functionRunResult.classList.add('hidden');
}

async function runFunctionCard() {
  if (functionRun.disabled || functionTitle.textContent !== 'Function')
    return;
  // The display may abbreviate digits; only the native full source is executable.
  const source = String(functionStyle.dataset.fullText || '').trim();
  clearFunctionRun();
  functionRunResult.classList.remove('hidden');
  if (!source) {
    functionRunOutput.textContent = 'No Function programme is available. Evaluate an input first.';
    return;
  }
  const sequence = functionRunSequence;
  const controller = new AbortController();
  functionRunController = controller;
  functionRun.disabled = true;
  setActionRunning(functionRun, true);
  functionRunOutput.textContent = 'Running…';
  const timer = setTimeout(() => controller.abort(), 45000);
  try {
    const response = await fetch('/function-run', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({source, precision: requestedValuePrecision()}),
      signal: controller.signal
    });
    const data = await response.json();
    if (sequence !== functionRunSequence)
      return;
    const output = String(data.output || '').trimEnd();
    const error = String(data.error || '').trim();
    functionRunOutput.textContent = response.ok && data.ok
      ? (output || 'Programme completed without output.')
      : [output, error || 'Programme execution failed.'].filter(Boolean).join('\n\n');
  } catch (error) {
    if (sequence === functionRunSequence)
      functionRunOutput.textContent = error.name === 'AbortError'
        ? 'Execution request timed out.'
        : 'Could not run programme: ' + error.message;
  } finally {
    clearTimeout(timer);
    if (sequence === functionRunSequence) {
      functionRunController = null;
      functionRun.disabled = false;
      setActionRunning(functionRun, false);
    }
  }
}

function setExpandableText(element, button, displayText, fullText) {
  if (element === functionStyle)
    clearFunctionRun();
  renderResultText(element, displayText || fullText || '');
  element.dataset.displayText = displayText || '';
  element.dataset.fullText = fullText || '';
  resetMoreDigitsButton(
    button,
    !!fullText && !!displayText && fullText !== displayText && hasAbbreviatedValue(displayText)
  );
}

function setValueText(fullText) {
  valueNote.textContent = '';
  valueNoteCard.classList.add('hidden');
  const full = String(fullText || '');
  setExpandableText(value, valueMore, full, full);
}

function renderDatetimeSections(element, button, sections, fallbackText = '') {
  const text = String(fallbackText || '').trim();
  const items = Array.isArray(sections) ? sections : [];

  element.replaceChildren();
  element.dataset.displayText = text;
  element.dataset.fullText = text;
  if (button)
    resetMoreDigitsButton(button, false);

  if (!items.length) {
    element.textContent = text;
    return;
  }

  const grid = document.createElement('div');
  grid.className = 'datetime-section-grid';
  items.forEach((section) => {
    const rows = Array.isArray(section && section.rows) ? section.rows : [];
    if (!rows.length)
      return;

    const details = document.createElement('details');
    details.className = 'datetime-section';
    if (section.open !== false)
      details.open = true;

    const summary = document.createElement('summary');
    summary.textContent = String(section.title || 'Calendar');
    details.appendChild(summary);

    const body = document.createElement('div');
    body.className = 'datetime-section-rows';
    rows.forEach((row) => {
      const labelText = String(row && row.label || '').trim();
      const valueText = String(row && row.value || '').trim();
      if (!labelText && !valueText)
        return;

      const line = document.createElement('div');
      line.className = 'datetime-row';
      const label = document.createElement('span');
      label.className = 'datetime-row-label';
      label.textContent = labelText;
      const value = document.createElement('span');
      value.className = 'datetime-row-value';
      value.textContent = valueText || 'unavailable';
      line.append(label, value);
      body.appendChild(line);
    });

    details.appendChild(body);
    grid.appendChild(details);
  });

  if (grid.childElementCount)
    element.appendChild(grid);
  else
    element.textContent = text;
}
