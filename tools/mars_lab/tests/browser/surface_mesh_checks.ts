/**
 * @file surface_mesh_checks.ts
 * @brief Native retained-mesh decoding, exterior and origin-centred axes, and vertex-layout regressions.
 *
 * Supplies small SVG fixtures directly to WebAssembly, checking geometry admission,
 * fan triangulation, colour expansion and temporary-handle lifetime without a GPU.
 */
interface Window {
    /** @brief Validate native display-metadata decoding through the real browser bridge. */
    checkLabSurfaceMesh(): void;
    /** @brief Validate exterior and origin-centred axes, rotation, zoom and bounded numeric label spacing. */
    checkLabSurfaceAxes(): void;
    /** @brief Verify depth-faded axes in real GPU pixels and native software/export plans. */
    checkLabSurfaceVisibility(): void;
}

/** @brief Exercise native mesh preparation using detached, fixture-owned SVG elements. */
window.checkLabSurfaceMesh = function checkLabSurfaceMesh(): void {
    /** @brief Report a failed native mesh assertion. */
    function check(condition: boolean, message: string): void {
        if (!condition)
            throw new Error('Surface mesh: ' + message);
    }
    const chart = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    chart.dataset.units = '1 2 3';
    /** @brief Decode the chart's current display metadata through the native bridge. */
    function decode(): MarsSurfaceMesh | null {
        return labDOM.call('lab_surface_mesh', chart) as MarsSurfaceMesh | null;
    }
    check(decode() === null, 'empty geometry requires explicit admission');
    chart.dataset.empty = 'true';
    check(decode()?.vertices.length === 0, 'explicitly empty chart is accepted');
    delete chart.dataset.empty;
    const segment = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    segment.dataset.segment = '0 0 0 1 1 1';
    segment.setAttribute('stroke', '#ff8000');
    chart.append(segment);
    const line = decode()!;
    check(line.primitives.length === 1 && line.vertices.length === 18 && line.units[1] === 2,
          'segment yields two interleaved vertices and preserves units');
    check(line.vertices[3] === 1 && line.vertices[4] === 128 / 255 && line.vertices[5] === 0,
          'RGB channels are normalised');
    check(line.vertices[6] === 1 && line.vertices[7] === 1 && line.vertices[8] === 1,
          'segments retain constant corner attributes');
    segment.dataset.segment = '0 0 NaN 1 1 1';
    check(decode() === null, 'non-finite coordinates are rejected');
    segment.dataset.segment = '0 0 0';
    check(decode() === null, 'incomplete endpoint pairs are rejected');
    segment.dataset.segment = '0 0 0 1 1 1';
    chart.dataset.units = '1 0 1';
    check(decode() === null, 'non-positive units are rejected');
    chart.dataset.units = '1 Infinity 1';
    check(decode() === null, 'non-finite units are rejected');
    chart.dataset.units = '1 1 1';
    segment.setAttribute('stroke', 'invalid');
    check(decode() === null, 'invalid RGB metadata is rejected');
    segment.removeAttribute('stroke');
    check(decode()?.primitives[0].colour === '#000000', 'missing colour defaults to black');
    const face = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    face.dataset.vertices = '0 0 0 1 0 0 1 1 0 0 1 0';
    face.setAttribute('fill', '#0080ff');
    chart.replaceChildren(face);
    const fan = decode()!;
    check(fan.primitives.length === 2 && fan.vertices.length === 54, 'quad produces two fan triangles');
    check(JSON.stringify(fan.primitives[1].points) === '[[0,0,0],[1,1,0],[0,1,0]]',
          'fan preserves vertex ordering');
    for (let vertex = 0; vertex < 6; ++vertex) {
        for (let corner = 0; corner < 3; ++corner)
            check(fan.vertices[vertex * 9 + 6 + corner] === Number(vertex % 3 === corner),
                  'triangle corner attributes match vertex positions');
    }
    face.dataset.vertices += ' 0 0 1 1 1 1';
    check(decode() === null, 'overlong polygons are rejected');
    face.dataset.vertices = '0 0 0 1 0 0 0 1 0';
    for (let i = 1; i < 2049; ++i)
        chart.append(face.cloneNode());
    for (let repeat = 0; repeat < 2; ++repeat) {
        const many = decode();
        check(many?.primitives.length === 2049 && many.vertices.length === 2049 * 27,
              'large repeated preparations release per-record handles');
    }
    window.checkLabSurfaceAxes();
    window.checkLabSurfaceVisibility();
};

/** @brief Hidden sections stay clearly visible, while front sections and the uncovered background remain solid. */
window.checkLabSurfaceVisibility = function checkLabSurfaceVisibility(): void {
    function check(condition: unknown, message: string): asserts condition {
        if (!condition) throw new Error('Surface visibility: ' + message);
    }
    const original = WorksheetAppearance.controller.current();
    WorksheetAppearance.controller.select('0', false);
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = 64;
    const vertices: number[] = [];
    for (const point of [[.25, .25, .5], [.75, .25, .5], [.75, .75, .5],
                         [.25, .25, .5], [.75, .75, .5], [.25, .75, .5]])
        vertices.push(...point, .1, .2, .3, 1, 1, 1);
    const gpu = MarsSurfaceGPU.create(canvas, new Float32Array(vertices), false);
    check(gpu, 'real WebGL renderer is available');
    const gl = canvas.getContext('webgl')!;
    const camera = new Float32Array([2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);
    const pixel = (x: number, y: number): Uint8Array => {
        const value = new Uint8Array(4);
        gl.readPixels(x, y, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, value);
        return value;
    };
    try {
        check(gpu.draw(camera, [1, 1, 1], [0, 0, 0], true), 'surface-only baseline draws');
        const baseline = pixel(40, 32);
        const axes: number[] = [];
        for (const point of [[-.9, -.05], [.9, -.05], [.9, .05], [-.9, -.05], [.9, .05], [-.9, .05]])
            axes.push(...point, .5, 1, 1);
        for (const point of [[-.05, -.9], [.05, -.9], [.05, .9], [-.05, -.9], [.05, .9], [-.05, .9]])
            axes.push(...point, -.5, 1, 1);
        check(gpu.draw(camera, [1, 1, 1], [0, 0, 0], true, new Float32Array(axes), .4),
              'axes draw in front of and behind the same surface depth buffer');
        const hidden = pixel(40, 32), outside = pixel(8, 32), front = pixel(32, 40);
        const colour = [148, 184, 177];
        for (let i = 0; i < 3; ++i) {
            check(Math.abs(hidden[i] - (colour[i] * .4 + baseline[i] * .6)) <= 2,
                  'covered axes retain forty per cent opacity rather than disappearing');
            check(Math.abs(outside[i] - colour[i]) <= 2 && Math.abs(front[i] - colour[i]) <= 2,
                  'uncovered and front-facing sections remain solid');
        }
        check(hidden[3] === 255 && outside[3] === 255 && front[3] === 255,
              'faded axes preserve the opaque surface underneath');
        check(gl.getError() === gl.NO_ERROR, 'depth passes do not leave GPU errors');
    } finally {
        gpu.dispose();
        WorksheetAppearance.controller.select(original, false);
    }
    const face: MarsSurfaceAxisFace = {points: [[0, 0], [10, 0], [10, 10], [0, 10]],
        depths: [2, 2, 2, 2], shade: .8, axis: 0, cone: false};
    const frame = {distance: 10, solids: [face]} as MarsSurfaceFrame;
    const cover = [
        {points: [[0, 0], [5, 0], [5, 10]], depths: [0, 0, 0]},
        {points: [[0, 0], [5, 10], [0, 10]], depths: [0, 0, 0]}
    ];
    labDOM.call('lab_surface_visibility', frame, cover);
    check(face.visible?.length && face.visible.every(piece => piece.every(point => point[0] >= 5 - 1e-8)),
          'software visibility keeps the uncovered half solid and fades only the covered half');
    const plan = labDOM.call<MarsSurfaceSVGPlan>('lab_surface_svg_plan', [], [], 0, [face]);
    check(plan.solids[0].faded && plan.solids[0].visible?.length === face.visible.length,
          'vector export retains both the faint full face and its opaque visible fragments');
};

/** @brief Check axis geometry through the real WebAssembly camera and projection bridge. */
window.checkLabSurfaceAxes = function checkLabSurfaceAxes(): void {
    function check(condition: boolean, message: string): void {
        if (!condition) throw new Error('Surface axes: ' + message);
    }
    const chart = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    const volume = {units: [1, 1, 1], exponent: 0, sampleExponent: 0, labelWidth: 30,
                    bounds: [-1, 3, -3, 5, -4, 4],
                    autoHeight: false, dirty: false};
    const ticks: MarsSurfaceTick[] = [];
    for (let axis = 0; axis < 3; ++axis) {
        for (let i = 0; i <= 10; ++i)
            ticks.push({axis, fraction: i / 10, label: String(i - 5), zero: i === 5});
    }
    const view = {chart, width: 560, height: 420, units: [1, 1, 1], names: ['x', 'y', 'z'],
                  ticks, volume, orientation: [0, 0, 0, 1]};
    function frame(): MarsSurfaceFrame {
        return labDOM.call<MarsSurfaceFrame>('lab_surface_frame', view, null);
    }
    function checkFrame(plan: MarsSurfaceFrame): void {
        const corners = plan.axisCorners;
        check(!!corners && corners.length === 3, 'native frame identifies all three coordinate edges');
        const screen = Array.from({length: 8}, (_, corner) => labDOM.call<number[]>('lab_surface_project', plan,
            [Number(!!(corner & 1)), Number(!!(corner & 2)), Number(!!(corner & 4))]));
        let joins = 0, supported = 0;
        for (let axis = 0; axis < 3; ++axis) {
            const corner = corners![axis], last = corner | 1 << axis;
            check(corner >= 0 && corner < 8 && !(corner & 1 << axis), 'edge starts at its lower coordinate bound');
            const path = plan.paths[axis * 2];
            const length = Math.hypot(path[2] - path[0], path[3] - path[1]);
            const dx = screen[last][0] - screen[corner][0], dy = screen[last][1] - screen[corner][1];
            const edgeLength = Math.hypot(dx, dy), fraction = edgeLength / length;
            check(Math.hypot(path[0] - screen[corner][0], path[1] - screen[corner][1]) < 1e-8 &&
                  Math.hypot(path[0] + (path[2] - path[0]) * fraction - screen[last][0],
                             path[1] + (path[3] - path[1]) * fraction - screen[last][1]) < 1e-8,
                  'axis endpoints coincide with the actual projected frame corners');
            const arrow = plan.paths[axis * 2 + 1];
            check(Math.hypot(arrow[2] - path[2], arrow[3] - path[3]) < 1e-8 &&
                  ((arrow[2] - screen[last][0]) * dx + (arrow[3] - screen[last][1]) * dy) / edgeLength > 0,
                  'arrow tips continue towards increasing coordinate values, clear of shared frame corners');
            const ticks = Array.from({length: 5}, (_, i) => {
                const point = [Number(!!(corner & 1)), Number(!!(corner & 2)), Number(!!(corner & 4))];
                point[axis] = i / 4;
                return {screen: labDOM.call<number[]>('lab_surface_project', plan, point),
                    depth: plan.rows.slice(6).reduce((sum, value, j) => sum + value * (point[j] - 0.5), 0)};
            });
            const gap = (i: number) => Math.hypot(ticks[i + 1].screen[0] - ticks[i].screen[0],
                                                  ticks[i + 1].screen[1] - ticks[i].screen[1]);
            const distance = plan.distance!;
            const ratio = ((distance + ticks[3].depth) * (distance + ticks[4].depth)) /
                          ((distance + ticks[0].depth) * (distance + ticks[1].depth));
            check(Math.abs(gap(0) / gap(3) - ratio) < 1e-8,
                  'equal coordinate tick intervals compress according to the pinhole distance formula');
            for (const [i, tick] of ticks.entries()) {
                const point = [(corner & 1) ? 0.5 : -0.5, (corner & 2) ? 0.5 : -0.5,
                               (corner & 4) ? 0.5 : -0.5];
                point[axis] = i / 4 - 0.5;
                const clip = [0, 1, 2, 3].map(row => plan.matrix[12 + row] +
                    point.reduce((sum, value, col) => sum + value * plan.matrix[col * 4 + row], 0));
                check(Math.hypot((clip[0] / clip[3] + 1) * plan.width / 2 - tick.screen[0],
                                  (1 - clip[1] / clip[3]) * plan.height / 2 - tick.screen[1]) < 1e-8,
                      'WebGL homogeneous division and native SVG/Canvas projection agree');
            }
            const distances = screen.map(point => ((path[2] - path[0]) * (point[1] - path[1]) -
                                                   (path[3] - path[1]) * (point[0] - path[0])) / length);
            supported += Number(Math.min(...distances) >= -1e-8 || Math.max(...distances) <= 1e-8);
            for (let other = 0; other < axis; ++other) {
                const previous = corners![other] | 1 << other;
                joins += Number(corner === corners![other] || corner === previous ||
                                last === corners![other] || last === previous);
            }
        }
        check(joins >= 2, 'three axes form one connected frame rather than floating independently');
        check(supported >= 2, 'face-on perspective retains two silhouette scales and the short third joining scale');
        check(!!plan.solids?.length && plan.solids.length <= 102 && plan.solids.every(face => face.points.length >= 3 &&
              face.shade >= 0 && face.shade <= 1), 'axes are bounded opaque cylinder and cone meshes');
        checkClearance(plan);
        for (let axis = 0; axis < 3; ++axis)
            check(plan.solids!.some(face => face.axis === axis && face.cone) &&
                  plan.solids!.some(face => face.axis === axis && !face.cone),
                  'every axis has a circular cylinder and a genuine three-dimensional cone');
        const boxes: number[][] = [];
        for (const label of plan.texts) {
            const size = label.size ?? 13;
            const width = label.text.length * size * 0.8;
            const left = label.x - (label.align === 'right' ? width : label.align === 'center' ? width / 2 : 0);
            const box = [left, label.y - size, left + width, label.y + size * 0.3];
            for (const other of boxes)
                check(box[2] + 3.99 <= other[0] || other[2] + 3.99 <= box[0] ||
                      box[3] + 3.99 <= other[1] || other[3] + 3.99 <= box[1], 'labels have visible clearance');
            boxes.push(box);
        }
        check(['x', 'y', 'z'].every(name => plan.texts.some(item => item.text === name)),
              'positive axis directions retain their names');
    }
    /** @brief Measure actual projected arrow meshes and names against the shared viewport clearance. */
    function checkClearance(plan: MarsSurfaceFrame): void {
        for (const face of plan.solids ?? [])
            check(face.points.every(point => point[0] >= 15.99 && point[0] <= plan.width - 15.99 &&
                  point[1] >= 15.99 && point[1] <= plan.height - 15.99),
                  'extended shafts and cone tips stay clear of every viewport edge');
        for (const label of plan.texts.filter(label => label.bold)) {
            const size = label.size ?? 14, width = label.text.length * size * 0.8;
            const left = label.x - (label.align === 'right' ? width : label.align === 'center' ? width / 2 : 0);
            check(left >= 15.99 && left + width <= plan.width - 15.99 && label.y - size >= 15.99 &&
                  label.y + size * 0.3 <= plan.height - 15.99,
                  'positive axis names keep their clearance beside the arrowheads');
        }
    }
    for (const angles of [[315, 30, 0], [18, 80, 45], [145, 15, 135], [315, 30, 180], [85, 5, 3], [90, 30, 90]]) {
        view.orientation = labDOM.call<number[]>('lab_surface_camera', 0, angles, null, null, null, 0);
        checkFrame(frame());
    }
    for (const units of [[1, 1, 1], [1, 0.7935906173, 0.7935906173]]) {
        view.units = units;
        for (const angles of [[180, -10, 0], [180, 10, 0], [0, -10, 0], [0, 10, 0]]) {
            view.orientation = labDOM.call<number[]>('lab_surface_camera', 0, angles, null, null, null, 0);
            const plan = frame();
            checkFrame(plan);
            const points = Array.from({length: 8}, (_, corner) => labDOM.call<number[]>('lab_surface_project', plan,
                [Number(!!(corner & 1)), Number(!!(corner & 2)), Number(!!(corner & 4))]));
            const span = Math.max(Math.max(...points.map(point => point[0])) - Math.min(...points.map(point => point[0])),
                                  Math.max(...points.map(point => point[1])) - Math.min(...points.map(point => point[1])));
            for (let axis = 0; axis < 3; ++axis) {
                const first = plan.axisCorners![axis], last = first | 1 << axis;
                check(Math.hypot(points[last][0] - points[first][0], points[last][1] - points[first][1]) > span * 0.17,
                      'nearly face-on views choose a useful depth edge instead of a short overlapping scale');
                for (let other = 0; other < axis; ++other) {
                    const low = plan.axisCorners![other], high = low | 1 << other;
                    const join = first === low || first === high ? first : last === low || last === high ? last : -1;
                    if (join < 0) continue;
                    const a = points[join === first ? last : first], b = points[join === low ? high : low];
                    const dx = a[0] - points[join][0], dy = a[1] - points[join][1];
                    const ex = b[0] - points[join][0], ey = b[1] - points[join][1];
                    check(Math.abs(dx * ey - dy * ex) / Math.max(Math.hypot(dx, dy), Math.hypot(ex, ey)) > span * 0.12,
                          'adjoining axes have perpendicular clearance instead of appearing as one straight scale');
                }
            }
        }
    }
    view.units = [1, 1, 1];
    volume.exponent = 1;
    volume.dirty = true;
    volume.bounds = [0, 2, -1, 3, -2, 2];
    checkFrame(frame());
    volume.exponent = 2;
    checkFrame(frame());
    volume.exponent = 0;
    volume.dirty = false;
    checkFrame(frame());
    check(frame().texts.every(item => item.size! >= 13), 'tick labels retain their readable native size');
    view.units = [0.7935906173, 0.7935906173, 1];
    for (const azimuth of [0, 90, 180, 270]) {
        Reflect.deleteProperty(view, 'axisCorners');
        view.orientation = labDOM.call<number[]>('lab_surface_camera', 0, [azimuth - 0.02, 30, 0], null, null, null, 0);
        const reference = frame().axisCorners!.join(',');
        for (let step = 0; step < 20; ++step) {
            view.orientation = labDOM.call<number[]>('lab_surface_camera', 0,
                [azimuth + (step % 2 ? -0.02 : 0.02), 30, 0], null, null, null, 0);
            const plan = frame();
            checkFrame(plan);
            check(plan.axisCorners!.join(',') === reference,
                  'small camera reversals retain readable reference edges instead of flipping the scales');
        }
    }
    view.units = [1, 1, 1];
    view.width = 800;
    view.height = 650;
    /** @brief Count interior strokes, excluding shared corner positions belonging to more than one axis. */
    function intermediateTickCount(plan: MarsSurfaceFrame, axis: number): number {
        const corner = plan.axisCorners![axis];
        const expected = view.ticks.filter(tick => tick.axis === axis && tick.fraction > 0 && tick.fraction < 1).map(tick => {
            const point = [Number(!!(corner & 1)), Number(!!(corner & 2)), Number(!!(corner & 4))];
            point[axis] = tick.fraction;
            return labDOM.call<number[]>('lab_surface_project', plan, point);
        });
        return plan.paths.slice(plan.axisPathCount).filter(path =>
            expected.some(point => Math.hypot(path[0] - point[0], path[1] - point[1]) < 1e-7)).length;
    }
    view.orientation = labDOM.call<number[]>('lab_surface_camera', 0, [0, 0, 0], null, null, null, 0);
    const shortCount = intermediateTickCount(frame(), 1);
    view.orientation = labDOM.call<number[]>('lab_surface_camera', 0, [90, 0, 0], null, null, null, 0);
    check(intermediateTickCount(frame(), 1) === 9 && shortCount < 9,
          'rotating a short depth axis into a long scale restores every retained intermediate stroke');
    view.width = 320;
    view.height = 240;
    frame();
    view.width = 800;
    view.height = 650;
    check(intermediateTickCount(frame(), 1) === 9 && view.ticks.length === 33,
          'resizing does not permanently prune the numerical tick catalogue');

    chart.dataset.axisOrigin = '0.25 0.375 0.5';
    view.units = [1, 1, 1];
    Reflect.set(volume, 'sampled', [-1, 3, -3, 5, -4, 4]);
    Reflect.set(volume, 'sampleExponent', 0);
    for (const exponent of [0, 1]) {
        volume.exponent = exponent;
        for (const angles of [[315, 30, 0], [18, 80, 45], [145, 15, 135]]) {
            view.orientation = labDOM.call<number[]>('lab_surface_camera', 0, angles, null, null, null, 0);
            const plan = frame();
            checkClearance(plan);
            const zero = [0.25, 0.375, 0.5].map(value => 0.5 + (value - 0.5) * Math.pow(2, exponent));
            const crossing = labDOM.call<number[]>('lab_surface_project', plan, zero);
            for (let axis = 0; axis < 3; ++axis) {
                const lower = [...zero], upper = [...zero];
                lower[axis] = -0.08;
                upper[axis] = 1.08;
                const a = labDOM.call<number[]>('lab_surface_project', plan, lower);
                const b = labDOM.call<number[]>('lab_surface_project', plan, upper);
                const path = plan.paths[axis * 2];
                const dx = path[2] - path[0], dy = path[3] - path[1];
                check(Math.hypot(path[0] - a[0], path[1] - a[1]) < 1e-8 &&
                      Math.abs(dx * (crossing[1] - path[1]) - dy * (crossing[0] - path[0])) /
                          Math.hypot(dx, dy) < 1e-8,
                      'all three retained axes pass through the exact mathematical zero after rotation and zoom');
                check((path[2] - b[0]) * (b[0] - a[0]) + (path[3] - b[1]) * (b[1] - a[1]) > 0,
                      'origin-centred cones continue to point in the positive coordinate direction');
            }
        }
    }
    volume.exponent = 0;
    chart.dataset.axisOrigin = '0.5 0.5 0.5';
    for (const [width, height] of [[320, 240], [560, 420], [800, 800], [1200, 700]]) {
        view.width = width;
        view.height = height;
        for (const units of [[1, 1, 1], [1, 1, 0.25], [0.25, 1, 1]]) {
            view.units = units;
            for (const angles of [[0, 5, 0], [90, 5, 0], [180, 5, 0], [270, 5, 0],
                                  [315, 30, 45], [0, 30, 90]]) {
                view.orientation = labDOM.call<number[]>('lab_surface_camera', 0, angles, null, null, null, 0);
                checkClearance(frame());
            }
        }
    }
};
