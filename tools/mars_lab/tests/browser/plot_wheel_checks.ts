/**
 * @file plot_wheel_checks.ts
 * @brief Wheel admission within native plot volumes and browser scrolling outside them.
 *
 * Checks projected-box geometry and real SVG transforms without sampling maths.
 * Live event checks exercise all globe-compatible modifiers, prevent-default
 * policy and unchanged camera orientation, restoring the original volume and controls.
 */
interface Window {
    /** @brief Check rotated volume boundaries and transformed curve/surface pointer admission. */
    checkLabPlotWheelGeometry(): void;
    /** @brief Check real wheel events on a ready surface, preserving its worksheet and viewport. */
    checkLabPlotWheel(chart: SVGSVGElement): Promise<void>;
}

window.checkLabPlotWheelGeometry = (): void => {
    const check = (condition: unknown, message: string): void => {
        if (!condition) throw new Error('Plot wheel geometry: ' + message);
    };
    const chart = document.createElementNS('http://www.w3.org/2000/svg', 'svg') as MarsSurfaceChart;
    chart.setAttribute('viewBox', '0 0 400 300');
    chart.style.cssText = 'position:fixed;left:40px;top:40px;width:400px;height:300px;transform-origin:0 0';
    chart.dataset.surface = 'true';
    document.body.append(chart);
    try {
        for (const angles of [[315, 30, 0], [85, 5, 3], [145, 65, 135], [90, 30, 90]]) {
            const quaternion = labDOM.call<number[]>('lab_surface_camera', 0, angles, null, null, null, 0);
            const rows = labDOM.call<number[][]>('lab_surface_camera', 1, quaternion, null, null, null, 0).flat();
            const frame: MarsSurfaceFrame = {width: 400, height: 300, fit: 100, rows, matrix: [],
                scaling: [1, 1, 1], offset: [0, 0, 0], drawMesh: true, paths: [], texts: []};
            chart.marsSurfaceFrame = frame;
            check(labDOM.call('lab_surface_contains', frame, 200, 150), 'the volume centre is inside');
            const points = Array.from({length: 8}, (_, i) => labDOM.call<number[]>('lab_surface_project', frame,
                [Number(!!(i & 1)), Number(!!(i & 2)), Number(!!(i & 4))]));
            check(points.every(point => labDOM.call('lab_surface_contains', frame, ...point)),
                  'all projected box corners lie on the admitted boundary');
            const radius = Math.max(...points.map(point => Math.hypot(point[0] - 200, point[1] - 150)));
            for (const point of points) {
                const x = 200 + (point[0] - 200) * 1.2, y = 150 + (point[1] - 150) * 1.2;
                if (Math.hypot(point[0] - 200, point[1] - 150) >= radius - 1e-8)
                    check(!labDOM.call('lab_surface_contains', frame, x, y), 'outside rays leave page scrolling available');
            }
            const xs = points.map(point => point[0]), ys = points.map(point => point[1]);
            const bounds = [[Math.min(...xs), Math.min(...ys)], [Math.max(...xs), Math.min(...ys)],
                            [Math.min(...xs), Math.max(...ys)], [Math.max(...xs), Math.max(...ys)]];
            if (angles[0] !== 90)
                check(bounds.filter(point => !labDOM.call('lab_surface_contains', frame, ...point)).length >= 2,
                      'empty corners inside the rectangular screen bounds remain outside the rotated volume');
            for (const transform of ['none', 'scale(.65)', 'rotate(23deg) scale(.8)']) {
                chart.style.transform = transform;
                const screen = marsTransportSVGScreen(chart)!;
                const event = (x: number, y: number) => ({clientX: screen.a * x + screen.c * y + screen.e,
                    clientY: screen.b * x + screen.d * y + screen.f});
                check(labDOM.call('lab_plot_pointer_inside', chart, event(200, 150)),
                      'SVG fitting and CSS rotation retain the correct inside pointer');
                check(!labDOM.call('lab_plot_pointer_inside', chart, event(0, 0)),
                      'SVG margins remain outside after fitting and CSS rotation');
            }
        }
        const frame = chart.marsSurfaceFrame!;
        check(!labDOM.call('lab_surface_contains', frame, NaN, 150) &&
              !labDOM.call('lab_surface_contains', {...frame, fit: 0}, 200, 150), 'invalid geometry is not captured');
        delete chart.dataset.surface;
        chart.dataset.plotLeft = '60'; chart.dataset.plotTop = '40';
        chart.dataset.plotWidth = '280'; chart.dataset.plotHeight = '200';
        const screen = marsTransportSVGScreen(chart)!;
        const event = (x: number, y: number) => ({clientX: screen.a * x + screen.c * y + screen.e,
            clientY: screen.b * x + screen.d * y + screen.f});
        check(labDOM.call('lab_plot_pointer_inside', chart, event(200, 150)) &&
              !labDOM.call('lab_plot_pointer_inside', chart, event(30, 150)) &&
              !labDOM.call('lab_plot_pointer_inside', chart, event(200, 20)),
              'curve axis labels and headings remain outside its data rectangle');
    } finally { chart.remove(); }
};

window.checkLabPlotWheel = async (chart: SVGSVGElement): Promise<void> => {
    const check = (condition: unknown, message: string): void => {
        if (!condition) throw new Error('Plot wheel events: ' + message);
    };
    window.checkLabPlotWheelGeometry();
    const view = labSurfacePreview.prepare(chart)!;
    check(view && labSurfacePreview.draw(chart, view.angles), 'the retained surface is ready');
    const volume = labSurfaceVolume.clone(view.volume), camera = {...view.camera};
    const held = labSurfaceVolume.interacting;
    if (!held) labSurfaceVolume.slider(4);
    const orientation = [...view.orientation], angles = [...view.angles] as MarsSurfaceAngles;
    const controls = ['plotLower', 'plotUpper', 'plotSecondLower', 'plotSecondUpper',
                      'plotYLower', 'plotYUpper', 'plotScale', 'plotInputRange', 'plotZoom'];
    const saved = controls.map(id => {
        const node = document.getElementById(id) as HTMLInputElement & {marsZoomText?: unknown; marsZoomValue?: unknown};
        return {node, value: node.value, text: node.marsZoomText, exact: node.marsZoomValue};
    });
    const restore = (): void => {
        labSurfaceVolume.cancel();
        view.volume = labSurfaceVolume.clone(volume); view.camera = {...camera};
        view.orientation = [...orientation] as MarsSurfaceQuaternion; view.angles = [...angles];
        for (const entry of saved) {
            entry.node.value = entry.value;
            if (entry.text === undefined) delete entry.node.marsZoomText;
            else entry.node.marsZoomText = entry.text;
            if (entry.exact === undefined) delete entry.node.marsZoomValue;
            else entry.node.marsZoomValue = entry.exact;
        }
        labDOM.call('lab_plot_controls_sync');
        labSurfacePreview.draw(chart, view.angles);
    };
    const modifiers: WheelEventInit[] = [{}, {shiftKey: true}, {altKey: true}, {ctrlKey: true}];
    const send = (x: number, y: number, modifier: WheelEventInit): WheelEvent => {
        const screen = marsTransportSVGScreen(chart)!;
        const event = new WheelEvent('wheel', {clientX: screen.a * x + screen.c * y + screen.e,
            clientY: screen.b * x + screen.d * y + screen.f,
            deltaY: -100, bubbles: true, cancelable: true, ...modifier});
        chart.dispatchEvent(event);
        return event;
    };
    try {
        const frame = view.framePlan!, before = JSON.stringify(volume);
        for (const modifier of modifiers) {
            check(!send(0, 0, modifier).defaultPrevented, 'wheel outside the volume preserves page scrolling');
            check(JSON.stringify(view.volume) === before &&
                  view.orientation.every((value, i) => value === orientation[i]), 'outside wheel leaves the scene unchanged');
        }
        let gain: number | undefined;
        for (const modifier of modifiers) {
            const event = send(frame.width / 2, frame.height / 2, modifier);
            check(event.defaultPrevented, 'wheel inside the volume is captured with every globe modifier');
            await new Promise(requestAnimationFrame);
            labSurfaceVolume.cancel();
            const change = view.volume.exponent - volume.exponent;
            check(change > 0 && (gain === undefined || Math.abs(change - gain) < 1e-12),
                  'plain, Shift, Alt and Ctrl wheel all apply the same zoom');
            gain = change;
            check(view.orientation.every((value, i) => value === orientation[i]) &&
                  view.angles.every((value, i) => value === angles[i]), 'wheel zoom does not rotate the camera');
            restore();
        }
    } finally {
        restore();
        if (!held) labSurfaceVolume.slider(0);
    }
};
