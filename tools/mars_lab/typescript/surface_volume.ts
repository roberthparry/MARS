/**
 * @file surface_volume.ts
 * @brief Gesture lifetime and refinement scheduling for retained plots.
 *
 * Owns debounce timers and pending refinement requests. Native code decides the
 * coordinate volume; browser callbacks release gesture state and guard stale work.
 */
/** Owns refinement timers and gesture lifetime; native code owns coordinate-volume decisions. */
class MarsSurfaceVolume {
    private timer: number | undefined;
    private pending: {token: ReturnType<typeof WorksheetRequests.labRequests.latestMain>; view: MarsSurfaceView} | null = null;
    private inRequest = false;
    private held = false;

    /** @brief Register release callbacks so pointer cancellation or window blur ends a held slider gesture. */
    constructor(private readonly host: MarsSurfaceVolumeHost, private readonly events: EventTarget = window) {
        this.events.addEventListener('pointerup', this.releaseSlider);
        this.events.addEventListener('pointercancel', this.releaseSlider);
        this.events.addEventListener('blur', this.releaseSlider);
    }

    /** @brief Report whether a slider gesture is held and refinement must remain deferred. */
    get interacting(): boolean { return this.held; }

    /** @brief Report synchronous entry into a refinement request, not its pending asynchronous lifetime. */
    get requesting(): boolean { return this.inRequest; }

    /** @brief Copy volume metadata and its coordinate arrays so snapshots do not share mutable bounds. */
    clone(volume: MarsSurfaceVolumeState): MarsSurfaceVolumeState {
        return {...volume, base: [...volume.base], bounds: [...volume.bounds],
            units: [...volume.units], original: [...volume.original], exact: [...volume.exact], sampled: [...volume.sampled]};
    }

    /**
     * @brief Ask native code to install a complete volume on a newly prepared view.
     * The same flag identifies a matching saved chart; native policy decides which saved
     * bounds and units remain applicable. On return the view has its required volume state.
     */
    initialise(view: MarsSurfaceInitialView, saved: MarsSurfaceSnapshot | null,
        same: boolean): asserts view is MarsSurfaceView {
        this.host.native.call<void>('lab_surface_volume_init', view, saved, Number(same));
    }

    /**
     * @brief Cancel the debounce timer and any refinement still owning the latest request token.
     * Does not cancel a superseding request. Manual cancellation also releases the held
     * gesture and clears the current view's dirty-volume flag.
     */
    cancel(manual = false): void {
        window.clearTimeout(this.timer);
        this.timer = undefined;
        if (this.pending && this.pending.token === this.host.generation()) this.host.cancel_plot();
        this.pending = null;
        if (manual) {
            this.held = false;
            const view = this.host.prepare(document.querySelector<SVGSVGElement>('#plotRendered [data-surface]'));
            if (view) view.volume.dirty = false;
        }
    }

    /**
     * @brief Debounce refinement of a dirty volume until interaction has ended.
     * The delayed callback checks chart connectivity, request generation, worksheet mode
     * and authored source before requesting native samples. Later requests cannot be cleared
     * by an older completion; failures are reported without changing that ownership rule.
     */
    schedule(view: MarsSurfaceView): void {
        window.clearTimeout(this.timer);
        if (!view.volume.dirty) return;
        const generation = this.host.generation(), mode = this.host.mode(), source = this.host.source();
        this.timer = window.setTimeout(() => {
            this.timer = undefined;
            if (!view.chart.isConnected || generation !== this.host.generation() || mode !== this.host.mode() ||
                source !== this.host.source()) return;
            const drag = (document.getElementById('plotRendered') as
                (HTMLElement & {marsPlotDrag?: MarsSurfaceGesture}) | null)?.marsPlotDrag;
            if (this.held || drag?.active || drag?.wheel) { this.schedule(view); return; }
            const first = document.getElementById('plotFirst');
            if (!(marsTransportIsDOM(first, 'HTMLInputElement') || marsTransportIsDOM(first, 'HTMLSelectElement'))) return;
            this.inRequest = true;
            let work: unknown;
            try { work = this.host.refine(first.value, view.chart); }
            finally { this.inRequest = false; }
            const request = {token: this.host.generation(), view};
            this.pending = request;
            Promise.resolve(work).catch((error: unknown) => console.error('Surface refinement failed', error)).finally(() => {
                if (this.pending === request) this.pending = null;
            });
        }, 350);
    }

    /**
     * @brief Mark a snapshot as an accepted refinement only while its request token is current.
     * Mutates and returns the supplied snapshot, clearing its dirty flag; null stays null.
     */
    accept(saved: MarsSurfaceSnapshot | null): MarsSurfaceSnapshot | null {
        if (saved && this.pending?.token === this.host.generation()) {
            saved.refining = true;
            saved.volume.dirty = false;
        }
        return saved;
    }

    /**
     * @brief Cancel superseded control work and ask native code to update the visible volume.
     * Kind and event are passed to native gesture policy unchanged. Successful updates
     * schedule refinement; rejected updates return false without scheduling new work.
     */
    set(view: MarsSurfaceView, value: number, reset = false, kind = 0,
        event: WheelEvent | MarsSurfaceGesture | null = null): boolean {
        this.host.cancel_controls();
        this.cancel();
        if (!this.host.native.call<boolean>('lab_surface_zoom', view, kind, value, event, Number(reset))) return false;
        this.schedule(view);
        return true;
    }

    /** @brief Track slider hold (action 4) and release (action 0), cancelling refinement on hold. */
    slider(action: number): void {
        if (action === 4) {
            this.host.cancel_controls();
            this.held = true;
            this.cancel();
        }
        if (action === 0) this.held = false;
    }

    private releaseSlider = (): void => {
        if (!this.held) return;
        this.held = false;
        const view = this.host.prepare(document.querySelector<SVGSVGElement>('#plotRendered [data-surface]'));
        if (view) this.schedule(view);
    };

    /** @brief Return native-clipped geometry without duplicating clipping mathematics in the browser. */
    clip(points: MarsSurfacePoint3[]): MarsSurfacePoint3[] {
        return this.host.native.call<MarsSurfacePoint3[]>('lab_surface_clip', points);
    }

    /** @brief Cancel owned refinement, release gesture state and remove the constructor's window listeners. */
    dispose(): void {
        this.cancel();
        this.held = false;
        this.events.removeEventListener('pointerup', this.releaseSlider);
        this.events.removeEventListener('pointercancel', this.releaseSlider);
        this.events.removeEventListener('blur', this.releaseSlider);
    }
}
const labSurfaceVolume: MarsSurfaceVolume = new MarsSurfaceVolume({
    native: labDOM,
    generation: () => WorksheetRequests.labRequests.latestMain(),
    cancel_plot: () => WorksheetRequests.labRequests.cancel('plot'),
    cancel_controls: () => WorksheetRequests.plots.cancelScheduledControls(),
    mode: () => WorksheetWorkspace.mode.currentMode(),
    source: () => WorksheetBindings.editor.currentExpressionText(),
    prepare: chart => labSurfacePreview.prepare(chart),
    refine: (variable, chart) => WorksheetRequests.plots.refineSurfacePlot(variable, chart)
});
