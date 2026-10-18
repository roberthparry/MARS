/**
 * @file surface_hover.ts
 * @brief Own curve inspection events and transient pointer overlays.
 *
 * Native C chooses and interpolates samples. This adapter only transports browser
 * geometry, throttles pointer events and owns the hit cursor and overlay nodes.
 * Inspection survives movement within a chart and yields to its ancestor gestures.
 * Read the pointer path in order: inspect(), updateHover(), findPoint(), showPoint().
 */

/** Supplies a retained view, or null while the plot cannot be inspected. */
interface MarsSurfaceHoverView {
    /** @brief Obtain rendering state for the selected curve. */
    (chart: SVGSVGElement): MarsSurfaceView | null;
}

/** Plain interface to the native operations used for curve inspection. */
interface MarsSurfaceHoverNative {
    /** @brief Invoke native code; the caller identifies its concrete result type. */
    call(name: string, ...args: unknown[]): unknown;
}

/** Pointer position captured for the next animation frame. */
interface MarsSurfaceHoverPointer {
    chart: SVGSVGElement;
    x: number;
    y: number;
}

/** Plot container carrying gesture state maintained by the native controller. */
interface MarsSurfaceHoverPlot extends HTMLElement {
    marsPlotDrag?: MarsSurfaceGesture;
}

/** Owns pointer scheduling, event subscriptions and the two temporary overlays. */
class MarsSurfaceHover {
    private static readonly owners = new WeakMap<SVGSVGElement, MarsSurfaceHover>();
    private readonly lifetime = new AbortController();
    private frame = 0;
    private pointer: MarsSurfaceHoverPointer | null = null;
    private lastPointer: MarsSurfaceHoverPointer | null = null;
    private chart: SVGSVGElement | null = null;
    private marker: HTMLDivElement | null = null;
    private tooltip: HTMLDivElement | null = null;

    /** @brief Install inspection with injected retained-view and native capabilities. */
    constructor(private readonly view: MarsSurfaceHoverView,
                private readonly native: MarsSurfaceHoverNative,
                private readonly events: EventTarget = window) {
        // Keep this instance as the receiver when the browser calls these methods.
        this.hide = this.hide.bind(this);
        this.leave = this.leave.bind(this);
        this.updateHover = this.updateHover.bind(this);
        const options = {signal: this.lifetime.signal, passive: true, capture: true};
        // These events invalidate the pointer position, end inspection or begin a competing gesture.
        const names = ['pointerdown', 'pointercancel', 'wheel', 'scroll', 'blur', 'pagehide'];
        for (const name of names) {
            this.events.addEventListener(name, this.hide, options);
        }
        this.events.addEventListener('pointerout', this.leave, options);
        this.events.addEventListener('pointerleave', this.leave, options);
    }

    /** @brief Hide inspection and cancel pending pointer work during gestures or redraws. */
    hide(): void {
        cancelAnimationFrame(this.frame);
        this.frame = 0;
        this.pointer = null;
        this.lastPointer = null;
        if (this.chart && MarsSurfaceHover.owners.get(this.chart) === this) {
            delete this.chart.dataset.surfaceHover;
            MarsSurfaceHover.owners.delete(this.chart);
        }
        this.chart = null;
        if (this.marker) {
            this.marker.remove();
            this.marker = null;
        }
        if (this.tooltip) {
            this.tooltip.remove();
            this.tooltip = null;
        }
    }

    /** @brief Release only this inspector's listeners, frame and overlay elements. */
    dispose(): void {
        this.lifetime.abort();
        this.hide();
    }

    /** @brief Remove stale feedback for a redraw while retaining only this chart's idle pointer. */
    invalidate(chart: SVGSVGElement): void {
        const pointer = this.pointer ?? this.lastPointer;
        this.hide();
        if (!this.lifetime.signal.aborted && pointer?.chart === chart && chart.isConnected && this.idle(chart)) {
            this.lastPointer = pointer;
        }
    }

    /** @brief Re-pick after a successful redraw unless a leave or gesture has cleared the idle pointer. */
    refresh(chart: SVGSVGElement): void {
        if (this.lastPointer?.chart !== chart) return;
        if (this.lifetime.signal.aborted || !chart.isConnected || !this.idle(chart)) {
            this.hide();
            return;
        }
        this.pointer = this.lastPointer;
        if (!this.frame) this.frame = requestAnimationFrame(this.updateHover);
    }

    /** @brief Queue one pointer sample; only idle curve charts admit inspection. */
    inspect(event: Event): void {
        if (this.lifetime.signal.aborted || !(event instanceof PointerEvent) || event.buttons ||
            event.pointerType === 'touch') {
            this.hide();
            return;
        }
        const chart = this.eventChart(event.target);
        if (!chart || !this.idle(chart)) {
            this.hide();
            return;
        }
        const previous = this.pointer?.chart ?? this.chart;
        if (previous && previous !== chart) {
            this.hide();
        }
        this.pointer = {chart: chart, x: event.clientX, y: event.clientY};
        this.lastPointer = this.pointer;
        if (!this.frame) {
            this.frame = requestAnimationFrame(this.updateHover);
        }
    }

    /** @brief Resolve the figure beneath transparent canvases as well as its painted SVG descendants. */
    private eventChart(target: EventTarget | null): SVGSVGElement | null {
        if (!(target instanceof Element)) return null;
        const selector = 'svg[data-surface][data-segments="true"]';
        const chart = target.closest(selector) ?? target.closest('svg[data-plot-figure]')?.querySelector(selector);
        return marsTransportIsDOM(chart, 'SVGSVGElement') ? chart : null;
    }

    /** @brief Ignore transitions between SVG descendants, including newly settled curve paths. */
    private leave(event: Event): void {
        const chart = this.pointer?.chart ?? this.lastPointer?.chart ?? this.chart;
        if (!chart) return;
        if (event instanceof PointerEvent && this.eventChart(event.relatedTarget) === chart) return;
        if (this.eventChart(event.target) !== chart && event.target instanceof Node &&
            !event.target.contains(chart)) return;
        this.hide();
    }

    /** @brief Follow the short ancestor chain to the native gesture owner outside nested plot panels. */
    private idle(chart: SVGSVGElement): boolean {
        let plot = chart.closest('.rendered') as MarsSurfaceHoverPlot | null;
        while (plot) {
            const drag = plot.marsPlotDrag;
            if (plot.classList.contains('plot-dragging') || drag?.active || drag?.wheel || drag?.busy || drag?.slider) {
                return false;
            }
            plot = plot.parentElement?.closest('.rendered') as MarsSurfaceHoverPlot | null;
        }
        return true;
    }

    /** @brief Pick the latest queued position only while its connected chart remains idle. */
    private updateHover(): void {
        const pointer = this.pointer;
        this.pointer = null;
        this.frame = 0;
        if (!pointer || !pointer.chart.isConnected || !this.idle(pointer.chart)) {
            this.hide();
            return;
        }
        const view = this.view(pointer.chart);
        const screen = pointer.chart.getScreenCTM();
        if (!view || !screen) {
            this.hide();
            return;
        }

        // Native code validates the screen transform and clips both pointer and marker.
        const hit = this.findPoint(view, screen, pointer);
        if (hit) {
            const owner = MarsSurfaceHover.owners.get(pointer.chart);
            if (owner && owner !== this) owner.hide();
            this.lastPointer = pointer;
            this.chart = pointer.chart;
            MarsSurfaceHover.owners.set(this.chart, this);
            this.chart.dataset.surfaceHover = 'true';
            this.showPoint(view.host, hit);
        } else {
            this.hide();
        }
    }

    /** @brief Delegate projected segment selection and formatted sample text to native code. */
    private findPoint(view: MarsSurfaceView, screen: DOMMatrix,
                       pointer: MarsSurfaceHoverPointer): MarsSurfaceHoverHit | null {
        let projection: MarsSurfaceFrame | null | undefined = view.framePlan;
        if (!projection) {
            projection = this.native.call('lab_surface_frame', view, null) as MarsSurfaceFrame | null;
        }
        if (!projection) {
            return null;
        }

        // C admits at most 65536 primitives and releases temporary handles per pair.
        // Pass the retained list directly: no slices or cross-call nearest-hit state.
        return this.native.call('lab_surface_hover', {chart: pointer.chart, primitives: view.primitives},
                                projection, screen, pointer.x, pointer.y, null) as MarsSurfaceHoverHit | null;
    }

    /** @brief Reuse the owned HTML overlays above both Canvas and settled SVG rendering. */
    private showPoint(host: HTMLElement, hit: MarsSurfaceHoverHit): void {
        const box = host.getBoundingClientRect();
        const left = hit.x - box.left - host.clientLeft + host.scrollLeft;
        const top = hit.y - box.top - host.clientTop + host.scrollTop;

        const marker = this.marker ?? document.createElement('div');
        marker.className = 'surface-pointer-marker';
        marker.setAttribute('aria-hidden', 'true');
        marker.style.left = left + 'px';
        marker.style.top = top + 'px';
        marker.style.pointerEvents = 'none';

        const tooltip = this.tooltip ?? document.createElement('div');
        tooltip.className = 'surface-pointer-tip';
        tooltip.textContent = hit.text;
        tooltip.style.position = 'absolute';
        tooltip.style.whiteSpace = 'pre';
        tooltip.style.pointerEvents = 'none';

        this.marker = marker;
        this.tooltip = tooltip;
        if (marker.parentElement !== host) host.appendChild(marker);
        if (tooltip.parentElement !== host) host.appendChild(tooltip);
        // Measure after insertion, when the browser knows the tooltip's size.
        this.placeTooltip(host, tooltip, left, top);
    }

    /** @brief Prefer the opposite side at plot edges before clamping within the scroll viewport. */
    private placeTooltip(host: HTMLElement, tooltip: HTMLDivElement, x: number, y: number): void {
        const width = tooltip.offsetWidth, height = tooltip.offsetHeight;
        const rightLimit = host.scrollLeft + host.clientWidth - width;
        const bottomLimit = host.scrollTop + host.clientHeight - height;
        let left = x + 12, top = y + 12;
        if (left > rightLimit) left = x - width - 12;
        if (top > bottomLimit) top = y - height - 12;
        if (left > rightLimit) {
            left = rightLimit;
        }
        if (left < host.scrollLeft) {
            left = host.scrollLeft;
        }
        if (top > bottomLimit) {
            top = bottomLimit;
        }
        if (top < host.scrollTop) {
            top = host.scrollTop;
        }
        tooltip.style.left = left + 'px';
        tooltip.style.top = top + 'px';
    }
}
