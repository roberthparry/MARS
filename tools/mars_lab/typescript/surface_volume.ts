/**
 * @file surface_volume.ts
 * @brief Gesture lifetime and refinement scheduling for retained plots.
 *
 * Owns debounce timers and pending refinement requests. Native code decides the
 * coordinate volume; browser callbacks release gesture state and guard stale work.
 * Read the redraw path in order: schedule(), redrawWhenReady(), finishRedraw().
 */
/** Details captured when a redraw is scheduled, used to reject obsolete work. */
interface MarsSurfaceRedraw {
    view: MarsSurfaceView;
    generation: number;
    mode: WorksheetMode;
    source: string;
}

/** Identity of an in-flight request; object identity also distinguishes completions. */
interface MarsSurfaceRequest {
    token: number;
    view: MarsSurfaceView;
    accepted: boolean;
}

/** The plot container carries gesture state installed by the native controller. */
interface MarsSurfacePlotElement extends HTMLElement {
    marsPlotDrag?: MarsSurfaceGesture;
}

/** Owns refinement timers and gesture lifetime; native code owns coordinate-volume decisions. */
class MarsSurfaceVolume {
    private timer: number | undefined;
    private scheduled: MarsSurfaceRedraw | null = null;
    private pending: MarsSurfaceRequest | null = null;
    private inRequest = false;
    private held = false;

    /** @brief Register release callbacks so pointer cancellation or window blur ends a held slider gesture. */
    constructor(private readonly host: MarsSurfaceVolumeHost, private readonly events: EventTarget = window) {
        // Bind once: removing a listener must use the same function that added it.
        this.releaseSlider = this.releaseSlider.bind(this);
        this.redrawWhenReady = this.redrawWhenReady.bind(this);
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
        this.host.native.call('lab_surface_volume_init', view, saved, Number(same));
    }

    /**
     * @brief Cancel the debounce timer and any refinement still owning the latest request token.
     * Does not cancel a superseding request. Manual cancellation also releases the held
     * gesture and restores accepted geometry without overwriting another plot's controls.
     */
    cancel(manual = false): void {
        window.clearTimeout(this.timer);
        this.timer = undefined;
        this.scheduled = null;
        if (this.pending && this.pending.token === this.host.generation()) this.host.cancelPlot();
        this.pending = null;
        if (manual) {
            this.held = false;
            const view = this.currentView();
            if (view) this.rollback(view, false);
        }
    }

    /** @brief Restore native sampled state and repaint only the still-connected view that owned the preview. */
    private rollback(view: MarsSurfaceView, restoreControls: boolean): void {
        if (!view.chart.isConnected) return;
        if (this.host.native.call('lab_surface_zoom', view, 4, Number(restoreControls), null, 0)) {
            this.host.redraw(view);
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
        this.timer = undefined;
        this.scheduled = null;
        if (!view.volume.dirty) {
            return;
        }
        this.scheduled = {
            view: view,
            generation: this.host.generation(),
            mode: this.host.mode(),
            source: this.host.source()
        };
        this.timer = window.setTimeout(this.redrawWhenReady, 350);
    }

    private redrawWhenReady(): void {
        const redraw = this.scheduled;
        this.scheduled = null;
        this.timer = undefined;
        if (!redraw || !redraw.view.chart.isConnected) return;
        if (redraw.generation !== this.host.generation() ||
            redraw.mode !== this.host.mode() || redraw.source !== this.host.source()) {
            this.rollback(redraw.view, false);
            return;
        }

        const plot = document.getElementById('plotRendered') as MarsSurfacePlotElement | null;
        const drag = plot?.marsPlotDrag;
        if (this.held || drag?.active || drag?.wheel) {
            this.schedule(redraw.view);
            return;
        }
        // plotFirst holds the selected independent variable used to request refined samples.
        const first = document.getElementById('plotFirst');
        if (!(marsTransportIsDOM(first, 'HTMLInputElement') || marsTransportIsDOM(first, 'HTMLSelectElement'))) {
            this.rollback(redraw.view, false);
            return;
        }

        // This flag covers request creation only, not the asynchronous redraw.
        this.inRequest = true;
        let work: unknown;
        try {
            work = this.host.refine(first.value, redraw.view.chart);
        } catch (error) {
            this.rollback(redraw.view, true);
            console.error('Surface refinement failed', error);
            return;
        } finally {
            this.inRequest = false;
        }
        const request: MarsSurfaceRequest = {token: this.host.generation(), view: redraw.view, accepted: false};
        this.pending = request;
        void this.finishRedraw(work, request);
    }

    private async finishRedraw(work: unknown, request: MarsSurfaceRequest): Promise<void> {
        try {
            await work;
        } catch (error) {
            console.error('Surface refinement failed', error);
        } finally {
            // An older completion must not clear a newer pending request.
            if (this.pending === request) {
                this.pending = null;
                // Native error responses resolve normally but never install accepted geometry.
                if (!request.accepted) this.rollback(request.view, request.token === this.host.generation());
            }
        }
    }

    /**
     * @brief Mark a snapshot as an accepted refinement only while its request token is current.
     * Mutates and returns the supplied snapshot, clearing its dirty flag; null stays null.
     */
    accept(saved: MarsSurfaceSnapshot | null): MarsSurfaceSnapshot | null {
        if (saved && this.pending?.token === this.host.generation()) {
            this.pending.accepted = true;
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
        this.host.cancelControls();
        this.cancel();
        if (!this.host.native.call('lab_surface_zoom', view, kind, value, event, Number(reset))) {
            this.rollback(view, true);
            return false;
        }
        this.schedule(view);
        return true;
    }

    /** @brief Track slider hold (action 4) and release (action 0), cancelling refinement on hold. */
    slider(action: number): void {
        if (action === 4) {
            this.host.cancelControls();
            this.held = true;
            this.cancel();
        }
        if (action === 0) this.held = false;
    }

    private releaseSlider(): void {
        if (!this.held) return;
        this.held = false;
        const view = this.currentView();
        if (view) this.schedule(view);
    }

    private currentView(): MarsSurfaceView | null {
        const chart = document.querySelector('#plotRendered [data-surface]');
        if (!marsTransportIsDOM(chart, 'SVGSVGElement')) {
            return null;
        }
        return this.host.prepare(chart);
    }

    /** @brief Return native-clipped geometry without duplicating clipping mathematics in the browser. */
    clip(points: MarsSurfacePoint3[]): MarsSurfacePoint3[] {
        // The native clipping API returns an array of three-coordinate points.
        return this.host.native.call('lab_surface_clip', points) as MarsSurfacePoint3[];
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

/**
 * @brief Shared page-wide instance that schedules and cancels native plot redraws.
 *
 * Connects the redraw scheduler to the worksheet and retained plot renderer.
 * The const binding prevents reassignment; the instance's internal state remains mutable.
 */
const labSurfaceVolume: MarsSurfaceVolume = new MarsSurfaceVolume({
    native: labDOM,
    /** @brief Read the active request generation. */
    generation(): number {
        return WorksheetRequests.labRequests.latestMain();
    },
    /** @brief Cancel the active plot request. */
    cancelPlot(): void {
        WorksheetRequests.labRequests.cancel('plot');
    },
    /** @brief Cancel a queued control-panel update. */
    cancelControls(): void {
        WorksheetRequests.plots.cancelScheduledControls();
    },
    /** @brief Read the active worksheet mode. */
    mode(): WorksheetMode {
        return WorksheetWorkspace.mode.currentMode();
    },
    /** @brief Read the expression exactly as entered. */
    source(): string {
        return WorksheetBindings.editor.currentExpressionText();
    },
    /** @brief Obtain retained rendering resources for the chart. */
    prepare(chart: SVGSVGElement | null): MarsSurfaceView | null {
        return labSurfacePreview.prepare(chart);
    },
    /** @brief Repaint an accepted view after native preview rollback. */
    redraw(view: MarsSurfaceView): void {
        labSurfacePreview.draw(view.chart, view.angles);
    },
    /** @brief Request new native samples for the originating chart. */
    refine(variable: string, chart?: SVGSVGElement) {
        return WorksheetRequests.plots.refineSurfacePlot(variable, chart);
    }
});
