/**
 * @file plot_palette.ts
 * @brief Theme colour grading shared by SVG, Canvas and WebGL mathematical plots.
 *
 * Maps the native blue, teal and sand display ramp onto the skin's three colour stops.
 * Geometry, expression values and native colour attributes remain unchanged. SVG style
 * overrides and a GPU colour matrix apply the same presentation to new and retained plots.
 */
namespace WorksheetPlotPalette {
    type RGB = [number, number, number];

    /** @brief Cached presentation colours and column-major WebGL transform for one theme. */
    interface Palette {
        skin: string;
        matrix: Float32Array;
        colours: Map<string, string>;
    }

    const chartSelector = 'svg[data-plot-chart], svg[data-surface]';
    const attributes = ['fill', 'stroke', 'color'] as const;
    const source: RGB[] = [[63 / 255, 107 / 255, 163 / 255],
                          [71 / 255, 181 / 255, 177 / 255],
                          [231 / 255, 197 / 255, 105 / 255]];
    let cached: Palette | null = null;

    function cross(a: RGB, b: RGB): RGB {
        return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
    }

    const inverse = [cross(source[1], source[2]), cross(source[2], source[0]), cross(source[0], source[1])];
    const determinant = source[0].reduce((sum, value, index) => sum + value * inverse[0][index], 0);

    function rgb(colour: string): RGB {
        const value = Number.parseInt(colour.slice(1), 16);
        return [(value >> 16 & 255) / 255, (value >> 8 & 255) / 255, (value & 255) / 255];
    }

    function current(): Palette {
        const root = document.documentElement, skin = root.dataset.skin || '0';
        if (cached?.skin === skin) return cached;
        const style = getComputedStyle(root);
        const read = (name: string, fallback: string): string => {
            const value = style.getPropertyValue(name).trim();
            return /^#[0-9a-f]{6}$/i.test(value) ? value : fallback;
        };
        const stops = [rgb(read('--plot-low', '#3f6ba3')), rgb(read('--plot-mid', '#47b5b1')),
                       rgb(read('--plot-high', '#e7c569'))];
        const matrix = new Float32Array(9);
        for (let column = 0; column < 3; ++column) {
            for (let row = 0; row < 3; ++row) {
                matrix[column * 3 + row] = skin === '0' ? Number(row === column) :
                    stops.reduce((sum, stop, index) => sum + stop[row] * inverse[index][column] / determinant, 0);
            }
        }
        const colours = new Map<string, string>();
        if (skin !== '0') {
            for (const [original, property] of [['#94b8b1', '--plot-axis'], ['#b8ccc5', '--plot-label'],
                                               ['#82aaa3', '--plot-grid'], ['#193d46', '--plot-wire']]) {
                colours.set(original, read(property, original));
            }
        }
        return cached = {skin, matrix, colours};
    }

    /** @brief Borrow the current colour transform; callers must not modify the cached matrix. */
    export function matrix(): Float32Array {
        return current().matrix;
    }

    /** @brief Grade a native hexadecimal plot colour, preserving alpha and non-colour SVG values. */
    export function colour(original: string): string {
        const palette = current();
        if (palette.skin === '0' || !/^#[0-9a-f]{6}(?:[0-9a-f]{2})?$/i.test(original)) return original;
        const key = original.toLowerCase(), saved = palette.colours.get(key);
        if (saved) return saved;
        const opaque = palette.colours.get(key.slice(0, 7));
        if (opaque) return opaque + key.slice(7);
        const channels = rgb(key), matrix = palette.matrix;
        let result = '#';
        for (let row = 0; row < 3; ++row) {
            const channel = channels.reduce((sum, value, column) => sum + matrix[column * 3 + row] * value, 0);
            result += Math.round(Math.max(0, Math.min(1, channel)) * 255).toString(16).padStart(2, '0');
        }
        result += key.slice(7);
        palette.colours.set(key, result);
        return result;
    }

    /** @brief Apply exportable inline presentation while retaining original native SVG attributes. */
    export function apply(root: Element): void {
        for (const node of [root, ...root.querySelectorAll('[fill], [stroke], [color]')]) {
            if (!(node instanceof SVGElement) || node.closest('defs')) continue;
            for (const attribute of attributes) {
                const original = node.getAttribute(attribute);
                if (original && /^#[0-9a-f]{6}(?:[0-9a-f]{2})?$/i.test(original)) {
                    const graded = colour(original);
                    if (graded === original) node.style.removeProperty(attribute);
                    else node.style.setProperty(attribute, graded);
                }
            }
        }
    }

    new MutationObserver(() => {
        for (const chart of document.querySelectorAll(chartSelector)) apply(chart);
        window.dispatchEvent(new Event('mars-plot-palette-change'));
    }).observe(document.documentElement, {attributes: true, attributeFilter: ['data-skin']});

    // Only newly inserted mathematical plots are visited; globe imagery and other artwork are excluded.
    new MutationObserver(records => {
        for (const record of records) {
            for (const node of record.addedNodes) {
                if (!(node instanceof Element)) continue;
                if (node.closest(chartSelector)) apply(node);
                else for (const chart of node.querySelectorAll(chartSelector)) apply(chart);
            }
        }
    }).observe(document.body, {childList: true, subtree: true});
    for (const chart of document.querySelectorAll(chartSelector)) apply(chart);
}
