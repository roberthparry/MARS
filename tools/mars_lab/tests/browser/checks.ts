/**
 * @file checks.ts
 * @brief Main browser regression harness for the Lab worksheet.
 *
 * Coordinates real-browser checks of evaluation, persistence, plots, layout and interaction using a test-only copy of the application page.
 */

/** Real-browser regressions injected only into a private test copy of the page. */
(function installBrowserChecks() {
    const startupCheck = new URLSearchParams(location.search).get('startup-check') === '1';
    const startupRequests: string[] = [];
    const startupReveals: string[] = [];
    if (startupCheck) {
        localStorage.setItem('mars.exprLab.lastMode', 'almanac');
        const originalFetch = window.fetch.bind(window);
        window.fetch = (input, options) => {
            startupRequests.push(String(input));
            return originalFetch(input, options);
        };
        const observer = new MutationObserver(() => {
            if (!document.documentElement.hasAttribute('data-starting')) {
                startupReveals.push(document.querySelector('[role="tab"][aria-selected="true"]')?.getAttribute('data-mode') || '');
                observer.disconnect();
            }
        });
        observer.observe(document.documentElement, {attributes: true, attributeFilter: ['data-starting']});
    }
    const failures: string[] = [];
    const nativeFetch = window.fetch.bind(window);
    const nativeSetTimeout = window.setTimeout.bind(window);
    const nativeClearTimeout = window.clearTimeout.bind(window);

    /** @brief Check native batch projection, clipping, stable depth order and temporary-handle lifetime. */
    function checkSurfaceProjectionPlan(): void {
        function check(condition: boolean, message: string): void {
            if (!condition) throw new Error('Surface projection plan: ' + message);
        }
        const frame = {
            width: 100, height: 100, fit: 100, drawMesh: true,
            rows: [1, 0, 0, 0, 1, 0, 0, 0, 1], scaling: [1, 1, 1], offset: [0, 0, 0]
        };
        const primitives: MarsSurfacePrimitive[] = [];
        const heights = [0.2, 0.8, 0.8];
        for (let i = 0; i < heights.length; ++i) {
            const z = heights[i];
            primitives.push({points: [[0, 0, z], [1, 0, z], [0, 1, z]], colour: String(i), depth: 0});
        }
        primitives.push({points: [[2, 2, 0], [3, 2, 0], [2, 3, 0]], colour: 'outside', depth: 0});
        const original = JSON.stringify(primitives);
        const plan = labDOM.call('lab_surface_polygons', frame, primitives, 0) as MarsSurfaceDrawing[];
        check(plan.length === 3 && plan[0].colour === '1' && plan[1].colour === '2' && plan[2].colour === '0',
              'far-to-near order is stable and excludes clipped primitives');
        for (const polygon of plan) {
            const source = primitives[Number(polygon.colour)];
            const single = labDOM.call('lab_surface_polygon', frame, source.points) as MarsSurfacePolygon;
            check(JSON.stringify(polygon.points) === JSON.stringify(single.points) && polygon.depth === single.depth,
                  'batch coordinates and depth match the individual projection');
        }
        check(JSON.stringify(primitives) === original, 'projection and sorting leave source geometry unchanged');
        const segments: MarsSurfacePrimitive[] = [
            {points: [[-1, 0.5, 0.5], [2, 0.5, 0.5]], colour: 'line', depth: 0},
            {points: [[0, 2, 0], [1, 2, 0]], colour: 'outside', depth: 0}
        ];
        const lines = labDOM.call('lab_surface_polygons', frame, segments, 1) as MarsSurfaceDrawing[];
        check(lines.length === 1 && lines[0].points[0][0] === 0 && lines[0].points[1][0] === 100,
              'segment clipping retains only the visible interval');
        const empty = labDOM.call('lab_surface_polygons', frame, [], 0) as MarsSurfaceDrawing[];
        check(empty.length === 0, 'empty geometry has an empty drawing plan');
        frame.drawMesh = false;
        const hidden = labDOM.call('lab_surface_polygons', frame, primitives, 0) as MarsSurfaceDrawing[];
        check(hidden.length === 0, 'hidden geometry has an empty drawing plan');
        frame.drawMesh = true;
        const many: MarsSurfacePrimitive[] = [];
        for (let i = 0; i < 2049; ++i) many.push(primitives[i % 3]);
        for (let repeat = 0; repeat < 2; ++repeat) {
            const result = labDOM.call('lab_surface_polygons', frame, many, 0) as MarsSurfaceDrawing[];
            check(result.length === many.length, 'large repeated projections stay within bridge handle capacity');
            for (let i = 1; i < result.length; ++i)
                check(result[i - 1].depth >= result[i].depth, 'large odd-sized plans remain depth ordered');
        }
    }

    /** @brief Bound each group so an unresolved mock identifies its owning regression. */
    async function runBrowserGroup(name: string, run: () => unknown, timeout = 60000): Promise<void> {
        let timer = 0;
        const deadline = new Promise(function startDeadline(_resolve, reject) {
            function timedOut(): void {
                reject(new Error(`Browser group ${name} did not finish within ${timeout / 1000} seconds`));
            }
            timer = nativeSetTimeout(timedOut, timeout);
        });
        try {
            await Promise.race([Promise.resolve().then(run), deadline]);
        } catch (error) {
            if (error instanceof Error) error.message = `Browser group ${name}: ${error.message}`;
            throw error;
        } finally {
            nativeClearTimeout(timer);
        }
    }

    const failureCase = new URLSearchParams(location.search).get('failure');
    const catalogueCheck = new URLSearchParams(location.search).get('catalogue-check');
    let releaseTowns: () => void = () => {};
    const townGate = new Promise<void>(resolve => { releaseTowns = resolve; });
    if (catalogueCheck) {
        nativeSetTimeout(() => {
            void report(`FAIL: catalogue check timed out (${catalogueCheck}); readyState=${document.readyState}; ` +
                `status=${document.getElementById('status')?.textContent}; ` +
                `errors=${failures.join('; ')}`);
        }, 60000);
        window.fetch = async (url, options) => {
            if (catalogueCheck === 'index-fail' && String(url) === '/jurisdictions')
                return new Response('unavailable', {status: 503});
            if (String(url).startsWith('/towns?after=')) {
                if (String(url).startsWith('/towns?after=0&jurisdiction=')) await townGate;
                else if (catalogueCheck === 'fail' && String(url).includes('&jurisdiction='))
                    return new Response('unavailable', {status: 503});
            }
            return nativeFetch(url, options);
        };
    }
    window.addEventListener('error', function recordError(event) {
        const target = event.target;
        const source = target instanceof Element ? target.getAttribute('src') || target.getAttribute('href') || target.tagName : 'unknown';
        failures.push(event.message || `Resource load failed: ${source.slice(0, 160)}`);
    }, true);
    window.addEventListener('unhandledrejection', function recordRejection(event) {
        if (!failureCase)
            failures.push(String(event.reason));
    });
    if (failureCase === 'catalogue') {
        window.fetch = (url, options) => String(url) === '/jurisdictions' ?
            Promise.resolve(new Response('unavailable', {status: 503})) :
            nativeFetch(url, options);
    }
    if (failureCase === 'database') {
        window.fetch = (url, options) => String(url) === '/jurisdictions' ?
            Promise.resolve(new Response(
                labWire.encode(
                    {available: false, options: [], towns: {}, locations: {}, error: 'Fixture database unavailable'}),
                {headers: {'Content-Type': 'application/x-protobuf'}})) :
            nativeFetch(url, options);
    }
    if (failureCase === 'server-missing' || failureCase === 'server-mismatch') {
        window.fetch = async (url, options) => {
            const response = await nativeFetch(url, options);
            if (String(url) !== '/bootstrap')
                return response;
            const config = labWire.decode(await response.arrayBuffer()) as LabTestRecord;
            if (failureCase === 'server-missing')
                delete config.BROWSER_ABI_VERSION;
            else
                config.BROWSER_ABI_VERSION = '0';
            return new Response(labWire.encode(config), {headers: {'Content-Type': 'application/x-protobuf'}});
        };
    }
    if (failureCase === 'script') {
        const append = document.head.appendChild.bind(document.head);
        document.head.appendChild = node => {
            if (node instanceof HTMLScriptElement && node.src.endsWith('/js/locations.js'))
                node.src = '/js/missing.js';
            return append(node);
        };
    }
    function check(condition: unknown, message: string): asserts condition {
        if (!condition)
            throw new Error(message);
    }
    async function checkDistributionPlot() {
        WorksheetBindings.controller.setExpressionEditor('@F{step(x)}');
        await WorksheetRequests.evaluation.evaluateExpression();
        await waitForAutomaticPlot();
        const host = (document.getElementById('plotRendered')! as LabTestPlotHost);
        check(host.querySelector<HTMLElement>('[data-impulse]')! && !host.textContent.includes('No plot'),
              'Fourier step spectrum reaches the visible plot with its impulse');
        check(WorksheetContext.rendered.querySelector('svg')! && !WorksheetContext.rendered.classList.contains('error'),
              'The complete distribution identity renders as mathematical TeX');
        WorksheetBindings.controller.setExpressionEditor('@F{asin(t)}');
        await WorksheetRequests.evaluation.evaluateExpression();
        await waitForAutomaticPlot();
        check(WorksheetContext.rendered.querySelector('svg') && !WorksheetContext.rendered.classList.contains('error'),
              'The compact finite-part transform identity renders as mathematical TeX');
        const identity = WorksheetContext.rendered.dataset.displayTex!;
        check(identity.includes('\\operatorname{Fp}\\!\\left[') && identity.includes('\\delta') && identity.includes(' = ') &&
              !identity.includes('NAN') && !identity.includes('\\underbrace') && !identity.includes('Curve values'),
              'The transform remains one complete identity without invalid coordinates or prose annotations');
        check(host.textContent.includes('Imaginary curve') && host.textContent.includes('Imaginary impulse weight'),
              'Distribution legends distinguish the sampled curve from the impulse weight');
        const legend = host.querySelector('[data-plot-legend]');
        check(legend && legend.querySelector('rect') &&
              legend.querySelector('path[stroke="#78d8cb"]') &&
              legend.querySelector('path[stroke="#b5a0ef"]'),
              'The boxed spectrum legend uses distinct curve and impulse colours');
        const legendBox = legend.querySelector('rect')!.getBoundingClientRect();
        const plotArea = host.querySelector('[data-series]')!.closest('svg')!.getBoundingClientRect();
        check(legendBox.bottom < plotArea.top,
              'The spectrum legend occupies a separate strip above the plot lines');
        let axisLabel: SVGTextElement | null = null;
        for (const label of host.querySelectorAll('text')) {
            if (label.textContent === 'Value / weight')
                axisLabel = label;
        }
        check(axisLabel && axisLabel.getBBox().x >= 0,
              'The complete distribution axis title fits inside the SVG');
        check(axisLabel && axisLabel.getBBox().x <= 32,
              'The distribution plot does not waste space to the left of its axis title');
        const caption = host.querySelector<HTMLElement>('.plot-figure-caption');
        check(caption && caption.textContent.includes('no pointwise 3-D, complex-plane or amplitude view'),
              'Distribution plots explain why ordinary complex projections are absent');
        const captionBox = caption.getBoundingClientRect();
        check(captionBox.top >= plotArea.bottom && Math.abs(captionBox.left - plotArea.left) < 2,
              'The spectrum caption sits below the plot and aligns with its vertical axis');
        check(!host.querySelector('.complex-grid') && host.querySelector('[data-impulse]'),
              'The distribution is retained instead of fabricating ordinary complex projections');
        const controls = document.getElementById('plotRange')!;
        const savedWidth = controls.style.width;
        controls.style.width = '60rem';
        await LabTestBrowser.paint();
        check(!document.getElementById('plotKindGroup')!.getClientRects().length &&
              host.querySelector('[data-distribution-spectrum]'),
              'Distribution spectra hide the inapplicable plot type selector');
        const input = document.getElementById('plotInputRangeGroup')!.getBoundingClientRect();
        const vertical = document.getElementById('plotVertical')!.getBoundingClientRect();
        check(Math.abs(input.top - vertical.top) < 1 && input.right < vertical.left,
              'Distribution input range and vertical range share one compact row');
        controls.style.width = '20rem';
        await LabTestBrowser.paint();
        const compact = controls.getBoundingClientRect();
        for (const id of ['plotInputRangeGroup', 'plotVertical']) {
            const bounds = document.getElementById(id)!.getBoundingClientRect();
            check(bounds.left >= compact.left && bounds.right <= compact.right + 1,
                  'Plot option cards wrap within a narrow panel');
        }
        controls.style.width = savedWidth;
        await LabTestBrowser.paint();
        WorksheetBindings.controller.setExpressionEditor('@F{zeta(sigma+it)}');
        await WorksheetRequests.evaluation.evaluateExpression();
        for (let attempt = 0; attempt < 1200 && !host.querySelector('[data-spectrum-pending]'); ++attempt)
            await LabTestBrowser.wait(25);
        await LabTestBrowser.paint();
        check(host.querySelector('[data-spectrum-pending]') &&
              host.textContent.includes('Set numerical values') && !controls.getClientRects().length,
              'An unset spectrum parameter prompts for values without irrelevant range controls');
        const sigma = WorksheetContext.variableValues.querySelector<HTMLInputElement>('[data-binding-name="σ"]')!;
        check(sigma, 'The spectrum exposes its sigma parameter');
        sigma.value = '2';
        sigma.dispatchEvent(new Event('input', {bubbles: true}));
        (document.getElementById('plotSecond')! as HTMLInputElement).value = 'σ';
        (document.getElementById('plotKind')! as HTMLInputElement).value = 'surface';
        await WorksheetRequests.evaluation.evaluateExpression();
        for (let attempt = 0; attempt < 1200 && !host.querySelector('[data-impulse]'); ++attempt)
            await LabTestBrowser.wait(25);
        await LabTestBrowser.paint();
        check(host.querySelector('[data-impulse]') && !host.querySelector('[data-spectrum-pending]') &&
              controls.getClientRects().length,
              'Binding sigma produces an impulse preview and restores its range controls: ' +
              host.textContent + '; controls=' + controls.className + '; result=' +
              WorksheetContext.rendered.dataset.displayTex + '; source=' + WorksheetContext.expr.value +
              '; range=' + controls.innerText);
        for (const id of ['plotKindGroup', 'plotSecondGroup', 'plotCamera', 'plotFirstChoices', 'plotVertical']) {
            check(!document.getElementById(id)!.getClientRects().length,
                  'Impulse spectra hide inapplicable controls: ' + id);
        }
        check((document.getElementById('plotSecond')! as HTMLInputElement).value === '' &&
              (document.getElementById('plotKind')! as HTMLInputElement).value === 'auto',
              'Installing a spectrum clears retained surface and second-variable selections');
        const spectrum = host.querySelector<SVGSVGElement>('[data-plot-chart][data-x-min]')!;
        check(spectrum && Number(spectrum.dataset.xMax) === 0 &&
              host.querySelectorAll('[data-schematic-impulse]').length === 100 &&
              Math.abs(Number(spectrum.dataset.xMin) + Math.log(100)) < 1e-10 &&
              host.querySelector('[data-schematic-impulse] [stroke-dasharray="1 4"]'),
              'A hundred equal-height stems have dotted tips and bounds from the last term to zero');
        check(!host.querySelector('.plot-figure-caption'),
              'Pure impulse spectra do not add an explanatory caption');
        const initialSpan = Number(spectrum.dataset.xMax) - Number(spectrum.dataset.xMin);
        document.querySelector<HTMLElement>('#plotCard [data-zoom-step="1"]')!.click();
        let zoomedSpan = initialSpan;
        for (let attempt = 0; attempt < 1200 && zoomedSpan >= initialSpan; ++attempt) {
            await LabTestBrowser.wait(25);
            const chart = host.querySelector<SVGSVGElement>('[data-plot-chart][data-x-min]');
            if (chart)
                zoomedSpan = Number(chart.dataset.xMax) - Number(chart.dataset.xMin);
        }
        check(zoomedSpan < initialSpan, 'Spectrum zoom narrows the frequency range and redraws');
        document.querySelector<HTMLElement>('#plotCard [data-zoom-reset]')!.click();
        for (let attempt = 0; attempt < 1200 && Math.abs(zoomedSpan - initialSpan) > 1e-10; ++attempt) {
            await LabTestBrowser.wait(25);
            const chart = host.querySelector<SVGSVGElement>('[data-plot-chart][data-x-min]');
            if (chart)
                zoomedSpan = Number(chart.dataset.xMax) - Number(chart.dataset.xMin);
        }
        check(Math.abs(zoomedSpan - initialSpan) < 1e-10, 'Reset restores the complete displayed spectrum');
        WorksheetBindings.controller.setExpressionEditor('@F{sin(t)}');
        await WorksheetRequests.evaluation.evaluateExpression();
        for (let attempt = 0; attempt < 1200 && host.querySelectorAll('[data-impulse]').length !== 2; ++attempt)
            await LabTestBrowser.wait(25);
        const sineSpectrum = host.querySelector<SVGSVGElement>('[data-plot-chart][data-x-min]')!;
        check(sineSpectrum && Number(sineSpectrum.dataset.xMin) < -1 && Number(sineSpectrum.dataset.xMax) > 1,
              'Switching from one-sided zeta to sine fits both impulses with endpoint clearance');
        for (const stem of host.querySelectorAll<SVGGraphicsElement>('[data-impulse]')) {
            const bounds = stem.getBoundingClientRect();
            const viewport = stem.closest('svg')!.getBoundingClientRect();
            check(bounds.left > viewport.left + 2 && bounds.right < viewport.right - 2,
                  'Both signed-frequency impulses are visibly inside the chart, not clipped by its frame');
        }
        WorksheetBindings.controller.setExpressionEditor('x^2');
        await WorksheetRequests.evaluation.evaluateExpression();
        await waitForAutomaticPlot();
        check(document.getElementById('plotKindGroup')!.getClientRects().length > 0,
              'Ordinary plots restore the plot type selector');
    }

    /** @brief Use crosshairs over 2-D plots, reserving the grabbing hand for active drags. */
    function checkFlatPlotCursor(panel: HTMLElement, dragging = false): void {
        const body = panel.querySelector<HTMLElement>('.rendered')!;
        check(getComputedStyle(body).cursor === 'crosshair',
              panel.dataset.complexPanel + ' plot whitespace uses a crosshair');
        for (const target of body.querySelectorAll('.rendered-zoom-frame, svg, path')) {
            const expected = dragging && target.closest('svg') ? 'grabbing' : 'crosshair';
            check(getComputedStyle(target).cursor === expected,
                  panel.dataset.complexPanel + ' uses ' + expected + ' over ' + target.tagName);
        }
        check(getComputedStyle(panel.querySelector('button')!).cursor === 'pointer',
              'The plot cursor does not change the Expand/Collapse button');
    }

    async function checkComplexGrid() {
        WorksheetBindings.controller.setExpressionEditor('exp(i*x)');
        await WorksheetRequests.evaluation.evaluateExpression();
        const host = (document.getElementById('plotRendered')! as LabTestPlotHost);
        const deadline = performance.now() + 20000;
        while (!host.querySelector<HTMLElement>('.complex-grid')! && performance.now() < deadline)
            await LabTestBrowser.wait(30);
        const panels = host.querySelectorAll<HTMLElement>('.complex-panel');
        async function assertFits() {
            // Retained WebGL canvases follow the SVG through ResizeObserver and animation frames.
            await LabTestBrowser.paint();
            for (const panel of panels) {
                const body = panel.querySelector<HTMLElement>('.rendered')!;
                if (!panel.getClientRects().length)
                    continue;
                if (panel.dataset.complexPanel !== 'svg')
                    checkFlatPlotCursor(panel);
                check(body.scrollWidth <= body.clientWidth + 1,
                      `Complex projection ${panel.dataset.complexPanel} must fit: ` +
                      `scroll=${body.scrollWidth}, width=${body.clientWidth}, host=${host.clientWidth}, ` +
                      `expanded=${panel.dataset.expanded}`);
                const svg = body.querySelector<HTMLElement>('.rendered-zoom-frame > svg')!;
                check(svg.getBoundingClientRect().width <= body.clientWidth + 1,
                      'The complete SVG fits inside each complex projection');
            }
        }
        check(panels.length === 4 && panels[0].querySelector<HTMLElement>('[data-segment]')!,
              'Complex output must show four native projections, including a true 3-D line');
        await window.checkLabSurfaceHoverDOM(panels[0].querySelector<SVGSVGElement>('svg[data-segments="true"]')!);
        const hostStyle = getComputedStyle(host), gridStyle = getComputedStyle(host.querySelector<HTMLElement>('.complex-grid')!);
        check((['paddingTop', 'paddingRight', 'paddingBottom', 'paddingLeft'] as const)
            .every(side => parseFloat(hostStyle[side]) <= 8 && parseFloat(gridStyle[side]) === 0) &&
              parseFloat(gridStyle.gap) <= 8,
              'Complex cards use one narrow outer gutter, without stacked formula padding');
        const cameraControls = document.getElementById('plotCamera')!;
        check(cameraControls.parentElement === panels[0] && document.querySelectorAll<HTMLElement>('#plotCamera').length === 1,
              'The original camera controls belong to the 3-D curve card, not below the four-view grid');
        function figureTitle(panel: HTMLElement) {
            const figure = panel.querySelector<HTMLElement>('[data-plot-figure]')!;
            return figure.querySelector<HTMLElement>(':scope > title')!.textContent || figure.getAttribute('aria-label');
        }
        const formula = figureTitle(panels[0]);
        check(formula && Array.from(panels).every(panel => panel.querySelector<SVGTextElement>('[data-plot-title]')! &&
              figureTitle(panel) === formula),
              'Every complex projection includes the same native mathematical expression title');
        check(panels[1].querySelector('svg')! && panels[2].textContent.includes('Imaginary') &&
              panels[3].textContent.includes('Amplitude'), 'All four projections are populated');
        await assertFits();
        const oldWidth = host.style.width;
        host.style.width = '340px';
        await assertFits();
        host.style.width = oldWidth;
        await assertFits();
        const plane = panels[1].querySelector<SVGSVGElement>('[data-x-min]')!;
        const xScale = Number(plane.dataset.plotWidth) / (Number(plane.dataset.xMax) - Number(plane.dataset.xMin));
        const yScale = Number(plane.dataset.plotHeight) / (Number(plane.dataset.yMax) - Number(plane.dataset.yMin));
        check(Math.abs(xScale / yScale - 1) < 0.0001, 'The complex plane preserves circular geometry');
        check(document.getElementById('plotVertical')!.classList.contains('hidden') &&
              !document.getElementById('plotInputRangeGroup')!.closest<HTMLElement>('.plot-ranges')!.classList.contains('hidden'),
              'Complex projections expose an input-range policy, not misleading common output limits');
        check(!document.getElementById('plotFirstGroup')!.getClientRects().length,
              'Automatic complex plots hide the entire single-variable range panel');
        const chart = panels[0].querySelector<SVGSVGElement>('[data-surface]')!;
        check(labSurfacePreview.draw(chart, [225, 30, 0]),
              'The space curve supports the retained local renderer');
        const button = panels[0].querySelector('button')!;
        button.click();
        check(panels[0].classList.contains('complex-expanded') && button.getAttribute('aria-expanded') === 'true',
              'Each complex projection expands independently');
        check(Array.from(panels).slice(1).every(panel => !panel.getClientRects().length),
              'Expansion replaces the entire grid, not just one row');
        check(panels[0].clientWidth >= host.querySelector<HTMLElement>('.complex-grid')!.clientWidth - 24,
              'The expanded projection fills the parent plot width');
        await assertFits();
        function assertSharp() {
            const layers = panels[0].querySelectorAll<HTMLCanvasElement>('[data-surface-overlay] canvas');
            check(layers.length === 2, 'The 3-D plot has separate geometry and label canvases');
            for (const layer of layers) {
                const rect = layer.getBoundingClientRect();
                check(layer.width >= Math.ceil(rect.width * window.devicePixelRatio) - 1 &&
                      layer.height >= Math.ceil(rect.height * window.devicePixelRatio) - 1,
                      'Expanded geometry and labels render at displayed device-pixel resolution');
            }
        }
        assertSharp();
        check(cameraControls.getBoundingClientRect().top >= panels[0].querySelector<HTMLElement>('.rendered')!.getBoundingClientRect().bottom &&
              panels[0].getBoundingClientRect().height <= window.innerHeight,
              'The expanded curve and its controls fit together within the viewport: ' + JSON.stringify({
                  camera: cameraControls.getBoundingClientRect().toJSON(),
                  body: panels[0].querySelector<HTMLElement>('.rendered')!.getBoundingClientRect().toJSON(),
                  panel: panels[0].getBoundingClientRect().toJSON(), height: window.innerHeight
              }));
        const pixelRatio = Object.getOwnPropertyDescriptor(window, 'devicePixelRatio');
        try {
            Object.defineProperty(window, 'devicePixelRatio', {configurable: true, value: 2});
            check(labSurfacePreview.draw(chart, [225, 30, 0]), 'High-density redraw succeeds');
            assertSharp();
        } finally {
            if (pixelRatio) Object.defineProperty(window, 'devicePixelRatio', pixelRatio);
            else Reflect.deleteProperty(window, 'devicePixelRatio');
            labSurfacePreview.draw(chart, [225, 30, 0]);
        }
        async function waitForVector() {
            const deadline = performance.now() + 5000;
            while (chart.dataset.localRenderer !== 'svg' && performance.now() < deadline)
                await LabTestBrowser.wait(25);
            const drawing = chart.querySelector<SVGSVGElement>('[data-surface-settled]')!;
            check(chart.dataset.localRenderer === 'svg' && drawing?.querySelector('path')! &&
                  drawing.textContent.includes('ℜ') && drawing.textContent.includes('ℑ') &&
                  getComputedStyle(panels[0].querySelector<SVGSVGElement>('[data-surface-overlay]')!).display === 'none',
                  'Idle space curves display vector geometry and Gothic axis labels, not a canvas');
        }
        await waitForVector();
        const angleControls = ['plotAzimuth', 'plotElevation', 'plotRoll'].map(id => document.getElementById(id)! as HTMLInputElement);
        const savedAngles = angleControls.map(control => control.value);
        const rect = chart.getBoundingClientRect(), quaternion = chart.dataset.cameraQuaternion;
        const sendCurve = (target: EventTarget, type: string, offset: number) => target.dispatchEvent(new PointerEvent(type, {
            pointerId: 91, pointerType: 'mouse', isPrimary: true, button: 0,
            buttons: type === 'pointerup' ? 0 : 1, clientX: rect.left + rect.width / 2 + offset,
            clientY: rect.top + rect.height / 2, bubbles: true, cancelable: true
        }));
        sendCurve(chart, 'pointerdown', 0);
        sendCurve(window, 'pointermove', 35);
        await LabTestBrowser.paint();
        check(!chart.querySelector<SVGSVGElement>('[data-surface-settled]')! && chart.dataset.localRenderer !== 'svg' &&
              chart.dataset.cameraQuaternion !== quaternion,
              'Dragging an idle SVG curve immediately resumes the retained camera preview');
        sendCurve(window, 'pointerup', 35);
        await waitForVector();
        angleControls.forEach((control, i) => {
            control.value = savedAngles[i];
            document.getElementById(control.id + 'Value')!.textContent = savedAngles[i] + '°';
        });
        labSurfacePreview.draw(chart, [Number(savedAngles[0]), Number(savedAngles[1]), Number(savedAngles[2])]);
        button.click();
        check(!panels[0].classList.contains('complex-expanded'), 'Projection collapse restores the grid');
        check(Array.from(panels).every(panel => panel.getClientRects().length),
              'Collapsing restores all four projections');
        for (const panel of Array.from(panels).slice(1)) {
            panel.querySelector('button')!.click();
            check(!cameraControls.getClientRects().length, 'Camera controls hide when a different projection expands');
            check(Array.from(panels).filter(entry => entry.getClientRects().length).length === 1 &&
                  panel.getClientRects().length, 'Every projection can occupy the whole parent plot');
            await assertFits();
            panel.querySelector('button')!.click();
        }
        await WorksheetWorkspace.mode.selectWorksheetMode('matrix');
        await WorksheetWorkspace.mode.selectWorksheetMode('expression');
        const restored = host.querySelector<HTMLElement>('.complex-panel')!;
        check(document.getElementById('plotCamera')! === cameraControls && cameraControls.parentElement === restored &&
              document.querySelectorAll<HTMLElement>('#plotCamera').length === 1,
              'Mode restoration retains one live camera control set in the restored curve card');
        restored.querySelector('button')!.click();
        check(restored.classList.contains('complex-expanded'), 'Restored complex cards retain working expansion');
        restored.querySelector('button')!.click();
        await runBrowserGroup('checkDockedCameraControls', checkDockedCameraControls);
        await runBrowserGroup('checkComplexComponentsPan', checkComplexComponentsPan);
        await checkComplexComponentsPan('complex_plane');
        await checkComplexComponentsPan('amplitude');
        await runBrowserGroup('checkComplexAutoZoom', checkComplexAutoZoom);
        await runBrowserGroup('checkRepeatedComplexWheel', checkRepeatedComplexWheel);
        await runBrowserGroup('checkEvaluatePlotReset', checkEvaluatePlotReset);
    }

    async function checkRepeatedComplexWheel() {
        const host = (document.getElementById('plotRendered')! as LabTestPlotHost);
        for (const projection of ['complex_plane', 'components', 'amplitude', 'svg']) {
            for (let burst = 0; burst < 2; ++burst) {
                const panel = host.querySelector<HTMLElement>(`[data-complex-panel="${projection}"]`)!;
                const chart = panel.querySelector<SVGSVGElement>('[data-plot-chart], [data-surface]')!;
                const siblings = [...host.querySelectorAll<HTMLElement>('.complex-panel')]
                    .filter(entry => entry !== panel).map(entry => {
                        const node = entry.querySelector<SVGSVGElement>('[data-plot-chart], [data-surface]')!;
                        return {node, bounds: ['xMin', 'xMax', 'yMin', 'yMax', 'zMin', 'zMax']
                            .map(key => node.dataset[key]).join(',')};
                    });
                const span = Number(chart.dataset.xMax) - Number(chart.dataset.xMin);
                const orientation = labSurfacePreview.snapshot()?.orientation.slice();
                const box = chart.getBoundingClientRect();
                const wheelDelta = (burst ? 1 : -1) * (projection === 'svg' ? 200 : 100);
                const event = new WheelEvent('wheel', {
                    clientX: box.left + box.width / 2, clientY: box.top + box.height / 2,
                    deltaY: wheelDelta, ctrlKey: true, bubbles: true, cancelable: true
                });
                chart.dispatchEvent(event);
                check(event.defaultPrevented, projection + ' Ctrl+wheel cancels scrolling across redraws');
                const deadline = performance.now() + 25000;
                if (projection !== 'svg') {
                    while ((chart.isConnected || host.marsPlotZoom?.busy || host.marsPlotZoom?.wheel) &&
                           performance.now() < deadline)
                        await LabTestBrowser.wait(25);
                    check(!chart.isConnected && !host.marsPlotZoom?.busy && !host.marsPlotZoom?.wheel,
                          projection + ' finishes each wheel burst and accepts the next');
                } else {
                    while ((chart.isConnected || WorksheetRequests.labRequests.busy() ||
                            labSurfacePreview.snapshot()?.volume.dirty) && performance.now() < deadline)
                        await LabTestBrowser.wait(25);
                    const next = host.querySelector<SVGSVGElement>('[data-complex-panel="svg"] [data-surface]')!;
                    const nextSpan = Number(next.dataset.xMax) - Number(next.dataset.xMin);
                    check(!chart.isConnected && next?.isConnected && !WorksheetRequests.labRequests.busy() &&
                          !labSurfacePreview.snapshot()?.volume.dirty &&
                          Math.abs(nextSpan / span - Math.exp(wheelDelta * 0.002)) < 1e-8,
                          'Space-curve wheel zoom uses its own chart span, not shared bounds changed by sibling 2-D zooms: ' +
                          JSON.stringify({span, nextSpan, expectedRatio: Math.exp(wheelDelta * 0.002),
                              sharedLower: (document.getElementById('plotLower')! as HTMLInputElement).value,
                              sharedUpper: (document.getElementById('plotUpper')! as HTMLInputElement).value}));
                    const view = labSurfacePreview.prepare(next)!;
                    check(Math.abs(view.camera.scale - 1) < 1e-10 &&
                          orientation?.every((value, i) => Math.abs(value - view.orientation[i]) < 1e-8),
                          'Space-curve wheel redraw fits the new samples without magnification or camera rotation');
                }
                check(siblings.every(({node, bounds}) => node.isConnected &&
                      ['xMin', 'xMax', 'yMin', 'yMax', 'zMin', 'zMax'].map(key => node.dataset[key]).join(',') === bounds),
                      projection + ' wheel zoom preserves sibling chart nodes and bounds');
            }
        }
    }

    async function checkDockedCameraControls() {
        const host = (document.getElementById('plotRendered')! as LabTestPlotHost);
        const panel = () => host.querySelector<HTMLElement>('[data-complex-panel="svg"]')!;
        const chart = () => panel().querySelector<SVGSVGElement>('[data-surface]')!;
        const frame = LabTestBrowser.paint;
        for (const expanded of [false, true]) {
            if (expanded) panel().querySelector('button')!.click();
            await frame();
            const camera = document.getElementById('plotCamera')!;
            check(camera.getBoundingClientRect().height <= 110,
                  'Docked camera controls are compact in grid and expanded views');
            for (const slider of camera.querySelectorAll<HTMLInputElement>('input[type="range"]')) {
                check(slider.getBoundingClientRect().width >= slider.parentElement!.clientWidth - 1 &&
                      slider.getBoundingClientRect().height >= (matchMedia('(pointer: coarse)').matches ? 36 : 28),
                      'Every docked slider fills its field while retaining a touch-sized target');
            }
            for (const [axis, id] of ['plotAzimuth', 'plotElevation', 'plotRoll'].entries()) {
                const slider = (document.getElementById(id)! as LabTestPlotInput), saved = slider.value;
                const before = chart().dataset.cameraQuaternion;
                const fit = labSurfacePreview.prepare(chart())!.framePlan!.fit;
                slider.value = String(Number(saved) + 10);
                slider.dispatchEvent(new Event('input', {bubbles: true}));
                await frame();
                check(chart().dataset.cameraQuaternion !== before &&
                      Number(chart().dataset.localCamera!.split(',')[axis]) === Number(slider.value),
                      id + ' updates the docked curve camera, not just its readout');
                check(Math.abs(labSurfacePreview.prepare(chart())!.framePlan!.fit - fit) < 1e-8,
                      'Space-curve rotation keeps a stable fit instead of pumping its scale');
                slider.dispatchEvent(new Event('change', {bubbles: true}));
                await frame();
                slider.value = saved;
                slider.dispatchEvent(new Event('input', {bubbles: true}));
                await frame();
                slider.dispatchEvent(new Event('change', {bubbles: true}));
                await frame();
            }
            const zoom = document.getElementById('plotZoom')! as HTMLInputElement;
            const baseline = [Number(chart().dataset.xMin), Number(chart().dataset.xMax)];
            const baselineSpan = baseline[1] - baseline[0];
            const orientation = labSurfacePreview.prepare(chart())!.orientation.slice();
            const siblings = [...host.querySelectorAll<HTMLElement>('.complex-panel')]
                .filter(entry => entry !== panel()).map(entry => {
                    const node = entry.querySelector<SVGSVGElement>('[data-plot-chart]')!;
                    return {node, bounds: ['xMin', 'xMax', 'yMin', 'yMax'].map(key => node.dataset[key]).join(',')};
                });
            async function waitForCurve(previous: SVGSVGElement) {
                const deadline = performance.now() + 25000;
                while ((previous.isConnected || WorksheetRequests.labRequests.busy() ||
                        labSurfacePreview.snapshot()?.volume.dirty || !chart() ||
                        chart()?.dataset.plotQuality === 'preview' || chart()?.dataset.localRenderer !== 'svg') &&
                       performance.now() < deadline)
                    await LabTestBrowser.wait(25);
                check(!previous.isConnected && !WorksheetRequests.labRequests.busy() &&
                      !labSurfacePreview.snapshot()?.volume.dirty && chart()?.dataset.localRenderer === 'svg' &&
                      chart()?.dataset.plotQuality !== 'preview', 'Curve range zoom completes a settled native SVG redraw');
                const view = labSurfacePreview.prepare(chart())!;
                check(Math.abs(view.camera.scale - 1) < 1e-10 &&
                      orientation.every((value, i) => Math.abs(value - view.orientation[i]) < 1e-8),
                      'Resampled curve fits its card at scale one and retains camera orientation');
                check(siblings.every(({node, bounds}) => node.isConnected &&
                      ['xMin', 'xMax', 'yMin', 'yMax'].map(key => node.dataset[key]).join(',') === bounds),
                      'Curve range zoom leaves every sibling chart and its bounds untouched');
            }
            for (const exponent of [1, 2, 0]) {
                const previous = chart();
                zoom.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, button: 0, isPrimary: true}));
                zoom.value = String(exponent);
                zoom.dispatchEvent(new Event('input', {bubbles: true}));
                zoom.dispatchEvent(new Event('change', {bubbles: true}));
                await waitForCurve(previous);
                const span = Number(chart().dataset.xMax) - Number(chart().dataset.xMin);
                check(Math.abs(span - baselineSpan / 2 ** exponent) < baselineSpan * 1e-8 &&
                      Math.abs(Number(chart().dataset.xMax) + Number(chart().dataset.xMin) -
                               baseline[0] - baseline[1]) < baselineSpan * 1e-8,
                      'Curve zoom selects centred 2x/4x ranges and returns to the original 100% baseline');
            }
            if (!expanded) {
                const previous = chart(), box = previous.getBoundingClientRect();
                const cx = box.left + box.width / 2, cy = box.top + box.height / 2;
                const send = (target: EventTarget, type: string, id: number, offset: number) =>
                    target.dispatchEvent(new PointerEvent(type, {
                        pointerId: id, pointerType: 'touch', isPrimary: id === 171, button: 0,
                        buttons: type === 'pointerup' || type === 'pointercancel' ? 0 : 1,
                        clientX: cx + offset, clientY: cy,
                        bubbles: true, cancelable: true
                    }));
                const generation = WorksheetRequests.labRequests.latestMain();
                send(previous, 'pointerdown', 171, -30);
                send(previous, 'pointerdown', 172, 30);
                send(window, 'pointermove', 171, -60);
                send(window, 'pointermove', 172, 60);
                await frame();
                send(window, 'pointercancel', 172, 60);
                send(window, 'pointerup', 171, -60);
                await frame();
                check(previous.isConnected && chart() === previous &&
                      WorksheetRequests.labRequests.latestMain() === generation &&
                      !labSurfacePreview.snapshot()?.volume.dirty &&
                      Math.abs(labSurfacePreview.prepare(chart())!.camera.scale - 1) < 1e-10 &&
                      Number(chart().dataset.xMin) === baseline[0] && Number(chart().dataset.xMax) === baseline[1],
                      'Cancelling a curve pinch restores its baseline without requesting native samples');
                send(previous, 'pointerdown', 171, -30);
                send(previous, 'pointerdown', 172, 30);
                send(window, 'pointermove', 171, -60);
                send(window, 'pointermove', 172, 60);
                await frame();
                send(window, 'pointerup', 172, 60);
                send(window, 'pointerup', 171, -60);
                await waitForCurve(previous);
                check(Math.abs(Number(chart().dataset.xMax) - Number(chart().dataset.xMin) - baselineSpan / 2) <
                      baselineSpan * 1e-8, 'A two-finger spread resamples half the curve parameter range');
                const pinched = chart();
                host.closest<HTMLElement>('.result-card')!.querySelector<HTMLElement>('[data-zoom-reset]')!.click();
                await waitForCurve(pinched);
                check(Math.abs(Number(chart().dataset.xMin) - baseline[0]) < baselineSpan * 1e-8 &&
                      Math.abs(Number(chart().dataset.xMax) - baseline[1]) < baselineSpan * 1e-8,
                      'Reset after a curve pinch restores the original parameter-range baseline');
            }
            check(chart().querySelector<SVGSVGElement>('[data-surface-settled] [data-series]')!.querySelectorAll('path').length === 1,
                  'The settled helix batches its segments into one SVG path');
            if (expanded) panel().querySelector('button')!.click();
        }
        await WorksheetRequests.plots.takeCalculus((document.getElementById('plotFirst')! as HTMLInputElement).value, 'plot', null, true);
        check(chart().dataset.plotQuality === 'preview' && chart().querySelector<HTMLElement>('[data-preview-notice]')!,
              'A native coarse curve exposes its preview quality and notice');
        const slider = (document.getElementById('plotAzimuth')! as HTMLInputElement);
        slider.value = String((Number(slider.value) + 10) % 360);
        slider.dispatchEvent(new Event('input', {bubbles: true}));
        await frame();
        slider.dispatchEvent(new Event('change', {bubbles: true}));
        const deadline = performance.now() + 25000;
        while (chart()?.dataset.plotQuality !== 'full' && performance.now() < deadline)
            await LabTestBrowser.wait(25);
        check(chart()?.dataset.plotQuality === 'full' && !chart().querySelector<HTMLElement>('[data-preview-notice]')!,
              'Releasing the camera replaces coarse samples and removes their preview notice');
    }

    /** @brief Check curve transforms when finite samples stop short of the requested interval. */
    function checkCurveSampleFrame(view: MarsSurfaceView): void {
        const originalFrame = labDOM.call('lab_surface_frame', view, null) as MarsSurfaceFrame;
        const volume = labSurfaceVolume.clone(view.volume);
        volume.base[0] = -8;
        volume.base[1] = 8;
        volume.sampled[0] = -4;
        volume.sampled[1] = 2;
        volume.bounds[0] = -2;
        volume.bounds[1] = 2;
        volume.sampleExponent = 1;
        volume.exponent = 2;
        volume.dirty = true;
        const preview = {...view, volume};
        const frame = labDOM.call('lab_surface_frame', preview, null) as MarsSurfaceFrame;
        check(frame.scaling[0] === 1.5 && frame.offset[0] === -0.25 &&
              frame.scaling[1] === 1 && frame.scaling[2] === 1 && frame.offset[1] === 0 && frame.offset[2] === 0,
              'Curve previews map actual finite x samples into the selected interval without magnifying output axes');
        check(frame.rows.every((value, index) => value === originalFrame.rows[index]) && frame.fit === originalFrame.fit,
              'Input zoom stretches selected curve samples into the original box instead of shortening its x axis');
        const first = (1 / 3 - 0.5) * frame.scaling[0] + frame.offset[0] + 0.5;
        const last = (1 - 0.5) * frame.scaling[0] + frame.offset[0] + 0.5;
        check(Math.abs(first) < 1e-12 && Math.abs(last - 1) < 1e-12,
              'An asymmetric finite sample clips at the requested endpoints, not the sampled midpoint');
        volume.bounds[0] = volume.bounds[1] = volume.sampled[0] = volume.sampled[1] = 1;
        volume.exponent = 80;
        volume.sampleExponent = 79;
        const deep = labDOM.call('lab_surface_frame', preview, null) as MarsSurfaceFrame;
        check(deep.drawMesh && deep.scaling[0] === 2 && deep.offset[0] === 0,
              'Microscopic curve zoom retains the exponent transform when double endpoints coincide');
    }

    /** @brief Keep the zoom slider track fixed while moving within an already extended range. */
    function checkCurveSliderTrack(view: MarsSurfaceView): void {
        const slider = document.getElementById('plotZoom')! as HTMLInputElement;
        const minimum = slider.min, maximum = slider.max;
        const volume = labSurfaceVolume.clone(view.volume);
        const preview = {...view, volume};
        try {
            for (const direction of [-1, 1]) {
                volume.exponent = direction < 0 ? -3.25 : 7.25;
                labDOM.call('lab_surface_zoom_sync', preview);
                const low = slider.min, high = slider.max;
                volume.exponent += direction;
                labDOM.call('lab_surface_zoom_sync', preview);
                check(slider.min === low && slider.max === high && Number(slider.value) === volume.exponent,
                      'Moving within an extended zoom track never shifts the limits beneath the pointer');
                volume.exponent = 0;
                labDOM.call('lab_surface_zoom_sync', preview);
                check(slider.min === low && slider.max === high && slider.value === '0',
                      'Passing through 100% does not collapse the slider track during a gesture');
            }
        } finally {
            slider.min = minimum;
            slider.max = maximum;
            labDOM.call('lab_surface_zoom_sync', view);
        }
    }

    /** @brief Regress logarithmic curve zoom, refinement ownership and independent sibling plots. */
    async function checkLogarithmicCurveZoom(): Promise<void> {
        WorksheetBindings.controller.setExpressionEditor('ln(-x^3)');
        await WorksheetRequests.evaluation.evaluateExpression();
        await waitForAutomaticPlot();
        const host = document.getElementById('plotRendered')!;
        const zoom = document.getElementById('plotZoom')! as HTMLInputElement;
        function curve(): SVGSVGElement {
            return host.querySelector<SVGSVGElement>('[data-complex-panel="svg"] [data-surface]')!;
        }
        async function waitForCurve(previous: SVGSVGElement | null): Promise<void> {
            const deadline = performance.now() + 25000;
            while ((previous?.isConnected || !curve() || WorksheetRequests.labRequests.busy() ||
                    labSurfacePreview.snapshot()?.volume.dirty || curve()?.dataset.localRenderer !== 'svg') &&
                   performance.now() < deadline)
                await LabTestBrowser.wait(25);
            check(!previous?.isConnected && !!curve() && !WorksheetRequests.labRequests.busy() &&
                  !labSurfacePreview.snapshot()?.volume.dirty && curve().dataset.localRenderer === 'svg',
                  'Logarithmic curve zoom finishes with settled geometry, not a stale preview');
        }
        await waitForCurve(null);
        await window.checkLabVisibleCurveHover(curve());
        const initial = labSurfacePreview.prepare(curve())!;
        const baseline = [...initial.volume.base];
        const orientation = [...initial.orientation];
        const baselineUnits = [...initial.volume.units];
        const baselineRows = [...initial.framePlan!.rows];
        const siblings = [...host.querySelectorAll<SVGSVGElement>('[data-complex-panel]:not([data-complex-panel="svg"]) [data-plot-chart]')];
        const siblingContent: string[] = [];
        for (const sibling of siblings) siblingContent.push(sibling.outerHTML);
        checkCurveSampleFrame(initial);
        checkCurveSliderTrack(initial);
        try {
            for (const exponent of [1, 2, -1, 0]) {
                const previous = curve();
                const generation = WorksheetRequests.labRequests.latestMain();
                zoom.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, button: 0, isPrimary: true}));
                zoom.value = String(exponent);
                zoom.dispatchEvent(new Event('input', {bubbles: true}));
                const preview = labSurfacePreview.prepare(previous)!;
                check(preview.framePlan!.rows.every((value, index) => Math.abs(value - baselineRows[index]) < 1e-10),
                      'Every logarithmic zoom preview retains the original coordinate-box proportions');
                if (exponent === 1) {
                    await LabTestBrowser.wait(450);
                    check(previous.isConnected && WorksheetRequests.labRequests.latestMain() === generation,
                          'Holding logarithmic curve zoom previews the range without premature sampling');
                }
                zoom.dispatchEvent(new Event('change', {bubbles: true}));
                await waitForCurve(previous);
                await window.checkLabVisibleCurveHover(curve());
                const view = labSurfacePreview.prepare(curve())!;
                const span = (baseline[1] - baseline[0]) / 2 ** exponent;
                check(Math.abs(view.volume.sampled[1] - view.volume.sampled[0] - span) < span * 1e-8 &&
                      Math.abs(view.volume.sampled[0] + view.volume.sampled[1] - baseline[0] - baseline[1]) < span * 1e-8,
                      'Logarithmic zoom samples the requested centred x-range and restores the full range at 100%');
                check(view.camera.scale === 1 && view.framePlan!.scaling.every(value => value === 1) &&
                      orientation.every((value, index) => Math.abs(value - view.orientation[index]) < 1e-8) &&
                      !view.framePlan!.texts.some(text => text.text.includes('Preview')),
                      'Accepted logarithmic samples have no camera magnification, rotation jump or stale preview notice');
                check(view.volume.units.every((value, index) => value === baselineUnits[index]) &&
                      view.framePlan!.rows.every((value, index) => Math.abs(value - baselineRows[index]) < 1e-10),
                      'Refinement retains the preview box: the x axis does not collapse as the interval narrows');
                for (let i = 0; i < siblings.length; ++i)
                    check(siblings[i].isConnected && siblings[i].outerHTML === siblingContent[i],
                          'Logarithmic curve zoom leaves sibling geometry and bounds unchanged');
            }
            const fetchBefore = WorksheetRequests.evaluationTransport.fetchEvaluation;
            let release: ((result: WorksheetRequests.MarsApiPostResult) => void) | undefined;
            const previous = curve();
            try {
                WorksheetRequests.evaluationTransport.fetchEvaluation = function (...args) {
                    if (args[2] !== 'plot') return fetchBefore(...args);
                    return new Promise(resolve => { release = resolve; });
                };
                zoom.value = '1';
                zoom.dispatchEvent(new Event('input', {bubbles: true}));
                const deadline = performance.now() + 5000;
                while (!release && performance.now() < deadline)
                    await LabTestBrowser.wait(25);
                check(!!release, 'Hold an older logarithmic range refinement in flight');
                zoom.value = '2';
                zoom.dispatchEvent(new Event('input', {bubbles: true}));
                const staleData = {ok: false, error: 'Obsolete logarithmic range'};
                release!({response: Object.assign(new Response(), {labData: async () => staleData}), data: staleData});
                WorksheetRequests.evaluationTransport.fetchEvaluation = fetchBefore;
                await waitForCurve(previous);
                await window.checkLabVisibleCurveHover(curve());
                const volume = labSurfacePreview.snapshot()!.volume;
                check(volume.exponent === 2 && volume.sampleExponent === 2 &&
                      !host.textContent.includes('Obsolete logarithmic range'),
                      'A cancelled logarithmic refinement cannot restore old bounds or leave a stale preview');
            } finally {
                WorksheetRequests.evaluationTransport.fetchEvaluation = fetchBefore;
            }
            const zoomed = curve();
            host.closest<HTMLElement>('.result-card')!.querySelector<HTMLElement>('[data-zoom-reset]')!.click();
            await waitForCurve(zoomed);
            await window.checkLabVisibleCurveHover(curve());
            check(zoom.value === '0' && zoom.min === '-3' && zoom.max === '6' &&
                  Number(curve().dataset.xMin) === baseline[0] && Number(curve().dataset.xMax) === baseline[1],
                  'Reset restores the full original logarithmic curve and default zoom track');
            const frame = labSurfacePreview.prepare(curve())!.framePlan!;
            for (const x of [0, 1]) for (const y of [0, 1]) for (const z of [0, 1]) {
                const point = labDOM.call('lab_surface_project', frame, [x, y, z]) as MarsSurfacePoint2;
                check(point[0] >= 0 && point[0] <= frame.width && point[1] >= 0 && point[1] <= frame.height,
                      'At 100% the complete logarithmic curve bounding box fits inside its card');
            }
            await checkCurveZoomRollback();
        } finally {
            // Programmatic evaluation does not perform the Evaluate button's plot reset.
            // Remove this fixture's authored ranges, exact-value metadata and Argand viewport.
            WorksheetRequests.labRequests.cancel('plot');
            labSurfaceVolume.cancel();
            labSurfacePreview.dispose();
            labDOM.call('lab_plot_controls_reset');
            WorksheetBindings.controller.setExpressionEditor('exp(i*x)');
            await WorksheetRequests.evaluation.evaluateExpression();
            await waitForAutomaticPlot();
            for (const id of ['plotLower', 'plotUpper', 'plotSecondLower', 'plotSecondUpper', 'plotYLower', 'plotYUpper']) {
                const input = document.getElementById(id)! as LabTestPlotInput;
                check(input.value === '' && input.marsZoomText === undefined && input.marsZoomValue === undefined,
                      'Logarithmic zoom fixtures do not leak ranges or exact-coordinate metadata into subsequent plots');
            }
            check(!(host as LabTestPlotHost).marsComplexPlaneBounds,
                  'Logarithmic zoom fixtures do not leak their Argand viewport into subsequent plots');
        }
    }

    /** @brief Failed and superseded curve previews restore accepted samples without changing sibling controls. */
    async function checkCurveZoomRollback(): Promise<void> {
        const host = document.getElementById('plotRendered')! as LabTestPlotHost;
        const chart = host.querySelector<SVGSVGElement>('[data-complex-panel="svg"] [data-surface]')!;
        const view = labSurfacePreview.prepare(chart)!;
        const zoom = document.getElementById('plotZoom')! as HTMLInputElement;
        const original = labSurfacePreview.snapshot()!;
        function changeZoom(): void {
            zoom.value = String(original.volume.exponent + 1);
            zoom.dispatchEvent(new Event('input', {bubbles: true}));
            zoom.dispatchEvent(new Event('change', {bubbles: true}));
        }
        async function waitForRollback(): Promise<void> {
            const deadline = performance.now() + 15000;
            while ((view.volume.dirty || WorksheetRequests.labRequests.busy() || chart.dataset.localRenderer !== 'svg') &&
                   performance.now() < deadline)
                await LabTestBrowser.wait(25);
            check(chart.isConnected && !view.volume.dirty && !WorksheetRequests.labRequests.busy() &&
                  chart.dataset.localRenderer === 'svg' && view.volume.exponent === original.volume.sampleExponent &&
                  view.camera.scale === 1 && view.volume.bounds.every((value, index) => value === view.volume.sampled[index]) &&
                  !view.framePlan!.texts.some(text => text.text.includes('Preview')),
                  'An abandoned curve preview returns to the accepted samples, slider value and settled drawing');
            await window.checkLabVisibleCurveHover(chart);
        }
        const fetchBefore = WorksheetRequests.evaluationTransport.fetchEvaluation;
        let requests = 0;
        try {
            WorksheetRequests.evaluationTransport.fetchEvaluation = async function (...args) {
                if (args[2] !== 'plot') return fetchBefore(...args);
                ++requests;
                const data = {ok: false, error: 'Deliberate curve refinement failure'};
                return {response: Object.assign(new Response(null, {status: 422}), {labData: async () => data}), data};
            };
            changeZoom();
            await waitForRollback();
            check(requests === 1 && document.getElementById('plotFeedback')!.textContent.includes('Deliberate curve refinement failure'),
                  'Failed native refinement keeps its diagnostic without repeatedly retrying or leaving a refining notice');
            for (const [id, key] of [['plotLower', 'exactXMin'], ['plotUpper', 'exactXMax']])
                check((document.getElementById(id)! as LabTestPlotInput).marsZoomValue === chart.dataset[key],
                      'Failure restores the curve\'s exact accepted input endpoints');
        } finally {
            WorksheetRequests.evaluationTransport.fetchEvaluation = fetchBefore;
        }

        // Change plots before the curve's debounce fires, exercising the real shared-request cancellation path.
        changeZoom();
        const plane = host.querySelector<SVGSVGElement>('[data-complex-panel="complex_plane"] [data-plot-chart]')!;
        const planeBox = plane.getBoundingClientRect();
        plane.dispatchEvent(new WheelEvent('wheel', {ctrlKey: true, bubbles: true, cancelable: true, deltaY: -100,
            clientX: planeBox.left + planeBox.width / 2, clientY: planeBox.top + planeBox.height / 2}));
        const deadline = performance.now() + 25000;
        while ((plane.isConnected || host.marsPlotZoom?.busy || host.marsPlotZoom?.wheel || WorksheetRequests.labRequests.busy()) &&
               performance.now() < deadline)
            await LabTestBrowser.wait(25);
        check(!plane.isConnected && !host.marsPlotZoom?.busy && !host.marsPlotZoom?.wheel,
              'The sibling plot completes its own wheel zoom after interrupting curve refinement');
        await waitForRollback();

        // A newer authored range owns shared controls even when cancelling a dirty curve.
        changeZoom();
        const lower = document.getElementById('plotLower')! as LabTestPlotInput;
        const upper = document.getElementById('plotUpper')! as LabTestPlotInput;
        lower.value = '7';
        upper.value = '9';
        lower.marsZoomValue = lower.marsZoomText = '7';
        upper.marsZoomValue = upper.marsZoomText = '9';
        labSurfaceVolume.cancel(true);
        await waitForRollback();
        check(lower.value === '7' && upper.value === '9' && lower.marsZoomValue === '7' && upper.marsZoomValue === '9',
              'Cancellation restores the curve without overwriting newer shared range controls');
    }

    async function checkEvaluatePlotReset() {
        const host = (document.getElementById('plotRendered')! as LabTestPlotHost);
        const limits = ['plotLower', 'plotUpper', 'plotSecondLower', 'plotSecondUpper', 'plotYLower', 'plotYUpper'];
        const camera = ['plotAzimuth', 'plotElevation', 'plotRoll', 'plotZoom'];
        const before = host.querySelector<SVGSVGElement>('[data-plot-chart]')!;
        limits.forEach((id, i) => {
            const control = (document.getElementById(id)! as LabTestPlotInput);
            control.value = String(i % 2 ? 8 : 5);
            control.marsZoomValue = control.value;
            control.marsZoomText = control.value;
        });
        camera.forEach(id => { (document.getElementById(id)! as LabTestPlotInput).value = '2'; });
        (document.getElementById('plotScale')! as HTMLInputElement).value = 'manual';
        host.marsComplexPlaneBounds = ['5', '8', '5', '8'];
        host.querySelector<HTMLElement>('[data-complex-panel="amplitude"] button')!.click();
        check(host.querySelector<HTMLElement>('.complex-expanded')!, 'A single projection is expanded before Evaluate');
        (document.getElementById('run')! as HTMLButtonElement).click();
        const deadline = performance.now() + 20000;
        while ((!host.querySelector<SVGSVGElement>('[data-plot-chart]')! || host.querySelector<SVGSVGElement>('[data-plot-chart]')! === before) &&
               performance.now() < deadline)
            await LabTestBrowser.wait(25);
        await waitForAutomaticPlot();
        check(limits.every(id => (document.getElementById(id)! as LabTestPlotInput).value === '' &&
                                (document.getElementById(id)! as LabTestPlotInput).marsZoomValue === undefined) &&
              camera.every(id => (document.getElementById(id)! as LabTestPlotInput).value === (document.getElementById(id)! as LabTestPlotInput).defaultValue) &&
              (document.getElementById('plotScale')! as HTMLInputElement).value === 'auto' && !host.marsComplexPlaneBounds,
              'Evaluate resets all plot bounds, cached zoom, camera controls and Argand viewport for unchanged input');
        check((document.getElementById('plotInputRange')! as HTMLInputElement).value === 'auto',
              'Evaluate restores automatic input ranges');
        const panels = [...host.querySelectorAll<HTMLElement>('.complex-panel')];
        check(panels.length === 4 && !host.marsComplexExpanded &&
              !host.querySelector<HTMLElement>('.complex-expanded, .complex-focused')! &&
              panels.every(panel => getComputedStyle(panel).display !== 'none' &&
                  panel.querySelector('button')!.getAttribute('aria-expanded') === 'false'),
              'Evaluate restores all four visible plots and clears single-plot expansion for unchanged input');
        const amplitude = host.querySelector<HTMLElement>('[data-complex-panel="amplitude"] [data-plot-chart]')!;
        check(amplitude && Number(amplitude.dataset.yMin) <= 1 && Number(amplitude.dataset.yMax) >= 1 &&
              amplitude.querySelector<SVGPathElement>('[data-series] path')!, 'Evaluate restores the visible unit-amplitude curve');
    }

    async function checkComplexAutoZoom() {
        const host = (document.getElementById('plotRendered')! as LabTestPlotHost);
        for (const projection of ['components', 'amplitude']) {
            const siblings = [...host.querySelectorAll<HTMLElement>('.complex-panel')]
                .filter(panel => panel.dataset.complexPanel !== projection)
                .map(panel => panel.querySelector<SVGSVGElement>('[data-plot-chart], [data-surface]')!);
            const chart = () => host.querySelector<SVGSVGElement>(`[data-complex-panel="${projection}"] [data-plot-chart]`)!;
            const initial = chart();
            initial.closest<HTMLElement>('.complex-panel')!.querySelector('button')!.click();
            await LabTestBrowser.nextFrame();
            const matrix = initial.getScreenCTM()!;
            const x = matrix.e + (Number(initial.dataset.plotLeft) + Number(initial.dataset.plotWidth) * 0.75) * matrix.a;
            const y = matrix.f + (Number(initial.dataset.plotTop) + Number(initial.dataset.plotHeight) * 0.9) * matrix.d;
            const span = Number(initial.dataset.xMax) - Number(initial.dataset.xMin);
            for (let i = 0; i < 10; ++i)
                initial.dispatchEvent(new WheelEvent('wheel', {
                    clientX: x, clientY: y, deltaY: -100, ctrlKey: true, bubbles: true, cancelable: true
                }));
            const deadline = performance.now() + 20000;
            while ((chart() === initial || host.marsPlotDrag.busy || host.marsPlotDrag.wheel) && performance.now() < deadline)
                await LabTestBrowser.wait(25);
            check(chart() !== initial && (document.getElementById('plotScale')! as HTMLInputElement).value !== 'manual' &&
                  Number(chart().dataset.xMax) - Number(chart().dataset.xMin) < span / 2 &&
                  chart().querySelector<SVGPathElement>('[data-series] path')!,
                  projection + ' repeated off-curve zoom keeps automatic height and visible samples');
            check(siblings.every(chart => chart.isConnected),
                  projection + ' zoom leaves every sibling chart intact');
            if (projection === 'amplitude')
                check(Number(chart().dataset.yMin) <= 1 && Number(chart().dataset.yMax) >= 1,
                      'Automatic amplitude zoom retains the unit-magnitude line');
            chart().closest<HTMLElement>('.complex-panel')!.querySelector('button')!.click();
            await WorksheetRequests.evaluation.evaluateExpression();
            await waitForAutomaticPlot();
        }
    }

    async function checkComplexComponentsPan(projection = 'components') {
        const host = (document.getElementById('plotRendered')! as LabTestPlotHost);
        const chart = () => host.querySelector<SVGSVGElement>(`[data-complex-panel="${projection}"] [data-plot-chart]`)!;
        const plane = projection === 'complex_plane';
        const controls = ['plotLower', 'plotUpper', 'plotYLower', 'plotYUpper', 'plotScale']
            .map(id => document.getElementById(id)! as HTMLInputElement);
        const saved = controls.map(control => control.value);
        for (const pointerType of ['mouse', 'touch']) {
            const initial = chart(), panel = initial.closest<HTMLElement>('.complex-panel')!;
            const siblings = [...host.querySelectorAll<HTMLElement>('.complex-panel')]
                .filter(other => other !== panel)
                .map(other => other.querySelector<SVGSVGElement>('[data-plot-chart], [data-surface]')!);
            if (plane) {
                const body = panel.querySelector<HTMLElement>('.rendered')!.getBoundingClientRect();
                const frame = panel.querySelector<HTMLElement>('.rendered-zoom-frame')!.getBoundingClientRect();
                check(initial.viewBox.baseVal.width === initial.viewBox.baseVal.height,
                      'Complex plane uses a square native viewport');
                check(Math.abs(Number(initial.dataset.plotLeft) + Number(initial.dataset.plotWidth) / 2 -
                    initial.viewBox.baseVal.width / 2) < 0.002,
                    'The equal-unit plotting rectangle is horizontally centred');
                check(Math.abs((body.top + body.bottom) - (frame.top + frame.bottom)) < 2,
                      'Complex plane is vertically centred in the available card body');
                check(Math.abs(panel.getBoundingClientRect().bottom - body.bottom) < 2,
                      'The complex-plane card ends with its chart rather than stretching to the camera card');
            }
            panel.querySelector('button')!.click();
            await LabTestBrowser.paint();
            const figure = panel.querySelector<HTMLElement>('.rendered-zoom-frame > svg')!;
            const rem = parseFloat(getComputedStyle(document.documentElement).fontSize);
            check(figure.getBoundingClientRect().height <= Math.max(12 * rem, innerHeight - 18 * rem) + 1,
                  projection + ' fits the viewport height when expanded');
            if (plane) {
                check(['ℜ', 'ℑ'].every(label => [...initial.querySelectorAll('text')].some(text => text.textContent === label)),
                      'Complex-plane axes use Gothic real and imaginary symbols');
                const imaginary = [...initial.querySelectorAll('text')].find(text => text.textContent === 'ℑ')!;
                check(Number(imaginary.getAttribute('x')) < Number(initial.dataset.plotLeft) - 24 &&
                      Math.abs(Number(imaginary.getAttribute('y')) - (Number(initial.dataset.plotTop) +
                          Number(initial.dataset.plotHeight) / 2 + 4)) < 0.002,
                      'The imaginary-axis label is centred to the left of the y-axis ticks');
            } else if (projection === 'components') {
                const real = initial.querySelector<SVGPathElement>('[data-series="0"] path')!;
                const imaginary = initial.querySelector<SVGPathElement>('[data-series="1"] path')!;
                check(getComputedStyle(real).stroke === 'rgb(214, 170, 85)' &&
                      getComputedStyle(imaginary).stroke === 'rgb(120, 216, 203)' &&
                      [...panel.querySelectorAll('svg')].every(svg => getComputedStyle(svg).filter === 'none'),
                      'Real and imaginary curves retain distinct native colours without an SVG filter');
                const legend = initial.querySelector<HTMLElement>('[data-plot-legend]')!;
                check(legend?.querySelector('rect')! && legend.textContent.includes('Real') &&
                      legend.textContent.includes('Imaginary') && legend.querySelectorAll('path').length === 2,
                      'Components have one boxed legend with coloured line samples');
            } else {
                const paths = [...initial.querySelectorAll<LabTestHoverPath>('[data-series] > path')];
                const expected = Number(initial.dataset.plotTop) + Number(initial.dataset.plotHeight) *
                    (Number(initial.dataset.yMax) - 1) / (Number(initial.dataset.yMax) - Number(initial.dataset.yMin));
                check(paths.length > 0 && paths.every(path => {
                    const values = path.getAttribute('d')!.split(' ');
                    return Math.abs(Number(values[2]) - expected) < 0.001 &&
                           Math.abs(Number(values[5]) - expected) < 0.001;
                }), 'Amplitude of exp(i*x) is one flat line, without stray curve fragments');
            }
            check([...initial.querySelectorAll<LabTestHoverPath>('[data-series] > path')].every(path => {
                const style = getComputedStyle(path);
                return style.vectorEffect === 'non-scaling-stroke' && parseFloat(style.strokeWidth) === 1.25;
            }), projection + ' uses thin screen-space strokes when expanded');
            check([...panel.querySelectorAll('svg')].every(svg => getComputedStyle(svg).filter === 'none'),
                  projection + ' uses unfiltered SVG at every nesting level');
            check([...initial.querySelectorAll<SVGTextElement>('[data-plot-grid], [data-plot-tick]')]
                .every(element => getComputedStyle(element).fill === 'none'),
                  'Grid and tick paths cannot inherit text fill');
            const matrix = initial.getScreenCTM()!;
            const pw = Number(initial.dataset.plotWidth) * matrix.a;
            const ph = Number(initial.dataset.plotHeight) * matrix.d;
            const x = matrix.e + Number(initial.dataset.plotLeft) * matrix.a + pw / 2;
            const y = matrix.f + Number(initial.dataset.plotTop) * matrix.d + ph / 2;
            const x0 = Number(initial.dataset.xMin), y0 = Number(initial.dataset.yMin);
            const spanX = Number(initial.dataset.xMax) - x0, spanY = Number(initial.dataset.yMax) - y0;
            const send = (target: EventTarget, type: string, dx: number, dy: number) => target.dispatchEvent(new PointerEvent(type, {
                pointerId: 81, pointerType, isPrimary: true, button: 0, buttons: type === 'pointerup' ? 0 : 1,
                clientX: x + dx, clientY: y + dy, bubbles: true, cancelable: true
            }));
            check(!send(initial, 'pointerdown', 0, 0), projection + ' captures a ' + pointerType + ' drag');
            checkFlatPlotCursor(panel, true);
            const paths = [...initial.querySelectorAll<LabTestHoverPath>('[data-series] > path')];
            const hoverCache = paths.map(path => path.marsHoverSegment);
            send(initial, 'pointermove', 35, 20);
            check(paths.every((path, i) => path.marsHoverSegment === hoverCache[i]) &&
                  [...host.querySelectorAll<HTMLElement>('.plot-pointer-tip, .plot-pointer-marker')]
                      .every(element => element.classList.contains('hidden')),
                  projection + ' skips segment inspection and hover overlays during a real chart-targeted drag');
            check(initial.querySelector<HTMLElement>('[data-series]')!.getAttribute('transform')?.startsWith('translate('),
                  'Components respond locally before the native resampling reply');
            const ticks = [...initial.querySelectorAll<SVGGElement>('[data-plot-pan-axis]')];
            check(ticks.length > 0 && ticks.every(tick => {
                const vertical = tick.dataset.plotPanAxis === '1';
                const shift = vertical ? 20 / matrix.d : 35 / matrix.a;
                const transform = tick.transform.baseVal.consolidate()!.matrix;
                return Math.abs(transform.e - (vertical ? 0 : shift)) < 0.001 &&
                       Math.abs(transform.f - (vertical ? shift : 0)) < 0.001;
            }), projection + ' ticks, labels, grids and zero guides track the curve during panning');
            await LabTestBrowser.wait(200);
            check(chart() === initial && !host.marsPlotDrag.busy,
                  projection + ' retains full-detail SVG without asynchronous replacements during dragging');
            send(window, 'pointermove', 40, 25);
            send(window, 'pointermove', 35, 20);
            send(window, 'pointerup', 35, 20);
            const deadline = performance.now() + 20000;
            while ((host.marsPlotDrag.busy || chart() === initial) && performance.now() < deadline)
                await LabTestBrowser.wait(25);
            check(chart() !== initial && !host.marsPlotDrag.busy &&
                  Math.abs(Number(chart().dataset.xMin) - (x0 - 35 * spanX / pw)) < 1e-8 &&
                  Math.abs(Number(chart().dataset.yMin) - (y0 + 20 * spanY / ph)) < 1e-8,
                  projection + ' retains the completed ' + pointerType + ' pan');
            check(siblings.every(chart => chart.isConnected),
                  projection + ' pan leaves every sibling chart intact');
            checkFlatPlotCursor(chart().closest<HTMLElement>('.complex-panel')!);
            if (plane) {
                check(controls.every((control, i) => control.value === saved[i]),
                      'Argand panning does not change sampling or component bounds');
                const after = chart();
                const xScale = Number(after.dataset.plotWidth) / (Number(after.dataset.xMax) - Number(after.dataset.xMin));
                const yScale = Number(after.dataset.plotHeight) / (Number(after.dataset.yMax) - Number(after.dataset.yMin));
                check(Math.abs(xScale / yScale - 1) < 0.0001, 'Argand panning preserves circular geometry');
                document.querySelector<HTMLElement>('#plotCard [data-zoom-reset]')!.click();
                const resetDeadline = performance.now() + 20000;
                while (chart() === after && performance.now() < resetDeadline)
                    await LabTestBrowser.wait(25);
                check(chart() !== after && Math.abs(Number(chart().dataset.xMin) - x0) < 1e-8 &&
                      Math.abs(Number(chart().dataset.yMin) - y0) < 1e-8,
                      'Reset restores the Argand viewport independently');
            }
            chart().closest<HTMLElement>('.complex-panel')!.querySelector('button')!.click();
        }
        controls.forEach((control, i) => { control.value = saved[i]; });
        await WorksheetRequests.evaluation.evaluateExpression();
        await waitForAutomaticPlot();
    }

    async function checkAsynchronousPlot() {
        const original = WorksheetRequests.evaluationTransport.fetchEvaluation;
        let release: ((value: WorksheetRequests.MarsApiPostResult) => void) | undefined, started = false;
        try {
            WorksheetRequests.evaluationTransport.fetchEvaluation = (...args) => {
                if (args[2] !== 'plot')
                    return original(...args);
                started = true;
                return new Promise(resolve => release = resolve);
            };
            WorksheetBindings.controller.setExpressionEditor('sin(x)');
            await WorksheetRequests.evaluation.evaluateExpression();
            for (let i = 0; i < 100 && !started; ++i)
                await LabTestBrowser.wait(10);
            check(started && !WorksheetRequests.labRequests.busy(), 'Evaluate must finish before the plot response');
            check(WorksheetContext.rendered.querySelector('svg')! && WorksheetContext.parsed.textContent && WorksheetContext.functionStyle.textContent,
                  'Mathematical cards appear while the plot is pending');
            const mathematics = WorksheetContext.rendered.innerHTML;
            release!({response: Object.assign(new Response(), {labData: async () => ({ok: false, error: 'Plot fixture unavailable'})}),
                      data: {ok: false, error: 'Plot fixture unavailable'}});
            await waitForAutomaticPlot();
            check((document.getElementById('plotRendered')! as LabTestPlotHost).textContent.includes('Plot fixture unavailable') &&
                  WorksheetContext.rendered.innerHTML === mathematics && !WorksheetContext.rendered.classList.contains('error'),
                  'A plot failure must not replace successful mathematics');
            started = false;
            await WorksheetRequests.evaluation.evaluateExpression();
            for (let i = 0; i < 100 && !started; ++i)
                await LabTestBrowser.wait(10);
            check(started, 'A fresh evaluation starts a fresh plot');
            WorksheetWorkspace.presentation.saveCurrentModeResultState();
            WorksheetWorkspace.mode.setMode('matrix');
            WorksheetWorkspace.presentation.syncModeUI();
            const previous = (document.getElementById('plotRendered')! as LabTestPlotHost).innerHTML;
            release!({response: Object.assign(new Response(), {labData: async () => ({ok: false, error: 'Obsolete plot fixture'})}),
                      data: {ok: false, error: 'Obsolete plot fixture'}});
            await LabTestBrowser.wait(50);
            check((document.getElementById('plotRendered')! as LabTestPlotHost).innerHTML === previous,
                  'A late plot must not publish after switching modes');
            WorksheetWorkspace.mode.setMode('expression');
            WorksheetWorkspace.presentation.syncModeUI();
            WorksheetWorkspace.presentation.restoreModeResultState();
            check((document.getElementById('plotRendered')! as LabTestPlotHost).textContent.includes('Plot cancelled'),
                  'Restoring cancelled background work must not leave a permanent loading message');
        } finally {
            if (release)
                release!({response: Object.assign(new Response(null, {status: 500}), {labData: async () => ({ok: false})}),
                          data: {ok: false}});
            WorksheetRequests.evaluationTransport.fetchEvaluation = original;
            WorksheetWorkspace.mode.setMode('expression');
            WorksheetWorkspace.presentation.syncModeUI();
        }
    }
    async function waitForAutomaticPlot() {
        if (document.getElementById('plotCard')!.classList.contains('hidden')) return;
        const node = (document.getElementById('plotRendered')! as LabTestPlotHost);
        for (let i = 0; i < 1200 && (!node.textContent || node.textContent.includes('Generating plot')); ++i)
            await LabTestBrowser.wait(25);
        check(!node.textContent.includes('Generating plot'), 'Automatic plot must finish');
    }
    function checkPlotLabelSpacing() {
        const rendered = (document.getElementById('plotRendered')! as LabTestPlotHost);
        const boxes = [...rendered.querySelectorAll<SVGGraphicsElement>('[data-plot-label]')].map(label => label.getBBox());
        check(boxes.length >= 3, 'Plot must retain readable axis numbers');
        for (let i = 0; i < boxes.length; ++i) {
            for (let j = 0; j < i; ++j) {
                const a = boxes[i], b = boxes[j];
                check(a.x + a.width + 2 <= b.x || b.x + b.width + 2 <= a.x ||
                      a.y + a.height + 2 <= b.y || b.y + b.height + 2 <= a.y,
                      'Rendered axis numbers must have clearance, including at shared corners');
            }
        }
    }
    async function checkPlotChoices() {
        const rendered = (document.getElementById('plotRendered')! as LabTestPlotHost);
        check(!document.querySelector<HTMLSelectElement>('#plotRange select, #plotRange .select-shell')!,
              'Plot settings must not contain dropdowns');
        for (const id of ['plotKind', 'plotScale', 'plotInputRange']) {
            const control = (document.getElementById(id)! as LabTestPlotInput), saved = control.value;
            const group = document.getElementById(id + 'Choices')!;
            const radios = [...group.querySelectorAll<HTMLInputElement>('input[type="radio"]')];
            if (!group.getClientRects().length)
                continue;
            check(control.type === 'hidden' && radios.length === (id === 'plotInputRange' ? 2 : 3),
                  'Plot choices expose Auto alongside their overrides');
            try {
                radios.find(radio => radio.value === 'auto')!.click();
                check(radios.every(radio => radio.checked === (radio.value === 'auto')),
                      'Automatic is the default selection where exposed');
                for (const radio of radios) {
                    if (radio.closest<HTMLElement>('label')!.classList.contains('hidden'))
                        continue;
                    radio.click();
                    check(control.value === radio.value && radio.checked &&
                          radios.filter(option => option.checked).length === 1,
                          'Radio overrides must be mutually exclusive and reach native payload controls');
                    if (id === 'plotScale') {
                        check(document.getElementById('plotManual')!.classList.contains('hidden') ===
                              (radio.value !== 'manual'), 'Manual limits should appear only when selected');
                        if (radio.value === 'auto')
                            check(['plotYLower', 'plotYUpper']
                                      .every(id => (document.getElementById(id)! as LabTestPlotInput).value === ''),
                                  'Vertical Auto clears only vertical limits');
                    }
                    if (id === 'plotInputRange') {
                        const multiple = document.querySelectorAll('#plotFirstChoices input').length > 1;
                        const hidden = radio.value === 'auto' && !multiple;
                        check((document.getElementById('plotFirstGroup')!.getClientRects().length === 0) === hidden,
                              'Input Auto hides the single-variable range panel but retains multiple-variable choices');
                        check([...document.querySelectorAll<HTMLElement>('.plot-ranges .plot-bound-pair')].every(pair =>
                                  pair.classList.contains('hidden') === (radio.value === 'auto')),
                              'Input Auto hides both variable bound pairs independently of vertical scale');
                        if (radio.value === 'auto')
                            check(['plotLower', 'plotUpper', 'plotSecondLower', 'plotSecondUpper'].every(id => {
                                const input = document.getElementById(id)! as LabTestPlotInput;
                                return input.value === '' && input.marsZoomValue === undefined;
                            }), 'Input Auto clears authored and exact cached variable bounds');
                    }
                }
                await LabTestBrowser.paint();
                if (id === 'plotInputRange') {
                    const selector = document.getElementById('plotInputRangeGroup')!.getBoundingClientRect();
                    const bounds = document.getElementById('plotFirstGroup')!.getBoundingClientRect();
                    const ranges = document.getElementById('plotRange')!.getBoundingClientRect();
                    const rem = parseFloat(getComputedStyle(document.documentElement).fontSize);
                    if (Math.abs(selector.top - bounds.top) < 1) {
                        check(selector.right < bounds.left,
                              'Input range selector and bounds do not overlap on a shared row');
                        check(selector.width <= 11 * rem + 1 && bounds.width > selector.width,
                              'The compact input range selector leaves more space for bounds');
                    } else {
                        check(bounds.top >= selector.bottom,
                              'Input range selector and bounds stack in narrow plot cards');
                    }
                    check(bounds.left >= ranges.left && bounds.right <= ranges.right + 1,
                          'Range fields stay inside the plot card');
                }
                const card = group.closest<HTMLElement>('.result-card')!.getBoundingClientRect();
                for (const label of group.querySelectorAll('label')) {
                    if (label.classList.contains('hidden'))
                        continue;
                    const box = label.getBoundingClientRect();
                    check(box.left >= card.left && box.right <= card.right,
                          'Radio options must stay inside normal, expanded and narrow plot cards');
                }
                radios.find(radio => radio.value === 'auto')!.click();
                check(control.value === 'auto' && radios.every(radio => radio.checked === (radio.value === 'auto')),
                      'Auto restores automatic selection without a Reset button');
                const override = radios.find(radio => radio.value !== 'auto' &&
                                             !radio.closest<HTMLElement>('label')!.classList.contains('hidden'))!;
                override.click();
                check(control.value === override.value && override.matches(':checked') &&
                      !Object.hasOwn(override, 'checked'),
                      'An override must be selectable again after Auto using the real browser checked state');
            } finally {
                control.value = saved;
                control.dispatchEvent(new Event('change', {bubbles: true}));
            }
        }
    }
    async function checkLivePlotControls() {
        const rendered = (document.getElementById('plotRendered')! as LabTestPlotHost);
        check(!document.getElementById('plotUpdate')! && !document.querySelector<HTMLElement>('#plotRange [data-plot-reset]')!,
              'Live plot controls have no Update or Reset buttons');
        const lower = (document.getElementById('plotLower')! as HTMLInputElement), upper = (document.getElementById('plotUpper')! as HTMLInputElement);
        const saved = [lower.value, upper.value];
        const savedScale = (document.getElementById('plotScale')! as HTMLInputElement).value;
        document.querySelector<HTMLInputElement>('#plotScaleChoices input[value="all"]')!.click();
        // Finish setup redraws before retaining the SVG that an invalid edit must preserve.
        await WorksheetRequests.plots.takePlot((document.getElementById('plotFirst')! as HTMLInputElement).value);
        async function waitFor(predicate: () => boolean) {
            const deadline = performance.now() + 15000;
            while (!predicate() && performance.now() < deadline)
                await LabTestBrowser.wait(25);
            check(predicate(), 'Live range edit must complete without an Update button');
        }
        const before = rendered.querySelector<HTMLElement>('[data-series]')!;
        check(before && !rendered.classList.contains('error'), 'Live range checks start from a valid completed plot');
        lower.value = '-';
        lower.dispatchEvent(new Event('input', {bubbles: true}));
        await waitFor(() => !document.getElementById('plotFeedback')!.classList.contains('hidden'));
        check(rendered.querySelector<HTMLElement>('[data-series]')! === before && !rendered.classList.contains('error'),
              'Incomplete live bounds retain the last valid plot');
        lower.value = '-6';
        upper.value = '6';
        lower.dispatchEvent(new Event('input', {bubbles: true}));
        upper.dispatchEvent(new Event('input', {bubbles: true}));
        await waitFor(() => rendered.querySelector<HTMLElement>('[data-series]')! !== before);
        check(document.getElementById('plotFeedback')!.classList.contains('hidden'),
              'Correcting the bounds clears live validation feedback');
        check((document.getElementById('plotInputRange')! as HTMLInputElement).value === 'manual',
              'Authored input bounds select Manual without changing the vertical policy');
        document.querySelector<HTMLInputElement>('#plotScaleChoices input[value="auto"]')!.click();
        check(lower.value === '-6' && upper.value === '6',
              'Vertical Auto preserves independently authored input bounds');
        const exactLower = lower as LabTestPlotInput;
        exactLower.marsZoomValue = '-6.0000001';
        exactLower.marsZoomText = '-6';
        document.querySelector<HTMLInputElement>('#plotInputRangeChoices input[value="auto"]')!.click();
        check(['plotLower', 'plotUpper', 'plotSecondLower', 'plotSecondUpper'].every(id => {
            const input = document.getElementById(id)! as LabTestPlotInput;
            return input.value === '' && input.marsZoomValue === undefined && input.marsZoomText === undefined;
        }) && (document.getElementById('plotInputRange')! as HTMLInputElement).value === 'auto',
              'Input Auto clears authored and exact gesture bounds before the next payload');
        lower.value = saved[0];
        upper.value = saved[1];
        (document.getElementById('plotScale')! as HTMLInputElement).value = savedScale;
        await WorksheetRequests.plots.takePlot((document.getElementById('plotFirst')! as HTMLInputElement).value);
    }
    function checkPlotRangeAlignment() {
        const rendered = (document.getElementById('plotRendered')! as LabTestPlotHost);
        const groups = [...document.querySelectorAll<HTMLElement>('.plot-ranges > .plot-group')]
            .filter(group => group.querySelector<HTMLElement>('.plot-bound-pair')?.getClientRects().length);
        const bounds = groups.map(group => group.getBoundingClientRect());
        for (const selector of ['.plot-bound-pair label', '.plot-bound-pair input']) {
            const rows = groups.map(group => [...group.querySelectorAll(selector)]
                .map(node => node.getBoundingClientRect()));
            for (const row of rows)
                check(row.length === 2 && Math.abs(row[0].top - row[1].top) < 1,
                      'Each range must keep its From and To labels and inputs aligned');
            if (bounds.length === 2 && Math.abs(bounds[0].top - bounds[1].top) < 1)
                check(Math.abs(rows[0][0].top - rows[1][0].top) < 1,
                      'Side-by-side variable ranges must align despite different hint and choice heights');
            else if (bounds.length === 2)
                check(bounds[1].top >= bounds[0].bottom,
                      'Narrow cards must stack variable ranges without overlap');
        }
    }
    function checkPlotControls(surface: boolean) {
        const rendered = (document.getElementById('plotRendered')! as LabTestPlotHost);
        checkPlotRangeAlignment();
        const visible = (id: string) => (document.getElementById(id)! as LabTestPlotInput).getClientRects().length > 0;
        check(visible('plotCamera') === surface && visible('plotVertical') &&
              document.getElementById('plotVerticalTitle')!.textContent === (surface ? 'Height range' : 'Vertical range'),
              'Vertical limits must remain available for curves and surfaces with a clear dimension label');
        const first = [...document.querySelectorAll<HTMLInputElement>('#plotFirstChoices input')];
        const second = [...document.querySelectorAll<HTMLInputElement>('#plotSecondChoices input')];
        check(first.length > 0 && first.every(radio => radio.type === 'radio'), 'Variables must be radio choices');
        check((document.getElementById('plotSecond')! as HTMLInputElement).type === 'hidden', 'No wide free-text variable input');
        check(second.length === first.length + 1 && second[0].value === '',
              'Second-axis choices include Auto and the native variables');
        for (const pair of document.querySelectorAll<HTMLElement>('.plot-bound-pair')) {
            if (!pair.getClientRects().length)
                continue;
            const fields = [...pair.querySelectorAll<HTMLElement>('.plot-field')].map(field => field.getBoundingClientRect());
            check(fields.length === 2 && Math.abs(fields[0].top - fields[1].top) < 2,
                  'Each From/To or Minimum/Maximum pair must stay together on one row');
        }
        if (surface) {
            const x = first.find(radio => radio.value === 'x'), y = first.find(radio => radio.value === 'y');
            check(x?.checked && y, 'The plotted first axis should be selected');
            second.find(radio => radio.value === 'y')!.click();
            y.click();
            check((document.getElementById('plotFirst')! as HTMLInputElement).value === 'y' &&
                  (document.getElementById('plotSecond')! as HTMLInputElement).value === '' &&
                  second.find(radio => radio.value === 'y')!.disabled,
                  'Changing the first axis clears a duplicate second axis and disables that radio');
            const previous = labDOM.bindEvents({});
            let selected;
            labDOM.bindEvents({...previous, schedulePlotControls: state => {
                labDOM.services(labDOM.call('lab_plot_controls_dispatch', 2, null, state),
                                {takePlot: variable => selected = variable});
            }});
            try {
                y.dispatchEvent(new Event('change', {bubbles: true}));
                check(selected === 'y', 'Live plotting must use the selected first-axis choice');
            } finally {
                labDOM.bindEvents(previous);
                x.click();
            }
        }
    }
    async function waitForVolume() {
        const deadline = performance.now() + 25000;
        while ((labSurfacePreview.snapshot()!?.volume.dirty || WorksheetRequests.labRequests.busy()) && performance.now() < deadline)
            await LabTestBrowser.wait(25);
        check(!labSurfacePreview.snapshot()!?.volume.dirty && !WorksheetRequests.labRequests.busy(),
              'Native volume refinement must finish without losing the interactive chart');
    }
    /** @brief Compare orientations while treating opposite quaternion signs as equivalent. */
    function quaternionAgreement(left: MarsSurfaceQuaternion, right: MarsSurfaceQuaternion): number {
        let product = 0;
        for (let coordinate = 0; coordinate < 4; ++coordinate)
            product += left[coordinate] * right[coordinate];
        return Math.abs(product);
    }

    async function checkPlotDrag() {
        const rendered = (document.getElementById('plotRendered')! as LabTestPlotHost);
        const controls = ['plotAzimuth', 'plotElevation', 'plotRoll'].map(id => document.getElementById(id)! as HTMLInputElement);
        const retained = rendered.querySelector<SVGSVGElement>('[data-surface]')!;
        const generation = WorksheetRequests.labRequests.latestMain();
        const settle = LabTestBrowser.paint;
        const pointer = (target: EventTarget, type: string, x: number, y: number, extras: PointerEventInit = {}) => target.dispatchEvent(new PointerEvent(type, {
            pointerId: 91, pointerType: 'touch', isPrimary: true, button: 0,
            buttons: type === 'pointerup' ? 0 : 1, clientX: x, clientY: y,
            bubbles: true, cancelable: true, ...extras
        }));
        const box = retained.getBoundingClientRect(), cx = box.left + box.width / 2, cy = box.top + box.height / 2;
        const quaternion = () => labSurfacePreview.snapshot()!.orientation;
        const initial = quaternion();
        check(!pointer(retained, 'pointerdown', cx, cy), 'Surface drag reserves touch movement');
        pointer(window, 'pointermove', cx + 80, cy - 60);
        await settle();
        check(quaternion().some((v, i) => Math.abs(v - initial[i]) > 0.01),
              'Finger drag changes the quaternion camera locally');
        pointer(window, 'pointerup', cx + 80, cy - 60);
        await settle();
        check(!(rendered as LabTestPlotHost).marsPlotDrag.busy && !(rendered as LabTestPlotHost).marsPlotDrag.active &&
              retained.isConnected && retained.dataset.localRenderer && WorksheetRequests.labRequests.latestMain() === generation,
              'Release keeps WebGL and the same mesh without a native request');
        const beforeCancel = quaternion();
        pointer(retained, 'pointerdown', cx, cy);
        pointer(window, 'pointermove', cx - 80, cy + 60);
        await settle();
        document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape', bubbles: true, cancelable: true}));
        await settle();
        check(quaternion().every((v, i) => Math.abs(v - beforeCancel[i]) < 1e-10),
              'Escape restores the exact quaternion, not rounded Euler readouts');
        // Repeated drags can cross either pole; no elevation clamp is applied to orientation.
        for (let i = 0; i < 8; ++i) {
            pointer(retained, 'pointerdown', cx, cy);
            pointer(window, 'pointermove', cx, cy + box.height * 0.6);
            await settle();
            pointer(window, 'pointerup', cx, cy + box.height * 0.6);
            await settle();
            check(Math.abs(Math.hypot(...quaternion()) - 1) < 1e-12,
                  'Repeated trackball rotations retain a normalised quaternion');
        }
        const identityCamera: MarsSurfaceQuaternion = [0, 0, 0, 1];
        const rotationRows = labSurfaceCamera.rows(identityCamera);
        check(rotationRows.length === 3, 'Camera rotation returns three rows');
        for (let row = 0; row < 3; ++row) {
            check(rotationRows[row].length === 3, 'Each camera rotation row has three coordinates');
            for (let column = 0; column < 3; ++column) {
                const expected = row === column ? 1 : 0;
                check(Math.abs(rotationRows[row][column] - expected) < 1e-12,
                      'Identity camera returns the identity rotation matrix');
            }
        }
        const composedCamera = labSurfaceCamera.multiply(identityCamera, identityCamera);
        for (let coordinate = 0; coordinate < 4; ++coordinate) {
            check(Math.abs(composedCamera[coordinate] - identityCamera[coordinate]) < 1e-12,
                  'Camera multiplication preserves the identity orientation');
        }
        for (const elevation of [-90, -89.999, 0, 89.999, 90]) {
            const q = labSurfaceCamera.fromAngles([37, elevation, 123]);
            const decoded = labSurfaceCamera.angles(q, [37, elevation, 123]);
            const restored = labSurfaceCamera.fromAngles(decoded);
            check(quaternionAgreement(q, restored) > 1 - 1e-9,
                  'Euler readouts reconstruct orientation even at either pole');
            const moved = labSurfaceCamera.drag(q, [cx, cy], [cx + 20, cy + 10], box, false);
            check(moved.every(Number.isFinite) && quaternionAgreement(q, moved) < 0.99999,
                  'Trackball remains responsive at the Euler singularities');
        }
        const exported = new DOMParser().parseFromString(WorksheetResults.clipboard.copyTextForTarget('plot'), 'image/svg+xml');
        check(!exported.querySelector('parsererror')! && exported.querySelectorAll('path').length > 1000 &&
              !exported.querySelector('canvas')! && exported.querySelector('text')!,
              'Copy exports current mesh and axes as standalone SVG');
        const exportedBefore = WorksheetResults.clipboard.copyTextForTarget('plot');
        retained.dispatchEvent(new WheelEvent('wheel', {deltaY: -100, altKey: true, bubbles: true, cancelable: true}));
        await LabTestBrowser.wait(450);
        check(retained.isConnected && retained.dataset.localRenderer && WorksheetRequests.labRequests.latestMain() === generation &&
              exportedBefore !== WorksheetResults.clipboard.copyTextForTarget('plot'),
              'Wheel rotation stays local and SVG export follows the latest camera');
        const chart = rendered.querySelector<SVGSVGElement>('[data-surface]')!;
        const angles: MarsSurfaceAngles = [
            Number(chart.dataset.azimuth), Number(chart.dataset.elevation), Number(chart.dataset.roll)
        ];
        const frame = chart.closest<HTMLElement>('.rendered-zoom-frame')!, figure = frame.querySelector('svg')!;
        const savedFigure = figure.style.cssText, savedFrame = frame.style.cssText;
        try {
            // Exercise a phone-sized fitted SVG inside a scrolled, padded card.
            figure.style.width = '280px';
            figure.style.height = '240px';
            frame.style.width = '280px';
            frame.style.height = '240px';
            labSurfacePreview.clear();
            const before = [rendered.scrollWidth, rendered.scrollHeight, rendered.scrollLeft, rendered.scrollTop];
            check(labSurfacePreview.draw(chart, angles), 'Phone-sized surface starts local rendering');
            const overlay = rendered.querySelector<SVGSVGElement>('[data-surface-overlay]')!;
            const expected = chart.getBoundingClientRect(), actual = overlay.getBoundingClientRect();
            check(overlay.parentElement === rendered && !chart.querySelector<HTMLElement>('foreignObject')! &&
                  (['left', 'top', 'width', 'height'] as const).every(key => Math.abs(expected[key] - actual[key]) < 1),
                  'Interactive canvases align with the fitted SVG instead of jumping sideways');
            check([rendered.scrollWidth, rendered.scrollHeight, rendered.scrollLeft, rendered.scrollTop]
                  .every((value, i) => Math.abs(value - before[i]) <= 1),
                  'Showing the mobile renderer must not enlarge or move the scrollable plot');
        } finally {
            labSurfacePreview.clear();
            figure.style.cssText = savedFigure;
            frame.style.cssText = savedFrame;
        }
        check(labSurfacePreview.draw(chart, angles), 'Local surface renderer accepts native geometry');
        const selected = chart.dataset.localRenderer;
        sessionStorage.setItem('labSurfaceRenderer', selected!);
        const probe = document.createElement('canvas').getContext('webgl')!;
        if (probe) {
            check(selected === 'webgl', 'Available WebGL must use the GPU renderer rather than silently failing shaders');
            probe.getExtension('WEBGL_lose_context')?.loseContext();
        }
        if (selected === 'webgl') {
            const gl = rendered.querySelector<HTMLCanvasElement>('[data-surface-overlay] canvas')!.getContext('webgl')!;
            check(!gl.isContextLost() && gl.getError() === gl.NO_ERROR, 'GPU surface draw has no WebGL errors');
            const program = gl.getParameter(gl.CURRENT_PROGRAM);
            const camera = gl.getUniform(program, gl.getUniformLocation(program, 'camera')!) as Float32Array;
            const keys = ['xMin', 'xMax', 'yMin', 'yMax', 'zMin', 'zMax'];
            const bounds = keys.map(key => Number(chart.dataset[key]));
            for (let axis = 0; axis < 3; axis++) {
                const centre = (bounds[axis * 2] + bounds[axis * 2 + 1]) / 2;
                bounds[axis * 2] = centre + (bounds[axis * 2] - centre) / 2;
                bounds[axis * 2 + 1] = centre + (bounds[axis * 2 + 1] - centre) / 2;
            }
            check(labSurfacePreview.draw(chart, angles, bounds), 'WebGL magnifies the retained scene for zoom');
            const zoomCamera = gl.getUniform(program, gl.getUniformLocation(program, 'camera')!) as Float32Array;
            const scaling = gl.getUniform(program, gl.getUniformLocation(program, 'domainScale')!) as Float32Array;
            check([0, 1, 4, 5, 8, 9, 12, 13].every(i => camera[i] === zoomCamera[i]) &&
                  [...scaling].every(value => Math.abs(value - 2) < 1e-6),
                  'Volume zoom doubles the mesh transform while retaining the fixed axis projection');
        }
        labSurfacePreview.dispose();
        const getContext = HTMLCanvasElement.prototype.getContext;
        try {
            HTMLCanvasElement.prototype.getContext = function unavailableContext() { return null; } as typeof getContext;
            check(labSurfacePreview.prepare(chart) === null &&
                  !rendered.querySelector('[data-surface-overlay]') && !chart.dataset.localRenderer,
                  'Unavailable canvas contexts reject preparation without attaching an overlay or claiming renderer ownership');
            HTMLCanvasElement.prototype.getContext = function(this: HTMLCanvasElement, type: string, ...args: unknown[]) {
                return type === 'webgl' ? null : getContext.call(this, type, ...args);
            } as typeof getContext;
            check(labSurfacePreview.draw(chart, angles) && chart.dataset.localRenderer === 'canvas',
                  'Unavailable WebGL uses a local canvas, not server sampling');
            const canvas = rendered.querySelector<HTMLCanvasElement>('[data-surface-overlay] canvas')!;
            const pixels = canvas.getContext('2d')!.getImageData(0, 0, canvas.width, canvas.height).data;
            let painted = false;
            for (let i = 3; i < pixels.length; i += 64) painted ||= pixels[i] !== 0;
            check(painted, 'Canvas fallback actually paints the retained triangles');
            const softwareView = labSurfacePreview.prepare(chart)!;
            labSurfacePreview.clear(true);
            const nativeLayers: SVGElement[] = [];
            const originalVisibility: string[] = [];
            for (const child of chart.children) {
                if (child instanceof SVGElement && (child.tagName === 'g' || child.tagName === 'path')) {
                    nativeLayers.push(child);
                    originalVisibility.push(child.style.visibility);
                }
            }
            check(nativeLayers.length >= 2, 'Visibility fixture retains multiple native SVG layers');
            try {
                nativeLayers[0].style.visibility = 'visible';
                nativeLayers[1].style.visibility = 'collapse';
                const expectedVisibility = nativeLayers.map(function visibility(node) { return node.style.visibility; });
                for (let cycle = 0; cycle < 2; ++cycle) {
                    check(labSurfacePreview.draw(chart, angles) && labSurfacePreview.draw(chart, angles),
                          'Repeated redraws preserve the retained surface');
                    for (const node of nativeLayers)
                        check(node.style.visibility === 'hidden', 'Interactive renderer hides each native base layer');
                    labSurfacePreview.clear(true);
                    for (let index = 0; index < nativeLayers.length; ++index)
                        check(nativeLayers[index].style.visibility === expectedVisibility[index],
                              'Forced clear restores original visibility after repeated redraws and reuse');
                    check(softwareView.overlay.style.display === 'none' && !chart.dataset.localRenderer,
                          'Forced clear hides the overlay and clears renderer ownership');
                }
            } finally {
                labSurfacePreview.clear(true);
                for (let index = 0; index < nativeLayers.length; ++index)
                    nativeLayers[index].style.visibility = originalVisibility[index];
            }
            check(labSurfacePreview.draw(chart, angles) && labSurfacePreview.prepare(chart) === softwareView,
                  'Redrawing after forced clear reuses the retained view');
            const softwarePlan = softwareView.polygons!();
            check(softwarePlan === softwareView.polygons!(), 'One frame reuses its native drawing plan');
            const vectorLayer = labSurfaceExport.layer(softwareView)!;
            const vectorPaths = vectorLayer.querySelectorAll('[data-series] path');
            check(vectorPaths.length === softwarePlan.length && softwareView.polygons!() === softwarePlan,
                  'SVG export reuses the Canvas plan and preserves its primitive count');
            for (let i = 0; i < vectorPaths.length; ++i) {
                const polygon = softwarePlan[i];
                check(vectorPaths[i].getAttribute('fill') === polygon.colour,
                      'SVG and Canvas use identical native painter order');
            }
            check(labSurfacePreview.draw(chart, angles), 'Redrawing a software frame succeeds');
            check(softwareView.polygons!() !== softwarePlan,
                  'A new frame invalidates the previous native drawing plan');
            function captureFailure(): never {
                check(!softwareView.overlay.isConnected && !chart.dataset.localRenderer,
                      'Snapshot capture temporarily removes the local renderer');
                throw new Error('Snapshot fixture failure');
            }
            let captureError = '';
            try {
                labSurfacePreview.captureResult(captureFailure);
            } catch (error) {
                if (error instanceof Error) captureError = error.message;
            }
            check(captureError === 'Snapshot fixture failure' && softwareView.overlay.isConnected &&
                  chart.dataset.localRenderer === 'canvas' && labSurfacePreview.prepare(chart) === softwareView,
                  'Failed snapshot capture restores the same retained view and propagates the error');
            function captureEmpty(): null { return null; }
            check(labSurfacePreview.captureResult(captureEmpty) === null && softwareView.overlay.isConnected,
                  'Empty snapshot capture restores the overlay without inventing result state');
            const capturedState: WorksheetResultSnapshot = {
                lastTex: '', lastDerivativeExpression: '', currentVariables: [],
                currentDifferentiable: false, resultInputBindings: []
            };
            function captureState(): WorksheetResultSnapshot { return capturedState; }
            check(labSurfacePreview.captureResult(captureState) === capturedState &&
                  capturedState.surfaceView?.key === labSurfacePreview.snapshot()!.key &&
                  softwareView.overlay.isConnected,
                  'Successful snapshot capture annotates the original result and restores its overlay');
        } finally {
            HTMLCanvasElement.prototype.getContext = getContext;
            labSurfacePreview.dispose();
        }
        check(!rendered.querySelector<SVGSVGElement>('[data-surface-overlay]')! && !chart.dataset.localRenderer,
              'Local-renderer disposal restores SVG interaction and releases its overlay');
        labSurfacePreview.draw(chart, angles);
        (document.getElementById('plotElevation')! as HTMLInputElement).value = '-80';
        (document.getElementById('plotElevation')! as HTMLInputElement).dispatchEvent(new Event('input', {bubbles: true}));
        await settle();
        (document.getElementById('plotElevation')! as HTMLInputElement).dispatchEvent(new Event('change', {bubbles: true}));
        await settle();
        const below = labSurfacePreview.snapshot()!.orientation;
        await WorksheetRequests.plots.takePlot((document.getElementById('plotFirst')! as HTMLInputElement).value);
        check(labSurfacePreview.snapshot()!.orientation.every((v, i) => Math.abs(v - below[i]) < 1e-10),
              'Native geometry refresh preserves a camera below the former elevation limit');
        const refreshed = rendered.querySelector<SVGSVGElement>('[data-surface]')!;
        const context = rendered.querySelector<HTMLCanvasElement>('[data-surface-overlay] canvas')!.getContext('webgl')!;
        const loss = context?.getExtension('WEBGL_lose_context');
        if (loss) {
            const beforeLoss = labSurfacePreview.snapshot()!;
            loss.loseContext();
            const deadline = performance.now() + 3000;
            while (refreshed.dataset.localRenderer !== 'canvas' && performance.now() < deadline)
                await LabTestBrowser.wait(25);
            check(refreshed.dataset.localRenderer === 'canvas' &&
                  labSurfacePreview.snapshot()!.orientation.every((v, i) => Math.abs(v - beforeLoss.orientation[i]) < 1e-10),
                  'Context loss switches to canvas without losing the quaternion camera');
        }
    }
    async function checkSurfacePinch() {
        const rendered = (document.getElementById('plotRendered')! as LabTestPlotHost);
        const chart = rendered.querySelector<SVGSVGElement>('[data-surface]')!, box = chart.getBoundingClientRect();
        const cx = box.left + box.width / 2, cy = box.top + box.height / 2;
        const send = (target: EventTarget, type: string, id: number, x: number) => target.dispatchEvent(new PointerEvent(type,
            {pointerId: id, pointerType: 'touch', isPrimary: id === 71, button: 0,
             buttons: type === 'pointerup' ? 0 : 1, clientX: x, clientY: cy, bubbles: true, cancelable: true}));
        async function settle() {
            for (let i = 0; i < 2; ++i) await LabTestBrowser.nextFrame();
        }
        const generation = WorksheetRequests.labRequests.latestMain();
        for (const cancelled of [false, true]) {
            const initial = labSurfacePreview.snapshot()!.camera;
            send(chart, 'pointerdown', 71, cx - 30);
            send(chart, 'pointerdown', 72, cx + 30);
            send(window, 'pointermove', 71, cx - 60);
            send(window, 'pointermove', 72, cx + 60);
            await settle();
            check(Math.abs(labSurfacePreview.snapshot()!.camera.scale - initial.scale * 2) < 1e-10,
                  'Two-finger spread magnifies the retained camera continuously');
            send(window, cancelled ? 'pointercancel' : 'pointerup', 72, cx + 60);
            await settle();
            check(chart.isConnected && WorksheetRequests.labRequests.latestMain() === generation && !(rendered as LabTestPlotHost).marsPlotDrag.active &&
                  Math.abs(labSurfacePreview.snapshot()!.camera.scale - initial.scale * (cancelled ? 1 : 2)) < 1e-10,
                  'Pinch release keeps the camera; cancellation restores it without native resampling');
            send(window, 'pointerup', 71, cx - 60);
        }
        rendered.closest<HTMLElement>('.result-card')!.querySelector<HTMLElement>('[data-zoom-reset]')!.click();
        await settle();
        check(labSurfacePreview.snapshot()!.camera.scale === 1, 'Reset restores the volume after pinch');
        await waitForVolume();
    }

    async function checkPlotPinch() {
        const rendered = (document.getElementById('plotRendered')! as LabTestPlotHost);
        const chart = () => rendered.querySelector<SVGSVGElement>('[data-plot-chart][data-x-min]')!;
        const surface = chart().hasAttribute('data-surface');
        if (surface) {
            await runBrowserGroup('checkSurfacePinch', checkSurfacePinch);
            return;
        }
        const coordinateKeys = surface ? ['xMin', 'xMax', 'yMin', 'yMax', 'zMin', 'zMax'] :
                                         ['xMin', 'xMax', 'yMin', 'yMax'];
        const original = coordinateKeys.map(key => Number(chart().dataset[key]));
        const card = rendered.closest<HTMLElement>('.result-card')!;
        async function wait(previous: SVGSVGElement) {
            const deadline = performance.now() + 20000;
            while (((rendered as LabTestPlotHost).marsPlotDrag.busy || chart() === previous) && performance.now() < deadline)
                await LabTestBrowser.wait(25);
            check(!(rendered as LabTestPlotHost).marsPlotDrag.busy && chart() !== previous, 'Pinch finishes with a native redraw');
            await LabTestBrowser.paint();
        }
        for (const factor of [0.5, 2]) {
            const before = chart(), matrix = before.getScreenCTM()!, view = before.viewBox.baseVal;
            const camera = ['azimuth', 'elevation', 'roll'].map(key => before.dataset[key]);
            const heightPolicy = (document.getElementById('plotScale')! as HTMLInputElement).value;
            const cx = Math.round(matrix.e + (surface ? view.width / 2 :
                Number(before.dataset.plotLeft) + Number(before.dataset.plotWidth) * 0.4) * matrix.a);
            const cy = Math.round(matrix.f + (surface ? view.height / 2 :
                Number(before.dataset.plotTop) + Number(before.dataset.plotHeight) * 0.55) * matrix.d);
            const send = (target: EventTarget, type: string, id: number, x: number) => target.dispatchEvent(new PointerEvent(type,
                {pointerId: id, pointerType: 'touch', isPrimary: id === 71, button: 0,
                 buttons: type === 'pointerup' ? 0 : 1, clientX: x, clientY: cy,
                 bubbles: true, cancelable: true}));
            check(!send(before, 'pointerdown', 71, cx - 30), 'First pinch finger starts the plot gesture');
            check(!send(before, 'pointerdown', 72, cx + 30) && (rendered as LabTestPlotHost).marsPlotDrag.pinch,
                  'Second finger promotes dragging to pinch zoom');
            const geometry = [...(rendered as LabTestPlotHost).marsPlotDrag.pinchGeometry];
            send(before, 'pointerdown', 73, cx);
            send(window, 'pointermove', 73, cx + 90);
            check(!(rendered as LabTestPlotHost).marsPlotDrag.changed, 'A third finger does not alter pinch zoom');
            send(window, 'pointermove', 71, cx - 30 / factor);
            send(window, 'pointermove', 72, cx + 30 / factor);
            if (surface) {
                const generation = WorksheetRequests.labRequests.latestMain();
                await LabTestBrowser.paint();
                check(chart() === before && !!before.dataset.localRenderer && WorksheetRequests.labRequests.latestMain() === generation,
                      'Held surface pinch projects retained geometry without a native sampling request');
            }
            send(window, 'pointerup', 72, cx + 30 / factor);
            await wait(before);
            const ranges = surface ? [['xMin', 'xMax'], ['yMin', 'yMax'], ['zMin', 'zMax']] :
                                     [['xMin', 'xMax'], ['yMin', 'yMax']];
            for (const [lo, hi] of ranges)
                check(Math.abs((Number(chart().dataset[hi]) - Number(chart().dataset[lo])) /
                      (Number(before.dataset[hi]) - Number(before.dataset[lo])) - factor) < 1e-7,
                      'Pinch changes axis ranges rather than image dimensions');
            for (const [axis, lo, hi] of [[0, 'xMin', 'xMax'], [1, 'yMin', 'yMax']] as const) {
                const fraction = geometry[4 + axis];
                const anchor = Number(before.dataset[lo]) * (1 - fraction) + Number(before.dataset[hi]) * fraction;
                const after = Number(chart().dataset[lo]) * (1 - fraction) + Number(chart().dataset[hi]) * fraction;
                check(Math.abs(after - anchor) < 1e-7, 'Pinch preserves the midpoint anchor');
            }
            if (surface)
                check(['azimuth', 'elevation', 'roll'].every((key, i) => chart().dataset[key] === camera[i]) &&
                      (document.getElementById('plotScale')! as HTMLInputElement).value === 'manual',
                      'Surface pinch preserves the camera and locks the scaled height viewport');
            check(!(rendered as LabTestPlotHost).marsPlotDrag.active && !(rendered as LabTestPlotHost).marsPlotDrag.pinch,
                  'Releasing either finger ends pinch and restores full-detail rendering');
            send(window, 'pointermove', 71, cx + 100);
            check(!(rendered as LabTestPlotHost).marsPlotDrag.active, 'The remaining finger cannot jump back into dragging');
            send(window, 'pointerup', 71, cx + 100);
            const zoomed = chart();
            card.querySelector<HTMLElement>('[data-zoom-reset]')!.click();
            await wait(zoomed);
            check(coordinateKeys.every((key, i) =>
                Math.abs(Number(chart().dataset[key]) - original[i]) < 1e-7), 'Reset restores the pre-pinch viewport');
            check((document.getElementById('plotScale')! as HTMLInputElement).value === heightPolicy,
                  'Reset restores the pre-pinch height policy');
            const restored = chart();
            send(restored, 'pointerdown', 71, cx - 30);
            send(restored, 'pointerdown', 72, cx + 30);
            send(window, 'pointermove', 72, cx + 60);
            send(window, 'pointercancel', 72, cx + 60);
            await wait(restored);
            check(['xMin', 'xMax', 'yMin', 'yMax'].every((key, i) =>
                Math.abs(Number(chart().dataset[key]) - original[i]) < 1e-7),
                'Cancelling the second finger restores the pre-gesture ranges');
            send(window, 'pointerup', 71, cx - 30);
        }
    }
    async function checkCurvePointerView() {
        const rendered = (document.getElementById('plotRendered')! as LabTestPlotHost);
        const settle = LabTestBrowser.paint;
        await settle();
        const card = rendered.closest<HTMLElement>('.result-card')!;
        const chart = () => rendered.querySelector<SVGSVGElement>('[data-plot-chart][data-x-min]:not([data-surface])')!;
        const before = chart(), width = rendered.querySelector('svg')!.getBoundingClientRect().width;
        const matrix = before.getScreenCTM()!;
        const left = matrix.e + Number(before.dataset.plotLeft) * matrix.a;
        const top = matrix.f + Number(before.dataset.plotTop) * matrix.d;
        const plotWidth = Number(before.dataset.plotWidth) * matrix.a;
        const plotHeight = Number(before.dataset.plotHeight) * matrix.d;
        const clientX = left + plotWidth * 0.25, clientY = top + plotHeight * 0.6;
        const x0 = Number(before.dataset.xMin), x1 = Number(before.dataset.xMax);
        const y0 = Number(before.dataset.yMin), y1 = Number(before.dataset.yMax);
        const anchor = x0 * 0.75 + x1 * 0.25;
        rendered.dispatchEvent(new PointerEvent('pointermove', {clientX, clientY, bubbles: true}));
        const tip = rendered.querySelector<HTMLElement>('.plot-pointer-tip')!, line = rendered.querySelector<HTMLElement>('.plot-pointer-line')!;
        const hoverX = Number(before.dataset.plotLeft) + Number(before.dataset.plotWidth) * 0.25;
        const crossings: number[] = [];
        for (const path of before.querySelectorAll('[data-series] > path')) {
            const p = path.getAttribute('d')!.split(' ');
            const ax = Number(p[1]), ay = Number(p[2]), bx = Number(p[4]), by = Number(p[5]);
            if (p.length !== 6 || ax === bx || hoverX < Math.min(ax, bx) || hoverX > Math.max(ax, bx))
                continue;
            const height = ay + (hoverX - ax) * (by - ay) / (bx - ax);
            crossings.push(y1 + (height - Number(before.dataset.plotTop)) / Number(before.dataset.plotHeight) * (y0 - y1));
        }
        const reportedY = Number(tip.textContent.split('\n')[1]?.replace('y ≈ ', ''));
        const horizontal = rendered.querySelector<HTMLElement>('.plot-pointer-horizontal')!;
        check(horizontal && !horizontal.classList.contains('hidden') &&
              Math.abs(horizontal.getBoundingClientRect().top - (top + (y1 - reportedY) / (y1 - y0) * plotHeight)) < 2 &&
              Math.abs(horizontal.getBoundingClientRect().width - plotWidth) < 2 &&
              getComputedStyle(horizontal).pointerEvents === 'none',
              'Horizontal hover guide spans the chart at the reported curve value');
        const marker = rendered.querySelector<HTMLElement>('.plot-pointer-marker')!;
        const markerBox = marker?.getBoundingClientRect();
        check(marker && !marker.classList.contains('hidden') &&
              Math.abs(markerBox.left + markerBox.width / 2 - clientX) < 2 &&
              Math.abs(markerBox.top + markerBox.height / 2 - horizontal.getBoundingClientRect().top) < 2 &&
              getComputedStyle(marker).pointerEvents === 'none',
              'Curve marker is centred at the crosshair intersection without intercepting pointer events');
        check(tip && line && !tip.classList.contains('hidden') &&
              Math.abs(Number(tip.textContent.split('\n')[0].replace('x ≈ ', '')) - anchor) < 1e-8 &&
              crossings.some(value => Math.abs(value - reportedY) < 1e-8) &&
              Math.abs(line.getBoundingClientRect().left - clientX) < 2,
              'Hover guide shows x and a plotted curve value rather than the pointer height: ' +
              JSON.stringify({text: tip?.textContent, crossings, anchor}));
        if (crossings.length === 1) {
            rendered.dispatchEvent(new PointerEvent('pointermove',
                {clientX, clientY: top + plotHeight * 0.15, bubbles: true}));
            check(Math.abs(Number(tip.textContent.split('\n')[1].replace('y ≈ ', '')) - reportedY) < 1e-8,
                  'Moving vertically leaves the single curve value unchanged');
        }
        check(getComputedStyle(line).pointerEvents === 'none' && getComputedStyle(tip).pointerEvents === 'none',
              'The hover guide must not intercept curve tooltips or gestures');
        // Check actual rendered tick anchors, independently of the range-to-screen calculation above.
        const bottom = Number(before.dataset.plotTop) + Number(before.dataset.plotHeight);
        const ticks = [...before.querySelectorAll('text')].filter(tick =>
            tick.getAttribute('text-anchor') === 'middle' && Number(tick.getAttribute('y')) > bottom &&
            Number.isFinite(Number(tick.textContent)));
        check(ticks.length > 1, 'The curve has visible numerical horizontal ticks');
        for (const tick of ticks) {
            const position = new DOMPoint(Number(tick.getAttribute('x')), Number(tick.getAttribute('y')))
                .matrixTransform(tick.getScreenCTM()!);
            if (position.x <= left + 1 || position.x >= left + plotWidth - 1)
                continue;
            rendered.dispatchEvent(new PointerEvent('pointermove',
                {clientX: position.x, clientY, bubbles: true}));
            check(Math.abs(Number(tip.textContent.split('\n')[0].replace('x ≈ ', '')) - Number(tick.textContent)) < 0.002 * (x1 - x0),
                  'Hover coordinates agree with the rendered tick ' + tick.textContent + ': ' + tip.textContent);
        }
        rendered.dispatchEvent(new PointerEvent('pointerleave'));
        check(tip.classList.contains('hidden') && line.classList.contains('hidden') &&
              horizontal.classList.contains('hidden') && marker.classList.contains('hidden'),
              'Leaving hides both hover guides and the curve marker');
        async function waitChart(previous: SVGSVGElement) {
            const deadline = performance.now() + 15000;
            while ((!chart() || chart() === previous) && performance.now() < deadline)
                await LabTestBrowser.wait(25);
            check(chart() && chart() !== previous, 'Viewport zoom requests a newly sampled plot');
            await settle();
        }
        const zoomEvent = new WheelEvent('wheel',
              {clientX, clientY, deltaY: -100, ctrlKey: true, bubbles: true, cancelable: true});
        const wheelFraction = (zoomEvent.clientX - left) / plotWidth;
        const wheelAnchor = x0 * (1 - wheelFraction) + x1 * wheelFraction;
        check(!before.dispatchEvent(zoomEvent),
              'Ctrl+wheel viewport zoom prevents page scrolling');
        await waitChart(before);
        const after = chart();
        check((document.getElementById('plotLower')! as HTMLInputElement).value.length < 10 &&
              (document.getElementById('plotYLower')! as HTMLInputElement).value.length < 10,
              'Ordinary zoom bounds must use short interval-aware labels');
        check(Math.abs(Number(after.dataset.xMin) - (wheelAnchor + (x0 - wheelAnchor) * 0.8)) < 1e-8 &&
              Math.abs(Number(after.dataset.xMax) - Number(after.dataset.xMin) - (x1 - x0) * 0.8) < 1e-8 &&
              Math.abs(Number(after.dataset.yMax) - Number(after.dataset.yMin) - (y1 - y0) * 0.8) < 1e-8,
              'Wheel zoom shrinks both axis ranges around the pointer: ' +
              JSON.stringify({x0, x1, y0, y1, wheelAnchor, bounds: {...after.dataset}}));
        const plotStyle = getComputedStyle(rendered);
        const availableWidth = rendered.clientWidth - parseFloat(plotStyle.paddingLeft) - parseFloat(plotStyle.paddingRight);
        check(Math.abs(rendered.querySelector('svg')!.getBoundingClientRect().width - availableWidth) < 2,
              'Viewport zoom must not enlarge the plot image: ' +
              JSON.stringify({before: width, after: rendered.querySelector('svg')!.getBoundingClientRect().width,
                              card: card.getBoundingClientRect().width, rendered: rendered.clientWidth}));
        card.querySelector<HTMLElement>('[data-zoom-step="1"]')!.click();
        await waitChart(after);
        const centred = chart();
        check(Math.abs(Number(centred.dataset.xMax) - Number(centred.dataset.xMin) - (x1 - x0) * 0.64) < 1e-8,
              'The plus button zooms the viewport rather than the result card');
        card.querySelector<HTMLElement>('[data-zoom-reset]')!.click();
        await waitChart(centred);
        check(Math.abs(Number(chart().dataset.xMin) - x0) < 1e-8 &&
              Math.abs(Number(chart().dataset.xMax) - x1) < 1e-8,
              'Reset restores the original pre-zoom view');
        for (const pointerType of ['mouse', 'touch']) {
            const initial = chart(), transform = initial.getScreenCTM()!;
            const pw = Number(initial.dataset.plotWidth) * transform.a;
            const ph = Number(initial.dataset.plotHeight) * transform.d;
            const cx = Math.round(transform.e + Number(initial.dataset.plotLeft) * transform.a + pw / 2);
            const cy = Math.round(transform.f + Number(initial.dataset.plotTop) * transform.d + ph / 2);
            const event = (target: EventTarget, type: string, x: number, y: number, id = 41) => target.dispatchEvent(new PointerEvent(type,
                {clientX: x, clientY: y, pointerId: id, pointerType, isPrimary: id === 41,
                 button: 0, buttons: type === 'pointerup' ? 0 : 1, bubbles: true, cancelable: true}));
            check(!event(initial, 'pointerdown', cx, cy), 'A primary pointer starts curve panning');
            event(window, 'pointermove', cx + 40, cy + 25, 42);
            check(!(rendered as LabTestPlotHost).marsPlotDrag.changed, 'A second pointer cannot move the curve');
            event(window, 'pointermove', cx + 40, cy + 25);
            check(initial.querySelector<HTMLElement>('[data-series]')!.getAttribute('transform')?.startsWith('translate(') &&
                  initial.querySelector<HTMLElement>('[data-series]')!.closest<HTMLElement>('[data-plot-window]')!,
                  'Curves follow the pointer immediately inside a clipped viewport, before any server reply');
            event(window, 'pointerup', cx + 40, cy + 25);
            const deadline = performance.now() + 20000;
            while (((rendered as LabTestPlotHost).marsPlotDrag.busy || chart() === initial) && performance.now() < deadline)
                await LabTestBrowser.wait(25);
            check(!(rendered as LabTestPlotHost).marsPlotDrag.busy && !(rendered as LabTestPlotHost).marsPlotDrag.active &&
                  Math.abs(Number(chart().dataset.xMin) - (x0 - 40 * (x1 - x0) / pw)) < 1e-8 &&
                  Math.abs(Number(chart().dataset.yMin) - (y0 + 25 * (y1 - y0) / ph)) < 1e-8 &&
                  Math.abs(Number(chart().dataset.xMax) - Number(chart().dataset.xMin) - (x1 - x0)) < 1e-8,
                  'Dragging pans both ranges without changing scale for ' + pointerType);
            const panned = chart();
            card.querySelector<HTMLElement>('[data-zoom-reset]')!.click();
            await waitChart(panned);
            check(Math.abs(Number(chart().dataset.xMin) - x0) < 1e-8 &&
                  Math.abs(Number(chart().dataset.yMin) - y0) < 1e-8, 'Reset restores the pre-pan view');
        }
        const cancelled = chart();
        const cancelMatrix = cancelled.getScreenCTM()!;
        const cx = Math.round(cancelMatrix.e + (Number(cancelled.dataset.plotLeft) +
                              Number(cancelled.dataset.plotWidth) / 2) * cancelMatrix.a);
        const cy = Math.round(cancelMatrix.f + (Number(cancelled.dataset.plotTop) +
                              Number(cancelled.dataset.plotHeight) / 2) * cancelMatrix.d);
        const pointer = {pointerId: 51, pointerType: 'mouse', isPrimary: true, button: 0, buttons: 1,
                         bubbles: true, cancelable: true};
        cancelled.dispatchEvent(new PointerEvent('pointerdown', {...pointer, clientX: cx, clientY: cy}));
        window.dispatchEvent(new PointerEvent('pointermove', {...pointer, clientX: cx - 30, clientY: cy - 20}));
        document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape', bubbles: true, cancelable: true}));
        const cancelDeadline = performance.now() + 20000;
        while ((rendered as LabTestPlotHost).marsPlotDrag.busy && performance.now() < cancelDeadline)
            await LabTestBrowser.wait(25);
        check(!(rendered as LabTestPlotHost).marsPlotDrag.busy && !(rendered as LabTestPlotHost).marsPlotDrag.active &&
              Math.abs(Number(chart().dataset.xMin) - x0) < 1e-8 &&
              Math.abs(Number(chart().dataset.yMin) - y0) < 1e-8,
              'Escape restores both original ranges even with a pan redraw in flight');
        await runBrowserGroup('checkPlotPinch', checkPlotPinch);
    }
    async function checkPlotWheelZoom() {
        const rendered = (document.getElementById('plotRendered')! as LabTestPlotHost);
        const settle = LabTestBrowser.paint;
        await settle();
        if (rendered.querySelector<SVGSVGElement>('[data-plot-chart][data-x-min]:not([data-surface])')!) {
            await runBrowserGroup('checkCurvePointerView', checkCurvePointerView);
            return;
        }
        const card = rendered.closest<HTMLElement>('.result-card')!;
        const chart = () => rendered.querySelector<SVGSVGElement>('[data-surface]')!;
        const controls = ['plotLower', 'plotUpper', 'plotSecondLower', 'plotSecondUpper',
                          'plotYLower', 'plotYUpper', 'plotScale'];
        const original = controls.map(id => (document.getElementById(id)! as LabTestPlotInput).value);
        const before = chart();
        const view = labSurfacePreview.prepare(before)!;
        const initial = labSurfacePreview.snapshot()!;
        const frame = view.framePlan!;
        // Native geometry units describe the sampled chart, not a retained cropped display volume.
        // Read both sides from that chart; preview framing is checked separately below.
        const sampledUnits = before.dataset.units!.split(' ').map(Number);
        const sampledSpans = ['x', 'y', 'z'].map(axis =>
            Number(before.dataset[axis + 'Max']) - Number(before.dataset[axis + 'Min']));
        const unitScale = sampledUnits[0] / sampledSpans[0];
        check(sampledUnits.length === 3 && sampledSpans.every(span => Number.isFinite(span) && span > 0) &&
              sampledUnits.every((v, i) => Math.abs(v / sampledSpans[i] - unitScale) < 1e-10),
              'Native sampled surfaces preserve equal coordinate units, including unequal height ranges: ' +
              JSON.stringify({units: sampledUnits, sampledSpans, displayBounds: initial.volume.bounds}));
        const projected: number[] = [];
        for (let row = 0; row < 2; ++row) {
            let span = 0;
            for (let column = 0; column < 3; ++column)
                span += Math.abs(frame.rows[row * 3 + column]);
            projected.push(span * frame.fit);
        }
        const margin = Math.min(Math.max(Number(initial.volume.labelWidth) + 25, 45), (frame.width - 80)/2);
        check(projected[0] <= frame.width - 2*margin + 1e-7 &&
              projected[1] <= frame.height - 100 + 1e-7 &&
              (Math.abs(projected[0] - (frame.width - 2*margin)) < 1e-7 ||
               Math.abs(projected[1] - (frame.height - 100)) < 1e-7),
              'The projected volume fills an available dimension without stretching or clipping');
        const lowControl = (document.getElementById('plotLower')! as HTMLInputElement), highControl = (document.getElementById('plotUpper')! as HTMLInputElement);
        const lowBefore = lowControl.value, highBefore = highControl.value;
        lowControl.value = 'pi-1'; highControl.value = 'pi+1';
        const symbolicView = {chart: before, units: [...view.units]} as
            LabTestSurfaceVolumeFixture;
        labDOM.call('lab_surface_volume_init', symbolicView, null, 0);
        lowControl.value = lowBefore; highControl.value = highBefore;
        check(symbolicView.volume.exact[0] === 'pi-1' && symbolicView.volume.exact[1] === 'pi+1',
              'C retains authored symbolic bounds rather than rebasing them through decimal metadata');
        if (before.dataset.mesh === 'false' && original[6] === 'auto') {
            const autoView = {...view, camera: {...view.camera}};
            labDOM.call('lab_surface_volume_init', autoView, null, 0);
            labDOM.call('lab_surface_zoom', autoView, 1, 2, null, 0);
            const autoFrame = labDOM.call<MarsSurfaceFrame>('lab_surface_frame', autoView, null);
            check(autoView.volume.autoHeight && (document.getElementById('plotScale')! as HTMLInputElement).value === 'auto' &&
                  autoView.volume.bounds[4] === initial.volume.bounds[4] &&
                  autoView.volume.bounds[5] === initial.volume.bounds[5] &&
                  autoFrame.scaling[0] === 4 && autoFrame.scaling[1] === 4 && autoFrame.scaling[2] === 1,
                  'Explicit Auto zoom narrows the horizontal region without clipping the height preview');
            const spans = [0, 1, 2].map(i => autoView.volume.bounds[2*i+1] - autoView.volume.bounds[2*i]);
            const lengths = [0, 1, 2].map(i => Math.hypot(...[0, 1, 2].map(j => autoFrame.rows[j*3+i])));
            check(lengths.every((v, i) => Math.abs(v / spans[i] - lengths[0] / spans[0]) < 1e-10),
                  'Auto height preview preserves equal physical units while its box proportions change');
            const refinedChart = before.cloneNode(false) as SVGSVGElement;
            const next = [...autoView.volume.bounds];
            next[4] += (next[5] - next[4]) / 4;
            const keys = ['XMin', 'XMax', 'YMin', 'YMax', 'ZMin', 'ZMax'];
            keys.forEach((key, i) => {
                refinedChart.dataset[key[0].toLowerCase() + key.slice(1)] = String(next[i]);
                refinedChart.dataset['exact' + key] = String(next[i]);
                refinedChart.dataset['label' + key] = String(next[i]);
            });
            const nextSpans = [0, 1, 2].map(i => next[2*i+1] - next[2*i]);
            const refinedView = {...autoView, chart: refinedChart,
                                 units: nextSpans.map(v => v / Math.max(...nextSpans))};
            labDOM.call('lab_surface_volume_init', refinedView,
                        {volume: labSurfaceVolume.clone(autoView.volume), refining: true}, 0);
            const refinedFrame = labDOM.call<MarsSurfaceFrame>('lab_surface_frame', refinedView, null);
            check(refinedView.volume.exponent === 2 && refinedView.volume.bounds[4] === next[4] &&
                  refinedView.volume.bounds[5] === next[5] && refinedFrame.scaling.every(v => v === 1),
                  'Auto refinement adopts the new native height without resetting horizontal zoom');
            const resetVolume = labSurfaceVolume.clone(refinedView.volume);
            resetVolume.started = false;
            resetVolume.bounds = [-2, 2, -2, 2, 0, 200];
            const resetBounds = [-10, 10, -10, 10, 0, 200];
            const resetChart = before.cloneNode(false) as SVGSVGElement;
            keys.forEach((key, i) => {
                resetChart.dataset[key[0].toLowerCase() + key.slice(1)] = String(resetBounds[i]);
                resetChart.dataset['exact' + key] = String(resetBounds[i]);
                resetChart.dataset['label' + key] = String(resetBounds[i]);
            });
            const resetView = {...refinedView, chart: resetChart, units: [0.1, 0.1, 1]};
            labDOM.call('lab_surface_volume_init', resetView, {volume: resetVolume, refining: true}, 0);
            check(resetView.volume.bounds.every((value, i) => value === resetBounds[i]) &&
                  resetView.volume.base.every((value, i) => value === resetBounds[i]) &&
                  resetView.volume.exact.every((value, i) => value === String(resetBounds[i])),
                  'Auto reset refinement adopts native bounds, baseline and exact endpoints instead of stale cropped input bounds');
            labDOM.call('lab_surface_zoom', autoView, 1, 0, null, 1);
        }
        // The following fixtures exercise strict coordinate-volume zoom independently of Auto height.
        view.volume.autoHeight = false;
        initial.volume.autoHeight = false;
        const axisPoints: MarsSurfacePoint3[] = [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]];
        const axes = axisPoints.map(view.project!);
        const wheel = (deltaY: number) => chart().dispatchEvent(new WheelEvent('wheel',
            {deltaY, ctrlKey: true, bubbles: true, cancelable: true}));
        check(!wheel(-100), 'Volume wheel zoom prevents page scrolling');
        await settle();
        const zoomed = labSurfacePreview.snapshot()!;
        for (let axis = 0; axis < 3; ++axis) {
            const base = initial.volume.bounds, bounds = zoomed.volume.bounds, at = axis * 2;
            check(Math.abs((base[at + 1] - base[at]) / (bounds[at + 1] - bounds[at]) -
                           zoomed.camera.scale / initial.camera.scale) < 1e-10 &&
                  Math.abs((base[at] + base[at + 1]) - (bounds[at] + bounds[at + 1])) < 1e-10,
                  'Volume zoom reduces all three spans by the same factor around their current centres');
        }
        check(axisPoints.every((p, i) =>
            view.project!(p).every((v, j) => Math.abs(v - axes[i][j]) < 1e-10)),
            'Axes stay fitted instead of growing with the mesh');
        check((document.getElementById('plotScale')! as HTMLInputElement).value === 'manual' &&
              zoomed.volume.exact.every((v, i) => (document.getElementById(controls[i])! as LabTestPlotInput).marsZoomValue!.includes(v)) &&
              controls.slice(0, 6).every(id => (document.getElementById(id)! as LabTestPlotInput).marsZoomValue!.includes('2^(-(')),
              'The native sampler receives exact baseline recipes rather than rounded browser coordinates');
        for (const policy of ['auto', 'all']) {
            (document.getElementById('plotScale')! as HTMLInputElement).value = policy;
            const restoredView = {chart: before, units: [...view.units], camera: {...zoomed.camera}} as
                LabTestSurfaceCameraFixture;
            labDOM.call('lab_surface_volume_init', restoredView, zoomed, 1);
            check(restoredView.volume.exponent === 0 && restoredView.camera.scale === 1 &&
                  restoredView.volume.bounds.every((value, i) => value === initial.volume.base[i]) &&
                  restoredView.volume.exact.every((value, i) => value === before.dataset[
                      ['exactXMin', 'exactXMax', 'exactYMin', 'exactYMax', 'exactZMin', 'exactZMax'][i]]),
                  policy + ' discards a clipped zoom preview even when the native mesh is unchanged');
        }
        (document.getElementById('plotScale')! as HTMLInputElement).value = 'manual';
        const refiningView = {chart: before, units: [...view.units], camera: {...zoomed.camera}} as
            LabTestSurfaceCameraFixture;
        labDOM.call('lab_surface_volume_init', refiningView,
                    {...zoomed, volume: labSurfaceVolume.clone(zoomed.volume), refining: true}, 1);
        check(refiningView.volume.exponent === zoomed.volume.exponent &&
              refiningView.volume.bounds.every((value, i) => value === zoomed.volume.bounds[i]),
              'Native zoom refinement keeps the selected clipped volume rather than fitting the surface again');
        const formattedBounds = [-3.1998336521996507, 3.1998336521996507,
                                 1000000.00001234, 1000000.00005678, -1.0666112173998836, 1.0666112173998836];
        labDOM.call('lab_plot_volume_bounds', formattedBounds);
        check((document.getElementById('plotUpper')! as HTMLInputElement).value === '3.2' &&
              (document.getElementById('plotYUpper')! as HTMLInputElement).value === '1.067' &&
              Number((document.getElementById('plotSecondLower')! as HTMLInputElement).value) !==
                  Number((document.getElementById('plotSecondUpper')! as HTMLInputElement).value) &&
              formattedBounds.every((v, i) => Number((document.getElementById(controls[i])! as LabTestPlotInput).marsZoomValue) === v),
              'C formats bounds by interval width without losing narrow spans or sampling coordinates');
        labSurfaceVolume.set(view, 1);
        const clipped = labSurfaceVolume.clip([[-1, -1, 0.5], [2, 0.5, 0.5], [0.5, 2, 0.5]]);
        check(clipped.length >= 3 && clipped.every(p => p.every(v => v >= -1e-12 && v <= 1 + 1e-12)),
              'Canvas and SVG polygons are clipped at the coordinate-volume faces');
        await waitForVolume();
        const refined = labSurfacePreview.snapshot()!;
        check(chart() !== before && Math.abs(refined.camera.scale - zoomed.camera.scale) < 1e-12 &&
              refined.volume.bounds.every((v, i) => Math.abs(v - refined.volume.sampled[i]) < 1e-8),
              'Refinement replaces geometry without resetting the chosen volume or zoom');
        wheel(100);
        await settle();
        check(Math.abs(labSurfacePreview.snapshot()!.camera.scale - initial.camera.scale) < 1e-12,
              'Reversing wheel direction reverses coordinate-volume zoom after refinement');
        card.querySelector<HTMLElement>('[data-zoom-reset]')!.click();
        await waitForVolume();
        check(labSurfacePreview.snapshot()!.camera.scale === 1 &&
              controls.every((id, i) => (document.getElementById(id)! as LabTestPlotInput).value === original[i]),
              'Reset restores the original ranges and automatic-height policy');

        // An obsolete native refinement must never move the viewport back.
        const fetchBefore = WorksheetRequests.evaluationTransport.fetchEvaluation;
        let release: ((value: WorksheetRequests.MarsApiPostResult) => void) | undefined;
        try {
            WorksheetRequests.evaluationTransport.fetchEvaluation = (...args) => args[2] === 'plot' ?
                new Promise(resolve => release = resolve) : fetchBefore(...args);
            card.querySelector<HTMLElement>('[data-zoom-step="1"]')!.click();
            const deadline = performance.now() + 5000;
            while (!release && performance.now() < deadline) await LabTestBrowser.wait(25);
            check(!!release, 'Hold one native volume refinement in flight');
            card.querySelector<HTMLElement>('[data-zoom-step="-1"]')!.click();
            await settle();
            const newest = labSurfacePreview.snapshot()!.volume.bounds;
            const staleData = {ok: false, error: 'Obsolete volume fixture'};
            release!({response: Object.assign(new Response(), {labData: async () => staleData}), data: staleData});
            WorksheetRequests.evaluationTransport.fetchEvaluation = fetchBefore;
            await waitForVolume();
            check(labSurfacePreview.snapshot()!.volume.bounds.every((v, i) => Math.abs(v - newest[i]) < 1e-10) &&
                  !rendered.textContent.includes('Obsolete volume fixture'),
                  'Late cancelled refinement cannot replace newer zoom bounds');
        } finally { WorksheetRequests.evaluationTransport.fetchEvaluation = fetchBefore; }
        card.querySelector<HTMLElement>('[data-zoom-reset]')!.click();
        await waitForVolume();
        const deepView = labSurfacePreview.prepare(chart())!;
        check(labSurfaceVolume.set(deepView, 80, false, 1), 'C accepts zoom beyond double coordinate resolution');
        labSurfacePreview.draw(chart(), deepView.angles);
        await waitForVolume();
        const deep = labSurfacePreview.snapshot()!;
        check(deep.volume.exponent === 80 && deep.volume.sampleExponent === 80 &&
              chart().dataset.exactZMin !== chart().dataset.exactZMax &&
              chart().dataset.localRenderer === 'webgl',
              'Microscopic native volume retains distinct exact endpoints and a normalised GPU frame');
        card.querySelector<HTMLElement>('[data-zoom-step="1"]')!.click();
        await waitForVolume();
        check(labSurfacePreview.snapshot()!.volume.exponent > 80,
              'Zoom continues after microscopic native refinement without rebasing through doubles');
        card.querySelector<HTMLElement>('[data-zoom-reset]')!.click();
        await waitForVolume();
        await runBrowserGroup('checkPlotPinch', checkPlotPinch);
    }
    async function checkFunctionRun() {
        await WorksheetRequests.functionCard.runFunctionCard();
        check(WorksheetContext.functionRunOutput.textContent.trim() === '5', 'Native Function RUN did not evaluate the full card');
        await window.checkLabMobileLayout('function run output');
        const originalFetch = window.fetch;
        const originalText = WorksheetContext.functionStyle.textContent;
        const fullSource = WorksheetContext.functionStyle.dataset.fullText!.trim();
        try {
            WorksheetContext.functionStyle.textContent = 'abbreviated...';
            window.fetch = async (url, options) => {
                const body = labWire.decode((options!.body as Uint8Array<ArrayBuffer>).buffer) as
                    {source: string; precision: number};
                check(String(url).includes('/function-run'), 'RUN used the wrong endpoint');
                check(
                    body.source === fullSource && body.precision === WorksheetWorkspace.precision.requestedValuePrecision(),
                    'RUN sent abbreviated text or the wrong precision');
                return new Response(
                    labWire.encode({ok: false, error: '<img src=x onerror=alert(1)>'}),
                    {status: 422, headers: {'Content-Type': 'application/x-protobuf'}});
            };
            await WorksheetRequests.functionCard.runFunctionCard();
            check(WorksheetContext.functionRunOutput.textContent.includes('<img'), 'RUN diagnostic missing');
            check(!WorksheetContext.functionRunOutput.querySelector('img')!, 'RUN diagnostic was interpreted as HTML');
            let release: ((value: Response) => void) | undefined;
            window.fetch = () => new Promise(resolve => {
                release = resolve;
            });
            const pending = WorksheetRequests.functionCard.runFunctionCard();
            WorksheetRequests.functionCard.clearFunctionRun();
            release!(new Response(
                labWire.encode({ok: true, output: 'stale'}), {headers: {'Content-Type': 'application/x-protobuf'}}));
            await pending;
            check(!WorksheetContext.functionRunOutput.textContent && !WorksheetContext.functionRun.disabled, 'Stale RUN result replaced the new card');
        } finally {
            window.fetch = originalFetch;
            WorksheetContext.functionStyle.textContent = originalText;
            WorksheetRequests.functionCard.clearFunctionRun();
        }
    }
    async function checkRenderedGlyphs() {
        await LabTestBrowser.paint();
        const svg = WorksheetContext.rendered.querySelector('svg')!;
        check(svg, `Native TeX SVG missing: mode=${WorksheetWorkspace.mode.currentMode()}, ` +
            `status=${WorksheetContext.statusEl.textContent}, result=${WorksheetContext.rendered.textContent}`);
        const bounds = svg.getBoundingClientRect();
        check(bounds.width > 0 && bounds.height > 0, 'TeX SVG has no visible dimensions');
        const glyphs = Array.from(svg.querySelectorAll('use'));
        check(glyphs.length > 0, 'TeX SVG has no glyphs');
        for (const glyph of glyphs) {
            const reference =
                glyph.getAttribute('href') || glyph.getAttributeNS('http://www.w3.org/1999/xlink', 'href');
            check(
                reference?.startsWith('#') && document.getElementById(reference.slice(1)),
                'TeX glyph reference is broken: ' + reference);
            const box = glyph.getBBox();
            check(box.width > 0 && box.height > 0, 'TeX glyph has no rendered geometry');
        }
    }

    async function checkWire() {
        function record(value: unknown): LabTestRecord {
            if (value === null || typeof value !== 'object' || Array.isArray(value))
                throw new Error('Expected a decoded record');
            return value as LabTestRecord;
        }
        check(!document.getElementById('lab-config')!, 'JSON configuration is still embedded in the page');
        const bootstrap = await labHttp.fetch('/bootstrap');
        check(bootstrap.headers.get('Content-Type') === 'application/x-protobuf', 'Bootstrap did not use Protobuf');
        const config = await bootstrap.labData() as LabConfiguration;
        check(
            config.BROWSER_ABI_VERSION === String(labWire.exports().lab_browser_abi_version()),
            'Running native server and WASM ABI differ');
        check(config.DEFAULT_EXPRESSION === labApplication.configuration.DEFAULT_EXPRESSION, 'Bootstrap defaults differ from the UI');
        const empty = labWire.encode({});
        check(Array.from(empty).join(',') === '8,1,18,2,8,5', 'Protobuf golden envelope mismatch');
        window.checkLabProtobufWire(labWire);
        const source = {
            unicode: '\ufeffμσ e\u0301 🪐\u0000',
            nil: null,
            yes: true,
            no: false,
            number: 1.25,
            exact: '1/3',
            list: [0, '', {nested: ['π', 42]}],
            empty: {}
        };
        const restored = labWire.decode(labWire.encode(source).buffer);
        check(JSON.stringify(source) === JSON.stringify(restored), 'C codec round trip changed a value');
        const storageKey = 'mars.browserTest.calendarState';
        const storedBefore = localStorage.getItem(`${storageKey}.protobuf`);
        try {
            localStorage.removeItem(`${storageKey}.protobuf`);
            check(WorksheetPersistence.persistence.readLocal(storageKey) === null, 'missing fallback was not empty');
            WorksheetPersistence.persistence.writeLocal(storageKey, source);
            check(
                JSON.stringify(WorksheetPersistence.persistence.readLocal(storageKey)) === JSON.stringify(source),
                'stored Protobuf changed Unicode, exact text or nested values');
            const stored = localStorage.getItem(`${storageKey}.protobuf`)!;
            check(
                JSON.stringify(labWire.decode(Uint8Array.from(atob(stored), c => c.charCodeAt(0)).buffer)) ===
                    JSON.stringify(source),
                'fallback storage is not the same Protobuf envelope as the API');
            WorksheetPersistence.persistence.writeLocal(storageKey, {text: 'μ'.repeat(10000)});
            check(record(WorksheetPersistence.persistence.readLocal(storageKey)).text === 'μ'.repeat(10000), 'chunked storage lost bytes');
            for (const invalid of ['not base64!', btoa('{}'), stored.slice(0, -4)]) {
                localStorage.setItem(`${storageKey}.protobuf`, invalid);
                check(WorksheetPersistence.persistence.readLocal(storageKey) === null, 'damaged, JSON or oversized fallback was accepted');
            }
            const decodeBase64 = window.atob, encodeBase64 = window.btoa;
            try {
                window.atob = window.btoa = function browserBase64Forbidden(): never {
                    throw new Error('Storage must use native base64');
                };
                WorksheetPersistence.persistence.writeLocal(storageKey, source);
                check(JSON.stringify(WorksheetPersistence.persistence.readLocal(storageKey)) === JSON.stringify(source),
                    'fallback persistence still depends on browser base64 conversion');
            } finally {
                window.atob = decodeBase64;
                window.btoa = encodeBase64;
            }
            WorksheetPersistence.persistence.writeLocal(storageKey, {town: 'Shrewsbury'});
            check(record(WorksheetPersistence.persistence.readLocal(storageKey)).town === 'Shrewsbury', 'fallback did not recover after corruption');
        } finally {
            if (storedBefore === null)
                localStorage.removeItem(`${storageKey}.protobuf`);
            else
                localStorage.setItem(`${storageKey}.protobuf`, storedBefore);
        }
        function rejects(action: () => unknown) {
            try {
                action();
                return false;
            } catch (_) {
                return true;
            }
        }
        const padded = new Uint8Array(empty.length + 4);
        padded.set(empty, 2);
        check(
            Object.keys(record(labWire.decode(new DataView(padded.buffer, 2, empty.length)))).length === 0,
            'DataView offsets were ignored');
        check(
            Object.keys(record(labWire.decode(padded.subarray(2, padded.length - 2)))).length === 0,
            'Typed view offsets were ignored');
        // Deliberately invalid buffer fixtures exercise runtime boundary rejection.
        check(rejects(() => labWire.decode({byteLength: empty.length} as unknown as MarsTransportBuffer)), 'Fake byte buffer accepted');
        if (typeof SharedArrayBuffer === 'function')
            check(rejects(() => labWire.decode(new Uint8Array(new SharedArrayBuffer(6)) as unknown as MarsTransportBuffer)), 'Shared buffer accepted');
        const realm = document.createElement('iframe');
        document.body.appendChild(realm);
        try {
            const foreign = new (realm.contentWindow as Window & typeof globalThis).Uint8Array(empty);
            check(Object.keys(record(labWire.decode(foreign))).length === 0, 'Cross-realm typed view rejected');
        } finally {
            realm.remove();
        }
        let getterCalled = false;
        const accessor = {
            get value() {
                getterCalled = true;
                return 1;
            }
        };
        check(rejects(() => labWire.encode(accessor)) && !getterCalled, 'Codec invoked an accessor');
        const arrayAccessor = [0];
        Object.defineProperty(arrayAccessor, '0', {get: function readArrayElement(): number {
            getterCalled = true;
            return 1;
        }});
        check(rejects(() => labWire.encode({array: arrayAccessor})) && !getterCalled,
              'Container snapshotting rejects array accessors without invoking them');
        check(rejects(() => labWire.encode({array: new Array(2)})), 'Container snapshotting rejects sparse arrays');
        const hiddenAccessor = {visible: 'kept'};
        Object.defineProperty(hiddenAccessor, 'hidden', {get: function readHidden(): never {
            throw new Error('Non-enumerable getter must not run');
        }});
        const visibleOnly = record(labWire.decode(labWire.encode(hiddenAccessor)));
        check(visibleOnly.visible === 'kept' && !Object.hasOwn(visibleOnly, 'hidden'),
              'Container snapshots retain enumerable data without inspecting hidden accessors');
        const recursive = new Proxy({}, {
            ownKeys() {
                labWire.encode({});
                return [];
            }
        });
        check(rejects(() => labWire.encode(recursive)), 'Recursive codec invocation accepted');
        check(labWire.encode({}).length === empty.length, 'Codec did not recover after rejected recursion');
        const numbers = record(labWire.decode(labWire.encode({zero: -0, tiny: Number.MIN_VALUE})));
        check(Object.is(numbers.zero, -0) && numbers.tiny === Number.MIN_VALUE, 'Binary64 edge values changed');
        const maximum = labWire.encode({x: 'a'.repeat(8388579)});
        check(maximum.length === 8388608, 'Exact wire limit was rejected');
        const maximumText = record(labWire.decode(maximum)).x;
        check(typeof maximumText === 'string' && maximumText.length === 8388579, 'Maximum message did not round trip');
        check(rejects(() => labWire.encode({x: 'a'.repeat(8388580)})), 'Wire limit plus one accepted');
        for (let i = 0; i < empty.length; ++i)
            check(rejects(() => labWire.decode(empty.slice(0, i).buffer)), 'Truncated envelope accepted');
        check(rejects(() => labWire.decode(new Uint8Array([8, 2, 18, 2, 8, 5]).buffer)), 'Unknown version accepted');
        check(rejects(() => labWire.decode(new Uint8Array([...empty, 8, 1]).buffer)), 'Duplicate version accepted');
        const unknown = labWire.decode(new Uint8Array([...empty, 120, 1]).buffer);
        check(Object.keys(record(unknown)).length === 0, 'Unknown supported field was not skipped');
        for (const node
                 of [[8, 5, 8, 5], [8, 5, 16, 1], [8, 5, 50, 11, 10, 1, 120, 18, 6, 8, 3, 34, 2, 192, 128],
                     [8, 5, 50, 9, 10, 1, 120, 18, 4, 8, 1, 16, 2], [8, 5, 50, 4, 18, 2, 8, 0, 50, 4, 18, 2, 8, 0]]) {
            check(
                rejects(() => labWire.decode(new Uint8Array([8, 1, 18, node.length, ...node]).buffer)),
                'Malformed schema payload accepted');
        }
        check(rejects(() => labWire.encode({number: Infinity})), 'Infinite number accepted');
        for (const ending of [[240, 127], [240, 255], [248, 127]]) {
            // Version-one object containing x = positive infinity, negative infinity or NaN.
            const node = [8, 5, 50, 16, 10, 1, 120, 18, 11, 8, 2, 25, 0, 0, 0, 0, 0, 0, ...ending];
            check(rejects(() => labWire.decode(new Uint8Array([8, 1, 18, node.length, ...node]))),
                  'Native decoding rejects non-finite binary64 values without a second browser tree walk');
        }
        const memberLimit = labWire.exports().lab_browser_member_limit();
        for (const count of [memberLimit, memberLimit + 1]) {
            let message = '';
            try {
                labWire.encode({list: new Array(count).fill(null)});
            } catch (caught: unknown) {
            const error = caught as Error;
                message = error.message;
            }
            check(message.includes(count === memberLimit ? 'Invalid or oversized' : 'too many members'),
                'host member admission preserves the exact native boundary');
        }
        check(record(labWire.decode(labWire.encode({valid: true}))).valid, 'codec recovers after member rejection');
        check(rejects(() => labWire.encode({text: '\ud800'})), 'Invalid surrogate accepted');
        check(rejects(() => labWire.encode({text: 'x'.repeat(8388608)})), 'Oversized message accepted');
        let deep = {};
        for (let i = 0; i < 34; ++i) deep = {deep};
        check(rejects(() => labWire.encode(deep)), 'Excessive nesting accepted');
        const malicious: LabTestRecord = Object.create(null);
        malicious.__proto__ = {polluted: true};
        const safe = labWire.decode(labWire.encode(malicious).buffer);
        check(Object.hasOwn(record(safe), '__proto__') && !Reflect.get({}, 'polluted'), 'Protobuf key changed an object prototype');
        check(
            labWire.precision(1, 53) === 17 && labWire.precision(2000000, 53) === 262144 &&
                labWire.precision(NaN, 53) === 53,
            'C precision validation failed');
        check(
            labWire.intervals(20000, 5000) === 20000 && labWire.intervals(42, 5000) === 5000,
            'C integration budget validation failed');
        const response = await labHttp.fetch('/state', {method: 'POST', body: labWire.encode({expression: '2+3'})});
        check(response.headers.get('Content-Type') === 'application/x-protobuf', 'Server did not speak Protobuf');
        const result = record(await response.labData());
        check(result.expression === '2+3', 'Native Protobuf round trip changed data');
        const invalid = await labHttp.fetch('/state', {method: 'POST', body: new Uint8Array([8, 2])});
        check(invalid.headers.get('Content-Type') === 'application/x-protobuf', 'Error response did not use Protobuf');
        check(invalid.status === 400 && !record(await invalid.labData()).ok, 'Malformed native request was not rejected');
        const originalFetch = window.fetch;
        try {
            for (const type of ['application/json', 'application/x-protobuf']) {
                window.fetch = async () => new Response(new Uint8Array(8388609), {headers: {'Content-Type': type}});
                const oversized = await labHttp.fetch('/fixture');
                let rejected = false;
                try {
                    await oversized.labData();
                } catch (caught: unknown) {
            const error = caught as Error;
                    rejected = error.message.includes(type === 'application/json' ? 'Expected a Protobuf' : 'native message limit');
                }
                check(rejected, `Oversized ${type} response was not bounded`);
            }
            let cancelled = false;
            const messageLimit = labWire.exports().lab_browser_message_limit();
            window.fetch = async () => new Response(new ReadableStream({
                start(controller) {
                    controller.enqueue(new Uint8Array(messageLimit));
                    controller.enqueue(new Uint8Array(1));
                },
                cancel() {
                    cancelled = true;
                }
            }), {headers: {'Content-Type': 'application/x-protobuf'}});
            let rejected = false;
            try {
                await (await labHttp.fetch('/fixture')).labData();
            } catch (caught: unknown) {
            const error = caught as Error;
                rejected = error.message.includes('native message limit');
            }
            check(rejected && cancelled, 'stream crossing the native bound is rejected and cancelled');
        } finally {
            window.fetch = originalFetch;
        }
    }
    async function report(message: string) {
        if (message.startsWith('PASS:') && sessionStorage.getItem('labSurfaceRenderer'))
            message += '; surface renderer: ' + sessionStorage.getItem('labSurfaceRenderer');
        await nativeFetch('/state', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-protobuf'},
            body: labWire.encode({expression: `BROWSER ${message}`, expression_updated_at: Date.now() + 1000000})
        });
    }
    async function checkHelpLayout() {
        const frame = document.createElement('iframe');
        frame.style.cssText = 'position:fixed;left:-10000px;top:0;height:1000px;border:0';
        try {
            const loaded = new Promise(resolve => frame.onload = resolve);
            frame.srcdoc = '<!doctype html><html><head><link rel="stylesheet" href="/index.css"></head>' +
                '<body></body></html>';
            document.body.appendChild(frame);
            await loaded;
            const workspace = document.getElementById('labWorkspace')!.cloneNode(true) as HTMLElement;
            workspace.classList.add('help-open');
            workspace.querySelector<HTMLElement>('#helpPane')!.classList.remove('hidden');
            workspace.querySelector<HTMLElement>('#helpClose')!.classList.remove('hidden');
            workspace.querySelector<HTMLElement>('#resultPane')!.classList.add('hidden');
            for (const card of workspace.querySelectorAll<HTMLElement>('.help-card'))
                card.classList.remove('hidden');
            frame.contentDocument!.body.appendChild(workspace);
            for (const width of [1920, 900, 390]) {
                frame.style.width = `${width}px`;
                await LabTestBrowser.paint();
                const panel = workspace.querySelector<HTMLElement>('#resultWorkspacePanel')!.getBoundingClientRect();
                const bar = workspace.querySelector<HTMLElement>('.lab-topbar')!.getBoundingClientRect();
                check(Math.abs(panel.left - bar.left) < 2 && Math.abs(panel.right - bar.right) < 2,
                      `Help spans the full workspace at ${width}px`);
                for (const card of workspace.querySelectorAll<HTMLElement>('.help-card'))
                    check(card.scrollWidth <= card.clientWidth + 1, `Help text overflows at ${width}px`);
                const links = workspace.querySelectorAll<HTMLAnchorElement>('.help-card a');
                check(links.length >= 5, 'Help retains reference and provider-policy links');
                for (const link of links) {
                    const style = frame.contentWindow!.getComputedStyle(link);
                    check(style.color === 'rgb(246, 211, 138)' && style.textDecorationLine.includes('underline'),
                          `Help links must use the bright, underlined theme style at ${width}px`);
                }
                check(frame.contentDocument!.documentElement.scrollWidth <= width,
                      `Help must not introduce page-wide horizontal scrolling at ${width}px`);
            }
        } finally {
            frame.remove();
        }
    }

    async function checkAlmanacLayout() {
        // Timestamp parsing belongs to test_lab_almanac_presentation.c; this fixture checks DOM consumption only.
        const visibility = WorksheetContext.worksheetSession.calendar.almanacVisibilityMode;
        const frame = document.createElement('iframe');
        frame.style.cssText = 'position:fixed;left:-10000px;top:0;height:1000px;border:0';
        try {
            const loaded = new Promise(resolve => {
                frame.onload = resolve;
            });
            frame.srcdoc = '<!doctype html><html><head><link rel="stylesheet" href="/index.css"></head>' +
                '<body><div id="fixture" class="rendered" style="padding:0;' +
                '--render-zoom:1.5;--render-margin-bottom:40px"></div></body></html>';
            document.body.appendChild(frame);
            await loaded;
            const target = frame.contentDocument!.getElementById('fixture')!;
            const [fixture, pending, sections] = window.labAlmanacFixtures();
            for (const incomplete
                     of [{visibility: 'all'},
                         {...fixture, almanac_presentation: {all: {...fixture.almanac_presentation.all, html: ''}}},
                         {...fixture, almanac_presentation: {all: {...fixture.almanac_presentation.all, html: 42}}}]) {
                target.textContent = 'Previous worksheet';
                target.dataset.copyText = 'Previous copy';
                let rejected = false;
                try {
                    WorksheetResults.calendarResults.renderAlmanacWorksheet(target, incomplete);
                } catch (caught: unknown) {
            const error = caught as Error;
                    rejected = error.message.includes('Almanac presentation') &&
                        error.message.includes('make mars-lab-restart');
                }
                check(rejected, 'Missing native Almanac markup was accepted as a blank successful worksheet');
                check(
                    target.textContent === 'Previous worksheet' && target.dataset.copyText === 'Previous copy',
                    'Rejected Almanac response cleared the previous worksheet');
            }
            // Poison raw metadata: DOM must use the native markup, not reconstruct it.
            fixture.all_rows = [];
            fixture.events = [];
            WorksheetResults.calendarResults.renderAlmanacWorksheet(target, fixture);
            check(
                target.querySelectorAll<HTMLElement>('.almanac-grid-table:not(.almanac-event-table) tbody tr').length === 4,
                'Native all-body rows were not used');
            check(
                target.querySelector<HTMLElement>('.almanac-body-symbol')!.textContent === '☉' &&
                    target.querySelector('.almanac-moon-phase')?.getAttribute('aria-label') ===
                        'Moon: 50.0% illuminated; local zenith up',
                'Native astronomical symbols or the accessible lunar phase were lost');
            check(
                target.querySelector<HTMLElement>('[data-body="SUN"]')!.namespaceURI === 'http://www.w3.org/2000/svg' &&
                    target.querySelector<HTMLElement>('[data-body="SUN"] circle')! &&
                    !target.querySelector<SVGTextElement>('.almanac-body-symbol image, .almanac-body-symbol text')!,
                'Body symbols must be vector geometry, not bitmap images or font glyphs');
            const brightStar = target.querySelector<HTMLElement>('[data-body="SIRIUS"]')!;
            const faintStar = target.querySelector<HTMLElement>('[data-body="POLARIS"]')!;
            check(
                brightStar?.classList.contains('almanac-star-symbol') &&
                    faintStar?.classList.contains('almanac-star-symbol') &&
                    brightStar.querySelector('path')!.getBoundingClientRect().width >
                        faintStar.querySelector('path')!.getBoundingClientRect().width &&
                    brightStar.getBoundingClientRect().width === faintStar.getBoundingClientRect().width &&
                    brightStar.getAttribute('aria-label')!.includes('visual magnitude -1.46'),
                'Star geometry must scale with brightness without changing the aligned icon slots');
            for (const star of [brightStar, faintStar]) {
                check(frame.contentWindow!.getComputedStyle(star.querySelector('path')!).fill ===
                          'rgb(243, 228, 179)',
                      'Stars must retain their warm pale-gold colour at every brightness-based size');
            }
            const moonIcon = target.querySelector<HTMLElement>('.almanac-moon-phase')!;
            check(
                frame.contentWindow!.getComputedStyle(target.querySelector<HTMLElement>('[data-body="SUN"]')!).color ===
                    'rgb(239, 195, 106)' &&
                    frame.contentWindow!.getComputedStyle(moonIcon).color === 'rgb(183, 195, 204)' &&
                    frame.contentWindow!.getComputedStyle(moonIcon.querySelector<HTMLElement>('.almanac-moon-shadow')!).fill ===
                        'rgb(65, 73, 79)' &&
                    frame.contentWindow!.getComputedStyle(moonIcon.querySelector<HTMLElement>('.almanac-moon-rim')!).stroke ===
                        'rgb(126, 139, 148)',
                'The Sun and phase-aware Moon lost their individual reference colours');
            const colourProbe = frame.contentDocument!.createElement('span');
            colourProbe.className = 'almanac-body-symbol';
            target.appendChild(colourProbe);
            for (const [body, colour] of [
                     ['MERCURY', 'rgb(184, 68, 197)'], ['VENUS', 'rgb(196, 182, 77)'],
                     ['MARS', 'rgb(217, 77, 76)'], ['JUPITER', 'rgb(232, 117, 120)'],
                     ['SATURN', 'rgb(217, 154, 57)'], ['URANUS', 'rgb(72, 190, 208)'],
                     ['NEPTUNE', 'rgb(128, 101, 233)']]) {
                colourProbe.dataset.body = body;
                check(frame.contentWindow!.getComputedStyle(colourProbe).color === colour, `${body} symbol colour`);
            }
            colourProbe.remove();
            check(
                moonIcon.getBoundingClientRect().width > 10 &&
                    moonIcon.getBoundingClientRect().width < 40 &&
                    moonIcon.querySelector('path')?.getAttribute('transform') === 'rotate(180.000000 12 12)',
                'Lunar SVG is missing or is not sized as an inline symbol');
            check(
                target.querySelector<HTMLElement>('.visible-cell.yes')!.textContent.trim() === '✓',
                'Client recomputed visibility from the raw row instead of native metadata');
            check(
                target.dataset.copyText === fixture.almanac_presentation.all.copy_text &&
                    almanacWorksheetCopyText(fixture, 'visible') === fixture.almanac_presentation.visible.copy_text,
                'Clipboard text was rebuilt instead of retaining the native variants');
            check(!target.querySelector<HTMLElement>('[data-almanac-land-totality]')!, 'Client recomputed land eligibility');
            const action = target.querySelector<HTMLElement>('[data-almanac-use-totality]')!;
            check(
                action?.dataset.date === '2027-08-02' && action.dataset.town === 'Málaga|36.7204|-4.4203|0',
                'Native totality action payload was lost');
            const table = target.querySelector<HTMLTableElement>('.almanac-event-table')!;
            const eclipseIcon = table.querySelector<HTMLElement>('.almanac-eclipse-symbol')!;
            check(
                eclipseIcon?.getAttribute('aria-label')!.includes('greatest eclipse; local zenith up') &&
                    eclipseIcon.querySelector('g')!.getAttribute('transform') ===
                        'translate(12 12) rotate(-45.000000000)' &&
                    eclipseIcon.querySelector('path')! && !eclipseIcon.querySelector('image')!,
                'The observer-oriented maximum-eclipse SVG was not installed beside the event name');
            check(frame.contentWindow!.getComputedStyle(eclipseIcon.querySelector('circle')!).fill ===
                      'rgb(239, 195, 106)',
                  'The solar-eclipse disc must use the same gold as the Sun symbol');
            const region = table.parentElement!;
            check(
                region.tabIndex === 0 && region.getAttribute('role') === 'region',
                'Table scroll area is not keyboard accessible');
            check(
                table.querySelector<HTMLElement>('.event-time[title]')!.textContent === '09:06:00',
                'Contact still includes the full date');
            check(table.querySelector<HTMLElement>('.event-time[title]')!.title.endsWith('GMT+1'), 'Full contact time was lost');
            for (const [viewport, card] of [[1920, 860], [1920, 1700], [320, 290], [390, 350], [430, 390]]) {
                frame.style.width = `${viewport}px`;
                target.style.width = `${card}px`;
                await LabTestBrowser.paint();
                for (const icon of target.querySelectorAll<HTMLElement>('td .almanac-body-symbol, td .almanac-eclipse-symbol')) {
                    const style = frame.contentWindow!.getComputedStyle(icon);
                    check(style.filter === 'none' && style.transform === 'none',
                          'Formula recolouring or zoom leaked into an astronomical SVG');
                    check(parseFloat(style.marginBottom) === 0,
                          `Astronomical symbol has formula spacing: ${style.marginBottom}`);
                    const cell = icon.closest<HTMLElement>('td')!, text = frame.contentDocument!.createRange();
                    text.selectNodeContents(cell.querySelector<HTMLElement>('.almanac-event-label')! || cell.lastChild);
                    const labelBox = text.getBoundingClientRect(), iconBox = icon.getBoundingClientRect();
                    check(Math.abs(iconBox.y + iconBox.height / 2 - labelBox.y - labelBox.height / 2) < 5,
                          'An astronomical icon is above its name instead of beside it');
                    if (icon.classList.contains('almanac-body-symbol')) {
                        const height = viewport > 560 ? icon.closest<HTMLElement>('tr')!.getBoundingClientRect().height :
                            cell.getBoundingClientRect().height;
                        check(height < 65, 'Body symbols inflated the name row height');
                    }
                }
                const bodies = target.querySelector<HTMLTableElement>('.almanac-body-table')!;
                if (viewport <= 560) {
                    check(bodies.scrollWidth <= bodies.clientWidth + 1,
                          'Phone body entries must not require horizontal scrolling');
                    for (const row of bodies.tBodies[0].rows) {
                        check(frame.contentWindow!.getComputedStyle(row).display === 'grid',
                              'Phone bodies must use labelled entries');
                        const metrics = row.querySelectorAll<HTMLElement>('td.number');
                        check(metrics.length === 7, 'A body measurement was lost on mobile');
                        for (const cell of metrics) {
                            const label = frame.contentWindow!.getComputedStyle(cell, '::before');
                            check(cell.dataset.label && label.content.includes(cell.dataset.label),
                                  'Mobile body measurement has no visible label');
                            check(cell.scrollWidth <= cell.clientWidth + 1, 'Body measurement is clipped');
                        }
                    }
                    check(bodies.querySelector<HTMLElement>('[data-field="declination"]')!.textContent === 'S 008° 00.12' &&
                          bodies.querySelector<HTMLElement>('[data-field="magnitude"]')!.textContent === '-26.7',
                          'Responsive body layout changed native values');
                } else {
                    check(frame.contentWindow!.getComputedStyle(bodies.tBodies[0].rows[0]).display === 'table-row',
                          'Desktop body table was replaced by mobile entries');
                }
                check(region.clientWidth <= card && target.scrollWidth <= card + 1, 'Almanac spills outside its card');
                for (const cell of table.querySelectorAll<HTMLElement>('tbody td')) {
                    check(
                        cell.scrollWidth <= cell.clientWidth + 1,
                        `Event content overlaps its cell: ${cell.dataset.label}`);
                }
                const headerStyle = frame.contentWindow!.getComputedStyle(table.tHead!);
                if (viewport < 900) {
                    check(headerStyle.display === 'none', 'Mobile events are not stacked');
                    check(
                        region.scrollWidth <= region.clientWidth + 1,
                        'Mobile event cards require horizontal scrolling');
                } else {
                    check(headerStyle.display !== 'none', 'Desktop event headings are hidden');
                    if (card === 860)
                        check(region.scrollWidth > region.clientWidth, 'Wide event table has no contained scroll area');
                }
            }
            WorksheetResults.calendarResults.renderAlmanacWorksheet(target, {...fixture, visibility: 'visible'});
            check(
                target.querySelectorAll<HTMLElement>('.almanac-grid-table:not(.almanac-event-table) tbody tr').length === 1 &&
                    target.querySelector<HTMLElement>('.body-name')!.textContent === '☉Sun' && !target.querySelector<HTMLElement>('.visible-cell')!,
                'Local visible variant was filtered again or retained the visibility column');
            check(
                target.dataset.copyText === fixture.almanac_presentation.visible.copy_text &&
                    target.querySelector('[data-almanac-visibility="visible"]')?.getAttribute('aria-pressed') === 'true' &&
                    !target.textContent.includes('visible bodies only'),
                'Visible variant text was not used verbatim');
            WorksheetResults.calendarResults.renderAlmanacWorksheet(target, {...fixture, visibility: 'all', weather_refresh: true});
            check(target.dataset.copyText === fixture.almanac_presentation.visible.copy_text,
                'Background weather reset the current visible-body filter');
            WorksheetResults.calendarResults.renderAlmanacWorksheet(target, pending);
            const emptyBody = target.querySelector<HTMLElement>('.almanac-body-table td[colspan]')!;
            check(emptyBody?.textContent.includes('No bodies found') &&
                  emptyBody.scrollWidth <= emptyBody.clientWidth + 1,
                  'Empty body filter must retain a readable full-width message');
            check(
                target.querySelector<HTMLElement>('[data-almanac-land-totality]')?.dataset.almanacLandTotality === '2461619.9' &&
                    target.querySelector<HTMLElement>('.event-time[title]')!.textContent === 'Unavailable',
                'Native land-search marker or unavailable compact-time text was reinterpreted');
            WorksheetResults.calendarResults.renderDatetimeSections(target, null, sections, 'native copy text');
            const details = target.querySelectorAll('details');
            check(details.length === 2 && !details[0].open && details[1].open, 'Native section expansion changed');
            check(!target.querySelector<HTMLElement>('script,img')!, 'Native section text became executable markup');
            check(
                details[0].querySelector('summary')!.textContent === '<script> & calendar' &&
                    details[0].querySelector<HTMLElement>('.datetime-row-value')!.textContent === 'quoted & "text"\rμ' &&
                    details[0].querySelectorAll<HTMLElement>('.datetime-row').length === 2 &&
                    details[0].textContent.includes('unavailable'),
                'Native section escaping, blank rows or empty-value policy changed');
            check(target.dataset.fullText === 'native copy text', 'Native section clipboard companion was lost');
            let missingDateRejected = false;
            try {
                WorksheetResults.calendarResults.renderDatetimeSections(
                    target, null, [{title: 'Old server', rows: [{label: 'Date', value: 'today'}]}], 'Date: today');
            } catch (caught: unknown) {
            const error = caught as Error;
                missingDateRejected = error.message.includes('DateTime presentation');
            }
            check(
                missingDateRejected && target.querySelectorAll('details').length === 2 &&
                    target.dataset.fullText === 'native copy text',
                'Incomplete DateTime response replaced structured sections with plain text');
            WorksheetResults.calendarResults.renderDatetimeSections(target, null, [], '<fallback>');
            check(
                target.textContent === '<fallback>' && !target.children.length,
                'Empty sections lost the text fallback');
        } finally {
            frame.remove();
            WorksheetContext.worksheetSession.calendar.almanacVisibilityMode = visibility;
        }
    }
    /** Exercise calendar edits and navigation through the real native history and control adapters. */
    async function checkCalendarHistoryNavigation(): Promise<void> {
        const savedFetch = window.fetch;
        const savedGlobeFetch = labHttp.fetch;
        labHttp.fetch = (url, options) => url === '/almanac-globe' || url === '/almanac-rivers' ?
            Promise.reject(new Error('History camera fixture: no regional imagery')) : savedGlobeFetch.call(labHttp, url, options);
        const savedMode = WorksheetWorkspace.mode.currentMode();
        // History must work even when an evaluation fails, without contacting astronomy/weather providers.
        WorksheetWorkspace.presentation.saveCurrentModeResultState();
        const modes = WorksheetWorkspace.WORKSPACE_MODE_NAMES;
        const savedResults = modes.map(mode => WorksheetWorkspace.presentation.resultStates[mode]);
        window.fetch = (url, options) => /^\/(almanac|datetime)-eval$/.test(String(url)) ?
            Promise.resolve(new Response(labWire.encode({ok: false, error: 'History fixture'}),
                {status: 422, headers: {'Content-Type': 'application/x-protobuf'}})) : savedFetch(url, options);
        try {
            for (const mode of ['almanac', 'datetime'] as const) {
                WorksheetWorkspace.mode.setMode(mode);
                WorksheetWorkspace.presentation.syncModeUI();
                await new Promise(requestAnimationFrame);
                await new Promise(requestAnimationFrame);
                const history = WorksheetPersistence.history;
                const saved = history.snapshot();
                const cursors = [history.cursor(0), history.cursor(1)];
                const retained = cursors.map(cursor => {
                    const entries: WorksheetSnapshot[] = [];
                    while (cursor.length) entries.push(cursor.pop()!);
                    return entries;
                });
                const latitude = document.getElementById(mode + 'Latitude') as HTMLInputElement;
                const longitude = document.getElementById(mode + 'Longitude') as HTMLInputElement;
                const elevation = document.getElementById(mode + 'Elevation') as HTMLInputElement;
                const town = document.getElementById(mode + 'Town') as WorksheetSelect;
                const back = document.getElementById('back') as HTMLButtonElement;
                const forward = document.getElementById('forward') as HTMLButtonElement;
                const evaluate = mode === 'almanac' ? WorksheetRequests.evaluation.evaluateAlmanac :
                    WorksheetRequests.evaluation.evaluateDatetime;
                const automatic = mode === 'almanac' ? WorksheetLocations.calendarActions.triggerAlmanacAutoEvaluation :
                    WorksheetLocations.calendarActions.triggerDatetimeAutoEvaluation;
                const globe = mode === 'almanac' ? Reflect.get(MarsAlmanacGlobe, 'instance') as {
                    view: number[]; orientation: number[]; flightTarget: number[] | null;
                    applyCamera(camera: number[]): void;
                    stopRegionRequests(): void;
                } : null;
                const savedCamera = globe ? [...globe.orientation, globe.view[2]] : null;
                const camera = (zoom: number, roll: number): number[] => {
                    document.querySelector<HTMLButtonElement>('[data-globe-action="centre"]')!.click();
                    const orientation = labDOM.call('lab_globe_rotate',
                        labDOM.call('lab_globe_centre', Number(latitude.value), Number(longitude.value)),
                        1, 0, 0, roll) as number[];
                    const value = [...orientation, zoom];
                    globe!.applyCamera(value);
                    return value;
                };
                const settledCamera = async (expected: number[], zoom = expected[4]): Promise<void> => {
                    if (globe!.flightTarget && !matchMedia('(prefers-reduced-motion: reduce)').matches) {
                        await new Promise(resolve => setTimeout(resolve, 1200));
                        check(globe!.view[2] > 1, 'nearby history navigation does not retreat to the whole-Earth overview');
                    }
                    for (let i = 0; i < 9000 && globe!.flightTarget; ++i)
                        await new Promise(resolve => setTimeout(resolve, 20));
                    check(!globe!.flightTarget && globe!.view[2] === zoom &&
                          globe!.orientation.every((value, i) => Math.abs(value - expected[i]) < 1e-12),
                          `history retains current zoom and restores saved roll: expected=${expected}, zoom=${zoom}, ` +
                          `actual=${globe!.orientation},${globe!.view[2]}, pending=${globe!.flightTarget}`);
                };
                try {
                    latitude.value = '51';
                    WorksheetLocations.townSelection.clearTownForCustomCoordinates(town, latitude, longitude, elevation);
                    let firstCamera = globe ? camera(12.5, .3) : null;
                    await evaluate({skipHistoryUpdate: true});
                    // Zoom and roll after the selection was committed, then depart without another evaluation.
                    if (globe) firstCamera = camera(43.75, .6);
                    check(history.backwardCount() === 0, mode + ': camera changes do not add history steps');
                    latitude.value = '52';
                    const secondCamera = globe ? camera(87.25, -.4) : null;
                    await automatic();
                    check(history.backwardCount() === 1 && !back.disabled && forward.disabled,
                          mode + ': automatic edit enables Back');
                    await WorksheetPersistence.navigation.navigate(0);
                    check(Number(latitude.value) === 51 && !forward.disabled && back.disabled,
                          `${mode}: Back restores coordinates and enables Forward: latitude=${latitude.value}, ` +
                          `back=${history.backwardCount()}/${back.disabled}, forward=${history.forwardCount()}/${forward.disabled}`);
                    if (firstCamera) await settledCamera(firstCamera, secondCamera![4]);
                    if (globe) {
                        firstCamera = camera(65.5, -.2);
                        check(history.forwardCount() === 1 && !forward.disabled,
                              'adjusting a restored camera retains the forward branch');
                    }
                    await WorksheetPersistence.navigation.navigate(1);
                    check(Number(latitude.value) === 52 && !back.disabled && forward.disabled,
                          mode + ': Forward restores the edited observer');
                    if (secondCamera) await settledCamera(secondCamera, firstCamera![4]);
                    await automatic();
                    check(history.backwardCount() === 1, mode + ': unchanged evaluation does not duplicate history');
                    await WorksheetPersistence.navigation.navigate(0);
                    if (globe) {
                        // A newer navigation must win without waiting for the first flight to finish.
                        await WorksheetPersistence.navigation.navigate(1);
                        await WorksheetPersistence.navigation.navigate(0);
                        await settledCamera(firstCamera!);
                    }
                    latitude.value = '53';
                    await automatic();
                    check(history.forwardCount() === 0 && forward.disabled && !back.disabled,
                          mode + ': a fresh edit discards the forward branch');
                    if (globe) {
                        MarsAlmanacGlobe.finishHistory(MarsAlmanacGlobe.beginHistory(), secondCamera, true);
                        document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!.click();
                        check(!globe.flightTarget, 'manual zoom interrupts a flight immediately');
                        const match = window.matchMedia;
                        const interruptedZoom = globe.view[2];
                        try {
                            window.matchMedia = query => query === '(prefers-reduced-motion: reduce)' ?
                                {matches: true} as MediaQueryList : match.call(window, query);
                            MarsAlmanacGlobe.finishHistory(MarsAlmanacGlobe.beginHistory(), firstCamera, true);
                            check(!globe.flightTarget && globe.view[2] === interruptedZoom,
                                  'reduced motion retains current zoom immediately without a flight');
                        } finally { window.matchMedia = match; }
                        const country = document.getElementById('almanacJurisdiction') as HTMLSelectElement;
                        country.value = 'GB-ENG';
                        await automatic({refreshJurisdiction: true, refreshCoordinates: true});
                        camera(12.5, .3);
                        await evaluate({skipHistoryUpdate: true});
                        // Catalogue hydration can replace a town key without changing its geographic observer.
                        const selectedTown = town.selectedOptions[0];
                        const originalTownKey = selectedTown.value;
                        selectedTown.value = originalTownKey + '|catalogue-refresh';
                        document.dispatchEvent(new Event('mars-almanac-observer-committed'));
                        document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!.click();
                        const closeUp = [...globe.orientation, globe.view[2]];
                        check(JSON.stringify(history.previous(null)?.globeCamera) === JSON.stringify(closeUp),
                              'manual zoom immediately updates the committed camera despite a refreshed town key');
                        for (const identity of ['invalid', JSON.stringify(['0', '0', '', 'GB-ENG']),
                            JSON.stringify([latitude.value, longitude.value, town.value, 'US'])]) {
                            history.rememberGlobeCamera(identity, [0, 0, 0, 1, 1]);
                            check(JSON.stringify(history.previous(null)?.globeCamera) === JSON.stringify(closeUp),
                                  'invalid or geographically different observer identities cannot overwrite the camera');
                        }
                        country.value = 'US';
                        country.dispatchEvent(new Event('change', {bubbles: true}));
                        for (let i = 0; i < 200; ++i) {
                            const committed = history.previous(null);
                            if ((committed?.almanac as Record<string, string> | undefined)?.jurisdiction === 'US' &&
                                Number(longitude.value) < -70) break;
                            await new Promise(resolve => setTimeout(resolve, 20));
                        }
                        check((history.previous(null)?.almanac as Record<string, string> | undefined)?.jurisdiction === 'US',
                              'country dropdown commits its asynchronously resolved destination');
                        await WorksheetPersistence.navigation.navigate(0);
                        await settledCamera(closeUp);
                        check(country.value === 'GB-ENG', 'country dropdown Back restores the departing country');
                        selectedTown.value = originalTownKey;
                    }
                } finally {
                    await WorksheetPersistence.navigation.restore(saved);
                    if (globe && savedCamera) {
                        document.querySelector<HTMLButtonElement>('[data-globe-action="centre"]')!.click();
                        globe.stopRegionRequests();
                        globe.applyCamera(savedCamera);
                    }
                    for (let side = 0; side < cursors.length; ++side) {
                        while (cursors[side].length) cursors[side].pop();
                        for (const entry of retained[side].reverse()) cursors[side].push(entry);
                    }
                    history.commit();
                }
            }
        } finally {
            window.fetch = savedFetch;
            labHttp.fetch = savedGlobeFetch;
            WorksheetWorkspace.mode.setMode(savedMode);
            WorksheetWorkspace.presentation.syncModeUI();
            modes.forEach((mode, index) => WorksheetWorkspace.presentation.resultStates[mode] = savedResults[index]);
            WorksheetWorkspace.presentation.restoreModeResultState(savedMode);
            WorksheetWorkspace.presentation.updateHistoryButtons();
        }
    }

    /** Exercise incremental publication without altering the installed catalogue or live menu contents. */
    async function checkTownPublicationScheduling(): Promise<void> {
        const app = new LabApplication();
        const fixture = app as unknown as {
            nativeCatalogue: LabCatalogue;
            publishTowns(towns: LabTownCatalogue): Promise<void>;
        };
        fixture.nativeCatalogue = {available: true, options: [], towns: {GB: []}};
        const selection = WorksheetLocations.townSelection;
        const saved = {
            towns: selection.townsForJurisdiction,
            populate: selection.populateTownSelect,
            coordinates: selection.selectTownByCoordinates,
            sync: WorksheetLocations.selects.syncSelection,
            evaluate: WorksheetWorkspace.mode.evaluateCurrentMode,
            mode: WorksheetWorkspace.mode.currentMode
        };
        const observerTown = WorksheetContext.almanacTown;
        const observerChildren = Array.from(observerTown.childNodes), observerIndex = observerTown.selectedIndex;
        const row = (value: string): LabTown => ({
            name: value, value, latitude: 'invalid', longitude: 'invalid', elevation: '0',
            timezone: 'Europe/London', default: false, detail: ''
        });
        let builds = 0, active = 0, peak = 0, inputHandled = false;
        try {
            // Both selectors resolve the same native-selected list (also models a country fallback).
            selection.townsForJurisdiction = () => fixture.nativeCatalogue.towns.GB;
            selection.populateTownSelect = async () => {
                check(inputHandled, 'town publication yields to queued browser work before rebuilding');
                ++builds;
                peak = Math.max(peak, ++active);
                await new Promise(resolve => setTimeout(resolve, 0));
                --active;
                return [];
            };
            selection.selectTownByCoordinates = () => true;
            WorksheetLocations.selects.syncSelection = () => {};
            await fixture.publishTowns({FR: [row('unrelated')]});
            check(builds === 0, 'unrelated countries never rebuild selected town menus');
            setTimeout(() => { inputHandled = true; }, 0);
            await Promise.all([
                fixture.publishTowns({GB: [row('first')]}),
                fixture.publishTowns({GB: [row('second')]})
            ]);
            check(Number(builds) === 2 && peak === 1,
                  'overlapping batches coalesce into one serial refresh of each affected menu');
            await fixture.publishTowns({GB: [row('first')]});
            check(Number(builds) === 2 && (labDOM.call('lab_town_store_list', 'GB') as LabTown[])
                .filter(town => town.value === 'first' || town.value === 'second').length === 2,
                  'duplicate batches neither rebuild menus nor duplicate towns');
            await fixture.publishTowns({GB: [row('third')]});
            check(Number(builds) === 4, 'later arrivals start a fresh refresh after the queue drains');
            let evaluations = 0;
            WorksheetWorkspace.mode.currentMode = () => 'almanac';
            WorksheetWorkspace.mode.evaluateCurrentMode = async options => {
                check(options?.skipHistoryUpdate, 'catalogue resolution does not append history');
                ++evaluations;
            };
            observerTown.value = '';
            selection.selectTownByCoordinates = select => {
                if (select === observerTown) {
                    observerTown.append(new Option('Recovered town', 'recovered-town'));
                    observerTown.value = 'recovered-town';
                }
                return true;
            };
            await fixture.publishTowns({GB: [row('fourth')]});
            check(evaluations === 1, 'a town resolved after startup refreshes the active worksheet');
            await fixture.publishTowns({GB: [row('fifth')]});
            check(evaluations === 1, 'later pages do not repeatedly evaluate an already named observer');
        } finally {
            selection.townsForJurisdiction = saved.towns;
            selection.populateTownSelect = saved.populate;
            selection.selectTownByCoordinates = saved.coordinates;
            WorksheetLocations.selects.syncSelection = saved.sync;
            WorksheetWorkspace.mode.evaluateCurrentMode = saved.evaluate;
            WorksheetWorkspace.mode.currentMode = saved.mode;
            observerTown.replaceChildren(...observerChildren);
            observerTown.selectedIndex = observerIndex;
        }
    }

    async function checkCountrySelectionReadiness() {
        const original = labApplication.prepareCountrySelection;
        let release!: () => void;
        let calls = 0;
        labApplication.prepareCountrySelection = () => new Promise<void>(resolve => { release = resolve; });
        const actions = new WorksheetLocations.CalendarActionController({
            flow: () => ({continue: async () => { ++calls; }})
        });
        try {
            const selection = actions.refreshAlmanacJurisdictionLocation();
            check(calls === 0, 'country default selection waits for its first town page');
            release();
            await selection;
            check(Number(calls) === 1, 'country default selection resumes after towns arrive');
            const obsolete = actions.refreshAlmanacJurisdictionLocation();
            document.getElementById('almanacLatitude')!.dispatchEvent(new Event('input', {bubbles: true}));
            release();
            await obsolete;
            check(Number(calls) === 1, 'late country towns cannot replace a newer authored position');
            await actions.refreshAlmanacJurisdictionLocation({updateCoordinates: false});
            check(Number(calls) === 2, 'point selection does not wait for country towns');
        } finally {
            labApplication.prepareCountrySelection = original;
        }
    }

    window.addEventListener('load', async function runBrowserChecks() {
        try {
            check(window.labReady instanceof Promise, 'Startup promise missing');
            if (catalogueCheck) {
                await window.labReady;
                check(!((document.getElementById('run')! as HTMLButtonElement).disabled), 'Town loading blocks maths');
                check(String(WorksheetContext.value.textContent).trim() === '5', 'Maths evaluates before towns arrive');
                check(MarsAlmanacGlobe.coordinate(51.99376, 'N', 'S') === '51° 59.63′ N', 'Decimal-minute latitude');
                check(MarsAlmanacGlobe.coordinate(-4.97631, 'E', 'W') === '4° 58.58′ W', 'Decimal-minute longitude');
                check(MarsAlmanacGlobe.coordinate(89.999999, 'N', 'S') === '90° N', 'Minutes carry into degrees');
                check(MarsAlmanacGlobe.coordinate(0, 'E', 'W') === '0°', 'Zero coordinate');
                if (catalogueCheck === 'success' || catalogueCheck === 'fail') {
                    check(document.querySelector('[data-jurisdiction-notice]:not(.hidden)') === null,
                        'Routine town loading stays silent while a country page is pending');
                    // Each navigation starts a new catalogue; establish the retained row before inducing page failure.
                    await labApplication.loadNearbyTowns(53.319, -3.4916);
                    check((labDOM.call('lab_town_store_list', 'GB-WLS') as LabTown[] | null)?.[0].name === 'Rhyl',
                        'Local towns appear while selected-jurisdiction loading is still blocked');
                }
                releaseTowns();
                await labApplication.townsReady;
                if (catalogueCheck === 'success') {
                    check(JSON.stringify(WorksheetLocations.townSelection.townsForJurisdiction('GB-WLS')) ===
                          JSON.stringify(labDOM.call('lab_town_store_list', 'GB-WLS')),
                          'native town resolution projects the shared catalogue for incremental menu updates');
                    await checkTownPublicationScheduling();
                    await checkCountrySelectionReadiness();
                    check((labDOM.call('lab_town_store_list', 'GB-WLS') as LabTown[])[0].name === 'Rhyl', 'Background towns not published');
                    check((labDOM.call('lab_town_store_list', 'GB-WLS') as LabTown[]).length === 1, 'Local/country pages duplicated a town');
                    location.href = '/?catalogue-check=fail';
                } else if (catalogueCheck === 'fail') {
                    check((labDOM.call('lab_town_store_list', 'GB-WLS') as LabTown[] | null)?.[0].name === 'Rhyl', 'Later failure discarded loaded towns');
                    check(document.querySelector('[data-jurisdiction-notice]:not(.hidden)') !== null, 'Town error not shown');
                    location.href = '/?catalogue-check=index-fail';
                } else {
                    check(!labApplication.catalogue.available, 'Failed metadata did not fall back');
                    await report('PASS: bounded town loading, independent maths startup and catalogue failure recovery');
                }
                return;
            }
            if (failureCase === 'database' || failureCase === 'catalogue') {
                await window.labReady;
                check(
                    (document.getElementById('datetimeJurisdiction')! as HTMLSelectElement).disabled,
                    'Unavailable database leaves menu enabled');
                check(document.querySelector<HTMLElement>('[data-jurisdiction-notice]')!, 'Unavailable database notice missing');
                check(
                    String(WorksheetContext.value.textContent).trim() === '7',
                    `Unavailable database blocked ordinary maths: mode=${WorksheetWorkspace.mode.currentMode()}, input=${WorksheetContext.expr.value}, ` +
                        `value=${WorksheetContext.value.textContent}, rendered=${WorksheetContext.rendered.textContent}`);
                if (failureCase === 'catalogue') {
                    location.href = '/?failure=script';
                    return;
                }
                await report(
                    'PASS: startup, native evaluation, editor, modes, cards, dates, state, mobile, failure recovery');
                return;
            }
            if (failureCase) {
                await window.labReady.catch(() => {});
                check(
                    document.getElementById('status')!.textContent === 'Startup failed',
                    'Missing startup asset not reported');
                check((document.getElementById('run')! as HTMLButtonElement).disabled, 'Failed startup leaves Evaluate enabled');
                if (failureCase === 'server-missing' || failureCase === 'server-mismatch') {
                    check(
                        document.getElementById('rendered')!.textContent.includes('make mars-lab-restart'),
                        'Server mismatch does not explain how to restart');
                    check(
                        !document.querySelector<HTMLElement>('script[src="/js/workspace.js"]')!,
                        'Incompatible server reached worksheet initialisation');
                    location.href =
                        failureCase === 'server-missing' ? '/?failure=server-mismatch' : '/?failure=database';
                    return;
                }
                check(
                    document.getElementById('rendered')!.textContent.includes('/js/'), 'Missing script not identified');
                location.href = '/?failure=server-missing';
                return;
            }
            await window.labReady;
            await labApplication.townsReady;
            if (startupCheck) {
                check(WorksheetWorkspace.mode.currentMode() === 'almanac' && startupReveals.join(',') === 'almanac',
                      `first restored mode: ${WorksheetWorkspace.mode.currentMode()}; reveals: ${startupReveals.join(',')}; ` +
                      `pending: ${document.documentElement.getAttribute('data-starting')}; requests: ${startupRequests.join(',')}`);
                check(!startupRequests.some(url => /\/(eval|equation-eval|diffequation-eval|matrix-eval|integrator-eval)(?:\?|$)/.test(url)),
                      'Almanac startup never evaluates an unrelated mathematical mode');
                check(!document.documentElement.hasAttribute('data-starting'), 'saved mode becomes visible after restoration');
                check(startupRequests.some(url => /\/almanac-eval(?:\?|$)/.test(url)),
                      'the restored Almanac receives its own initial evaluation');
                const savedCamera = sessionStorage.getItem('startup-globe-camera');
                if (!savedCamera) {
                    const zoom = document.querySelector<HTMLButtonElement>('[data-globe-action="in"]')!;
                    for (let i = 0; i < 3; ++i) {
                        zoom.click();
                        await new Promise(requestAnimationFrame);
                    }
                    const canvas = document.querySelector<HTMLCanvasElement>('#almanacGlobe canvas')!;
                    const capture = canvas.setPointerCapture;
                    canvas.setPointerCapture = () => {};
                    try {
                        const box = canvas.getBoundingClientRect();
                        const x = box.left + box.width / 2, y = box.top + box.height / 2;
                        canvas.dispatchEvent(new PointerEvent('pointerdown', {pointerId: 82, button: 0, clientX: x, clientY: y}));
                        canvas.dispatchEvent(new PointerEvent('pointermove', {pointerId: 82, clientX: x + 30, clientY: y + 15}));
                        canvas.dispatchEvent(new PointerEvent('pointerup', {pointerId: 82}));
                    } finally { canvas.setPointerCapture = capture; }
                    const camera = MarsAlmanacGlobe.captureHistory();
                    check(Boolean(camera && camera[4] > 1), 'startup fixture zooms the actual globe');
                    const latitude = document.getElementById('almanacLatitude') as HTMLInputElement;
                    const previousLatitude = latitude.value;
                    latitude.value = '12.345';
                    window.dispatchEvent(new PageTransitionEvent('pagehide', {persisted: true}));
                    latitude.value = previousLatitude;
                    const bookmark = JSON.parse(localStorage.getItem('mars.exprLab.globeCamera.v1') || 'null');
                    check(Boolean(bookmark && camera!.every((value, i) => Math.abs(value - bookmark.camera[i]) < 1e-10)),
                          'exit flush saves the displayed camera before debounce even while observer fields differ');
                    window.addEventListener('pagehide', () => {
                        const record = JSON.parse(localStorage.getItem('mars.exprLab.globeCamera.v1')!);
                        record.observer = JSON.stringify(['12.345', '67.890', '', '']);
                        localStorage.setItem('mars.exprLab.globeCamera.v1', JSON.stringify(record));
                    }, {once: true});
                    sessionStorage.setItem('startup-globe-camera', JSON.stringify(camera));
                    location.reload();
                    return;
                }
                const expectedCamera = JSON.parse(savedCamera) as number[];
                const restoredCamera = MarsAlmanacGlobe.captureHistory();
                check(Boolean(restoredCamera && expectedCamera.every((value, i) => Math.abs(value - restoredCamera[i]) < 1e-10)),
                      `a real page reload restores the globe orientation and zoom: expected ${savedCamera}; ` +
                      `actual ${JSON.stringify(restoredCamera)}; stored ${localStorage.getItem('mars.exprLab.globeCamera.v1')}; ` +
                      `observer ${JSON.stringify(['Latitude', 'Longitude', 'Town', 'Jurisdiction'].map(name =>
                          (document.getElementById('almanac' + name) as HTMLInputElement).value))}`);
                sessionStorage.removeItem('startup-globe-camera');
                initialiseLabTestAdapters();
                WorksheetWorkspace.mode.setMode('expression');
                try { await window.checkLabStateFlow(); }
                finally { WorksheetWorkspace.mode.setMode('almanac'); }
                await report('PASS: saved Almanac is the first visible and evaluated mode');
                return;
            }
            initialiseLabTestAdapters();
            if (new URLSearchParams(location.search).get('evaluation-check') === '1') {
                await runBrowserGroup('checkLabEvaluation', window.checkLabEvaluation);
                await runBrowserGroup('checkLabEvaluationInstall', window.checkLabEvaluationInstall);
                await runBrowserGroup('checkLabEvaluationCards', window.checkLabEvaluationCards);
                await runBrowserGroup('checkLabWeather', window.checkLabWeather);
                await runBrowserGroup('checkLabLocationFlow', window.checkLabLocationFlow);
                await runBrowserGroup('checkLabLocationDom', window.checkLabLocationDom);
                await report('PASS: asynchronous result installation, drawing opportunities and stale-request rejection');
                return;
            }
            if (new URLSearchParams(location.search).get('history-check') === '1') {
                await runBrowserGroup('checkLabRequests', window.checkLabRequests);
                await runBrowserGroup('checkLabLayout', window.checkLabLayout);
                await runBrowserGroup('checkLabWorkspaceDom', window.checkLabWorkspaceDom);
                await runBrowserGroup('checkLabLocationDom', window.checkLabLocationDom);
                await runBrowserGroup('checkCalendarHistoryNavigation', checkCalendarHistoryNavigation, 600000);
                await runBrowserGroup('checkLabHistoryPersistence', window.checkLabHistoryPersistence, 120000);
                await report('PASS: persistent per-browser history, all modes, reopening, revisions and size limits');
                return;
            }
            if (new URLSearchParams(location.search).get('select-check') === '1') {
                await runBrowserGroup('real DateTime sky header', async () => {
                    const originalMode = WorksheetWorkspace.mode.currentMode();
                    await labApplication.refreshHomeSky();
                    WorksheetWorkspace.mode.setMode('datetime');
                    WorksheetWorkspace.presentation.syncModeUI();
                    const response = await labHttp.fetch('/datetime-eval', {
                        method: 'POST',
                        body: labWire.encode({date: '2026-10-22', start: '2026-01-01', end: '2027-01-01',
                            year: '2026', latitude: '52.7077', longitude: '-2.7541', gmt_offset: '1',
                            elevation: '75', jurisdiction: 'GB-ENG'})
                    });
                    const data = await response.labData() as LabTestCalendarEvaluation;
                    check(response.ok && data.ok, 'Real DateTime response succeeds');
                    await WorksheetRequests.evaluation.installEvaluationResult(5, data);
                    WorksheetWorkspace.presentation.setStatus('Ready');
                    const home = document.getElementById('homeSky')!;
                    check(Boolean(home) && WorksheetContext.statusEl.innerHTML === home.innerHTML,
                          'Real DateTime response shows the independent home header');
                    const homeMarkup = home.innerHTML;
                    await window.checkLabMobileLayout('DateTime home header spacing', true);
                    WorksheetWorkspace.mode.setMode(originalMode);
                    WorksheetWorkspace.presentation.syncModeUI();
                    delete WorksheetWorkspace.presentation.resultStates.datetime;
                    await WorksheetWorkspace.mode.selectWorksheetMode('datetime');
                    check(WorksheetContext.statusEl.innerHTML === homeMarkup,
                          'DateTime mode entry preserves the independent home header');
                    window.scrollTo(0, 0);
                    await WorksheetWorkspace.mode.selectWorksheetMode('almanac');
                    check(window.scrollY < 1 && document.activeElement === WorksheetContext.almanacDate,
                          'Entering Almanac focuses its date without scrolling below the globe');
                    check(WorksheetContext.statusEl.innerHTML === homeMarkup,
                          'Almanac uses the same home origin rather than its selected observer');
                    await window.checkLabMobileLayout('Almanac home header spacing', true);
                    WorksheetWorkspace.mode.setMode(originalMode);
                    WorksheetWorkspace.presentation.syncModeUI();
                });
                await runBrowserGroup('checkLabWeather', window.checkLabWeather);
                await runBrowserGroup('checkLabWorkspaceDom', window.checkLabWorkspaceDom);
                await runBrowserGroup('checkLabLocationFlow', window.checkLabLocationFlow);
                await runBrowserGroup('checkLabLocationDom', window.checkLabLocationDom);
                await runBrowserGroup('checkCalendarHistoryNavigation', checkCalendarHistoryNavigation, 600000);
                await runBrowserGroup('checkLabHistoryPersistence', window.checkLabHistoryPersistence, 120000);
                await runBrowserGroup('checkLabSelects', window.checkLabSelects);
                await runBrowserGroup('checkLabSelectEvents', window.checkLabSelectEvents);
                await runBrowserGroup('checkLabAlmanacEvents', window.checkLabAlmanacEvents);
                await runBrowserGroup('checkLabSurfaceMath', window.checkLabSurfaceMath, 1200000);
                await report('PASS: calendar Back/Forward, selection menus, 5000-town selection and globe busy-state interactions');
                return;
            }
            if (new URLSearchParams(location.search).get('globe-check') === '1') {
                await runBrowserGroup('checkLabSurfaceMath', window.checkLabSurfaceMath, 1200000);
                await report('PASS: globe projection, picking and water-preserving photographic colours; ' +
                    (document.getElementById('almanacGlobe')!.dataset.flightTestTiming || ''));
                return;
            }
            if (labProfile.enabled) {
                await report('PASS: PROFILE ' + JSON.stringify(await labProfile.run()));
                return;
            }
            await runBrowserGroup('wire', checkWire);
            await runBrowserGroup('workers', () => labCheckWorkers(check));
            check(
                (await nativeFetch('/js/worksheet.js')).status === 404 && !labApplication.definitionScripts.includes('worksheet') &&
                    !document.querySelector<HTMLElement>('script[src="/js/worksheet.js"]')!,
                'Retired worksheet script remains served or loaded');
            check(
                (await nativeFetch('/js/evaluation.js')).status === 404 &&
                    !labApplication.definitionScripts.includes('evaluation') &&
                    !document.querySelector<HTMLElement>('script[src="/js/evaluation.js"]')!,
                'Retired evaluation script remains served or loaded');
            await runBrowserGroup('labWorkspaceChecks', window.labWorkspaceChecks);
            await runBrowserGroup('checkLabRequests', window.checkLabRequests);
            await runBrowserGroup('checkLabEvaluation', window.checkLabEvaluation);
            await runBrowserGroup('checkLabEvaluationCards', window.checkLabEvaluationCards);
            await runBrowserGroup('checkLabResultFlows', window.checkLabResultFlows);
            await runBrowserGroup('checkLabBindingFlows', window.checkLabBindingFlows);
            await runBrowserGroup('checkLabLocationFlow', window.checkLabLocationFlow);
            await runBrowserGroup('checkCalendarHistoryNavigation', checkCalendarHistoryNavigation, 600000);
            await runBrowserGroup('checkLabHistoryPersistence', window.checkLabHistoryPersistence, 120000);
            await runBrowserGroup('checkLabRequestFlows', window.checkLabRequestFlows);
            await runBrowserGroup('checkLabStateFlow', window.checkLabStateFlow);
            await runBrowserGroup('checkLabSurfaceMath', window.checkLabSurfaceMath, 1200000);
            await runBrowserGroup('checkSurfaceProjectionPlan', checkSurfaceProjectionPlan);
            await runBrowserGroup('checkLabSurfaceMesh', window.checkLabSurfaceMesh);
            await runBrowserGroup('checkLabSurfaceSVG', window.checkLabSurfaceSVG);
            await runBrowserGroup('checkLabSurfaceHover', window.checkLabSurfaceHover);
            await runBrowserGroup('checkLabTransportFlows', window.checkLabTransportFlows);
            window.checkLabEditor();
            await runBrowserGroup('checkRenderedGlyphs', checkRenderedGlyphs);
            await runBrowserGroup('checkLabForms', window.checkLabForms);
            await runBrowserGroup('checkLabCalendarState', window.checkLabCalendarState);
            await runBrowserGroup('checkLabPersistence', window.checkLabPersistence);
            await runBrowserGroup('checkLabStorage', window.checkLabStorage);
            window.checkLabWidgets();
            await runBrowserGroup('checkLabSelects', window.checkLabSelects);
            await runBrowserGroup('checkLabSelectEvents', window.checkLabSelectEvents);
            await runBrowserGroup('checkLabLayout', window.checkLabLayout);
            await runBrowserGroup('checkLabSyntax', window.checkLabSyntax);
            await runBrowserGroup('checkLabProjection', window.checkLabProjection);
            await runBrowserGroup('checkRenderedGlyphs', checkRenderedGlyphs);
            await runBrowserGroup('checkLabWorkspaceDom', window.checkLabWorkspaceDom);
            await runBrowserGroup('checkRenderedGlyphs', checkRenderedGlyphs);
            await runBrowserGroup('checkLabLocationDom', window.checkLabLocationDom);
            await runBrowserGroup('checkLabResultDom', window.checkLabResultDom);
            await runBrowserGroup('checkLabBindingDom', window.checkLabBindingDom);
            await runBrowserGroup('checkLabBindingSync', window.checkLabBindingSync);
            await runBrowserGroup('checkLabBindingCommit', window.checkLabBindingCommit);
            await runBrowserGroup('checkLabFunction', window.checkLabFunction);
            await runBrowserGroup('checkLabEvaluationInstall', window.checkLabEvaluationInstall);
            await runBrowserGroup('checkLabGoal', window.checkLabGoal);
            await runBrowserGroup('checkLabEvaluationSetup', window.checkLabEvaluationSetup);
            await runBrowserGroup('checkLabWeather', window.checkLabWeather);
            await runBrowserGroup('checkLabPayload', window.checkLabPayload);
            await runBrowserGroup('checkLabPickerDom', window.checkLabPickerDom);
            await runBrowserGroup('checkLabEvents', window.checkLabEvents);
            await runBrowserGroup('checkLabWidgetEvents', window.checkLabWidgetEvents);
            await runBrowserGroup('checkLabAlmanacEvents', window.checkLabAlmanacEvents);
            check(!failures.length, failures.join('; '));
            check(WorksheetWorkspace.mode.currentMode() === 'expression', 'Initial expression mode not restored');
            await runBrowserGroup('checkAlmanacLayout', checkAlmanacLayout);
            await runBrowserGroup('checkHelpLayout', checkHelpLayout);
            await window.checkLabMobileLayout('expression result');
            check(document.styleSheets.length > 0, 'External stylesheet not loaded');
            const stylesheet = Array.from(document.styleSheets).find(sheet => sheet.href?.endsWith('/index.css'))!;
            const styleNames = [
                'theme', 'layout', 'worksheets', 'dates', 'forms', 'bindings', 'mobile', 'buttons', 'results', 'help',
                'responsive'
            ];
            const imports = Array.from(stylesheet.cssRules);
            check(imports.length === styleNames.length, 'Stylesheet import count changed');
            imports.forEach((rule, index) => {
                check(
                    rule instanceof CSSImportRule && rule.href.endsWith(`/css/${styleNames[index]}.css`),
                    `Stylesheet cascade order changed at ${styleNames[index]}`);
                check(rule instanceof CSSImportRule && rule.styleSheet !== null && rule.styleSheet.cssRules.length > 0, `Stylesheet ${styleNames[index]} failed to load`);
            });
            check(
                getComputedStyle(document.documentElement).getPropertyValue('--code').trim(),
                'Theme variables missing');
            check(getComputedStyle((document.getElementById('run')! as HTMLButtonElement)).borderRadius === '999px', 'Button styling missing');
            check((labDOM.call('lab_town_store_list', 'GB-WLS') as LabTown[]).length > 0, 'Welsh catalogue unavailable');
            check(
                String(WorksheetContext.value.textContent).trim() === '5',
                `Initial calculation: ${WorksheetContext.value.textContent}; ${WorksheetContext.rendered.textContent}`);
            check(WorksheetContext.functionStyle.textContent.includes('expression'), 'Function card missing');
            await runBrowserGroup('checkRenderedGlyphs', checkRenderedGlyphs);
            await runBrowserGroup('checkFunctionRun', checkFunctionRun);

            const authored = '@S_{-inf}^x e^(-1/2(t-@mu)^2/@sigma^2) dt';
            WorksheetBindings.controller.setExpressionEditor(authored);
            check(WorksheetContext.expr.value === authored, 'Editor rewrote authored infinity or Greek tokens');
            {
                const plotRendered = (document.getElementById('plotRendered')! as LabTestPlotHost);
                WorksheetBindings.controller.setExpressionEditor('acos(sin(2-x*y)+z)');
                await WorksheetRequests.evaluation.evaluateExpression();
                await waitForAutomaticPlot();
                const slicePanel = document.getElementById('plotSlices')!;
                const slice = slicePanel.querySelector<HTMLElement>('.plot-slice')!;
                check(slice && slice.dataset.name === 'z' && !slicePanel.classList.contains('hidden'),
                      'A third free variable receives a plot-local slice panel');
                check(slice.querySelector<HTMLInputElement>('.slice-lower')!.value === '-2' &&
                      slice.querySelector<HTMLInputElement>('.slice-upper')!.value === '2', 'Native domain analysis selects the slice range');
                const sliceSource = WorksheetBindings.editor.currentExpressionText();
                const sliceSlider = slice.querySelector<HTMLInputElement>('.slice-slider')!;
                check(getComputedStyle(sliceSlider).appearance === 'none' &&
                      sliceSlider.getBoundingClientRect().height === 160,
                      'The slice slider uses the compact themed track rather than the browser default');
                check(getComputedStyle(slice.querySelector<HTMLInputElement>('.slice-value')!).color ===
                      getComputedStyle((document.getElementById('plotLower')! as HTMLInputElement)).color,
                      'Slice text fields share the MARS plot control colours');
                check(slicePanel.querySelector<HTMLElement>('.slice-choices')!.classList.contains('hidden'),
                      'A single extra variable needs no variable selector');
                check(slice.querySelector<HTMLElement>('.slice-auto')!.matches(':checked') &&
                      getComputedStyle(slice.querySelector<HTMLElement>('.slice-limits')!).display === 'none',
                      'Evaluate defaults to Auto with no visible From/To fields');
                async function waitForSlice(predicate: () => unknown) {
                    for (let n = 0; n < 600 && !predicate(); ++n)
                        await LabTestBrowser.wait(25);
                    check(predicate(), 'Slice response must settle: ' + WorksheetContext.statusEl.textContent);
                }
                sliceSlider.value = '625';
                sliceSlider.dispatchEvent(new Event('input', {bubbles: true}));
                await waitForSlice(() => plotRendered.querySelector<HTMLElement>('[data-plot-quality="preview"]')! &&
                                         slice.querySelector<HTMLInputElement>('.slice-value')!.value === '½');
                check(WorksheetBindings.editor.currentExpressionText() === sliceSource, 'Dragging a slice does not edit the worksheet bindings');
                sliceSlider.dispatchEvent(new Event('change', {bubbles: true}));
                await waitForSlice(() => plotRendered.querySelector<HTMLElement>('[data-plot-quality="full"]')!);
                check(plotRendered.querySelector<LabTestPlotTitle>('title')?.textContent.includes('z = ½'),
                      'The exported plot heading identifies its exact slice');
                const exactSlice = slice.querySelector<HTMLInputElement>('.slice-value')!;
                exactSlice.value = '1/3';
                exactSlice.dispatchEvent(new Event('change', {bubbles: true}));
                await waitForSlice(() => exactSlice.value === '⅓');
                check(slice.querySelector<HTMLInputElement>('.slice-position')!.value === '-1',
                      'The rounded slider handle cannot overwrite an exact typed value');
                await WorksheetRequests.plots.takePlot('x');
                check(exactSlice.value === '⅓', 'Redrawing preserves the exact slice rather than quantising it');
                slice.querySelector<HTMLElement>('.slice-manual')!.click();
                check(slice.querySelector<HTMLElement>('.slice-manual')!.matches(':checked') &&
                      getComputedStyle(slice.querySelector<HTMLElement>('.slice-limits')!).display !== 'none',
                      'Manual reveals the editable range');
                slice.querySelector<HTMLInputElement>('.slice-lower')!.value = '-4';
                slice.querySelector<HTMLInputElement>('.slice-upper')!.value = '4';
                await WorksheetRequests.plots.takePlot('x');
                slice.querySelector<HTMLElement>('.slice-auto')!.click();
                await waitForSlice(() => slice.querySelector<HTMLInputElement>('.slice-upper')!.value === '2');
                check(exactSlice.value === '⅓' && slice.querySelector<HTMLInputElement>('.slice-lower')!.value === '-2' &&
                      getComputedStyle(slice.querySelector<HTMLElement>('.slice-limits')!).display === 'none',
                      'Auto restores native limits and preserves a valid exact slice value');
                const stage = document.getElementById('plotStage')!;
                stage.style.width = '360px';
                await LabTestBrowser.paint();
                const chartBox = plotRendered.getBoundingClientRect(), panelBox = slicePanel.getBoundingClientRect();
                check(panelBox.left >= chartBox.right - 1 && panelBox.width <= 73 &&
                      getComputedStyle(sliceSlider).writingMode === 'vertical-lr',
                      'A narrow vertical slice rail stays beside the visible plot on a mobile-width card');
                stage.style.width = '';
                await WorksheetWorkspace.mode.selectWorksheetMode('matrix');
                await WorksheetWorkspace.mode.selectWorksheetMode('expression');
                check(document.querySelector<HTMLInputElement>('#plotSlices .slice-value')?.value === '⅓',
                      'Returning to the worksheet restores its accepted exact slice');
                WorksheetBindings.controller.setExpressionEditor('x+y+z+w');
                await WorksheetRequests.evaluation.evaluateExpression();
                await waitForAutomaticPlot();
                const choices = slicePanel.querySelectorAll<HTMLElement>('.slice-choice');
                const rows = slicePanel.querySelectorAll<HTMLElement>('.plot-slice');
                const checkedSlice = (index: number) => choices[index].matches(':checked') &&
                    [...choices].filter(radio => radio.matches(':checked')).length === 1 &&
                    [...choices].every(radio => !Object.hasOwn(radio, 'checked'));
                check(choices.length === 2 && checkedSlice(0) &&
                      [...rows].filter(row => getComputedStyle(row).display !== 'none').length === 1,
                      'Multiple parameters have radio choices but only one visible slider');
                check(choices[1].getBoundingClientRect().top >= choices[0].getBoundingClientRect().bottom &&
                      Math.abs(choices[1].getBoundingClientRect().left - choices[0].getBoundingClientRect().left) < 1 &&
                      getComputedStyle(slicePanel.querySelector<HTMLElement>('.slice-choices')!).display === 'block',
                      'Parameter radio buttons stack vertically in the narrow rail');
                const beforeSelection = plotRendered.querySelector('svg')!;
                choices[1].click();
                check(checkedSlice(1) && rows[0].classList.contains('hidden') &&
                      getComputedStyle(rows[0]).display === 'none' && getComputedStyle(rows[1]).display !== 'none' &&
                      plotRendered.querySelector('svg')! === beforeSelection,
                      'Selecting another parameter swaps the editor without resampling');
                const secondValue = rows[1].querySelector<HTMLInputElement>('.slice-value')!;
                secondValue.value = '1/3';
                secondValue.dispatchEvent(new Event('change', {bubbles: true}));
                await waitForSlice(() => secondValue.value === '⅓');
                check(checkedSlice(1) && !rows[1].classList.contains('hidden'),
                      'A plot response preserves the selected parameter');
                choices[0].click();
                check(checkedSlice(0) && rows[0].querySelector<HTMLInputElement>('.slice-value')!.value === '0' && secondValue.value === '⅓',
                      'Switching sliders preserves independent exact parameter values');
                choices[1].click();
                await WorksheetWorkspace.mode.selectWorksheetMode('matrix');
                await WorksheetWorkspace.mode.selectWorksheetMode('expression');
                check(slicePanel.querySelectorAll<HTMLElement>('.slice-choice')[1].matches(':checked') &&
                      slicePanel.querySelectorAll<HTMLElement>('.slice-choice:checked').length === 1 &&
                      slicePanel.querySelectorAll<HTMLElement>('.plot-slice:not(.hidden)').length === 1,
                      'Worksheet restoration preserves the selected parameter');
                let selectedRow = slicePanel.querySelector<HTMLElement>('.plot-slice:not(.hidden)')!;
                selectedRow.querySelector<HTMLElement>('.slice-manual')!.click();
                await WorksheetWorkspace.mode.selectWorksheetMode('matrix');
                await WorksheetWorkspace.mode.selectWorksheetMode('expression');
                selectedRow = slicePanel.querySelector<HTMLElement>('.plot-slice:not(.hidden)')!;
                check(selectedRow.querySelector<HTMLElement>('.slice-manual')!.matches(':checked'),
                      'Mode switching retains the parameter range choice');
                await WorksheetRequests.evaluation.evaluateExpression();
                await waitForAutomaticPlot();
                check([...slicePanel.querySelectorAll<HTMLElement>('.plot-slice')].every(row =>
                    row.querySelector<HTMLElement>('.slice-auto')!.matches(':checked') &&
                    getComputedStyle(row.querySelector<HTMLElement>('.slice-limits')!).display === 'none'),
                    'Evaluate resets all ranges to Auto even for unchanged input');
            }
            WorksheetBindings.controller.setExpressionEditor('Laplace(asin(-x^2),x,s)');
            await WorksheetRequests.evaluation.evaluateExpression();
            await waitForAutomaticPlot();
            check(document.getElementById('plotCard')!.classList.contains('hidden') &&
                  !document.getElementById('plotRendered')!.textContent,
                  'An unresolved transform removes the previous plot without requesting an automatic range');
            check(WorksheetContext.rendered.querySelector('svg'),
                  'An unresolved transform retains its symbolic result card');
            await WorksheetWorkspace.mode.selectWorksheetMode('matrix');
            await WorksheetWorkspace.mode.selectWorksheetMode('expression');
            check(document.getElementById('plotCard')!.classList.contains('hidden'),
                  'Returning to a symbolic-only result does not restore an obsolete plot');
            WorksheetBindings.controller.setExpressionEditor('Laplace(exp(-x),x,s)');
            await WorksheetRequests.evaluation.evaluateExpression();
            await waitForAutomaticPlot();
            check(!document.getElementById('plotCard')!.classList.contains('hidden') &&
                  document.getElementById('plotRendered')!.querySelector('svg'),
                  'A supported transform restores automatic plotting');
            WorksheetBindings.controller.setExpressionEditor('3+4');
            await WorksheetRequests.evaluation.evaluateExpression();
            check(String(WorksheetContext.value.textContent).trim() === '7', 'Second native calculation should equal 7');
            {
                const slider = WorksheetContext.precisionSlider;
                const savedBits = WorksheetWorkspace.precision.requestedPrecisionBits();
                const position = savedBits === 53 ? 4 : 0;
                const bits = labWire.exports().lab_workspace_precision_slider_bits(position);
                const request = WorksheetRequests.labRequests.latestMain();
                slider.value = String(position);
                slider.dispatchEvent(new Event('input', {bubbles: true}));
                check(WorksheetWorkspace.precision.requestedPrecisionBits() === savedBits,
                      'Precision drag previews without recalculating');
                slider.dispatchEvent(new Event('change', {bubbles: true}));
                for (let attempt = 0; attempt < 400; ++attempt) {
                    if (WorksheetRequests.labRequests.latestMain() !== request &&
                        !WorksheetRequests.labRequests.busy()) break;
                    await LabTestBrowser.wait(25);
                }
                check(WorksheetWorkspace.precision.requestedPrecisionBits() === bits &&
                      WorksheetRequests.labRequests.latestMain() !== request &&
                      !WorksheetRequests.labRequests.busy() && WorksheetContext.statusEl.textContent.includes(bits + ' bits'),
                      'Releasing precision slider commits its bits and completes a fresh native evaluation');
                await WorksheetWorkspace.precision.setWorksheetPrecision(savedBits);
            }
            {
                const savedBits = WorksheetWorkspace.precision.requestedPrecisionBits();
                const panel = document.getElementById('precisionCustomPanel')!;
                const toggle = document.getElementById('precisionCustomToggle')! as HTMLButtonElement;
                const input = document.getElementById('precisionCustomInput')! as HTMLInputElement;
                const apply = document.getElementById('precisionCustomApply')! as HTMLButtonElement;
                function edit(value: string) {
                    input.value = value;
                    input.dispatchEvent(new Event('input', {bubbles: true}));
                }
                toggle.click();
                check(!panel.classList.contains('hidden') && document.activeElement === input,
                      'ellipsis opens the custom precision editor and focuses its input');
                edit('10000');
                check(document.getElementById('precisionCustomPreview')!.textContent === 'Uses 33220 bits' &&
                      WorksheetWorkspace.precision.requestedPrecisionBits() === savedBits,
                      'custom precision previews its native conversion without committing');
                document.getElementById('precisionCustomBits')!.click();
                check(input.value === '33220', 'bits unit displays exact binary precision');
                document.getElementById('precisionCustomDigits')!.click();
                check(String(input.value) === '10000', 'unit round trips preserve the requested decimal precision');
                edit('78913');
                check(!apply.disabled, 'maximum custom decimal precision can be applied');
                document.getElementById('precisionCustomBits')!.click();
                edit('262144');
                check(!apply.disabled, 'maximum custom bit precision can be applied');
                edit('262145');
                check(apply.disabled && input.getAttribute('aria-invalid') === 'true',
                      'Apply is disabled above the custom bit precision limit');
                document.getElementById('precisionCustomDigits')!.click();
                for (const invalid of ['', '-1', '1e4', '1.5', '78914']) {
                    edit(invalid);
                    check(apply.disabled && input.getAttribute('aria-invalid') === 'true',
                          'invalid custom precision cannot be applied: ' + invalid);
                }
                input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape', bubbles: true, cancelable: true}));
                check(panel.classList.contains('hidden') && document.activeElement === toggle &&
                      WorksheetWorkspace.precision.requestedPrecisionBits() === savedBits,
                      'Escape cancels the draft and restores focus');
                toggle.click();
                edit('10000');
                const request = WorksheetRequests.labRequests.latestMain();
                input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter', bubbles: true, cancelable: true}));
                for (let attempt = 0; attempt < 400; ++attempt) {
                    if (WorksheetRequests.labRequests.latestMain() !== request && !WorksheetRequests.labRequests.busy()) break;
                    await LabTestBrowser.wait(25);
                }
                check(panel.classList.contains('hidden') &&
                      WorksheetWorkspace.precision.requestedPrecisionBits() === 33220 &&
                      WorksheetRequests.labRequests.latestMain() !== request && !WorksheetRequests.labRequests.busy() &&
                      WorksheetContext.precisionValue.textContent.startsWith('Custom:') &&
                      WorksheetContext.precisionSlider.value === WorksheetContext.precisionSlider.max,
                      'custom precision evaluates through the native workflow beyond the slider range');
                toggle.click();
                document.getElementById('precisionCustomCancel')!.click();
                check(WorksheetWorkspace.precision.requestedPrecisionBits() === 33220,
                      'Cancel preserves the applied custom precision');
                await WorksheetWorkspace.precision.setWorksheetPrecision(savedBits);
            }
            check(document.getElementById('plotSlices')!.classList.contains('hidden') &&
                  !document.getElementById('plotStage')!.classList.contains('has-slices'),
                  'No extra variables leaves no slice panel or reserved column');
            await runBrowserGroup('checkRenderedGlyphs', checkRenderedGlyphs);
            check(WorksheetPersistence.history.backwardCount() > 0, 'Evaluation history missing');

            WorksheetBindings.controller.setExpressionEditor('2*x');
            await WorksheetRequests.evaluation.evaluateExpression();
            check(WorksheetContext.rendered.dataset.displayTex!.includes('\\mkern'), 'Native multiplication spacing missing');
            await runBrowserGroup('checkRenderedGlyphs', checkRenderedGlyphs);
            await WorksheetRequests.plots.takeDerivative('x');
            check(!WorksheetContext.rendered.classList.contains('error'), 'Shared derivative action failed');
            check(String(WorksheetContext.value.textContent).trim() === '2', 'Native derivative card lost its exact numerical value');
            check(WorksheetContext.parsed.dataset.fullText === '2', 'Derivative expression was not taken from the native card');
            await WorksheetRequests.plots.takeIntegral('x');
            check(!WorksheetContext.rendered.classList.contains('error'), 'Shared integral action failed');
            check(WorksheetContext.parsed.dataset.fullText!.includes('x'), 'Integral card lost its native expression');
            check(WorksheetContext.functionStyle.dataset.fullText!.length > 0, 'Integral Function card missing');
            check(WorksheetContext.rendered.dataset.fullTex!.length > 0, 'Integral TeX metadata missing');
            await runBrowserGroup('checkDistributionPlot', checkDistributionPlot);
            await runBrowserGroup('checkComplexGrid', checkComplexGrid);
            await runBrowserGroup('checkLogarithmicCurveZoom', checkLogarithmicCurveZoom);
            await runBrowserGroup('checkAsynchronousPlot', checkAsynchronousPlot);
            const plotRendered = (document.getElementById('plotRendered')! as LabTestPlotHost);
            WorksheetBindings.controller.setExpressionEditor('asin(x)');
            await WorksheetRequests.evaluation.evaluateExpression();
            (document.getElementById('plotLower')! as HTMLInputElement).value = '';
            (document.getElementById('plotUpper')! as HTMLInputElement).value = '';
            await WorksheetRequests.plots.takePlot('x');
            check(!plotRendered.classList.contains('error') && !!plotRendered.querySelector<HTMLElement>('[data-series]')! &&
                  (document.getElementById('plotLower')! as HTMLInputElement).value === '' &&
                  (document.getElementById('plotUpper')! as HTMLInputElement).value === '',
                  'Blank bounds must plot the native real domain without replacing Automatic controls with numbers');
            WorksheetBindings.controller.setExpressionEditor('sin(x)');
            await WorksheetRequests.evaluation.evaluateExpression();
            (document.getElementById('plotLower')! as HTMLInputElement).value = '-@pi';
            (document.getElementById('plotUpper')! as HTMLInputElement).value = '@pi';
            await WorksheetRequests.plots.takePlot('x');
            check(!plotRendered.classList.contains('error'), 'Native plot request failed: ' +
                  plotRendered.textContent + ' / ' + document.getElementById('plotFeedback')!.textContent);
            check(!!plotRendered.querySelector<HTMLElement>('svg [data-series="0"]')!, 'Plot SVG was not displayed');
            check(document.querySelector<HTMLElement>('#plotCard .card-title > span')!.textContent === 'Plot', 'Plot card title missing');
            check(!!plotRendered.querySelector<SVGTextElement>('svg [data-plot-title] use')! && plotRendered.dataset.displayTex!.includes('\\sin'),
                  'Plot formula title must be rendered as TeX glyphs in the same SVG');
            check(!plotRendered.textContent.includes('Function plot'), 'Generic plot title must be removed');
            check(WorksheetContext.parsed.dataset.fullText!.includes('sin'), 'Plot lost its source expression');
            check(WorksheetResults.clipboard.copyTextForTarget('plot').includes('data-series') && !WorksheetContext.rendered.querySelector<HTMLElement>('[data-series]')!,
                  'Plot copy and mathematical rendering belong to independent cards');
            const plotCard = plotRendered.closest<HTMLElement>('.result-card')!;
            const plotExpand = plotCard.querySelector<HTMLElement>('[data-expand-card]')!;
            const nextLayout = () => LabTestBrowser.paint();
            setResultZoom(plotCard, 3);
            await nextLayout();
            await runBrowserGroup('checkPlotWheelZoom', checkPlotWheelZoom);
            await runBrowserGroup('checkPlotChoices', checkPlotChoices);
            checkPlotControls(false);
            check(document.getElementById('plotKindGroup')!.classList.contains('plot-options-card'),
                  'Expression plot type uses the compact settings card');
            await runBrowserGroup('checkLivePlotControls', checkLivePlotControls);
            await window.checkLabMobileLayout('curve plot');
            const curveDragTarget = plotRendered.querySelector<SVGPathElement>('[data-series] path')!;
            check(curveDragTarget.dispatchEvent(new PointerEvent('pointerdown',
                  {button: 0, buttons: 1, pointerId: 11, pointerType: 'touch', isPrimary: true,
                   bubbles: true, cancelable: true})) &&
                  !plotRendered.classList.contains('plot-dragging'), 'Curve plots must not enable surface rotation');
            check(getComputedStyle(plotRendered.querySelector('svg')!).touchAction === 'none',
                  'Curve plots reserve finger movement for panning');
            const formulaBox = plotRendered.querySelector<SVGTextElement>('[data-plot-title]')!.getBoundingClientRect();
            const axesBox = plotRendered.querySelector<SVGSVGElement>('[data-plot-chart] [data-plot-axes]')!.getBoundingClientRect();
            check(formulaBox.width > 0 && formulaBox.height > 0 && formulaBox.bottom < axesBox.top,
                  'Rendered formula title must sit above the plot axes without overlap');
            const figure = plotRendered.querySelector<HTMLElement>('[data-plot-figure]')!;
            const curve = figure.querySelector<SVGPathElement>('[data-series] path')!;
            check(getComputedStyle(figure).filter === 'none' && getComputedStyle(curve).stroke === 'rgb(120, 216, 203)',
                  'Plot curves must retain their teal colour, without the monochrome formula filter');
            check(getComputedStyle(figure.querySelector<SVGTextElement>('[data-plot-title] use')!).fill === 'rgb(230, 199, 120)',
                  'Plot formula title must retain its gold colour');
            check(!figure.querySelector('rect')! && !plotRendered.textContent.includes('Vertical range clipped'),
                  'Plot must have no surrounding box or clipping annotation');
            const chart = figure.querySelector<SVGSVGElement>('[data-plot-chart]')!;
            const edgePaths = [...chart.querySelectorAll<LabTestHoverPath>('[data-plot-axes] path')].map(path => path.getAttribute('d'));
            const axisLeft = Number(chart.dataset.plotLeft), axisTop = Number(chart.dataset.plotTop);
            const axisRight = axisLeft + Number(chart.dataset.plotWidth);
            const axisBottom = axisTop + Number(chart.dataset.plotHeight);
            const edgePath = (x1: number, y1: number, x2: number, y2: number) =>
                `M ${x1.toFixed(3)} ${y1.toFixed(3)} L ${x2.toFixed(3)} ${y2.toFixed(3)}`;
            check(edgePaths.includes(edgePath(axisLeft, axisBottom, axisRight, axisBottom)) &&
                  edgePaths.includes(edgePath(axisLeft, axisTop, axisLeft, axisBottom)) &&
                  !edgePaths.includes(edgePath(axisLeft, axisTop, axisRight, axisTop)) &&
                  !edgePaths.includes(edgePath(axisRight, axisTop, axisRight, axisBottom)),
                  'Bottom and left axes must be solid, with top and right edges open');
            check([...figure.querySelectorAll<SVGTextElement>('[data-plot-chart] text')].every(label =>
                      !['x', 'y', 'f'].includes(label.textContent)), 'Plot must not display axis titles');
            const smallPlotWidth = plotRendered.querySelector('svg')!.getBoundingClientRect().width;
            plotExpand.click();
            await nextLayout();
            const expandedPanel = document.getElementById('resultWorkspacePanel')!.getBoundingClientRect();
            const workspaceBar = document.querySelector<HTMLElement>('.lab-topbar')!.getBoundingClientRect();
            check(getComputedStyle(document.getElementById('workspacePanel')!).display === 'none',
                  'Expanded plot must hide the editor');
            check(Math.abs(expandedPanel.left - workspaceBar.left) < 2 &&
                      Math.abs(expandedPanel.right - workspaceBar.right) < 2,
                  'Expanded plot must span both workspace columns');
            const largePlotWidth = plotRendered.querySelector('svg')!.getBoundingClientRect().width;
            const plotStyle = getComputedStyle(plotRendered);
            const availablePlotWidth = plotRendered.clientWidth - parseFloat(plotStyle.paddingLeft) -
                                       parseFloat(plotStyle.paddingRight);
            check(largePlotWidth > smallPlotWidth && Math.abs(largePlotWidth - availablePlotWidth) < 2,
                  'Expanded plot SVG must grow to fit the available width at 100%');
            await runBrowserGroup('checkPlotChoices', checkPlotChoices);
            plotExpand.click();
            await nextLayout();
            const collapsedWidth = plotRendered.querySelector('svg')!.getBoundingClientRect().width;
            const collapsedAvailable = plotRendered.clientWidth - parseFloat(plotStyle.paddingLeft) -
                                       parseFloat(plotStyle.paddingRight);
            check(getComputedStyle(document.getElementById('workspacePanel')!).display !== 'none' &&
                      collapsedWidth < largePlotWidth && Math.abs(collapsedWidth - collapsedAvailable) < 2 &&
                      plotRendered.scrollWidth <= plotRendered.clientWidth + 1,
                  'Collapsing a plot restores the editor and fits the current column without horizontal overflow');
            const plotHeading = WorksheetContext.rightPaneTitle.textContent;
            WorksheetWorkspace.presentation.saveCurrentModeResultState();
            plotExpand.click();
            for (const mode of ['equation', 'diffequation', 'matrix', 'integrator', 'datetime', 'almanac'] as const) {
                WorksheetWorkspace.mode.setMode(mode);
                WorksheetWorkspace.presentation.syncModeUI();
                WorksheetWorkspace.presentation.restoreModeResultState();
                check(!WorksheetContext.rightPaneTitle.textContent.toLowerCase().includes('plot'), `${mode} inherited the plot heading`);
                check(!WorksheetContext.labWorkspace.classList.contains('result-card-expanded'), `${mode} inherited plot expansion`);
            }
            WorksheetWorkspace.mode.setMode('expression');
            WorksheetWorkspace.presentation.syncModeUI();
            WorksheetWorkspace.presentation.restoreModeResultState();
            await nextLayout();
            check(WorksheetContext.rightPaneTitle.textContent === plotHeading && WorksheetContext.renderedTitle.textContent === 'Rendered TeX' &&
                      !!plotRendered.querySelector<HTMLElement>('svg [data-series="0"]')!,
                  'Returning to Expression restores its plot and matching headings');
            (document.getElementById('plotScale')! as HTMLInputElement).value = 'manual';
            (document.getElementById('plotYLower')! as HTMLInputElement).value = '-1/2';
            (document.getElementById('plotYUpper')! as HTMLInputElement).value = '1/2';
            await WorksheetRequests.plots.takePlot('x');
            check(!plotRendered.classList.contains('error'), 'Manual plot limits failed');
            check(!plotRendered.textContent.includes('Vertical range clipped'), 'Manual clipping must be silent');
            (document.getElementById('plotScale')! as HTMLInputElement).value = 'all';
            await WorksheetRequests.plots.takePlot('x');
            check(!plotRendered.classList.contains('error'), 'Fit all failed');
            check(!plotRendered.textContent.includes('Vertical range clipped'), 'Fit all retained manual limits');
            const plotPrecision = WorksheetWorkspace.precision.requestedValuePrecision();
            try {
                WorksheetWorkspace.precision.setRequestedPrecisionBits(384);
                WorksheetBindings.controller.setExpressionEditor('tan(x)');
                await WorksheetRequests.evaluation.evaluateExpression();
                (document.getElementById('plotLower')! as HTMLInputElement).value = '-2pi';
                (document.getElementById('plotUpper')! as HTMLInputElement).value = '2pi';
                (document.getElementById('plotScale')! as HTMLInputElement).value = 'manual';
                (document.getElementById('plotYLower')! as HTMLInputElement).value = '-12';
                (document.getElementById('plotYUpper')! as HTMLInputElement).value = '12';
                await WorksheetRequests.plots.takePlot('x');
                check(!plotRendered.classList.contains('error'), 'High-precision tangent plot failed');
                const paths = plotRendered.querySelectorAll<LabTestHoverPath>('svg [data-series="0"] path');
                check(paths.length > 0, 'Tangent plot contains no visible curve');
                const frame = plotRendered.querySelector<SVGSVGElement>('[data-plot-chart]')!;
                const left = Number(frame.dataset.plotLeft), width = Number(frame.dataset.plotWidth);
                for (const path of paths) {
                    const coordinates = path.getAttribute('d')!.match(/-?\d+(?:\.\d+)?/g)!.map(Number);
                    for (const fraction of [0.125, 0.375, 0.625, 0.875]) {
                        const pole = left + width * fraction;
                        check(!(coordinates[0] < pole && coordinates[2] > pole), 'Curve connected across a tangent pole');
                    }
                }
            } finally {
                WorksheetWorkspace.precision.setRequestedPrecisionBits(plotPrecision);
            }
            (document.getElementById('plotScale')! as HTMLInputElement).value = 'auto';
            (document.getElementById('plotYLower')! as HTMLInputElement).value = '';
            (document.getElementById('plotYUpper')! as HTMLInputElement).value = '';
            (document.getElementById('plotLower')! as HTMLInputElement).value = '-5';
            (document.getElementById('plotUpper')! as HTMLInputElement).value = '5';
            WorksheetBindings.controller.setExpressionEditor('1/1^x + 1/2^x + 1/3^x + ...');
            await WorksheetRequests.plots.takePlot('x');
            check(!plotRendered.classList.contains('error') && !!plotRendered.querySelector<HTMLElement>('svg [data-series]')!,
                  'Reciprocal series must plot its convergent portion across -5 to 5');
            check(plotRendered.dataset.displayTex!.includes('\\sum'), 'Reciprocal series must retain its sigma title');
            (document.getElementById('plotUpper')! as HTMLInputElement).value = '1';
            const convergentView = plotRendered.querySelector<HTMLElement>('[data-series]')!;
            await WorksheetRequests.plots.takePlot('x');
            const diagnostic = document.getElementById('plotFeedback')!;
            check(!plotRendered.classList.contains('error') &&
                  plotRendered.querySelector<HTMLElement>('[data-series]')! === convergentView &&
                  !diagnostic.classList.contains('hidden') && diagnostic.textContent.includes('No finite real points') &&
                  !diagnostic.textContent.includes('NAN') && !diagnostic.textContent.includes('binding variable'),
                  'Wholly divergent range retains the previous view with a separate clean diagnostic');
            await window.checkLabMobileLayout('plot error');
            (document.getElementById('plotLower')! as HTMLInputElement).value = '-10';
            (document.getElementById('plotUpper')! as HTMLInputElement).value = '10';
            WorksheetBindings.controller.setExpressionEditor('x^2 + y^2');
            await WorksheetRequests.evaluation.evaluateExpression();
            (document.getElementById('plotKind')! as HTMLInputElement).value = 'auto';
            (document.getElementById('plotSecond')! as HTMLInputElement).value = '';
            (document.getElementById('plotLower')! as HTMLInputElement).value = '-2';
            (document.getElementById('plotUpper')! as HTMLInputElement).value = '2';
            (document.getElementById('plotSecondLower')! as HTMLInputElement).value = '-2';
            (document.getElementById('plotSecondUpper')! as HTMLInputElement).value = '2';
            await WorksheetRequests.plots.takePlot('x');
            check(!plotRendered.classList.contains('error') && !!plotRendered.querySelector<SVGSVGElement>('[data-surface]')!,
                  'Automatic two-variable plot must display a surface');
            const curveOption = document.querySelector<HTMLInputElement>('#plotKindChoices input[value="curve"]')!;
            check(!curveOption.closest<HTMLElement>('label')!.getClientRects().length,
                  'A curve is not offered while the second variable is unset');
            const fixedY = WorksheetContext.variableValues.querySelector<HTMLInputElement>('[data-binding-name="y"]')!;
            const originalY = fixedY.value;
            fixedY.value = '0';
            labDOM.call('lab_plot_controls_sync');
            check(curveOption.closest<HTMLElement>('label')!.getClientRects().length > 0,
                  'Fixing the other variable at zero permits a curve');
            (document.getElementById('plotKind')! as HTMLInputElement).value = 'curve';
            fixedY.value = originalY;
            labDOM.call('lab_plot_controls_sync');
            check(!curveOption.closest<HTMLElement>('label')!.getClientRects().length &&
                  (document.getElementById('plotKind')! as HTMLInputElement).value === 'auto',
                  'Clearing the fixed value hides the curve option and removes its stale selection');
            check((document.getElementById('plotAzimuth')! as HTMLInputElement).value === '315' &&
                  document.getElementById('plotAzimuthValue')!.textContent === '315°' &&
                  !!plotRendered.querySelector<SVGSVGElement>('[data-surface][data-azimuth="315.000"]')!,
                  'Initial surface and camera controls must share the ascending-axis default');
            for (let axis = 0; axis < 3; ++axis) {
                const positions = [...plotRendered.querySelectorAll<LabTestHoverPath>(`[data-plot-tick="${axis}"] path`)].map(path => {
                    const point = path.getPointAtLength(0);
                    return axis === 2 ? -point.y : point.x;
                });
                check(positions.length > 1 && positions.every((value, i) => !i || value > positions[i - 1]),
                      'Initial horizontal ticks must increase left to right and height ticks bottom to top');
            }
            const plotControls = document.getElementById('plotRange')!;
            await LabTestBrowser.paint();
            const manualRadio = document.querySelector<HTMLInputElement>('#plotScaleChoices input[value="manual"]')!;
            manualRadio.click();
            check(document.getElementById('plotVertical')!.getClientRects().length > 0 &&
                  document.getElementById('plotManual')!.getClientRects().length > 0 &&
                  document.getElementById('plotVerticalTitle')!.textContent === 'Height range',
                  'Surface height controls must expose editable manual limits inside the plot card');
            (document.getElementById('plotYLower')! as HTMLInputElement).value = '0';
            (document.getElementById('plotYUpper')! as HTMLInputElement).value = '1/2';
            await WorksheetRequests.plots.takePlot('x');
            const heightLabels = [...plotRendered.querySelectorAll<HTMLElement>('[data-plot-label="2"]')]
                .map(label => Number(label.textContent));
            const croppedSurface = plotRendered.querySelector<SVGSVGElement>('[data-surface]')!;
            check(Number(croppedSurface.dataset.zMin) === 0 && Number(croppedSurface.dataset.zMax) === 0.5 &&
                  heightLabels.every(value => value >= 0 && value <= 0.5) &&
                  !!croppedSurface.querySelector<HTMLElement>('[data-axis-handle="2"]')! &&
                  !!plotRendered.querySelector<SVGPathElement>('[data-series] path')!,
                  'Manual height bounds must crop the rendered surface without losing its axes');
            document.querySelector<HTMLInputElement>('#plotScaleChoices input[value="auto"]')!.click();
            await WorksheetRequests.plots.takePlot('x');
            check((document.getElementById('plotScale')! as HTMLInputElement).value === 'auto' &&
                  document.querySelector<HTMLInputElement>('#plotScaleChoices input[value="auto"]')!.checked &&
                  document.getElementById('plotManual')!.getClientRects().length === 0,
                  'Reset must restore automatic surface scaling');
            check(plotControls.closest<HTMLElement>('.result-card')! === plotRendered.closest<HTMLElement>('.result-card')! &&
                  !document.getElementById('workspacePanel')!.contains(plotControls) &&
                  getComputedStyle(plotControls).display !== 'none',
                  'Plot controls must live inside the visible plot card');
            for (const id of ['plotAzimuth', 'plotElevation', 'plotRoll', 'plotZoom']) {
                check((document.getElementById(id)! as LabTestPlotInput).type === 'range', '3-D camera controls must be sliders');
                const slider = (document.getElementById(id)! as LabTestPlotInput);
                const field = slider.closest<HTMLElement>('.plot-field')!;
                check(Math.abs(slider.getBoundingClientRect().width - field.getBoundingClientRect().width) < 1 &&
                      slider.getBoundingClientRect().height + 0.01 >= (matchMedia('(pointer: coarse)').matches ? 36 : 28) &&
                      Math.abs(field.querySelector('output')!.getBoundingClientRect().top -
                          field.querySelector('label')!.getBoundingClientRect().top) < 1 &&
                      field.querySelector('label')!.getBoundingClientRect().bottom <=
                          slider.getBoundingClientRect().top + 0.01,
                      'Camera values sit beside labels above full-width sliders with touch-sized targets: ' +
                      JSON.stringify({id, slider: slider.getBoundingClientRect(), field: field.getBoundingClientRect(),
                          output: field.querySelector('output')!.getBoundingClientRect(),
                          label: field.querySelector('label')!.getBoundingClientRect(),
                          camera: document.getElementById('plotCamera')!.className,
                          plot: plotRendered.className, feedback: document.getElementById('plotFeedback')!.textContent}));
            }
            const savedCardWidth = plotCard.style.width;
            plotCard.style.width = '360px';
            check(getComputedStyle(plotControls).display === 'flex' &&
                  plotControls.scrollWidth <= plotControls.clientWidth + 1 &&
                  getComputedStyle(document.getElementById('plotCamera')!).gridTemplateColumns.split(' ').length === 2 &&
                  (document.getElementById('plotAzimuth')! as HTMLInputElement).getBoundingClientRect().width > 100,
                  'Narrow plot cards use a compact two-by-two camera grid without overflow');
            checkPlotRangeAlignment();
            await runBrowserGroup('checkPlotChoices', checkPlotChoices);
            plotCard.style.width = savedCardWidth;
            checkPlotControls(true);
            check(document.getElementById('plotCamera')!.classList.contains('plot-options-card') &&
                  plotControls.firstElementChild!.id === 'plotCamera' &&
                  document.getElementById('plotCamera')!.getBoundingClientRect().bottom <=
                      document.getElementById('plotVertical')!.getBoundingClientRect().top + 0.01 &&
                  !document.querySelector<HTMLElement>('#plotCamera .plot-hint')!,
                  'Compact camera controls come first below the chart, with gestures in Help');
            const zoomSlider = (document.getElementById('plotZoom')! as HTMLInputElement);
            const zoomToken = WorksheetRequests.labRequests.latestMain();
            zoomSlider.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, button: 0, isPrimary: true}));
            zoomSlider.value = '1';
            zoomSlider.dispatchEvent(new Event('input', {bubbles: true}));
            await LabTestBrowser.paint();
            await LabTestBrowser.wait(450);
            check(Number(plotRendered.querySelector<SVGSVGElement>('[data-surface]')!.dataset.cameraZoom) === 2 &&
                  document.getElementById('plotZoomValue')!.textContent === '200%' &&
                  WorksheetRequests.labRequests.latestMain() === zoomToken,
                  'Held zoom slider updates the clipped volume without requesting native samples');
            zoomSlider.dispatchEvent(new Event('change', {bubbles: true}));
            await waitForVolume();
            plotCard.querySelector<HTMLElement>('[data-zoom-reset]')!.click();
            await LabTestBrowser.paint();
            check(zoomSlider.value === '0' && document.getElementById('plotZoomValue')!.textContent === '100%',
                  'Camera reset synchronises the zoom slider and readout');
            await waitForVolume();
            await window.checkLabMobileLayout('surface plot and height controls');
            const facets = [...plotRendered.querySelectorAll<SVGSVGElement>('[data-surface] [data-series] path')];
            check(facets.length === 2 * 69 * 69 && new Set(facets.map(path => path.getAttribute('fill'))).size > 4,
                  'Full surface must contain the shaded 70-by-70 mesh');
            check(!!plotRendered.querySelector<SVGTextElement>('[data-plot-title] use')!, 'Surface must retain its rendered formula title');
            checkPlotLabelSpacing();
            for (const [axis, name] of [[0, 'x'], [1, 'y'], [2, 'u']]) {
                check(!!plotRendered.querySelector<HTMLElement>(`[data-axis-arrow="${axis}"]`)! &&
                      plotRendered.querySelector<HTMLElement>(`[data-axis-name="${axis}"]`)!.textContent.trim() === name,
                      'Surface axes must identify their variables at increasing-value arrowheads');
            }
            for (const axis of [0, 1, 2])
                check(plotRendered.querySelectorAll<LabTestHoverPath>(`[data-plot-tick="${axis}"] path`).length >= 3,
                      'Every surface axis needs visible tick strokes');
            const initialMesh = plotRendered.querySelector<HTMLElement>('[data-series]')!.innerHTML;
            const sliderRequest = WorksheetRequests.labRequests.latestMain();
            (document.getElementById('plotAzimuth')! as HTMLInputElement).value = '120';
            (document.getElementById('plotElevation')! as HTMLInputElement).value = '40';
            (document.getElementById('plotRoll')! as HTMLInputElement).value = '90';
            (document.getElementById('plotAzimuth')! as HTMLInputElement).dispatchEvent(new Event('input', {bubbles: true}));
            (document.getElementById('plotElevation')! as HTMLInputElement).dispatchEvent(new Event('input', {bubbles: true}));
            (document.getElementById('plotRoll')! as HTMLInputElement).dispatchEvent(new Event('input', {bubbles: true}));
            check(document.getElementById('plotAzimuthValue')!.textContent === '120°' &&
                  document.getElementById('plotElevationValue')!.textContent === '40°' &&
                  document.getElementById('plotRollValue')!.textContent === '90°',
                  'Slider readouts must follow input before committing a redraw');
            check(plotRendered.querySelector<HTMLElement>('[data-series]')!.innerHTML === initialMesh,
                  'Slider input alone must not launch repeated native requests');
            await LabTestBrowser.wait(450);
            check(plotRendered.querySelector<HTMLElement>('[data-local-camera="120,40,90"]')! &&
                  WorksheetRequests.labRequests.latestMain() === sliderRequest &&
                  plotRendered.querySelector<HTMLElement>('[data-series]')!.innerHTML === initialMesh,
                  'All camera sliders update WebGL while held, without a debounce-time sampling request');
            (document.getElementById('plotRoll')! as HTMLInputElement).dispatchEvent(new Event('change', {bubbles: true}));
            const rotationDeadline = performance.now() + 15000;
            while (plotRendered.marsPlotDrag.busy &&
                   performance.now() < rotationDeadline)
                await LabTestBrowser.wait(25);
            check(!!plotRendered.querySelector<HTMLElement>('[data-local-camera="120,40,90"]')! &&
                  WorksheetRequests.labRequests.latestMain() === sliderRequest,
                  'Committing camera sliders keeps the local camera without native sampling');
            check(plotRendered.querySelector<HTMLElement>('[data-series]')!.innerHTML === initialMesh,
                  'Rotation reuses native geometry');
            checkPlotLabelSpacing();
            for (const [azimuth, elevation, roll] of [[45, 30, 0], [45, 5, 180], [225, 85, 270]]) {
                (document.getElementById('plotLower')! as HTMLInputElement).value = '-10';
                (document.getElementById('plotUpper')! as HTMLInputElement).value = '10';
                (document.getElementById('plotSecondLower')! as HTMLInputElement).value = '-10';
                (document.getElementById('plotSecondUpper')! as HTMLInputElement).value = '10';
                (document.getElementById('plotAzimuth')! as HTMLInputElement).value = String(azimuth);
                (document.getElementById('plotElevation')! as HTMLInputElement).value = String(elevation);
                (document.getElementById('plotRoll')! as HTMLInputElement).value = String(roll);
                await WorksheetRequests.plots.takePlot('x');
                checkPlotLabelSpacing();
            }
            for (const id of ['plotAzimuth', 'plotElevation', 'plotRoll']) {
                const style = getComputedStyle((document.getElementById(id)! as LabTestPlotInput));
                check(style.backgroundColor !== 'rgb(255, 255, 255)' && style.color !== 'rgb(0, 0, 0)',
                      'Camera controls must retain the dark Lab theme');
            }
            const surfaceExpand = plotRendered.closest<HTMLElement>('.result-card')!.querySelector<HTMLElement>('[data-expand-card]')!;
            surfaceExpand.click();
            check(WorksheetContext.labWorkspace.classList.contains('result-card-expanded'), 'Surface must expand across the workspace');
            check(plotControls.getBoundingClientRect().width > 0 &&
                  plotRendered.closest<HTMLElement>('.expanded-card')!.contains(plotControls),
                  'Expanded plot must retain its controls');
            checkPlotRangeAlignment();
            await runBrowserGroup('checkPlotWheelZoom', checkPlotWheelZoom);
            await runBrowserGroup('checkPlotDrag', checkPlotDrag);
            (document.getElementById('plotAzimuth')! as HTMLInputElement).value = '75';
            (document.getElementById('plotAzimuth')! as HTMLInputElement).dispatchEvent(new Event('input', {bubbles: true}));
            (document.getElementById('plotAzimuth')! as HTMLInputElement).dispatchEvent(new Event('change', {bubbles: true}));
            const updateDeadline = performance.now() + 15000;
            while (plotRendered.marsPlotDrag.busy && performance.now() < updateDeadline)
                await LabTestBrowser.wait(25);
            check(Number(plotRendered.querySelector<SVGSVGElement>('[data-local-camera]')?.dataset.localCamera!.split(',')[0]) === 75 &&
                  WorksheetContext.labWorkspace.classList.contains('result-card-expanded'),
                  'Live plotting must work without leaving the expanded card');
            surfaceExpand.click();
            (document.getElementById('plotLower')! as HTMLInputElement).value = '-10';
            const retainedSurface = labSurfacePreview.snapshot()!;
            for (let cycle = 0; cycle < 2; ++cycle) {
                await WorksheetWorkspace.mode.selectWorksheetMode('matrix');
                check(!document.querySelector<SVGSVGElement>('#plotRendered [data-surface-overlay]')!,
                      'Leaving a surface releases its transient renderer');
                await WorksheetWorkspace.mode.selectWorksheetMode('expression');
                await nextLayout();
                const restored = labSurfacePreview.snapshot()!;
                const chart = plotRendered.querySelector<SVGSVGElement>('[data-surface]')!;
                const overlay = plotRendered.querySelector<SVGSVGElement>('[data-surface-overlay]')!;
                check(chart && restored && overlay && overlay.style.display !== 'none' &&
                      plotRendered.querySelectorAll<SVGSVGElement>('[data-surface-overlay]').length === 1 &&
                      chart.dataset.localRenderer && labSurfacePreview.prepare(chart)!.framePlan!.drawMesh,
                      'Returning to Expression recreates a visible surface renderer, not empty saved canvases');
                check(restored.key === retainedSurface.key &&
                      restored.orientation.every((v, i) => Math.abs(v - retainedSurface.orientation[i]) < 1e-10) &&
                      restored.volume.exponent === retainedSurface.volume.exponent,
                      'Mode round trips retain the exact volume and quaternion camera');
                check(!(WorksheetWorkspace.presentation.resultStates.expression as LabTestSavedPlotResult).plotRendered.innerHTML.includes('data-surface-overlay'),
                      'Saved plot HTML contains only reusable native geometry');
            }
            (document.getElementById('plotAzimuth')! as HTMLInputElement).value = '45';
            (document.getElementById('plotElevation')! as HTMLInputElement).value = '30';
            (document.getElementById('plotRoll')! as HTMLInputElement).value = '0';
            (document.getElementById('plotUpper')! as HTMLInputElement).value = '10';
            (document.getElementById('plotSecondLower')! as HTMLInputElement).value = '-10';
            (document.getElementById('plotSecondUpper')! as HTMLInputElement).value = '10';
            WorksheetBindings.controller.setExpressionEditor('3+4');
            await WorksheetRequests.evaluation.evaluateExpression();
            await waitForAutomaticPlot();
            check(document.getElementById('plotCard')!.classList.contains('hidden'),
                  'A constant evaluation removes the previous surface plot card');
            WorksheetBindings.controller.setExpressionEditor('ln(-1)');
            await WorksheetRequests.evaluation.evaluateExpression();
            check(document.getElementById('plotCard')!.classList.contains('hidden'),
                  'A complex constant has no applicable plot card');
            check(!document.getElementById('plotRendered')!.textContent.includes('Select a variable'),
                  'Constants do not request a plot without an axis');
            for (const [mode, source, series, surface] of [
                ['equation', 'x^2=1', 2, false],
                ['equation', 'x^2+y^2=1', 1, false],
                ['equation', 'x^2+y^2+z^2=1', 1, true],
                ['equation', 'cos(z)-sin(2-xy)=0', 1, true],
                ['equation', 'cos(z)-sin(pi-xy)=0', 1, true],
                ['equation', 'x^4-x^3-26x^2+4x+8=0', 2, false],
                ['diffequation', 'Dx(y)=x*y; y(0)=1', 1, false],
                ['diffequation', "y''+6y*y'+4y^3=0; y(0)=1; y'(0)=0", 1, false],
                ['diffequation', "y''+6y*y'+4y^3=0; y(0)=0; y'(0)=1", 1, false],
                ['diffequation', "y''+6y*y'+4y^3=0", 9, false],
                ['diffequation', 'Dx(y)=y', 5, false],
                ['diffequation', '{2*Dx(u)+Dy(u)=0 | x=?, y=?; ; u(x,0)=x^2}', 1, true]
            ] as Array<[WorksheetMode, string, number, boolean]>) {
                WorksheetWorkspace.mode.setMode(mode);
                WorksheetWorkspace.presentation.syncModeUI();
                WorksheetBindings.controller.setExpressionEditor(source);
                await WorksheetWorkspace.mode.evaluateCurrentMode();
                await WorksheetBindings.controller.commitVisibleBindingInputs();
                const button = document.getElementById('expressionPlot')!;
                const authored = WorksheetBindings.editor.currentExpressionText();
                check(!button, mode + ' has no separate Plot button');
                await waitForAutomaticPlot();
                check(!!plotRendered.querySelector<HTMLElement>('[data-series]')!, mode + ' evaluates an automatic plot');
                check(!!WorksheetContext.rendered.innerHTML && !!WorksheetContext.functionStyle.textContent, mode + ' retains mathematical cards');
                if (source === 'cos(z)-sin(pi-xy)=0') {
                    check(!!WorksheetContext.rendered.querySelector('svg')! && !WorksheetContext.rendered.textContent.includes('rendering failed'),
                          'Implicit trigonometric solution families render as mathematics');
                }
                (document.getElementById('plotLower')! as HTMLInputElement).value = mode === 'equation' ? '' : '-2';
                (document.getElementById('plotUpper')! as HTMLInputElement).value = mode === 'equation' ? '' : '2';
                await WorksheetRequests.plots.takePlot((document.getElementById('plotFirst')! as HTMLInputElement).value);
                check(plotRendered.querySelectorAll<HTMLElement>('[data-series]').length === series, mode + ' plots all native curves: ' +
                      JSON.stringify({source, variables: WorksheetBindings.panel.currentVariables, status: WorksheetContext.statusEl.textContent,
                                      error: plotRendered.textContent.slice(0, 400)}));
                check(!!plotRendered.querySelector<SVGSVGElement>('[data-surface]')! === surface, mode + ' selects native dimensionality');
                if (source === 'cos(z)-sin(2-xy)=0') {
                    (document.getElementById('plotScale')! as HTMLInputElement).value = 'manual';
                    for (const id of ['plotLower', 'plotSecondLower', 'plotYLower'])
                        (document.getElementById(id)! as LabTestPlotInput).value = '-5';
                    for (const id of ['plotUpper', 'plotSecondUpper', 'plotYUpper'])
                        (document.getElementById(id)! as LabTestPlotInput).value = '5';
                    await WorksheetRequests.plots.takePlot('z');
                    const initialChart = plotRendered.querySelector<SVGSVGElement>('[data-surface]')!;
                    const initialRequest = WorksheetRequests.labRequests.latestMain();
                    const target = plotRendered.querySelector<SVGGraphicsElement>('[data-axis-handle]')!;
                    const send = (node: EventTarget, type: string, x: number, y: number) => node.dispatchEvent(new PointerEvent(type, {
                        clientX: x, clientY: y, button: 0, buttons: type === 'pointerup' ? 0 : 1,
                        pointerId: 31, pointerType: 'touch', isPrimary: true, bubbles: true, cancelable: true
                    }));
                    send(target, 'pointerdown', 100, 100);
                    send(window, 'pointermove', 140, 110);
                    const deadline = performance.now() + 20000;
                    while (!plotRendered.querySelector<HTMLElement>('[data-local-renderer]')! && performance.now() < deadline)
                        await LabTestBrowser.wait(25);
                    const preview = plotRendered.querySelector<SVGSVGElement>('[data-local-renderer]')!;
                    check(preview === initialChart && WorksheetRequests.labRequests.latestMain() === initialRequest &&
                          plotRendered.marsPlotDrag.active && plotRendered.querySelector<HTMLCanvasElement>('[data-surface-overlay] canvas')!,
                          'Wide implicit equations rotate the retained mesh without native sampling before release');
                    send(window, 'pointerup', 140, 110);
                    const finish = performance.now() + 20000;
                    while ((plotRendered.marsPlotDrag.busy ||
                            !plotRendered.querySelector<HTMLElement>('[data-plot-quality="full"]')!) && performance.now() < finish)
                        await LabTestBrowser.wait(25);
                    const full = plotRendered.querySelector<SVGSVGElement>('[data-plot-quality="full"]')!;
                    check(full === initialChart && plotRendered.querySelector<HTMLElement>('[data-local-renderer]')! &&
                          WorksheetRequests.labRequests.latestMain() === initialRequest && !plotRendered.marsPlotDrag.busy &&
                          !plotRendered.querySelector<HTMLElement>('[data-preview-notice]')!,
                          'Releasing an implicit-equation axis keeps the full retained mesh without sampling');
                    const orientation = labSurfacePreview.snapshot()!.orientation;
                    initialChart.dispatchEvent(new WheelEvent('wheel', {
                        deltaY: 100, altKey: true, bubbles: true, cancelable: true
                    }));
                    await LabTestBrowser.wait(450);
                    check(WorksheetRequests.labRequests.latestMain() === initialRequest &&
                          labSurfacePreview.snapshot()!.orientation.some((v, i) => Math.abs(v - orientation[i]) > 0.001),
                          'Equation-mode wheel rotation uses the same persistent camera as expression mode');
                }
                if (source === 'x^2+y^2+z^2=1') {
                    const card = plotRendered.closest<HTMLElement>('.result-card')!;
                    const retained = plotRendered.querySelector<SVGSVGElement>('[data-surface]')!;
                    const generation = WorksheetRequests.labRequests.latestMain();
                    for (let step = 0; step < 5; step++) card.querySelector<HTMLElement>('[data-zoom-step="1"]')!.click();
                    await LabTestBrowser.paint();
                    check(retained.isConnected && retained.querySelector<HTMLElement>('[data-uv]')! &&
                          Number(retained.dataset.cameraZoom) > 3 && WorksheetRequests.labRequests.latestMain() === generation,
                          'Volume preview stays interactive even when the selected sphere interior is empty');
                    await waitForVolume();
                    const empty = plotRendered.querySelector<SVGSVGElement>('[data-surface]')!;
                    check(empty?.dataset.empty === 'true' && plotRendered.querySelector<HTMLElement>('[data-local-renderer]')!,
                          'Native sampling of the empty sphere interior retains interactive axes');
                    labDOM.call('lab_layout_plot', null, 'Plot request timed out');
                    for (let step = 0; step < 5; step++) card.querySelector<HTMLElement>('[data-zoom-step="-1"]')!.click();
                    await LabTestBrowser.paint();
                    check(empty.isConnected && Math.abs(Number(empty.dataset.cameraZoom) - 1) < 1e-10,
                          'A retained surface can still zoom back out after a failed redraw');
                    await waitForVolume();
                    check(plotRendered.querySelector<SVGSVGElement>('[data-surface] [data-uv]')!,
                          'Zooming out of an empty volume samples the sphere again');
                    card.querySelector<HTMLElement>('[data-zoom-reset]')!.click();
                    await LabTestBrowser.paint();
                    check((document.getElementById('plotScale')! as HTMLInputElement).value === 'auto',
                          'Volume reset restores automatic mathematical bounds');
                    await waitForVolume();
                }
                if ((document.getElementById('plotInputRange')! as HTMLInputElement).value === 'auto')
                    check([...document.querySelectorAll<HTMLElement>('.plot-ranges .plot-bound-pair')]
                              .every(pair => pair.getClientRects().length === 0),
                          mode + ' hides variable bound-entry fields in input Auto');
                if (!surface) {
                    const axisLabel = document.querySelector<HTMLElement>('#plotFirstChoices label:only-child')!;
                    if (axisLabel) {
                        const style = getComputedStyle(axisLabel);
                        check(style.backgroundColor === 'rgba(0, 0, 0, 0)' &&
                              style.borderTopColor === 'rgba(0, 0, 0, 0)' && axisLabel.textContent.endsWith(' range'),
                              'Single-axis range has a plain readable heading, not a cramped selection chip');
                    }
                }
                check((getComputedStyle(document.getElementById('plotSecondGroup')!).display !== 'none') === surface,
                      mode + ' shows second-variable controls only for a surface');
                check((getComputedStyle(document.getElementById('plotCamera')!).display !== 'none') === surface,
                      mode + ' shows camera controls only for a surface');
                if (mode === 'equation') {
                    if (source === 'x^2+y^2+z^2=1') {
                        const mesh = plotRendered.querySelector<SVGSVGElement>('[data-mesh="true"]')!;
                        check(mesh && Number(mesh.dataset.sampleZMin) < -0.99 &&
                              Number(mesh.dataset.sampleZMax) > 0.99,
                              'The sphere contains both hemispheres, not a single height branch');
                        const silhouette = mesh.querySelector<SVGGElement>('[data-series]')!.getBBox();
                        check(Math.abs(silhouette.width / silhouette.height - 1) < 0.03 &&
                              Number(mesh.dataset.xMin) > -1.3 && Number(mesh.dataset.xMax) < 1.3 &&
                              Number(mesh.dataset.zMin) > -1.3 && Number(mesh.dataset.zMax) < 1.3,
                              'The sphere stays round inside a tightly fitted equal-unit box');
                        const axesBox = mesh.querySelector<SVGGElement>('[data-plot-axes]')!.getBBox();
                        const top = Math.min(silhouette.y, axesBox.y);
                        const bottom = Math.max(silhouette.y + silhouette.height, axesBox.y + axesBox.height);
                        check(silhouette.width > mesh.viewBox.baseVal.width * 0.36 &&
                              top < mesh.viewBox.baseVal.height * 0.12 &&
                              bottom > mesh.viewBox.baseVal.height * 0.88,
                              'The surface and axes fill the plot without large empty top and bottom margins');
                    }
                    if (source === 'x^2+y^2=1') {
                        const chart = plotRendered.querySelector<SVGSVGElement>('[data-plot-chart]')!;
                        const d = chart.dataset, quadrants = new Set();
                        const paths = [...chart.querySelectorAll<LabTestHoverPath>('[data-series] path')];
                        check(paths.length > 100, 'The unit circle has a refined closed contour');
                        for (const path of paths) {
                            const point = path.getPointAtLength(path.getTotalLength() / 2);
                            const x = Number(d.xMin) + (point.x - Number(d.plotLeft)) / Number(d.plotWidth) *
                                (Number(d.xMax) - Number(d.xMin));
                            const y = Number(d.yMax) - (point.y - Number(d.plotTop)) / Number(d.plotHeight) *
                                (Number(d.yMax) - Number(d.yMin));
                            check(Math.abs(x * x + y * y - 1) < 0.002,
                                  'Every circle segment satisfies the implicit equation');
                            quadrants.add((x >= 0 ? 1 : 0) + (y >= 0 ? 2 : 0));
                        }
                        check(quadrants.size === 4 && Math.abs(Number(d.plotWidth) - Number(d.plotHeight)) < 1,
                              'The circle covers all quadrants with equal coordinate scales');
                    }
                    const solutionCard = document.getElementById('valueCard')!;
                    check(solutionCard.getClientRects().length > 0 &&
                          document.getElementById('valueTitle')!.textContent === 'Solutions' && WorksheetContext.value.textContent.trim(),
                          'Equation plots retain their Solutions card');
                    if (source.includes('x^4'))
                        for (const root of ['-4.671042', '-0.489034', '0.633721', '5.526355'])
                            check(WorksheetContext.value.textContent.includes(root), 'Equation plot includes numerical root ' + root);
                    check(!plotRendered.textContent.includes('Left side') && !plotRendered.textContent.includes('Right side'),
                          'Equation plots omit generic side labels');
                    if (source.includes('x^4')) {
                        const ticks = [...plotRendered.querySelectorAll<HTMLElement>('[data-plot-label="1"]')].map(t => Number(t.textContent));
                        check(ticks.includes(-200) && ticks.includes(200) && ticks.every(t => Math.abs(t) < 300),
                              'Automatic equation frame shows the quartic turning points without distant tails');
                        await runBrowserGroup('checkCurvePointerView', checkCurvePointerView);
                        await runBrowserGroup('checkLivePlotControls', checkLivePlotControls);
                    }
                }
                if (mode === 'diffequation' && !surface && series === 1)
                    check(!plotRendered.textContent.includes('Solution 1'), 'A single solution does not need a legend');
                if (mode === 'diffequation' && !surface && series > 1) {
                    const titles = [...plotRendered.querySelectorAll<LabTestPlotTitle>('[data-series] > title')];
                    check(titles.length === series && titles.every(title => title.marsPlotLabel.includes(' = ')),
                          'Every family curve has a native hover tooltip identifying its constants');
                    check([...plotRendered.querySelectorAll<HTMLElement>('svg title')].every(title => !title.textContent) &&
                          plotRendered.dataset.displaySvg!.includes(titles[0].marsPlotLabel),
                          'Only the custom tooltip is active; copied native SVG retains its labels');
                    check(![...plotRendered.querySelectorAll('text')].some(text => text.textContent.includes(' = ')),
                          'Family constants must not clutter the plot with a visible legend');
                    const curves = [...plotRendered.querySelectorAll<HTMLElement>('[data-series]')];
                    const chart = plotRendered.querySelector<SVGSVGElement>('[data-x-min]')!;
                    const transform = chart.getScreenCTM()!;
                    const point = {
                        clientX: transform.e + (Number(chart.dataset.plotLeft) + Number(chart.dataset.plotWidth) / 2) * transform.a,
                        clientY: transform.f + (Number(chart.dataset.plotTop) + Number(chart.dataset.plotHeight) / 2) * transform.d,
                        bubbles: true, pointerType: 'mouse'
                    };
                    for (const active of curves.slice(0, 2)) {
                        active.querySelector('path')!.dispatchEvent(new PointerEvent('pointermove', point));
                        check(curves.every(curve => Number(getComputedStyle(curve).opacity) === (curve === active ? 1 : 0.18)),
                              'Hovering a family curve dims only the other solutions');
                        check(active.getAttribute('aria-label') === active.querySelector<LabTestPlotTitle>('title')!.marsPlotLabel,
                              'Focused family curve retains its constant tooltip');
                        const tooltip = plotRendered.querySelector<HTMLElement>('.plot-pointer-tip')!;
                        const lines = tooltip.textContent.split('\n');
                        const constants = active.querySelector<LabTestPlotTitle>('title')!.marsPlotLabel.split(', ');
                        check(!plotRendered.querySelector<HTMLElement>('.plot-pointer-horizontal')!.classList.contains('hidden'),
                              'Selected family curves display a horizontal guide as well as a vertical guide');
                        check(!plotRendered.querySelector<HTMLElement>('.plot-pointer-marker')!.classList.contains('hidden'),
                              'Selected family curves mark the point described by the tooltip');
                        check(!tooltip.classList.contains('hidden') && tooltip.textContent.includes('x ≈ ') &&
                              lines[1].startsWith('y ≈ ') && lines.length === constants.length + 2 &&
                              constants.every((value, i) => lines[i + 2] === value) &&
                              getComputedStyle(tooltip).whiteSpace === 'pre-line',
                              'The visible hover tooltip puts x, y and each selected curve constant on separate lines');
                    }
                    chart.dispatchEvent(new PointerEvent('pointermove', point));
                    check(curves.every(curve => Number(getComputedStyle(curve).opacity) === 1),
                          'Moving between curves restores the whole solution family');
                    check(plotRendered.querySelector<HTMLElement>('.plot-pointer-tip')!.textContent.split('\n').length === 2,
                          'Moving off a curve removes stale constants from the coordinate tooltip');
                    curves[0].querySelector('path')!.dispatchEvent(new PointerEvent('pointermove', point));
                    plotRendered.dispatchEvent(new PointerEvent('pointerleave'));
                    check(curves.every(curve => Number(getComputedStyle(curve).opacity) === 1),
                          'Leaving a family plot clears curve focus');
                }
                if (!surface) {
                    document.querySelector<HTMLInputElement>('#plotScaleChoices input[value="manual"]')!.click();
                    (document.getElementById('plotYLower')! as HTMLInputElement).value = '-2';
                    (document.getElementById('plotYUpper')! as HTMLInputElement).value = '2';
                    const beforeUpdate = plotRendered.querySelector<HTMLElement>('[data-series]')!;
                    (document.getElementById('plotYUpper')! as HTMLInputElement).dispatchEvent(new Event('input', {bubbles: true}));
                    const deadline = performance.now() + 15000;
                    const updated = () => plotRendered.querySelector<HTMLElement>('[data-series]')! &&
                        plotRendered.querySelector<HTMLElement>('[data-series]')! !== beforeUpdate &&
                        plotRendered.querySelector<HTMLElement>('[data-plot-label="1"]')!;
                    while (!updated() && performance.now() < deadline)
                        await LabTestBrowser.wait(25);
                    const ticks = [...plotRendered.querySelectorAll<HTMLElement>('[data-plot-label="1"]')].map(t => Number(t.textContent));
                    check(plotRendered.querySelector<HTMLElement>('[data-series]')! !== beforeUpdate && ticks.includes(-1) &&
                          ticks.includes(1) && ticks.every(t => t >= -2 && t <= 2),
                          mode + ' Update plot applies manual vertical bounds');
                    if (mode === 'equation')
                        check(document.getElementById('valueCard')!.getClientRects().length > 0 && WorksheetContext.value.textContent.trim(),
                              'Live equation range updates retain the solutions');
                    document.querySelector<HTMLInputElement>('#plotScaleChoices input[value="auto"]')!.click();
                }
                check((document.getElementById('plotCamera')!.getClientRects().length > 0) === surface,
                      mode + ' shows camera controls only for surfaces');
                check(WorksheetWorkspace.mode.currentMode() === mode && WorksheetBindings.editor.currentExpressionText() === authored, mode + ' retains the authored problem: ' +
                      JSON.stringify({mode: WorksheetWorkspace.mode.currentMode(), before: authored, after: WorksheetBindings.editor.currentExpressionText()}));
                check(!WorksheetBindings.panel.currentDifferentiable, mode + ' does not expose expression derivative buttons');
                await window.checkLabMobileLayout(mode + ' plot');
                await WorksheetWorkspace.mode.evaluateCurrentMode();
                const solutionCard = document.getElementById('valueCard')!;
                check(solutionCard.getClientRects().length > 0 && !solutionCard.classList.contains('hidden') &&
                      document.getElementById('valueTitle')!.textContent === 'Solutions',
                      mode + ' restores its solution card after plotting in the same mode');
                check(document.getElementById('renderedTitle')!.textContent ===
                      (mode === 'equation' ? 'Rendered TeX' : 'Equation and solutions'),
                      mode + ' restores its mathematical heading instead of retaining Plot');
                if (mode === 'equation' && source.includes('x^4')) {
                    for (const root of ['-4.671042', '-0.489034', '0.633721', '5.526355'])
                        check(WorksheetContext.value.textContent.includes(root), 'Re-evaluated quartic retains numerical root ' + root);
                }
                await waitForAutomaticPlot();
                await window.checkLabMobileLayout(mode + ' restored solutions');
            }
            WorksheetBindings.controller.setExpressionEditor("y''+3y*y'+y^4=0; y(0)=1; y'(0)=0");
            await WorksheetWorkspace.mode.evaluateCurrentMode();
            check(!document.getElementById('expressionPlot')!, 'Local Taylor series must not advertise unsupported plotting');
            WorksheetWorkspace.mode.setMode('integrator');
            WorksheetWorkspace.presentation.syncModeUI();
            WorksheetBindings.controller.setExpressionEditor('sin(x)');
            WorksheetBindings.integratorPanel.renderIntegratorRows([{kind: 'bound', name: 'x', lo: '-pi', hi: 'pi'}]);
            await WorksheetWorkspace.mode.evaluateCurrentMode();
            await waitForAutomaticPlot();
            check(!!plotRendered.querySelector<HTMLElement>('[data-integral-shading]')! &&
                  !!plotRendered.querySelector<HTMLElement>('[data-sign="positive"]')! &&
                  !!plotRendered.querySelector<HTMLElement>('[data-sign="negative"]')!,
                  'Integrator Evaluate independently plots and shades both signed contributions');
            check(!!WorksheetContext.rendered.querySelector('svg')! && WorksheetContext.value.textContent.trim(),
                  'Integrator plot retains its mathematical and numerical results');
            const integrationRows = JSON.stringify(WorksheetWorkspace.integrator.activeIntegratorBoundRows());
            (document.getElementById('plotLower')! as HTMLInputElement).value = '-1';
            (document.getElementById('plotUpper')! as HTMLInputElement).value = '1';
            await WorksheetRequests.plots.takePlot('x');
            check(JSON.stringify(WorksheetWorkspace.integrator.activeIntegratorBoundRows()) === integrationRows,
                  'Integrator viewport changes never alter integration limits');
            WorksheetBindings.controller.setExpressionEditor('sin(t)');
            WorksheetBindings.integratorPanel.renderIntegratorRows([{kind: 'bound', name: 't', lo: '', hi: ''}]);
            await WorksheetWorkspace.mode.evaluateCurrentMode();
            await waitForAutomaticPlot();
            check(!!plotRendered.querySelector<HTMLElement>('[data-series]')! &&
                  !plotRendered.querySelector<HTMLElement>('[data-integral-shading]')! &&
                  (document.getElementById('plotFirst')! as HTMLInputElement).value === 't',
                  'Indefinite integrals plot the integrand without shading using the authored variable');
            WorksheetWorkspace.mode.setMode('expression');
            WorksheetWorkspace.presentation.syncModeUI();
            WorksheetBindings.controller.setExpressionEditor('3+4');
            await WorksheetRequests.evaluation.evaluateExpression();
            const ordinaryZoom = resultZoomIndex(WorksheetContext.rendered.closest<HTMLElement>('.result-card')!);
            check(WorksheetContext.rendered.dispatchEvent(new WheelEvent('wheel', {deltaY: -100, bubbles: true, cancelable: true})) &&
                  resultZoomIndex(WorksheetContext.rendered.closest<HTMLElement>('.result-card')!) === ordinaryZoom,
                  'Ordinary formula results must retain unmodified wheel scrolling');

            const card = WorksheetContext.rendered.closest<HTMLElement>('.result-card')!;
            const before = resultZoomIndex(card);
            stepResultZoom(card, 1);
            check(resultZoomIndex(card) === before + 1, 'Result zoom did not change');
            await runBrowserGroup('checkRenderedGlyphs', checkRenderedGlyphs);
            const expand = card.querySelector<HTMLElement>('[data-expand-card]')!;
            expand.click();
            check(expand.getAttribute('aria-expanded') === 'true', 'Result expansion failed');
            WorksheetContext.help.click();
            check(!WorksheetContext.helpPane.classList.contains('hidden'), 'Help did not open');
            check(expand.getAttribute('aria-expanded') === 'false', 'Help must leave result expansion cleanly');
            WorksheetContext.help.click();

            for (const mode
                     of ['matrix', 'integrator', 'datetime', 'almanac', 'equation', 'diffequation', 'expression'] as const) {
                WorksheetWorkspace.mode.setMode(mode);
                WorksheetWorkspace.mode.restoreModeEditor(mode);
                WorksheetWorkspace.presentation.syncModeUI();
                check(WorksheetWorkspace.mode.currentMode() === mode, `Mode ${mode} unavailable`);
                check(
                    document.querySelector<HTMLElement>(`[data-mode="${mode}"]`)!.classList.contains('active'),
                    `Mode ${mode} tab inactive`);
                const sourceBeforeHelp: string = WorksheetContext.expr.value;
                await window.checkLabMobileLayout(mode + ' worksheet');
                WorksheetContext.help.click();
                const helpClose = (document.getElementById('helpClose')! as HTMLButtonElement);
                const workspace = document.getElementById('workspacePanel')!;
                const panel = document.getElementById('resultWorkspacePanel')!.getBoundingClientRect();
                const bar = document.querySelector<HTMLElement>('.lab-topbar')!.getBoundingClientRect();
                check(getComputedStyle(workspace).display === 'none', `${mode} editor hidden for Help`);
                check(Math.abs(panel.left - bar.left) < 2 && Math.abs(panel.right - bar.right) < 2,
                      `${mode} Help must span both workspace columns`);
                check(document.activeElement === helpClose, `${mode} Help return button is keyboard accessible`);
                for (const card of WorksheetContext.helpPane.querySelectorAll<HTMLElement>('.help-card')) {
                    const selected = card.dataset.helpModes!.split(',').includes(mode);
                    check(card.classList.contains('hidden') !== selected, `${mode} Help topic filtering`);
                }
                await window.checkLabMobileLayout(mode + ' help');
                helpClose.click();
                check(!document.getElementById('labWorkspace')!.classList.contains('help-open'),
                      `${mode} Help return restores the workspace`);
                check(document.activeElement === WorksheetContext.help && WorksheetContext.expr.value === sourceBeforeHelp,
                      `${mode} Help return preserves the input and restores focus`);
            }
            WorksheetWorkspace.mode.setMode('matrix');
            WorksheetWorkspace.presentation.syncModeUI();
            const matrixSource = '(x, 0; 0, x^2)';
            await WorksheetRequests.presentation.prepareLabEditor(matrixSource);
            WorksheetBindings.controller.setExpressionEditor(matrixSource);
            WorksheetContext.matrixOperation.value = 'eval';
            await WorksheetRequests.evaluation.evaluateMatrix();
            check(!WorksheetContext.rendered.classList.contains('error'), 'Shared matrix renderer failed');
            await window.checkLabMobileLayout('matrix result');
            await WorksheetRequests.plots.takeDerivative('x');
            check(!WorksheetContext.rendered.classList.contains('error'), 'Native matrix derivative construction failed');
            check(WorksheetContext.parsed.dataset.fullText!.length > 0, 'Matrix derivative result missing');
            await WorksheetRequests.plots.takeIntegral('x');
            check(!WorksheetContext.rendered.classList.contains('error'), 'Native matrix integral construction failed');
            check(WorksheetContext.parsed.dataset.fullText!.length > 0, 'Matrix integral result missing');
            WorksheetWorkspace.presentation.state.lastMatrixScalarExpression = 'x^2';
            await WorksheetRequests.plots.takeDerivative('x');
            check(!WorksheetContext.rendered.classList.contains('error'), 'Shared scalar-matrix calculus handler failed');
            check(WorksheetContext.parsed.dataset.fullText!.includes('x'), 'Scalar-matrix derivative missing');
            WorksheetWorkspace.presentation.state.lastMatrixScalarExpression = '';
            check(!labApplication.definitionScripts.includes('calculus'), 'Retired calculus script still loaded');
            WorksheetWorkspace.mode.setMode('datetime');
            WorksheetWorkspace.presentation.syncModeUI();
            const calendarResponse = await labHttp.fetch('/datetime-eval', {
                method: 'POST',
                body: labWire.encode({
                    date: '2026-10-09',
                    start: '2026-01-01',
                    end: '2027-01-01',
                    year: '2026',
                    latitude: '52.7077',
                    longitude: '-2.7541',
                    gmt_offset: '1',
                    elevation: '75',
                    jurisdiction: 'GB-ENG'
                })
            });
            const calendarData = await calendarResponse.labData() as LabTestCalendarEvaluation;
            check(calendarResponse.ok && calendarData.ok, 'Native DateTime evaluation failed');
            await WorksheetRequests.evaluation.installEvaluationResult(5, calendarData);
            await window.checkLabMobileLayout('all calendar result cards');
            for (const [element, name] of [
                     [WorksheetContext.rendered, 'overview'], [WorksheetContext.parsed, 'range'], [WorksheetContext.functionStyle, 'calendar'], [WorksheetContext.value, 'solar'],
                     [WorksheetContext.datetimeLocalBody, 'local']] as const) {
                const sections = calendarData[`${name}_sections`].filter(section => section.rows.length);
                check(sections.length > 0, `Native ${name} card has no sections`);
                check(
                    element.querySelectorAll<HTMLElement>('details.datetime-section').length === sections.length,
                    `Native ${name} card degraded to plain text`);
                check(
                    getComputedStyle(element.querySelector<HTMLElement>('.datetime-section-grid')!).display === 'grid',
                    `Native ${name} card lost its grid layout`);
                const details = element.querySelector('details')!;
                const open = details.open;
                details.querySelector('summary')!.click();
                check(details.open !== open, `Native ${name} section cannot collapse`);
                details.open = open;
                check(element.dataset.fullText === calendarData[name].trim(), `Native ${name} clipboard text changed`);
            }
            const englishHolidays = {
                Genna: 'Ethiopian Christmas',
                Timkat: 'Ethiopian Epiphany; commemorates the baptism of Jesus',
                Fasika: 'Ethiopian Easter',
                Enkutatash: 'Ethiopian New Year',
                Meskel: 'Commemoration of the finding of the True Cross'
            };
            const holidayLabels = [...WorksheetContext.functionStyle.querySelectorAll<HTMLElement>('.datetime-row-label')];
            for (const [name, translation] of Object.entries(englishHolidays)) {
                const label = holidayLabels.find(node => node.textContent === name);
                check(label?.title === translation, `${name} must retain its name and show its English tooltip`);
            }
            check(holidayLabels.filter(node => node.hasAttribute('title')).length === 17,
                  'All non-English built-in observances have native English tooltips');
            // Use native-produced, Protobuf-encoded astronomy fixtures through the real
            // request/evaluation/installation path; do not mock the result installer.
            const beforeCalendarFetch = window.fetch;
            const [almanacFixture] = window.labAlmanacFixtures();
            const homeHeaderPreserved = () => !!document.querySelector('#homeSky .sky-status-strip') &&
                WorksheetContext.statusEl.innerHTML === document.getElementById('homeSky')!.innerHTML;
            let almanacReply: LabTestAlmanacReply = {...almanacFixture, ok: true};
            window.fetch = (url, options) => String(url) === '/almanac-eval' ?
                Promise.resolve(
                    new Response(labWire.encode(almanacReply), {headers: {'Content-Type': 'application/x-protobuf'}})) :
                beforeCalendarFetch(url, options);
            try {
                WorksheetWorkspace.mode.setMode('almanac');
                WorksheetWorkspace.presentation.syncModeUI();
                await WorksheetRequests.evaluation.evaluateAlmanac({skipHistoryUpdate: true});
                check(
                    !WorksheetContext.rendered.classList.contains('error') &&
                        homeHeaderPreserved() &&
                        WorksheetContext.rendered.querySelectorAll<HTMLElement>('.almanac-grid-table:not(.almanac-event-table) tbody tr').length ===
                            4,
                    'Successful Almanac evaluation did not install a visible worksheet: ' + JSON.stringify({
                        status: WorksheetContext.statusEl.textContent,
                        error: WorksheetContext.rendered.classList.contains('error'),
                        rows: WorksheetContext.rendered.querySelectorAll('.almanac-body-table tbody tr').length,
                        text: WorksheetContext.rendered.textContent?.slice(0, 300)
                    }));
                WorksheetContext.rendered.querySelector<HTMLElement>('[data-almanac-visibility="visible"]')!.click();
                await window.checkLabMobileLayout('almanac result');
                check(
                    WorksheetContext.rendered.querySelectorAll<HTMLElement>('.almanac-grid-table:not(.almanac-event-table) tbody tr').length === 1 &&
                        WorksheetContext.rendered.dataset.copyText === almanacFixture.almanac_presentation.visible.copy_text,
                    'Evaluated Almanac cannot switch to the native visible-body variant');
                const acceptedAlmanac = WorksheetContext.worksheetSession.calendar.almanacLastWorksheetData;
                almanacReply = {ok: true, visibility: 'all'};
                await WorksheetRequests.evaluation.evaluateAlmanac({skipHistoryUpdate: true});
                check(
                    WorksheetContext.rendered.classList.contains('error') && WorksheetContext.rendered.textContent.includes('Almanac presentation') &&
                        homeHeaderPreserved() &&
                        WorksheetContext.worksheetSession.calendar.almanacLastWorksheetData === acceptedAlmanac,
                    'Incomplete Almanac response lost its error, replaced accepted state or changed the home header');
                almanacReply = {...almanacFixture, ok: true, visibility: 'visible'};
                await WorksheetRequests.evaluation.evaluateAlmanac({skipHistoryUpdate: true});
                check(
                    !WorksheetContext.rendered.classList.contains('error') && WorksheetContext.rendered.querySelector<HTMLElement>('.almanac-grid-table')! &&
                        homeHeaderPreserved(),
                    'Almanac could not recover after an incomplete response');
            } finally {
                window.fetch = beforeCalendarFetch;
                WorksheetWorkspace.mode.setMode('datetime');
                WorksheetWorkspace.presentation.syncModeUI();
                await WorksheetRequests.evaluation.installEvaluationResult(5, calendarData);
            }
            const dateButton = document.querySelector<HTMLElement>('[data-date-target="datetimeDate"]')!;
            dateButton.click();
            check(!WorksheetContext.marsDatePicker.classList.contains('hidden'), 'Date picker did not open');
            check(WorksheetContext.marsDatePickerGrid.children.length >= 28, 'Date grid missing');
            WorksheetContext.marsDatePickerClose.click();
            check(WorksheetContext.marsDatePicker.classList.contains('hidden'), 'Date picker did not close');
            const restartHistory = new WorksheetHistoryPersistence();
            await restartHistory.start();
            WorksheetWorkspace.mode.setMode('expression');
            WorksheetWorkspace.presentation.syncModeUI();
            await WorksheetRequests.calendarRequests.refreshMobileAccess();
            check(WorksheetContext.mobileTitle.textContent.length > 0, 'Mobile access metadata missing');
            // Page-hide persistence reads the editor, not merely the explicit saved-state fixture.
            await WorksheetRequests.presentation.prepareLabEditor('3+4');
            WorksheetBindings.controller.setExpressionEditor('3+4');
            document.dispatchEvent(new Event('input'));
            await WorksheetPersistence.persistence.saveWorksheet('expression', '3+4');
            await restartHistory.flush();
            // Matrix evaluation persists its mode; restore the restart fixture explicitly.
            const resetMode =
                await labHttp.fetch('/state', {method: 'POST', body: labWire.encode({lab_mode: 'expression'})});
            check(resetMode.ok, 'Could not restore expression-mode restart fixture');
            const saved = await (await labHttp.fetch('/state')).labData() as {expression: string};
            check(saved.expression === '3+4', 'Worksheet state not saved');
            check(!failures.length, failures.join('; '));
            location.href = '/?failure=catalogue';
        } catch (caught: unknown) {
            const error = caught as Error;
            await report(`FAIL: ${error.message}\n${error.stack || ''}`);
        }
    });
})();
