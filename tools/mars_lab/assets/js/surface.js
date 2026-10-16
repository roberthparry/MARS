/**
 * Retained native surface geometry for local camera interaction. No expressions
 * are evaluated here: normalised vertices, colours and labels come from MARS.
 * WebGL depth-tests complete triangles; a painter-ordered canvas is the fallback.
 * The SVG remains the authoritative idle view and export representation.
 */
const labSurfacePreview = (() => {
    let current = null;

    function gpu(canvas, vertices) {
        let gl;
        try { gl = canvas.getContext('webgl', {alpha: true, antialias: true, preserveDrawingBuffer: false}); }
        catch (_) { return null; }
        if (!gl) return null;
        const shaders = [];
        let program, buffer;
        try {
            const precision = gl.getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.HIGH_FLOAT)?.precision > 0 ?
                'highp' : 'mediump';
            for (const [type, source] of [[gl.VERTEX_SHADER, `
                attribute vec3 position; attribute vec3 colour; attribute vec3 corner;
                uniform mat4 camera; uniform vec3 domainScale; uniform vec3 domainOffset;
                varying mediump vec3 shade; varying mediump vec3 edge; varying ${precision} vec3 coordinate;
                void main() {
                    coordinate = position * domainScale + domainOffset;
                    gl_Position = camera * vec4(coordinate, 1.0); shade = colour; edge = corner;
                }
            `], [gl.FRAGMENT_SHADER, `
                precision ${precision} float;
                varying mediump vec3 shade; varying mediump vec3 edge; varying ${precision} vec3 coordinate;
                void main() {
                    float wire = smoothstep(0.0, 0.025, min(edge.x, min(edge.y, edge.z)));
                    gl_FragColor = vec4(mix(shade * 0.65, shade, wire), 1.0);
                }
            `]]) {
                const shader = gl.createShader(type);
                shaders.push(shader);
                gl.shaderSource(shader, source);
                gl.compileShader(shader);
                if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error('Surface shader unavailable');
            }
            program = gl.createProgram();
            for (const shader of shaders) gl.attachShader(program, shader);
            gl.linkProgram(program);
            if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error('Surface program unavailable');
            gl.useProgram(program);
            buffer = gl.createBuffer();
            gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
            gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
            ['position', 'colour', 'corner'].forEach((name, i) => {
                const location = gl.getAttribLocation(program, name);
                gl.enableVertexAttribArray(location);
                gl.vertexAttribPointer(location, 3, gl.FLOAT, false, 36, i * 12);
            });
            const camera = gl.getUniformLocation(program, 'camera');
            const domainScale = gl.getUniformLocation(program, 'domainScale');
            const domainOffset = gl.getUniformLocation(program, 'domainOffset');
            gl.enable(gl.DEPTH_TEST);
            gl.disable(gl.CULL_FACE);
            return {
                draw(matrix, scaling, offset) {
                    if (gl.isContextLost()) return false;
                    gl.viewport(0, 0, canvas.width, canvas.height);
                    gl.clearColor(0, 0, 0, 0);
                    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
                    gl.uniformMatrix4fv(camera, false, matrix);
                    gl.uniform3fv(domainScale, scaling);
                    gl.uniform3fv(domainOffset, offset);
                    gl.drawArrays(gl.TRIANGLES, 0, vertices.length / 9);
                    return true;
                },
                dispose() {
                    gl.deleteBuffer(buffer);
                    gl.deleteProgram(program);
                    for (const shader of shaders) gl.deleteShader(shader);
                    gl.getExtension('WEBGL_lose_context')?.loseContext();
                }
            };
        } catch (_) {
            if (buffer) gl.deleteBuffer(buffer);
            if (program) gl.deleteProgram(program);
            for (const shader of shaders) gl.deleteShader(shader);
            gl.getExtension('WEBGL_lose_context')?.loseContext();
            return null;
        }
    }

    function clear() {
        if (!current) return;
        for (const [node, visibility] of current.hidden) node.style.visibility = visibility;
        current.hidden = [];
        current.overlay.style.display = 'none';
        delete current.chart.dataset.localRenderer;
        delete current.chart.dataset.localCamera;
    }

    function dispose() {
        clear();
        if (!current) return;
        current.gpu?.dispose();
        current.observer.disconnect();
        current.overlay.remove();
        current = null;
    }

    function prepare(chart) {
        if (current?.chart === chart) return current;
        dispose();
        if (!chart?.isConnected || !chart.dataset.units) return null;
        const triangles = [], data = [];
        for (const path of chart.querySelectorAll('[data-vertices]')) {
            const points = path.dataset.vertices.split(' ').map(Number);
            if (points.length < 9 || points.length > 15 || points.length % 3 || !points.every(Number.isFinite))
                return null;
            const rgb = Number.parseInt(path.getAttribute('fill').slice(1), 16);
            const colour = [(rgb >> 16 & 255) / 255, (rgb >> 8 & 255) / 255, (rgb & 255) / 255];
            for (let i = 3; i + 3 < points.length; i += 3) {
                const triangle = [points.slice(0, 3), points.slice(i, i + 3), points.slice(i + 3, i + 6)];
                triangles.push({points: triangle, colour: path.getAttribute('fill'), depth: 0});
                triangle.forEach((point, j) => data.push(...point.map(x => x - 0.5), ...colour,
                                                       Number(j === 0), Number(j === 1), Number(j === 2)));
            }
        }
        if (!triangles.length) return null;
        const units = chart.dataset.units.split(' ').map(Number);
        if (units.length !== 3 || !units.every(x => Number.isFinite(x) && x > 0)) return null;
        const width = chart.viewBox.baseVal.width, height = chart.viewBox.baseVal.height;
        // Keep composited canvases out of SVG foreignObject: mobile engines can
        // position those layers outside the fitted SVG when they first appear.
        const host = chart.closest('.rendered');
        if (!host) return null;
        const overlay = document.createElement('div');
        overlay.dataset.surfaceOverlay = '';
        overlay.style.cssText = `position:absolute;left:0;top:0;width:${width}px;height:${height}px;` +
            'transform-origin:0 0;pointer-events:none;display:none;overflow:hidden';
        const holder = document.createElement('div');
        holder.style.cssText = `position:relative;width:${width}px;height:${height}px;` +
            'transform-origin:0 0;pointer-events:none';
        let canvas = document.createElement('canvas');
        const vertices = new Float32Array(data);
        const renderer = gpu(canvas, vertices);
        if (!renderer) canvas = document.createElement('canvas');
        const labels = document.createElement('canvas');
        const ratio = Math.min(window.devicePixelRatio || 1, 2);
        for (const layer of [canvas, labels]) {
            layer.width = Math.round(width * ratio);
            layer.height = Math.round(height * ratio);
            layer.style.cssText = `position:absolute;left:0;top:0;width:${width}px;height:${height}px`;
            holder.appendChild(layer);
        }
        const context = renderer ? null : canvas.getContext('2d');
        const ink = labels.getContext('2d');
        if ((!renderer && !context) || !ink) { renderer?.dispose(); return null; }
        context?.scale(ratio, ratio);
        ink.scale(ratio, ratio);
        overlay.appendChild(holder);
        host.appendChild(overlay);
        const ticks = [...chart.querySelectorAll('[data-tick-fraction]')].map(node => ({
            axis: Number(node.dataset.plotTick), fraction: Number(node.dataset.tickFraction),
            label: node.nextElementSibling?.tagName === 'text' ? node.nextElementSibling.textContent : ''
        }));
        const names = [0, 1, 2].map(axis => chart.querySelector(`[data-axis-name="${axis}"]`)?.textContent || '');
        const observer = new MutationObserver(() => { if (!chart.isConnected) dispose(); });
        observer.observe(document.getElementById('plotRendered'), {childList: true, subtree: true});
        current = {chart, host, holder, triangles, vertices, units, width, height, overlay, gpu: renderer, context, ink,
                   ticks, names, observer, hidden: []};
        canvas.addEventListener('webglcontextlost', () => { if (current?.overlay === overlay) clear(); });
        return current;
    }

    function draw(chart, angles, bounds = null) {
        const view = prepare(chart);
        if (!view || !angles.every(Number.isFinite)) return false;
        // Map the native chart's coordinate system into the HTML scroll content,
        // including nested SVG headings, mobile fitting, borders and scroll offsets.
        const screen = chart.getScreenCTM(), box = view.host.getBoundingClientRect();
        if (!screen) return false;
        const tx = screen.e - box.left - view.host.clientLeft + view.host.scrollLeft;
        const ty = screen.f - box.top - view.host.clientTop + view.host.scrollTop;
        const xs = [0, screen.a * view.width, screen.c * view.height,
                    screen.a * view.width + screen.c * view.height];
        const ys = [0, screen.b * view.width, screen.d * view.height,
                    screen.b * view.width + screen.d * view.height];
        const overlayLeft = Math.min(...xs), overlayTop = Math.min(...ys);
        // Size the clipping layer in screen pixels: transforming an 800px-wide
        // outer box alone can still enlarge the mobile scrollable overflow area.
        Object.assign(view.overlay.style, {
            left: `${tx + overlayLeft}px`, top: `${ty + overlayTop}px`,
            width: `${Math.max(...xs) - overlayLeft}px`, height: `${Math.max(...ys) - overlayTop}px`
        });
        view.holder.style.transform =
            `matrix(${screen.a},${screen.b},${screen.c},${screen.d},${-overlayLeft},${-overlayTop})`;
        const {width, height, vertices} = view;
        const units = view.units;
        const scaling = [1, 1, 1], offset = [0, 0, 0];
        if (bounds) {
            const keys = ['xMin', 'xMax', 'yMin', 'yMax', 'zMin', 'zMax'];
            const original = keys.map(key => Number(chart.dataset[key]));
            if (bounds.length !== 6 || !bounds.every(Number.isFinite) || !original.every(Number.isFinite)) return false;
            for (let axis = 0; axis < 3; axis++) {
                const lo = bounds[axis * 2], hi = bounds[axis * 2 + 1], span = hi - lo;
                // Unrepresentable previews wait for native refinement on release.
                if (!(span > Math.max(Math.abs(lo), Math.abs(hi), 1) * Number.EPSILON * 4096)) return false;
                scaling[axis] = (original[axis * 2 + 1] - original[axis * 2]) / span;
                offset[axis] = ((original[axis * 2] - lo) + (original[axis * 2 + 1] - hi)) / (2 * span);
                if (!(scaling[axis] > 0 && scaling[axis] < 10000) || !Number.isFinite(offset[axis])) return false;
            }
        }
        const [a, e, r] = angles.map(x => x * Math.PI / 180);
        const ca = Math.cos(a), sa = Math.sin(a), ce = Math.cos(e), se = Math.sin(e);
        const cr = Math.cos(r), sr = Math.sin(r);
        const mesh = chart.dataset.mesh === 'true';
        const extent = Math.abs(ca) + Math.abs(sa);
        const labelWidth = Number(chart.dataset.labelWidth) || 0;
        const cameraMargin = Math.min(Math.max(80, labelWidth + 20), (width - 80) / 2);
        const sx = mesh ? 1 : (width - 2 * cameraMargin) / extent;
        const sy = mesh ? 1 : (height - 160) / (se * extent + ce);
        const horizontal = [sx * ca, -sx * sa, 0];
        const vertical = [-sy * se * sa, -sy * se * ca, -sy * ce];
        const x = units.map((u, i) => u * (cr * horizontal[i] - sr * vertical[i]));
        const y = units.map((u, i) => u * (sr * horizontal[i] + cr * vertical[i]));
        const z = [ce * sa * units[0], ce * ca * units[1], -se * units[2]];
        const dot = (row, p) => row[0] * (p[0] - 0.5) + row[1] * (p[1] - 0.5) + row[2] * (p[2] - 0.5);
        const axisPoint = (axis, t) => {
            const p = [Number(sa < 0), Number(ca < 0), 0];
            if (axis === 2) { p[0] = Number(ca < 0); p[1] = Number(sa >= 0); }
            p[axis] = t;
            return p;
        };
        let left = Infinity, right = -Infinity, top = Infinity, bottom = -Infinity;
        function include(px, py) {
            left = Math.min(left, px); right = Math.max(right, px);
            top = Math.min(top, py); bottom = Math.max(bottom, py);
        }
        // Keep the original camera fit throughout zoom: magnify the scene, not
        // a newly fitted/clipped coordinate box at every pointer movement.
        for (let i = 0; i < vertices.length; i += 9)
            include(x[0] * vertices[i] + x[1] * vertices[i + 1] + x[2] * vertices[i + 2],
                    y[0] * vertices[i] + y[1] * vertices[i + 1] + y[2] * vertices[i + 2]);
        for (let axis = 0; axis < 3; axis++)
            for (const t of [0, 1]) { const p = axisPoint(axis, t); include(dot(x, p), dot(y, p)); }
        const margin = Math.min(Math.max(45, labelWidth + 25), (width - 80) / 2);
        const scale = Math.min((width - 2 * margin) / (right - left), (height - 100) / (bottom - top));
        const cx = width / 2 - scale * (left + right) / 2;
        const cy = height / 2 - scale * (top + bottom) / 2;
        const transform = p => p.map((v, i) => (v - 0.5) * scaling[i] + offset[i] + 0.5);
        const project = p => [cx + scale * dot(x, transform(p)), cy + scale * dot(y, transform(p))];
        if (view.gpu) {
            const matrix = new Float32Array(16);
            for (let i = 0; i < 3; i++) {
                matrix[i * 4] = 2 * scale * x[i] / width;
                matrix[i * 4 + 1] = -2 * scale * y[i] / height;
                matrix[i * 4 + 2] = z[i] / (2 * Math.max(...scaling));
            }
            matrix[12] = 2 * cx / width - 1;
            matrix[13] = 1 - 2 * cy / height;
            matrix[14] = -z.reduce((sum, v, i) => sum + v * offset[i], 0) / (2 * Math.max(...scaling));
            matrix[15] = 1;
            if (!view.gpu.draw(matrix, scaling, offset)) { dispose(); return false; }
        } else {
            const ctx = view.context;
            ctx.clearRect(0, 0, width, height);
            const triangles = view.triangles;
            for (const triangle of triangles)
                triangle.depth = triangle.points.reduce((sum, p) => sum + dot(z, transform(p)), 0);
            triangles.sort((a, b) => b.depth / b.points.length - a.depth / a.points.length);
            ctx.strokeStyle = '#193d4659';
            ctx.lineWidth = 0.45;
            for (const triangle of triangles) {
                ctx.beginPath();
                triangle.points.forEach((p, i) => { const q = project(p); i ? ctx.lineTo(...q) : ctx.moveTo(...q); });
                ctx.closePath();
                ctx.fillStyle = triangle.colour;
                ctx.fill(); ctx.stroke();
            }
        }
        const ctx = view.ink;
        ctx.clearRect(0, 0, width, height);
        ctx.strokeStyle = '#94b8b1'; ctx.fillStyle = '#b8ccc5'; ctx.lineWidth = 1.2;
        ctx.font = '11px sans-serif'; ctx.textAlign = 'center';
        for (let axis = 0; axis < 3; axis++) {
            const start = project(axisPoint(axis, 0)), end = project(axisPoint(axis, 1));
            const length = Math.hypot(end[0] - start[0], end[1] - start[1]);
            const dx = (end[0] - start[0]) / length, dy = (end[1] - start[1]) / length;
            ctx.beginPath(); ctx.moveTo(...start); ctx.lineTo(end[0] + dx * 12, end[1] + dy * 12); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(end[0] + dx * 5 - dy * 3.5, end[1] + dy * 5 + dx * 3.5);
            ctx.lineTo(end[0] + dx * 12, end[1] + dy * 12);
            ctx.lineTo(end[0] + dx * 5 + dy * 3.5, end[1] + dy * 5 - dx * 3.5); ctx.stroke();
            ctx.textAlign = dx < -0.1 ? 'right' : dx > 0.1 ? 'left' : 'center';
            ctx.fillText(view.names[axis], end[0] + dx * 26, end[1] + dy * 26 + 4);
            const centre = project([0.5, 0.5, 0]);
            let nx = dy, ny = -dx;
            if (nx * ((start[0] + end[0]) / 2 - centre[0]) + ny * ((start[1] + end[1]) / 2 - centre[1]) < 0) {
                nx = -nx; ny = -ny;
            }
            if (axis === 2) { nx = -cr; ny = -sr; }
            const side = Math.abs(nx) < 1e-9 ? 0 : Math.sign(nx);
            ctx.textAlign = side < 0 ? 'right' : side > 0 ? 'left' : 'center';
            for (const tick of view.ticks) {
                if (tick.axis !== axis || !tick.label) continue;
                const p = project(axisPoint(axis, tick.fraction));
                ctx.beginPath(); ctx.moveTo(...p); ctx.lineTo(p[0] + nx * 6, p[1] + ny * 6); ctx.stroke();
                ctx.fillText(tick.label, p[0] + nx * 13 + side * 3, p[1] + ny * 13 + 4);
            }
        }
        ctx.textAlign = 'center';
        if (bounds) ctx.fillText('Existing samples — new region refines when the gesture ends', width / 2, height - 12);
        if (!view.hidden.length) {
            for (const child of chart.children) {
                if (child.tagName === 'g' || child.tagName === 'path') {
                    view.hidden.push([child, child.style.visibility]);
                    child.style.visibility = 'hidden';
                }
            }
        }
        view.overlay.style.display = '';
        chart.dataset.localRenderer = view.gpu ? 'webgl' : 'canvas';
        chart.dataset.localCamera = angles.join(',');
        return true;
    }
    window.addEventListener('pagehide', dispose);
    return Object.freeze({prepare, draw, clear, dispose});
})();
