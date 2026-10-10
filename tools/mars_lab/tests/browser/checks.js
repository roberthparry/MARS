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
                '<body><div id="fixture"></div></body></html>';
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
                target.querySelectorAll('.almanac-grid-table:not(.almanac-event-table) tbody tr').length === 2,
                'Native all-body rows were not used');
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
                    target.querySelector('.body-name').textContent === 'Sun' && !target.querySelector('.visible-cell'),
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

            for (const mode
                     of ['matrix', 'integrator', 'datetime', 'almanac', 'equation', 'diffequation', 'expression']) {
                setMode(mode);
                restoreModeEditor(mode);
                syncModeUI();
                check(currentMode() === mode, `Mode ${mode} unavailable`);
                check(
                    document.querySelector(`[data-mode="${mode}"]`).classList.contains('active'),
                    `Mode ${mode} tab inactive`);
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
                            2,
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
