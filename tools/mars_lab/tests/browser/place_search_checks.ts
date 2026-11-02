/**
 * @file place_search_checks.ts
 * @brief Worldwide search selection, accent handling and asynchronous ownership regressions.
 *
 * Uses the actual native menu and town store with cancellable, controlled responses. Typing must not alter the
 * observer; choosing a result must retain its real identity. Visible suggestions reuse cached SVG flags, including
 * regional artwork, without replacing names or country details. Superseded or disposed requests cannot change rows.
 */

interface Window {
    /** @brief Exercise native worldwide search projection and its browser request lifetime. */
    checkLabPlaceSearch(): Promise<void>;
}

/** @brief Exceed the former admission cap in an isolated real WASM worker while retaining active countries. */
async function checkTownCacheRetention(): Promise<void> {
    const origin = JSON.stringify(location.origin);
    const source = `
        const base = ${origin};
        const read = globalThis.fetch.bind(globalThis);
        globalThis.fetch = (input, options) => read(typeof input === 'string' ? new URL(input, base) : input, options);
        importScripts(new URL('/js/transport.js', base).href);
        (async () => {
            const check = (ok, message) => { if (!ok) throw new Error(message); };
            await labWire.start(false);
            const row = (name, latitude = '5', longitude = '5', detail = '') => ({
                name, value: name + '|' + latitude + '|' + longitude + '|0', latitude, longitude, detail,
                elevation: '0', timezone: 'UTC', country_code: '', default: false
            });
            labDOM.call('lab_town_store_policy', {pins: ['GB', 'US']});
            check(labDOM.call('lab_town_store_add', 'GB', [row('Retained British town', '51', '-2')]) === 1 &&
                  labDOM.call('lab_town_store_add', 'US', [row('Retained American town', '40', '-74')]) === 1,
                  'initial selected-country records are retained');
            for (const code of ['GB', 'US']) for (let page = 0; page < 2; ++page) {
                const rows = Array.from({length: 512}, (_, i) => row(code + ' pinned ' + (page * 512 + i),
                    '5', '5', 'p'.repeat(4096)));
                check(labDOM.call('lab_town_store_add', code, rows) === 512,
                      'two active catalogues can together exceed the former eight-MiB allowance');
            }
            const countries = ['FR','DE','CA','NZ','ES','IT','NO','SE','DK','FI',
                               'PT','JP','BR','IN','CN','NL','IE','CH','AT','BE'];
            for (const code of countries) {
                const rows = Array.from({length: 512}, (_, i) => row(code + i, '5', '5', 'x'.repeat(4096)));
                check(labDOM.call('lab_town_store_add', code, rows) === 512,
                      'visiting ' + code + ' must recycle inactive catalogue space');
                await new Promise(resolve => setTimeout(resolve, 0));
            }
            const status = labDOM.call('lab_town_store_policy', {});
            check(status.bytes <= status.limit && status.limit === 16 * 1048576 && status.evicted.includes('FR'),
                  'country LRU retention stays within its fixed byte allowance');
            check(labDOM.call('lab_town_store_list', 'GB').length === 1025 &&
                  labDOM.call('lab_town_store_list', 'US').length === 1025 &&
                  labDOM.call('lab_town_store_list', 'FR') === null,
                  'both selected countries survive whilst an older inactive catalogue is removed');
            check(labDOM.call('lab_town_store_add', 'AU-WA', [row('Perth', '-31.9522', '115.8614')]) === 1 &&
                  labDOM.call('lab_town_store_add', 'ZA', [row('Pretoria', '-25.7449', '28.1878')]) === 1,
                  'uncached Perth and Pretoria are admitted after repeated country visits');
            check(labDOM.call('lab_town_store_list', 'AU')[0].name === 'Perth' &&
                  labDOM.call('lab_town_store_list', 'ZA')[0].name === 'Pretoria',
                  'new search references remain available through native selection keys');
            check(labDOM.call('lab_town_store_add', 'CO', [row('Existing')]) === 1,
                  'a rollback fixture is retained before its failing batch');
            check(labDOM.call('lab_town_store_add', 'CO', [row('Rolled back'), row('z'.repeat(65537))]) === -1 &&
                  labDOM.call('lab_town_store_list', 'CO').length === 1,
                  'failed batches roll back their additions without removing existing records');
            check(labDOM.call('lab_town_store_region', -3, 50, -1, 53)[0].town.name === 'Retained British town',
                  'spatial bucket slots remain valid after multiple country removals');
            check(labDOM.call('lab_town_store_add', 'FR', [row('Reloaded')]) === 1 &&
                  labDOM.call('lab_town_store_list', 'FR')[0].name === 'Reloaded',
                  'discarded countries can be loaded again without stale keys or list slots');
            labDOM.call('lab_town_store_clear');
            const empty = labDOM.call('lab_town_store_policy', {});
            check(empty.bytes === 0 && empty.count === 0 && !empty.evicted.length,
                  'disposal releases the cache and its eviction notices');
            postMessage({ok: true});
        })().catch(error => postMessage({error: String(error)}));
    `;
    const url = URL.createObjectURL(new Blob([source], {type: 'text/javascript'}));
    const worker = new Worker(url);
    let timeout = 0;
    try {
        await new Promise<void>((resolve, reject) => {
            timeout = window.setTimeout(() => reject(new Error('Town-cache retention check timed out')), 60000);
            worker.onmessage = event => event.data.ok ? resolve() : reject(new Error(event.data.error));
            worker.onerror = event => { event.preventDefault(); reject(new Error(event.message)); };
        });
    } finally { window.clearTimeout(timeout); worker.terminate(); URL.revokeObjectURL(url); }
}

/** @brief Select Lüderitz without knowing Namibia and reject stale suggestions through the real menu. */
window.checkLabPlaceSearch = async function checkLabPlaceSearch(): Promise<void> {
    function check(condition: unknown, message: string): void {
        if (!condition) throw new Error('Place search: ' + message);
    }
    async function flagVisible(item: HTMLElement, flag: string): Promise<void> {
        const deadline = performance.now() + 2500;
        while (item.querySelector('img.country-flag')?.getAttribute('src') !== flag && performance.now() < deadline)
            await new Promise<void>(resolve => requestAnimationFrame(() => resolve()));
        check(item.querySelector('.select-option-label > img.country-flag')?.getAttribute('src') === flag,
              'available artwork appears beside the place name without replacing its country detail');
    }
    const root = document.createElement('section');
    root.className = 'mode-panel';
    root.style.cssText = 'position:fixed;left:0;top:0;z-index:10000;width:500px';
    const country = document.createElement('select'), town = document.createElement('select');
    const select = document.createElement('select');
    country.id = 'testSearchJurisdiction'; town.id = 'testSearchTown';
    select.id = 'testPlaceSearch'; select.dataset.globalSearch = '1';
    country.append(new Option('United Kingdom', 'GB'), new Option('Namibia', 'NA'),
                   new Option('Svalbard & Jan Mayen', 'SJ'), new Option('Portugal', 'PT'));
    const azores = new Option('Região Autónoma dos Açores', 'PT-20');
    azores.hidden = true; country.append(azores);
    const wales = new Option('Britain (UK) - Wales', 'GB-WLS');
    wales.dataset.countryParent = 'GB-WLS';
    country.append(wales);
    const england = new Option('Britain (UK) - England', 'GB-ENG');
    england.dataset.countryParent = 'GB-ENG';
    country.append(england, new Option('Singapore', 'SG'));
    const westernAustralia = new Option('Western Australia', 'AU-WA');
    westernAustralia.hidden = true; westernAustralia.dataset.countryParent = 'AU';
    country.append(new Option('Australia', 'AU'), westernAustralia, new Option('South Africa', 'ZA'));
    country.value = 'GB'; town.append(new Option('Previous town', 'previous'));
    Reflect.set(country, 'labJurisdictionLabels', {
        NA: 'Namibia', GB: 'United Kingdom', SJ: 'Svalbard & Jan Mayen', PT: 'Portugal',
        'PT-20': 'Portugal — Região Autónoma dos Açores', 'GB-WLS': 'Britain (UK) - Wales',
        'GB-ENG': 'Britain (UK) - England', SG: 'Singapore', AU: 'Australia',
        'AU-WA': 'Australia - Western Australia', ZA: 'South Africa'
    });
    root.append(country, town, select); document.body.append(root);
    const menu = WorksheetLocations.selects.enhance(select, {searchable: true, details: true});
    check(menu, 'fixture menu is enhanced');
    const requests: {
        query: string; signal: AbortSignal; resolve(response: WorksheetPlaces.SearchResponse): void
    }[] = [];
    let remembered = 0, selected = 0;
    const controller = new WorksheetPlaces.SearchController(select, 'almanac', {
        read(query, signal) {
            return new Promise(resolve => { requests.push({query, signal, resolve}); });
        },
        remember() { remembered++; },
        selected() { selected++; }
    });
    const shell = select.closest<HTMLElement>('.select-shell')!;
    const input = shell.querySelector<HTMLInputElement>('.select-search')!;
    const row: LabTown = {
        name: 'Lüderitz', value: 'Lüderitz|-26.6481|15.1594|15', jurisdiction: 'NA',
        latitude: '-26.6481', longitude: '15.1594', elevation: '15', timezone: 'Africa/Windhoek',
        detail: '26° 38.89′ S, 15° 9.56′ E', default: false
    };
    const response: WorksheetPlaces.SearchResponse = {available: true, matches: [row]};
    const flag = 'data:image/svg+xml;base64,' + btoa(
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 18"><path fill="#356" d="M0 0h24v18H0z"/></svg>');
    const cornishFlag = 'data:image/svg+xml;base64,' + btoa(window.labAlmanacFixtures()[3]);
    WorksheetResults.tooltip.rememberCountry('NA', 'Namibia', flag);
    WorksheetResults.tooltip.rememberCountry('PT-20', 'Portugal', flag);
    WorksheetResults.tooltip.rememberCountry('SJ', 'Svalbard & Jan Mayen', '');
    WorksheetResults.tooltip.rememberCountry('GB-CON', 'Cornwall, England', cornishFlag);
    WorksheetResults.tooltip.rememberCountry('SG', 'Singapore', '');
    async function edit(query: string): Promise<void> {
        input.value = query;
        input.dispatchEvent(new Event('input', {bubbles: true}));
        await new Promise(resolve => window.setTimeout(resolve, 280));
    }
    async function settle(): Promise<void> { await Promise.resolve(); await Promise.resolve(); }
    try {
        shell.querySelector<HTMLButtonElement>('.select-button')!.click();
        WorksheetResults.tooltip.show(shell.querySelector<HTMLButtonElement>('.select-button')!);
        await edit('Luderitz');
        check(requests.length === 1 && requests[0].query === 'Luderitz', 'typing issues a bounded debounced query');
        requests[0].resolve(response); await settle();
        const suggestion = shell.querySelector<HTMLButtonElement>('.select-option:not(.hidden)')!;
        check(suggestion?.textContent?.includes('Lüderitz') && suggestion.textContent.includes('Namibia'),
              'accent-insensitive results show the country and remain visible under the original query');
        check(suggestion.dataset.inlineFlag === '1' && suggestion.dataset.countryCode === 'NA',
              'native search rows request inline artwork using their actual country code');
        await flagVisible(suggestion, flag);
        check(country.value === 'GB' && town.value === 'previous' && !remembered && !selected,
              'suggestions cannot select a different observer');
        suggestion.click();
        check(country.value === 'NA' && town.value === row.value && remembered === 1 && selected === 1,
              'an explicit choice records history and applies the actual country and town');
        check(town.selectedOptions[0]?.dataset.timezone === 'Africa/Windhoek' && select.value === '',
              'local time-zone metadata survives and the independent search resets');
        shell.querySelector<HTMLButtonElement>('.select-button')!.click();
        await edit('svalbard');
        const longyearbyen: LabTown = {
            name: 'Longyearbyen', value: 'Longyearbyen|78.2232|15.6469|28', jurisdiction: 'SJ',
            latitude: '78.2232', longitude: '15.6469', elevation: '28', timezone: 'Arctic/Longyearbyen',
            detail: '78° 13.39′ N, 15° 38.81′ E', default: true
        };
        requests[1].resolve({available: true, matches: [longyearbyen]}); await settle();
        const area = shell.querySelector<HTMLButtonElement>('.select-option:not(.hidden)')!;
        check(area?.textContent?.includes('Longyearbyen') && area.textContent.includes('Svalbard'),
              'an area query retains a suitable town despite the different town name');
        check(!area.querySelector('img.country-flag'), 'unavailable artwork leaves a usable textual result');
        area.click();
        check(country.value === 'SJ' && town.value === longyearbyen.value && selected === 2 && remembered === 2,
              'Svalbard selects Longyearbyen through the same ordinary location workflow');
        check(town.selectedOptions[0]?.dataset.timezone === 'Arctic/Longyearbyen',
              'area results retain the selected town time zone');
        shell.querySelector<HTMLButtonElement>('.select-button')!.click();
        await edit('pico');
        const madalena: LabTown = {
            name: 'Madalena (GeoNames 3372908)', matched_name: 'Pico', jurisdiction: 'PT-20',
            value: 'Madalena|38.53642|-28.5266|8', latitude: '38.53642', longitude: '-28.5266',
            elevation: '8', timezone: 'Atlantic/Azores', detail: '38° 32.19′ N, 28° 31.60′ W', default: false
        };
        requests[2].resolve({available: true, matches: [madalena]}); await settle();
        const island = shell.querySelector<HTMLButtonElement>('.select-option:not(.hidden)')!;
        check(island?.textContent?.includes('Pico — Madalena') && !island.textContent.includes('GeoNames'),
              'island matches show the island and clean reference-town name');
        await flagVisible(island, flag);
        island.click();
        check(country.value === 'PT-20' && town.value === madalena.value && selected === 3 && remembered === 3,
              'choosing Pico selects the real Madalena reference and its Azores membership');
        check(town.selectedOptions[0]?.textContent === 'Madalena' &&
              town.selectedOptions[0]?.dataset.timezone === 'Atlantic/Azores' &&
              town.selectedOptions[0]?.dataset.elevation === '8',
              'the alias does not rename the town or replace its time zone and elevation');
        shell.querySelector<HTMLButtonElement>('.select-button')!.click();
        await edit('Older'); await edit('Newer');
        check(requests.length === 5 && requests[3].signal.aborted, 'new queries abort the previous request');
        requests[4].resolve({available: true, matches: []}); await settle();
        requests[3].resolve(response); await settle();
        check(!shell.querySelector('.select-option:not(.hidden)'), 'late old suggestions cannot replace newer results');
        await edit('Pending'); controller.dispose();
        check(requests[5].signal.aborted, 'disposal cancels pending transport');
        requests[5].resolve(response); await settle();
        check(!shell.querySelector('.select-option:not(.hidden)'), 'disposed searches cannot redraw the menu');
        check(country.value === 'PT-20' && town.value === madalena.value && selected === 3,
              'cancellation never changes worksheet state');
        const english: LabTown = {...row, name: 'London', jurisdiction: 'GB-ENG',
            value: 'London|51.5085|-0.1257|25', latitude: '51.5085', longitude: '-0.1257',
            elevation: '25', timezone: 'Europe/London'};
        labDOM.call('lab_town_store_add', 'GB-ENG', [english]);
        const welsh: LabTown = {...row, name: 'Rhyl', jurisdiction: 'GB-WLS',
            value: 'Rhyl|53.319|-3.4916|5', latitude: '53.319', longitude: '-3.4916',
            elevation: '5', timezone: 'Europe/London'};
        check(labDOM.call('lab_location_search_results', select, {available: true, matches: [welsh]}),
              'a constituent-country search result is prepared');
        select.value = 'GB-WLS:' + welsh.value;
        check(labDOM.call('lab_location_search_choose', select) && country.value === 'GB-WLS' && town.value === welsh.value,
              'choosing Wales preserves its separate country selection');
        check(!Array.from(town.options).some(option => option.value === english.value),
              'a Welsh search selection does not populate the English town list');
        const cornish: LabTown = {...english, name: 'Newquay', matched_name: 'Cornwall', country_code: 'GB-CON',
            value: 'Newquay|50.41557|-5.07319|31', latitude: '50.41557', longitude: '-5.07319', elevation: '31'};
        check(labDOM.call('lab_location_search_results', select, {available: true, matches: [cornish]}),
              'regional flag metadata survives replacement of the search results');
        const region = shell.querySelector<HTMLButtonElement>('.select-option:not(.hidden)')!;
        await flagVisible(region, cornishFlag);
        check(region.textContent?.includes('Cornwall — Newquay') && region.textContent.includes('Britain (UK) - England'),
              'regional artwork preserves both the named place and its country label');
        const welshFlag = 'data:image/svg+xml;base64,' + btoa(
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 18"><path fill="#c23" d="M0 0h24v18H0z"/></svg>');
        WorksheetResults.tooltip.rememberCountry('GB-WLS', 'Wales', welshFlag);
        WorksheetResults.tooltip.rememberCountry('GB-ENG', 'England', flag);
        const welshCountry: LabTown = {...welsh, matched_name: 'Wales', country_code: 'GB-WLS'};
        const englishVillage: LabTown = {...english, name: 'Wales', matched_name: 'Wales, Sheffield', country_code: 'GB-ENG',
            value: 'Wales|53.34061|-1.28162|110', latitude: '53.34061', longitude: '-1.28162', elevation: '110'};
        check(labDOM.call('lab_location_search_results', select, {available: true, matches: [welshCountry, englishVillage]}),
              'the country and its English namesake are independently prepared');
        const namesakes = shell.querySelectorAll<HTMLButtonElement>('.select-option:not(.hidden)');
        check(namesakes[0]?.textContent?.includes('Wales — Rhyl') && namesakes[0].dataset.countryCode === 'GB-WLS',
              'the country Wales retains its Welsh flag identity');
        check(namesakes[1]?.querySelector('.select-option-label')?.textContent === 'Wales, Sheffield' &&
              namesakes[1].dataset.countryCode === 'GB-ENG',
              'the English village is explicitly qualified instead of masquerading as the country');
        await flagVisible(namesakes[0], welshFlag);
        await flagVisible(namesakes[1], flag);
        select.value = 'GB-WLS:' + welsh.value;
        check(labDOM.call('lab_location_search_choose', select) && country.value === 'GB-WLS' && town.value === welsh.value,
              'selecting the Welsh country reference cannot choose the English namesake');
        check(labDOM.call('lab_location_search_results', select, {
            available: true, matches: [welshCountry, {...englishVillage, matched_name: undefined}]
        }), 'a catalogue without a recorded locality label still supports namesake results');
        check(shell.querySelectorAll('.select-option:not(.hidden) .select-option-label')[1]?.textContent === 'Wales, England',
              'unqualified namesakes receive a native jurisdiction fallback until locality data are installed');
        const singapore: LabTown = {...row, name: 'Singapore', matched_name: 'Singapore', jurisdiction: 'SG',
            value: 'Singapore|1.28967|103.85007|23', latitude: '1.28967', longitude: '103.85007',
            elevation: '23', timezone: 'Asia/Singapore'};
        check(labDOM.call('lab_location_search_results', select, {available: true, matches: [singapore]}),
              'a country and representative town may share a name');
        const sameName = shell.querySelector('.select-option:not(.hidden) .select-option-label');
        check(sameName?.textContent === 'Singapore', 'identical country and town names are displayed once');
        const corePlaces: LabTown[] = [
            {...row, name: 'Perth', jurisdiction: 'AU-WA', latitude: '-31.9522', longitude: '115.8614',
             value: 'Perth|-31.9522|115.8614|34', elevation: '34', timezone: 'Australia/Perth'},
            {...row, name: 'Pretoria', jurisdiction: 'ZA', latitude: '-25.7449', longitude: '28.1878',
             value: 'Pretoria|-25.7449|28.1878|1332', elevation: '1332', timezone: 'Africa/Johannesburg'}
        ];
        for (const place of corePlaces) {
            check(labDOM.call('lab_location_search_results', select, {available: true, matches: [place]}),
                  `${place.name} is available independently of the selected country`);
            const item = shell.querySelector<HTMLButtonElement>('.select-option:not(.hidden)');
            check(item?.querySelector('.select-option-label')?.textContent === place.name,
                  `${place.name} remains visible even when its regional country option is hidden`);
            select.value = place.jurisdiction + ':' + place.value;
            check(labDOM.call('lab_location_search_choose', select) && town.value === place.value &&
                  town.selectedOptions[0]?.dataset.timezone === place.timezone,
                  `selecting ${place.name} preserves its location and local time zone`);
        }
        await checkTownCacheRetention();
    } finally {
        WorksheetResults.tooltip.hide();
        controller.dispose(); menu?.dispose();
        labDOM.call('lab_menu_index_invalidate', select);
        labDOM.call('lab_menu_index_invalidate', town);
        labDOM.call('lab_menu_index_invalidate', country);
        root.remove();
    }
};
