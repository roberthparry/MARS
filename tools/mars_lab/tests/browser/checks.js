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
            const config = labWire.decode(await response.arrayBuffer());
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
    function check(condition, message) {
        if (!condition)
            throw new Error(message);
    }
    async function checkAsynchronousPlot() {
        const original = fetchEvaluation;
        let release, started;
        try {
            fetchEvaluation = (...args) => {
                if (args[2] !== 'plot')
                    return original(...args);
                started = true;
                return new Promise(resolve => release = resolve);
            };
            setExpressionEditor('sin(x)');
            await evaluateExpression();
            for (let i = 0; i < 100 && !started; ++i)
                await new Promise(resolve => setTimeout(resolve, 10));
            check(started && !labRequests.busy(), 'Evaluate must finish before the plot response');
            check(rendered.querySelector('svg') && parsed.textContent && functionStyle.textContent,
                  'Mathematical cards appear while the plot is pending');
            const mathematics = rendered.innerHTML;
            release({response: {ok: true}, data: {ok: false, error: 'Plot fixture unavailable'}});
            await waitForAutomaticPlot();
            check(document.getElementById('plotRendered').textContent.includes('Plot fixture unavailable') &&
                  rendered.innerHTML === mathematics && !rendered.classList.contains('error'),
                  'A plot failure must not replace successful mathematics');
            started = false;
            await evaluateExpression();
            for (let i = 0; i < 100 && !started; ++i)
                await new Promise(resolve => setTimeout(resolve, 10));
            check(started, 'A fresh evaluation starts a fresh plot');
            saveCurrentModeResultState();
            setMode('matrix');
            syncModeUI();
            const previous = document.getElementById('plotRendered').innerHTML;
            release({response: {ok: true}, data: {ok: false, error: 'Obsolete plot fixture'}});
            await new Promise(resolve => setTimeout(resolve, 50));
            check(document.getElementById('plotRendered').innerHTML === previous,
                  'A late plot must not publish after switching modes');
            setMode('expression');
            syncModeUI();
            restoreModeResultState();
            check(document.getElementById('plotRendered').textContent.includes('Plot cancelled'),
                  'Restoring cancelled background work must not leave a permanent loading message');
        } finally {
            if (release)
                release({response: {ok: false}, data: {ok: false}});
            fetchEvaluation = original;
            setMode('expression');
            syncModeUI();
        }
    }
    async function waitForAutomaticPlot() {
        const node = document.getElementById('plotRendered');
        for (let i = 0; i < 1200 && (!node.textContent || node.textContent.includes('Generating plot')); ++i)
            await new Promise(resolve => setTimeout(resolve, 25));
        check(!node.textContent.includes('Generating plot'), 'Automatic plot must finish');
    }
    function checkPlotLabelSpacing() {
        const rendered = document.getElementById('plotRendered');
        const boxes = [...rendered.querySelectorAll('[data-plot-label]')].map(label => label.getBBox());
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
        const rendered = document.getElementById('plotRendered');
        check(!document.querySelector('#plotRange select, #plotRange .select-shell'),
              'Plot settings must not contain dropdowns');
        for (const id of ['plotKind', 'plotScale']) {
            const control = document.getElementById(id), saved = control.value;
            const group = document.getElementById(id + 'Choices');
            const radios = [...group.querySelectorAll('input[type="radio"]')];
            if (!group.getClientRects().length)
                continue;
            check(control.type === 'hidden' && radios.length === 3,
                  'Plot choices expose Auto alongside their overrides');
            try {
                control.value = 'auto';
                control.dispatchEvent(new Event('change', {bubbles: true}));
                check(radios.every(radio => radio.checked === (radio.value === 'auto')),
                      'Automatic is the default selection where exposed');
                for (const radio of radios) {
                    if (radio.closest('label').classList.contains('hidden'))
                        continue;
                    radio.click();
                    check(control.value === radio.value && radio.checked &&
                          radios.filter(option => option.checked).length === 1,
                          'Radio overrides must be mutually exclusive and reach native payload controls');
                    if (id === 'plotScale') {
                        check(document.getElementById('plotManual').classList.contains('hidden') ===
                              (radio.value !== 'manual'), 'Manual limits should appear only when selected');
                        check([...document.querySelectorAll('.plot-ranges .plot-bound-pair')].every(pair =>
                                  pair.classList.contains('hidden') === (radio.value === 'auto')),
                              'Auto hides both horizontal bound pairs');
                        if (radio.value === 'auto')
                            check(['plotLower', 'plotUpper', 'plotSecondLower', 'plotSecondUpper', 'plotYLower', 'plotYUpper']
                                      .every(id => document.getElementById(id).value === ''),
                                  'Auto clears hidden bounds');
                    }
                }
                await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
                const card = group.closest('.result-card').getBoundingClientRect();
                for (const label of group.querySelectorAll('label')) {
                    if (label.classList.contains('hidden'))
                        continue;
                    const box = label.getBoundingClientRect();
                    check(box.left >= card.left && box.right <= card.right,
                          'Radio options must stay inside normal, expanded and narrow plot cards');
                }
                radios.find(radio => radio.value === 'auto').click();
                check(control.value === 'auto' && radios.every(radio => radio.checked === (radio.value === 'auto')),
                      'Auto restores automatic selection without a Reset button');
                const override = radios.find(radio => radio.value !== 'auto' &&
                                             !radio.closest('label').classList.contains('hidden'));
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
        const rendered = document.getElementById('plotRendered');
        check(!document.getElementById('plotUpdate') && !document.querySelector('#plotRange [data-plot-reset]'),
              'Live plot controls have no Update or Reset buttons');
        const lower = document.getElementById('plotLower'), upper = document.getElementById('plotUpper');
        const saved = [lower.value, upper.value];
        const savedScale = document.getElementById('plotScale').value;
        document.querySelector('#plotScaleChoices input[value="all"]').click();
        const waitFor = async predicate => {
            const deadline = performance.now() + 15000;
            while (!predicate() && performance.now() < deadline)
                await new Promise(resolve => setTimeout(resolve, 25));
            check(predicate(), 'Live range edit must complete without an Update button');
        };
        const before = rendered.querySelector('[data-series]');
        lower.value = '-';
        lower.dispatchEvent(new Event('input', {bubbles: true}));
        await waitFor(() => !document.getElementById('plotFeedback').classList.contains('hidden'));
        check(rendered.querySelector('[data-series]') === before && !rendered.classList.contains('error'),
              'Incomplete live bounds retain the last valid plot');
        lower.value = '-6';
        upper.value = '6';
        lower.dispatchEvent(new Event('input', {bubbles: true}));
        upper.dispatchEvent(new Event('input', {bubbles: true}));
        await waitFor(() => rendered.querySelector('[data-series]') !== before);
        check(document.getElementById('plotFeedback').classList.contains('hidden'),
              'Correcting the bounds clears live validation feedback');
        lower.value = saved[0];
        upper.value = saved[1];
        document.getElementById('plotScale').value = savedScale;
        await takePlot(document.getElementById('plotFirst').value);
    }
    function checkPlotRangeAlignment() {
        const rendered = document.getElementById('plotRendered');
        const groups = [...document.querySelectorAll('.plot-ranges > .plot-group')]
            .filter(group => group.getClientRects().length);
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
    function checkPlotControls(surface) {
        const rendered = document.getElementById('plotRendered');
        checkPlotRangeAlignment();
        const visible = id => document.getElementById(id).getClientRects().length > 0;
        check(visible('plotCamera') === surface && visible('plotVertical') &&
              document.getElementById('plotVerticalTitle').textContent === (surface ? 'Height range' : 'Vertical range'),
              'Vertical limits must remain available for curves and surfaces with a clear dimension label');
        const first = [...document.querySelectorAll('#plotFirstChoices input')];
        const second = [...document.querySelectorAll('#plotSecondChoices input')];
        check(first.length > 0 && first.every(radio => radio.type === 'radio'), 'Variables must be radio choices');
        check(document.getElementById('plotSecond').type === 'hidden', 'No wide free-text variable input');
        check(second.length === first.length + 1 && second[0].value === '',
              'Second-axis choices include Auto and the native variables');
        for (const pair of document.querySelectorAll('.plot-bound-pair')) {
            if (!pair.getClientRects().length)
                continue;
            const fields = [...pair.querySelectorAll('.plot-field')].map(field => field.getBoundingClientRect());
            check(fields.length === 2 && Math.abs(fields[0].top - fields[1].top) < 2,
                  'Each From/To or Minimum/Maximum pair must stay together on one row');
        }
        if (surface) {
            const x = first.find(radio => radio.value === 'x'), y = first.find(radio => radio.value === 'y');
            check(x?.checked && y, 'The plotted first axis should be selected');
            second.find(radio => radio.value === 'y').click();
            y.click();
            check(document.getElementById('plotFirst').value === 'y' &&
                  document.getElementById('plotSecond').value === '' &&
                  second.find(radio => radio.value === 'y').disabled,
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
        while ((labSurfacePreview.snapshot()?.volume.dirty || labRequests.busy()) && performance.now() < deadline)
            await new Promise(resolve => setTimeout(resolve, 25));
        check(!labSurfacePreview.snapshot()?.volume.dirty && !labRequests.busy(),
              'Native volume refinement must finish without losing the interactive chart');
    }
    async function checkPlotDrag() {
        const rendered = document.getElementById('plotRendered');
        const controls = ['plotAzimuth', 'plotElevation', 'plotRoll'].map(id => document.getElementById(id));
        const retained = rendered.querySelector('[data-surface]');
        const generation = labRequests.latestMain();
        const settle = () => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
        const pointer = (target, type, x, y, extras = {}) => target.dispatchEvent(new PointerEvent(type, {
            pointerId: 91, pointerType: 'touch', isPrimary: true, button: 0,
            buttons: type === 'pointerup' ? 0 : 1, clientX: x, clientY: y,
            bubbles: true, cancelable: true, ...extras
        }));
        const box = retained.getBoundingClientRect(), cx = box.left + box.width / 2, cy = box.top + box.height / 2;
        const quaternion = () => labSurfacePreview.snapshot().orientation;
        const initial = quaternion();
        check(!pointer(retained, 'pointerdown', cx, cy), 'Surface drag reserves touch movement');
        pointer(window, 'pointermove', cx + 80, cy - 60);
        await settle();
        check(quaternion().some((v, i) => Math.abs(v - initial[i]) > 0.01),
              'Finger drag changes the quaternion camera locally');
        pointer(window, 'pointerup', cx + 80, cy - 60);
        await settle();
        check(!rendered.marsPlotDrag.busy && !rendered.marsPlotDrag.active &&
              retained.isConnected && retained.dataset.localRenderer && labRequests.latestMain() === generation,
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
        for (const elevation of [-90, -89.999, 0, 89.999, 90]) {
            const q = labSurfaceCamera.fromAngles([37, elevation, 123]);
            const decoded = labSurfaceCamera.angles(q, [37, elevation, 123]);
            const restored = labSurfaceCamera.fromAngles(decoded);
            check(Math.abs(q.reduce((sum, v, i) => sum + v * restored[i], 0)) > 1 - 1e-9,
                  'Euler readouts reconstruct orientation even at either pole');
            const moved = labSurfaceCamera.drag(q, [cx, cy], [cx + 20, cy + 10], box, false);
            check(moved.every(Number.isFinite) && Math.abs(q.reduce((sum, v, i) => sum + v * moved[i], 0)) < 0.99999,
                  'Trackball remains responsive at the Euler singularities');
        }
        const exported = new DOMParser().parseFromString(copyTextForTarget('plot'), 'image/svg+xml');
        check(!exported.querySelector('parsererror') && exported.querySelectorAll('path').length > 1000 &&
              !exported.querySelector('canvas') && exported.querySelector('text'),
              'Copy exports current mesh and axes as standalone SVG');
        const exportedBefore = copyTextForTarget('plot');
        retained.dispatchEvent(new WheelEvent('wheel', {deltaY: -100, altKey: true, bubbles: true, cancelable: true}));
        await new Promise(resolve => setTimeout(resolve, 450));
        check(retained.isConnected && retained.dataset.localRenderer && labRequests.latestMain() === generation &&
              exportedBefore !== copyTextForTarget('plot'),
              'Wheel rotation stays local and SVG export follows the latest camera');
        const chart = rendered.querySelector('[data-surface]');
        const angles = ['azimuth', 'elevation', 'roll'].map(key => Number(chart.dataset[key]));
        const frame = chart.closest('.rendered-zoom-frame'), figure = frame.querySelector('svg');
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
            const overlay = rendered.querySelector('[data-surface-overlay]');
            const expected = chart.getBoundingClientRect(), actual = overlay.getBoundingClientRect();
            check(overlay.parentElement === rendered && !chart.querySelector('foreignObject') &&
                  ['left', 'top', 'width', 'height'].every(key => Math.abs(expected[key] - actual[key]) < 1),
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
        sessionStorage.setItem('labSurfaceRenderer', selected);
        const probe = document.createElement('canvas').getContext('webgl');
        if (probe) {
            check(selected === 'webgl', 'Available WebGL must use the GPU renderer rather than silently failing shaders');
            probe.getExtension('WEBGL_lose_context')?.loseContext();
        }
        if (selected === 'webgl') {
            const gl = rendered.querySelector('[data-surface-overlay] canvas').getContext('webgl');
            check(!gl.isContextLost() && gl.getError() === gl.NO_ERROR, 'GPU surface draw has no WebGL errors');
            const program = gl.getParameter(gl.CURRENT_PROGRAM);
            const camera = gl.getUniform(program, gl.getUniformLocation(program, 'camera'));
            const keys = ['xMin', 'xMax', 'yMin', 'yMax', 'zMin', 'zMax'];
            const bounds = keys.map(key => Number(chart.dataset[key]));
            for (let axis = 0; axis < 3; axis++) {
                const centre = (bounds[axis * 2] + bounds[axis * 2 + 1]) / 2;
                bounds[axis * 2] = centre + (bounds[axis * 2] - centre) / 2;
                bounds[axis * 2 + 1] = centre + (bounds[axis * 2 + 1] - centre) / 2;
            }
            check(labSurfacePreview.draw(chart, angles, bounds), 'WebGL magnifies the retained scene for zoom');
            const zoomCamera = gl.getUniform(program, gl.getUniformLocation(program, 'camera'));
            const scaling = gl.getUniform(program, gl.getUniformLocation(program, 'domainScale'));
            check([0, 1, 4, 5, 8, 9, 12, 13].every(i => camera[i] === zoomCamera[i]) &&
                  [...scaling].every(value => Math.abs(value - 2) < 1e-6),
                  'Volume zoom doubles the mesh transform while retaining the fixed axis projection');
        }
        labSurfacePreview.dispose();
        const getContext = HTMLCanvasElement.prototype.getContext;
        try {
            HTMLCanvasElement.prototype.getContext = function(type, ...args) {
                return type === 'webgl' ? null : getContext.call(this, type, ...args);
            };
            check(labSurfacePreview.draw(chart, angles) && chart.dataset.localRenderer === 'canvas',
                  'Unavailable WebGL uses a local canvas, not server sampling');
            const canvas = rendered.querySelector('[data-surface-overlay] canvas');
            const pixels = canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height).data;
            let painted = false;
            for (let i = 3; i < pixels.length; i += 64) painted ||= pixels[i] !== 0;
            check(painted, 'Canvas fallback actually paints the retained triangles');
        } finally {
            HTMLCanvasElement.prototype.getContext = getContext;
            labSurfacePreview.dispose();
        }
        check(!rendered.querySelector('[data-surface-overlay]') && !chart.dataset.localRenderer,
              'Local-renderer disposal restores SVG interaction and releases its overlay');
        labSurfacePreview.draw(chart, angles);
        document.getElementById('plotElevation').value = '-80';
        document.getElementById('plotElevation').dispatchEvent(new Event('input', {bubbles: true}));
        await settle();
        document.getElementById('plotElevation').dispatchEvent(new Event('change', {bubbles: true}));
        await settle();
        const below = labSurfacePreview.snapshot().orientation;
        await takePlot(document.getElementById('plotFirst').value);
        check(labSurfacePreview.snapshot().orientation.every((v, i) => Math.abs(v - below[i]) < 1e-10),
              'Native geometry refresh preserves a camera below the former elevation limit');
        const refreshed = rendered.querySelector('[data-surface]');
        const context = rendered.querySelector('[data-surface-overlay] canvas').getContext('webgl');
        const loss = context?.getExtension('WEBGL_lose_context');
        if (loss) {
            const beforeLoss = labSurfacePreview.snapshot();
            loss.loseContext();
            const deadline = performance.now() + 3000;
            while (refreshed.dataset.localRenderer !== 'canvas' && performance.now() < deadline)
                await new Promise(resolve => setTimeout(resolve, 25));
            check(refreshed.dataset.localRenderer === 'canvas' &&
                  labSurfacePreview.snapshot().orientation.every((v, i) => Math.abs(v - beforeLoss.orientation[i]) < 1e-10),
                  'Context loss switches to canvas without losing the quaternion camera');
        }
    }
    async function checkSurfacePinch() {
        const rendered = document.getElementById('plotRendered');
        const chart = rendered.querySelector('[data-surface]'), box = chart.getBoundingClientRect();
        const cx = box.left + box.width / 2, cy = box.top + box.height / 2;
        const send = (target, type, id, x) => target.dispatchEvent(new PointerEvent(type,
            {pointerId: id, pointerType: 'touch', isPrimary: id === 71, button: 0,
             buttons: type === 'pointerup' ? 0 : 1, clientX: x, clientY: cy, bubbles: true, cancelable: true}));
        const settle = async () => {
            for (let i = 0; i < 2; ++i) await new Promise(resolve => requestAnimationFrame(resolve));
        };
        const generation = labRequests.latestMain();
        for (const cancelled of [false, true]) {
            const initial = labSurfacePreview.snapshot().camera;
            send(chart, 'pointerdown', 71, cx - 30);
            send(chart, 'pointerdown', 72, cx + 30);
            send(window, 'pointermove', 71, cx - 60);
            send(window, 'pointermove', 72, cx + 60);
            await settle();
            check(Math.abs(labSurfacePreview.snapshot().camera.scale - initial.scale * 2) < 1e-10,
                  'Two-finger spread magnifies the retained camera continuously');
            send(window, cancelled ? 'pointercancel' : 'pointerup', 72, cx + 60);
            await settle();
            check(chart.isConnected && labRequests.latestMain() === generation && !rendered.marsPlotDrag.active &&
                  Math.abs(labSurfacePreview.snapshot().camera.scale - initial.scale * (cancelled ? 1 : 2)) < 1e-10,
                  'Pinch release keeps the camera; cancellation restores it without native resampling');
            send(window, 'pointerup', 71, cx - 60);
        }
        rendered.closest('.result-card').querySelector('[data-zoom-reset]').click();
        await settle();
        check(labSurfacePreview.snapshot().camera.scale === 1, 'Reset restores the volume after pinch');
        await waitForVolume();
    }

    async function checkPlotPinch() {
        const rendered = document.getElementById('plotRendered');
        const chart = () => rendered.querySelector('[data-plot-chart][data-x-min]');
        const surface = chart().hasAttribute('data-surface');
        if (surface) {
            await checkSurfacePinch();
            return;
        }
        const coordinateKeys = surface ? ['xMin', 'xMax', 'yMin', 'yMax', 'zMin', 'zMax'] :
                                         ['xMin', 'xMax', 'yMin', 'yMax'];
        const original = coordinateKeys.map(key => Number(chart().dataset[key]));
        const card = rendered.closest('.result-card');
        const wait = async previous => {
            const deadline = performance.now() + 20000;
            while ((rendered.marsPlotDrag.busy || chart() === previous) && performance.now() < deadline)
                await new Promise(resolve => setTimeout(resolve, 25));
            check(!rendered.marsPlotDrag.busy && chart() !== previous, 'Pinch finishes with a native redraw');
            await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
        };
        for (const factor of [0.5, 2]) {
            const before = chart(), matrix = before.getScreenCTM(), view = before.viewBox.baseVal;
            const camera = ['azimuth', 'elevation', 'roll'].map(key => before.dataset[key]);
            const heightPolicy = document.getElementById('plotScale').value;
            const cx = Math.round(matrix.e + (surface ? view.width / 2 :
                Number(before.dataset.plotLeft) + Number(before.dataset.plotWidth) * 0.4) * matrix.a);
            const cy = Math.round(matrix.f + (surface ? view.height / 2 :
                Number(before.dataset.plotTop) + Number(before.dataset.plotHeight) * 0.55) * matrix.d);
            const send = (target, type, id, x) => target.dispatchEvent(new PointerEvent(type,
                {pointerId: id, pointerType: 'touch', isPrimary: id === 71, button: 0,
                 buttons: type === 'pointerup' ? 0 : 1, clientX: x, clientY: cy,
                 bubbles: true, cancelable: true}));
            check(!send(before, 'pointerdown', 71, cx - 30), 'First pinch finger starts the plot gesture');
            check(!send(before, 'pointerdown', 72, cx + 30) && rendered.marsPlotDrag.pinch,
                  'Second finger promotes dragging to pinch zoom');
            const geometry = [...rendered.marsPlotDrag.pinchGeometry];
            send(before, 'pointerdown', 73, cx);
            send(window, 'pointermove', 73, cx + 90);
            check(!rendered.marsPlotDrag.changed, 'A third finger does not alter pinch zoom');
            send(window, 'pointermove', 71, cx - 30 / factor);
            send(window, 'pointermove', 72, cx + 30 / factor);
            if (surface) {
                const generation = labRequests.latestMain();
                await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
                check(chart() === before && !!before.dataset.localRenderer && labRequests.latestMain() === generation,
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
            for (const [axis, lo, hi] of [[0, 'xMin', 'xMax'], [1, 'yMin', 'yMax']]) {
                const fraction = geometry[4 + axis];
                const anchor = Number(before.dataset[lo]) * (1 - fraction) + Number(before.dataset[hi]) * fraction;
                const after = Number(chart().dataset[lo]) * (1 - fraction) + Number(chart().dataset[hi]) * fraction;
                check(Math.abs(after - anchor) < 1e-7, 'Pinch preserves the midpoint anchor');
            }
            if (surface)
                check(['azimuth', 'elevation', 'roll'].every((key, i) => chart().dataset[key] === camera[i]) &&
                      document.getElementById('plotScale').value === 'manual',
                      'Surface pinch preserves the camera and locks the scaled height viewport');
            check(!rendered.marsPlotDrag.active && !rendered.marsPlotDrag.pinch,
                  'Releasing either finger ends pinch and restores full-detail rendering');
            send(window, 'pointermove', 71, cx + 100);
            check(!rendered.marsPlotDrag.active, 'The remaining finger cannot jump back into dragging');
            send(window, 'pointerup', 71, cx + 100);
            const zoomed = chart();
            card.querySelector('[data-zoom-reset]').click();
            await wait(zoomed);
            check(coordinateKeys.every((key, i) =>
                Math.abs(Number(chart().dataset[key]) - original[i]) < 1e-7), 'Reset restores the pre-pinch viewport');
            check(document.getElementById('plotScale').value === heightPolicy,
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
        const rendered = document.getElementById('plotRendered');
        const settle = () => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
        await settle();
        const card = rendered.closest('.result-card');
        const chart = () => rendered.querySelector('[data-plot-chart][data-x-min]:not([data-surface])');
        const before = chart(), width = rendered.querySelector('svg').getBoundingClientRect().width;
        const matrix = before.getScreenCTM();
        const left = matrix.e + Number(before.dataset.plotLeft) * matrix.a;
        const top = matrix.f + Number(before.dataset.plotTop) * matrix.d;
        const plotWidth = Number(before.dataset.plotWidth) * matrix.a;
        const plotHeight = Number(before.dataset.plotHeight) * matrix.d;
        const clientX = left + plotWidth * 0.25, clientY = top + plotHeight * 0.6;
        const x0 = Number(before.dataset.xMin), x1 = Number(before.dataset.xMax);
        const y0 = Number(before.dataset.yMin), y1 = Number(before.dataset.yMax);
        const anchor = x0 * 0.75 + x1 * 0.25;
        rendered.dispatchEvent(new PointerEvent('pointermove', {clientX, clientY, bubbles: true}));
        const tip = rendered.querySelector('.plot-pointer-tip'), line = rendered.querySelector('.plot-pointer-line');
        const hoverX = Number(before.dataset.plotLeft) + Number(before.dataset.plotWidth) * 0.25;
        const crossings = [...before.querySelectorAll('[data-series] > path')].flatMap(path => {
            const p = path.getAttribute('d').split(' ');
            const ax = Number(p[1]), ay = Number(p[2]), bx = Number(p[4]), by = Number(p[5]);
            if (p.length !== 6 || ax === bx || hoverX < Math.min(ax, bx) || hoverX > Math.max(ax, bx)) return [];
            const height = ay + (hoverX - ax) * (by - ay) / (bx - ax);
            return [y1 + (height - Number(before.dataset.plotTop)) / Number(before.dataset.plotHeight) * (y0 - y1)];
        });
        const reportedY = Number(tip.textContent.split('\n')[1]?.replace('y ≈ ', ''));
        const horizontal = rendered.querySelector('.plot-pointer-horizontal');
        check(horizontal && !horizontal.classList.contains('hidden') &&
              Math.abs(horizontal.getBoundingClientRect().top - (top + (y1 - reportedY) / (y1 - y0) * plotHeight)) < 2 &&
              Math.abs(horizontal.getBoundingClientRect().width - plotWidth) < 2 &&
              getComputedStyle(horizontal).pointerEvents === 'none',
              'Horizontal hover guide spans the chart at the reported curve value');
        const marker = rendered.querySelector('.plot-pointer-marker');
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
                .matrixTransform(tick.getScreenCTM());
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
        const waitChart = async previous => {
            const deadline = performance.now() + 15000;
            while ((!chart() || chart() === previous) && performance.now() < deadline)
                await new Promise(resolve => setTimeout(resolve, 25));
            check(chart() && chart() !== previous, 'Viewport zoom requests a newly sampled plot');
            await settle();
        };
        const zoomEvent = new WheelEvent('wheel',
              {clientX, clientY, deltaY: -100, ctrlKey: true, bubbles: true, cancelable: true});
        const wheelFraction = (zoomEvent.clientX - left) / plotWidth;
        const wheelAnchor = x0 * (1 - wheelFraction) + x1 * wheelFraction;
        check(!before.dispatchEvent(zoomEvent),
              'Ctrl+wheel viewport zoom prevents page scrolling');
        await waitChart(before);
        const after = chart();
        check(document.getElementById('plotLower').value.length < 10 &&
              document.getElementById('plotYLower').value.length < 10,
              'Ordinary zoom bounds must use short interval-aware labels');
        check(Math.abs(Number(after.dataset.xMin) - (wheelAnchor + (x0 - wheelAnchor) * 0.8)) < 1e-8 &&
              Math.abs(Number(after.dataset.xMax) - Number(after.dataset.xMin) - (x1 - x0) * 0.8) < 1e-8 &&
              Math.abs(Number(after.dataset.yMax) - Number(after.dataset.yMin) - (y1 - y0) * 0.8) < 1e-8,
              'Wheel zoom shrinks both axis ranges around the pointer: ' +
              JSON.stringify({x0, x1, y0, y1, wheelAnchor, bounds: {...after.dataset}}));
        const plotStyle = getComputedStyle(rendered);
        const availableWidth = rendered.clientWidth - parseFloat(plotStyle.paddingLeft) - parseFloat(plotStyle.paddingRight);
        check(Math.abs(rendered.querySelector('svg').getBoundingClientRect().width - availableWidth) < 2,
              'Viewport zoom must not enlarge the plot image: ' +
              JSON.stringify({before: width, after: rendered.querySelector('svg').getBoundingClientRect().width,
                              card: card.getBoundingClientRect().width, rendered: rendered.clientWidth}));
        card.querySelector('[data-zoom-step="1"]').click();
        await waitChart(after);
        const centred = chart();
        check(Math.abs(Number(centred.dataset.xMax) - Number(centred.dataset.xMin) - (x1 - x0) * 0.64) < 1e-8,
              'The plus button zooms the viewport rather than the result card');
        card.querySelector('[data-zoom-reset]').click();
        await waitChart(centred);
        check(Math.abs(Number(chart().dataset.xMin) - x0) < 1e-8 &&
              Math.abs(Number(chart().dataset.xMax) - x1) < 1e-8,
              'Reset restores the original pre-zoom view');
        for (const pointerType of ['mouse', 'touch']) {
            const initial = chart(), transform = initial.getScreenCTM();
            const pw = Number(initial.dataset.plotWidth) * transform.a;
            const ph = Number(initial.dataset.plotHeight) * transform.d;
            const cx = Math.round(transform.e + Number(initial.dataset.plotLeft) * transform.a + pw / 2);
            const cy = Math.round(transform.f + Number(initial.dataset.plotTop) * transform.d + ph / 2);
            const event = (target, type, x, y, id = 41) => target.dispatchEvent(new PointerEvent(type,
                {clientX: x, clientY: y, pointerId: id, pointerType, isPrimary: id === 41,
                 button: 0, buttons: type === 'pointerup' ? 0 : 1, bubbles: true, cancelable: true}));
            check(!event(initial, 'pointerdown', cx, cy), 'A primary pointer starts curve panning');
            event(window, 'pointermove', cx + 40, cy + 25, 42);
            check(!rendered.marsPlotDrag.changed, 'A second pointer cannot move the curve');
            event(window, 'pointermove', cx + 40, cy + 25);
            check(initial.querySelector('[data-series]').getAttribute('transform')?.startsWith('translate(') &&
                  initial.querySelector('[data-series]').closest('[data-plot-window]'),
                  'Curves follow the pointer immediately inside a clipped viewport, before any server reply');
            event(window, 'pointerup', cx + 40, cy + 25);
            const deadline = performance.now() + 20000;
            while ((rendered.marsPlotDrag.busy || chart() === initial) && performance.now() < deadline)
                await new Promise(resolve => setTimeout(resolve, 25));
            check(!rendered.marsPlotDrag.busy && !rendered.marsPlotDrag.active &&
                  Math.abs(Number(chart().dataset.xMin) - (x0 - 40 * (x1 - x0) / pw)) < 1e-8 &&
                  Math.abs(Number(chart().dataset.yMin) - (y0 + 25 * (y1 - y0) / ph)) < 1e-8 &&
                  Math.abs(Number(chart().dataset.xMax) - Number(chart().dataset.xMin) - (x1 - x0)) < 1e-8,
                  'Dragging pans both ranges without changing scale for ' + pointerType);
            const panned = chart();
            card.querySelector('[data-zoom-reset]').click();
            await waitChart(panned);
            check(Math.abs(Number(chart().dataset.xMin) - x0) < 1e-8 &&
                  Math.abs(Number(chart().dataset.yMin) - y0) < 1e-8, 'Reset restores the pre-pan view');
        }
        const cancelled = chart();
        const cancelMatrix = cancelled.getScreenCTM();
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
        while (rendered.marsPlotDrag.busy && performance.now() < cancelDeadline)
            await new Promise(resolve => setTimeout(resolve, 25));
        check(!rendered.marsPlotDrag.busy && !rendered.marsPlotDrag.active &&
              Math.abs(Number(chart().dataset.xMin) - x0) < 1e-8 &&
              Math.abs(Number(chart().dataset.yMin) - y0) < 1e-8,
              'Escape restores both original ranges even with a pan redraw in flight');
        await checkPlotPinch();
    }
    async function checkPlotWheelZoom() {
        const rendered = document.getElementById('plotRendered');
        const settle = () => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
        await settle();
        if (rendered.querySelector('[data-plot-chart][data-x-min]:not([data-surface])')) {
            await checkCurvePointerView();
            return;
        }
        const card = rendered.closest('.result-card');
        const chart = () => rendered.querySelector('[data-surface]');
        const controls = ['plotLower', 'plotUpper', 'plotSecondLower', 'plotSecondUpper',
                          'plotYLower', 'plotYUpper', 'plotScale'];
        const original = controls.map(id => document.getElementById(id).value);
        const initial = labSurfacePreview.snapshot(), before = chart();
        const view = labSurfacePreview.prepare(before);
        const frame = view.framePlan;
        const spans = [0, 1, 2].map(i => initial.volume.bounds[2*i+1] - initial.volume.bounds[2*i]);
        const unitScale = view.units[0] / spans[0];
        check(view.units.every((v, i) => Math.abs(v / spans[i] - unitScale) < 1e-10),
              'All surface types preserve equal coordinate units, including unequal height ranges');
        const projected = [0, 1].map(row =>
            frame.rows.slice(row*3, row*3+3).reduce((sum, v) => sum + Math.abs(v), 0) * frame.fit);
        const margin = Math.min(Math.max(initial.volume.labelWidth + 25, 45), (frame.width - 80)/2);
        check(projected[0] <= frame.width - 2*margin + 1e-7 &&
              projected[1] <= frame.height - 100 + 1e-7 &&
              (Math.abs(projected[0] - (frame.width - 2*margin)) < 1e-7 ||
               Math.abs(projected[1] - (frame.height - 100)) < 1e-7),
              'The projected volume fills an available dimension without stretching or clipping');
        const lowControl = document.getElementById('plotLower'), highControl = document.getElementById('plotUpper');
        const lowBefore = lowControl.value, highBefore = highControl.value;
        lowControl.value = 'pi-1'; highControl.value = 'pi+1';
        const symbolicView = {chart: before, units: [...view.units]};
        labDOM.call('lab_surface_volume_init', symbolicView, null, 0);
        lowControl.value = lowBefore; highControl.value = highBefore;
        check(symbolicView.volume.exact[0] === 'pi-1' && symbolicView.volume.exact[1] === 'pi+1',
              'C retains authored symbolic bounds rather than rebasing them through decimal metadata');
        const axes = [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]].map(view.project);
        const wheel = deltaY => chart().dispatchEvent(new WheelEvent('wheel',
            {deltaY, ctrlKey: true, bubbles: true, cancelable: true}));
        check(!wheel(-100), 'Volume wheel zoom prevents page scrolling');
        await settle();
        const zoomed = labSurfacePreview.snapshot();
        for (let axis = 0; axis < 3; ++axis) {
            const base = initial.volume.bounds, bounds = zoomed.volume.bounds, at = axis * 2;
            check(Math.abs((base[at + 1] - base[at]) / (bounds[at + 1] - bounds[at]) -
                           zoomed.camera.scale / initial.camera.scale) < 1e-10 &&
                  Math.abs((base[at] + base[at + 1]) - (bounds[at] + bounds[at + 1])) < 1e-10,
                  'Volume zoom reduces all three spans by the same factor around their current centres');
        }
        check([[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]].every((p, i) =>
            view.project(p).every((v, j) => Math.abs(v - axes[i][j]) < 1e-10)),
            'Axes stay fitted instead of growing with the mesh');
        check(document.getElementById('plotScale').value === 'manual' &&
              zoomed.volume.exact.every((v, i) => document.getElementById(controls[i]).marsZoomValue.includes(v)) &&
              controls.slice(0, 6).every(id => document.getElementById(id).marsZoomValue.includes('2^(-(')),
              'The native sampler receives exact baseline recipes rather than rounded browser coordinates');
        const formattedBounds = [-3.1998336521996507, 3.1998336521996507,
                                 1000000.00001234, 1000000.00005678, -1.0666112173998836, 1.0666112173998836];
        labDOM.call('lab_plot_volume_bounds', formattedBounds);
        check(document.getElementById('plotUpper').value === '3.2' &&
              document.getElementById('plotYUpper').value === '1.067' &&
              Number(document.getElementById('plotSecondLower').value) !==
                  Number(document.getElementById('plotSecondUpper').value) &&
              formattedBounds.every((v, i) => Number(document.getElementById(controls[i]).marsZoomValue) === v),
              'C formats bounds by interval width without losing narrow spans or sampling coordinates');
        labSurfaceVolume.set(view, 1);
        const clipped = labSurfaceVolume.clip([[-1, -1, 0.5], [2, 0.5, 0.5], [0.5, 2, 0.5]], p => p);
        check(clipped.length >= 3 && clipped.every(p => p.every(v => v >= -1e-12 && v <= 1 + 1e-12)),
              'Canvas and SVG polygons are clipped at the coordinate-volume faces');
        await waitForVolume();
        const refined = labSurfacePreview.snapshot();
        check(chart() !== before && Math.abs(refined.camera.scale - zoomed.camera.scale) < 1e-12 &&
              refined.volume.bounds.every((v, i) => Math.abs(v - refined.volume.sampled[i]) < 1e-8),
              'Refinement replaces geometry without resetting the chosen volume or zoom');
        wheel(100);
        await settle();
        check(Math.abs(labSurfacePreview.snapshot().camera.scale - initial.camera.scale) < 1e-12,
              'Reversing wheel direction reverses coordinate-volume zoom after refinement');
        card.querySelector('[data-zoom-reset]').click();
        await waitForVolume();
        check(labSurfacePreview.snapshot().camera.scale === 1 &&
              controls.every((id, i) => document.getElementById(id).value === original[i]),
              'Reset restores the original ranges and automatic-height policy');

        // An obsolete native refinement must never move the viewport back.
        const fetchBefore = fetchEvaluation;
        let release;
        try {
            fetchEvaluation = (...args) => args[2] === 'plot' ?
                new Promise(resolve => release = resolve) : fetchBefore(...args);
            card.querySelector('[data-zoom-step="1"]').click();
            const deadline = performance.now() + 5000;
            while (!release && performance.now() < deadline) await new Promise(resolve => setTimeout(resolve, 25));
            check(!!release, 'Hold one native volume refinement in flight');
            card.querySelector('[data-zoom-step="-1"]').click();
            await settle();
            const newest = labSurfacePreview.snapshot().volume.bounds;
            release({response: {ok: true}, data: {ok: false, error: 'Obsolete volume fixture'}});
            fetchEvaluation = fetchBefore;
            await waitForVolume();
            check(labSurfacePreview.snapshot().volume.bounds.every((v, i) => Math.abs(v - newest[i]) < 1e-10) &&
                  !rendered.textContent.includes('Obsolete volume fixture'),
                  'Late cancelled refinement cannot replace newer zoom bounds');
        } finally { fetchEvaluation = fetchBefore; }
        card.querySelector('[data-zoom-reset]').click();
        await waitForVolume();
        const deepView = labSurfacePreview.prepare(chart());
        check(labSurfaceVolume.set(deepView, 80, false, 1), 'C accepts zoom beyond double coordinate resolution');
        labSurfacePreview.draw(chart(), deepView.angles);
        await waitForVolume();
        const deep = labSurfacePreview.snapshot();
        check(deep.volume.exponent === 80 && deep.volume.sampleExponent === 80 &&
              chart().dataset.exactZMin !== chart().dataset.exactZMax &&
              chart().dataset.localRenderer === 'webgl',
              'Microscopic native volume retains distinct exact endpoints and a normalised GPU frame');
        card.querySelector('[data-zoom-step="1"]').click();
        await waitForVolume();
        check(labSurfacePreview.snapshot().volume.exponent > 80,
              'Zoom continues after microscopic native refinement without rebasing through doubles');
        card.querySelector('[data-zoom-reset]').click();
        await waitForVolume();
        await checkPlotPinch();
    }
    async function checkFunctionRun() {
        await runFunctionCard();
        check(functionRunOutput.textContent.trim() === '5', 'Native Function RUN did not evaluate the full card');
        await window.checkLabMobileLayout('function run output');
        const originalFetch = window.fetch;
        const originalText = functionStyle.textContent;
        const fullSource = functionStyle.dataset.fullText.trim();
        try {
            functionStyle.textContent = 'abbreviated...';
            window.fetch = async (url, options) => {
                const body = labWire.decode(options.body.buffer);
                check(String(url).includes('/function-run'), 'RUN used the wrong endpoint');
                check(
                    body.source === fullSource && body.precision === requestedValuePrecision(),
                    'RUN sent abbreviated text or the wrong precision');
                return new Response(
                    labWire.encode({ok: false, error: '<img src=x onerror=alert(1)>'}),
                    {status: 422, headers: {'Content-Type': 'application/x-protobuf'}});
            };
            await runFunctionCard();
            check(functionRunOutput.textContent.includes('<img'), 'RUN diagnostic missing');
            check(!functionRunOutput.querySelector('img'), 'RUN diagnostic was interpreted as HTML');
            let release;
            window.fetch = () => new Promise(resolve => {
                release = resolve;
            });
            const pending = runFunctionCard();
            clearFunctionRun();
            release(new Response(
                labWire.encode({ok: true, output: 'stale'}), {headers: {'Content-Type': 'application/x-protobuf'}}));
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
        check(!document.getElementById('lab-config'), 'JSON configuration is still embedded in the page');
        const bootstrap = await labFetch('/bootstrap');
        check(bootstrap.headers.get('Content-Type') === 'application/x-protobuf', 'Bootstrap did not use Protobuf');
        const config = await bootstrap.labData();
        check(
            config.BROWSER_ABI_VERSION === String(labWire.exports().lab_browser_abi_version()),
            'Running native server and WASM ABI differ');
        check(config.DEFAULT_EXPRESSION === labConfig.DEFAULT_EXPRESSION, 'Bootstrap defaults differ from the UI');
        const empty = labWire.encode({});
        check(Array.from(empty).join(',') === '8,1,18,2,8,5', 'Protobuf golden envelope mismatch');
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
            check(loadLabLocalState(storageKey) === null, 'missing fallback was not empty');
            saveLabLocalState(storageKey, source);
            check(
                JSON.stringify(loadLabLocalState(storageKey)) === JSON.stringify(source),
                'stored Protobuf changed Unicode, exact text or nested values');
            const stored = localStorage.getItem(`${storageKey}.protobuf`);
            check(
                JSON.stringify(labWire.decode(Uint8Array.from(atob(stored), c => c.charCodeAt(0)).buffer)) ===
                    JSON.stringify(source),
                'fallback storage is not the same Protobuf envelope as the API');
            saveLabLocalState(storageKey, {text: 'μ'.repeat(10000)});
            check(loadLabLocalState(storageKey).text === 'μ'.repeat(10000), 'chunked storage lost bytes');
            for (const invalid of ['not base64!', btoa('{}'), stored.slice(0, -4)]) {
                localStorage.setItem(`${storageKey}.protobuf`, invalid);
                check(loadLabLocalState(storageKey) === null, 'damaged, JSON or oversized fallback was accepted');
            }
            const getStoredItem = Storage.prototype.getItem;
            const decode_base64 = window.atob;
            const storage_limit = labWire.exports().lab_browser_base64_limit();
            let stored_length = storage_limit, decode_calls = 0;
            try {
                Storage.prototype.getItem = function(key) {
                    return key === `${storageKey}.protobuf` ? 'A'.repeat(stored_length) : getStoredItem.call(this, key);
                };
                window.atob = () => {
                    ++decode_calls;
                    throw new Error('Boundary probe reached base64 decoding');
                };
                check(loadLabLocalState(storageKey) === null && decode_calls === 1,
                    'exact native storage bound reaches decoding and retains error recovery');
                ++stored_length;
                check(loadLabLocalState(storageKey) === null && decode_calls === 1,
                    'oversized fallback was not rejected before decoding');
            } finally {
                Storage.prototype.getItem = getStoredItem;
                window.atob = decode_base64;
            }
            saveLabLocalState(storageKey, {town: 'Shrewsbury'});
            check(loadLabLocalState(storageKey).town === 'Shrewsbury', 'fallback did not recover after corruption');
        } finally {
            if (storedBefore === null)
                localStorage.removeItem(`${storageKey}.protobuf`);
            else
                localStorage.setItem(`${storageKey}.protobuf`, storedBefore);
        }
        const rejects = action => {
            try {
                action();
                return false;
            } catch (_) {
                return true;
            }
        };
        const padded = new Uint8Array(empty.length + 4);
        padded.set(empty, 2);
        check(
            Object.keys(labWire.decode(new DataView(padded.buffer, 2, empty.length))).length === 0,
            'DataView offsets were ignored');
        check(
            Object.keys(labWire.decode(padded.subarray(2, padded.length - 2))).length === 0,
            'Typed view offsets were ignored');
        check(rejects(() => labWire.decode({byteLength: empty.length})), 'Fake byte buffer accepted');
        if (typeof SharedArrayBuffer === 'function')
            check(rejects(() => labWire.decode(new Uint8Array(new SharedArrayBuffer(6)))), 'Shared buffer accepted');
        const realm = document.createElement('iframe');
        document.body.appendChild(realm);
        try {
            const foreign = new realm.contentWindow.Uint8Array(empty);
            check(Object.keys(labWire.decode(foreign)).length === 0, 'Cross-realm typed view rejected');
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
        const recursive = new Proxy({}, {
            ownKeys() {
                labWire.encode({});
                return [];
            }
        });
        check(rejects(() => labWire.encode(recursive)), 'Recursive codec invocation accepted');
        check(labWire.encode({}).length === empty.length, 'Codec did not recover after rejected recursion');
        const numbers = labWire.decode(labWire.encode({zero: -0, tiny: Number.MIN_VALUE}));
        check(Object.is(numbers.zero, -0) && numbers.tiny === Number.MIN_VALUE, 'Binary64 edge values changed');
        const maximum = labWire.encode({x: 'a'.repeat(8388579)});
        check(maximum.length === 8388608, 'Exact wire limit was rejected');
        check(labWire.decode(maximum).x.length === 8388579, 'Maximum message did not round trip');
        check(rejects(() => labWire.encode({x: 'a'.repeat(8388580)})), 'Wire limit plus one accepted');
        for (let i = 0; i < empty.length; ++i)
            check(rejects(() => labWire.decode(empty.slice(0, i).buffer)), 'Truncated envelope accepted');
        check(rejects(() => labWire.decode(new Uint8Array([8, 2, 18, 2, 8, 5]).buffer)), 'Unknown version accepted');
        check(rejects(() => labWire.decode(new Uint8Array([...empty, 8, 1]).buffer)), 'Duplicate version accepted');
        const unknown = labWire.decode(new Uint8Array([...empty, 120, 1]).buffer);
        check(Object.keys(unknown).length === 0, 'Unknown supported field was not skipped');
        for (const node
                 of [[8, 5, 8, 5], [8, 5, 16, 1], [8, 5, 50, 11, 10, 1, 120, 18, 6, 8, 3, 34, 2, 192, 128],
                     [8, 5, 50, 9, 10, 1, 120, 18, 4, 8, 1, 16, 2], [8, 5, 50, 4, 18, 2, 8, 0, 50, 4, 18, 2, 8, 0]]) {
            check(
                rejects(() => labWire.decode(new Uint8Array([8, 1, 18, node.length, ...node]).buffer)),
                'Malformed schema payload accepted');
        }
        check(rejects(() => labWire.encode({number: Infinity})), 'Infinite number accepted');
        const member_limit = labWire.exports().lab_browser_member_limit();
        for (const count of [member_limit, member_limit + 1]) {
            let message = '';
            try {
                labWire.encode({list: new Array(count).fill(null)});
            } catch (error) {
                message = error.message;
            }
            check(message.includes(count === member_limit ? 'Invalid or oversized' : 'too many members'),
                'host member admission preserves the exact native boundary');
        }
        check(labWire.decode(labWire.encode({valid: true})).valid, 'codec recovers after member rejection');
        check(rejects(() => labWire.encode({text: '\ud800'})), 'Invalid surrogate accepted');
        check(rejects(() => labWire.encode({text: 'x'.repeat(8388608)})), 'Oversized message accepted');
        let deep = {};
        for (let i = 0; i < 34; ++i) deep = {deep};
        check(rejects(() => labWire.encode(deep)), 'Excessive nesting accepted');
        const malicious = Object.create(null);
        malicious.__proto__ = {polluted: true};
        const safe = labWire.decode(labWire.encode(malicious).buffer);
        check(Object.hasOwn(safe, '__proto__') && !({}).polluted, 'Protobuf key changed an object prototype');
        check(
            labWire.precision(1, 53) === 17 && labWire.precision(2000000, 53) === 1048576 &&
                labWire.precision(NaN, 53) === 53,
            'C precision validation failed');
        check(
            labWire.intervals(20000, 5000) === 20000 && labWire.intervals(42, 5000) === 5000,
            'C integration budget validation failed');
        const response = await labFetch('/state', {method: 'POST', body: labWire.encode({expression: '2+3'})});
        check(response.headers.get('Content-Type') === 'application/x-protobuf', 'Server did not speak Protobuf');
        const result = await response.labData();
        check(result.expression === '2+3', 'Native Protobuf round trip changed data');
        const invalid = await labFetch('/state', {method: 'POST', body: new Uint8Array([8, 2])});
        check(invalid.headers.get('Content-Type') === 'application/x-protobuf', 'Error response did not use Protobuf');
        check(invalid.status === 400 && !(await invalid.labData()).ok, 'Malformed native request was not rejected');
        const originalFetch = window.fetch;
        try {
            for (const type of ['application/json', 'application/x-protobuf']) {
                window.fetch = async () => new Response(new Uint8Array(8388609), {headers: {'Content-Type': type}});
                const oversized = await labFetch('/fixture');
                let rejected = false;
                try {
                    await oversized.labData();
                } catch (error) {
                    rejected = error.message.includes(type === 'application/json' ? 'Expected a Protobuf' : '8 MiB');
                }
                check(rejected, `Oversized ${type} response was not bounded`);
            }
            let cancelled = false;
            const message_limit = labWire.exports().lab_browser_message_limit();
            window.fetch = async () => new Response(new ReadableStream({
                start(controller) {
                    controller.enqueue(new Uint8Array(message_limit));
                    controller.enqueue(new Uint8Array(1));
                },
                cancel() {
                    cancelled = true;
                }
            }), {headers: {'Content-Type': 'application/x-protobuf'}});
            let rejected = false;
            try {
                await (await labFetch('/fixture')).labData();
            } catch (error) {
                rejected = error.message.includes('8 MiB');
            }
            check(rejected && cancelled, 'stream crossing the native bound is rejected and cancelled');
        } finally {
            window.fetch = originalFetch;
        }
    }
    async function report(message) {
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
            const workspace = document.getElementById('labWorkspace').cloneNode(true);
            workspace.classList.add('help-open');
            workspace.querySelector('#helpPane').classList.remove('hidden');
            workspace.querySelector('#helpClose').classList.remove('hidden');
            workspace.querySelector('#resultPane').classList.add('hidden');
            for (const card of workspace.querySelectorAll('.help-card'))
                card.classList.remove('hidden');
            frame.contentDocument.body.appendChild(workspace);
            for (const width of [1920, 900, 390]) {
                frame.style.width = `${width}px`;
                await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
                const panel = workspace.querySelector('#resultWorkspacePanel').getBoundingClientRect();
                const bar = workspace.querySelector('.lab-topbar').getBoundingClientRect();
                check(Math.abs(panel.left - bar.left) < 2 && Math.abs(panel.right - bar.right) < 2,
                      `Help spans the full workspace at ${width}px`);
                for (const card of workspace.querySelectorAll('.help-card'))
                    check(card.scrollWidth <= card.clientWidth + 1, `Help text overflows at ${width}px`);
                check(frame.contentDocument.documentElement.scrollWidth <= width,
                      `Help must not introduce page-wide horizontal scrolling at ${width}px`);
            }
        } finally {
            frame.remove();
        }
    }

    async function checkAlmanacLayout() {
        // Timestamp parsing belongs to test_lab_almanac_presentation.c; this fixture checks DOM consumption only.
        const visibility = almanacVisibilityMode;
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
            const target = frame.contentDocument.getElementById('fixture');
            const [fixture, pending, sections] = window.labAlmanacFixtures();
            for (const incomplete
                     of [{visibility: 'all'},
                         {...fixture, almanac_presentation: {all: {...fixture.almanac_presentation.all, html: ''}}},
                         {...fixture, almanac_presentation: {all: {...fixture.almanac_presentation.all, html: 42}}}]) {
                target.textContent = 'Previous worksheet';
                target.dataset.copyText = 'Previous copy';
                let rejected = false;
                try {
                    renderAlmanacWorksheet(target, incomplete);
                } catch (error) {
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
            renderAlmanacWorksheet(target, fixture);
            check(
                target.querySelectorAll('.almanac-grid-table:not(.almanac-event-table) tbody tr').length === 4,
                'Native all-body rows were not used');
            check(
                target.querySelector('.almanac-body-symbol').textContent === '☉' &&
                    target.querySelector('.almanac-moon-phase')?.getAttribute('aria-label') ===
                        'Moon: 50.0% illuminated; local zenith up',
                'Native astronomical symbols or the accessible lunar phase were lost');
            check(
                target.querySelector('[data-body="SUN"]').namespaceURI === 'http://www.w3.org/2000/svg' &&
                    target.querySelector('[data-body="SUN"] circle') &&
                    !target.querySelector('.almanac-body-symbol image, .almanac-body-symbol text'),
                'Body symbols must be vector geometry, not bitmap images or font glyphs');
            const brightStar = target.querySelector('[data-body="SIRIUS"]');
            const faintStar = target.querySelector('[data-body="POLARIS"]');
            check(
                brightStar?.classList.contains('almanac-star-symbol') &&
                    faintStar?.classList.contains('almanac-star-symbol') &&
                    brightStar.querySelector('path').getBoundingClientRect().width >
                        faintStar.querySelector('path').getBoundingClientRect().width &&
                    brightStar.getBoundingClientRect().width === faintStar.getBoundingClientRect().width &&
                    brightStar.getAttribute('aria-label').includes('visual magnitude -1.46'),
                'Star geometry must scale with brightness without changing the aligned icon slots');
            for (const star of [brightStar, faintStar]) {
                check(frame.contentWindow.getComputedStyle(star.querySelector('path')).fill ===
                          'rgb(243, 228, 179)',
                      'Stars must retain their warm pale-gold colour at every brightness-based size');
            }
            const moonIcon = target.querySelector('.almanac-moon-phase');
            check(
                frame.contentWindow.getComputedStyle(target.querySelector('[data-body="SUN"]')).color ===
                    'rgb(239, 195, 106)' &&
                    frame.contentWindow.getComputedStyle(moonIcon).color === 'rgb(183, 195, 204)' &&
                    frame.contentWindow.getComputedStyle(moonIcon.querySelector('.almanac-moon-shadow')).fill ===
                        'rgb(65, 73, 79)' &&
                    frame.contentWindow.getComputedStyle(moonIcon.querySelector('.almanac-moon-rim')).stroke ===
                        'rgb(126, 139, 148)',
                'The Sun and phase-aware Moon lost their individual reference colours');
            const colourProbe = frame.contentDocument.createElement('span');
            colourProbe.className = 'almanac-body-symbol';
            target.appendChild(colourProbe);
            for (const [body, colour] of [
                     ['MERCURY', 'rgb(184, 68, 197)'], ['VENUS', 'rgb(196, 182, 77)'],
                     ['MARS', 'rgb(217, 77, 76)'], ['JUPITER', 'rgb(232, 117, 120)'],
                     ['SATURN', 'rgb(217, 154, 57)'], ['URANUS', 'rgb(72, 190, 208)'],
                     ['NEPTUNE', 'rgb(128, 101, 233)']]) {
                colourProbe.dataset.body = body;
                check(frame.contentWindow.getComputedStyle(colourProbe).color === colour, `${body} symbol colour`);
            }
            colourProbe.remove();
            check(
                moonIcon.getBoundingClientRect().width > 10 &&
                    moonIcon.getBoundingClientRect().width < 40 &&
                    moonIcon.querySelector('path')?.getAttribute('transform') === 'rotate(180.000000 12 12)',
                'Lunar SVG is missing or is not sized as an inline symbol');
            check(
                target.querySelector('.visible-cell.yes').textContent.trim() === '✓',
                'Client recomputed visibility from the raw row instead of native metadata');
            check(
                target.dataset.copyText === fixture.almanac_presentation.all.copy_text &&
                    almanacWorksheetCopyText(fixture, 'visible') === fixture.almanac_presentation.visible.copy_text,
                'Clipboard text was rebuilt instead of retaining the native variants');
            check(!target.querySelector('[data-almanac-land-totality]'), 'Client recomputed land eligibility');
            const action = target.querySelector('[data-almanac-use-totality]');
            check(
                action?.dataset.date === '2027-08-02' && action.dataset.town === 'Málaga|36.7204|-4.4203|0',
                'Native totality action payload was lost');
            const table = target.querySelector('.almanac-event-table');
            const eclipseIcon = table.querySelector('.almanac-eclipse-symbol');
            check(
                eclipseIcon?.getAttribute('aria-label').includes('greatest eclipse; local zenith up') &&
                    eclipseIcon.querySelector('g').getAttribute('transform') ===
                        'translate(12 12) rotate(-45.000000000)' &&
                    eclipseIcon.querySelector('path') && !eclipseIcon.querySelector('image'),
                'The observer-oriented maximum-eclipse SVG was not installed beside the event name');
            check(frame.contentWindow.getComputedStyle(eclipseIcon.querySelector('circle')).fill ===
                      'rgb(239, 195, 106)',
                  'The solar-eclipse disc must use the same gold as the Sun symbol');
            const region = table.parentElement;
            check(
                region.tabIndex === 0 && region.getAttribute('role') === 'region',
                'Table scroll area is not keyboard accessible');
            check(
                table.querySelector('.event-time[title]').textContent === '09:06:00',
                'Contact still includes the full date');
            check(table.querySelector('.event-time[title]').title.endsWith('GMT+1'), 'Full contact time was lost');
            for (const [viewport, card] of [[1920, 860], [1920, 1700], [320, 290], [390, 350], [430, 390]]) {
                frame.style.width = `${viewport}px`;
                target.style.width = `${card}px`;
                await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
                for (const icon of target.querySelectorAll('.almanac-body-symbol, .almanac-eclipse-symbol')) {
                    const style = frame.contentWindow.getComputedStyle(icon);
                    check(style.filter === 'none' && style.transform === 'none',
                          'Formula recolouring or zoom leaked into an astronomical SVG');
                    check(parseFloat(style.marginBottom) === 0,
                          `Astronomical symbol has formula spacing: ${style.marginBottom}`);
                    const cell = icon.closest('td'), text = frame.contentDocument.createRange();
                    text.selectNodeContents(cell.querySelector('.almanac-event-label') || cell.lastChild);
                    const labelBox = text.getBoundingClientRect(), iconBox = icon.getBoundingClientRect();
                    check(Math.abs(iconBox.y + iconBox.height / 2 - labelBox.y - labelBox.height / 2) < 5,
                          'An astronomical icon is above its name instead of beside it');
                    if (icon.classList.contains('almanac-body-symbol')) {
                        const height = viewport > 560 ? icon.closest('tr').getBoundingClientRect().height :
                            cell.getBoundingClientRect().height;
                        check(height < 65, 'Body symbols inflated the name row height');
                    }
                }
                const bodies = target.querySelector('.almanac-body-table');
                if (viewport <= 560) {
                    check(bodies.scrollWidth <= bodies.clientWidth + 1,
                          'Phone body entries must not require horizontal scrolling');
                    for (const row of bodies.tBodies[0].rows) {
                        check(frame.contentWindow.getComputedStyle(row).display === 'grid',
                              'Phone bodies must use labelled entries');
                        const metrics = row.querySelectorAll('td.number');
                        check(metrics.length === 7, 'A body measurement was lost on mobile');
                        for (const cell of metrics) {
                            const label = frame.contentWindow.getComputedStyle(cell, '::before');
                            check(cell.dataset.label && label.content.includes(cell.dataset.label),
                                  'Mobile body measurement has no visible label');
                            check(cell.scrollWidth <= cell.clientWidth + 1, 'Body measurement is clipped');
                        }
                    }
                    check(bodies.querySelector('[data-field="declination"]').textContent === 'S 008° 00.12' &&
                          bodies.querySelector('[data-field="magnitude"]').textContent === '-26.7',
                          'Responsive body layout changed native values');
                } else {
                    check(frame.contentWindow.getComputedStyle(bodies.tBodies[0].rows[0]).display === 'table-row',
                          'Desktop body table was replaced by mobile entries');
                }
                check(region.clientWidth <= card && target.scrollWidth <= card + 1, 'Almanac spills outside its card');
                for (const cell of table.querySelectorAll('tbody td')) {
                    check(
                        cell.scrollWidth <= cell.clientWidth + 1,
                        `Event content overlaps its cell: ${cell.dataset.label}`);
                }
                const headerStyle = frame.contentWindow.getComputedStyle(table.tHead);
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
            renderAlmanacWorksheet(target, {...fixture, visibility: 'visible'});
            check(
                target.querySelectorAll('.almanac-grid-table:not(.almanac-event-table) tbody tr').length === 1 &&
                    target.querySelector('.body-name').textContent === '☉Sun' && !target.querySelector('.visible-cell'),
                'Local visible variant was filtered again or retained the visibility column');
            check(
                target.dataset.copyText === fixture.almanac_presentation.visible.copy_text &&
                    target.textContent.includes('visible bodies only'),
                'Visible variant text was not used verbatim');
            renderAlmanacWorksheet(target, pending);
            const emptyBody = target.querySelector('.almanac-body-table td[colspan]');
            check(emptyBody?.textContent.includes('No bodies found') &&
                  emptyBody.scrollWidth <= emptyBody.clientWidth + 1,
                  'Empty body filter must retain a readable full-width message');
            check(
                target.querySelector('[data-almanac-land-totality]')?.dataset.almanacLandTotality === '2461619.9' &&
                    target.querySelector('.event-time[title]').textContent === 'Unavailable',
                'Native land-search marker or unavailable compact-time text was reinterpreted');
            renderDatetimeSections(target, null, sections, 'native copy text');
            const details = target.querySelectorAll('details');
            check(details.length === 2 && !details[0].open && details[1].open, 'Native section expansion changed');
            check(!target.querySelector('script,img'), 'Native section text became executable markup');
            check(
                details[0].querySelector('summary').textContent === '<script> & calendar' &&
                    details[0].querySelector('.datetime-row-value').textContent === 'quoted & "text"\rμ' &&
                    details[0].querySelectorAll('.datetime-row').length === 2 &&
                    details[0].textContent.includes('unavailable'),
                'Native section escaping, blank rows or empty-value policy changed');
            check(target.dataset.fullText === 'native copy text', 'Native section clipboard companion was lost');
            let missingDateRejected = false;
            try {
                renderDatetimeSections(
                    target, null, [{title: 'Old server', rows: [{label: 'Date', value: 'today'}]}], 'Date: today');
            } catch (error) {
                missingDateRejected = error.message.includes('DateTime presentation');
            }
            check(
                missingDateRejected && target.querySelectorAll('details').length === 2 &&
                    target.dataset.fullText === 'native copy text',
                'Incomplete DateTime response replaced structured sections with plain text');
            renderDatetimeSections(target, null, [], '<fallback>');
            check(
                target.textContent === '<fallback>' && !target.children.length,
                'Empty sections lost the text fallback');
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
                check(
                    document.getElementById('datetimeJurisdiction').disabled,
                    'Unavailable database leaves menu enabled');
                check(document.querySelector('[data-jurisdiction-notice]'), 'Unavailable database notice missing');
                check(
                    String(value.textContent).trim() === '7',
                    `Unavailable database blocked ordinary maths: mode=${currentMode()}, input=${expr.value}, ` +
                        `value=${value.textContent}, rendered=${rendered.textContent}`);
                await report(
                    'PASS: startup, native evaluation, editor, modes, cards, dates, state, mobile, failure recovery');
                return;
            }
            if (failureCase) {
                await window.labReady.catch(() => {});
                check(
                    document.getElementById('status').textContent === 'Startup failed',
                    'Missing startup asset not reported');
                check(document.getElementById('run').disabled, 'Failed startup leaves Evaluate enabled');
                if (failureCase === 'catalogue') {
                    check(
                        document.getElementById('rendered').textContent.includes('503'),
                        'Startup error lacks HTTP status');
                    location.href = '/?failure=script';
                    return;
                }
                if (failureCase === 'server-missing' || failureCase === 'server-mismatch') {
                    check(
                        document.getElementById('rendered').textContent.includes('make mars-lab-restart'),
                        'Server mismatch does not explain how to restart');
                    check(
                        !document.querySelector('script[src="/js/workspace.js"]'),
                        'Incompatible server reached worksheet initialisation');
                    location.href =
                        failureCase === 'server-missing' ? '/?failure=server-mismatch' : '/?failure=database';
                    return;
                }
                check(
                    document.getElementById('rendered').textContent.includes('/js/'), 'Missing script not identified');
                location.href = '/?failure=server-missing';
                return;
            }
            await window.labReady;
            initialise_lab_test_adapters();
            if (labProfile.enabled) {
                await report('PASS: PROFILE ' + JSON.stringify(await labProfile.run()));
                return;
            }
            await checkWire();
            await labCheckWorkers(check);
            check(
                (await nativeFetch('/js/worksheet.js')).status === 404 && !labDefinitionScripts.includes('worksheet') &&
                    !document.querySelector('script[src="/js/worksheet.js"]'),
                'Retired worksheet script remains served or loaded');
            check(
                (await nativeFetch('/js/evaluation.js')).status === 404 &&
                    !labDefinitionScripts.includes('evaluation') &&
                    !document.querySelector('script[src="/js/evaluation.js"]'),
                'Retired evaluation script remains served or loaded');
            await window.labWorkspaceChecks();
            await window.checkLabRequests();
            await window.checkLabEvaluation();
            await window.checkLabEvaluationCards();
            await window.checkLabResultFlows();
            await window.checkLabBindingFlows();
            await window.checkLabLocationFlow();
            await window.checkLabRequestFlows();
            await window.checkLabStateFlow();
            await window.checkLabTransportFlows();
            window.checkLabEditor();
            await checkRenderedGlyphs();
            await window.checkLabForms();
            await window.checkLabCalendarState();
            await window.checkLabPersistence();
            await window.checkLabStorage();
            window.checkLabWidgets();
            await window.checkLabSelects();
            await window.checkLabSelectEvents();
            await window.checkLabLayout();
            await window.checkLabSyntax();
            await window.checkLabProjection();
            await checkRenderedGlyphs();
            await window.checkLabWorkspaceDom();
            await checkRenderedGlyphs();
            await window.checkLabLocationDom();
            await window.checkLabResultDom();
            await window.checkLabBindingDom();
            await window.checkLabBindingSync();
            await window.checkLabBindingCommit();
            await window.checkLabFunction();
            await window.checkLabEvaluationInstall();
            await window.checkLabGoal();
            await window.checkLabEvaluationSetup();
            await window.checkLabWeather();
            await window.checkLabPayload();
            await window.checkLabPickerDom();
            await window.checkLabEvents();
            await window.checkLabWidgetEvents();
            await window.checkLabAlmanacEvents();
            check(!failures.length, failures.join('; '));
            check(currentMode() === 'expression', 'Initial expression mode not restored');
            await checkAlmanacLayout();
            await checkHelpLayout();
            await window.checkLabMobileLayout('expression result');
            check(document.styleSheets.length > 0, 'External stylesheet not loaded');
            const stylesheet = Array.from(document.styleSheets).find(sheet => sheet.href?.endsWith('/index.css'));
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
                check(rule.styleSheet.cssRules.length > 0, `Stylesheet ${styleNames[index]} failed to load`);
            });
            check(
                getComputedStyle(document.documentElement).getPropertyValue('--code').trim(),
                'Theme variables missing');
            check(getComputedStyle(document.getElementById('run')).borderRadius === '999px', 'Button styling missing');
            check(JURISDICTION_TOWN_OPTIONS['GB-WLS'].length > 0, 'Welsh catalogue unavailable');
            check(
                String(value.textContent).trim() === '5',
                `Initial calculation: ${value.textContent}; ${rendered.textContent}`);
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
            await takeDerivative('x');
            check(!rendered.classList.contains('error'), 'Shared derivative action failed');
            check(String(value.textContent).trim() === '2', 'Native derivative card lost its exact numerical value');
            check(parsed.dataset.fullText === '2', 'Derivative expression was not taken from the native card');
            await takeIntegral('x');
            check(!rendered.classList.contains('error'), 'Shared integral action failed');
            check(parsed.dataset.fullText.includes('x'), 'Integral card lost its native expression');
            check(functionStyle.dataset.fullText.length > 0, 'Integral Function card missing');
            check(rendered.dataset.fullTex.length > 0, 'Integral TeX metadata missing');
            await checkAsynchronousPlot();
            const plotRendered = document.getElementById('plotRendered');
            setExpressionEditor('asin(x)');
            await evaluateExpression();
            document.getElementById('plotLower').value = '';
            document.getElementById('plotUpper').value = '';
            await takePlot('x');
            check(!plotRendered.classList.contains('error') && !!plotRendered.querySelector('[data-series]') &&
                  document.getElementById('plotLower').value === '' &&
                  document.getElementById('plotUpper').value === '',
                  'Blank bounds must plot the native real domain without replacing Automatic controls with numbers');
            setExpressionEditor('sin(x)');
            await evaluateExpression();
            document.getElementById('plotLower').value = '-@pi';
            document.getElementById('plotUpper').value = '@pi';
            await takePlot('x');
            check(!plotRendered.classList.contains('error'), 'Native plot request failed');
            check(!!plotRendered.querySelector('svg [data-series="0"]'), 'Plot SVG was not displayed');
            check(document.querySelector('#plotCard .card-title > span').textContent === 'Plot', 'Plot card title missing');
            check(!!plotRendered.querySelector('svg [data-plot-title] use') && plotRendered.dataset.displayTex.includes('\\sin'),
                  'Plot formula title must be rendered as TeX glyphs in the same SVG');
            check(!plotRendered.textContent.includes('Function plot'), 'Generic plot title must be removed');
            check(parsed.dataset.fullText.includes('sin'), 'Plot lost its source expression');
            check(copyTextForTarget('plot').includes('data-series') && !rendered.querySelector('[data-series]'),
                  'Plot copy and mathematical rendering belong to independent cards');
            const plotCard = plotRendered.closest('.result-card');
            const plotExpand = plotCard.querySelector('[data-expand-card]');
            const nextLayout = () => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
            setResultZoom(plotCard, 3);
            await nextLayout();
            await checkPlotWheelZoom();
            await checkPlotChoices();
            checkPlotControls(false);
            check(document.getElementById('plotKindGroup').classList.contains('plot-options-card'),
                  'Expression plot type uses the compact settings card');
            await checkLivePlotControls();
            await window.checkLabMobileLayout('curve plot');
            const curveDragTarget = plotRendered.querySelector('[data-series] path');
            check(curveDragTarget.dispatchEvent(new PointerEvent('pointerdown',
                  {button: 0, buttons: 1, pointerId: 11, pointerType: 'touch', isPrimary: true,
                   bubbles: true, cancelable: true})) &&
                  !plotRendered.classList.contains('plot-dragging'), 'Curve plots must not enable surface rotation');
            check(getComputedStyle(plotRendered.querySelector('svg')).touchAction === 'none',
                  'Curve plots reserve finger movement for panning');
            const formulaBox = plotRendered.querySelector('[data-plot-title]').getBoundingClientRect();
            const axesBox = plotRendered.querySelector('[data-plot-chart] [data-plot-axes]').getBoundingClientRect();
            check(formulaBox.width > 0 && formulaBox.height > 0 && formulaBox.bottom < axesBox.top,
                  'Rendered formula title must sit above the plot axes without overlap');
            const figure = plotRendered.querySelector('[data-plot-figure]');
            const curve = figure.querySelector('[data-series] path');
            check(getComputedStyle(figure).filter === 'none' && getComputedStyle(curve).stroke === 'rgb(120, 216, 203)',
                  'Plot curves must retain their teal colour, without the monochrome formula filter');
            check(getComputedStyle(figure.querySelector('[data-plot-title] use')).fill === 'rgb(230, 199, 120)',
                  'Plot formula title must retain its gold colour');
            check(!figure.querySelector('rect') && !plotRendered.textContent.includes('Vertical range clipped'),
                  'Plot must have no surrounding box or clipping annotation');
            const chart = figure.querySelector('[data-plot-chart]');
            const edgePaths = [...chart.querySelectorAll('[data-plot-axes] path')].map(path => path.getAttribute('d'));
            const axisLeft = Number(chart.dataset.plotLeft), axisTop = Number(chart.dataset.plotTop);
            const axisRight = axisLeft + Number(chart.dataset.plotWidth);
            const axisBottom = axisTop + Number(chart.dataset.plotHeight);
            const edgePath = (x1, y1, x2, y2) =>
                `M ${x1.toFixed(3)} ${y1.toFixed(3)} L ${x2.toFixed(3)} ${y2.toFixed(3)}`;
            check(edgePaths.includes(edgePath(axisLeft, axisBottom, axisRight, axisBottom)) &&
                  edgePaths.includes(edgePath(axisLeft, axisTop, axisLeft, axisBottom)) &&
                  !edgePaths.includes(edgePath(axisLeft, axisTop, axisRight, axisTop)) &&
                  !edgePaths.includes(edgePath(axisRight, axisTop, axisRight, axisBottom)),
                  'Bottom and left axes must be solid, with top and right edges open');
            check([...figure.querySelectorAll('[data-plot-chart] text')].every(label =>
                      !['x', 'y', 'f'].includes(label.textContent)), 'Plot must not display axis titles');
            const smallPlotWidth = plotRendered.querySelector('svg').getBoundingClientRect().width;
            plotExpand.click();
            await nextLayout();
            const expandedPanel = document.getElementById('resultWorkspacePanel').getBoundingClientRect();
            const workspaceBar = document.querySelector('.lab-topbar').getBoundingClientRect();
            check(getComputedStyle(document.getElementById('workspacePanel')).display === 'none',
                  'Expanded plot must hide the editor');
            check(Math.abs(expandedPanel.left - workspaceBar.left) < 2 &&
                      Math.abs(expandedPanel.right - workspaceBar.right) < 2,
                  'Expanded plot must span both workspace columns');
            const largePlotWidth = plotRendered.querySelector('svg').getBoundingClientRect().width;
            const plotStyle = getComputedStyle(plotRendered);
            const availablePlotWidth = plotRendered.clientWidth - parseFloat(plotStyle.paddingLeft) -
                                       parseFloat(plotStyle.paddingRight);
            check(largePlotWidth > smallPlotWidth && Math.abs(largePlotWidth - availablePlotWidth) < 2,
                  'Expanded plot SVG must grow to fit the available width at 100%');
            await checkPlotChoices();
            plotExpand.click();
            await nextLayout();
            const collapsedWidth = plotRendered.querySelector('svg').getBoundingClientRect().width;
            const collapsedAvailable = plotRendered.clientWidth - parseFloat(plotStyle.paddingLeft) -
                                       parseFloat(plotStyle.paddingRight);
            check(getComputedStyle(document.getElementById('workspacePanel')).display !== 'none' &&
                      collapsedWidth < largePlotWidth && Math.abs(collapsedWidth - collapsedAvailable) < 2 &&
                      plotRendered.scrollWidth <= plotRendered.clientWidth + 1,
                  'Collapsing a plot restores the editor and fits the current column without horizontal overflow');
            const plotHeading = rightPaneTitle.textContent;
            saveCurrentModeResultState();
            plotExpand.click();
            for (const mode of ['equation', 'diffequation', 'matrix', 'integrator', 'datetime', 'almanac']) {
                setMode(mode);
                syncModeUI();
                restoreModeResultState();
                check(!rightPaneTitle.textContent.toLowerCase().includes('plot'), `${mode} inherited the plot heading`);
                check(!labWorkspace.classList.contains('result-card-expanded'), `${mode} inherited plot expansion`);
            }
            setMode('expression');
            syncModeUI();
            restoreModeResultState();
            await nextLayout();
            check(rightPaneTitle.textContent === plotHeading && renderedTitle.textContent === 'Rendered TeX' &&
                      !!plotRendered.querySelector('svg [data-series="0"]'),
                  'Returning to Expression restores its plot and matching headings');
            document.getElementById('plotScale').value = 'manual';
            document.getElementById('plotYLower').value = '-1/2';
            document.getElementById('plotYUpper').value = '1/2';
            await takePlot('x');
            check(!plotRendered.classList.contains('error'), 'Manual plot limits failed');
            check(!plotRendered.textContent.includes('Vertical range clipped'), 'Manual clipping must be silent');
            document.getElementById('plotScale').value = 'all';
            await takePlot('x');
            check(!plotRendered.classList.contains('error'), 'Fit all failed');
            check(!plotRendered.textContent.includes('Vertical range clipped'), 'Fit all retained manual limits');
            const plotPrecision = requestedValuePrecision();
            try {
                setRequestedPrecisionBits(384);
                setExpressionEditor('tan(x)');
                await evaluateExpression();
                document.getElementById('plotLower').value = '-2pi';
                document.getElementById('plotUpper').value = '2pi';
                document.getElementById('plotScale').value = 'manual';
                document.getElementById('plotYLower').value = '-12';
                document.getElementById('plotYUpper').value = '12';
                await takePlot('x');
                check(!plotRendered.classList.contains('error'), 'High-precision tangent plot failed');
                const paths = plotRendered.querySelectorAll('svg [data-series="0"] path');
                check(paths.length > 0, 'Tangent plot contains no visible curve');
                const frame = plotRendered.querySelector('[data-plot-chart]');
                const left = Number(frame.dataset.plotLeft), width = Number(frame.dataset.plotWidth);
                for (const path of paths) {
                    const coordinates = path.getAttribute('d').match(/-?\d+(?:\.\d+)?/g).map(Number);
                    for (const fraction of [0.125, 0.375, 0.625, 0.875]) {
                        const pole = left + width * fraction;
                        check(!(coordinates[0] < pole && coordinates[2] > pole), 'Curve connected across a tangent pole');
                    }
                }
            } finally {
                setRequestedPrecisionBits(plotPrecision);
            }
            document.getElementById('plotScale').value = 'auto';
            document.getElementById('plotYLower').value = '';
            document.getElementById('plotYUpper').value = '';
            document.getElementById('plotLower').value = '-5';
            document.getElementById('plotUpper').value = '5';
            setExpressionEditor('1/1^x + 1/2^x + 1/3^x + ...');
            await takePlot('x');
            check(!plotRendered.classList.contains('error') && !!plotRendered.querySelector('svg [data-series]'),
                  'Reciprocal series must plot its convergent portion across -5 to 5');
            check(plotRendered.dataset.displayTex.includes('\\sum'), 'Reciprocal series must retain its sigma title');
            document.getElementById('plotUpper').value = '1';
            const convergentView = plotRendered.querySelector('[data-series]');
            await takePlot('x');
            const diagnostic = document.getElementById('plotFeedback');
            check(!plotRendered.classList.contains('error') &&
                  plotRendered.querySelector('[data-series]') === convergentView &&
                  !diagnostic.classList.contains('hidden') && diagnostic.textContent.includes('No finite real points') &&
                  !diagnostic.textContent.includes('NAN') && !diagnostic.textContent.includes('binding variable'),
                  'Wholly divergent range retains the previous view with a separate clean diagnostic');
            await window.checkLabMobileLayout('plot error');
            document.getElementById('plotLower').value = '-10';
            document.getElementById('plotUpper').value = '10';
            setExpressionEditor('x^2 + y^2');
            await evaluateExpression();
            document.getElementById('plotKind').value = 'auto';
            document.getElementById('plotSecond').value = '';
            document.getElementById('plotLower').value = '-2';
            document.getElementById('plotUpper').value = '2';
            document.getElementById('plotSecondLower').value = '-2';
            document.getElementById('plotSecondUpper').value = '2';
            await takePlot('x');
            check(!plotRendered.classList.contains('error') && !!plotRendered.querySelector('[data-surface]'),
                  'Automatic two-variable plot must display a surface');
            const curveOption = document.querySelector('#plotKindChoices input[value="curve"]');
            check(!curveOption.closest('label').getClientRects().length,
                  'A curve is not offered while the second variable is unset');
            const fixedY = variableValues.querySelector('[data-binding-name="y"]');
            const originalY = fixedY.value;
            fixedY.value = '0';
            labDOM.call('lab_plot_controls_sync');
            check(curveOption.closest('label').getClientRects().length > 0,
                  'Fixing the other variable at zero permits a curve');
            document.getElementById('plotKind').value = 'curve';
            fixedY.value = originalY;
            labDOM.call('lab_plot_controls_sync');
            check(!curveOption.closest('label').getClientRects().length &&
                  document.getElementById('plotKind').value === 'auto',
                  'Clearing the fixed value hides the curve option and removes its stale selection');
            check(document.getElementById('plotAzimuth').value === '315' &&
                  document.getElementById('plotAzimuthValue').textContent === '315°' &&
                  !!plotRendered.querySelector('[data-surface][data-azimuth="315.000"]'),
                  'Initial surface and camera controls must share the ascending-axis default');
            for (let axis = 0; axis < 3; ++axis) {
                const positions = [...plotRendered.querySelectorAll(`[data-plot-tick="${axis}"] path`)].map(path => {
                    const point = path.getPointAtLength(0);
                    return axis === 2 ? -point.y : point.x;
                });
                check(positions.length > 1 && positions.every((value, i) => !i || value > positions[i - 1]),
                      'Initial horizontal ticks must increase left to right and height ticks bottom to top');
            }
            const plotControls = document.getElementById('plotRange');
            await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
            const manualRadio = document.querySelector('#plotScaleChoices input[value="manual"]');
            manualRadio.click();
            check(document.getElementById('plotVertical').getClientRects().length > 0 &&
                  document.getElementById('plotManual').getClientRects().length > 0 &&
                  document.getElementById('plotVerticalTitle').textContent === 'Height range',
                  'Surface height controls must expose editable manual limits inside the plot card');
            document.getElementById('plotYLower').value = '0';
            document.getElementById('plotYUpper').value = '1/2';
            await takePlot('x');
            const heightLabels = [...plotRendered.querySelectorAll('[data-plot-label="2"]')]
                .map(label => Number(label.textContent));
            const croppedSurface = plotRendered.querySelector('[data-surface]');
            check(Number(croppedSurface.dataset.zMin) === 0 && Number(croppedSurface.dataset.zMax) === 0.5 &&
                  heightLabels.every(value => value >= 0 && value <= 0.5) &&
                  !!croppedSurface.querySelector('[data-axis-handle="2"]') &&
                  !!plotRendered.querySelector('[data-series] path'),
                  'Manual height bounds must crop the rendered surface without losing its axes');
            document.querySelector('#plotScaleChoices input[value="auto"]').click();
            await takePlot('x');
            check(document.getElementById('plotScale').value === 'auto' &&
                  document.querySelector('#plotScaleChoices input[value="auto"]').checked &&
                  document.getElementById('plotManual').getClientRects().length === 0,
                  'Reset must restore automatic surface scaling');
            check(plotControls.closest('.result-card') === plotRendered.closest('.result-card') &&
                  !document.getElementById('workspacePanel').contains(plotControls) &&
                  getComputedStyle(plotControls).display !== 'none',
                  'Plot controls must live inside the visible plot card');
            for (const id of ['plotAzimuth', 'plotElevation', 'plotRoll', 'plotZoom']) {
                check(document.getElementById(id).type === 'range', '3-D camera controls must be sliders');
                const slider = document.getElementById(id);
                const field = slider.closest('.plot-field');
                check(Math.abs(slider.getBoundingClientRect().width - field.getBoundingClientRect().width) < 1 &&
                      slider.getBoundingClientRect().height >= 44 &&
                      Math.abs(field.querySelector('output').getBoundingClientRect().top -
                          field.querySelector('label').getBoundingClientRect().top) < 1 &&
                      field.querySelector('label').getBoundingClientRect().bottom <=
                          slider.getBoundingClientRect().top,
                      'Camera values sit beside labels above full-width sliders with touch-sized targets');
            }
            const savedCardWidth = plotCard.style.width;
            plotCard.style.width = '360px';
            check(getComputedStyle(plotControls).gridTemplateColumns.split(' ').length === 1 &&
                  plotControls.scrollWidth <= plotControls.clientWidth + 1 &&
                  getComputedStyle(document.getElementById('plotCamera')).gridTemplateColumns.split(' ').length === 2 &&
                  document.getElementById('plotAzimuth').getBoundingClientRect().width > 100,
                  'Narrow plot cards use a compact two-by-two camera grid without overflow');
            checkPlotRangeAlignment();
            await checkPlotChoices();
            plotCard.style.width = savedCardWidth;
            checkPlotControls(true);
            check(document.getElementById('plotCamera').classList.contains('plot-options-card') &&
                  plotControls.firstElementChild.id === 'plotCamera' &&
                  document.getElementById('plotCamera').getBoundingClientRect().bottom <=
                      document.getElementById('plotVertical').getBoundingClientRect().top &&
                  !document.querySelector('#plotCamera .plot-hint'),
                  'Compact camera controls come first below the chart, with gestures in Help');
            const zoomSlider = document.getElementById('plotZoom');
            const zoomToken = labRequests.latestMain();
            zoomSlider.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, button: 0, isPrimary: true}));
            zoomSlider.value = '1';
            zoomSlider.dispatchEvent(new Event('input', {bubbles: true}));
            await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
            await new Promise(resolve => setTimeout(resolve, 450));
            check(Number(plotRendered.querySelector('[data-surface]').dataset.cameraZoom) === 2 &&
                  document.getElementById('plotZoomValue').textContent === '200%' &&
                  labRequests.latestMain() === zoomToken,
                  'Held zoom slider updates the clipped volume without requesting native samples');
            zoomSlider.dispatchEvent(new Event('change', {bubbles: true}));
            await waitForVolume();
            plotCard.querySelector('[data-zoom-reset]').click();
            await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
            check(zoomSlider.value === '0' && document.getElementById('plotZoomValue').textContent === '100%',
                  'Camera reset synchronises the zoom slider and readout');
            await waitForVolume();
            await window.checkLabMobileLayout('surface plot and height controls');
            const facets = [...plotRendered.querySelectorAll('[data-surface] [data-series] path')];
            check(facets.length === 2 * 69 * 69 && new Set(facets.map(path => path.getAttribute('fill'))).size > 4,
                  'Full surface must contain the shaded 70-by-70 mesh');
            check(!!plotRendered.querySelector('[data-plot-title] use'), 'Surface must retain its rendered formula title');
            checkPlotLabelSpacing();
            for (const [axis, name] of [[0, 'x'], [1, 'y'], [2, 'u']]) {
                check(!!plotRendered.querySelector(`[data-axis-arrow="${axis}"]`) &&
                      plotRendered.querySelector(`[data-axis-name="${axis}"]`).textContent.trim() === name,
                      'Surface axes must identify their variables at increasing-value arrowheads');
            }
            for (const axis of [0, 1, 2])
                check(plotRendered.querySelectorAll(`[data-plot-tick="${axis}"] path`).length >= 3,
                      'Every surface axis needs visible tick strokes');
            const initialMesh = plotRendered.querySelector('[data-series]').innerHTML;
            const sliderRequest = labRequests.latestMain();
            document.getElementById('plotAzimuth').value = '120';
            document.getElementById('plotElevation').value = '40';
            document.getElementById('plotRoll').value = '90';
            document.getElementById('plotAzimuth').dispatchEvent(new Event('input', {bubbles: true}));
            document.getElementById('plotElevation').dispatchEvent(new Event('input', {bubbles: true}));
            document.getElementById('plotRoll').dispatchEvent(new Event('input', {bubbles: true}));
            check(document.getElementById('plotAzimuthValue').textContent === '120°' &&
                  document.getElementById('plotElevationValue').textContent === '40°' &&
                  document.getElementById('plotRollValue').textContent === '90°',
                  'Slider readouts must follow input before committing a redraw');
            check(plotRendered.querySelector('[data-series]').innerHTML === initialMesh,
                  'Slider input alone must not launch repeated native requests');
            await new Promise(resolve => setTimeout(resolve, 450));
            check(plotRendered.querySelector('[data-local-camera="120,40,90"]') &&
                  labRequests.latestMain() === sliderRequest &&
                  plotRendered.querySelector('[data-series]').innerHTML === initialMesh,
                  'All camera sliders update WebGL while held, without a debounce-time sampling request');
            document.getElementById('plotRoll').dispatchEvent(new Event('change', {bubbles: true}));
            const rotationDeadline = performance.now() + 15000;
            while (plotRendered.marsPlotDrag.busy &&
                   performance.now() < rotationDeadline)
                await new Promise(resolve => setTimeout(resolve, 25));
            check(!!plotRendered.querySelector('[data-local-camera="120,40,90"]') &&
                  labRequests.latestMain() === sliderRequest,
                  'Committing camera sliders keeps the local camera without native sampling');
            check(plotRendered.querySelector('[data-series]').innerHTML === initialMesh,
                  'Rotation reuses native geometry');
            checkPlotLabelSpacing();
            for (const [azimuth, elevation, roll] of [[45, 30, 0], [45, 5, 180], [225, 85, 270]]) {
                document.getElementById('plotLower').value = '-10';
                document.getElementById('plotUpper').value = '10';
                document.getElementById('plotSecondLower').value = '-10';
                document.getElementById('plotSecondUpper').value = '10';
                document.getElementById('plotAzimuth').value = String(azimuth);
                document.getElementById('plotElevation').value = String(elevation);
                document.getElementById('plotRoll').value = String(roll);
                await takePlot('x');
                checkPlotLabelSpacing();
            }
            for (const id of ['plotAzimuth', 'plotElevation', 'plotRoll']) {
                const style = getComputedStyle(document.getElementById(id));
                check(style.backgroundColor !== 'rgb(255, 255, 255)' && style.color !== 'rgb(0, 0, 0)',
                      'Camera controls must retain the dark Lab theme');
            }
            const surfaceExpand = plotRendered.closest('.result-card').querySelector('[data-expand-card]');
            surfaceExpand.click();
            check(labWorkspace.classList.contains('result-card-expanded'), 'Surface must expand across the workspace');
            check(plotControls.getBoundingClientRect().width > 0 &&
                  plotRendered.closest('.expanded-card').contains(plotControls),
                  'Expanded plot must retain its controls');
            checkPlotRangeAlignment();
            await checkPlotWheelZoom();
            await checkPlotDrag();
            document.getElementById('plotAzimuth').value = '75';
            document.getElementById('plotAzimuth').dispatchEvent(new Event('input', {bubbles: true}));
            document.getElementById('plotAzimuth').dispatchEvent(new Event('change', {bubbles: true}));
            const updateDeadline = performance.now() + 15000;
            while (plotRendered.marsPlotDrag.busy && performance.now() < updateDeadline)
                await new Promise(resolve => setTimeout(resolve, 25));
            check(Number(plotRendered.querySelector('[data-local-camera]')?.dataset.localCamera.split(',')[0]) === 75 &&
                  labWorkspace.classList.contains('result-card-expanded'),
                  'Live plotting must work without leaving the expanded card');
            surfaceExpand.click();
            document.getElementById('plotLower').value = '-10';
            const retainedSurface = labSurfacePreview.snapshot();
            for (let cycle = 0; cycle < 2; ++cycle) {
                await selectWorksheetMode('matrix');
                check(!document.querySelector('#plotRendered [data-surface-overlay]'),
                      'Leaving a surface releases its transient renderer');
                await selectWorksheetMode('expression');
                await nextLayout();
                const restored = labSurfacePreview.snapshot();
                const chart = plotRendered.querySelector('[data-surface]');
                const overlay = plotRendered.querySelector('[data-surface-overlay]');
                check(chart && restored && overlay && overlay.style.display !== 'none' &&
                      plotRendered.querySelectorAll('[data-surface-overlay]').length === 1 &&
                      chart.dataset.localRenderer && labSurfacePreview.prepare(chart).framePlan.drawMesh,
                      'Returning to Expression recreates a visible surface renderer, not empty saved canvases');
                check(restored.key === retainedSurface.key &&
                      restored.orientation.every((v, i) => Math.abs(v - retainedSurface.orientation[i]) < 1e-10) &&
                      restored.volume.exponent === retainedSurface.volume.exponent,
                      'Mode round trips retain the exact volume and quaternion camera');
                check(!modeResultState.expression.plotRendered.innerHTML.includes('data-surface-overlay'),
                      'Saved plot HTML contains only reusable native geometry');
            }
            document.getElementById('plotAzimuth').value = '45';
            document.getElementById('plotElevation').value = '30';
            document.getElementById('plotRoll').value = '0';
            document.getElementById('plotUpper').value = '10';
            document.getElementById('plotSecondLower').value = '-10';
            document.getElementById('plotSecondUpper').value = '10';
            setExpressionEditor('3+4');
            await evaluateExpression();
            await waitForAutomaticPlot();
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
            ]) {
                setMode(mode);
                syncModeUI();
                setExpressionEditor(source);
                await evaluateCurrentMode();
                await commitVisibleBindingInputs();
                const button = document.getElementById('expressionPlot');
                const authored = currentExpressionText();
                check(!button, mode + ' has no separate Plot button');
                await waitForAutomaticPlot();
                check(!!plotRendered.querySelector('[data-series]'), mode + ' evaluates an automatic plot');
                check(!!rendered.innerHTML && !!functionStyle.textContent, mode + ' retains mathematical cards');
                if (source === 'cos(z)-sin(pi-xy)=0') {
                    check(!!rendered.querySelector('svg') && !rendered.textContent.includes('rendering failed'),
                          'Implicit trigonometric solution families render as mathematics');
                }
                document.getElementById('plotLower').value = mode === 'equation' ? '' : '-2';
                document.getElementById('plotUpper').value = mode === 'equation' ? '' : '2';
                await takePlot(document.getElementById('plotFirst').value);
                check(plotRendered.querySelectorAll('[data-series]').length === series, mode + ' plots all native curves: ' +
                      JSON.stringify({source, variables: currentVariables, status: statusEl.textContent,
                                      error: plotRendered.textContent.slice(0, 400)}));
                check(!!plotRendered.querySelector('[data-surface]') === surface, mode + ' selects native dimensionality');
                if (source === 'cos(z)-sin(2-xy)=0') {
                    document.getElementById('plotScale').value = 'manual';
                    for (const id of ['plotLower', 'plotSecondLower', 'plotYLower'])
                        document.getElementById(id).value = '-5';
                    for (const id of ['plotUpper', 'plotSecondUpper', 'plotYUpper'])
                        document.getElementById(id).value = '5';
                    await takePlot('z');
                    const initialChart = plotRendered.querySelector('[data-surface]');
                    const initialRequest = labRequests.latestMain();
                    const target = plotRendered.querySelector('[data-axis-handle]');
                    const send = (node, type, x, y) => node.dispatchEvent(new PointerEvent(type, {
                        clientX: x, clientY: y, button: 0, buttons: type === 'pointerup' ? 0 : 1,
                        pointerId: 31, pointerType: 'touch', isPrimary: true, bubbles: true, cancelable: true
                    }));
                    send(target, 'pointerdown', 100, 100);
                    send(window, 'pointermove', 140, 110);
                    const deadline = performance.now() + 20000;
                    while (!plotRendered.querySelector('[data-local-renderer]') && performance.now() < deadline)
                        await new Promise(resolve => setTimeout(resolve, 25));
                    const preview = plotRendered.querySelector('[data-local-renderer]');
                    check(preview === initialChart && labRequests.latestMain() === initialRequest &&
                          plotRendered.marsPlotDrag.active && plotRendered.querySelector('[data-surface-overlay] canvas'),
                          'Wide implicit equations rotate the retained mesh without native sampling before release');
                    send(window, 'pointerup', 140, 110);
                    const finish = performance.now() + 20000;
                    while ((plotRendered.marsPlotDrag.busy ||
                            !plotRendered.querySelector('[data-plot-quality="full"]')) && performance.now() < finish)
                        await new Promise(resolve => setTimeout(resolve, 25));
                    const full = plotRendered.querySelector('[data-plot-quality="full"]');
                    check(full === initialChart && plotRendered.querySelector('[data-local-renderer]') &&
                          labRequests.latestMain() === initialRequest && !plotRendered.marsPlotDrag.busy &&
                          !plotRendered.querySelector('[data-preview-notice]'),
                          'Releasing an implicit-equation axis keeps the full retained mesh without sampling');
                    const orientation = labSurfacePreview.snapshot().orientation;
                    initialChart.dispatchEvent(new WheelEvent('wheel', {
                        deltaY: 100, altKey: true, bubbles: true, cancelable: true
                    }));
                    await new Promise(resolve => setTimeout(resolve, 450));
                    check(labRequests.latestMain() === initialRequest &&
                          labSurfacePreview.snapshot().orientation.some((v, i) => Math.abs(v - orientation[i]) > 0.001),
                          'Equation-mode wheel rotation uses the same persistent camera as expression mode');
                }
                if (source === 'x^2+y^2+z^2=1') {
                    const card = plotRendered.closest('.result-card');
                    const retained = plotRendered.querySelector('[data-surface]');
                    const generation = labRequests.latestMain();
                    for (let step = 0; step < 5; step++) card.querySelector('[data-zoom-step="1"]').click();
                    await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
                    check(retained.isConnected && retained.querySelector('[data-uv]') &&
                          Number(retained.dataset.cameraZoom) > 3 && labRequests.latestMain() === generation,
                          'Volume preview stays interactive even when the selected sphere interior is empty');
                    await waitForVolume();
                    const empty = plotRendered.querySelector('[data-surface]');
                    check(empty?.dataset.empty === 'true' && plotRendered.querySelector('[data-local-renderer]'),
                          'Native sampling of the empty sphere interior retains interactive axes');
                    labDOM.call('lab_layout_plot', null, 'Plot request timed out');
                    for (let step = 0; step < 5; step++) card.querySelector('[data-zoom-step="-1"]').click();
                    await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
                    check(empty.isConnected && Math.abs(Number(empty.dataset.cameraZoom) - 1) < 1e-10,
                          'A retained surface can still zoom back out after a failed redraw');
                    await waitForVolume();
                    check(plotRendered.querySelector('[data-surface] [data-uv]'),
                          'Zooming out of an empty volume samples the sphere again');
                    card.querySelector('[data-zoom-reset]').click();
                    await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
                    check(document.getElementById('plotScale').value === 'auto',
                          'Volume reset restores automatic mathematical bounds');
                    await waitForVolume();
                }
                if (document.getElementById('plotScale').value === 'auto')
                    check([...document.querySelectorAll('#plotRange .plot-bound-pair')]
                              .every(pair => pair.getClientRects().length === 0),
                          mode + ' hides all bound-entry fields in Auto');
                if (!surface) {
                    const axisLabel = document.querySelector('#plotFirstChoices label:only-child');
                    if (axisLabel) {
                        const style = getComputedStyle(axisLabel);
                        check(style.backgroundColor === 'rgba(0, 0, 0, 0)' &&
                              style.borderTopColor === 'rgba(0, 0, 0, 0)' && axisLabel.textContent.endsWith(' range'),
                              'Single-axis range has a plain readable heading, not a cramped selection chip');
                    }
                }
                check((getComputedStyle(document.getElementById('plotSecondGroup')).display !== 'none') === surface,
                      mode + ' shows second-variable controls only for a surface');
                check((getComputedStyle(document.getElementById('plotCamera')).display !== 'none') === surface,
                      mode + ' shows camera controls only for a surface');
                if (mode === 'equation') {
                    if (source === 'x^2+y^2+z^2=1') {
                        const mesh = plotRendered.querySelector('[data-mesh="true"]');
                        check(mesh && Number(mesh.dataset.sampleZMin) < -0.99 &&
                              Number(mesh.dataset.sampleZMax) > 0.99,
                              'The sphere contains both hemispheres, not a single height branch');
                        const silhouette = mesh.querySelector('[data-series]').getBBox();
                        check(Math.abs(silhouette.width / silhouette.height - 1) < 0.03 &&
                              Number(mesh.dataset.xMin) > -1.3 && Number(mesh.dataset.xMax) < 1.3 &&
                              Number(mesh.dataset.zMin) > -1.3 && Number(mesh.dataset.zMax) < 1.3,
                              'The sphere stays round inside a tightly fitted equal-unit box');
                        const axesBox = mesh.querySelector('[data-plot-axes]').getBBox();
                        const top = Math.min(silhouette.y, axesBox.y);
                        const bottom = Math.max(silhouette.y + silhouette.height, axesBox.y + axesBox.height);
                        check(silhouette.width > mesh.viewBox.baseVal.width * 0.36 &&
                              top < mesh.viewBox.baseVal.height * 0.12 &&
                              bottom > mesh.viewBox.baseVal.height * 0.88,
                              'The surface and axes fill the plot without large empty top and bottom margins');
                    }
                    if (source === 'x^2+y^2=1') {
                        const chart = plotRendered.querySelector('[data-plot-chart]');
                        const d = chart.dataset, quadrants = new Set();
                        const paths = [...chart.querySelectorAll('[data-series] path')];
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
                    const solutionCard = document.getElementById('valueCard');
                    check(solutionCard.getClientRects().length > 0 &&
                          document.getElementById('valueTitle').textContent === 'Solutions' && value.textContent.trim(),
                          'Equation plots retain their Solutions card');
                    if (source.includes('x^4'))
                        for (const root of ['-4.671042', '-0.489034', '0.633721', '5.526355'])
                            check(value.textContent.includes(root), 'Equation plot includes numerical root ' + root);
                    check(!plotRendered.textContent.includes('Left side') && !plotRendered.textContent.includes('Right side'),
                          'Equation plots omit generic side labels');
                    if (source.includes('x^4')) {
                        const ticks = [...plotRendered.querySelectorAll('[data-plot-label="1"]')].map(t => Number(t.textContent));
                        check(ticks.includes(-200) && ticks.includes(200) && ticks.every(t => Math.abs(t) < 300),
                              'Automatic equation frame shows the quartic turning points without distant tails');
                        await checkCurvePointerView();
                        await checkLivePlotControls();
                    }
                }
                if (mode === 'diffequation' && !surface && series === 1)
                    check(!plotRendered.textContent.includes('Solution 1'), 'A single solution does not need a legend');
                if (mode === 'diffequation' && !surface && series > 1) {
                    const titles = [...plotRendered.querySelectorAll('[data-series] > title')];
                    check(titles.length === series && titles.every(title => title.marsPlotLabel.includes(' = ')),
                          'Every family curve has a native hover tooltip identifying its constants');
                    check([...plotRendered.querySelectorAll('svg title')].every(title => !title.textContent) &&
                          plotRendered.dataset.displaySvg.includes(titles[0].marsPlotLabel),
                          'Only the custom tooltip is active; copied native SVG retains its labels');
                    check(![...plotRendered.querySelectorAll('text')].some(text => text.textContent.includes(' = ')),
                          'Family constants must not clutter the plot with a visible legend');
                    const curves = [...plotRendered.querySelectorAll('[data-series]')];
                    const chart = plotRendered.querySelector('[data-x-min]');
                    const transform = chart.getScreenCTM();
                    const point = {
                        clientX: transform.e + (Number(chart.dataset.plotLeft) + Number(chart.dataset.plotWidth) / 2) * transform.a,
                        clientY: transform.f + (Number(chart.dataset.plotTop) + Number(chart.dataset.plotHeight) / 2) * transform.d,
                        bubbles: true, pointerType: 'mouse'
                    };
                    for (const active of curves.slice(0, 2)) {
                        active.querySelector('path').dispatchEvent(new PointerEvent('pointermove', point));
                        check(curves.every(curve => Number(getComputedStyle(curve).opacity) === (curve === active ? 1 : 0.18)),
                              'Hovering a family curve dims only the other solutions');
                        check(active.getAttribute('aria-label') === active.querySelector('title').marsPlotLabel,
                              'Focused family curve retains its constant tooltip');
                        const tooltip = plotRendered.querySelector('.plot-pointer-tip');
                        const lines = tooltip.textContent.split('\n');
                        const constants = active.querySelector('title').marsPlotLabel.split(', ');
                        check(!plotRendered.querySelector('.plot-pointer-horizontal').classList.contains('hidden'),
                              'Selected family curves display a horizontal guide as well as a vertical guide');
                        check(!plotRendered.querySelector('.plot-pointer-marker').classList.contains('hidden'),
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
                    check(plotRendered.querySelector('.plot-pointer-tip').textContent.split('\n').length === 2,
                          'Moving off a curve removes stale constants from the coordinate tooltip');
                    curves[0].querySelector('path').dispatchEvent(new PointerEvent('pointermove', point));
                    plotRendered.dispatchEvent(new PointerEvent('pointerleave'));
                    check(curves.every(curve => Number(getComputedStyle(curve).opacity) === 1),
                          'Leaving a family plot clears curve focus');
                }
                if (!surface) {
                    document.querySelector('#plotScaleChoices input[value="manual"]').click();
                    document.getElementById('plotYLower').value = '-2';
                    document.getElementById('plotYUpper').value = '2';
                    const beforeUpdate = plotRendered.querySelector('[data-series]');
                    document.getElementById('plotYUpper').dispatchEvent(new Event('input', {bubbles: true}));
                    const deadline = performance.now() + 15000;
                    const updated = () => plotRendered.querySelector('[data-series]') &&
                        plotRendered.querySelector('[data-series]') !== beforeUpdate &&
                        plotRendered.querySelector('[data-plot-label="1"]');
                    while (!updated() && performance.now() < deadline)
                        await new Promise(resolve => setTimeout(resolve, 25));
                    const ticks = [...plotRendered.querySelectorAll('[data-plot-label="1"]')].map(t => Number(t.textContent));
                    check(plotRendered.querySelector('[data-series]') !== beforeUpdate && ticks.includes(-1) &&
                          ticks.includes(1) && ticks.every(t => t >= -2 && t <= 2),
                          mode + ' Update plot applies manual vertical bounds');
                    if (mode === 'equation')
                        check(document.getElementById('valueCard').getClientRects().length > 0 && value.textContent.trim(),
                              'Live equation range updates retain the solutions');
                    document.querySelector('#plotScaleChoices input[value="auto"]').click();
                }
                check((document.getElementById('plotCamera').getClientRects().length > 0) === surface,
                      mode + ' shows camera controls only for surfaces');
                check(currentMode() === mode && currentExpressionText() === authored, mode + ' retains the authored problem: ' +
                      JSON.stringify({mode: currentMode(), before: authored, after: currentExpressionText()}));
                check(!currentDifferentiable, mode + ' does not expose expression derivative buttons');
                await window.checkLabMobileLayout(mode + ' plot');
                await evaluateCurrentMode();
                const solutionCard = document.getElementById('valueCard');
                check(solutionCard.getClientRects().length > 0 && !solutionCard.classList.contains('hidden') &&
                      document.getElementById('valueTitle').textContent === 'Solutions',
                      mode + ' restores its solution card after plotting in the same mode');
                check(document.getElementById('renderedTitle').textContent ===
                      (mode === 'equation' ? 'Rendered TeX' : 'Equation and solutions'),
                      mode + ' restores its mathematical heading instead of retaining Plot');
                if (mode === 'equation' && source.includes('x^4')) {
                    for (const root of ['-4.671042', '-0.489034', '0.633721', '5.526355'])
                        check(value.textContent.includes(root), 'Re-evaluated quartic retains numerical root ' + root);
                }
                await waitForAutomaticPlot();
                await window.checkLabMobileLayout(mode + ' restored solutions');
            }
            setExpressionEditor("y''+3y*y'+y^4=0; y(0)=1; y'(0)=0");
            await evaluateCurrentMode();
            check(!document.getElementById('expressionPlot'), 'Local Taylor series must not advertise unsupported plotting');
            setMode('integrator');
            syncModeUI();
            setExpressionEditor('sin(x)');
            renderIntegratorRows([{kind: 'bound', name: 'x', lo: '-pi', hi: 'pi'}]);
            await evaluateCurrentMode();
            await waitForAutomaticPlot();
            check(!!plotRendered.querySelector('[data-integral-shading]') &&
                  !!plotRendered.querySelector('[data-sign="positive"]') &&
                  !!plotRendered.querySelector('[data-sign="negative"]'),
                  'Integrator Evaluate independently plots and shades both signed contributions');
            check(!!rendered.querySelector('svg') && value.textContent.trim(),
                  'Integrator plot retains its mathematical and numerical results');
            const integrationRows = JSON.stringify(currentIntegratorBoundRows());
            document.getElementById('plotLower').value = '-1';
            document.getElementById('plotUpper').value = '1';
            await takePlot('x');
            check(JSON.stringify(currentIntegratorBoundRows()) === integrationRows,
                  'Integrator viewport changes never alter integration limits');
            setExpressionEditor('sin(t)');
            renderIntegratorRows([{kind: 'bound', name: 't', lo: '', hi: ''}]);
            await evaluateCurrentMode();
            await waitForAutomaticPlot();
            check(!!plotRendered.querySelector('[data-series]') &&
                  !plotRendered.querySelector('[data-integral-shading]') &&
                  document.getElementById('plotFirst').value === 't',
                  'Indefinite integrals plot the integrand without shading using the authored variable');
            setMode('expression');
            syncModeUI();
            setExpressionEditor('3+4');
            await evaluateExpression();
            const ordinaryZoom = resultZoomIndex(rendered.closest('.result-card'));
            check(rendered.dispatchEvent(new WheelEvent('wheel', {deltaY: -100, bubbles: true, cancelable: true})) &&
                  resultZoomIndex(rendered.closest('.result-card')) === ordinaryZoom,
                  'Ordinary formula results must retain unmodified wheel scrolling');

            const card = rendered.closest('.result-card');
            const before = resultZoomIndex(card);
            stepResultZoom(card, 1);
            check(resultZoomIndex(card) === before + 1, 'Result zoom did not change');
            await checkRenderedGlyphs();
            const expand = card.querySelector('[data-expand-card]');
            expand.click();
            check(expand.getAttribute('aria-expanded') === 'true', 'Result expansion failed');
            help.click();
            check(!helpPane.classList.contains('hidden'), 'Help did not open');
            check(expand.getAttribute('aria-expanded') === 'false', 'Help must leave result expansion cleanly');
            help.click();

            for (const mode
                     of ['matrix', 'integrator', 'datetime', 'almanac', 'equation', 'diffequation', 'expression']) {
                setMode(mode);
                restoreModeEditor(mode);
                syncModeUI();
                check(currentMode() === mode, `Mode ${mode} unavailable`);
                check(
                    document.querySelector(`[data-mode="${mode}"]`).classList.contains('active'),
                    `Mode ${mode} tab inactive`);
                const sourceBeforeHelp = expr.value;
                await window.checkLabMobileLayout(mode + ' worksheet');
                help.click();
                const helpClose = document.getElementById('helpClose');
                const workspace = document.getElementById('workspacePanel');
                const panel = document.getElementById('resultWorkspacePanel').getBoundingClientRect();
                const bar = document.querySelector('.lab-topbar').getBoundingClientRect();
                check(getComputedStyle(workspace).display === 'none', `${mode} editor hidden for Help`);
                check(Math.abs(panel.left - bar.left) < 2 && Math.abs(panel.right - bar.right) < 2,
                      `${mode} Help must span both workspace columns`);
                check(document.activeElement === helpClose, `${mode} Help return button is keyboard accessible`);
                for (const card of helpPane.querySelectorAll('.help-card')) {
                    const selected = card.dataset.helpModes.split(',').includes(mode);
                    check(card.classList.contains('hidden') !== selected, `${mode} Help topic filtering`);
                }
                await window.checkLabMobileLayout(mode + ' help');
                helpClose.click();
                check(!document.getElementById('labWorkspace').classList.contains('help-open'),
                      `${mode} Help return restores the workspace`);
                check(document.activeElement === help && expr.value === sourceBeforeHelp,
                      `${mode} Help return preserves the input and restores focus`);
            }
            setMode('matrix');
            syncModeUI();
            const matrixSource = '(x, 0; 0, x^2)';
            await prepareLabEditor(matrixSource);
            setExpressionEditor(matrixSource);
            matrixOperation.value = 'eval';
            await evaluateMatrix();
            check(!rendered.classList.contains('error'), 'Shared matrix renderer failed');
            await window.checkLabMobileLayout('matrix result');
            await takeDerivative('x');
            check(!rendered.classList.contains('error'), 'Native matrix derivative construction failed');
            check(parsed.dataset.fullText.length > 0, 'Matrix derivative result missing');
            await takeIntegral('x');
            check(!rendered.classList.contains('error'), 'Native matrix integral construction failed');
            check(parsed.dataset.fullText.length > 0, 'Matrix integral result missing');
            lastMatrixScalarExpression = 'x^2';
            await takeDerivative('x');
            check(!rendered.classList.contains('error'), 'Shared scalar-matrix calculus handler failed');
            check(parsed.dataset.fullText.includes('x'), 'Scalar-matrix derivative missing');
            lastMatrixScalarExpression = '';
            check(!labDefinitionScripts.includes('calculus'), 'Retired calculus script still loaded');
            setMode('datetime');
            syncModeUI();
            const calendarResponse = await labFetch('/datetime-eval', {
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
            const calendarData = await calendarResponse.labData();
            check(calendarResponse.ok && calendarData.ok, 'Native DateTime evaluation failed');
            await installEvaluationResult(5, calendarData);
            await window.checkLabMobileLayout('all calendar result cards');
            for (const [element, name] of [
                     [rendered, 'overview'], [parsed, 'range'], [functionStyle, 'calendar'], [value, 'solar'],
                     [datetimeLocalBody, 'local']]) {
                const sections = calendarData[`${name}_sections`].filter(section => section.rows.length);
                check(sections.length > 0, `Native ${name} card has no sections`);
                check(
                    element.querySelectorAll('details.datetime-section').length === sections.length,
                    `Native ${name} card degraded to plain text`);
                check(
                    getComputedStyle(element.querySelector('.datetime-section-grid')).display === 'grid',
                    `Native ${name} card lost its grid layout`);
                const details = element.querySelector('details');
                const open = details.open;
                details.querySelector('summary').click();
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
            const holidayLabels = [...functionStyle.querySelectorAll('.datetime-row-label')];
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
            let almanacReply = {...almanacFixture, ok: true};
            window.fetch = (url, options) => String(url) === '/almanac-eval' ?
                Promise.resolve(
                    new Response(labWire.encode(almanacReply), {headers: {'Content-Type': 'application/x-protobuf'}})) :
                beforeCalendarFetch(url, options);
            try {
                setMode('almanac');
                syncModeUI();
                await evaluateAlmanac({skipHistoryUpdate: true});
                check(
                    !rendered.classList.contains('error') &&
                        document.getElementById('status').textContent.startsWith('Ready') &&
                        rendered.querySelectorAll('.almanac-grid-table:not(.almanac-event-table) tbody tr').length ===
                            4,
                    'Successful Almanac evaluation did not install a visible worksheet');
                rendered.querySelector('[data-almanac-visibility="visible"]').click();
                await window.checkLabMobileLayout('almanac result');
                check(
                    rendered.querySelectorAll('.almanac-grid-table:not(.almanac-event-table) tbody tr').length === 1 &&
                        rendered.dataset.copyText === almanacFixture.almanac_presentation.visible.copy_text,
                    'Evaluated Almanac cannot switch to the native visible-body variant');
                const acceptedAlmanac = almanacLastWorksheetData;
                almanacReply = {ok: true, visibility: 'all'};
                await evaluateAlmanac({skipHistoryUpdate: true});
                check(
                    rendered.classList.contains('error') && rendered.textContent.includes('Almanac presentation') &&
                        document.getElementById('status').textContent.startsWith('Error') &&
                        almanacLastWorksheetData === acceptedAlmanac,
                    'Incomplete Almanac response reported Ready or replaced accepted state');
                almanacReply = {...almanacFixture, ok: true, visibility: 'visible'};
                await evaluateAlmanac({skipHistoryUpdate: true});
                check(
                    !rendered.classList.contains('error') && rendered.querySelector('.almanac-grid-table') &&
                        document.getElementById('status').textContent.startsWith('Ready'),
                    'Almanac could not recover after an incomplete response');
            } finally {
                window.fetch = beforeCalendarFetch;
                setMode('datetime');
                syncModeUI();
                await installEvaluationResult(5, calendarData);
            }
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
            // Page-hide persistence reads the editor, not merely the explicit saved-state fixture.
            await prepareLabEditor('3+4');
            setExpressionEditor('3+4');
            await saveWorksheetState('expression', '3+4');
            // Matrix evaluation persists its mode; restore the restart fixture explicitly.
            const resetMode =
                await labFetch('/state', {method: 'POST', body: labWire.encode({lab_mode: 'expression'})});
            check(resetMode.ok, 'Could not restore expression-mode restart fixture');
            const saved = await (await labFetch('/state')).labData();
            check(saved.expression === '3+4', 'Worksheet state not saved');
            check(!failures.length, failures.join('; '));
            location.href = '/?failure=catalogue';
        } catch (error) {
            await report(`FAIL: ${error.message}\n${error.stack || ''}`);
        }
    });
})();
