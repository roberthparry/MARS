/**
 * @file surface_export.ts
 * @brief Browser SVG elements and serialisation for retained native plot frames.
 *
 * C/WebAssembly constructs path commands and groups adjacent curve segments.
 * This adapter creates detached SVG nodes, applies literal attributes and text,
 * and serialises a cleaned copy without changing the live plot.
 */

/** Literal SVG attributes accepted by the browser writer. */
interface MarsSurfaceSVGAttributes {
    [name: string]: string | number;
}

/** One native-built path with its original plot colour. */
interface MarsSurfaceSVGPath {
    path: string;
    colour: string;
}

/** Native series paths and axis paths, ready for browser element creation. */
interface MarsSurfaceSVGPlan {
    series: MarsSurfaceSVGPath[];
    axes: string[];
    solids: {path: string; shade: number; axis: number; cone: boolean}[];
}

/** Native capability used to construct a vector drawing plan. */
interface MarsSurfaceExportNative {
    /** @brief Invoke native code; the exporter identifies the returned plan type. */
    call(name: string, ...args: unknown[]): unknown;
}

/** Creates SVG nodes without parsing markup or interpreting plot geometry. */
class MarsSurfaceSVGWriter {
    /** @brief Namespace used for every generated vector element. */
    static readonly namespace = 'http://www.w3.org/2000/svg';

    /** @brief Append an SVG node with literal attributes and optional plain text. */
    static node(parent: SVGElement, name: string,
                attributes: MarsSurfaceSVGAttributes, text?: string): SVGElement {
        const element = document.createElementNS(this.namespace, name);
        for (const name of Object.keys(attributes)) {
            element.setAttribute(name, String(attributes[name]));
        }
        if (text !== undefined) {
            element.textContent = text;
        }
        parent.appendChild(element);
        return element;
    }
}

/** Owns the browser side of settled vector drawings and SVG export. */
class MarsSurfaceExporter {
    /** @brief Retain the native SVG planning capability; no plot state is owned here. */
    constructor(private readonly native: MarsSurfaceExportNative) {}

    /**
     * @brief Build a detached SVG group from a connected view's native projection plan.
     * Native painter ordering is shared with Canvas. Path commands and curve batching
     * are native decisions; this method only creates and styles browser elements.
     */
    layer(view: MarsSurfaceView | null): SVGGElement | null {
        if (!view?.chart.isConnected) return null;
        if (!view.polygons || !view.framePlan) return null; // View is not fully prepared.
        const segments = view.chart.dataset.segments === 'true';
        const plan = this.native.call('lab_surface_svg_plan', view.polygons(),
            view.framePlan.paths.slice(view.framePlan.axisPathCount ?? 0), Number(segments),
            view.framePlan.solids ?? []) as MarsSurfaceSVGPlan;
        const drawing = document.createElementNS(MarsSurfaceSVGWriter.namespace, 'g');
        this.appendSeries(drawing, plan.series, segments);
        for (const face of plan.solids) {
            MarsSurfaceSVGWriter.node(drawing, 'path', {
                d: face.path, fill: '#94b8b1', stroke: '#94b8b1', 'stroke-width': 0.35,
                'stroke-linejoin': 'round', 'data-axis-colour': '#94b8b1', 'data-axis-shade': face.shade
            });
        }
        this.appendAxes(drawing, plan.axes, view.framePlan.texts,
            view.framePlan.pathWidths?.slice(view.framePlan.axisPathCount ?? 0) ?? []);
        WorksheetPlotPalette.apply(drawing);
        return drawing;
    }

    private appendSeries(drawing: SVGGElement, paths: MarsSurfaceSVGPath[], segments: boolean): void {
        const series = MarsSurfaceSVGWriter.node(drawing, 'g', {
            'data-series': '', stroke: '#193d46', 'stroke-opacity': 0.35,
            'stroke-width': 0.45, 'stroke-linejoin': 'round'
        });
        for (const path of paths) {
            if (segments) {
                MarsSurfaceSVGWriter.node(series, 'path', {
                    d: path.path, fill: 'none', stroke: path.colour,
                    'stroke-width': 2, 'stroke-opacity': 1
                });
            } else {
                MarsSurfaceSVGWriter.node(series, 'path', {d: path.path, fill: path.colour});
            }
        }
    }

    private appendAxes(drawing: SVGGElement, paths: string[], texts: MarsSurfaceText[], widths: number[]): void {
        const axes = MarsSurfaceSVGWriter.node(drawing, 'g', {});
        for (const [index, path] of paths.entries()) {
            MarsSurfaceSVGWriter.node(axes, 'path', {
                d: path, fill: 'none', stroke: '#94b8b1', 'stroke-width': widths[index] ?? 1.4
            });
        }
        for (const text of texts) {
            const anchor = text.align === 'left' ? 'start' : text.align === 'right' ? 'end' : 'middle';
            MarsSurfaceSVGWriter.node(axes, 'text', {
                x: text.x, y: text.y, fill: '#b8ccc5', 'font-size': text.size ?? 13,
                'font-weight': text.bold ? 600 : 400, stroke: '#10161d', 'stroke-width': 2.8,
                'stroke-linejoin': 'round', 'paint-order': 'stroke',
                'font-family': 'sans-serif', 'text-anchor': anchor
            }, text.text);
        }
    }

    private removeMetadata(root: Element): void {
        const elements = [root, ...root.querySelectorAll('*')];
        for (const element of elements) {
            for (const attribute of [...element.attributes]) {
                if (attribute.name.startsWith('data-')) {
                    element.removeAttribute(attribute.name);
                }
            }
        }
    }

    /**
     * @brief Serialise a cloned plot figure without internal data attributes.
     * Leaves the live chart unchanged. Returns null when its figure or drawing
     * is unavailable; the returned string owns no browser resources.
     */
    svg(view: MarsSurfaceView | null): string | null {
        if (!view || !view.chart.isConnected) {
            return null;
        }
        const figure = view.chart.closest('[data-plot-figure]');
        if (!figure) {
            return null;
        }
        const copy = figure.cloneNode(true);
        if (!marsTransportIsDOM(copy, 'Element')) {
            return null;
        }
        const chart = copy.querySelector('[data-surface]');
        const drawing = this.layer(view);
        if (!marsTransportIsDOM(chart, 'SVGSVGElement') || !drawing) {
            return null;
        }
        chart.replaceChildren(drawing);
        chart.style.overflow = 'hidden';
        WorksheetPlotPalette.apply(copy);
        this.removeMetadata(copy);
        return new XMLSerializer().serializeToString(copy);
    }
}

/** @brief Shared page-wide SVG exporter; each call returns a detached drawing or independent string. */
const labSurfaceExport: MarsSurfaceExporter = new MarsSurfaceExporter(labDOM);
