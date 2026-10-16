/** Coordinate-volume zoom and cancellable native refinement of retained surfaces. */
const labSurfaceVolume = (() => {
    let timer, pending, requesting = false, held = false;
    const clone = volume => ({...volume, base: [...volume.base], bounds: [...volume.bounds],
        units: [...volume.units], original: [...volume.original]});
    function initialise(view, saved, same) {
        labDOM.call('lab_surface_volume_init', view, saved, Number(same));
    }
    function cancel(manual = false) {
        clearTimeout(timer);
        if (pending && pending.token === labRequests.latestMain()) labRequests.cancel('plot');
        pending = null;
        if (manual) {
            held = false;
            const view = labSurfacePreview.prepare(document.querySelector('#plotRendered [data-surface]'));
            if (view) view.volume.dirty = false;
        }
    }
    function schedule(view) {
        clearTimeout(timer);
        if (!view?.volume.dirty) return;
        const generation = labRequests.latestMain(), mode = currentMode(), source = currentExpressionText();
        timer = setTimeout(() => {
            if (!view.chart.isConnected || generation !== labRequests.latestMain() || mode !== currentMode() ||
                source !== currentExpressionText()) return;
            const drag = document.getElementById('plotRendered').marsPlotDrag;
            if (held || drag?.active || drag?.wheel) { schedule(view); return; }
            requesting = true;
            let work;
            try { work = takeCalculus(document.getElementById('plotFirst').value, 'plot', null, false, true); }
            finally { requesting = false; }
            const request = {token: labRequests.latestMain(), view};
            pending = request;
            Promise.resolve(work).catch(error => console.error('Surface refinement failed', error)).finally(() => {
                if (pending === request) pending = null;
            });
        }, 350);
    }
    function accept(saved) {
        if (saved && pending?.token === labRequests.latestMain()) {
            saved.refining = true;
            saved.volume.dirty = false;
        }
        return saved;
    }
    function set(view, value, reset = false, kind = 0, event = null) {
        clearTimeout(plotControlsTimer);
        plotControlsTimer = undefined;
        cancel();
        if (!labDOM.call('lab_surface_zoom', view, kind, value, event, Number(reset))) return false;
        schedule(view);
        return true;
    }
    function slider(action) {
        if (action === 4) {
            clearTimeout(plotControlsTimer);
            plotControlsTimer = undefined;
            held = true;
            cancel();
        }
        if (action === 0) held = false;
    }
    function releaseSlider() {
        if (!held) return;
        held = false;
        const view = labSurfacePreview.prepare(document.querySelector('#plotRendered [data-surface]'));
        if (view) schedule(view);
    }
    window.addEventListener('pointerup', releaseSlider);
    window.addEventListener('pointercancel', releaseSlider);
    window.addEventListener('blur', releaseSlider);
    function clip(points) {
        return labDOM.call('lab_surface_clip', points);
    }
    return Object.freeze({initialise, clone, cancel, schedule, accept, set, slider, clip,
        get requesting() { return requesting; }});
})();
