/**
 * @file history_drag.ts
 * @brief Pointer gestures and scroll feedback for the worksheet history list.
 *
 * Browser hit testing selects an insertion position without copying or changing
 * snapshots. The editor submits the completed move to the native timeline API.
 * Mouse, touch and pen share one gesture; cancellation never changes history.
 */
interface WorksheetHistoryDragState {
    pointer: number;
    row: HTMLElement;
    from: number;
    to: number;
    startX: number;
    startY: number;
    startScroll: number;
    x: number;
    y: number;
    active: boolean;
}

class WorksheetHistoryDrag {
    private gesture: WorksheetHistoryDragState | null = null;
    private marker: HTMLElement | null = null;
    private frame = 0;
    private lastTime = 0;
    private ignoreClick = false;

    /** Attach browser-only gestures to the editor's existing detachable lifetime. */
    constructor(private readonly list: HTMLElement, signal: AbortSignal,
                private readonly valid: () => boolean,
                private readonly move: (from: number, to: number) => void,
                private readonly announce: (text: string) => void,
                private readonly settled: () => void) {
        list.addEventListener('pointerdown', this.down, {signal});
        list.addEventListener('click', this.click, {signal, capture: true});
        list.addEventListener('lostpointercapture', this.lost, {signal});
        document.addEventListener('pointermove', this.motion, {signal, passive: false});
        document.addEventListener('pointerup', this.up, {signal});
        document.addEventListener('pointercancel', this.lost, {signal});
        document.addEventListener('keydown', this.key, {signal, capture: true});
        window.addEventListener('blur', () => this.cancel(), {signal});
        signal.addEventListener('abort', () => this.cancel(), {once: true});
    }

    /** Native persistence waits until a live drag has finished or been cancelled. */
    public get active(): boolean { return Boolean(this.gesture?.active); }

    private readonly down = (event: PointerEvent): void => {
        this.ignoreClick = false;
        const handle = event.target instanceof Element ? event.target.closest('.history-drag-handle') : null;
        const row = handle?.closest<HTMLElement>('[data-history-position]');
        if (!row || row.parentElement !== this.list || !event.isPrimary || event.button !== 0 ||
            this.gesture || !this.valid()) return;
        const from = Number(row.dataset.historyPosition);
        this.gesture = {pointer: event.pointerId, row, from, to: from, startX: event.clientX, startY: event.clientY,
            startScroll: this.list.scrollTop, x: event.clientX, y: event.clientY, active: false};
        event.preventDefault();
        try { this.list.setPointerCapture(event.pointerId); } catch { /* Synthetic pointer checks have no capture. */ }
    };

    private readonly motion = (event: PointerEvent): void => {
        const gesture = this.gesture;
        if (!gesture || event.pointerId !== gesture.pointer) return;
        if (!this.valid()) { this.cancel(); return; }
        gesture.x = event.clientX;
        gesture.y = event.clientY;
        if (!gesture.active && Math.hypot(gesture.x - gesture.startX, gesture.y - gesture.startY) >= 6) {
            gesture.active = true;
            gesture.row.classList.add('history-dragging');
            this.list.classList.add('history-reordering');
            this.ignoreClick = true;
        }
        if (gesture.active) {
            event.preventDefault();
            if (!this.frame) this.frame = requestAnimationFrame(this.draw);
        }
    };

    /** Keep browser hit testing and scrolling on animation frames, independent of worksheet evaluation. */
    private readonly draw = (time: number): void => {
        this.frame = 0;
        const gesture = this.gesture;
        if (!gesture?.active) return;
        if (!this.valid()) { this.cancel(); return; }
        const box = this.list.getBoundingClientRect(), elapsed = this.lastTime ? Math.min(32, time - this.lastTime) : 16;
        this.lastTime = time;
        const edge = Math.min(48, box.height / 3);
        let speed = 0;
        if (gesture.x >= box.left && gesture.x <= box.right && gesture.y >= box.top - edge && gesture.y <= box.bottom + edge) {
            if (gesture.y < box.top + edge) speed = -Math.min(1, (box.top + edge - gesture.y) / edge);
            else if (gesture.y > box.bottom - edge) speed = Math.min(1, (gesture.y - box.bottom + edge) / edge);
        }
        const previous = this.list.scrollTop;
        this.list.scrollTop += speed * elapsed * 0.6;
        gesture.row.style.transform = `translateY(${gesture.y - gesture.startY + this.list.scrollTop - gesture.startScroll}px)`;
        this.target();
        if (this.list.scrollTop !== previous) this.frame = requestAnimationFrame(this.draw);
    };

    /** The browser's spatial hit test avoids scanning the list for every pointer movement. */
    private target(): void {
        const gesture = this.gesture;
        if (!gesture) return;
        this.marker?.classList.remove('history-drop-before', 'history-drop-after');
        this.marker = null;
        gesture.to = gesture.from;
        const box = this.list.getBoundingClientRect();
        if (gesture.x < box.left || gesture.x > box.right || gesture.y < box.top - 48 || gesture.y > box.bottom + 48)
            return;
        const y = Math.max(box.top + 2, Math.min(box.bottom - 2, gesture.y));
        const hit = document.elementFromPoint(gesture.x, y)?.closest<HTMLElement>('[data-history-position]');
        if (!hit || hit === gesture.row || hit.parentElement !== this.list) return;
        const rowBox = hit.getBoundingClientRect(), position = Number(hit.dataset.historyPosition);
        const after = y > (rowBox.top + rowBox.bottom) / 2;
        const insertion = position + Number(after);
        gesture.to = insertion - Number(gesture.from < insertion);
        if (gesture.to === gesture.from) return;
        this.marker = hit;
        hit.classList.add(after ? 'history-drop-after' : 'history-drop-before');
    }

    private readonly up = (event: PointerEvent): void => {
        const gesture = this.gesture;
        if (!gesture || event.pointerId !== gesture.pointer) return;
        gesture.x = event.clientX;
        gesture.y = event.clientY;
        if (gesture.active) this.target();
        const box = this.list.getBoundingClientRect();
        const commit = gesture.active && gesture.from !== gesture.to && this.valid() &&
            gesture.x >= box.left && gesture.x <= box.right && gesture.y >= box.top && gesture.y <= box.bottom;
        this.cancel();
        if (commit) this.move(gesture.from, gesture.to);
    };

    private readonly lost = (event: PointerEvent): void => {
        if (this.gesture?.pointer === event.pointerId) this.cancel();
    };

    private readonly key = (event: KeyboardEvent): void => {
        if (event.key !== 'Escape' || !this.gesture) return;
        event.preventDefault();
        event.stopPropagation();
        this.cancel();
        this.announce('Move cancelled.');
    };

    private readonly click = (event: MouseEvent): void => {
        if (!this.ignoreClick || !event.detail) return;
        this.ignoreClick = false;
        event.preventDefault();
        event.stopImmediatePropagation();
    };

    /** Remove transient feedback, release capture and retain the native timeline unchanged. */
    public cancel(): void {
        const gesture = this.gesture;
        this.gesture = null;
        if (this.frame) cancelAnimationFrame(this.frame);
        this.frame = 0;
        this.lastTime = 0;
        gesture?.row.classList.remove('history-dragging');
        if (gesture) gesture.row.style.removeProperty('transform');
        this.marker?.classList.remove('history-drop-before', 'history-drop-after');
        this.marker = null;
        this.list.classList.remove('history-reordering');
        if (gesture && this.list.hasPointerCapture(gesture.pointer)) this.list.releasePointerCapture(gesture.pointer);
        if (gesture?.active) this.settled();
    }
}
