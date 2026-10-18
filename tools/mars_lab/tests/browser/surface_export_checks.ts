/**
 * @file surface_export_checks.ts
 * @brief Native SVG command planning and literal browser-node regressions.
 *
 * Checks primitive boundaries, adjacent-colour batching, polygon closure, axis paths
 * and bounded handle lifetimes through the actual WebAssembly bridge. Browser checks
 * also ensure that plain labels and attributes are never parsed as markup.
 */
interface Window {
    /** @brief Exercise SVG planning and safe browser element creation. */
    checkLabSurfaceSVG(): void;
}

/** @brief Verify native drawing plans using simple, exactly representable coordinates. */
window.checkLabSurfaceSVG = function checkLabSurfaceSVG(): void {
    /** @brief Report a failed SVG planning assertion. */
    function check(condition: boolean, message: string): void {
        if (!condition)
            throw new Error('Surface SVG: ' + message);
    }

    /** @brief Ask native code to plan paths for the supplied projection records. */
    function plan(polygons: MarsSurfaceDrawing[], paths: number[][], segments: boolean): MarsSurfaceSVGPlan {
        return labDOM.call('lab_surface_svg_plan', polygons, paths, Number(segments)) as MarsSurfaceSVGPlan;
    }

    /** @brief Build one independently coloured, exactly representable segment. */
    function segment(colour: string, x: number): MarsSurfaceDrawing {
        return {points: [[x, 1], [x + 2, 3]], colour: colour, depth: 0};
    }
    const polygons = [segment('#123456', 0), segment('#123456', 10),
                      segment('#abcdef', 20), segment('#123456', 30)];
    const before = JSON.stringify(polygons);
    const curves = plan(polygons, [[0, 0, 5, 10, 15, 20]], true);
    check(curves.series.length === 3, 'only adjacent colours are batched');
    check(curves.series[0].path === 'M0 1L2 3M10 1L12 3',
          'each independent segment starts a new subpath, preserving gaps');
    check(curves.series[2].colour === '#123456' && curves.series[2].path === 'M30 1L32 3',
          'colour reappearance preserves painter order');
    check(curves.axes[0] === 'M0 0L5 10L15 20', 'native axis path preserves every vertex');
    check(JSON.stringify(polygons) === before, 'SVG planning does not mutate the projection plan');
    const triangle: MarsSurfaceDrawing = {points: [[0, 0], [1, 0], [0, 1]], colour: '#000000', depth: 0};
    const mesh = plan([triangle, triangle], [], false);
    check(mesh.series.length === 2 && mesh.series[0].path === 'M0 0L1 0L0 1Z',
          'mesh faces close individually without colour batching');
    const empty = plan([], [], true);
    check(empty.series.length === 0 && empty.axes.length === 0, 'empty plans remain empty');
    const invalid: MarsSurfaceDrawing = {points: [[NaN, 0], [1, 1]], colour: '#123456', depth: 0};
    const interrupted = plan([polygons[0], invalid, polygons[1]], [[0, 0, 1], [0, 0, Infinity, 1]], true);
    check(interrupted.series.length === 2 && interrupted.axes.length === 0,
          'invalid records are skipped and cannot merge separate colour groups');
    const long_axis: number[] = [];
    for (let i = 0; i < 2049; ++i)
        long_axis.push(i, i);
    const long_path = plan([], [long_axis], false);
    check(long_path.axes[0].split('L').length === 2049,
          'long individual axes release temporary point handles');
    const many: MarsSurfaceDrawing[] = [];
    const axes: number[][] = [];
    for (let i = 0; i < 2049; ++i) {
        many.push(segment('#123456', i));
        axes.push([i, 0, i, 1]);
    }
    for (let repeat = 0; repeat < 2; ++repeat) {
        const large = plan(many, axes, true);
        check(large.series.length === 1 && large.series[0].path.split('M').length === 2050 &&
              large.axes.length === 2049, 'large repeated plans retain every path without exhausting handles');
    }
    const parent = document.createElementNS('http://www.w3.org/2000/svg', 'g');
    const label = '<tspan>not markup</tspan> & "quoted"';
    const text = MarsSurfaceSVGWriter.node(parent, 'text', {'aria-label': label}, label);
    check(text.textContent === label && text.getAttribute('aria-label') === label && !text.children.length,
          'browser writer treats text and attribute values literally');
};
