/**
 * @file surface_mesh_checks.ts
 * @brief Native retained-mesh decoding and vertex-layout regressions.
 *
 * Supplies small SVG fixtures directly to WebAssembly, checking geometry admission,
 * fan triangulation, colour expansion and temporary-handle lifetime without a GPU.
 */
interface Window {
    /** @brief Validate native display-metadata decoding through the real browser bridge. */
    checkLabSurfaceMesh(): void;
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
};
