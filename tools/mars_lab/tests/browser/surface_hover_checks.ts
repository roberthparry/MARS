/**
 * @file surface_hover_checks.ts
 * @brief Native curve inspection regressions for projection, gaps and viewport clipping.
 *
 * Uses independent endpoint pairs and an explicit camera frame, exercising the real
 * native bridge without evaluating or interpreting mathematical expressions.
 */
interface Window { checkLabSurfaceHover(): Promise<boolean>; }
interface Window { checkLabSurfaceHoverDOM(chart: SVGSVGElement): Promise<void>; }

/** @brief Exercise real curve pointer events and ensure inspection never enters exported SVG. */
window.checkLabSurfaceHoverDOM = async function (chart: SVGSVGElement): Promise<void> {
    const check = (condition: unknown, message: string): void => {
        if (!condition) throw new Error('Surface hover DOM: ' + message);
    };
    const settle = (): Promise<void> => new Promise(resolve =>
        requestAnimationFrame(() => requestAnimationFrame(() => resolve())));
    const view = labSurfacePreview.prepare(chart);
    check(view, 'real curve has a retained view');
    if (!view) return;
    labSurfacePreview.draw(chart, view.angles);
    await settle();
    const frame = view.framePlan, screen = chart.getScreenCTM();
    check(frame && screen, 'retained projection is ready');
    if (!frame || !screen) return;
    let location: DOMPoint | undefined;
    for (const primitive of view.primitives) {
        const projected = labDOM.call<MarsSurfacePolygon>('lab_surface_polygon', frame, primitive.points);
        if (projected.points.length !== 2) continue;
        const [a, b] = projected.points;
        const x = (a[0] + b[0]) / 2, y = (a[1] + b[1]) / 2;
        if (x < 12 || y < 12 || x > view.width - 12 || y > view.height - 12) continue;
        location = new DOMPoint(x, y).matrixTransform(screen);
        break;
    }
    check(location, 'real curve contains a visible inspection point');
    if (!location) return;
    const point = location;
    const pointer = (type: string, buttons = 0): void => {
        chart.dispatchEvent(new PointerEvent(type, {bubbles: true, pointerId: 92,
            pointerType: 'mouse', isPrimary: true, buttons, button: 0,
            clientX: point.x, clientY: point.y}));
    };
    const hidden = (): boolean => !view.host.querySelector('.surface-pointer-marker, .surface-pointer-tip');
    const show = async (): Promise<void> => {
        pointer('pointermove');
        await settle();
        const marker = view.host.querySelector<HTMLElement>('.surface-pointer-marker');
        const tip = view.host.querySelector<HTMLElement>('.surface-pointer-tip');
        check(marker && tip && tip.textContent.includes('Approximate sample') &&
            tip.textContent.includes('Re ≈') && tip.textContent.includes('Im ≈'), 'pointer shows marker and formatted sample');
        if (tip) {
            const bounds = tip.getBoundingClientRect(), host = view.host.getBoundingClientRect();
            check(bounds.right <= host.right + 1 && bounds.left >= host.left - 1, 'tooltip stays horizontally inside its host');
        }
    };
    try {
        await show();
        const copied = WorksheetResults.clipboard.copyTextForTarget('plot');
        check(!copied.includes('surface-pointer-') && !copied.includes('Approximate sample'), 'copy excludes transient inspection');
        await show();
        pointer('pointerleave');
        check(hidden(), 'pointerleave hides inspection');
        await show();
        pointer('pointerdown', 1);
        check(hidden(), 'drag start hides inspection');
        pointer('pointerup');
        await settle();
        await show();
        chart.dispatchEvent(new WheelEvent('wheel', {bubbles: true, deltaY: 0}));
        check(hidden(), 'wheel hides inspection');
    } finally {
        pointer('pointercancel');
        pointer('pointerleave');
    }
};

/** @brief Check native picking thresholds, interpolation and bounded bridge batches. */
window.checkLabSurfaceHover = async function (): Promise<boolean> {
    const check = (condition: unknown, message: string): void => {
        if (!condition) throw new Error('Surface hover: ' + message);
    };
    const chart = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    Object.assign(chart.dataset, {xMin: '-10', xMax: '10', yMin: '-1', yMax: '1', zMin: '-2', zMax: '2'});
    const frame = {width: 100, height: 100, fit: 100, rows: [1, 0, 0, 0, 1, 0],
        scaling: [1, 1, 1], offset: [0, 0, 0], drawMesh: true};
    const screen = {a: 1, b: 0, c: 0, d: 1, e: 0, f: 0};
    const segment = (a: MarsSurfacePoint3, b: MarsSurfacePoint3) => ({points: [a, b]});
    const line = segment([0, 0.5, 0.5], [1, 0.5, 0.5]);
    const pick = (primitives: {points: MarsSurfacePoint3[]}[], x: number, y: number,
                  previous: MarsSurfaceHoverHit | null = null): MarsSurfaceHoverHit | null =>
        labDOM.call('lab_surface_hover', {chart, primitives}, frame, screen, x, y, previous);
    const hit = pick([line], 50, 55);
    check(hit?.x === 50 && hit.y === 50 && hit.distance === 25, 'nearest projected point');
    check(hit?.text.includes('x ≈ 0') && hit.text.includes('Re ≈ 0') && hit.text.includes('Im ≈ 0'), 'approximate interpolated coordinates');
    check(!pick([line], 50, 61), 'ten CSS pixel threshold');
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
    check(pick(Array.from({length: 128}, () => line), 50, 50)?.distance === 0, 'full batch stays within bridge handle capacity');
    check(pick([], 50, 55, hit)?.distance === 25, 'best hit survives a subsequent empty batch');
    check(!pick(Array.from({length: 129}, () => line), 50, 50), 'oversized bridge batches rejected');
    frame.drawMesh = false;
    check(!pick([line], 50, 50), 'hidden geometry is not picked');
    return true;
};
