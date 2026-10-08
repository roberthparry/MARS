/** Real-browser regressions injected only into a private test copy of the page. */
(() => {
  const failures = [];
  const nativeFetch = window.fetch.bind(window);
  const failureCase = new URLSearchParams(location.search).get('failure');
  window.addEventListener('error', event => failures.push(event.message || 'Resource load failed'), true);
  window.addEventListener('unhandledrejection', event => {
    if (!failureCase)
      failures.push(String(event.reason));
  });
  if (failureCase === 'catalogue') {
    window.fetch = (url, options) => String(url) === '/jurisdictions'
      ? Promise.resolve(new Response('unavailable', {status: 503}))
      : nativeFetch(url, options);
  }
  if (failureCase === 'database') {
    window.fetch = (url, options) => String(url) === '/jurisdictions'
      ? Promise.resolve(new Response(JSON.stringify({
          available: false, options: [], towns: {}, locations: {}, error: 'Fixture database unavailable'
        }), {headers: {'Content-Type': 'application/json'}}))
      : nativeFetch(url, options);
  }
  if (failureCase === 'script') {
    const append = document.head.appendChild.bind(document.head);
    document.head.appendChild = node => {
      if (node instanceof HTMLScriptElement && node.src.endsWith('/js/controls.js'))
        node.src = '/js/missing.js';
      return append(node);
    };
  }
  function check(condition, message) {
    if (!condition)
      throw new Error(message);
  }
  async function checkFunctionRun() {
    await runFunctionCard();
    check(functionRunOutput.textContent.trim() === '5', 'Native Function RUN did not evaluate the full card');
    const originalFetch = window.fetch;
    const originalText = functionStyle.textContent;
    const fullSource = functionStyle.dataset.fullText.trim();
    try {
      functionStyle.textContent = 'abbreviated...';
      window.fetch = async (url, options) => {
        const body = JSON.parse(options.body);
        check(String(url).includes('/function-run'), 'RUN used the wrong endpoint');
        check(body.source === fullSource && body.precision === requestedValuePrecision(),
          'RUN sent abbreviated text or the wrong precision');
        return new Response(JSON.stringify({ok: false, error: '<img src=x onerror=alert(1)>'}),
          {status: 422, headers: {'Content-Type': 'application/json'}});
      };
      await runFunctionCard();
      check(functionRunOutput.textContent.includes('<img'), 'RUN diagnostic missing');
      check(!functionRunOutput.querySelector('img'), 'RUN diagnostic was interpreted as HTML');
      let release;
      window.fetch = () => new Promise(resolve => { release = resolve; });
      const pending = runFunctionCard();
      clearFunctionRun();
      release(new Response(JSON.stringify({ok: true, output: 'stale'})));
      await pending;
      check(!functionRunOutput.textContent && !functionRun.disabled, 'Stale RUN result replaced the new card');
    } finally {
      window.fetch = originalFetch;
      functionStyle.textContent = originalText;
      clearFunctionRun();
    }
  }
  async function checkRenderedGlyphs() {
    await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    const svg = rendered.querySelector('svg');
    check(svg, 'Native TeX SVG missing');
    const bounds = svg.getBoundingClientRect();
    check(bounds.width > 0 && bounds.height > 0, 'TeX SVG has no visible dimensions');
    const glyphs = Array.from(svg.querySelectorAll('use'));
    check(glyphs.length > 0, 'TeX SVG has no glyphs');
    for (const glyph of glyphs) {
      const reference = glyph.getAttribute('href') || glyph.getAttributeNS('http://www.w3.org/1999/xlink', 'href');
      check(reference?.startsWith('#') && document.getElementById(reference.slice(1)), 'TeX glyph reference is broken');
      const box = glyph.getBBox();
      check(box.width > 0 && box.height > 0, 'TeX glyph has no rendered geometry');
    }
  }
  async function report(message) {
    await nativeFetch('/state', {
      method: 'POST', headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({expression: `BROWSER ${message}`, expression_updated_at: Date.now() + 1000000})
    });
  }
  async function checkAlmanacLayout() {
    for (const zone of ['GMT+0', 'GMT+1', 'GMT-5', 'GMT+5:30', 'GMT+05:45', 'GMT-03:30', 'GMT']) {
      const text = `2027-08-02 10:48:52 ${zone}`;
      check(compactAlmanacLocalTime(text) === '10:48:52', `Local time not compacted: ${zone}`);
      check(compactAlmanacGmtTime(text) === '10:48:52', `GMT time not compacted: ${zone}`);
    }
    check(compactAlmanacLocalTime('Unavailable') === 'Unavailable', 'Unknown contact text was discarded');
    const visibility = almanacVisibilityMode;
    const frame = document.createElement('iframe');
    frame.style.cssText = 'position:fixed;left:-10000px;top:0;height:1000px;border:0';
    try {
      const loaded = new Promise(resolve => { frame.onload = resolve; });
      frame.srcdoc = '<!doctype html><html><head><link rel="stylesheet" href="/index.css"></head>' +
        '<body><div id="fixture"></div></body></html>';
      document.body.appendChild(frame);
      await loaded;
      const target = frame.contentDocument.getElementById('fixture');
      renderAlmanacWorksheet(target, {
        visibility: 'all', worksheet_title: 'Layout regression', rows: [],
        event_title: 'Upcoming eclipses and inner planetary transits',
        events: [{
          category: 'Solar', name: 'Solar eclipse', kind: 'partial', magnitude: '0.482', obscuration: '37.9%',
          first_contact: '2027-08-02 09:06:00 GMT+1', greatest: '2027-08-02 10:08:00 GMT+1',
          fourth_contact: '2027-08-02 11:13:00 GMT+1', gmt_time: '2027-08-02 09:08:00 GMT+0',
          nearest_totality: 'Málaga, ES; 36.7204, -4.4203; 2027-08-02 10:48:52 GMT+2; 1782 km from observer'
        }]
      });
      const table = target.querySelector('.almanac-event-table');
      const region = table.parentElement;
      check(region.tabIndex === 0 && region.getAttribute('role') === 'region', 'Table scroll area is not keyboard accessible');
      check(table.querySelector('.event-time[title]').textContent === '09:06:00', 'Contact still includes the full date');
      check(table.querySelector('.event-time[title]').title.endsWith('GMT+1'), 'Full contact time was lost');
      for (const [viewport, card] of [[1920, 860], [1920, 1700], [390, 390]]) {
        frame.style.width = `${viewport}px`;
        target.style.width = `${card}px`;
        await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
        check(region.clientWidth <= card && target.scrollWidth <= card + 1, 'Almanac spills outside its card');
        for (const cell of table.querySelectorAll('tbody td')) {
          check(cell.scrollWidth <= cell.clientWidth + 1, `Event content overlaps its cell: ${cell.dataset.label}`);
        }
        const headerStyle = frame.contentWindow.getComputedStyle(table.tHead);
        if (viewport < 900) {
          check(headerStyle.display === 'none', 'Mobile events are not stacked');
          check(region.scrollWidth <= region.clientWidth + 1, 'Mobile event cards require horizontal scrolling');
        } else {
          check(headerStyle.display !== 'none', 'Desktop event headings are hidden');
          if (card === 860)
            check(region.scrollWidth > region.clientWidth, 'Wide event table has no contained scroll area');
        }
      }
    } finally {
      frame.remove();
      almanacVisibilityMode = visibility;
    }
  }
  window.addEventListener('load', async () => {
    try {
      check(window.labReady instanceof Promise, 'Startup promise missing');
      if (failureCase === 'database') {
        await window.labReady;
        check(document.getElementById('datetimeJurisdiction').disabled, 'Unavailable database leaves menu enabled');
        check(document.querySelector('[data-jurisdiction-notice]'), 'Unavailable database notice missing');
        check(String(value.textContent).trim() === '7', 'Unavailable database blocked ordinary maths');
        await report('PASS: startup, native evaluation, editor, modes, cards, dates, state, mobile, failure recovery');
        return;
      }
      if (failureCase) {
        await window.labReady.catch(() => {});
        check(document.getElementById('status').textContent === 'Startup failed', 'Missing startup asset not reported');
        check(document.getElementById('run').disabled, 'Failed startup leaves Evaluate enabled');
        if (failureCase === 'catalogue') {
          check(document.getElementById('rendered').textContent.includes('503'), 'Startup error lacks HTTP status');
          location.href = '/?failure=script';
          return;
        }
        check(document.getElementById('rendered').textContent.includes('/js/'), 'Missing script not identified');
        location.href = '/?failure=database';
        return;
      }
      await window.labReady;
      check(!failures.length, failures.join('; '));
      check(currentMode() === 'expression', 'Initial expression mode not restored');
      await checkAlmanacLayout();
      check(document.styleSheets.length > 0, 'External stylesheet not loaded');
      const stylesheet = Array.from(document.styleSheets).find(sheet => sheet.href?.endsWith('/index.css'));
      const styleNames = ['theme', 'layout', 'worksheets', 'dates', 'forms', 'bindings',
        'mobile', 'buttons', 'results', 'help', 'responsive'];
      const imports = Array.from(stylesheet.cssRules);
      check(imports.length === styleNames.length, 'Stylesheet import count changed');
      imports.forEach((rule, index) => {
        check(rule instanceof CSSImportRule && rule.href.endsWith(`/css/${styleNames[index]}.css`),
          `Stylesheet cascade order changed at ${styleNames[index]}`);
        check(rule.styleSheet.cssRules.length > 0, `Stylesheet ${styleNames[index]} failed to load`);
      });
      check(getComputedStyle(document.documentElement).getPropertyValue('--code').trim(), 'Theme variables missing');
      check(getComputedStyle(document.getElementById('run')).borderRadius === '999px', 'Button styling missing');
      check(JURISDICTION_TOWN_OPTIONS['GB-WLS'].length > 0, 'Welsh catalogue unavailable');
      check(String(value.textContent).trim() === '5', `Initial calculation: ${value.textContent}; ${rendered.textContent}`);
      check(functionStyle.textContent.includes('expression'), 'Function card missing');
      await checkRenderedGlyphs();
      await checkFunctionRun();

      const authored = '@S_{-inf}^x e^(-1/2(t-@mu)^2/@sigma^2) dt';
      setExpressionEditor(authored);
      check(expr.value === authored, 'Editor rewrote authored infinity or Greek tokens');
      setExpressionEditor('3+4');
      await evaluateExpression();
      check(String(value.textContent).trim() === '7', 'Second native calculation should equal 7');
      await checkRenderedGlyphs();
      check(currentHistoryLength() > 0, 'Evaluation history missing');

      setExpressionEditor('2*x');
      await evaluateExpression();
      check(rendered.dataset.displayTex.includes('\\mkern'), 'Native multiplication spacing missing');
      await checkRenderedGlyphs();
      setExpressionEditor('3+4');
      await evaluateExpression();

      const card = rendered.closest('.result-card');
      const before = resultZoomIndex(card);
      stepResultZoom(card, 1);
      check(resultZoomIndex(card) === before + 1, 'Result zoom did not change');
      await checkRenderedGlyphs();
      const expand = card.querySelector('[data-expand-card]');
      expand.click();
      check(expand.getAttribute('aria-expanded') === 'true', 'Result expansion failed');
      expand.click();
      help.click();
      check(!helpPane.classList.contains('hidden'), 'Help did not open');
      help.click();

      for (const mode of ['matrix', 'integrator', 'datetime', 'almanac', 'equation', 'diffequation', 'expression']) {
        setMode(mode);
        restoreModeEditor(mode);
        syncModeUI();
        check(currentMode() === mode, `Mode ${mode} unavailable`);
        check(document.querySelector(`[data-mode="${mode}"]`).classList.contains('active'), `Mode ${mode} tab inactive`);
      }
      setMode('datetime');
      syncModeUI();
      const dateButton = document.querySelector('[data-date-target="datetimeDate"]');
      dateButton.click();
      check(!marsDatePicker.classList.contains('hidden'), 'Date picker did not open');
      check(marsDatePickerGrid.children.length >= 28, 'Date grid missing');
      marsDatePickerClose.click();
      check(marsDatePicker.classList.contains('hidden'), 'Date picker did not close');
      setMode('expression');
      syncModeUI();
      await refreshMobileAccess();
      check(mobileTitle.textContent.length > 0, 'Mobile access metadata missing');
      await saveLastExpression('3+4');
      const saved = await (await nativeFetch('/state')).json();
      check(saved.expression === '3+4', 'Worksheet state not saved');
      check(!failures.length, failures.join('; '));
      location.href = '/?failure=catalogue';
    } catch (error) {
      await report(`FAIL: ${error.message}`);
    }
  });
})();
