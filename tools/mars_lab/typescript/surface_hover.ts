/**
 * @file surface_hover.ts
 * @brief Own curve inspection events and transient pointer overlays.
 *
 * Native C chooses and interpolates samples. This adapter only transports browser
 * geometry, throttles pointer events and releases its listeners and overlay nodes.
 */
class MarsSurfaceHover {
    private readonly lifetime = new AbortController();
    private frame = 0;
    private marker: HTMLDivElement | null = null;
    private tooltip: HTMLDivElement | null = null;

    /** @brief Install inspection with injected retained-view and native capabilities. */
    constructor(private readonly view: (chart: SVGSVGElement) => MarsSurfaceView | null,
                private readonly native: Pick<LabDOMBridge, 'call'>,
                private readonly events: EventTarget = window) {
        const options = {signal: this.lifetime.signal, passive: true};
        for (const name of ['pointerdown', 'pointerout', 'pointerleave', 'pointercancel', 'wheel', 'scroll', 'blur'])
            events.addEventListener(name, this.hide, {...options, capture: true});
    }

    /** @brief Hide inspection and cancel pending pointer work during gestures or redraws. */
    readonly hide = (): void => {
        cancelAnimationFrame(this.frame);
        this.frame = 0;
        this.marker?.remove();
        this.tooltip?.remove();
        this.marker = this.tooltip = null;
    };

    /** @brief Release only this inspector's listeners, frame and overlay elements. */
    readonly dispose = (): void => {
        this.lifetime.abort();
        this.hide();
    };

    /** @brief Queue one pointer sample; only idle curve charts admit inspection. */
    readonly inspect = (event: Event): void => {
        this.hide();
        if (!(event instanceof PointerEvent) || event.buttons || event.pointerType === 'touch') return;
        const chart = event.target instanceof Element ? event.target.closest<SVGSVGElement>('svg[data-surface][data-segments="true"]') : null;
        if (!chart) return;
        const drag = (chart.closest('.rendered') as
            (HTMLElement & {marsPlotDrag?: MarsSurfaceGesture}) | null)?.marsPlotDrag;
        if (drag?.active || drag?.wheel || drag?.busy || drag?.slider) return;
        this.frame = requestAnimationFrame(() => {
            this.frame = 0;
            if (!chart.isConnected) return;
            const view = this.view(chart), screen = chart.getScreenCTM();
            if (!view || !screen) return;
            const pointer = new DOMPoint(event.clientX, event.clientY).matrixTransform(screen.inverse());
            if (pointer.x < 0 || pointer.y < 0 || pointer.x > view.width || pointer.y > view.height) return;
            const frame = view.framePlan ?? this.native.call<MarsSurfaceFrame | null>('lab_surface_frame', view, null);
            if (!frame) return;
            // Bound total work and each bridge scope; C alone compares hit distances.
            if (view.primitives.length > 65536) return;
            let hit: MarsSurfaceHoverHit | null = null;
            for (let offset = 0; offset < view.primitives.length; offset += 128)
                hit = this.native.call<MarsSurfaceHoverHit | null>('lab_surface_hover',
                    {chart, primitives: view.primitives.slice(offset, offset + 128)}, frame,
                    screen, event.clientX, event.clientY, hit);
            if (!hit) return;
            const box = view.host.getBoundingClientRect();
            const left = hit.x - box.left - view.host.clientLeft + view.host.scrollLeft;
            const top = hit.y - box.top - view.host.clientTop + view.host.scrollTop;
            this.marker = document.createElement('div');
            this.marker.className = 'surface-pointer-marker';
            this.marker.style.cssText = `left:${left}px;top:${top}px;pointer-events:none`;
            this.tooltip = document.createElement('div');
            this.tooltip.className = 'surface-pointer-tip';
            this.tooltip.textContent = hit.text;
            this.tooltip.style.cssText = `position:absolute;left:${left + 12}px;top:${top + 12}px;white-space:pre;pointer-events:none`;
            view.host.append(this.marker, this.tooltip);
            this.tooltip.style.left = `${Math.max(view.host.scrollLeft, Math.min(left + 12,
                view.host.scrollLeft + view.host.clientWidth - this.tooltip.offsetWidth))}px`;
            this.tooltip.style.top = `${Math.max(view.host.scrollTop, Math.min(top + 12,
                view.host.scrollTop + view.host.clientHeight - this.tooltip.offsetHeight))}px`;
        });
    };
}
