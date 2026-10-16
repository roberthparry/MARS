/** Vector export and idle line views from the retained camera and native projection. */
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
    function layer(view) {
        if (!view?.project || !view.chart.isConnected) return null;
        const drawing = document.createElementNS(ns, 'g');
        const series = node(drawing, 'g', {'data-series': '', stroke: '#193d46', 'stroke-opacity': 0.35,
            'stroke-width': 0.45, 'stroke-linejoin': 'round'});
        // SVG has no depth buffer. Sort only for vector output, never during WebGL interaction.
        const triangles = (view.framePlan.drawMesh ? view.primitives : []).map(triangle => ({
            ...view.polygon(triangle.points), colour: triangle.colour
        })).filter(triangle => triangle.points.length >= (view.chart.dataset.segments === 'true' ? 2 : 3));
        triangles.sort((a, b) => b.depth - a.depth);
        if (view.chart.dataset.segments === 'true') {
            // Batch adjacent same-colour segments without joining across native gaps.
            let colour, commands = [];
            const flush = () => {
                if (commands.length) node(series, 'path', {d: commands.join(''), fill: 'none',
                    stroke: colour, 'stroke-width': 2, 'stroke-opacity': 1});
                commands = [];
            };
            for (const triangle of triangles) {
                if (colour !== triangle.colour) { flush(); colour = triangle.colour; }
                commands.push(triangle.points.map((p, i) => `${i ? 'L' : 'M'}${p[0]} ${p[1]}`).join(''));
            }
            flush();
            view.paintAxes(axisContext(node(drawing, 'g', {})));
            return drawing;
        }
        for (const triangle of triangles) {
            const points = triangle.points;
            node(series, 'path', {d: points.map((p, i) => `${i ? 'L' : 'M'}${p[0]} ${p[1]}`).join('') + 'Z',
                fill: triangle.colour});
        }
        view.paintAxes(axisContext(node(drawing, 'g', {})));
        return drawing;
    }
    function svg(view) {
        if (!view?.project || !view.chart.isConnected) return null;
        const figure = view.chart.closest('[data-plot-figure]');
        if (!figure) return null;
        const copy = figure.cloneNode(true), chart = copy.querySelector('[data-surface]');
        chart.replaceChildren(layer(view));
        chart.style.overflow = 'hidden';
        for (const element of [copy, ...copy.querySelectorAll('*')]) {
            for (const attribute of [...element.attributes])
                if (attribute.name.startsWith('data-')) element.removeAttribute(attribute.name);
        }
        return new XMLSerializer().serializeToString(copy);
    }
    return Object.freeze({svg, layer});
})();
