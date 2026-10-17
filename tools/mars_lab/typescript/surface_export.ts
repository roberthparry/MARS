/**
 * @file surface_export.ts
 * @brief SVG output for retained native plot frames.
 *
 * Builds vector elements from native projections for settled curves and export.
 * Shares the interactive view's geometry without evaluating expressions or camera maths.
 */
/** Vector output for retained native frames; no projection mathematics lives here. */
class MarsSurfaceSVGWriter {
    /** @brief Namespace used for every generated vector element. */
    static readonly namespace = 'http://www.w3.org/2000/svg';

    /**
     * @brief Create and append an SVG element with literal attributes and optional text content.
     * Returns the appended node; callers own its lifetime. Text is never interpreted as markup.
     */
    static node<K extends keyof SVGElementTagNameMap>(parent: SVGElement, name: K,
        attributes: Record<string, string | number>, text?: string): SVGElementTagNameMap[K] {
        const element = document.createElementNS(this.namespace, name);
        for (const [key, value] of Object.entries(attributes)) element.setAttribute(key, String(value));
        if (text !== undefined) element.textContent = text;
        parent.appendChild(element);
        return element;
    }
}

/** Canvas-compatible axis sink with its own path and paint state. */
class MarsSurfaceSVGAxisContext implements MarsSurfaceAxisContext {
    private path = '';
    strokeStyle: string | CanvasGradient | CanvasPattern = '#94b8b1';
    fillStyle: string | CanvasGradient | CanvasPattern = '#b8ccc5';
    lineWidth = 1.2;
    font = '11px sans-serif';
    textAlign: CanvasTextAlign = 'center';

    /** @brief Bind an axis-painting sink to a caller-owned SVG group without clearing existing children. */
    constructor(private readonly parent: SVGGElement) {}

    /** @brief Accept the canvas clear operation as a no-op; each exported layer uses a fresh group. */
    clearRect(): void {}

    /** @brief Reset the buffered path without removing previously emitted SVG elements. */
    beginPath(): void { this.path = ''; }

    /** @brief Append a move command in the native frame's SVG coordinates. */
    moveTo(x: number, y: number): void { this.path += `M${x} ${y}`; }

    /** @brief Append a line command to the current buffered path. */
    lineTo(x: number, y: number): void { this.path += `L${x} ${y}`; }

    /** @brief Emit the buffered path using the current stroke colour and width, leaving the buffer intact. */
    stroke(): void {
        MarsSurfaceSVGWriter.node(this.parent, 'path', {d: this.path, fill: 'none',
            stroke: String(this.strokeStyle), 'stroke-width': this.lineWidth});
    }
    /**
     * @brief Emit a literal axis label using current alignment and fill colour.
     * Labels use the fixed 11-pixel sans-serif export font; canvas gradients and patterns
     * are not reproduced by this minimal axis sink.
     */
    fillText(text: string, x: number, y: number): void {
        MarsSurfaceSVGWriter.node(this.parent, 'text', {x, y, fill: String(this.fillStyle),
            'font-size': 11, 'font-family': 'sans-serif',
            'text-anchor': this.textAlign === 'left' ? 'start' : this.textAlign === 'right' ? 'end' : 'middle'}, text);
    }
}

/** Builds detached vector output from a connected view's retained native projection. */
class MarsSurfaceExporter {
    /**
     * @brief Build a detached SVG group containing projected geometry and axes.
     * Requires a connected view with a prepared native frame and projection callbacks.
     * Returns null otherwise; painter ordering is applied only to the exported copy.
     */
    layer(view: MarsSurfaceView | null): SVGGElement | null {
        if (!view?.project || !view.polygon || !view.framePlan || !view.paintAxes || !view.chart.isConnected) return null;
        const drawing = document.createElementNS(MarsSurfaceSVGWriter.namespace, 'g');
        const series = MarsSurfaceSVGWriter.node(drawing, 'g', {'data-series': '', stroke: '#193d46',
            'stroke-opacity': 0.35, 'stroke-width': 0.45, 'stroke-linejoin': 'round'});
        const polygon = view.polygon;
        // SVG has no depth buffer. Sort only for vector output, never during GPU interaction.
        const triangles = (view.framePlan.drawMesh ? view.primitives : []).map(triangle => ({
            ...polygon(triangle.points), colour: triangle.colour
        })).filter(triangle => triangle.points.length >= (view.chart.dataset.segments === 'true' ? 2 : 3));
        triangles.sort((a, b) => b.depth - a.depth);
        if (view.chart.dataset.segments === 'true') {
            let colour = '';
            let commands: string[] = [];
            const flush = (): void => {
                if (commands.length) MarsSurfaceSVGWriter.node(series, 'path', {d: commands.join(''), fill: 'none',
                    stroke: colour, 'stroke-width': 2, 'stroke-opacity': 1});
                commands = [];
            };
            for (const triangle of triangles) {
                if (colour !== triangle.colour) { flush(); colour = triangle.colour; }
                commands.push(triangle.points.map((p, i) => `${i ? 'L' : 'M'}${p[0]} ${p[1]}`).join(''));
            }
            flush();
        } else {
            for (const triangle of triangles) {
                MarsSurfaceSVGWriter.node(series, 'path', {
                    d: triangle.points.map((p, i) => `${i ? 'L' : 'M'}${p[0]} ${p[1]}`).join('') + 'Z',
                    fill: triangle.colour
                });
            }
        }
        view.paintAxes(new MarsSurfaceSVGAxisContext(MarsSurfaceSVGWriter.node(drawing, 'g', {})));
        return drawing;
    }

    /**
     * @brief Serialise a cloned plot figure with vector geometry and no internal data attributes.
     * Leaves the live chart unchanged. Returns null when the figure or prepared vector layer
     * is unavailable; callers receive an independent string rather than a retained DOM node.
     */
    svg(view: MarsSurfaceView | null): string | null {
        if (!view?.project || !view.chart.isConnected) return null;
        const figure = view.chart.closest('[data-plot-figure]');
        if (!figure) return null;
        const copy = figure.cloneNode(true);
        if (!marsTransportIsDOM(copy, 'Element')) return null;
        const chart = copy.querySelector<SVGSVGElement>('[data-surface]');
        const drawing = this.layer(view);
        if (!chart || !drawing) return null;
        chart.replaceChildren(drawing);
        chart.style.overflow = 'hidden';
        for (const element of [copy, ...copy.querySelectorAll('*')]) {
            for (const attribute of [...element.attributes])
                if (attribute.name.startsWith('data-')) element.removeAttribute(attribute.name);
        }
        return new XMLSerializer().serializeToString(copy);
    }
}
const labSurfaceExport: MarsSurfaceExporter = new MarsSurfaceExporter();
