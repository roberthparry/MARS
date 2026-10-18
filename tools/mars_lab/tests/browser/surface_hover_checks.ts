/**
 * @file surface_hover_checks.ts
 * @brief Native curve picking and browser inspection presentation and lifetime regressions.
 *
 * Uses independent endpoint pairs and an explicit camera frame, exercising the real
 * native bridge without evaluating or interpreting mathematical expressions. Real chart
 * events cover canvas and settled SVG targets, cursor feedback and visible overlay bounds.
 */
interface Window {
    /** @brief Check native projection, clipping and handle bounds. */
    checkLabSurfaceHover(): Promise<boolean>;

    /** @brief Check rendered curve event routing, overlay visibility and inspector ownership. */
    checkLabSurfaceHoverDOM(chart: SVGSVGElement): Promise<void>;

    /** @brief Inspect actual visible SVG strokes without preparing or redrawing the view in the fixture. */
    checkLabVisibleCurveHover(chart: SVGSVGElement): Promise<void>;
}

/** @brief Endpoint records supplied to native hover picking, including malformed empty records. */
interface SurfaceHoverFixturePrimitive {
    points: MarsSurfacePoint3[];
}

/** @brief Pick browser-rendered strokes, independently of the retained projection used by hit testing. */
window.checkLabVisibleCurveHover = async function checkLabVisibleCurveHover(chart: SVGSVGElement): Promise<void> {
    /** @brief Admit only rendered paths with a measurable stroke. */
    function visiblePath(path: SVGPathElement): boolean {
        return getComputedStyle(path).visibility === 'visible' && path.getTotalLength() > 0;
    }

    chart.scrollIntoView({block: 'center', inline: 'nearest'});
    await LabTestBrowser.wait(250);
    const paths = [...chart.querySelectorAll<SVGPathElement>('[data-series] path')].filter(visiblePath);
    const host = chart.closest<HTMLElement>('.rendered')!;
    let inspected = 0;
    // At most six paths and three positions per path cover both logarithmic branches.
    const stride = Math.max(1, Math.ceil(paths.length / 6));
    for (let i = 0; i < paths.length; i += stride) {
        const path = paths[i];
        for (const fraction of [0.25, 0.5, 0.75]) {
            const screen = path.getScreenCTM();
            if (!screen)
                continue;
            const point = path.getPointAtLength(path.getTotalLength() * fraction).matrixTransform(screen);
            const target = document.elementFromPoint(point.x, point.y);
            if (!target || !chart.closest('[data-plot-figure]')!.contains(target))
                continue;
            target.dispatchEvent(new PointerEvent('pointermove', {bubbles: true, pointerType: 'mouse',
                pointerId: 94, buttons: 0, clientX: point.x, clientY: point.y}));
            await LabTestBrowser.paint();
            const marker = host.querySelector<HTMLElement>('.surface-pointer-marker');
            const tip = host.querySelector<HTMLElement>('.surface-pointer-tip');
            const box = marker?.getBoundingClientRect();
            const cursor = getComputedStyle(target).cursor;
            if (!box || !tip || cursor !== 'crosshair' ||
                Math.hypot(box.left + box.width / 2 - point.x, box.top + box.height / 2 - point.y) > 2) {
                const plot = document.getElementById('plotRendered') as MarsSurfaceHoverPlot;
                throw new Error('Visible curve hover: ' + JSON.stringify({point, box, cursor,
                    renderer: chart.dataset.localRenderer, target: target.tagName, tip: tip?.textContent,
                    gesture: plot.marsPlotDrag, held: labSurfaceVolume.interacting,
                    volume: labSurfacePreview.snapshot()?.volume}));
            }
            ++inspected;
        }
    }
    chart.dispatchEvent(new PointerEvent('pointerleave'));
    if (!inspected)
        throw new Error('Visible curve hover: no visible strokes inspected');
};

/** @brief Exercise real curve pointer events and ensure inspection never enters exported SVG. */
window.checkLabSurfaceHoverDOM = async function checkLabSurfaceHoverDOM(chart: SVGSVGElement): Promise<void> {
    /** @brief Report a failed live curve inspection assertion. */
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('Surface hover DOM: ' + message);
    }

    const settle = LabTestBrowser.paint;
    const view = labSurfacePreview.prepare(chart);
    check(view, 'real curve has a retained view');
    if (!view) return;
    chart.scrollIntoView({block: 'center', inline: 'nearest'});
    labSurfacePreview.draw(chart, view.angles);
    // Initial layout and scrolling can queue resize redraws after the next animation frame.
    // Start the pointer fixture only once that layout has reached its idle vector state.
    const layoutDeadline = performance.now() + 3000;
    while ((!view.settled || view.frame) && performance.now() < layoutDeadline) await settle();
    check(view.settled && !view.frame, 'initial curve layout reaches an idle state');
    labSurfacePreview.draw(chart, view.angles);
    await settle();
    const frame = view.framePlan, screen = chart.getScreenCTM();
    check(frame && screen, 'retained projection is ready');
    if (!frame || !screen) return;
    const figure = chart.closest<SVGSVGElement>('svg[data-plot-figure]')!;
    check(figure, 'curve belongs to a rendered figure');
    let location: DOMPoint | undefined;
    for (const primitive of view.primitives) {
        const projected = labDOM.call<MarsSurfacePolygon>('lab_surface_polygon', frame, primitive.points);
        if (projected.points.length !== 2) continue;
        const [a, b] = projected.points;
        const x = (a[0] + b[0]) / 2, y = (a[1] + b[1]) / 2;
        if (x < 12 || y < 12 || x > view.width - 12 || y > view.height - 12) continue;
        const candidate = new DOMPoint(x, y).matrixTransform(screen);
        if (candidate.x < 12 || candidate.y < 12 || candidate.x > innerWidth - 12 || candidate.y > innerHeight - 12) continue;
        const target = document.elementFromPoint(candidate.x, candidate.y);
        if (!target || (!chart.contains(target) && target !== figure)) continue;
        location = candidate;
        break;
    }
    check(location, 'real curve contains a visible inspection point');
    if (!location) return;
    const point = location;
    let lastTarget: Element | null = null;
    let showOrdinal = 0;
    /** @brief Dispatch one pointer event at the retained visible sample. */
    function pointer(type: string, buttons = 0, target: Element | null = null): void {
        target ??= document.elementFromPoint(point.x, point.y);
        lastTarget = target;
        check(target && (chart.contains(target) || target === figure), 'browser hit target belongs to the rendered curve');
        target?.dispatchEvent(new PointerEvent(type, {bubbles: true, pointerId: 92,
            pointerType: 'mouse', isPrimary: true, buttons, button: 0,
            clientX: point.x, clientY: point.y}));
    }

    /** @brief Check that both transient inspection nodes are absent. */
    function hidden(): boolean {
        return !view!.host.querySelector('.surface-pointer-marker, .surface-pointer-tip');
    }

    /** @brief Compare the visible marker centre with the current native screen-space hit. */
    function markerAligned(): boolean {
        const marker = view!.host.querySelector<HTMLElement>('.surface-pointer-marker');
        const currentScreen = chart.getScreenCTM();
        if (!marker || !currentScreen) return false;
        const hit = labDOM.call<MarsSurfaceHoverHit | null>('lab_surface_hover', {chart, primitives: view!.primitives},
            view!.framePlan, currentScreen, point.x, point.y, null);
        const bounds = marker.getBoundingClientRect();
        return !!hit && Math.abs(bounds.left + bounds.width / 2 - hit.x) < 1 &&
            Math.abs(bounds.top + bounds.height / 2 - hit.y) < 1;
    }

    /** @brief Show inspection and check cursor, native alignment, stacking and host bounds. */
    const show = async function show(stage: string): Promise<void> {
        ++showOrdinal;
        const beforeFrame = view.framePlan;
        pointer('pointermove');
        await settle();
        const hoverDeadline = performance.now() + 2000;
        // A resize can invalidate an already visible marker; wait for the queued re-pick,
        // without supplying another pointer event that could conceal a lost-hover defect.
        while (!markerAligned() && performance.now() < hoverDeadline)
            await settle();
        const marker = view.host.querySelector<HTMLElement>('.surface-pointer-marker');
        const tip = view.host.querySelector<HTMLElement>('.surface-pointer-tip');
        if (!marker || !tip || !tip.textContent.includes('Approximate sample') ||
            !tip.textContent.includes('Re ≈') || !tip.textContent.includes('Im ≈')) {
            const currentScreen = chart.getScreenCTM();
            const plot = document.getElementById('plotRendered') as MarsSurfaceHoverPlot;
            const drag = plot.marsPlotDrag;
            const hit = currentScreen && view.framePlan ? labDOM.call<MarsSurfaceHoverHit | null>('lab_surface_hover',
                {chart, primitives: view.primitives}, view.framePlan, currentScreen, point.x, point.y, null) : null;
            check(false, 'pointer shows marker and formatted sample ' + JSON.stringify({
                show: showOrdinal, stage, marker: !!marker, tip: !!tip, text: tip?.textContent,
                connected: chart.isConnected, renderer: chart.dataset.localRenderer,
                hover: chart.dataset.surfaceHover, settled: !!view.settled,
                target: lastTarget?.tagName, targetId: lastTarget?.id,
                targetInside: !!lastTarget && chart.contains(lastTarget),
                targetIsFigure: lastTarget === figure, point: {x: point.x, y: point.y}, hit,
                initialScreen: [screen.a, screen.b, screen.c, screen.d, screen.e, screen.f],
                currentScreen: currentScreen && [currentScreen.a, currentScreen.b, currentScreen.c,
                    currentScreen.d, currentScreen.e, currentScreen.f],
                frameChanged: beforeFrame !== view.framePlan, pendingFrame: view.frame,
                host: view.host.getBoundingClientRect(), chart: chart.getBoundingClientRect(),
                dragging: plot.classList.contains('plot-dragging'),
                gesture: {active: drag?.active, busy: drag?.busy, wheel: drag?.wheel, slider: drag?.slider},
                interacting: labSurfaceVolume.interacting
            }));
        }
        check(getComputedStyle(chart).cursor === 'crosshair' && getComputedStyle(figure).cursor === 'crosshair',
            'a native hit changes both possible SVG event targets to the inspection cursor');
        if (marker) {
            const bounds = marker.getBoundingClientRect(), style = getComputedStyle(marker);
            const hit = labDOM.call<MarsSurfaceHoverHit>('lab_surface_hover', {chart, primitives: view.primitives},
                view.framePlan, chart.getScreenCTM(), point.x, point.y, null);
            check(bounds.width >= 9 && bounds.height >= 9 && style.position === 'absolute' &&
                style.visibility === 'visible' && style.display !== 'none', 'marker has a visible, non-zero CSS box');
            check(hit && Math.abs(bounds.left + bounds.width / 2 - hit.x) < 1 &&
                Math.abs(bounds.top + bounds.height / 2 - hit.y) < 1,
                'marker is centred on the native screen-space hit ' + JSON.stringify({stage, show: showOrdinal,
                    bounds, hit, left: marker.style.left, top: marker.style.top,
                    host: view.host.getBoundingClientRect(), scroll: [view.host.scrollLeft, view.host.scrollTop],
                    offsetParent: (marker.offsetParent as Element | null)?.getBoundingClientRect(),
                    margin: style.margin, transform: style.transform, text: tip?.textContent,
                    sameView: labSurfacePreview.prepare(chart) === view,
                    renderer: chart.dataset.localRenderer, pendingFrame: view.frame,
                    client: [view.host.clientWidth, view.host.clientHeight],
                    overflow: [view.host.scrollWidth, view.host.scrollHeight]}));
            check(Number(style.zIndex) > Number(getComputedStyle(view.overlay).zIndex) &&
                Number(style.zIndex) > Number(getComputedStyle(tip!).zIndex), 'marker paints above canvas and tooltip layers');
            check(style.pointerEvents === 'none' && getComputedStyle(tip!).pointerEvents === 'none',
                'inspection overlays leave browser hit testing on the chart');
        }
        if (tip) {
            const bounds = tip.getBoundingClientRect(), host = view.host.getBoundingClientRect();
            check(bounds.right <= host.right + 1 && bounds.left >= host.left - 1, 'tooltip stays horizontally inside its host');
            check(bounds.bottom <= host.bottom + 1 && bounds.top >= host.top - 1, 'tooltip stays vertically inside its host');
        }
    };
    try {
        check(getComputedStyle(chart).cursor === 'grab', 'idle plot offers dragging');
        pointer('pointermove', 0, figure);
        await settle();
        check(!hidden(), 'canvas pass-through events on the outer figure reach native picking');
        await show('initial browser target');
        const marker = view.host.querySelector('.surface-pointer-marker');
        pointer('pointermove');
        check(view.host.querySelector('.surface-pointer-marker') === marker, 'movement keeps the marker until the next sample');
        await settle();
        check(view.host.querySelector('.surface-pointer-marker') === marker, 'successive hits reuse the marker');
        const settledDeadline = performance.now() + 2000;
        while (!view.settled && performance.now() < settledDeadline) await settle();
        check(view.settled && view.overlay.style.display === 'none', 'curve reaches its settled SVG rendering');
        await show('settled SVG');
        chart.dispatchEvent(new PointerEvent('pointerout', {bubbles: true, relatedTarget: view.settled}));
        check(!hidden(), 'moving into settled SVG descendants preserves inspection');
        const copied = WorksheetResults.clipboard.copyTextForTarget('plot');
        check(!copied.includes('surface-pointer-') && !copied.includes('Approximate sample'), 'copy excludes transient inspection');
        await show('after clipboard export');
        pointer('pointerleave');
        check(hidden() && getComputedStyle(chart).cursor === 'grab', 'pointerleave hides inspection and restores grab');
        // A replacement SVG can arrive beneath a stationary mouse: no move event follows its entry.
        pointer('pointerover');
        await settle();
        check(!hidden() && chart.dataset.surfaceHover === 'true',
            'entering a replacement curve restores marker and inspection cursor without pointer movement');
        await show('after pointerleave');
        pointer('pointerdown', 1);
        check(hidden() && getComputedStyle(chart).cursor === 'grabbing', 'drag start hides inspection and retains grabbing');
        pointer('pointerup');
        await settle();
        await show('after pointerup');
        await checkLabSurfaceHoverLifetime(chart, view, point);
        await show('after ownership checks');
        chart.dispatchEvent(new WheelEvent('wheel', {bubbles: true, deltaY: 0}));
        check(hidden() && chart.dataset.surfaceHover === undefined, 'wheel hides inspection and releases the hit cursor');
    } finally {
        pointer('pointercancel');
        pointer('pointerleave');
    }
};

/** @brief Check scheduling, gesture ownership and teardown using an existing rendered view and isolated events. */
async function checkLabSurfaceHoverLifetime(chart: SVGSVGElement, view: MarsSurfaceView, point: DOMPoint): Promise<void> {
    /** @brief Report a failed inspector lifetime assertion. */
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('Surface hover lifetime: ' + message);
    }

    const settle = LabTestBrowser.paint;
    const sample = labDOM.call<MarsSurfaceHoverHit>('lab_surface_hover', {chart, primitives: view.primitives},
        view.framePlan, chart.getScreenCTM(), point.x, point.y, null);
    check(sample, 'real sample is available for presentation checks');
    let hit: MarsSurfaceHoverHit | null = sample;
    let calls = 0, available = true;
    const native: MarsSurfaceHoverNative = {
        /** @brief Count native picks while returning the scenario's current sample. */
        call(name: string): unknown {
            check(name === 'lab_surface_hover', 'inspection reuses the retained projection');
            ++calls;
            return hit;
        }
    };
    const events = new EventTarget(), otherEvents = new EventTarget();
    /** @brief Supply the retained view only while this fixture owns an available chart. */
    function retained(selected: SVGSVGElement): MarsSurfaceView | null {
        return available && selected === chart ? view : null;
    }

    const first = new MarsSurfaceHover(retained, native, events);
    const second = new MarsSurfaceHover(retained, native, otherEvents);
    const plot = document.getElementById('plotRendered') as MarsSurfaceHoverPlot;
    const savedDrag = plot.marsPlotDrag;
    const gesture: MarsSurfaceGesture = {x: 0, y: 0, touchX: 0, touchY: 0};
    const foreign = document.createElement('div');
    foreign.className = 'surface-pointer-marker';
    foreign.hidden = true;
    /** @brief Deliver a non-bubbling sample to one inspector without the page's inspector. */
    function move(owner = first, target: Element = chart, buttons = 0, pointerType = 'mouse'): void {
        // Non-bubbling samples keep this lifetime fixture separate from the page's inspector.
        const event = new PointerEvent('pointermove', {clientX: point.x, clientY: point.y, buttons, pointerType});
        target.dispatchEvent(event);
        owner.inspect(event);
    }

    /** @brief Read the chart's current inspection ownership marker. */
    function active(): boolean {
        return chart.dataset.surfaceHover === 'true';
    }
    try {
        move();
        move();
        await settle();
        check(calls === 1 && active(), 'queued moves share one native pick');
        const marker = view.host.querySelector('.surface-pointer-marker');
        const tip = view.host.querySelector('.surface-pointer-tip');
        check(marker && tip, 'inspector owns both overlay nodes');
        view.host.appendChild(foreign);
        events.dispatchEvent(new PointerEvent('pointerout', {relatedTarget: chart.firstElementChild}));
        check(marker?.isConnected && active(), 'internal pointer transitions keep the owned overlays');
        move();
        check(marker?.isConnected, 'pending work does not remove the marker');
        await settle();
        check(marker?.isConnected && tip?.isConnected, 'successive samples retain both nodes');

        const box = view.host.getBoundingClientRect();
        hit = {...sample, x: box.left + view.host.clientLeft + view.host.clientWidth - 6,
            y: box.top + view.host.clientTop + view.host.clientHeight - 6};
        move();
        await settle();
        const markerBox = marker!.getBoundingClientRect(), tipBox = tip!.getBoundingClientRect();
        check(tipBox.right < markerBox.left || tipBox.bottom < markerBox.top,
            'edge tooltip moves to the opposite side of its marker');
        hit = null;
        move();
        await settle();
        check(!active() && !marker?.isConnected && !tip?.isConnected && foreign.isConnected &&
            getComputedStyle(chart).cursor === 'grab', 'empty plot restores grab and removes only owned overlays');
        hit = sample;

        move();
        await settle();
        const beforeRefresh = calls;
        first.invalidate(chart);
        check(!active() && !view.host.querySelector('.surface-pointer-tip'), 'redraw removes stale inspection feedback');
        first.refresh(chart);
        first.refresh(chart);
        await settle();
        check(calls === beforeRefresh + 1 && active(), 'successful redraw re-picks the stationary pointer once');

        plot.marsPlotDrag = gesture;
        for (const flag of ['active', 'wheel', 'busy', 'slider'] as const) {
            const before = calls;
            move();
            gesture[flag] = true;
            await settle();
            check(calls === before && !active(), 'queued work yields to ancestor gesture: ' + flag);
            move();
            await settle();
            check(calls === before && !active(), 'new work yields to ancestor gesture: ' + flag);
            gesture[flag] = false;
        }
        plot.marsPlotDrag = savedDrag;

        for (const name of ['pointerdown', 'pointercancel', 'wheel', 'scroll', 'blur', 'pagehide', 'pointerleave']) {
            move();
            await settle();
            check(active(), 'inspection resumes before ' + name);
            move();
            const before = calls;
            events.dispatchEvent(new Event(name));
            await settle();
            check(!active() && calls === before, name + ' removes feedback and cancels pending work');
            move();
            await settle();
            first.invalidate(chart);
            const beforeRefresh = calls;
            events.dispatchEvent(new Event(name));
            first.refresh(chart);
            await settle();
            check(!active() && calls === beforeRefresh, name + ' prevents redraw from reviving the previous pointer');
        }
        move(first, chart, 1);
        move(first, chart, 0, 'touch');
        const beforeUnsupported = calls;
        await settle();
        check(calls === beforeUnsupported && !active(), 'pressed buttons and touch never inspect');
        available = false;
        move();
        await settle();
        check(calls === beforeUnsupported && !active(), 'an unavailable retained view cannot show feedback');
        available = true;

        const detached = chart.cloneNode(false) as SVGSVGElement;
        view.host.appendChild(detached);
        move(first, detached);
        detached.remove();
        await settle();
        check(calls === beforeUnsupported && !active(), 'detached charts cancel queued inspection');

        move();
        await settle();
        move(second);
        await settle();
        check(active() && view.host.querySelectorAll('.surface-pointer-tip').length === 1,
            'a replacement inspector takes ownership without duplicate tooltips');
        first.hide();
        first.dispose();
        events.dispatchEvent(new Event('wheel'));
        check(active() && foreign.isConnected, 'old-owner cleanup cannot clear the replacement or unrelated overlays');
        const beforeDisposed = calls;
        move(first);
        await settle();
        check(calls === beforeDisposed && active(), 'a disposed inspector cannot reclaim the chart');
        otherEvents.dispatchEvent(new Event('wheel'));
        check(!active(), 'replacement keeps its own invalidation listeners');
        move(second);
        second.dispose();
        await settle();
        check(calls === beforeDisposed && !active() && foreign.isConnected, 'disposal cancels pending work and preserves foreign nodes');
    } finally {
        plot.marsPlotDrag = savedDrag;
        first.dispose();
        second.dispose();
        foreign.remove();
    }
}

/** @brief Check native picking thresholds, interpolation and whole-list handle bounds. */
window.checkLabSurfaceHover = async function checkLabSurfaceHover(): Promise<boolean> {
    /** @brief Report a failed native picking assertion. */
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('Surface hover: ' + message);
    }
    const chart = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    Object.assign(chart.dataset, {xMin: '-10', xMax: '10', yMin: '-1', yMax: '1', zMin: '-2', zMax: '2'});
    const frame = {width: 100, height: 100, fit: 100, rows: [1, 0, 0, 0, 1, 0],
        scaling: [1, 1, 1], offset: [0, 0, 0], drawMesh: true};
    const screen = {a: 1, b: 0, c: 0, d: 1, e: 0, f: 0};
    /** @brief Keep each independent endpoint pair in its own primitive. */
    function segment(a: MarsSurfacePoint3, b: MarsSurfacePoint3): SurfaceHoverFixturePrimitive {
        return {points: [a, b]};
    }

    /** @brief Allocate a distinct malformed primitive for handle-release checks. */
    function emptyPrimitive(): SurfaceHoverFixturePrimitive {
        return {points: []};
    }

    const line = segment([0, 0.5, 0.5], [1, 0.5, 0.5]);
    /** @brief Pick against the scenario's current projection and screen transform. */
    function pick(primitives: SurfaceHoverFixturePrimitive[], x: number, y: number,
                  previous: MarsSurfaceHoverHit | null = null): MarsSurfaceHoverHit | null {
        return labDOM.call('lab_surface_hover', {chart, primitives}, frame, screen, x, y, previous);
    }
    const hit = pick([line], 50, 55);
    check(hit?.x === 50 && hit.y === 50 && hit.distance === 25, 'nearest projected point');
    check(hit?.text.includes('x ≈ 0') && hit.text.includes('Re ≈ 0') && hit.text.includes('Im ≈ 0'), 'approximate interpolated coordinates');
    check(!pick([line], 50, 61), 'ten CSS pixel threshold');
    check(!pick([line], -1, 50, hit) && !pick([line], 101, 50, hit),
        'outside pointers cannot select nearby endpoints or retain a previous hit');
    check(pick([line], 0, 50)?.distance === 0 && pick([line], 100, 50)?.distance === 0,
        'pointers exactly on chart edges remain eligible');
    const vertical = segment([0.5, 0, 0.5], [0.5, 1, 0.5]);
    check(!pick([vertical], 50, -1) && !pick([vertical], 50, 101),
        'outside top and bottom pointers are rejected within the picking radius');
    screen.d = 0;
    check(!pick([line], 50, 0, hit), 'singular screen transform rejects even a previous hit');
    screen.d = 1;
    for (const name of ['a', 'b', 'c', 'd', 'e', 'f'] as const) {
        const saved = screen[name];
        screen[name] = Infinity;
        check(!pick([line], 50, 50, hit), 'non-finite screen coefficient rejected: ' + name);
        screen[name] = saved;
    }
    check(!pick([line], NaN, 50, hit), 'non-finite pointer rejected');
    Object.assign(screen, {a: 0, b: 2, c: -2, d: 0, e: 250, f: 10});
    check(pick([line], 150, 110)?.distance === 0 && !pick([line], 150, 9, hit),
        'native pointer bounds use the translated rotated screen transform');
    Object.assign(screen, {a: 1, b: 0, c: 0, d: 1, e: 0, f: 0});
    check(pick([line], 50, 55)?.distance === 25, 'valid picking recovers after invalid transforms');
    check(!pick([segment([0, 0.5, 0.5], [0.2, 0.5, 0.5]),
        segment([0.8, 0.5, 0.5], [1, 0.5, 0.5])], 50, 50), 'never bridge separate endpoint pairs');
    screen.a = 2;
    screen.d = 2;
    check(!pick([line], 100, 111) && pick([line], 100, 109)?.x === 100, 'threshold remains CSS pixels after scaling');
    screen.a = screen.d = 1;
    frame.rows = [0, -1, 0, 1, 0, 0];
    check(pick([line], 55, 75)?.y === 75, 'camera rotation participates in picking');
    frame.rows = [1, 0, 0, 0, 1, 0];
    frame.fit = 200;
    check(!pick([segment([0.225, 0.5, 0.5], [0.225, 0.6, 0.5])], 1, 55), 'offscreen marker rejected although pointer is inside');
    frame.fit = 100;
    check(pick(new Array<SurfaceHoverFixturePrimitive>(128).fill(line), 50, 50)?.distance === 0,
        'full batch stays within bridge handle capacity');
    check(pick([], 50, 55, hit)?.distance === 25, 'best hit survives a subsequent empty batch');
    const distant = segment([0, 0, 0.5], [1, 0, 0.5]);
    const many = new Array<SurfaceHoverFixturePrimitive>(257).fill(distant);
    many[256] = line;
    check(pick(many, 50, 55)?.distance === 25, 'one call finds a hit beyond the former 128-pair boundary');
    const full = new Array<SurfaceHoverFixturePrimitive>(65536).fill(line);
    check(pick(full, 50, 50)?.distance === 0, 'maximum admitted list releases handles for every pair');
    check(!pick(Array.from({length: 65536}, emptyPrimitive), 50, 50),
        'non-segment rejection also releases temporary handles');
    check(!pick(new Array<SurfaceHoverFixturePrimitive>(65536).fill(distant), 50, 50),
        'no-hit scan remains within the handle budget');
    full.push(line);
    check(!pick(full, 50, 50, hit), 'over-limit list rejected rather than returning a partial or previous hit');
    check(pick([line], 50, 55)?.distance === 25, 'valid picking recovers after over-limit rejection');
    frame.drawMesh = false;
    check(!pick([line], 50, 50), 'hidden geometry is not picked');
    return true;
};
