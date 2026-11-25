/**
 * @file resource_recovery_checks.ts
 * @brief Browser regressions for interrupted town pages and missing globe imagery.
 *
 * The private fixture interrupts real requests and image decoding, then verifies
 * bounded retries, cursor retention, reconnection and image-request cancellation.
 * It never reads or replaces the user's browser storage or saved worksheets.
 */

/** Install faults before startup and retain only the request counters needed by this browser fixture. */
function installLabResourceRecoveryFixture(): {run(): Promise<void>} {
    const fetchResponse = window.fetch;
    const decode = HTMLImageElement.prototype.decode;
    const cursors: string[] = [];
    let firstPages = 0, interrupted = '', faults = 0, images = 0, failImages = false;
    const stalled: HTMLImageElement[] = [];
    window.fetch = async (input, options) => {
        const url = new URL(String(input), location.href);
        if (url.pathname === '/towns' && url.searchParams.get('jurisdiction') === 'GB') {
            const cursor = url.searchParams.get('after')!;
            if (cursor === '0') ++firstPages;
            else {
                if (!interrupted) interrupted = cursor;
                if (cursor === interrupted) {
                    cursors.push(cursor);
                    if (++faults === 1) return new Response('Temporary outage', {status: 503});
                    if (faults === 2) throw new TypeError('Fixture connection interrupted');
                }
            }
        }
        return fetchResponse(input, options);
    };
    HTMLImageElement.prototype.decode = function(): Promise<void> {
        const url = new URL(this.src, location.href);
        if (url.pathname === '/earth_surface.png') {
            if (url.searchParams.has('fixture-stall')) {
                stalled.push(this);
                return new Promise<void>(() => {});
            }
            if (++images === 1 || failImages) return Promise.reject(new Error('Fixture image transfer interrupted'));
        }
        return decode.call(this);
    };
    function check(condition: unknown, message: string): asserts condition {
        if (!condition) throw new Error('Resource recovery: ' + message);
    }
    async function waitFor(ready: () => boolean, message: string, timeout = 15000): Promise<void> {
        const deadline = performance.now() + timeout;
        while (!ready() && performance.now() < deadline)
            await new Promise(resolve => window.setTimeout(resolve, 25));
        check(ready(), message);
    }
    return {async run(): Promise<void> {
        try {
            check(cursors.length === 3 && cursors.every(cursor => cursor === interrupted) && firstPages === 1,
                  'HTTP and connection failures retry the interrupted page without redownloading earlier pages');
            check(!document.querySelector('[data-jurisdiction-notice]:not(.hidden)'),
                  'a recovered country page leaves no stale incomplete-loading warning');
            await WorksheetWorkspace.mode.selectWorksheetMode('almanac');
            const root = document.getElementById('almanacGlobe')!;
            const globe = Reflect.get(MarsAlmanacGlobe, 'instance') as object;
            check(globe, 'the real displayed globe owns the imagery requests');
            await waitFor(() => root.dataset.surface === 'blue-marble' && !Reflect.get(globe, 'worldLoading'),
                          'the missing base image recovers without a drag, zoom or reload');
            check(images === 2, 'the initial failed overview has one successful automatic replacement');
            failImages = true;
            delete root.dataset.surface;
            const before = images;
            await Reflect.apply(Reflect.get(MarsAlmanacGlobe.prototype, 'loadWorld'), globe, []);
            await waitFor(() => images === before + 4 && !Reflect.get(globe, 'worldLoading'),
                          'permanent imagery failure stops after three delayed retries');
            await new Promise(resolve => window.setTimeout(resolve, 500));
            check(images === before + 4, 'a persistent failure does not spin or issue unbounded requests');
            failImages = false;
            window.dispatchEvent(new Event('online'));
            await waitFor(() => root.dataset.surface === 'blue-marble' && !Reflect.get(globe, 'worldLoading'),
                          'reconnecting resumes an exhausted image load');
            check(images === before + 5, 'reconnection publishes one replacement overview');

            const earthImage = Reflect.get(MarsAlmanacGlobe.prototype, 'earthImage');
            const lifetime = new AbortController();
            let error: unknown;
            try {
                await Reflect.apply(earthImage, {imageryLifetime: lifetime}, ['/earth_surface.png?fixture-stall=1']);
            } catch (failure) { error = failure; }
            check(error instanceof DOMException && error.name === 'TimeoutError' && !stalled[0].hasAttribute('src'),
                  'an image decode that never settles times out and releases its source');
            const pending = Reflect.apply(earthImage, {imageryLifetime: lifetime},
                                          ['/earth_surface.png?fixture-stall=2']) as Promise<HTMLImageElement>;
            lifetime.abort();
            error = undefined;
            try { await pending; } catch (failure) { error = failure; }
            check(error instanceof DOMException && error.name === 'AbortError' && !stalled[1].hasAttribute('src'),
                  'disposing imagery cancels decoding immediately and releases its source');

            const fetchPage = labHttp.fetch;
            let cancelledRequests = 0;
            try {
                labHttp.fetch = async (url, options) => {
                    if (url === '/towns?fixture-cancel=1') {
                        ++cancelledRequests;
                        return new Response('Temporary outage', {status: 503}) as MarsTransportResponse;
                    }
                    return fetchPage.call(labHttp, url, options);
                };
                const controller = new AbortController();
                const pendingPage = (Reflect.apply(Reflect.get(LabApplication.prototype, 'readTownPage'),
                    labApplication, ['/towns?fixture-cancel=1', controller.signal]) as Promise<LabCatalogue>)
                    .catch(failure => failure as unknown);
                await waitFor(() => cancelledRequests === 1, 'the cancellable page reaches its initial retry delay');
                controller.abort();
                const cancelled = await pendingPage;
                await new Promise(resolve => window.setTimeout(resolve, 600));
                check(cancelled instanceof DOMException && cancelled.name === 'AbortError' && cancelledRequests === 1,
                      'cancelling a country during backoff prevents the next request');
            } finally { labHttp.fetch = fetchPage; }
        } finally {
            window.fetch = fetchResponse;
            HTMLImageElement.prototype.decode = decode;
        }
    }};
}
