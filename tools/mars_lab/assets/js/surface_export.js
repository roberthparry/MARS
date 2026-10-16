/** On-demand vector export of the retained mesh, camera and native axis labels. */
const labSurfaceExport = (() => {
    const ns = 'http://www.w3.org/2000/svg';
    function node(parent, name, attributes, text) {
        const element = document.createElementNS(ns, name);
        for (const [key, value] of Object.entries(attributes)) element.setAttribute(key, String(value));
        if (text !== undefined) element.textContent = text;
        parent.appendChild(element);
        return element;
    }
    function axisContext(parent) {
        let path = '';
        return {
            clearRect() {},
            beginPath() { path = ''; },
            moveTo(x, y) { path += `M${x} ${y}`; },
            lineTo(x, y) { path += `L${x} ${y}`; },
            stroke() { node(parent, 'path', {d: path, fill: 'none', stroke: this.strokeStyle,
                'stroke-width': this.lineWidth}); },
            fillText(text, x, y) {
                node(parent, 'text', {x, y, fill: this.fillStyle, 'font-size': 11, 'font-family': 'sans-serif',
                    'text-anchor': this.textAlign === 'left' ? 'start' : this.textAlign === 'right' ? 'end' : 'middle'}, text);
            }
        };
    }
    function svg(view) {
        if (!view?.project || !view.chart.isConnected) return null;
        const figure = view.chart.closest('[data-plot-figure]');
        if (!figure) return null;
        const copy = figure.cloneNode(true), chart = copy.querySelector('[data-surface]');
        chart.replaceChildren();
        chart.style.overflow = 'hidden';
        const series = node(chart, 'g', {'data-series': '', stroke: '#193d46', 'stroke-opacity': 0.35,
            'stroke-width': 0.45, 'stroke-linejoin': 'round'});
        // SVG has no depth buffer. Sort only for export, never during WebGL interaction.
        const triangles = (view.framePlan.drawMesh ? view.triangles : []).map(triangle => ({
            ...view.polygon(triangle.points), colour: triangle.colour
        })).filter(triangle => triangle.points.length >= 3);
        triangles.sort((a, b) => b.depth - a.depth);
        for (const triangle of triangles) {
            const points = triangle.points;
            node(series, 'path', {d: points.map((p, i) => `${i ? 'L' : 'M'}${p[0]} ${p[1]}`).join('') + 'Z',
                fill: triangle.colour});
        }
        view.paintAxes(axisContext(node(chart, 'g', {})));
        for (const element of [copy, ...copy.querySelectorAll('*')]) {
            for (const attribute of [...element.attributes])
                if (attribute.name.startsWith('data-')) element.removeAttribute(attribute.name);
        }
        return new XMLSerializer().serializeToString(copy);
    }
    return Object.freeze({svg});
})();
