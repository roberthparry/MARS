/**
 * @file plot_palette_checks.ts
 * @brief Live plot theme, GPU pixel, software fallback and vector export regressions.
 *
 * Exercises all fourteen skins using a retained native surface and a constant-colour
 * GPU triangle. Native geometry and camera snapshots must survive decorative changes.
 */
interface Window {
    /** @brief Check theme changes on a connected native mathematical surface. */
    checkLabPlotPalette(chart: SVGSVGElement): Promise<void>;
}

window.checkLabPlotPalette = async function checkLabPlotPalette(chart: SVGSVGElement): Promise<void> {
    function check(condition: unknown, message: string): asserts condition {
        if (!condition) throw new Error('Plot palette: ' + message);
    }
    const original = WorksheetAppearance.controller.current();
    const namespace = 'http://www.w3.org/2000/svg';
    const fixture = document.createElement('div');
    fixture.style.cssText = 'position:fixed;left:-1000px;width:64px;height:64px';
    const svg = document.createElementNS(namespace, 'svg');
    svg.dataset.plotChart = 'true';
    fixture.append(svg);
    const anchors = ['#3f6ba3', '#47b5b1', '#e7c569'];
    for (const colour of anchors) {
        const face = document.createElementNS(namespace, 'path');
        face.setAttribute('fill', colour);
        face.setAttribute('d', 'M0 0L64 0L0 64Z');
        svg.append(face);
    }
    const flag = document.createElementNS(namespace, 'svg');
    flag.innerHTML = '<path fill="#3f6ba3" d="M0 0L1 1"/>';
    fixture.append(flag);
    const probe = document.createElement('span');
    fixture.append(probe);
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = 64;
    fixture.append(canvas);
    document.body.append(fixture);
    const vertices: number[] = [];
    for (const [index, point] of [[0, 0, 0.5], [1, 0, 0.5], [0, 1, 0.5]].entries()) {
        vertices.push(...point, 63 / 255, 107 / 255, 163 / 255,
                      Number(index === 0), Number(index === 1), Number(index === 2));
    }
    const gpu = MarsSurfaceGPU.create(canvas, new Float32Array(vertices), false);
    const camera = new Float32Array([2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);
    const view = labSurfacePreview.prepare(chart);
    check(view && gpu, 'retained surface and real WebGL fixture are available');
    const geometry = JSON.stringify(view.primitives);
    const snapshot = JSON.stringify(labSurfacePreview.snapshot());
    const nativeFill = chart.querySelector('[data-vertices]')!.getAttribute('fill');
    const revision = labWire.exports().lab_workspace_history_revision(0);
    try {
        for (let skin = 0; skin < 14; ++skin) {
            WorksheetAppearance.controller.select(String(skin), false);
            await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
            const style = getComputedStyle(document.documentElement);
            const properties = ['--plot-low', '--plot-mid', '--plot-high'];
            for (let index = 0; index < anchors.length; ++index) {
                probe.style.color = style.getPropertyValue(properties[index]);
                const face = svg.children[index];
                check(getComputedStyle(face).fill === getComputedStyle(probe).color,
                      `skin ${skin}: SVG ramp anchor ${index} matches the theme`);
                check(face.getAttribute('fill') === anchors[index], 'native SVG colour attributes remain unchanged');
            }
            check(!flag.firstElementChild!.hasAttribute('style'), 'non-plot artwork retains its authored colours');
            check(gpu.draw(camera, [1, 1, 1], [0, 0, 0], true), 'retained GPU fixture draws after a theme change');
            const gl = canvas.getContext('webgl')!;
            const pixel = new Uint8Array(4);
            gl.readPixels(16, 16, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
            const expected = Number.parseInt(style.getPropertyValue('--plot-low').trim().slice(1), 16);
            for (let channel = 0; channel < 3; ++channel) {
                const wanted = expected >> (16 - channel * 8) & 255;
                check(Math.abs(pixel[channel] - wanted) <= 2 && pixel[3] === 255,
                      `skin ${skin}: GPU pixels match the SVG palette`);
            }
            check(labSurfacePreview.prepare(chart) === view && JSON.stringify(view.primitives) === geometry &&
                  JSON.stringify(labSurfacePreview.snapshot()) === snapshot,
                  'theme changes keep the retained renderer, geometry, camera and volume');
            check(chart.querySelector('[data-vertices]')!.getAttribute('fill') === nativeFill,
                  'native colour metadata survives theme changes');
        }
        // Switching back to Earth restores the exact original colours, including after every other skin.
        WorksheetAppearance.controller.select('0', false);
        await new Promise(requestAnimationFrame);
        check(!svg.firstElementChild!.getAttribute('style'), 'Earth restores unmodified native SVG colours');
        const saved = labSurfacePreview.snapshot();
        labSurfacePreview.dispose();
        const software = labSurfacePreview.prepare(chart, saved, true);
        check(software?.context && !software.gpu, 'software fallback uses the retained Canvas renderer');
        const pixels = (): string => {
            check(labSurfacePreview.draw(chart, software.angles), 'software surface draws');
            const context = software.context!;
            const data = context.getImageData(0, 0, context.canvas.width, context.canvas.height).data;
            const sums = [0, 0, 0, 0];
            for (let offset = 0; offset < data.length; offset += 4) {
                for (let channel = 0; channel < 4; ++channel) sums[channel] += data[offset + channel];
            }
            check(sums[3] > 0, 'software surface contains visible pixels');
            return JSON.stringify(sums);
        };
        const earth = pixels();
        WorksheetAppearance.controller.select('1', false);
        await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
        check(pixels() !== earth && JSON.stringify(labSurfacePreview.snapshot()) === snapshot,
              'Mars recolours visible software pixels while preserving the camera and volume');
        const exported = labSurfacePreview.exportSvg();
        const documentCopy = new DOMParser().parseFromString(exported || '', 'image/svg+xml');
        const fill = documentCopy.querySelector('[fill="' + nativeFill + '"]') as SVGElement | null;
        const expectedFill = document.createElementNS(namespace, 'path');
        expectedFill.style.fill = WorksheetPlotPalette.colour(nativeFill!);
        check(fill && fill.style.fill === expectedFill.style.fill,
              'vector export contains self-contained theme colours: ' +
              JSON.stringify({original: nativeFill, actual: fill?.style.fill, expected: expectedFill.style.fill,
                              exported: exported?.slice(0, 200)}));
        check(labWire.exports().lab_workspace_history_revision(0) === revision,
              'decorative changes do not evaluate or enter worksheet history');
    } finally {
        gpu.dispose();
        fixture.remove();
        WorksheetAppearance.controller.select(original, false);
        labSurfacePreview.dispose();
        const restored = labSurfacePreview.prepare(chart, JSON.parse(snapshot) as MarsSurfaceSnapshot);
        if (restored) labSurfacePreview.draw(chart, restored.angles);
    }
};
