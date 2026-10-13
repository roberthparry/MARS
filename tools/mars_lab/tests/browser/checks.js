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
    function checkPlotLabelSpacing() {
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
        check(!document.querySelector('#plotRange select, #plotRange .select-shell'),
              'Plot settings must not contain dropdowns');
        for (const id of ['plotKind', 'plotScale']) {
            const control = document.getElementById(id), saved = control.value;
            const group = document.getElementById(id + 'Choices');
            const radios = [...group.querySelectorAll('input[type="radio"]')];
            const reset = group.querySelector('[data-plot-reset]');
            if (!group.getClientRects().length)
                continue;
            check(control.type === 'hidden' && radios.length === 2 && radios.every(radio => radio.value !== 'auto'),
                  'Small option sets must use two explicit radios with no Automatic option');
            try {
                control.value = 'auto';
                control.dispatchEvent(new Event('change', {bubbles: true}));
                check(radios.every(radio => !radio.checked) && reset.disabled,
                      'Automatic must be the unselected default');
                for (const radio of radios) {
                    radio.click();
                    check(control.value === radio.value && radio.checked &&
                          radios.filter(option => option.checked).length === 1 && !reset.disabled,
                          'Radio overrides must be mutually exclusive and reach native payload controls');
                    if (id === 'plotScale')
                        check(document.getElementById('plotManual').classList.contains('hidden') ===
                              (radio.value !== 'manual'), 'Manual limits should appear only when selected');
                }
                await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
                const card = group.closest('.result-card').getBoundingClientRect();
                for (const label of group.querySelectorAll('label')) {
                    const box = label.getBoundingClientRect();
                    check(box.left >= card.left && box.right <= card.right,
                          'Radio options must stay inside normal, expanded and narrow plot cards');
                }
                reset.click();
                check(control.value === 'auto' && radios.every(radio => !radio.checked) && reset.disabled,
                      'Reset must return to automatic without a separate Automatic option');
            } finally {
                control.value = saved;
                control.dispatchEvent(new Event('change', {bubbles: true}));
            }
        }
    }
    function checkPlotRangeAlignment() {
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
        checkPlotRangeAlignment();
        const visible = id => document.getElementById(id).getClientRects().length > 0;
        check(visible('plotCamera') === surface && visible('plotVertical') !== surface,
              'Only the applicable dimension controls should be visible');
        const first = [...document.querySelectorAll('#plotFirstChoices input')];
        const second = [...document.querySelectorAll('#plotSecondChoices input')];
        check(first.length > 0 && first.every(radio => radio.type === 'radio'), 'Variables must be radio choices');
        check(document.getElementById('plotSecond').type === 'hidden', 'No wide free-text variable input');
        check(second.length === first.length && second.every(radio => radio.value !== ''),
              'Only actual variables belong in the second-axis radio group');
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
            labDOM.bindEvents({...previous, takePlot: variable => selected = variable});
            try {
                document.getElementById('plotUpdate').click();
                check(selected === 'y', 'Update plot must use the selected first-axis radio');
            } finally {
                labDOM.bindEvents(previous);
                x.click();
            }
        }
    }
    async function checkPlotDrag() {
        const controls = ['plotAzimuth', 'plotElevation', 'plotRoll'].map(id => document.getElementById(id));
        const saved = controls.map(control => control.value);
        const values = () => controls.map(control => Number(control.value)).join(',');
        const seed = () => controls.forEach((control, index) => {
            control.value = [45, 30, 0][index];
            control.dispatchEvent(new Event('input', {bubbles: true}));
        });
        const pending = [];
        let requests = 0;
        let wheelEnd;
        const registry = {redrawPlotDrag: (state, variable, preview) => {
            ++requests;
            pending.push({state, variable, preview});
        }, schedulePlotWheelEnd: (state, revision, delay) => wheelEnd = {state, revision, delay}};
        const previous = labDOM.bindEvents({});
        labDOM.bindEvents({...previous, ...registry});
        const complete = (accepted = true) => {
            const request = pending.shift();
            check(!!request, 'Drag completion must belong to an outstanding request');
            labDOM.services(labDOM.call('lab_plot_drag_complete', request.state, Number(accepted)), registry);
        };
        const idle = (end = wheelEnd) => {
            check(end.delay === 300, 'Wheel bursts should settle after 300 ms without further movement');
            labDOM.services(labDOM.call('lab_plot_drag_dispatch', 6, end.revision, end.state), registry);
        };
        const surface = rendered.querySelector('[data-surface]');
        const path = surface.querySelector('[data-series] path');
        const mouse = (target, type, x, y, extras = {}) => target.dispatchEvent(new MouseEvent(
            type, {clientX: x, clientY: y, button: 0, buttons: type === 'mouseup' ? 0 : 1,
                   bubbles: true, cancelable: true, ...extras}));
        const wheel = (target, deltaY, extras = {}) => target.dispatchEvent(new WheelEvent(
            'wheel', {deltaY, bubbles: true, cancelable: true, ...extras}));
        try {
            labDOM.call('lab_plot_drag_install', document, window);
            labDOM.call('lab_plot_drag_install', document, window);
            seed();
            check(mouse(path, 'mousedown', 100, 100, {button: 2, buttons: 2}),
                  'Right mouse button must keep its normal behaviour');
            mouse(window, 'mousemove', 140, 80, {buttons: 2});
            mouse(window, 'mouseup', 140, 80, {button: 2});
            check(values() === '45,30,0' && requests === 0, 'Right dragging must not rotate');
            check(!mouse(path, 'mousedown', 100, 100), 'Left dragging must prevent text selection');
            mouse(window, 'mousemove', 140, 80);
            check(values() === '65,20,0' && requests === 1 && pending[0].variable === 'x' && pending[0].preview,
                  'Upward dragging must decrease elevation and start a live redraw before mouse release');
            for (let i = 0; i < 30; ++i)
                mouse(window, 'mousemove', 142 + i * 2, 80);
            check(values() === '95,20,0' && requests === 1 && pending.length === 1,
                  'Rapid moves must replace the pending camera, not build a request queue');
            check(controls.every(control => document.getElementById(control.id + 'Value').textContent ===
                                           control.value + '°'), 'Dragging must synchronise degree readouts');
            complete();
            check(requests === 2 && pending.length === 1 && rendered.classList.contains('plot-dragging'),
                  'Completion must redraw the newest angle while the button is still held');
            mouse(window, 'mouseup', 200, 80);
            complete();
            check(pending.length === 1 && !pending[0].preview, 'Release must queue one full-detail redraw');
            complete();
            check(pending.length === 0 && !rendered.classList.contains('plot-dragging'),
                  'Release outside the plot must finish with exactly one full-detail redraw');
            seed();
            mouse(path, 'mousedown', 100, 100, {shiftKey: true});
            mouse(window, 'mousemove', 40, 200);
            check(values() === '45,30,330' && pending.length === 1,
                  'Shift-drag must redraw only roll and wrap negative angles');
            mouse(window, 'mouseup', 40, 200);
            complete();
            complete();
            seed();
            mouse(path, 'mousedown', 100, 100);
            mouse(window, 'mousemove', 100, 140);
            check(values() === '45,50,0', 'Downward dragging must increase elevation without changing azimuth or roll');
            mouse(window, 'mousemove', 820, -1000);
            check(values() === '45,5,0', 'Azimuth must wrap and upward dragging clamp at the lower elevation limit');
            mouse(window, 'mousemove', 100, 1000);
            check(values() === '45,85,0', 'Downward dragging must clamp at the upper elevation limit');
            document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape', bubbles: true, cancelable: true}));
            check(values() === '45,30,0' && !rendered.classList.contains('plot-dragging'),
                  'Escape must restore controls and end the gesture');
            complete();
            check(pending.length === 1, 'Cancellation must restore the original view after an in-flight redraw');
            complete();
            mouse(path, 'mousedown', 100, 100);
            mouse(window, 'mousemove', 150, 150);
            window.dispatchEvent(new Event('blur'));
            check(values() === '45,30,0', 'Window blur must cancel the gesture');
            complete();
            complete();
            const beforeClick = requests;
            mouse(path, 'mousedown', 100, 100);
            mouse(window, 'mousemove', 120, 120, {buttons: 0});
            check(!rendered.classList.contains('plot-dragging'), 'A lost release must not leave rotation stuck on');
            mouse(path, 'mousedown', 100, 100);
            mouse(window, 'mouseup', 101, 101);
            check(requests === beforeClick, 'A click or tiny movement must not request another plot');
            mouse(path, 'mousedown', 100, 100);
            const replacement = surface.cloneNode(true);
            surface.replaceWith(replacement);
            mouse(window, 'mouseup', 150, 150);
            replacement.replaceWith(surface);
            check(requests === beforeClick && !rendered.classList.contains('plot-dragging'),
                  'An unrelated replacement result must invalidate an unfinished drag');
            mouse(path, 'mousedown', 100, 100);
            mouse(window, 'mousemove', 150, 150);
            mouse(window, 'mousemove', 160, 160);
            complete(false);
            check(pending.length === 0 && !rendered.classList.contains('plot-dragging'),
                  'A superseded request must discard queued movement without another request');
            seed();
            const card = rendered.closest('.result-card'), zoom = resultZoomIndex(card);
            check(!wheel(path, 100, {shiftKey: true}) && values() === '45,35,0' &&
                  pending.length === 1 && pending[0].preview,
                  'Shift+wheel down must increase elevation and request the same coarse preview as dragging');
            const staleWheelEnd = wheelEnd;
            check(!wheel(path, -1, {shiftKey: true, deltaMode: 1}) && values() === '45,30,0',
                  'Shift+wheel up must decrease elevation, including line-mode events');
            for (let i = 0; i < 30; ++i)
                wheel(path, 1, {altKey: true, deltaMode: 2});
            check(values() === '195,30,0' && pending.length === 1 && resultZoomIndex(card) === zoom,
                  'Alt+wheel down must turn right without zooming or building a request queue');
            idle(staleWheelEnd);
            check(rendered.marsPlotDrag.wheel, 'A stale inactivity callback must not end a newer wheel movement');
            complete();
            check(pending.length === 1 && pending[0].preview, 'Wheel completion must preview the latest camera');
            complete();
            check(pending.length === 0 && rendered.marsPlotDrag.wheel,
                  'A preview finishing before inactivity must keep the wheel gesture alive');
            idle();
            check(pending.length === 1 && !pending[0].preview, 'Wheel inactivity must restore full detail');
            complete();
            idle();
            check(pending.length === 0, 'Repeated inactivity callbacks must not duplicate the final redraw');
            seed();
            for (let i = 0; i < 10; ++i)
                wheel(path, -100, {altKey: true});
            check(values() === '355,30,0', 'Alt+wheel up must turn left and wrap azimuth');
            for (let i = 0; i < 20; ++i)
                wheel(path, -100, {shiftKey: true});
            check(values() === '355,5,0', 'Shift+wheel must respect the lower elevation limit');
            complete();
            complete();
            idle();
            complete();
            const atLimit = requests;
            check(!wheel(path, -100, {shiftKey: true}) && requests === atLimit,
                  'An outward wheel event at the elevation limit must not request another redraw');
            for (let i = 0; i < 20; ++i)
                wheel(path, 100, {shiftKey: true});
            check(values() === '355,85,0', 'Shift+wheel must respect the upper elevation limit');
            check(controls.every(control => document.getElementById(control.id + 'Value').textContent ===
                                           control.value + '°'), 'Wheel rotation must synchronise degree readouts');
            complete(false);
            idle();
            check(pending.length === 0, 'Superseding a wheel redraw must discard queued camera changes');
            for (const modifiers of [{shiftKey: true}, {altKey: true}]) {
                check(wheel(path, 0, {...modifiers, deltaX: 100}) &&
                      wheel(card.querySelector('.card-title'), 100, modifiers) &&
                      wheel(controls[0], 100, modifiers), 'Rotation must require vertical wheel motion over the image');
                check(!wheel(path, -100, {...modifiers, ctrlKey: true}) && pending.length === 0 &&
                      values() === '355,85,0', 'Ctrl+wheel must take zoom precedence over camera rotation');
                setResultZoom(card, zoom);
            }
            check(wheel(path, 100, {shiftKey: true, altKey: true}) && pending.length === 0,
                  'Combined Shift+Alt must not choose an ambiguous rotation axis');
            seed();
            wheel(path, 100, {altKey: true});
            idle();
            check(pending.length === 1 && pending[0].preview,
                  'Inactivity during an in-flight preview must not start a parallel redraw');
            complete();
            check(pending.length === 1 && !pending[0].preview,
                  'An in-flight preview must be followed by full detail after inactivity');
            complete();
            seed();
            wheel(path, 100, {shiftKey: true});
            document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape', bubbles: true, cancelable: true}));
            check(values() === '45,30,0' && !rendered.marsPlotDrag.wheel,
                  'Escape must cancel wheel rotation using the same camera restoration as dragging');
            idle();
            complete();
            check(pending.length === 1 && !pending[0].preview, 'Cancellation must restore full detail');
            complete();
        } finally {
            window.dispatchEvent(new Event('blur'));
            while (pending.length)
                complete(false);
            labDOM.bindEvents(previous);
            controls.forEach((control, index) => {
                control.value = saved[index];
                control.dispatchEvent(new Event('input', {bubbles: true}));
            });
        }
        const initialAzimuth = Number(controls[0].value), initialElevation = Number(controls[1].value);
        const waitForAngle = async angle => {
            const selector = `[data-surface][data-azimuth="${angle.toFixed(3)}"]`;
            const deadline = performance.now() + 15000;
            while (!rendered.querySelector(selector) && performance.now() < deadline)
                await new Promise(resolve => setTimeout(resolve, 25));
            check(!!rendered.querySelector(selector), 'Dragging must redraw native geometry before mouse release');
        };
        const initialMesh = rendered.querySelector('[data-series]').innerHTML;
        mouse(path, 'mousedown', 100, 100);
        mouse(window, 'mousemove', 140, 80);
        await waitForAngle((initialAzimuth + 20) % 360);
        check(Number(rendered.querySelector('[data-surface]').dataset.elevation) ===
              Math.max(5, initialElevation - 10), 'An upward drag must render the decreased elevation');
        check(rendered.classList.contains('plot-dragging') &&
              rendered.querySelector('[data-series]').innerHTML !== initialMesh &&
              rendered.querySelectorAll('[data-series] path').length < 400,
              'The actual surface must move while the left button is held');
        mouse(window, 'mousemove', 180, 120);
        await waitForAngle((initialAzimuth + 40) % 360);
        check(Number(rendered.querySelector('[data-surface]').dataset.elevation) ===
              Math.min(85, initialElevation + 10), 'A downward drag must render the increased elevation');
        mouse(window, 'mouseup', 180, 120);
        check(!rendered.classList.contains('plot-dragging'), 'Live rotation must end on mouse release');
        const fullDeadline = performance.now() + 15000;
        while ((rendered.querySelectorAll('[data-series] path').length < 1000 || rendered.marsPlotDrag.busy) &&
               performance.now() < fullDeadline)
            await new Promise(resolve => setTimeout(resolve, 25));
        check(rendered.querySelectorAll('[data-series] path').length > 1000 && !rendered.marsPlotDrag.busy,
              'Release must restore the full native mesh, even without a final movement');
        for (const [modifier, axis] of [['shiftKey', 'elevation'], ['altKey', 'azimuth']]) {
            const before = rendered.querySelector('[data-surface]');
            const angle = Number(before.dataset[axis]) - 5;
            wheel(before.querySelector('[data-series] path'), -100, {[modifier]: true});
            const deadline = performance.now() + 15000;
            let sawPreview = false;
            while ((rendered.marsPlotDrag.busy || rendered.marsPlotDrag.wheel) && performance.now() < deadline) {
                sawPreview ||= rendered.querySelectorAll('[data-series] path').length === 2 * 12 * 12;
                await new Promise(resolve => setTimeout(resolve, 25));
            }
            const after = rendered.querySelector('[data-surface]');
            check(sawPreview && !rendered.marsPlotDrag.busy && !rendered.marsPlotDrag.wheel &&
                  after !== before && Number(after.dataset[axis]) === angle &&
                  rendered.querySelectorAll('[data-series] path').length === 2 * 69 * 69,
                  'Both wheel rotations must show a coarse native preview then restore the full 70-by-70 mesh');
        }
    }
    function checkPlotWheelZoom() {
        const card = rendered.closest('.result-card'), svg = rendered.querySelector('svg');
        const path = svg.querySelector('[data-series] path'), saved = resultZoomIndex(card);
        const initial = labWire.exports().lab_view_zoom_index(NaN);
        const wheel = (target, deltaY, deltaX = 0, modifiers = {ctrlKey: true}) =>
            target.dispatchEvent(new WheelEvent('wheel', {deltaY, deltaX, bubbles: true, cancelable: true, ...modifiers}));
        try {
            setResultZoom(card, initial);
            const unhandled = [{}, {metaKey: true}];
            if (!rendered.querySelector('[data-surface]'))
                unhandled.push({shiftKey: true}, {altKey: true});
            for (const modifiers of unhandled) {
                check(wheel(path, -100, 0, modifiers) && wheel(svg, 100, 0, modifiers) &&
                      wheel(card.querySelector('.card-title'), -100, 0, modifiers) &&
                      resultZoomIndex(card) === initial,
                      'Wheel events without an applicable plot modifier must remain unhandled and must not change zoom');
            }
            const width = svg.getBoundingClientRect().width;
            check(!wheel(path, -100) && resultZoomIndex(card) === initial + 1 &&
                  svg.getBoundingClientRect().width > width,
                  'Ctrl+wheel over a plotted path must zoom in and prevent page scrolling');
            const zoomedWidth = svg.getBoundingClientRect().width;
            check(!wheel(svg, 100) && resultZoomIndex(card) === initial &&
                  svg.getBoundingClientRect().width < zoomedWidth,
                  'Ctrl+wheel over the plot background must zoom out');
            check(wheel(svg, 0) && wheel(svg, 0, 100) && resultZoomIndex(card) === initial,
                  'Zero or horizontal-only wheel motion must not change plot zoom');
            check(wheel(card.querySelector('.card-title'), -100) &&
                  wheel(document.getElementById('plotRoll'), -100) && resultZoomIndex(card) === initial,
                  'Wheel outside the plot image must retain normal scrolling and control behaviour');
            const maximum = labWire.exports().lab_view_zoom_count() - 1;
            for (const [limit, delta] of [[0, 100], [maximum, -100]]) {
                setResultZoom(card, limit);
                check(!wheel(svg, delta) && resultZoomIndex(card) === limit,
                      'Wheel zoom must respect the card zoom limits');
            }
            check(rendered.querySelector('[data-series] path') === path,
                  'Wheel zoom must reuse the SVG without resampling or replacing its geometry');
        } finally {
            setResultZoom(card, saved);
        }
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
            try {
                Storage.prototype.getItem = function(key) {
                    return key === `${storageKey}.protobuf` ? 'A'.repeat(5592409) : getStoredItem.call(this, key);
                };
                check(loadLabLocalState(storageKey) === null, 'oversized fallback was accepted');
            } finally {
                Storage.prototype.getItem = getStoredItem;
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
        const maximum = labWire.encode({x: 'a'.repeat(4194275)});
        check(maximum.length === 4194304, 'Exact wire limit was rejected');
        check(labWire.decode(maximum).x.length === 4194275, 'Maximum message did not round trip');
        check(rejects(() => labWire.encode({x: 'a'.repeat(4194276)})), 'Wire limit plus one accepted');
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
        check(rejects(() => labWire.encode({text: '\ud800'})), 'Invalid surrogate accepted');
        check(rejects(() => labWire.encode({text: 'x'.repeat(4194304)})), 'Oversized message accepted');
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
                window.fetch = async () => new Response(new Uint8Array(4194305), {headers: {'Content-Type': type}});
                const oversized = await labFetch('/fixture');
                let rejected = false;
                try {
                    await oversized.labData();
                } catch (error) {
                    rejected = error.message.includes(type === 'application/json' ? 'Expected a Protobuf' : '4 MiB');
                }
                check(rejected, `Oversized ${type} response was not bounded`);
            }
        } finally {
            window.fetch = originalFetch;
        }
    }
    async function report(message) {
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
            for (const [viewport, card] of [[1920, 860], [1920, 1700], [390, 390]]) {
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
                    if (icon.classList.contains('almanac-body-symbol'))
                        check(icon.closest('tr').getBoundingClientRect().height < 65,
                              'Body symbols inflated the table row height');
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
            setExpressionEditor('asin(x)');
            await evaluateExpression();
            document.getElementById('plotLower').value = '';
            document.getElementById('plotUpper').value = '';
            await takePlot('x');
            check(!rendered.classList.contains('error') && !!rendered.querySelector('[data-series]') &&
                  document.getElementById('plotLower').value === '' &&
                  document.getElementById('plotUpper').value === '',
                  'Blank bounds must plot the native real domain without replacing Automatic controls with numbers');
            setExpressionEditor('sin(x)');
            await evaluateExpression();
            document.getElementById('plotLower').value = '-@pi';
            document.getElementById('plotUpper').value = '@pi';
            await takePlot('x');
            check(!rendered.classList.contains('error'), 'Native plot request failed');
            check(!!rendered.querySelector('svg [data-series="0"]'), 'Plot SVG was not displayed');
            check(document.getElementById('renderedTitle').textContent === 'Plot', 'Plot card title missing');
            check(!!rendered.querySelector('svg [data-plot-title] use') && rendered.dataset.displayTex.includes('\\sin'),
                  'Plot formula title must be rendered as TeX glyphs in the same SVG');
            check(!rendered.textContent.includes('Function plot'), 'Generic plot title must be removed');
            check(parsed.dataset.fullText.includes('sin'), 'Plot lost its source expression');
            const plotCard = rendered.closest('.result-card');
            const plotExpand = plotCard.querySelector('[data-expand-card]');
            const nextLayout = () => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
            setResultZoom(plotCard, 3);
            await nextLayout();
            checkPlotWheelZoom();
            await checkPlotChoices();
            checkPlotControls(false);
            const curveDragTarget = rendered.querySelector('[data-series] path');
            check(curveDragTarget.dispatchEvent(new MouseEvent('mousedown',
                  {button: 0, buttons: 1, bubbles: true, cancelable: true})) &&
                  !rendered.classList.contains('plot-dragging'), 'Curve plots must not enable surface rotation');
            const formulaBox = rendered.querySelector('[data-plot-title]').getBoundingClientRect();
            const axesBox = rendered.querySelector('[data-plot-chart] [data-plot-axes]').getBoundingClientRect();
            check(formulaBox.width > 0 && formulaBox.height > 0 && formulaBox.bottom < axesBox.top,
                  'Rendered formula title must sit above the plot axes without overlap');
            const figure = rendered.querySelector('[data-plot-figure]');
            const curve = figure.querySelector('[data-series] path');
            check(getComputedStyle(figure).filter === 'none' && getComputedStyle(curve).stroke === 'rgb(120, 216, 203)',
                  'Plot curves must retain their teal colour, without the monochrome formula filter');
            check(getComputedStyle(figure.querySelector('[data-plot-title] use')).fill === 'rgb(230, 199, 120)',
                  'Plot formula title must retain its gold colour');
            check(!figure.querySelector('rect') && !rendered.textContent.includes('Vertical range clipped'),
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
            const smallPlotWidth = rendered.querySelector('svg').getBoundingClientRect().width;
            plotExpand.click();
            await nextLayout();
            const expandedPanel = document.getElementById('resultWorkspacePanel').getBoundingClientRect();
            const workspaceBar = document.querySelector('.lab-topbar').getBoundingClientRect();
            check(getComputedStyle(document.getElementById('workspacePanel')).display === 'none',
                  'Expanded plot must hide the editor');
            check(Math.abs(expandedPanel.left - workspaceBar.left) < 2 &&
                      Math.abs(expandedPanel.right - workspaceBar.right) < 2,
                  'Expanded plot must span both workspace columns');
            const largePlotWidth = rendered.querySelector('svg').getBoundingClientRect().width;
            const plotStyle = getComputedStyle(rendered);
            const availablePlotWidth = rendered.clientWidth - parseFloat(plotStyle.paddingLeft) -
                                       parseFloat(plotStyle.paddingRight);
            check(largePlotWidth > smallPlotWidth && Math.abs(largePlotWidth - availablePlotWidth) < 2,
                  'Expanded plot SVG must grow to fit the available width at 100%');
            await checkPlotChoices();
            plotExpand.click();
            await nextLayout();
            const collapsedWidth = rendered.querySelector('svg').getBoundingClientRect().width;
            const collapsedAvailable = rendered.clientWidth - parseFloat(plotStyle.paddingLeft) -
                                       parseFloat(plotStyle.paddingRight);
            check(getComputedStyle(document.getElementById('workspacePanel')).display !== 'none' &&
                      collapsedWidth < largePlotWidth && Math.abs(collapsedWidth - collapsedAvailable) < 2 &&
                      rendered.scrollWidth <= rendered.clientWidth + 1,
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
            check(rightPaneTitle.textContent === plotHeading && renderedTitle.textContent === 'Plot' &&
                      !!rendered.querySelector('svg [data-series="0"]'),
                  'Returning to Expression restores its plot and matching headings');
            document.getElementById('plotScale').value = 'manual';
            document.getElementById('plotYLower').value = '-1/2';
            document.getElementById('plotYUpper').value = '1/2';
            await takePlot('x');
            check(!rendered.classList.contains('error'), 'Manual plot limits failed');
            check(!rendered.textContent.includes('Vertical range clipped'), 'Manual clipping must be silent');
            document.getElementById('plotScale').value = 'all';
            await takePlot('x');
            check(!rendered.classList.contains('error'), 'Fit all failed');
            check(!rendered.textContent.includes('Vertical range clipped'), 'Fit all retained manual limits');
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
                check(!rendered.classList.contains('error'), 'High-precision tangent plot failed');
                const paths = rendered.querySelectorAll('svg [data-series="0"] path');
                check(paths.length > 0, 'Tangent plot contains no visible curve');
                const frame = rendered.querySelector('[data-plot-chart]');
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
            check(!rendered.classList.contains('error') && !!rendered.querySelector('svg [data-series]'),
                  'Reciprocal series must plot its convergent portion across -5 to 5');
            check(rendered.dataset.displayTex.includes('\\sum'), 'Reciprocal series must retain its sigma title');
            document.getElementById('plotUpper').value = '1';
            await takePlot('x');
            check(rendered.classList.contains('error') && rendered.textContent.includes('No finite real points') &&
                  !rendered.textContent.includes('NAN') && !rendered.textContent.includes('binding variable'),
                  'Wholly divergent range must show a clean calculation diagnostic');
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
            check(!rendered.classList.contains('error') && !!rendered.querySelector('[data-surface]'),
                  'Automatic two-variable plot must display a surface');
            const plotControls = document.getElementById('plotRange');
            check(plotControls.closest('.result-card') === rendered.closest('.result-card') &&
                  !document.getElementById('workspacePanel').contains(plotControls) &&
                  getComputedStyle(plotControls).display !== 'none',
                  'Plot controls must live inside the visible plot card');
            for (const id of ['plotAzimuth', 'plotElevation', 'plotRoll'])
                check(document.getElementById(id).type === 'range', '3-D camera controls must be sliders');
            const savedCardWidth = plotCard.style.width;
            plotCard.style.width = '360px';
            check(getComputedStyle(plotControls).gridTemplateColumns.split(' ').length === 1 &&
                  plotControls.scrollWidth <= plotControls.clientWidth + 1 &&
                  document.getElementById('plotAzimuth').getBoundingClientRect().width > 60,
                  'Narrow plot cards must stack controls and retain a usable slider without overflow');
            checkPlotRangeAlignment();
            await checkPlotChoices();
            plotCard.style.width = savedCardWidth;
            checkPlotControls(true);
            const facets = [...rendered.querySelectorAll('[data-surface] [data-series] path')];
            check(facets.length === 2 * 69 * 69 && new Set(facets.map(path => path.getAttribute('fill'))).size > 4,
                  'Full surface must contain the shaded 70-by-70 mesh');
            check(!!rendered.querySelector('[data-plot-title] use'), 'Surface must retain its rendered formula title');
            checkPlotLabelSpacing();
            for (const [axis, name] of [[0, 'x'], [1, 'y'], [2, 'u']]) {
                check(!!rendered.querySelector(`[data-axis-arrow="${axis}"]`) &&
                      rendered.querySelector(`[data-axis-name="${axis}"]`).textContent.trim() === name,
                      'Surface axes must identify their variables at increasing-value arrowheads');
            }
            for (const axis of [0, 1, 2])
                check(rendered.querySelectorAll(`[data-plot-tick="${axis}"] path`).length >= 3,
                      'Every surface axis needs visible tick strokes');
            const initialMesh = rendered.querySelector('[data-series]').innerHTML;
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
            check(rendered.querySelector('[data-series]').innerHTML === initialMesh,
                  'Slider input alone must not launch repeated native requests');
            document.getElementById('plotRoll').dispatchEvent(new Event('change', {bubbles: true}));
            const rotationDeadline = performance.now() + 15000;
            while (!rendered.querySelector('[data-surface][data-azimuth="120.000"][data-elevation="40.000"]') &&
                   performance.now() < rotationDeadline)
                await new Promise(resolve => setTimeout(resolve, 25));
            check(!!rendered.querySelector('[data-surface][data-azimuth="120.000"][data-elevation="40.000"]'),
                  'Changing a camera control must redraw the current surface automatically');
            check(!!rendered.querySelector('[data-surface][data-roll="90.000"]'),
                  'Roll must reach the native camera');
            check(rendered.querySelector('[data-series]').innerHTML !== initialMesh,
                  'Rotation must change native projected geometry');
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
            const surfaceExpand = rendered.closest('.result-card').querySelector('[data-expand-card]');
            surfaceExpand.click();
            check(labWorkspace.classList.contains('result-card-expanded'), 'Surface must expand across the workspace');
            check(plotControls.getBoundingClientRect().width > 0 &&
                  rendered.closest('.expanded-card').contains(plotControls),
                  'Expanded plot must retain its controls');
            checkPlotRangeAlignment();
            const secondHint = document.querySelector('#plotSecondGroup .plot-hint');
            const savedHintWidth = secondHint.style.maxWidth;
            try {
                secondHint.style.maxWidth = '12ch';
                checkPlotRangeAlignment();
            } finally {
                secondHint.style.maxWidth = savedHintWidth;
            }
            checkPlotWheelZoom();
            await checkPlotDrag();
            document.getElementById('plotAzimuth').value = '75';
            document.getElementById('plotAzimuth').dispatchEvent(new Event('input', {bubbles: true}));
            document.getElementById('plotUpdate').click();
            const updateDeadline = performance.now() + 15000;
            while (!rendered.querySelector('[data-surface][data-azimuth="75.000"]') && performance.now() < updateDeadline)
                await new Promise(resolve => setTimeout(resolve, 25));
            check(!!rendered.querySelector('[data-surface][data-azimuth="75.000"]') &&
                  labWorkspace.classList.contains('result-card-expanded'),
                  'Update plot must work without leaving the expanded card');
            surfaceExpand.click();
            document.getElementById('plotLower').value = '-10';
            document.getElementById('plotAzimuth').value = '45';
            document.getElementById('plotElevation').value = '30';
            document.getElementById('plotRoll').value = '0';
            document.getElementById('plotUpper').value = '10';
            document.getElementById('plotSecondLower').value = '-10';
            document.getElementById('plotSecondUpper').value = '10';
            setExpressionEditor('3+4');
            await evaluateExpression();
            check(getComputedStyle(plotControls).display === 'none', 'Ordinary results must hide plot controls');
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
