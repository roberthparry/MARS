/**
 * Authored expression text, compact bindings and editor readiness.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function splitTopLevel(text, separator) {
  const parts = [];
  let start = 0;
  let depth = 0;
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (ch === '(' || ch === '[' || ch === '{') depth++;
    else if (ch === ')' || ch === ']' || ch === '}') depth = Math.max(0, depth - 1);
    else if (ch === separator && depth === 0) {
      parts.push(text.slice(start, i));
      start = i + 1;
    }
  }
  parts.push(text.slice(start));
  return parts;
}

function indexOfTopLevel(text, needle) {
  let depth = 0;
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (ch === '(' || ch === '[' || ch === '{') depth++;
    else if (ch === ')' || ch === ']' || ch === '}') depth = Math.max(0, depth - 1);
    else if (ch === needle && depth === 0) return i;
  }
  return -1;
}

function lastIndexOfTopLevel(text, needle) {
  let depth = 0;
  let found = -1;
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (ch === '(' || ch === '[' || ch === '{') depth++;
    else if (ch === ')' || ch === ']' || ch === '}') depth = Math.max(0, depth - 1);
    else if (ch === needle && depth === 0) found = i;
  }
  return found;
}

function compareBindingNames(left, right) {
  const leftName = String((left && left.name) || left || '');
  const rightName = String((right && right.name) || right || '');
  return leftName.localeCompare(rightName, undefined, {numeric: true, sensitivity: 'base'}) ||
    leftName.localeCompare(rightName);
}

function sortedAssignmentParts(parts) {
  return [...(parts || [])].sort((left, right) => {
    const leftEq = indexOfTopLevel(left, '=');
    const rightEq = indexOfTopLevel(right, '=');
    const leftName = leftEq >= 0 ? left.slice(0, leftEq).trim() : String(left || '').trim();
    const rightName = rightEq >= 0 ? right.slice(0, rightEq).trim() : String(right || '').trim();
    return compareBindingNames(leftName, rightName);
  });
}

function expressionWithSortedConstants(text) {
  const normalized = expressionForEditor(text).trim();
  const parts = bindingParts(normalized);
  if (!parts || !parts.constants)
    return normalized;

  const variableAssignments = splitTopLevel(parts.variables, ',')
    .map((part) => part.trim())
    .filter(Boolean);
  const constantAssignments = sortedAssignmentParts(
    splitTopLevel(parts.constants, ',')
      .map((part) => part.trim())
      .filter(Boolean)
  );

  let bindingText = variableAssignments.join(', ');
  if (constantAssignments.length) {
    const constants = constantAssignments.join(', ');
    bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;
  }

  return `{ ${parts.body} | ${bindingText} }`;
}

function canGoalSeek() {
  return currentVariables.length > 0;
}

function derivativeExpressionFromLine(line) {
  const match = String(line || '').match(/^d\/d[^=]*=\s*(.+)$/);
  return match ? expressionForEditor(match[1].trim()) : '';
}

function integralExpressionFromLine(line) {
  const match = String(line || '').match(/^∫d[^=]*=\s*(.+)$/);
  return match ? expressionForEditor(match[1].trim()) : '';
}

function expressionForEvaluation(text) {
  return String(text || '').replace(/(=\s*)\?/g, '$1NAN');
}

function expressionForEditor(text) {
  return String(text || '')
    .replace(/(=\s*)NAN\b/g, '$1?');
}

function restoreCompactBindingValues(text) {
  text = String(text || '').trim();
  if (!text.includes('...'))
    return text;

  const parts = bindingParts(text);
  if (!parts)
    return text;

  const restoreValues = new Map(bindingValueCache);
  const fullParts = bindingParts(expr.dataset.fullExpression || fullExpressionText);
  if (fullParts) {
    [fullParts.variables, fullParts.constants].forEach((assignmentsText) => {
      splitTopLevel(assignmentsText, ',').forEach((part) => {
        const eq = indexOfTopLevel(part, '=');
        if (eq < 0)
          return;

        const name = part.slice(0, eq).trim();
        const value = part.slice(eq + 1).trim();
        if (name && value && !restoreValues.has(name))
          restoreValues.set(name, value);
      });
    });
  }

  if (restoreValues.size === 0)
    return text;

  let changed = false;
  function restoreAssignments(assignmentsText) {
    return splitTopLevel(assignmentsText, ',')
      .map((part) => {
        const eq = indexOfTopLevel(part, '=');
        if (eq < 0)
          return part.trim();

        const name = part.slice(0, eq).trim();
        const valueText = part.slice(eq + 1).trim();
        const cached = restoreValues.get(name);
        if (cached && valueText.endsWith('...') && cached.startsWith(valueText.slice(0, -3))) {
          changed = true;
          return `${name} = ${cached}`;
        }
        return part.trim();
      })
      .filter(Boolean)
      .join(', ');
  }

  const variables = restoreAssignments(parts.variables);
  const constants = restoreAssignments(parts.constants);
  let bindingText = variables;
  if (constants)
    bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;

  return changed ? `{ ${parts.body} | ${bindingText} }` : text;
}

function currentExpressionText() {
  const text = expr.value.trim();
  const full = expr.dataset.fullExpression || fullExpressionText;
  const compact = expr.dataset.displayExpression || displayedExpressionText;
  if (full && compact && text === compact)
    return full;
  if (text.includes('...'))
    return restoreCompactBindingValues(text);
  return text;
}

function expressionBodyForEditor(fullText) {
  let text = expressionForEditor(fullText).trim();
  let parts = bindingParts(text);
  // Recover repeated editor binding envelopes without changing the expression body.
  while (parts) {
    text = parts.body;
    parts = bindingParts(text);
  }
  return text;
}

function clearExpressionSource() {
  fullExpressionText = '';
  displayedExpressionText = '';
  lastEvaluationInputText = '';
  bindingValueCache = new Map();
  delete expr.dataset.fullExpression;
  delete expr.dataset.displayExpression;
  delete expr.dataset.bindingRefreshValid;
  delete expr.dataset.evaluationReady;
  clearGoalSeekRequest();
}

function clearGoalSeekRequest() {
  delete expr.dataset.goalSeekSource;
  delete expr.dataset.goalSeekTarget;
}

function currentGoalSeekSource() {
  const source = expr.dataset.goalSeekSource || '';

  if (!source)
    return '';
  if (expr.value.trim() !== (expr.dataset.displayExpression || displayedExpressionText))
    return '';
  return source;
}

function expressionHasSolvedVariables(text) {
  const parts = bindingParts(text);

  if (!parts)
    return false;

  return splitTopLevel(parts.variables, ',').some((part) => {
    const eq = indexOfTopLevel(part, '=');
    if (eq < 0)
      return false;

    const valueText = part.slice(eq + 1).trim();
    return valueText && valueText !== '?' && !/^NAN$/i.test(valueText);
  });
}

function isUnsetBindingValue(valueText) {
  const text = String(valueText || '').trim();
  return !text || text === '?' || /^NAN$/i.test(text);
}

function expressionReadyToEvaluate() {
  if (currentMode() !== 'expression')
    return true;

  return Boolean(currentExpressionText());
}

function bindingParts(text) {
  text = String(text || '').trim();
  const wrapped = text.startsWith('{') && text.endsWith('}');
  if (wrapped)
    text = text.slice(1, -1).trim();

  const pipe = lastIndexOfTopLevel(text, '|');
  if (pipe < 0)
    return null;

  const body = text.slice(0, pipe).trim();
  const bindings = text.slice(pipe + 1).trim();
  if (!bindings || indexOfTopLevel(bindings, '=') < 0)
    return null;
  const semi = indexOfTopLevel(bindings, ';');
  const variables = semi >= 0 ? bindings.slice(0, semi).trim() : bindings;
  const constants = semi >= 0 ? bindings.slice(semi + 1).trim() : '';
  return {wrapped, body, variables, constants};
}

function compactBindingValue(valueText) {
  const text = String(valueText || '').trim();
  if (!text || text === '?' || /^NAN$/i.test(text))
    return {display: text, shortened: false};
  if (text.includes('...'))
    return {display: text, shortened: false};
  if (text.length <= COMPACT_BINDING_VALUE_LIMIT)
    return {display: text, shortened: false};
  const display = compactLongNumericTokens(text);
  return {display, shortened: display !== text};
}

function compactLongNumericTokens(text) {
  const source = String(text || '').replace(
    /(^|[^A-Za-z0-9_.])([+-]?\d(?:\.\d+)?\.\.\.)[x×]10\^(?:\{([+-]?\d+)\}|([+-]?\d+))/g,
    (match, prefix, mantissa, bracedExponent, plainExponent) => {
      const exponent = String(bracedExponent || plainExponent || '');
      return `${prefix}${mantissa}e${exponent.startsWith('-') || exponent.startsWith('+') ? exponent : `+${exponent}`}`;
    }
  );
  return source.replace(
    /(^|[^A-Za-z0-9_.])([+-]?(?:\d+\.\d+|\d{21,})(?:[Ee][+-]?\d+)?(?:\.\.\.[Ee][+-]?\d+)?)/g,
    (match, prefix, numberText) => {
      if (numberText.includes('...') || numberText.length <= COMPACT_BINDING_VALUE_LIMIT)
        return match;
      if (/^[+-]?\d+$/.test(numberText)) {
        const sign = numberText.startsWith('-') || numberText.startsWith('+') ? numberText[0] : '';
        const digits = sign ? numberText.slice(1) : numberText;
        const significantDigits = digits.replace(/^0+/, '') || '0';
        if (significantDigits.length <= COMPACT_INTEGER_DIGITS_KEEP)
          return match;
        const shown = significantDigits.slice(0, COMPACT_INTEGER_DIGITS_KEEP);
        const mantissa = `${shown[0]}.${shown.slice(1)}`;
        return `${prefix}${sign}${mantissa}...e+${significantDigits.length - 1}`;
      }
      return `${prefix}${numberText.slice(0, COMPACT_BINDING_VALUE_KEEP)}...`;
    }
  );
}

function compactExpressionForEditor(fullText) {
  const full = expressionForEditor(fullText);
  const parts = bindingParts(full);
  if (!parts) {
    const display = compactLongNumericTokens(full);
    return {display, bindings: [], shortened: display !== full};
  }

  const bindingValues = [];
  let shortened = false;
  const body = compactLongNumericTokens(parts.body);
  shortened = shortened || body !== parts.body;

  function compactAssignments(assignmentsText, kind) {
    // Conditions follow the assignment group, not its final value.
    const conditionStart = indexOfTopLevel(assignmentsText, ';');
    const conditions = conditionStart >= 0 ? assignmentsText.slice(conditionStart) : '';
    if (conditionStart >= 0)
      assignmentsText = assignmentsText.slice(0, conditionStart);
    const rows = splitTopLevel(assignmentsText, ',')
      .map((part) => {
        const eq = indexOfTopLevel(part, '=');
        if (eq < 0) {
          const text = part.trim();
          return text ? {name: text, text, bind: false} : null;
        }

        const name = part.slice(0, eq).trim();
        const valueText = part.slice(eq + 1).trim();
        const compact = compactBindingValue(valueText);
        shortened = shortened || compact.shortened;
        return name
          ? {name, value: valueText, display: compact.display, kind, text: `${name} = ${compact.display}`, bind: true}
          : null;
      })
      .filter(Boolean);

    if (kind === 'constant')
      rows.sort(compareBindingNames);
    rows.forEach((row) => {
      if (row.bind && row.name)
        bindingValues.push({
          name: row.name,
          value: row.value || '',
          display: row.display || '',
          kind
        });
    });
    const texts = rows.map((row) => row.text);
    if (conditions && texts.length)
      texts[texts.length - 1] += conditions;
    return texts;
  }

  const variableAssignments = compactAssignments(parts.variables, 'variable');
  const constantAssignments = compactAssignments(parts.constants, 'constant');

  let bindingText = variableAssignments.join(', ');
  if (constantAssignments.length) {
    const constants = constantAssignments.join(', ');
    bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;
  }

  return {
    display: `{ ${body} | ${bindingText} }`,
    bindings: bindingValues,
    shortened
  };
}
