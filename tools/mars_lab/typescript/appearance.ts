/**
 * @file appearance.ts
 * @brief Restore and project this browser's decorative skin before the worksheet is painted.
 *
 * CSS and local vector artwork supply the complete appearance. This small host adapter
 * persists an independent browser preference, synchronises open tabs and updates native
 * browser chrome. Choosing a skin never enters the calculation or worksheet-history flow.
 */
namespace WorksheetAppearance {
    /** @brief Versioned, origin-local preference; independent of server worksheet records. */
    export const storageKey = 'mars.lab.skin.v1';

    /** @brief Browser-only palette selection with injectable storage for restricted-storage checks. */
    export class SkinController {
        private readonly lifetime = new AbortController();
        private picker: HTMLSelectElement | null = null;
        private themeColour: HTMLMetaElement | null = null;

        /** @brief Retain the appearance root and storage capability without installing listeners. */
        public constructor(
            private readonly root: HTMLElement,
            private readonly storage: Pick<Storage, 'getItem' | 'setItem'>
        ) {
            this.changed = this.changed.bind(this);
            this.stored = this.stored.bind(this);
        }

        /** @brief Return the current validated identifier; missing or invalid values mean Earth. */
        public current(): string {
            return this.identifier(this.root.dataset.skin);
        }

        /** @brief Apply a saved preference before styles load; denied storage safely selects Earth. */
        public restore(): void {
            let saved: string | null = null;
            try { saved = this.storage.getItem(storageKey); } catch { /* Storage is optional. */ }
            this.select(saved, false);
        }

        /** @brief Apply a numbered skin immediately and optionally save it; storage failure keeps the live choice. */
        public select(value: unknown, persist = true): void {
            const identifier = this.identifier(value);
            this.root.dataset.skin = identifier;
            if (this.picker) this.picker.value = identifier;
            if (this.themeColour) {
                const colour = getComputedStyle(this.root).getPropertyValue('--paper').trim();
                if (colour) this.themeColour.content = colour;
            }
            if (persist) {
                try { this.storage.setItem(storageKey, identifier); } catch { /* The live selection still applies. */ }
            }
        }

        /** @brief Connect the accessible selector and cross-tab updates once; callbacks expire on dispose. */
        public mount(picker: HTMLSelectElement, themeColour: HTMLMetaElement | null, events: EventTarget = window): void {
            if (this.picker || this.lifetime.signal.aborted) return;
            this.picker = picker;
            this.themeColour = themeColour;
            this.select(this.current(), false);
            picker.addEventListener('change', this.changed, {signal: this.lifetime.signal});
            events.addEventListener('storage', this.stored, {signal: this.lifetime.signal});
        }

        /** @brief Remove owned subscriptions on permanent page departure; no worksheet state is touched. */
        public dispose(): void {
            this.lifetime.abort();
            this.picker = null;
            this.themeColour = null;
        }

        private identifier(value: unknown): string {
            return typeof value === 'string' && /^(?:[0-9]|1[0-3])$/.test(value) ? value : '0';
        }

        private changed(): void {
            this.select(this.picker?.value);
        }

        private stored(event: Event): void {
            const change = event as StorageEvent;
            if (change.key === storageKey || change.key === null) this.select(change.newValue, false);
        }
    }

    /** @brief Lazy storage access also handles browsers which deny access to the localStorage property itself. */
    export const controller = new SkinController(document.documentElement, {
        getItem: key => localStorage.getItem(key),
        setItem: (key, value) => localStorage.setItem(key, value)
    });
    controller.restore();

    /** @brief Attach controls after parsing, before the deferred application starts restoring worksheets. */
    function install(): void {
        const picker = document.getElementById('skinSelect');
        if (picker instanceof HTMLSelectElement)
            controller.mount(picker, document.querySelector<HTMLMetaElement>('meta[name="theme-color"]'));
    }

    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', install, {once: true});
    else install();
}
