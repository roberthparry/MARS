/**
 * Retained native surface geometry for local camera interaction. No expressions
 * are evaluated here: normalised vertices, colours and labels come from MARS.
 * WebGL depth-tests complete triangles; a painter-ordered canvas is the fallback.
 * WebGL remains visible at rest. SVG is generated on demand for vector export.
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
                    coordinate = (position - vec3(0.5)) * domainScale + domainOffset;
                    gl_Position = camera * vec4(coordinate, 1.0); shade = colour; edge = corner;
                }
            `], [gl.FRAGMENT_SHADER, `
                precision ${precision} float;
                varying mediump vec3 shade; varying mediump vec3 edge; varying ${precision} vec3 coordinate;
                void main() {
                    if (any(greaterThan(abs(coordinate), vec3(0.5000001)))) discard;
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
                draw(matrix, scaling, offset, visible) {
                    if (gl.isContextLost()) return false;
                    gl.viewport(0, 0, canvas.width, canvas.height);
                    gl.clearColor(0, 0, 0, 0);
                    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
                    gl.uniformMatrix4fv(camera, false, matrix);
                    gl.uniform3fv(domainScale, scaling);
                    gl.uniform3fv(domainOffset, offset);
                    gl.drawArrays(gl.TRIANGLES, 0, visible ? vertices.length / 9 : 0);
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

    function clear(force = false) {
        if (!current) return;
        if (!force && current.chart.isConnected) {
            draw(current.chart, current.angles);
            return;
        }
        for (const [node, visibility] of current.hidden) node.style.visibility = visibility;
        current.hidden = [];
        current.overlay.style.display = 'none';
        delete current.chart.dataset.localRenderer;
        delete current.chart.dataset.localCamera;
    }

    function dispose() {
        clear(true);
        if (!current) return;
        cancelAnimationFrame(current.frame);
        current.gpu?.dispose();
        current.observer.disconnect();
        current.resizeObserver?.disconnect();
        current.overlay.remove();
        current = null;
    }

    function signature(chart) {
        return JSON.stringify([chart.closest('[data-plot-figure]')?.querySelector('title')?.textContent,
            ...['exactXMin', 'exactXMax', 'exactYMin', 'exactYMax', 'exactZMin', 'exactZMax', 'units']
                .map(key => chart.dataset[key])]);
    }

    function identity(chart) {
        return chart.closest('[data-plot-figure]')?.querySelector('title')?.textContent || '';
    }

    function snapshot() {
        return current ? {key: signature(current.chart), identity: identity(current.chart), camera: {...current.camera},
            orientation: [...current.orientation], angles: [...current.angles],
            volume: labSurfaceVolume.clone(current.volume)} : null;
    }

    // Canvas pixels and GPU resources cannot survive an innerHTML snapshot.
    function captureResult(capture) {
        const view = current, saved = snapshot();
        if (!view) return capture();
        clear(true);
        view.overlay.remove();
        try {
            const state = capture();
            if (state) state.surfaceView = saved;
            return state;
        } finally {
            if (current === view && view.chart.isConnected) {
                view.host.appendChild(view.overlay);
                draw(view.chart, view.angles);
            }
        }
    }

    function restoreResult(saved) {
        const chart = document.querySelector('#plotRendered [data-surface]');
        if (!chart) return;
        // Mode controls and fitted card dimensions settle after DOM restoration.
        requestAnimationFrame(() => {
            if (!chart.isConnected) return;
            const view = prepare(chart, saved);
            if (view) draw(chart, view.angles);
        });
    }

    function syncZoom(view) {
        labDOM.call('lab_surface_zoom_sync', view);
    }

    function prepare(chart, saved = null, software = false) {
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
                triangle.forEach((point, j) => data.push(...point, ...colour,
                                                       Number(j === 0), Number(j === 1), Number(j === 2)));
            }
        }
        if (!triangles.length && chart.dataset.empty !== 'true') return null;
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
        const renderer = software ? null : gpu(canvas, vertices);
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
                   ticks, names, observer, hidden: [], camera: saved && (saved.key === signature(chart) || saved.refining) ?
                       {...saved.camera} : {scale: 1, x: 0, y: 0},
                   angles: ['Azimuth', 'Elevation', 'Roll'].map(key =>
                       Number(document.getElementById('plot' + key).value))};
        const preserveOrientation = saved?.identity === identity(chart) && saved.orientation &&
            current.angles.every((value, i) => Math.abs(value - saved.angles[i]) <= 0.51);
        current.orientation = preserveOrientation ?
            [...saved.orientation] : labSurfaceCamera.fromAngles(current.angles);
        if (preserveOrientation) current.angles = [...saved.angles];
        labSurfaceVolume.initialise(current, saved, saved?.key === signature(chart));
        canvas.addEventListener('webglcontextlost', event => {
            event.preventDefault();
            if (current?.overlay !== overlay) return;
            const savedView = snapshot();
            dispose();
            const fallback = prepare(chart, savedView, true);
            if (fallback) draw(chart, fallback.angles);
        });
        if (window.ResizeObserver) {
            const view = current;
            view.resizeObserver = new ResizeObserver(() => {
                if (current !== view || view.frame) return;
                view.frame = requestAnimationFrame(() => {
                    view.frame = 0;
                    if (current === view && chart.isConnected) draw(chart, view.angles);
                });
            });
            view.resizeObserver.observe(host);
        }
        syncZoom(current);
        return current;
    }

    function draw(chart, angles, bounds = null) {
        const view = prepare(chart);
        if (!view || !angles.every(Number.isFinite)) return false;
        if (angles.some((value, i) => Math.abs(value - view.angles[i]) > 1e-9))
            view.orientation = labSurfaceCamera.fromAngles(angles);
        view.angles = [...angles];
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
        const frame = labDOM.call('lab_surface_frame', view, bounds);
        if (!frame) return false;
        view.framePlan = frame;
        view.project = point => labDOM.call('lab_surface_project', frame, point);
        view.polygon = points => labDOM.call('lab_surface_polygon', frame, points);
        const {scaling, offset} = frame;
        const matrix = new Float32Array(frame.matrix);
        if (view.gpu) {
            if (!view.gpu.draw(matrix, scaling, offset, frame.drawMesh)) {
                const savedView = snapshot();
                dispose();
                const fallback = prepare(chart, savedView, true);
                return fallback ? draw(chart, fallback.angles, bounds) : false;
            }
        } else {
            const ctx = view.context;
            ctx.clearRect(0, 0, width, height);
            const triangles = (frame.drawMesh ? view.triangles : []).map(triangle => ({
                ...view.polygon(triangle.points), colour: triangle.colour
            })).filter(triangle => triangle.points.length >= 3);
            triangles.sort((a, b) => b.depth - a.depth);
            ctx.strokeStyle = '#193d4659';
            ctx.lineWidth = 0.45;
            for (const triangle of triangles) {
                ctx.beginPath();
                triangle.points.forEach((p, i) => { i ? ctx.lineTo(...p) : ctx.moveTo(...p); });
                ctx.closePath();
                ctx.fillStyle = triangle.colour;
                ctx.fill(); ctx.stroke();
            }
        }
        view.paintAxes = ctx => {
            ctx.clearRect(0, 0, width, height);
            ctx.strokeStyle = '#94b8b1'; ctx.fillStyle = '#b8ccc5'; ctx.lineWidth = 1.2;
            ctx.font = '11px sans-serif'; ctx.textAlign = 'center';
            for (const path of frame.paths) {
                ctx.beginPath();
                ctx.moveTo(path[0], path[1]);
                for (let i = 2; i < path.length; i += 2) ctx.lineTo(path[i], path[i + 1]);
                ctx.stroke();
            }
            for (const text of frame.texts) {
                ctx.textAlign = text.align;
                ctx.fillText(text.text, text.x, text.y);
            }
        };
        view.paintAxes(view.ink);
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
        chart.dataset.cameraZoom = String(view.camera.scale);
        chart.dataset.volumeBounds = view.volume.bounds.join(',');
        chart.dataset.cameraQuaternion = view.orientation.join(',');
        syncZoom(view);
        return true;
    }

    function rotate(chart, state, angles) {
        const view = prepare(chart);
        if (!view) return false;
        if (state.slider || state.wheel || !state.trackball) return draw(chart, angles);
        if (!state.rotationStart) {
            state.rotationStart = [...view.orientation];
            state.rotationBox = chart.getBoundingClientRect();
        }
        view.orientation = state.rotationCancelled ? [...state.rotationStart] :
            labSurfaceCamera.drag(state.rotationStart, [state.x, state.y],
                [state.touchX, state.touchY], state.rotationBox, state.rollOnly);
        view.angles = labSurfaceCamera.angles(view.orientation, view.angles);
        ['plotAzimuth', 'plotElevation', 'plotRoll'].forEach((id, i) => {
            const control = document.getElementById(id), value = view.angles[i];
            control.value = String(value);
            document.getElementById(id + 'Value').textContent = String(Math.round(value)) + '°';
        });
        return draw(chart, view.angles);
    }

    function zoom(chart, factor, x, y, reset = false) {
        const view = prepare(chart);
        if (!view || !labSurfaceVolume.set(view, factor, reset)) return false;
        if (!view.frame) view.frame = requestAnimationFrame(() => {
            view.frame = 0;
            if (current === view && chart.isConnected) draw(chart, view.angles);
        });
        return true;
    }

    function pinch(chart, state, angles) {
        const view = prepare(chart);
        if (!view) return false;
        labSurfaceVolume.set(view, 0, false, 3, state);
        return draw(chart, view.angles);
    }
    window.addEventListener('pagehide', dispose);
    window.addEventListener('resize', () => requestAnimationFrame(() => requestAnimationFrame(() => {
        if (current?.chart.isConnected) draw(current.chart, current.angles);
    })));
    return Object.freeze({prepare, draw, clear, dispose, snapshot, captureResult, restoreResult, zoom, pinch, rotate,
        exportSvg: () => {
            if (!current || !draw(current.chart, current.angles)) return null;
            return labSurfaceExport.svg(current);
        }});
})();
