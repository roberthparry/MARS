/**
 * @file place_search.ts
 * @brief Browser request and event ownership for independent worldwide place searches.
 *
 * C and SQLite own matching, ranking, record validation, result projection and reference selection. This adapter
 * owns debounce timers, cancellable HTTP requests and DOM events. Results cannot move the observer before an
 * explicit selection, and obsolete requests cannot replace newer suggestions. The ordinary town-change workflow
 * retains responsibility for history, calendar offsets, evaluation and globe travel.
 */

namespace WorksheetPlaces {
    /** @brief Bounded native search response; records retain their authored catalogue metadata. */
    export interface SearchResponse {
        available: boolean;
        matches: LabTown[];
        error?: string;
    }

    /** @brief Injectable browser capabilities; no search index or duplicate worksheet state is retained here. */
    export interface SearchServices {
        read(query: string, signal: AbortSignal): Promise<SearchResponse>;
        remember(mode: 'datetime' | 'almanac'): void;
        selected(mode: 'datetime' | 'almanac', select: WorksheetSelect): void;
    }

    const liveServices: SearchServices = {
        async read(query, signal) {
            const response = await labHttp.fetch('/place-search', {
                method: 'POST', body: labWire.encode({query}), signal
            });
            if (!response.ok) throw new Error(`Place search returned HTTP ${response.status}`);
            return await response.labData() as SearchResponse;
        },
        remember(mode) {
            if (mode === 'almanac') MarsAlmanacGlobe.captureHistory();
        },
        selected(_mode, select) {
            const panel = select.closest('.mode-panel');
            const town = panel?.querySelector<HTMLSelectElement>('select[id$="Town"]');
            const country = panel?.querySelector<HTMLSelectElement>('select[id$="Jurisdiction"]');
            town?.dispatchEvent(new Event('change', {bubbles: true}));
            if (country) void labApplication.prepareCountrySelection(country.value);
        }
    };

    /** @brief Own one themed search menu's browser lifetime and cancellation boundaries. */
    export class SearchController {
        private readonly lifetime = new AbortController();
        private request: AbortController | null = null;
        private timer = 0;
        private readonly shell: HTMLElement;
        private readonly input: HTMLInputElement;

        constructor(
            private readonly select: WorksheetSelect,
            private readonly mode: 'datetime' | 'almanac',
            private readonly services: SearchServices = liveServices
        ) {
            this.shell = select.closest<HTMLElement>('.select-shell')!;
            this.input = this.shell.querySelector<HTMLInputElement>('.select-search')!;
            this.input.maxLength = 128;
            this.input.addEventListener('input', this.edited.bind(this), {signal: this.lifetime.signal});
            this.input.addEventListener('keydown', event => {
                if (event.key === 'Escape') this.reset();
            }, {signal: this.lifetime.signal});
            this.shell.querySelector('.select-button')!.addEventListener('click', this.reset.bind(this),
                {signal: this.lifetime.signal});
            this.select.addEventListener('change', this.choose.bind(this), {signal: this.lifetime.signal});
            this.select.ownerDocument.addEventListener('click', event => {
                if (event.target instanceof Node && !this.shell.contains(event.target)) this.cancel();
            }, {capture: true, signal: this.lifetime.signal});
            labDOM.call('lab_location_search_status', select, 0);
        }

        /** @brief Release timers and HTTP work; disposal never changes worksheet controls. */
        dispose(): void {
            this.lifetime.abort();
            this.cancel();
        }

        private cancel(): void {
            window.clearTimeout(this.timer);
            this.timer = 0;
            this.request?.abort();
            this.request = null;
        }

        private reset(): void {
            this.cancel();
            this.input.value = '';
            labDOM.call('lab_location_search_status', this.select, 0);
        }

        private edited(): void {
            this.cancel();
            const query = this.input.value.trim();
            labDOM.call('lab_location_search_status', this.select, query.length >= 2 ? 1 : 0);
            if (query.length < 2) return;
            this.timer = window.setTimeout(() => { void this.search(query); }, 250);
        }

        private async search(query: string): Promise<void> {
            const request = new AbortController();
            this.request = request;
            try {
                const signal = AbortSignal.any([request.signal, this.lifetime.signal, AbortSignal.timeout(10000)]);
                const response = await this.services.read(query, signal);
                if (!this.current(request, query)) return;
                if (!labDOM.call('lab_location_search_results', this.select, response))
                    labDOM.call('lab_location_search_status', this.select, 3);
            } catch {
                if (this.current(request, query)) labDOM.call('lab_location_search_status', this.select, 3);
            } finally {
                if (this.request === request) this.request = null;
            }
        }

        private current(request: AbortController, query: string): boolean {
            return this.request === request && !request.signal.aborted && !this.lifetime.signal.aborted &&
                this.input.value.trim() === query && this.shell.classList.contains('open');
        }

        private choose(): void {
            if (!this.select.value) return;
            this.cancel();
            this.services.remember(this.mode);
            if (labDOM.call('lab_location_search_choose', this.select)) this.services.selected(this.mode, this.select);
            this.input.value = '';
        }
    }
}
