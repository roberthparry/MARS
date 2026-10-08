/**
 * Card expansion, zoom and fitting native TeX images.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function resetMoreDigitsButton(button, canExpand) {
  button.classList.toggle('hidden', !canExpand);
  button.textContent = 'Show more digits';
  button.dataset.expanded = 'false';
}

function hasAbbreviatedValue(text) {
  return String(text || '').includes('...');
}

function resultZoomIndex(card) {
  const raw = Number(card && card.dataset.zoomIndex);
  if (Number.isFinite(raw))
    return Math.max(0, Math.min(RESULT_ZOOM_LEVELS.length - 1, Math.round(raw)));
  return RESULT_ZOOM_DEFAULT_INDEX;
}

function svgLengthPixels(value) {
  const match = String(value || '').trim().match(/^([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*(px|pt|pc|in|cm|mm)?$/i);
  if (!match)
    return 0;

  const amount = Number.parseFloat(match[1]);
  if (!Number.isFinite(amount))
    return 0;

  const unit = String(match[2] || 'px').toLowerCase();
  if (unit === 'pt') return amount * 96 / 72;
  if (unit === 'pc') return amount * 16;
  if (unit === 'in') return amount * 96;
  if (unit === 'cm') return amount * 96 / 2.54;
  if (unit === 'mm') return amount * 96 / 25.4;
  return amount;
}

function svgIntrinsicSize(svg) {
  const attrWidth = svgLengthPixels(svg.getAttribute('width'));
  const attrHeight = svgLengthPixels(svg.getAttribute('height'));
  let viewWidth = 0;
  let viewHeight = 0;
  if (svg.viewBox && svg.viewBox.baseVal) {
    viewWidth = svg.viewBox.baseVal.width;
    viewHeight = svg.viewBox.baseVal.height;
  }

  const previousTransform = svg.style.transform;
  svg.style.transform = 'none';
  const rect = svg.getBoundingClientRect();
  svg.style.transform = previousTransform;

  return {
    width: Math.max(1, attrWidth || rect.width || viewWidth || 1),
    height: Math.max(1, attrHeight || rect.height || viewHeight || 1)
  };
}

function updateRenderedZoomFrame(card, scale) {
  const frame = card ? card.querySelector('.rendered-zoom-frame') : null;
  const svg = frame ? frame.querySelector('svg') : null;
  if (!frame || !svg)
    return;

  if (!svg.dataset.baseWidth || !svg.dataset.baseHeight) {
    const intrinsic = svgIntrinsicSize(svg);
    svg.dataset.baseWidth = String(intrinsic.width);
    svg.dataset.baseHeight = String(intrinsic.height);
  }

  const width = Number(svg.dataset.baseWidth) || 1;
  const height = Number(svg.dataset.baseHeight) || 1;
  let appliedScale = scale;
  const matrixValue = frame.parentElement && frame.parentElement.matches('#value.matrix-tex-value')
    ? frame.parentElement
    : null;
  if (matrixValue) {
    const style = getComputedStyle(matrixValue);
    const availableWidth = Math.max(
      1,
      matrixValue.clientWidth -
        (Number.parseFloat(style.paddingLeft) || 0) -
        (Number.parseFloat(style.paddingRight) || 0)
    );
    const zoom = RESULT_ZOOM_LEVELS[resultZoomIndex(card)];
    const baseScale = scale / Math.max(zoom, Number.EPSILON);
    appliedScale = Math.min(baseScale, availableWidth / width) * zoom;
  }
  frame.style.width = `${width * appliedScale}px`;
  frame.style.height = `${height * appliedScale}px`;
  svg.style.width = `${width}px`;
  svg.style.height = `${height}px`;
  svg.style.transform = `scale(${appliedScale})`;
}

function applyResultZoom(card) {
  if (!card)
    return;

  const index = resultZoomIndex(card);
  const zoom = RESULT_ZOOM_LEVELS[index];
  const computed = getComputedStyle(card);
  const renderBase = Number.parseFloat(computed.getPropertyValue('--render-base-scale')) || 1.35;
  const textBase = Number.parseFloat(computed.getPropertyValue('--result-base-font-rem')) || 0.92;
  const renderFontBase = Number.parseFloat(computed.getPropertyValue('--render-base-font-rem')) || 1.15;
  const marginBase = Number.parseFloat(computed.getPropertyValue('--render-base-margin-rem')) || 3;
  const renderScale = renderBase * zoom;

  card.dataset.zoomIndex = String(index);
  card.style.setProperty('--result-zoom', String(zoom));
  card.style.setProperty('--result-font-size', `${textBase * zoom}rem`);
  card.style.setProperty('--render-font-size', `${renderFontBase * zoom}rem`);
  card.style.setProperty('--render-zoom', String(renderScale));
  card.style.setProperty('--render-margin-bottom', `${marginBase * Math.max(1, zoom)}rem`);
  updateRenderedZoomFrame(card, renderScale);
  card.querySelectorAll('[data-zoom-reset]').forEach((button) => {
    button.textContent = `${Math.round(zoom * 100)}%`;
    button.setAttribute('aria-label', `Reset zoom from ${Math.round(zoom * 100)}%`);
  });
  card.querySelectorAll('[data-zoom-step="-1"]').forEach((button) => {
    button.disabled = index <= 0;
  });
  card.querySelectorAll('[data-zoom-step="1"]').forEach((button) => {
    button.disabled = index >= RESULT_ZOOM_LEVELS.length - 1;
  });
}

function setResultZoom(card, index) {
  if (!card)
    return;
  card.dataset.zoomIndex = String(Math.max(0, Math.min(RESULT_ZOOM_LEVELS.length - 1, index)));
  applyResultZoom(card);
  scheduleRenderedTeXFit();
}

function stepResultZoom(card, direction) {
  setResultZoom(card, resultZoomIndex(card) + (direction < 0 ? -1 : 1));
}

function collapseResultCards() {
  labWorkspace.classList.remove('result-card-expanded');
  resultPane.classList.remove('card-expanded');
  document.querySelectorAll('.result-card.expanded-card')
    .forEach((card) => card.classList.remove('expanded-card'));
  expandCardButtons.forEach((button) => {
    button.textContent = 'Expand';
    button.setAttribute('aria-expanded', 'false');
  });
}

function toggleResultCardExpansion(button) {
  const card = button.closest('.result-card');
  if (!card)
    return;

  const isExpanded = card.classList.contains('expanded-card');
  collapseResultCards();
  if (isExpanded) {
    requestAnimationFrame(() => applyResultZoom(card));
    return;
  }

  labWorkspace.classList.add('result-card-expanded');
  resultPane.classList.add('card-expanded');
  card.classList.add('expanded-card');
  button.textContent = 'Collapse';
  button.setAttribute('aria-expanded', 'true');
  requestAnimationFrame(() => applyResultZoom(card));
}

function setRenderedContent(svg, fallbackText = '') {
  const card = rendered.closest('.result-card');
  rendered.replaceChildren();
  if (svg) {
    const frame = document.createElement('div');
    frame.className = 'rendered-zoom-frame';
    frame.innerHTML = svg;
    rendered.appendChild(frame);
  } else {
    rendered.textContent = fallbackText;
  }
  if (card)
    requestAnimationFrame(() => applyResultZoom(card));
}

function svgMarkupIntrinsicWidth(svgMarkup) {
  const container = document.createElement('div');
  container.innerHTML = String(svgMarkup || '');
  const svg = container.querySelector('svg');
  if (!svg)
    return 0;

  const attributeWidth = svgLengthPixels(svg.getAttribute('width'));
  if (attributeWidth > 0)
    return attributeWidth;

  const viewBox = String(svg.getAttribute('viewBox') || '')
    .trim()
    .split(/[\s,]+/)
    .map((value) => Number.parseFloat(value));
  return viewBox.length === 4 && Number.isFinite(viewBox[2])
    ? Math.max(0, viewBox[2])
    : 0;
}

function renderedContentWidth() {
  const style = getComputedStyle(rendered);
  const paddingLeft = Number.parseFloat(style.paddingLeft) || 0;
  const paddingRight = Number.parseFloat(style.paddingRight) || 0;
  return Math.max(0, rendered.clientWidth - paddingLeft - paddingRight);
}

function renderedSolutionScale(card) {
  if (!card)
    return 1;
  const style = getComputedStyle(card);
  const base = Number.parseFloat(
    style.getPropertyValue('--render-base-scale')
  ) || 1.35;
  return base * RESULT_ZOOM_LEVELS[resultZoomIndex(card)];
}

function fitRenderedTeXToCard() {
  renderedTeXFitFrame = 0;
  const fittingDiffequation = currentMode() === 'diffequation';
  const fittingIntegral = currentMode() === 'expression' &&
    /integral\s+result$/i.test(rightPaneTitle.textContent || '');
  const fittingResponsiveExpression =
    rendered.dataset.responsiveFit === 'true';
  if (!fittingDiffequation && !fittingIntegral && !fittingResponsiveExpression)
    return;

  const compactSvg = rendered.dataset.compactSvg || '';
  const wrappedSvg = rendered.dataset.wrappedSvg || '';
  if (!compactSvg)
    return;

  const card = rendered.closest('.result-card');
  const compactWidth = svgMarkupIntrinsicWidth(compactSvg) *
    renderedSolutionScale(card);
  const useWrapped = !!wrappedSvg &&
    compactWidth > renderedContentWidth() + 1;
  const variant = useWrapped ? 'wrapped' : 'compact';
  rendered.classList.toggle('vertically-wrapped-tex', useWrapped);
  if (rendered.dataset.responsiveVariant === variant)
    return;

  rendered.dataset.responsiveVariant = variant;
  rendered.dataset.displayTex = useWrapped
    ? (rendered.dataset.wrappedTex || rendered.dataset.compactTex || '')
    : (rendered.dataset.compactTex || '');
  setRenderedContent(
    useWrapped ? wrappedSvg : compactSvg,
    rendered.dataset.responsiveFallback || 'No symbolic solution available'
  );
}

function scheduleRenderedTeXFit() {
  if (renderedTeXFitFrame)
    cancelAnimationFrame(renderedTeXFitFrame);
  renderedTeXFitFrame = requestAnimationFrame(
    fitRenderedTeXToCard
  );
}

function solverTexScale() {
  const value = Number.parseFloat(
    getComputedStyle(functionStyle).getPropertyValue('--solver-tex-scale')
  );
  return Number.isFinite(value) && value > 0 ? value : 1.5;
}

function solverTexContentWidth() {
  const style = getComputedStyle(functionStyle);
  const paddingLeft = Number.parseFloat(style.paddingLeft) || 0;
  const paddingRight = Number.parseFloat(style.paddingRight) || 0;
  return Math.max(
    0,
    functionStyle.clientWidth - paddingLeft - paddingRight
  );
}

function installSolverTexSvg(svgMarkup, variant) {
  if (!svgMarkup || functionStyle.dataset.solverVariant === variant)
    return;

  const svgStart = svgMarkup.indexOf('<svg');
  functionStyle.innerHTML = svgStart >= 0
    ? svgMarkup.slice(svgStart)
    : svgMarkup;
  functionStyle.classList.add('equation-function');
  const solverSvg = functionStyle.querySelector('svg');
  const solverSvgWidth = solverSvg?.getAttribute('width');
  if (solverSvg && solverSvgWidth) {
    solverSvg.style.width =
      `calc(${solverSvgWidth} * var(--solver-tex-scale))`;
    solverSvg.style.maxWidth = '100%';
    solverSvg.style.height = 'auto';
  }
  functionStyle.dataset.solverVariant = variant;
}

async function fitSolverTexToCard() {
  solverFitFrame = 0;
  if (currentMode() !== 'diffequation' ||
      !functionStyle.classList.contains('equation-function'))
    return;

  const compactSvg = functionStyle.dataset.solverCompactSvg || '';
  const wrappedTex = functionStyle.dataset.solverWrappedTex || '';
  let wrappedSvg = functionStyle.dataset.solverWrappedSvg || '';
  if (!compactSvg)
    return;

  const compactWidth = svgMarkupIntrinsicWidth(compactSvg) *
    solverTexScale();
  const useWrapped = !!wrappedTex &&
    wrappedTex !== functionStyle.dataset.solverCompactTex &&
    compactWidth > solverTexContentWidth() + 1;

  if (useWrapped && !wrappedSvg && !solverWrapRenderPending) {
    solverWrapRenderPending = true;
    try {
      const renderedWrapped = await renderTexSvg(wrappedTex);
      wrappedSvg = renderedWrapped.svg || '';
      functionStyle.dataset.solverWrappedSvg = wrappedSvg;
    } catch (_err) {
      wrappedSvg = '';
    } finally {
      solverWrapRenderPending = false;
    }
  }

  installSolverTexSvg(
    useWrapped && wrappedSvg ? wrappedSvg : compactSvg,
    useWrapped && wrappedSvg ? 'wrapped' : 'compact'
  );
}

function scheduleSolverTexFit() {
  if (solverFitFrame)
    cancelAnimationFrame(solverFitFrame);
  solverFitFrame = requestAnimationFrame(() => {
    void fitSolverTexToCard();
  });
}

function clearRenderedError() {
  rendered.classList.remove('error');
  rendered.classList.remove('vertically-wrapped-tex');
  rendered.style.color = '';
  rendered.style.background = '';
  rendered.style.borderColor = '';
  rendered.style.boxShadow = '';
  rendered.style.textShadow = '';
  rendered.style.fontFamily = '';
}

function setRenderedError(message) {
  rendered.replaceChildren();
  rendered.textContent = message || 'Evaluation failed';
  rendered.classList.add('error');
  rendered.style.color = '#ffd99a';
  rendered.style.background =
    'radial-gradient(circle at 14% 18%, rgba(229, 173, 87, 0.16), transparent 34%), ' +
    'linear-gradient(135deg, rgba(73, 23, 25, 0.88), rgba(38, 12, 19, 0.78))';
  rendered.style.borderColor = 'rgba(229, 173, 87, 0.42)';
  rendered.style.boxShadow =
    'inset 0 0 0 1px rgba(255, 232, 181, 0.07), 0 0 1.35rem rgba(153, 27, 27, 0.22)';
  rendered.style.textShadow = '0 0 0.7rem rgba(255, 204, 112, 0.16)';
  rendered.style.fontFamily = 'Georgia, "Times New Roman", serif';
}
