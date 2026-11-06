/**
 * @file globe_atlas_checks.ts
 * @brief Fixed-cell geography, multi-source cache pressure and real GPU upload regressions.
 *
 * Exercises the production WASM policy and photographic worker with two independently addressed 3600-square
 * source photographs. Native resolution tiers and a shared image/block budget prevent repeated whole-image
 * decoding; restarted workers must still reuse persisted raw source blocks without PNG requests.
 */
async function checkGlobeAtlas(check: (condition: boolean, message: string) => void): Promise<void> {
    await checkImageModule(check);
    for (const boundary of [4, 12, 60, 240, 1200]) {
        const before = labDOM.call('lab_globe_grid_spacing', boundary * .999, 800) as number[];
        const after = labDOM.call('lab_globe_grid_spacing', boundary * 1.001, 800) as number[];
        check(before[0] === after[0] && before[1] === after[1] && Math.abs(before[2] - after[2]) < .02,
              'grid intervals fade continuously across each old zoom threshold');
    }
    check(!labDOM.call('lab_globe_grid_spacing', NaN, 800), 'invalid display scales cannot select a grid');
    for (const latitude of [-89.875, 89.875]) {
        const picked = labDOM.call('lab_globe_grid', latitude, 0, 100, 800) as number[];
        check(!(picked[2] & 2), 'hover does not select meridians hidden by the polar density limit');
    }
    for (const [lat, lon] of [[52, -2], [-33, 151], [89.99, -179.9], [0, 180]]) {
        const start = [...labDOM.call('lab_globe_centre', 0, 179.9) as number[], 150];
        const end = [...labDOM.call('lab_globe_rotate',
            labDOM.call('lab_globe_centre', lat, lon), 1, 0, 0, .4) as number[], 4096];
        for (const progress of [0, .4, .96, 1]) {
            const next = labDOM.call('lab_globe_flight', start, end, Math.min(1, progress + .04), false) as number[];
            const expected = labDOM.call('lab_globe_view', next.slice(0, 4), next[4], 1) as number[];
            const predicted = labDOM.call('lab_globe_flight_ahead', start, end, progress, false) as number[];
            check(predicted.every((value, i) => Math.abs(value - expected[i]) < 1e-9),
                  'native look-ahead follows the actual great-circle camera: ' +
                  `location=${lat},${lon}; progress=${progress}; actual=${predicted}; expected=${expected}`);
        }
        check(labDOM.call('lab_globe_flight_ahead', [], end, .5, false) === null,
              'invalid route cameras cannot schedule photographic look-ahead');
    }
    const cacheURL = '/js/globe_texture_cache.js';
    const {GlobeTextureCache} = await import(cacheURL) as typeof import('../../typescript/globe_texture_cache.js');
    for (const [alpha, opaque] of [[255, true], [254, false], [0, false]] as const) {
        labDOM.call('lab_texture_premultiply', new Uint8Array([180, 80, 40, alpha]));
        check(Boolean(labDOM.call('lab_texture_opaque')) === opaque,
              'native opacity requires every prepared alpha value to equal 255');
    }
    const rows = new Uint8Array(512 * 64 * 4).fill(255);
    labDOM.call('lab_photo_coverage_rows', rows, 0);
    check(!labDOM.call('lab_photo_target_covered', [0, 0, 256, 64]),
          'an incomplete alpha pass cannot skip photographic source reads');
    for (let row = 64; row < 512; row += 64) {
        rows.fill(255);
        if (row === 64) rows[((100 - row) * 512 + 300) * 4 + 3] = 254;
        labDOM.call('lab_photo_coverage_rows', rows, row);
    }
    check(Boolean(labDOM.call('lab_photo_target_covered', [0, 0, 256, 512])) &&
          !labDOM.call('lab_photo_target_covered', [256, 0, 256, 512]) &&
          !labDOM.call('lab_photo_target_covered', [299.5, 100, 1, 1]),
          'native source coverage skips only wholly opaque rectangles, including fractional pixel edges');
    check(!labDOM.call('lab_photo_target_covered', [512, 0, .0000001, 1]) &&
          !labDOM.call('lab_photo_target_covered', [0, 0, NaN, 1]) &&
          !labDOM.call('lab_photo_targets_overlap', [0, 0, 256, 512], [256, 0, 256, 512]) &&
          Boolean(labDOM.call('lab_photo_targets_overlap', [0, 0, 256.1, 512], [256, 0, 256, 512])),
          'native geometry distinguishes disjoint, overlapping and invalid source rectangles');
    labDOM.call('lab_photo_coverage_rows', new Uint8Array(0), 0);
    check(!labDOM.call('lab_photo_target_covered', [0, 0, 1, 1]),
          'the next photographic cell cannot reuse the previous cell\'s alpha coverage');
    check(Boolean(labDOM.call('lab_photo_read_ahead', [0, 0, 260, 512], [256, 0, 256, 512])) &&
          Boolean(labDOM.call('lab_photo_read_ahead', [0, 0, 256, 512], [256, 0, 256, 512])) &&
          !labDOM.call('lab_photo_read_ahead', [0, 0, 256, 512], [128, 0, 256, 512]) &&
          !labDOM.call('lab_photo_read_ahead', [0, 0, 256, 512], [0, 0, 256, 512]) &&
          !labDOM.call('lab_photo_read_ahead', [0, 0, NaN, 512], [256, 0, 256, 512]),
          'native read-ahead permits narrow catalogue gutters but defers duplicate, substantially overlapping or invalid sources');
    const neighbourhood = new GlobeTextureCache<number>(64, 128 * 1024 * 1024, () => {});
    try {
        for (let i = 0; i < 6; ++i)
            check(neighbourhood.put(`source-neighbour-${i}`, i, 2048 * 2048 * 4),
                  'the bounded source cache admits a photograph needed by the same viewport');
        check(neighbourhood.size === 6 && neighbourhood.get('source-neighbour-0') === 0,
              'six neighbouring sixteen-MiB photographs coexist without repeatedly evicting the first source');
        for (let i = 6; i < 10; ++i) neighbourhood.put(`source-neighbour-${i}`, i, 2048 * 2048 * 4);
        check(neighbourhood.size === 8 && neighbourhood.get('source-neighbour-1') === null,
              'the expanded source cache still enforces its exact 128-MiB budget');
    } finally { neighbourhood.dispose(); }
    const descendants = new GlobeTextureCache<number>(64, 64 * 512 * 512 * 4, () => {});
    try {
        const level = 18, step = 360 / 2 ** level;
        const left = Math.floor((180 - 2.75) / step), top = Math.floor((90 - 52.7) / step);
        for (let y = 0; y < 8; ++y) for (let x = 0; x < 8; ++x) {
            const column = left + x, row = top + y;
            descendants.put(`photo:${level}:${column}:${row}:`, 1, 512 * 512 * 4,
                [column * step - 180, 90 - (row + 1) * step, (column + 1) * step - 180, 90 - row * step], level);
        }
        const west = left * step - 180, north = 90 - top * step;
        const view = [west + step / 4, north - 7.75 * step, west + 7.75 * step, north - step / 4];
        const page = descendants.page(view, 100)!;
        check(page.coverage > .999999 && !page.next,
              `zooming out over a fully cached fine mosaic schedules no replacement images: coverage=${page.coverage}; next=${page.next?.key}`);
        check(page.protect.length === 64,
              `every displayed fine cell remains protected from speculative eviction: ${page.protect.length}/64`);
        const arrival = descendants.journey(view, 100, [149.1, -35.32, 149.2, -35.22], 400, null, '', true)!;
        check(!arrival.next,
              'a full visible mosaic postpones distant preparation until a safe GPU slot becomes available');
        check(!descendants.put('photo:12:3744:1415:', 1, 512 * 512 * 4,
            [149.0625, -34.453125, 149.150390625, -34.365234375], 12, arrival.protect, 2) && descendants.size === 64,
              'even a completed offscreen image cannot displace the only displayed copy of a geographic cell');
        const departing = descendants.journey([149.1, -35.32, 149.2, -35.22], 800,
            view, 100, null, '', true)!;
        check(Boolean(departing.next) && departing.next!.bounds[0] < 149.2 && departing.next!.bounds[2] > 149.1,
              'a fully cached arrival cannot prevent missing current-view imagery from entering the atlas');
        const firstVisible = departing.next!;
        check(descendants.put(firstVisible.key, 1, 512 * 512 * 4, firstVisible.bounds,
            firstVisible.level, departing.protect, 2),
              'a full cached destination leaves an actual GPU admission slot for the visible view');
    } finally { descendants.dispose(); }
    const spatial = new GlobeTextureCache<number>(64, 64 * 1024 * 1024, () => {});
    try {
        for (const extent of [.035, .14, .28, 8]) {
            for (const displayPixels of [400, 800, 1536]) {
                const page = spatial.page([-2.8, 52.7, -2.8 + extent, 52.7 + extent], displayPixels)!;
                const span = page.next!.bounds[2] - page.next!.bounds[0];
                check(510 * extent / span >= displayPixels / (extent > 1 ? 4 : 1),
                      `close views request detail; wider empty views first request drawable parents: ` +
                      `${extent} degrees, ${displayPixels} pixels`);
                check(page.bounds[0] <= -2.8 && page.bounds[1] <= 52.7 &&
                      page.bounds[2] >= -2.8 + extent && page.bounds[3] >= 52.7 + extent,
                      `photographic lookup covers the entire view: ${extent} degrees, ${displayPixels} pixels; ${page.bounds}`);
            }
        }
        const british = spatial.page([-6, 50, 2, 57], 800)!;
        check(british.next!.bounds[2] - british.next!.bounds[0] <= 5.625,
              'initial British coverage does not decode a 22.5-degree window beyond the visible map');
        const view = [-2.82, 52.62, -2.68, 52.70];
        const low = spatial.page(view, 400)!;
        check(Boolean(low?.next), 'a fixed photographic page supplies a centre-first missing geographic cell');
        const coarse = spatial.journey(view, 400, view, 400, null, '', true)!;
        check(low.next!.level > coarse.next!.level,
              'stationary detail starts immediately instead of waiting for coarse coverage at the view edges');
        check(!spatial.page(view, 400, '', [], low.next!)!.replace,
              'native admission retains a running image that still covers the current view at a useful scale');
        const width = low.next!.bounds[2] - low.next!.bounds[0];
        const offscreen = {...low.next!, bounds: [view[2] + width / 10, view[1],
            view[2] + width * 1.1, view[1] + width]};
        check(spatial.page(view, 400, '', [], offscreen)!.replace,
              'an offscreen job inside the lookup padding cannot delay missing visible detail');
        check(spatial.page([149.1, -35.32, 149.2, -35.22], 800, '', [], low.next!)!.replace,
              'native admission replaces old-view work when Canberra needs a different photographic cell');
        check(spatial.page(view, 400, '', [], {...low.next!, level: 4})!.replace,
              'a running continental photograph cannot delay useful close-view detail');
        spatial.put(low.next!.key, 1, 512 * 512 * 4, low.next!.bounds, low.next!.level);
        const high = spatial.page(view, 800)!;
        const pixels = new Uint8Array(labWire.exports().memory.buffer, high.pointer, 1024).slice();
        check(pixels.some((value, i) => i % 4 === 0 && value > 0),
              'zooming towards finer cells retains a resident lower-resolution photographic ancestor');
        const moving = spatial.page([-2.81999, 52.62001, -2.67999, 52.70001], 800)!;
        check(moving.bounds.join() === high.bounds.join() && spatial.get(low.next!.key) === 1,
              'small pans preserve geographic cell keys and their GPU resource slots');
        const unchanged = spatial.page([-2.81999, 52.62001, -2.67999, 52.70001], 800)!;
        check(unchanged.revision === moving.revision,
              'repeated camera redraws reuse unchanged lookup bytes and their GPU upload revision');
        let refined = high;
        for (let i = 0; i < 8 && refined.next!.coverage; ++i) {
            const next = refined.next!;
            spatial.put(next.key, 2, 512 * 512 * 4, next.bounds, next.level);
            refined = spatial.page(view, 800)!;
        }
        const fine = refined.next!;
        const fineSlot = spatial.putSlot(fine.key, 2, 512 * 512 * 4, fine.bounds, fine.level);
        const wider = spatial.page(view, 200)!;
        check(new Uint8Array(labWire.exports().memory.buffer, wider.pointer, 1024).some((value, i) =>
            i % 4 === 0 && value === fineSlot + 1),
            'zooming out through two resolution tiers keeps already uploaded sharp cells visible');
        for (const factor of [8, 32, 128]) {
            const centre = [(fine.bounds[0] + fine.bounds[2]) / 2, (fine.bounds[1] + fine.bounds[3]) / 2];
            const extent = (fine.bounds[2] - fine.bounds[0]) * factor;
            const zoomed = spatial.page([centre[0] - extent, centre[1] - extent,
                centre[0] + extent, centre[1] + extent], 200)!;
            check(new Uint8Array(labWire.exports().memory.buffer, zoomed.pointer, 64 * 64 * 4).some((value, i) =>
                i % 4 === 0 && value === fineSlot + 1) && zoomed.protect.includes(fineSlot),
                `a ${factor}-fold zoom-out retains uploaded detail in a protected sparse refinement node`);
        }
        const arrival = spatial.page([50, 10, 50.1, 10.1], 400)!.next!;
        const arrivalSlot = spatial.putSlot(arrival.key, 3, 512 * 512 * 4, arrival.bounds, arrival.level);
        const pinned = spatial.page([50, 10, 50.1, 10.1], 400, '', wider.protect)!.protect;
        for (let i = 0; i < 80; ++i) spatial.put(`pressure:${i}`, i + 4, 512 * 512 * 4,
            undefined, 0, pinned);
        check(pinned.includes(fineSlot) && pinned.includes(arrivalSlot) &&
              spatial.get(fine.key) === 2 && spatial.get(arrival.key) === 3,
              'visible and prefetched arrival pixels survive cache pressure together');
        let admitted = 1;
        for (; admitted < 32; ++admitted) {
            const plan = spatial.page(view, 400)!;
            if (!plan.next) break;
            const bounds = plan.next.bounds, step = bounds[2] - bounds[0];
            check(bounds[0] <= view[2] + step * .5 && bounds[2] >= view[0] - step * .5 &&
                  bounds[3] >= view[1] - step * .5 && bounds[1] <= view[3] + step * .5,
                  'prefetch remains within half a photographic cell of the current view');
            spatial.put(plan.next.key, admitted + 1, 512 * 512 * 4, bounds, plan.next.level);
        }
        check(admitted < 32, 'stationary views stop requesting cells after filling their bounded neighbourhood');
        for (const [extent, latitude, longitude] of [[8, 46, -2.8], [.5, 52.7, -2.8], [2, 47, 2], [2, -35, 179]]) {
            const filled = new GlobeTextureCache<number>(64, 64 * 512 * 512 * 4, () => {});
            try {
                const bounds = [longitude, latitude, longitude + extent, latitude + extent];
                let page = filled.page(bounds, 1536)!;
                for (let count = 0; count < 128 && page.next; ++count) {
                    const next = page.next;
                    check(filled.put(next.key, 1, 512 * 512 * 4, next.bounds, next.level, page.protect),
                          'all visible high-density cells fit within the fixed photographic cache budget');
                    page = filled.page(bounds, 1536)!;
                }
                check(!page.next && Math.abs(page.coverage - 1) < 1e-9 && filled.size <= 64,
                      `a high-density page finishes the whole viewport and stops prefetch eviction: ${bounds}; ` +
                      `coverage=${page.coverage}; size=${filled.size}; next=${JSON.stringify(page.next)}; ` +
                      `protected=${page.protect.length}`);
            } finally { filled.dispose(); }
        }
        const aheadView = [2, 52.62, 2.14, 52.70];
        const predicted = spatial.page(view, 400, '', [], null, aheadView)!;
        check(predicted.next!.bounds[0] > 1.8,
              'after visible detail is complete, native panning prediction fills the next view before padding behind it');
        check(!spatial.page(view, 400, '', [], predicted.next!, aheadView)!.replace,
              'useful predicted-view preparation survives redraws before the pointer reaches it');
        check(!spatial.page([-180, -90, 180, 90], 800) && !spatial.page([0, 0, 1, 1], NaN),
              'continental and invalid pages fail safely without unbounded tile admission');
        for (const bounds of [[179.9, -.1, 180.1, .1], [-180.1, -.1, -179.9, .1], [1, 89.8, 2, 90]]) {
            const page = spatial.page(bounds, 800)!;
            check(Boolean(page?.next) && page.bounds[0] <= bounds[0] && page.bounds[1] <= bounds[1] &&
                  page.bounds[2] >= bounds[2] && page.bounds[3] >= bounds[3],
                  'fixed geographic pages cover polar and date-line views');
            check(!spatial.page(bounds, 800, '', [], page.next!)!.replace,
                  'wrapped and polar photographic jobs retain their usefulness under native geographic matching');
        }
    } finally { spatial.dispose(); }
    const small = new GlobeTextureCache<number>(16, 16 * 512 * 512 * 4, () => {});
    try {
        const view = [-2.82, 52.62, -2.68, 52.70];
        let page = small.page(view, 512)!;
        for (let i = 0; i < 20 && page.next; ++i) {
            const next = page.next;
            check(small.put(next.key, 1, 512 * 512 * 4, next.bounds, next.level, page.protect),
                  'small photographic budgets admit each visible cell without evicting another visible cell');
            page = small.page(view, 512)!;
        }
        check(!page.next && page.coverage === 1 && small.size <= 16,
              'small budgets finish visible detail without continuously evicting and reloading offscreen neighbours');
    } finally { small.dispose(); }
    for (const [latitude, longitude] of [[52.707, -2.754], [-33.869, 151.209], [-33.925, 18.424]]) {
        const camera = labDOM.call('lab_globe_centre', latitude, longitude) as number[];
        const view = (labDOM.call('lab_globe_view', camera, 4096, 1) as number[]).slice(3);
        const maximum = new GlobeTextureCache<number>(64, 64 * 512 * 512 * 4, () => {});
        try {
            let plan = maximum.journey(view, 1536, view, 1536, null, '', true)!;
            for (let i = 0; i < 128 && plan.next; ++i) {
                const cell = plan.next;
                check(maximum.put(cell.key, 1, 512 * 512 * 4, cell.bounds, cell.level, plan.protect, 2),
                      'maximum-zoom refinement can replace its progressive parents without losing visible coverage');
                plan = maximum.page(view, 1536)!;
            }
            check(plan.detail > .999999 && !plan.next,
                  `maximum-zoom views finish at their full display demand: ${latitude},${longitude}; ` +
                  `detail=${plan.detail}; next=${plan.next?.key}`);
        } finally { maximum.dispose(); }
    }
    const progress = new GlobeTextureCache<number>(64, 64 * 512 * 512 * 4, () => {});
    try {
        const view = [-2.82, 52.62, -2.68, 52.70], initial = progress.page(view, 800)!;
        const cell = initial.next!;
        const slot = progress.putSlot(cell.key, 1, 512 * 512 * 4, cell.bounds, cell.level, [], 1);
        const partial = progress.page(view, 800)!;
        check(partial.coverage === 0 && partial.next!.key === cell.key &&
              new Uint8Array(labWire.exports().memory.buffer, partial.pointer, 64 * 64 * 4).some((value, i) =>
                  i % 4 === 0 && value === slot + 1),
              'partial photographic pixels draw immediately without claiming complete geographic coverage');
        const wider = progress.page(view, 40)!;
        check(new Uint8Array(labWire.exports().memory.buffer, wider.pointer, 64 * 64 * 4).some((value, i) =>
            i % 4 === 0 && value === slot + 1) && wider.protect.includes(slot),
              'zoom-out retains and protects ready partial pixels while their remaining sources load');
        progress.putSlot(cell.key, 1, 512 * 512 * 4, cell.bounds, cell.level, partial.protect, 2);
        check(progress.page(view, 800)!.coverage > 0,
              'finishing a partial cell promotes its existing GPU slot to complete coverage');
        const parent = progress.journey(view, 800, view, 800, null, '', true)!.next!;
        const parentSlot = progress.putSlot(parent.key, 2, 512 * 512 * 4, parent.bounds, parent.level, [], 2);
        progress.remove(cell.key);
        progress.putSlot(cell.key, 1, 512 * 512 * 4, cell.bounds, cell.level, [], 1);
        const inherited = progress.page(view, 800)!;
        const addresses = new Uint8Array(labWire.exports().memory.buffer, inherited.pointer, 64 * 64 * 4);
        check(addresses.some((value, i) => i % 4 === 0 && value === parentSlot + 1),
              'incomplete fine imagery preserves a complete resident parent behind its missing source crops');
    } finally { progress.dispose(); }
    const packed = new GlobeTextureCache<number>(64, 64 * 512 * 512 * 4, () => {});
    try {
        const level = 18, span = 360 / 2 ** level;
        const left = Math.floor((-2.75 + 180) / span), top = Math.floor((90 - 52.7) / span);
        for (let y = 0; y < 8; ++y) for (let x = 0; x < 8; ++x) {
            const column = left + x, row = top + y;
            packed.put(`photo:${level}:${column}:${row}:`, 1, 512 * 512 * 4,
                [column * span - 180, 90 - (row + 1) * span, (column + 1) * span - 180, 90 - row * span], level);
        }
        const wide = packed.page([-3, 52.5, -2.5, 53], 400)!;
        const next = wide.next!;
        check(Boolean(next), 'a full fine mosaic selects a broader replacement that actually encloses a resident cell');
        check(!packed.put(next.key, 1, 512 * 512 * 4, next.bounds, next.level, wide.protect, 1) && packed.size === 64,
              'an unfinished broad replacement cannot evict fine geography before all its pixels arrive');
        check(wide.protect.length === 64 && packed.put(next.key, 1, 512 * 512 * 4,
            next.bounds, next.level, wide.protect, 2) && packed.size === 64,
            'a completed broader cell replaces an enclosed finer cell without unprotecting displayed pixels');
        let expanded = packed.page([-3, 52.5, -2.5, 53], 400)!;
        for (let i = 0; i < 128 && expanded.next; ++i) {
            const cell = expanded.next;
            check(packed.put(cell.key, 2, 512 * 512 * 4, cell.bounds, cell.level, expanded.protect, 2),
                  'a full atlas admits each useful replacement while preserving drawable geographic coverage');
            expanded = packed.page([-3, 52.5, -2.5, 53], 400)!;
        }
        check(expanded.coverage > .999999 && expanded.detail > .999999 && !expanded.next,
              `a large zoom-out finishes refinement under a full fine cache: coverage=${expanded.coverage}; ` +
              `detail=${expanded.detail}; next=${expanded.next?.key}`);
    } finally { packed.dispose(); }
    const travelling = new GlobeTextureCache<number>(64, 64 * 1024 * 1024, () => {});
    try {
        const view = [-2.82, 52.62, -2.68, 52.70], ahead = [0, 52.62, .14, 52.70], arrival = [50, 10, 50.1, 10.1];
        const arrivalJob = travelling.page(arrival, 400)!.next!;
        check(!travelling.journey(view, 400, arrival, 400, ahead, '', false, arrivalJob)!.replace,
              'current arrival preparation survives route redraws even when the destination is off screen');
        let plan = travelling.journey(view, 400, arrival, 400, ahead, '', true)!;
        check(plan.pace === .3 && plan.next!.bounds[0] < 0,
              'native travel planning first fills missing visible coverage and reduces its display-clock pace');
        for (let i = 0; i < 8 && plan.coverage < 1; ++i) {
            const next = plan.next!;
            travelling.put(next.key, 1, 512 * 512 * 4, next.bounds, next.level, plan.protect);
            plan = travelling.journey(view, 400, arrival, 400, ahead, '', true)!;
        }
        check(Math.abs(plan.coverage - 1) < 1e-9 && plan.pace > .3 && plan.pace < 1 &&
              plan.next!.bounds[0] >= -1.5 && plan.next!.bounds[2] > 0,
              'broad coverage improves pace while awaiting fine pixels and schedules the route before distant detail');
        for (let i = 0; i < 12 && plan.next!.coverage; ++i) {
            const next = plan.next!;
            travelling.put(next.key, 1, 512 * 512 * 4, next.bounds, next.level, plan.protect);
            plan = travelling.journey(view, 400, arrival, 400, ahead, '', true)!;
        }
        const first = plan.next!.bounds[0];
        const second = travelling.journey(view, 400, arrival, 400, ahead, '', true)!.next!.bounds[0];
        check((first < 0 && second > 49) || (first > 49 && second < 0),
              'native scheduling shares fine refinement between the visible route and destination after broad coverage');
        for (let i = 0; i < 50 && plan.pace < 1; ++i) {
            const next = plan.next!;
            travelling.put(next.key, 1, 512 * 512 * 4, next.bounds, next.level, plan.protect);
            plan = travelling.journey(view, 400, arrival, 400, ahead, '', true)!;
        }
        check(Math.abs(plan.pace - 1) < 1e-9 && travelling.size <= 64,
              'requested visible detail restores full travel pace within the unchanged GPU cache budget');
    } finally { travelling.dispose(); }
    const source = {url: '/earth_regions/01.png', sha256: 'a'.repeat(64), width: 3600,
        source: [500, 500, 40, 40], target: [0, 0, 512, 512]};
    type SourcePlan = {
        width: number; retain: boolean; key: string;
        parts: {needed: number; target?: number[]; key: string; url?: string}[];
    };
    const planned = labDOM.call('lab_photo_source', source) as SourcePlan;
    for (const width of [1800, 3600]) {
        const maximum = labDOM.call('lab_photo_source', {...source, width,
            source: [500, 500, 20, 20], target: [0, 0, 512, 512], pyramid: true}) as SourcePlan;
        check(maximum.width === width && maximum.parts.every(part => part.url!.includes(`&tier=${width}&`)),
              `magnified crops use the original ${width}-pixel PNG tier and its original prepared blocks`);
    }
    const parts = planned.parts;
    const needed = parts.filter(part => part.needed);
    check(planned.width === 3600 && !planned.retain,
          'a full-resolution overview retains its needed blocks instead of flushing the entire 64 MiB source cache');
    check(parts.length === 4 && needed.length === 4 &&
          needed.reduce((area, part) => area + part.target![2] * part.target![3], 0) === 512 * 512,
          'source-cell gutters retain exact coverage where a crop crosses four decoded blocks');
    check(parts[0].key.endsWith(':0:0:') && new Set(parts.map(part => part.key)).size === 4,
          'source keys preserve zero coordinates and uniquely address every decoded block');
    const edge = labDOM.call('lab_photo_source', {...source, source: [3500, 3500, 100, 100]}) as SourcePlan;
    check(edge.parts.length === 2 * 2 && edge.parts.every(part => part.needed),
          'native source planning visits only intersecting rows and columns at the image edge');
    const shifted = labDOM.call('lab_photo_source', {...source, source: [501, 501, 40, 40]}) as SourcePlan;
    check(shifted.parts.filter(part => part.needed).every((part, i) => part.key === needed[i].key),
          'source block identities are independent of camera-dependent crop rectangles');
    const overview = labDOM.call('lab_photo_source', {...source,
        source: [0, 0, 3600, 3600], target: [0, 0, 65, 50]}) as SourcePlan;
    check(overview.width === 128 && overview.retain && overview.parts.length === 1 && overview.key !== planned.key &&
          overview.parts[0].key !== parts[0].key,
          'a 65-pixel overview uses one 128-pixel source instead of 64 full-resolution raw blocks');
    const sourceCache = new GlobeTextureCache<number>(64, 64 * 1024 * 1024, () => {});
    const queuedCache = new GlobeTextureCache<number>(25, 26 * 1024 * 1024, () => {});
    try {
        const input = {...source, source: [1200, 900, 1024, 1024], target: [0, 0, 512, 512]};
        const fine = labDOM.call('lab_photo_source', input) as SourcePlan;
        const reduced = {...input, target: [0, 0, 256, 256]};
        const cold = sourceCache.source(reduced) as SourcePlan;
        check(fine.width === 2048 && cold.width === 1024, 'uncached source plans keep their demand-based resolution');
        sourceCache.put(fine.key, 1, fine.width * fine.width * 4);
        const reused = sourceCache.source(reduced) as SourcePlan;
        check(reused.key === fine.key && reused.width === fine.width &&
              Math.abs(reused.parts.reduce((area, part) => area + part.target![2] * part.target![3], 0) - 256 * 256) < 1e-7,
              `zoom-out reuses an adequate resident photograph while remapping its pixels to the new target: ` +
              `expected=${fine.key}; actual=${JSON.stringify(reused)}`);
        sourceCache.remove(fine.key);
        for (const part of fine.parts) sourceCache.put(part.key, 1, 514 * 514 * 4);
        check((sourceCache.source(reduced) as SourcePlan).key === fine.key,
              'a complete set of finer resident source blocks also avoids another reduced PNG decode');
        sourceCache.remove(fine.parts[0].key);
        check((sourceCache.source(reduced) as SourcePlan).key === cold.key,
              'a missing fine source block rejects that tier instead of reporting nonexistent cache coverage');
        check((sourceCache.source(reduced, undefined, fine.parts.map(part => part.key)) as SourcePlan).key === fine.key,
              'stored block addresses can satisfy the finer tier without loading image payloads into the planner');
        for (const part of fine.parts) queuedCache.put(part.key, 1, 514 * 514 * 4);
        check((sourceCache.source(reduced, queuedCache) as SourcePlan).key === fine.key,
              'queued raw pixels can complete the reusable finer tier before their disk writes finish');
        sourceCache.put(cold.key, 1, cold.width * cold.width * 4);
        check((sourceCache.source(input) as SourcePlan).width >= fine.width,
              'zoom-in never substitutes a smaller cached photograph for requested fine source detail');
    } finally { sourceCache.dispose(); queuedCache.dispose(); }
    check(!labDOM.call('lab_photo_source', {...source, target: [0, 0, Infinity, 512]}),
          'non-finite photographic demand cannot allocate an unbounded decode');

    const blockPlan = labDOM.call('lab_photo_source', {...source, pyramid: true}) as SourcePlan;
    check(blockPlan.key === `block-v1:${'a'.repeat(64)}:3600:` && !blockPlan.retain &&
          blockPlan.parts[0].url === `/earth-block?v=1&sha=${'a'.repeat(64)}&tier=3600&row=0&column=0`,
          'WASM supplies canonical immutable block identities and directly indexed HTTP addresses');
    const pyramidFixture = globeHTTPFixture(async bytes => {
        const request = labWire.decode(bytes) as {west: number};
        return new Response(labWire.encode({ok: true, key: `pyramid-only:${request.west}`, photos: [
            {...source, pyramid: true, source: [500 + request.west, 500, 40, 40]}
        ]}));
    }, {endpoint: '/almanac-rivers', script: '/js/globe_photo_worker.js', nativeShore: false,
        blockProbe: true, denyPhotographs: true, storageDenied: true, decodeProbe: true});
    const pyramidWorker = new pyramidFixture.Worker('/js/globe_photo_worker.js');
    try {
        for (const west of [0, 1]) {
            const pixels = await new Promise<ArrayBuffer>((resolve, reject) => {
                const timeout = setTimeout(() => reject(new Error('Prepared block fixture timed out')), 10000);
                pyramidWorker.onmessage = event => {
                    if (event.data.partial) return;
                    clearTimeout(timeout);
                    event.data.error ? reject(new Error(event.data.error)) : resolve(event.data.pixels);
                };
                pyramidWorker.onerror = event => { clearTimeout(timeout); reject(new Error(event.message)); };
                pyramidWorker.postMessage({key: `block-only:${west}`, bounds: [west, 0, west + 1, 1], level: 8});
            });
            const centre = new Uint8Array(pixels, (256 * 512 + 256) * 4, 4);
            check(centre[0] === 180 && centre[1] === 80 && centre[2] === 40 && centre[3] === 255,
                  'the real worker paints prepared blocks while whole-photograph requests are forbidden');
            await pyramidFixture.waitForDecode(4);
        }
    } finally { pyramidWorker.terminate(); pyramidFixture.dispose(); }

    const image = await fetch('/earth_regions/01.png');
    const digest = image.headers.get('ETag')!.replace(/^"sha256-|"$/g, '');
    const encoded = await image.arrayBuffer(), header = new Uint8Array(encoded, 0, 24).slice();
    check(Boolean(labDOM.call('lab_photo_source_header', header, 3600)) &&
          !labDOM.call('lab_photo_source_header', header, 1800) &&
          !labDOM.call('lab_photo_source_header', header.subarray(0, 23), 3600),
          'native PNG dimensions are checked before allocating a reduced browser image');
    header[0] ^= 1;
    check(!labDOM.call('lab_photo_source_header', header, 3600), 'invalid PNG signatures are rejected natively');
    const bitmap = await createImageBitmap(new Blob([encoded], {type: 'image/png'}));
    const reference = document.createElement('canvas');
    reference.width = reference.height = 512;
    const context = reference.getContext('2d')!;
    context.imageSmoothingQuality = 'high';
    context.drawImage(bitmap, 1200, 900, 1024, 1024, 0, 0, 512, 512);
    context.drawImage(bitmap, 1288, 988, 1024, 1024, 0, 0, 512, 512);
    const expected = context.getImageData(256, 256, 1, 1).data;
    bitmap.close();
    const tier = await createImageBitmap(new Blob([encoded], {type: 'image/png'}),
        {resizeWidth: 2048, resizeHeight: 2048, resizeQuality: 'high'});
    context.clearRect(0, 0, 512, 512);
    context.drawImage(tier, 0, 0, 1024 * 2048 / 3600, 1024 * 2048 / 3600, 0, 0, 512, 512);
    const unseenExpected = context.getImageData(256, 256, 1, 1).data;
    context.clearRect(0, 0, 512, 512);
    for (const crop of [[2400, 900], [2488, 988]])
        context.drawImage(tier, crop[0] * 2048 / 3600, crop[1] * 2048 / 3600,
            1024 * 2048 / 3600, 1024 * 2048 / 3600, 0, 0, 256, 256);
    const lowerZoomExpected = context.getImageData(128, 128, 1, 1).data;
    tier.close(); reference.width = reference.height = 1;
    let plans = 0, lastWest = NaN, lastCell = '', failNext = false;
    const persistedBlocks = new Set<string>();
    const respond = async (bytes: Uint8Array<ArrayBuffer>) => {
        const payload = labWire.decode(bytes) as {west: number; south: number; format: string};
        check(payload.format === 'photo-plan', 'photographic cells request no river geometry');
        ++plans;
        lastWest = payload.west;
        lastCell = `${payload.west}:${payload.south}`;
        if (failNext) { failNext = false; return new Response(labWire.encode({ok: false})); }
        const offset = payload.west >= .03 ? 1200 : payload.west > 0 ? 128 : 0;
        const photos = [
            {url: '/earth_regions/123.png', sha256: digest, width: 3600,
             source: [1200 + offset, 900, 1024, 1024], target: [0, 0, 512, 512]},
            {url: '/earth_regions/4095.png', sha256: digest, width: 3600,
             source: [1288 + offset, 988, 1024, 1024], target: [0, 0, 512, 512]}
        ];
        if (payload.west === .04) { photos.splice(1); photos[0].source = [0, 0, 1024, 1024]; }
        if (payload.west === .05) for (const photo of photos) photo.target = [0, 0, 256, 256];
        for (const photo of payload.west === .05 ? [] : photos) {
            const sourcePlan = labDOM.call('lab_photo_source', photo) as SourcePlan;
            for (const part of sourcePlan.parts) if (part.needed) persistedBlocks.add(part.key);
        }
        return new Response(labWire.encode({ok: true, key: `atlas-source-test:${payload.west}:${payload.south}`, photos}));
    };
    const OriginalWorker = globalThis.Worker;
    const alphaFixture = globeHTTPFixture(async () => new Response(labWire.encode({
        ok: true, alpha: true, key: 'alpha-background-regression', photos: [
            {url: '/earth_regions/9999.png', sha256: 'c'.repeat(64), width: 1800,
             source: [0, 0, 1800, 1800], target: [0, 0, 512, 512]},
            {url: '/earth_regions/123.png', sha256: 'c'.repeat(64), width: 1800,
             source: [0, 0, 1800, 1800], target: [0, 0, 512, 512]},
            {url: '/earth_regions/4095.png', sha256: 'c'.repeat(64), width: 1800,
             source: [0, 0, 1800, 1800], target: [0, 0, 512, 512]}
        ]
    })), {endpoint: '/almanac-rivers', script: '/js/globe_photo_worker.js', nativeShore: false,
        alphaProbe: true, storageDenied: true});
    const alphaWorker = new alphaFixture.Worker('/js/globe_photo_worker.js');
    try {
        const result = await new Promise<ArrayBuffer>((resolve, reject) => {
            const timeout = setTimeout(() => reject(new Error('Alpha background fixture timed out')), 15000);
            alphaWorker.onmessage = event => {
                if (event.data.partial) return;
                clearTimeout(timeout);
                event.data.error ? reject(new Error(event.data.error)) : resolve(event.data.pixels);
            };
            alphaWorker.onerror = event => { clearTimeout(timeout); reject(new Error(event.message)); };
            alphaWorker.postMessage({key: 'alpha-background', bounds: [26.7, 67.5, 28.1, 68.9], level: 8});
        });
        const top = new Uint8Array(result, (100 * 512 + 256) * 4, 4);
        const bottom = new Uint8Array(result, (400 * 512 + 256) * 4, 4);
        check(top[0] === 20 && top[2] === 180 && top[3] === 255 &&
              bottom[0] === 180 && bottom[2] === 40 && bottom[3] === 255,
              'a transparent foreground keeps its detailed pixels while revealing the complete image underneath');
    } finally { alphaWorker.terminate(); alphaFixture.dispose(); }
    const coveredFixture = globeHTTPFixture(async bytes => {
        const request = labWire.decode(bytes) as {west: number};
        const foreground = {url: '/earth_regions/4095.png', sha256: 'c'.repeat(64), width: 1800,
            source: [0, 0, 1800, 500], target: [0, 0, 256, 512]};
        const hidden = {...foreground, url: '/earth_regions/9999.png'};
        const background = {...foreground, url: '/earth_regions/123.png',
            source: [0, 0, 1800, 1800], target: [256, 0, 256, 512]};
        return new Response(labWire.encode({ok: true, alpha: true, key: `covered-source:${request.west}`,
            photos: [...(request.west ? [] : [background]), hidden, hidden, foreground]}));
    }, {endpoint: '/almanac-rivers', script: '/js/globe_photo_worker.js', nativeShore: false,
        alphaProbe: true, storageDenied: true});
    const coveredWorker = new coveredFixture.Worker('/js/globe_photo_worker.js');
    try {
        for (const west of [0, 1]) {
            const result = await new Promise<ArrayBuffer>((resolve, reject) => {
                const timeout = setTimeout(() => reject(new Error('Covered-source fixture timed out')), 15000);
                coveredWorker.onmessage = event => {
                    if (event.data.partial) return;
                    clearTimeout(timeout);
                    event.data.error ? reject(new Error(event.data.error)) : resolve(event.data.pixels);
                };
                coveredWorker.onerror = event => { clearTimeout(timeout); reject(new Error(event.message)); };
                coveredWorker.postMessage({key: `covered-source-${west}`, bounds: [west, 0, west + .1, .1], level: 8});
            });
            const left = new Uint8Array(result, (256 * 512 + 128) * 4, 4);
            const right = new Uint8Array(result, (256 * 512 + 384) * 4, 4);
            check(left[2] === 180 && left[3] === 255 && (west ? right[3] === 0 : right[0] === 180 && right[3] === 255),
                  'opaque source rectangles skip unavailable overlapping PNGs while uncovered areas retain their own background or gap');
        }
    } finally { coveredWorker.terminate(); coveredFixture.dispose(); }
    if (typeof GlobeSurfaceCache === 'undefined') await new Promise<void>((resolve, reject) => {
        const script = document.createElement('script');
        script.src = '/js/globe_surface_cache.js';
        script.onload = () => resolve();
        script.onerror = () => reject(new Error('Raw cache script unavailable'));
        document.head.appendChild(script);
    });
    const cancellableCache = new GlobeSurfaceCache(512);
    await cancellableCache.put('atlas-cancelled-read',
        new Blob([new Uint8Array(512 * 512 * 4)], {type: 'application/octet-stream'}));
    const cacheAbort = new AbortController();
    const pendingRead = cancellableCache.get('atlas-cancelled-read', cacheAbort.signal);
    await Promise.resolve();
    cacheAbort.abort();
    check(!await pendingRead && Boolean(await cancellableCache.get('atlas-cancelled-read')),
          'cancelling an obsolete raw read leaves the stored pixels available for a later view');
    const interrupted = globeHTTPFixture(async bytes => {
        const view = labWire.decode(bytes) as {west: number; south: number};
        return new Response(labWire.encode({ok: true, key: `interrupt:${view.west}:${view.south}`, photos: [
            {url: view.west < 0 ? '/earth_tiles/00-00.png' : '/earth_regions/123.png', sha256: digest,
             width: view.west < 0 ? 1800 : 3600, source: [800, 600, 512, 512], target: [0, 0, 512, 512]}
        ]}));
    }, {endpoint: '/almanac-rivers', script: '/js/globe_photo_worker.js', nativeShore: false,
        holdPhotograph: true, regionalAliases: true, storageDenied: true});
    globalThis.Worker = interrupted.Worker;
    const interruptedAtlasURL = '/js/globe_photo_atlas.js';
    const {GlobePhotoAtlas: InterruptibleAtlas} = await import(interruptedAtlasURL) as
        typeof import('../../typescript/globe_photo_atlas.js');
    const interruptedCanvas = document.createElement('canvas'), interruptedGL = interruptedCanvas.getContext('webgl')!;
    let interruptedPublished = 0;
    const interruptedAtlas = new InterruptibleAtlas(interruptedGL, () => ++interruptedPublished, 2048);
    try {
        interruptedAtlas.bind([-2.82, 52.62, -2.68, 52.70], 800);
        await interrupted.waitForPhotograph();
        const previous = Reflect.get(interruptedAtlas, 'current') as {key: string; id: number};
        const started = performance.now();
        interruptedAtlas.bind([149.1, -35.32, 149.2, -35.22], 800);
        const replacement = Reflect.get(interruptedAtlas, 'current') as {key: string; id: number};
        const transport = Reflect.get(interruptedAtlas, 'worker') as Worker;
        transport.dispatchEvent(new MessageEvent('message', {data: {...previous, pixels: new ArrayBuffer(0)}}));
        check(replacement.id !== previous.id && !Reflect.get(interruptedAtlas, 'retryAt') && interruptedPublished === 0,
              'late old-view replies cannot fail or complete the replacement photographic request');
        while (!interruptedPublished && performance.now() - started < 5000)
            await new Promise(resolve => setTimeout(resolve, 20));
        check(interruptedPublished === 1 && !Reflect.get(interruptedAtlas, 'retryAt'),
              `Canberra reaches the GPU while the old photograph remains blocked: ${performance.now() - started} ms`);
    } finally {
        interruptedAtlas.dispose(); globalThis.Worker = OriginalWorker; interrupted.dispose();
        interruptedCanvas.width = interruptedCanvas.height = 1;
    }
    const nasa = await fetch('/earth_tiles/00-00.png');
    const nasaDigest = nasa.headers.get('ETag')!.replace(/^"sha256-|"$/g, '');
    await nasa.body?.cancel();
    const globalFixture = globeHTTPFixture(async () => new Response(labWire.encode({
        ok: true, alpha: true, key: 'nasa-full-resolution-worker', photos: [
            {url: '/earth_tiles_detail/47-23.png', sha256: nasaDigest, width: 1800,
             source: [800, 800, 200, 200], target: [0, 0, 512, 512]}
        ]
    })), {endpoint: '/almanac-rivers', script: '/js/globe_photo_worker.js', nativeShore: false,
        globalAliases: true, storageDenied: true});
    const globalWorker = new globalFixture.Worker('/js/globe_photo_worker.js');
    try {
        const result = await new Promise<ArrayBuffer>((resolve, reject) => {
            const timeout = setTimeout(() => reject(new Error('NASA full-resolution worker timed out')), 15000);
            globalWorker.onmessage = event => {
                if (event.data?.id !== 923) return;
                if (event.data.error) { clearTimeout(timeout); reject(new Error(event.data.error)); return; }
                if (!event.data.pixels || event.data.partial) return;
                clearTimeout(timeout); resolve(event.data.pixels);
            };
            globalWorker.onerror = () => { clearTimeout(timeout); reject(new Error('NASA detail worker failed')); };
            globalWorker.postMessage({id: 923, bounds: [172.6, -89.9, 172.7, -89.8], key: 'global-detail', level: 8});
        });
        const pixels = new Uint8Array(result);
        check(pixels.length === 512 * 512 * 4 && pixels[3] === 255 && pixels[pixels.length - 1] === 255,
              'the real worker decodes full-resolution NASA source addresses into opaque GPU-ready cells');
    } finally {
        globalWorker.terminate(); globalFixture.dispose();
    }
    const streaming = globeHTTPFixture(async () => new Response(labWire.encode({ok: true, key: 'streaming-photo', photos: [
        {url: '/earth_regions/123.png', sha256: digest, width: 3600,
         source: [0, 0, 3600, 3600], target: [0, 0, 256, 512]},
        {url: '/earth_tiles/00-00.png', sha256: nasaDigest, width: 1800,
         source: [0, 0, 1800, 1800], target: [256, 0, 256, 256]},
        {url: '/earth_regions/4095.png', sha256: digest, width: 3600,
         source: [0, 0, 3600, 3600], target: [256, 256, 256, 256]}
    ]})), {endpoint: '/almanac-rivers', script: '/js/globe_photo_worker.js', nativeShore: false,
        persistProbe: true, regionalAliases: true, holdPhotograph: true, decodeProbe: true});
    globalThis.Worker = streaming.Worker;
    const streamingCanvas = document.createElement('canvas'), streamingGL = streamingCanvas.getContext('webgl')!;
    let partialFrames = 0, completedFrames = 0;
    const streamingAtlas = new InterruptibleAtlas(streamingGL, () => {
        if (Reflect.get(streamingAtlas, 'busy')) ++partialFrames;
        else ++completedFrames;
    }, 2048);
    try {
        const view = [-2.82, 52.62, -2.68, 52.70];
        streamingAtlas.bind(view, 800);
        streaming.releaseSurfaceCache();
        await streaming.waitForPhotograph();
        await streaming.waitForDecode(2);
        const deadline = performance.now() + 3000;
        while (!partialFrames && performance.now() < deadline) await new Promise(resolve => setTimeout(resolve, 20));
        const cache = Reflect.get(streamingAtlas, 'cells') as import('../../typescript/globe_texture_cache.js').GlobeTextureCache<number>;
        check(partialFrames > 0 && completedFrames === 0 && cache.page(view, 512)!.coverage === 0,
              'ready source pixels reach the GPU while another PNG is blocked, without completing its geographic cell');
        const source = labDOM.call('lab_photo_source', {url: '/earth_regions/123.png', sha256: digest, width: 3600,
            source: [0, 0, 3600, 3600], target: [0, 0, 256, 512]}) as SourcePlan;
        await streaming.waitForCache(source.parts[0].key);
        check(completedFrames === 0,
              'raw pixels persist asynchronously while the current cell is still waiting for another photograph');
        const previous = Reflect.get(streamingAtlas, 'current') as {key: string};
        streamingAtlas.bind([149.1, -35.32, 149.2, -35.22], 800);
        check(cache.get(previous.key) !== null,
              'replacing an unfinished request preserves its already uploaded GPU pixels');
        streamingAtlas.bind(view, 800);
        streaming.releasePhotograph();
        const end = performance.now() + 5000;
        while (!completedFrames && performance.now() < end) await new Promise(resolve => setTimeout(resolve, 20));
        check(completedFrames === 1 && cache.page(view, 512)!.coverage > 0,
              'the prefetched PNG finishes in the same GPU slot after the remaining source is released');
    } finally {
        streaming.releasePhotograph(); streamingAtlas.dispose(); globalThis.Worker = OriginalWorker; streaming.dispose();
        streamingCanvas.width = streamingCanvas.height = 1;
    }
    const fixture = globeHTTPFixture(respond, {endpoint: '/almanac-rivers', script: '/js/globe_photo_worker.js',
        nativeShore: false, surfaceProbe: true, regionalAliases: true});
    let worker: Worker | null = null;
    const receive = (key: string, west: number) => new Promise<ArrayBuffer>((resolve, reject) => {
        const timeout = window.setTimeout(() => reject(new Error('Photographic worker timed out')), 90000);
        worker!.onmessage = event => {
            if (event.data.partial) {
                check(event.data.key !== key, 'older controllers ignore partial updates and still await the final tile');
                return;
            }
            if (event.data.key !== key) return;
            window.clearTimeout(timeout);
            event.data.error ? reject(new Error(event.data.error)) : resolve(event.data.pixels as ArrayBuffer);
        };
        worker!.onerror = event => {
            window.clearTimeout(timeout); event.preventDefault(); reject(new Error(event.message));
        };
        worker!.postMessage({key, level: 10, bounds: [west, 0, west + .1, .1]});
    });
    try {
        worker = new fixture.Worker('/js/globe_photo_worker.js');
        const first = await receive('first', 0);
        const firstPixels = new Uint8Array(first);
        const centre = (256 * 512 + 256) * 4;
        check(first.byteLength === 512 * 512 * 4 &&
              expected.every((value, i) => Math.abs(firstPixels[centre + i] - value) <= 3),
              'two full sources reach WebGL accurately while every persistent-cache write is deliberately blocked');
        const sourcePlans = fixture.sourcePlanCount();
        const immediate = await receive('immediate-revisit', 0);
        check(immediate.byteLength === first.byteLength && plans === 2 && fixture.sourcePlanCount() === sourcePlans,
              'a completed queued cell is reused directly without source planning while its disk write is blocked');
        await fixture.bypassPhotographs(true);
        const second = await receive('adjacent', .01);
        check(second.byteLength === first.byteLength && plans === 3,
              'an adjacent cell reads queued raw pixels with whole-photograph hits disabled and disk writes still blocked');
        await fixture.bypassPhotographs(false);
        const disjoint = await receive('different-source-blocks', .03);
        check(disjoint.byteLength === first.byteLength && plans === 4,
              'new blocks elsewhere in both decoded photographs reuse the images without another PNG fetch');
        const zoomedOut = await receive('cached-source-at-lower-zoom', .05);
        check(zoomedOut.byteLength === first.byteLength && plans === 5,
              'zooming out reuses the decoded fine PNGs instead of downloading either photograph at a smaller tier');
        fixture.releaseSurfaceCache();
        await fixture.waitForCache('atlas-source-test:0.03:0');
        check(fixture.cacheCount() >= persistedBlocks.size + 3 && fixture.cacheCount() <= 35,
              'visited source blocks persist before bounded background preparation of unused source cores');
        const whole = labDOM.call('lab_photo_source', {url: '/earth_regions/123.png', sha256: digest, width: 3600,
            source: [0, 0, 3600, 3600], target: [0, 0, 2048, 2048]}) as SourcePlan;
        await fixture.waitForCache(whole.parts[whole.parts.length - 1].key);
    } finally { worker?.terminate(); fixture.dispose(); }

    const persistent = globeHTTPFixture(respond, {endpoint: '/almanac-rivers', script: '/js/globe_photo_worker.js',
        nativeShore: false, denyPhotographs: true, regionalAliases: true});
    try {
        worker = new persistent.Worker('/js/globe_photo_worker.js');
        const revisit = await receive('persisted-source', .02);
        check(revisit.byteLength === 512 * 512 * 4,
              'a new worker renders another cell from persisted raw source blocks with all PNG reads forbidden');
        const unseen = await receive('previously-unvisited-source', .04);
        const unseenPixel = new Uint8Array(unseen, (256 * 512 + 256) * 4, 4);
        check(unseen.byteLength === 512 * 512 * 4 && unseenExpected.every((value, i) =>
            Math.abs(unseenPixel[i] - value) <= 3),
              'a restarted worker draws a previously unvisited part of the photograph without downloading or decoding its PNG');
        const lowerZoom = await receive('persisted-lower-zoom', .05);
        const lowerPixel = new Uint8Array(lowerZoom, (128 * 512 + 128) * 4, 4);
        check(lowerZoomExpected.every((value, i) => Math.abs(lowerPixel[i] - value) <= 3),
              'after a worker restart, zoom-out uses indexed higher-resolution disk pixels with all PNG reads forbidden');
    } finally { worker?.terminate(); persistent.dispose(); }

    const fixtureGPU = globeHTTPFixture(respond, {endpoint: '/almanac-rivers', script: '/js/globe_photo_worker.js',
        nativeShore: false, denyPhotographs: true, regionalAliases: true});
    globalThis.Worker = fixtureGPU.Worker;
    const atlasURL = '/js/globe_photo_atlas.js';
    const {GlobePhotoAtlas} = await import(atlasURL) as typeof import('../../typescript/globe_photo_atlas.js');
    const canvas = document.createElement('canvas'), gl = canvas.getContext('webgl')!;
    let published = 0;
    const atlas = new GlobePhotoAtlas(gl, () => { if (!Reflect.get(atlas, 'busy')) ++published; }, 2048);
    const framebuffer = gl.createFramebuffer();
    try {
        atlas.bind([-2.82, 52.62, -2.68, 52.70], 800);
        for (let i = 0; i < 1200 && !published; ++i) await new Promise(resolve => setTimeout(resolve, 25));
        check(published === 1, 'a fixed photographic cell reaches the GPU independently of drawing the next frame');
        const residentCells = Reflect.get(atlas, 'cells') as import('../../typescript/globe_texture_cache.js').GlobeTextureCache<number>;
        const page = residentCells.page([-2.82, 52.62, -2.68, 52.70], 512)!;
        const addresses = new Uint8Array(labWire.exports().memory.buffer, page.pointer, 1024);
        let slot = -1;
        for (let i = 0; i < addresses.length; i += 4) if (addresses[i]) { slot = addresses[i] - 1; break; }
        check(slot >= 0, 'the native page references the resident photographic slot');
        gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
        gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, Reflect.get(atlas, 'atlas'), 0);
        const pixels = new Uint8Array(4);
        gl.readPixels(slot % 4 * 512 + 256, Math.floor(slot / 4) * 512 + 256, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
        check(gl.getError() === gl.NO_ERROR && expected.every((value, i) => Math.abs(pixels[i] - value) <= 3),
              'the atlas preserves photographic colour and alpha during direct RGBA upload');
        failNext = true;
        atlas.prefetch([50, 10, 50.1, 10.1], 800);
        atlas.bind([-180, -90, 180, 90], 800);
        for (let i = 0; i < 600 && published < 2; ++i) await new Promise(resolve => setTimeout(resolve, 25));
        const failedCell = lastCell, retryDeadline = Reflect.get(atlas, 'retryAt') as number;
        check(published === 2 && lastWest > 49 && retryDeadline > performance.now(),
              `a flight prepares its destination whilst zoomed out and retains a bounded retry: ` +
              `published=${published}, west=${lastWest}, retry=${retryDeadline - performance.now()}`);
        atlas.bind([-180, -90, 180, 90], 800);
        for (let i = 0; i < 600 && published < 3; ++i) await new Promise(resolve => setTimeout(resolve, 25));
        check(published === 3 && lastWest > 49 && lastCell !== failedCell && performance.now() < retryDeadline,
              'a failed arrival cell does not suspend neighbouring photography for its ten-second retry delay');
        atlas.bind([-2.82, 52.62, -2.68, 52.70], 800);
        const retained = residentCells.page([-2.82, 52.62, -2.68, 52.70], 512)!;
        check(new Uint8Array(labWire.exports().memory.buffer, retained.pointer, 1024).some((value, i) =>
            i % 4 === 0 && value === slot + 1), 'arrival prefetch preserves the visible photographic page');
        for (let i = 0; i < 20; ++i) {
            const before = published;
            atlas.bind([-180, -90, 180, 90], 800);
            if (!Reflect.get(atlas, 'busy')) break;
            for (let j = 0; j < 600 && published === before; ++j)
                await new Promise(resolve => setTimeout(resolve, 10));
        }
        const prepared = plans;
        atlas.bind([-4, 45, -3.9, 45.1], 800);
        await new Promise(resolve => setTimeout(resolve, 100));
        check(plans > prepared && lastWest < 0,
              'a warmed arrival allows the native scheduler to fill missing imagery on the visible route');
        // The real redraw refreshes protection after a fast worker completion; this fixture draws explicitly.
        atlas.bind([-4, 45, -3.9, 45.1], 800);
        const protectedSlots = Reflect.get(atlas, 'protect') as number[];
        const arrivalPage = residentCells.page([50, 10, 50.1, 10.1], 512)!;
        check(arrivalPage.protect.length > 0 && arrivalPage.protect.every(value => protectedSlots.includes(value)),
              'arrival slots remain pinned independently of an unrelated intermediate view: ' +
              JSON.stringify({protectedSlots, arrivalSlots: arrivalPage.protect, published,
                  destination: Reflect.get(atlas, 'destination'), current: Reflect.get(atlas, 'current')}));
        atlas.prefetch(null);
    } finally {
        gl.bindFramebuffer(gl.FRAMEBUFFER, null); gl.deleteFramebuffer(framebuffer);
        atlas.dispose(); fixtureGPU.dispose(); globalThis.Worker = OriginalWorker;
        gl.getExtension('WEBGL_lose_context')?.loseContext();
    }
}
