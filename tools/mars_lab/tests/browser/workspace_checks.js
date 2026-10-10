/** Isolated real-WASM workspace checks; parent invokes labWorkspaceChecks after loading this script. */
window.labWorkspaceChecks = async function labWorkspaceChecks() {
    const check = (ok, message) => {
        if (!ok)
            throw new Error(`Workspace: ${message}`);
    };
    const response = await fetch('/wasm/lab_browser.wasm', {cache: 'no-store'});
    check(response.ok, 'could not load the native module');
    const module = await WebAssembly.compile(await response.arrayBuffer());
    const imports = {};
    // Workspace functions must not delegate their state decisions to a host import.
    for (const entry of WebAssembly.Module.imports(module)) {
        check(entry.kind === 'function', 'unexpected non-function import');
        check(
            entry.module === 'env' && /^(lab_host_|lab_dom_)/.test(entry.name),
            `unexpected runtime import ${entry.module}.${entry.name}`);
        (imports[entry.module] ||= {})[entry.name] = () => {
            throw new Error(`Workspace unexpectedly called host import ${entry.name}`);
        };
    }
    const native = (await WebAssembly.instantiate(module, imports)).exports;
    // Exercise actual WASM runtime primitives, not host shims, including unaligned ranges and canaries.
    const memory = new Uint8Array(native.memory.buffer);
    const buffer = native.lab_workspace_input(0);
    for (const name of ['memset', 'memcpy', 'memmove', 'memcmp'])
        check(typeof native[name] === 'function', `missing native ${name}`);
    for (const length of [0, 1, 3, 31, 256, 4096]) {
        const source = buffer + 1, target = buffer + 8193;
        memory.fill(0x5a, buffer, target + length + 1);
        check(native.memset(source, 0x1ab, length) === source, 'memset return pointer');
        check(memory.subarray(source, source + length).every(byte => byte === 0xab), 'memset byte conversion');
        check(memory[source - 1] === 0x5a && memory[source + length] === 0x5a, 'memset touched a canary');
        check(native.memcpy(target, source, length) === target, 'memcpy return pointer');
        check(memory.subarray(target, target + length).every(byte => byte === 0xab), 'memcpy contents');
        check(memory[target - 1] === 0x5a && memory[target + length] === 0x5a, 'memcpy touched a canary');
        check(native.memcmp(source, target, length) === 0, 'memcmp equal ranges');
        if (length) {
            memory[target + length - 1] = 0x7f;
            check(native.memcmp(source, target, length) > 0, 'memcmp unsigned-byte ordering');
            check(native.memcmp(target, source, length) < 0, 'memcmp reverse ordering');
        }
        for (const [from, to] of [[1, 4], [4, 1], [1, 1], [1, 8193], [8193, 1]]) {
            const extent = Math.max(from, to) + length + 1;
            for (let i = 0; i < extent; ++i)
                memory[buffer + i] = i % 251;
            const expected = memory.slice(buffer, buffer + extent);
            expected.copyWithin(to, from, from + length);
            check(native.memmove(buffer + to, buffer + from, length) === buffer + to, 'memmove return pointer');
            check(
                memory.subarray(buffer, buffer + extent).every((byte, index) => byte === expected[index]),
                `memmove contents/canaries for ${from}, ${to}, ${length}`);
        }
    }
    // A one-past-memory pointer must not be dereferenced for a zero-length operation.
    const end = memory.length;
    check(native.memset(end, -1, 0) === end, 'zero-length memset accessed memory');
    check(native.memcpy(end, end, 0) === end, 'zero-length memcpy accessed memory');
    check(native.memmove(end, end, 0) === end, 'zero-length memmove accessed memory');
    check(native.memcmp(end, end, 0) === 0, 'zero-length memcmp accessed memory');
    const encoder = new TextEncoder();
    const decoder = new TextDecoder('utf-8', {fatal: true, ignoreBOM: true});
    const stage = (text, index = 0) => {
        const bytes = typeof text === 'string' ? encoder.encode(text) : text;
        check(bytes.length <= native.lab_workspace_capacity(), 'test input exceeds staging bound');
        new Uint8Array(native.memory.buffer, native.lab_workspace_input(index), bytes.length).set(bytes);
        return bytes.length;
    };
    const read = length => {
        check(length >= 0, 'native read failed');
        return decoder.decode(new Uint8Array(native.memory.buffer, native.lab_workspace_output(), length));
    };
    const push = (mode, side, text, flags = 0, hasText = true) =>
        native.lab_workspace_history_push(mode, side, stage(text), hasText, flags);
    const pop = (mode, side) => read(native.lab_workspace_history_pop(mode, side));

    native.lab_workspace_reset();
    const names = ['expression', 'equation', 'diffequation', 'matrix', 'integrator', 'datetime', 'almanac'];
    for (let mode = 0; mode < names.length; ++mode) {
        check(native.lab_workspace_mode_id(stage(names[mode])) === mode, `mode ${names[mode]} lookup`);
        check(native.lab_workspace_precision(mode) === (mode < 4 ? 256 : 53), 'default bit precision');
        check(
            native.lab_workspace_history_count(mode, 0) === 0 && native.lab_workspace_history_count(mode, 1) === 0,
            'new mode has history');
    }
    for (const text of ['', 'Matrix', '__proto__', 'matrix-extra'])
        check(native.lab_workspace_mode_id(stage(text)) === 0, 'unknown mode did not fall back');
    check(native.lab_workspace_select(4) === 1 && native.lab_workspace_select(4) === 0, 'mode change detection');
    check(
        native.lab_workspace_mode() === 4 && native.lab_workspace_select(99) === 1 && native.lab_workspace_mode() === 0,
        'invalid mode selection');

    check(
        native.lab_workspace_precision_set(4, 17) === 17 && native.lab_workspace_requested_precision(4) === 53,
        'legacy stored/requested floors differ');
    check(native.lab_workspace_request_precision(4, 1) === 53, 'interactive binary64 floor');
    check(
        native.lab_workspace_precision_set(4, 1280) === 1280 && native.lab_workspace_digits(1280) === 386,
        'screenshot precision mapping');
    check(native.lab_workspace_precision(0) === 256, 'precision leaked across modes');
    for (const value of [NaN, Infinity, -Infinity])
        check(native.lab_workspace_precision_set(4, value) === 1280, 'non-finite precision changed state');
    check(
        native.lab_workspace_precision_set(4, 2 ** 40) === 1048576 && native.lab_workspace_digits(1048576) === 315653,
        'maximum precision');
    check(native.lab_workspace_precision_set(4, -100) === 17, 'saved precision minimum');
    check(
        !native.lab_workspace_precision_can_step(4, -1) && native.lab_workspace_precision_can_step(4, 1),
        'precision control availability at the lower limit');
    native.lab_workspace_precision_set(4, 1048576);
    check(
        native.lab_workspace_precision_can_step(4, -1) && !native.lab_workspace_precision_can_step(4, 1),
        'precision control availability at the upper limit');
    for (const [bits, digits] of [[17, 17], [53, 17], [106, 32], [256, 78], [384, 116]])
        check(native.lab_workspace_digits(bits) === digits, 'decimal precision conversion');
    for (const [bits, next, previous] of [
             [53, 106, 53], [106, 256, 53], [256, 384, 106], [257, 384, 256], [384, 512, 256],
             [1048576, 1048576, 1048448]]) {
        check(native.lab_workspace_precision_step(bits, 1) === next, 'upward precision step');
        check(native.lab_workspace_precision_step(bits, -1) === previous, 'downward precision step');
    }

    const unicode = '\ufeffμσ e\u0301 🪐\u0000';
    check(native.lab_workspace_editor_set(0, stage(unicode)) === 1, 'editor set');
    stage('overwrite transfer buffer');
    check(read(native.lab_workspace_editor_get(0)) === unicode, 'editor not owned or UTF-8 changed');
    check(native.lab_workspace_editor_set(1, stage('equation')) === 1, 'second editor set');
    check(read(native.lab_workspace_editor_get(0)) === unicode, 'editor mode isolation');
    check(native.lab_workspace_editor_set(1, 0) === 1 && native.lab_workspace_editor_get(1) === 0, 'blank editor');
    check(
        native.lab_workspace_editor_set(7, 1) === 0 && native.lab_workspace_editor_get(7) === -1,
        'invalid editor mode');
    check(
        native.lab_workspace_editor_set(0, native.lab_workspace_capacity() + 1) === 0 &&
            read(native.lab_workspace_editor_get(0)) === unicode,
        'oversize editor changed existing text');
    check(
        native.lab_workspace_editor_set(0, -1) === 0 && native.lab_workspace_commit(0, -1) === 0 &&
            native.lab_workspace_history_push(0, 0, -1, 1, 0) === -1,
        'wrapped unsigned lengths accepted');

    // Exercise every mode against the actual C controller, including calendar overrides and empty defaults.
    for (let mode = 0; mode < 7; ++mode) {
        const authored = `authored ${mode} μ`, fallback = `default ${mode} σ`;
        native.lab_workspace_editor_set(mode, 0);
        check(
            read(native.lab_workspace_editor_restore(mode, stage(fallback, 1))) === fallback,
            'empty editor did not display its default');
        check(native.lab_workspace_editor_get(mode) === 0, 'restoration changed owned text');
        check(
            native.lab_workspace_editor_capture(mode, stage(authored), stage(fallback, 1)) === 1,
            'capture rejected valid text');
        const expected = mode < 5 ? authored : fallback;
        check(read(native.lab_workspace_editor_get(mode)) === expected, 'mode capture policy');
        check(
            native.lab_workspace_editor_capture(mode, 0, stage(fallback, 1)) === 1 &&
                read(native.lab_workspace_editor_get(mode)) === expected,
            'blank capture policy');
        native.lab_workspace_editor_set(mode, stage(authored));
        check(
            read(native.lab_workspace_editor_restore(mode, stage(fallback, 1))) === expected,
            'saved editor restoration policy');
        check(read(native.lab_workspace_editor_get(mode)) === authored, 'restoration overwrote saved editor');
        for (const bound of [0, 1, -1])
            check(
                !!native.lab_workspace_editor_bound(mode, bound) === (mode < 3 || (mode < 5 && !!bound)),
                'binding editor policy');
        check(
            read(native.lab_workspace_editor_restore(mode, 0)) === (mode < 5 ? authored : ''), 'empty default policy');
    }
    native.lab_workspace_editor_set(0, stage(unicode));
    native.lab_workspace_editor_get(0);
    for (const [mode, length, fallback] of [[7, 0, 0], [0, -1, 0], [0, 0, -1], [6, 0, 4194305]]) {
        check(native.lab_workspace_editor_capture(mode, length, fallback) === 0, 'invalid capture accepted');
        check(read(native.lab_workspace_editor_get(0)) === unicode, 'invalid capture changed editor');
    }
    for (const [mode, length] of [[7, 0], [0, -1], [6, 4194305]]) {
        check(native.lab_workspace_editor_restore(mode, length) === -1, 'invalid restoration accepted');
        check(read(new TextEncoder().encode(unicode).length) === unicode, 'invalid restoration changed output');
    }
    check(!native.lab_workspace_editor_bound(7, 1), 'invalid mode selected binding editor');
    for (let mode = 1; mode < 7; ++mode) native.lab_workspace_editor_set(mode, 0);

    const sourceFields = ['full μ\u0000', 'display σ', 'last 🪐', 'goal α', '1/3'];
    sourceFields.forEach((text, field) => {
        check(native.lab_workspace_source_set(field, stage(text)), 'source field set');
    });
    native.lab_workspace_editor_set(0, stage('shorter editor'));
    sourceFields.forEach((text, field) => {
        check(read(native.lab_workspace_source_get(field)) === text, 'source survived arena compaction');
    });
    check(
        read(native.lab_workspace_source_resolve(stage(sourceFields[1]), 0)) === sourceFields[0],
        'full source resolve');
    check(
        read(native.lab_workspace_source_resolve(stage(sourceFields[1]), 1)) === sourceFields[3],
        'goal source resolve');
    check(
        !native.lab_workspace_source_resolve(stage('changed'), 0) &&
            !native.lab_workspace_source_resolve(stage('changed'), 1),
        'changed display cannot revive old sources');
    check(
        !native.lab_workspace_source_set(5, 0) && !native.lab_workspace_source_set(0, -1) &&
            native.lab_workspace_source_get(5) === -1,
        'source field bounds');
    check(read(native.lab_workspace_source_get(0)) === sourceFields[0], 'invalid source update retains bytes');
    native.lab_workspace_source_set(1, 0);
    check(
        native.lab_workspace_source_matches(0) && !native.lab_workspace_source_resolve(0, 0),
        'empty display matches but cannot restore full source');
    sourceFields.forEach((_text, field) => native.lab_workspace_source_set(field, 0));
    native.lab_workspace_editor_set(0, stage(unicode));

    const a = stage('first', 0), b = stage('first', 1);
    check(native.lab_workspace_equal(a, b) === 1, 'byte equality');
    stage('other', 1);
    check(native.lab_workspace_equal(a, b) === 0, 'byte inequality');
    check(native.lab_workspace_commit(0, stage('committed')) === 1, 'commit');
    check(native.lab_workspace_previous(0, stage('committed')) === 0, 'unchanged commit');
    check(read(native.lab_workspace_previous(0, stage('changed'))) === 'committed', 'previous committed state');
    check(native.lab_workspace_previous(1, stage('changed')) === 0, 'commit leaked across modes');

    // Round-trip a real C-codec snapshot, not just synthetic arena bytes.
    const snapshot = {mode: 'integrator', text: 'exp(Li(x)) + μ', bounds: 'x:0:1', intervalCap: '5000'};
    const encoded = labWire.encode({state: snapshot});
    check(native.lab_workspace_commit(4, stage(encoded)) === 1, 'codec snapshot commit');
    snapshot.intervalCap = '20000';
    const restoredLength = native.lab_workspace_previous(4, stage(labWire.encode({state: snapshot})));
    const restoredBytes = new Uint8Array(native.memory.buffer, native.lab_workspace_output(), restoredLength).slice();
    const restored = labWire.decode(restoredBytes.buffer).state;
    check(
        restored.text === snapshot.text && restored.bounds === snapshot.bounds && restored.intervalCap === '5000',
        'committed codec snapshot lost form metadata or borrowed host mutation');
    check(
        historyStatesEqual(restored, {...restored}) && !historyStatesEqual(restored, snapshot),
        'JS history adapter did not delegate complete equality');
    check(
        validPrecisionBits('17', 256) === 17 && validPrecisionBits('invalid', 256) === 256 &&
            validPrecisionBits('  +1280suffix', 256) === 1280,
        'DOM precision conversion/legacy validation');

    check(push(0, 0, 'A', 3) === 1 && push(0, 0, 'A', 3) === 1, 'adjacent deduplication');
    check(push(0, 1, 'redo') === 1, 'forward push');
    check(
        push(0, 0, 'blank', 3, false) === 1 && native.lab_workspace_history_count(0, 1) === 0,
        'blank submission must clear forward but not push');
    check(
        push(0, 1, 'redo') === 1 && push(0, 0, 'A', 3) === 1 && native.lab_workspace_history_count(0, 1) === 0,
        'duplicate submission must clear forward');
    check(push(1, 0, 'equation') === 1, 'other mode history');
    check(read(native.lab_workspace_navigate(0, 0, stage('B'), 1)) === 'A', 'back restoration');
    check(
        native.lab_workspace_history_count(0, 0) === 0 && native.lab_workspace_history_count(0, 1) === 1,
        'back must save current snapshot on forward stack');
    check(read(native.lab_workspace_navigate(0, 1, stage('A'), 1)) === 'B', 'forward restoration');
    check(native.lab_workspace_history_count(1, 0) === 1, 'navigation changed another mode');
    check(
        read(native.lab_workspace_navigate(0, 0, stage('blank'), 0)) === 'A' &&
            native.lab_workspace_history_count(0, 1) === 0,
        'blank current state entered forward history');
    check(native.lab_workspace_navigate(0, 0, stage('unused'), 1) === 0, 'empty navigation');
    check(
        native.lab_workspace_navigate(1, 0, native.lab_workspace_capacity() + 1, 1) === -1 && pop(1, 0) === 'equation',
        'invalid navigation mutated history');

    for (let i = 0; i < 140; ++i) push(2, 0, `entry-${i}`);
    check(native.lab_workspace_history_count(2, 0) === 128, 'entry bound not enforced');
    for (let i = 139; i >= 12; --i) check(pop(2, 0) === `entry-${i}`, 'bounded LIFO order');
    check(pop(2, 0) === '', 'oldest entries were not evicted');

    // Exercise the full protocol-sized byte capacity, beyond the former 128 KiB limit.
    native.lab_workspace_reset();
    const large = new Uint8Array(native.lab_workspace_capacity()).fill(65);
    check(native.lab_workspace_editor_set(6, stage(large)) === 1, '4 MiB editor rejected');
    check(native.lab_workspace_commit(6, stage(large)) === 1, '4 MiB committed snapshot rejected');
    for (let i = 0; i < 9; ++i) {
        large[0] = 66 + i;
        check(native.lab_workspace_history_push(0, 0, stage(large), 1, 0) > 0, 'arena pressure push');
    }
    check(native.lab_workspace_history_count(0, 0) === 6, 'shared byte limit/eviction');
    for (let i = 8; i >= 3; --i) {
        const length = native.lab_workspace_history_pop(0, 0);
        const bytes = new Uint8Array(native.memory.buffer, native.lab_workspace_output(), length);
        check(
            length === large.length && bytes[0] === 66 + i && bytes[length - 1] === 65,
            'large snapshot damaged during arena compaction');
    }
    check(
        native.lab_workspace_editor_get(6) === large.length &&
            new Uint8Array(native.memory.buffer, native.lab_workspace_output(), 1)[0] === 65,
        'history pressure evicted active editor');
    check(
        native.lab_workspace_previous(6, stage('different')) === large.length &&
            new Uint8Array(native.memory.buffer, native.lab_workspace_output(), 1)[0] === 65,
        'history pressure evicted committed snapshot');

    // Fill the pool with non-evictable owners; rejected writes must not discard state.
    native.lab_workspace_reset();
    for (let mode = 0; mode < 4; ++mode) {
        check(native.lab_workspace_editor_set(mode, stage(large)) === 1, 'active editor pool fill');
        check(native.lab_workspace_commit(mode, stage(large)) === 1, 'committed pool fill');
    }
    check(
        native.lab_workspace_editor_set(4, stage('extra')) === 0 &&
            native.lab_workspace_commit(4, stage('extra')) === 0 && push(4, 0, 'extra') === -1,
        'pool exhaustion was not transactional');
    check(
        native.lab_workspace_editor_set(0, stage('replacement')) === 1 &&
            read(native.lab_workspace_editor_get(0)) === 'replacement',
        'replacement did not reclaim old bytes');
    check(native.lab_workspace_history_count(4, 0) === 0, 'failed push created history');

    // At capacity, forward invalidation itself supplies the bytes for a new back entry.
    native.lab_workspace_reset();
    for (let mode = 0; mode < 3; ++mode) {
        check(native.lab_workspace_editor_set(mode, stage(large)) === 1, 'invalidation editor fill');
        check(native.lab_workspace_commit(mode, stage(large)) === 1, 'invalidation commit fill');
    }
    check(native.lab_workspace_history_push(0, 0, stage(large), 1, 0) === 1, 'retained back entry');
    large[0] = 90;
    check(native.lab_workspace_history_push(0, 1, stage(large), 1, 0) === 1, 'discarded forward entry');
    large[0] = 91;
    check(
        native.lab_workspace_history_push(0, 0, stage(large), 1, 3) === 2 &&
            native.lab_workspace_history_count(0, 1) === 0,
        'forward invalidation unnecessarily evicted back history');

    // A full destination stack releases its oldest entry before considering another mode's history.
    native.lab_workspace_reset();
    for (let mode = 0; mode < 4; ++mode) {
        check(native.lab_workspace_editor_set(mode, stage(large)) === 1, 'entry-limit editor fill');
        check(native.lab_workspace_commit(mode, stage(large)) === 1, 'entry-limit commit fill');
    }
    check(
        native.lab_workspace_editor_set(0, stage(large.subarray(0, large.length - 129))) === 1,
        'entry-limit byte reservation');
    check(push(0, 0, 'Z') === 1, 'other mode sentinel');
    for (let i = 0; i < 128; ++i) check(push(1, 0, 'X') === i + 1, 'entry-limit stack fill');
    check(
        push(1, 0, 'Y') === 128 && native.lab_workspace_history_count(0, 0) === 1 && pop(0, 0) === 'Z',
        'entry-limit replacement unnecessarily evicted another mode');

    // Failed replacements/navigation must leave both stacks, existing owners and output untouched.
    native.lab_workspace_reset();
    for (let mode = 0; mode < 4; ++mode) {
        check(native.lab_workspace_editor_set(mode, stage(large)) === 1, 'failure editor fill');
        check(native.lab_workspace_commit(mode, stage(large)) === 1, 'failure commit fill');
    }
    check(
        native.lab_workspace_editor_set(0, stage(large.subarray(0, large.length - 4))) === 1,
        'could not reserve four test bytes');
    check(
        native.lab_workspace_editor_set(4, stage('E')) === 1 && native.lab_workspace_commit(4, stage('C')) === 1,
        'small retained owners');
    check(push(0, 0, 'A') === 1 && push(0, 1, 'B') === 1, 'small retained stacks');
    check(read(native.lab_workspace_editor_get(4)) === 'E', 'output sentinel');
    check(
        native.lab_workspace_editor_set(4, stage('longer')) === 0 &&
            native.lab_workspace_commit(4, stage('longer')) === 0 &&
            native.lab_workspace_history_push(0, 0, stage('longer'), 1, 3) === -1 &&
            native.lab_workspace_navigate(0, 0, stage('longer'), 1) === -1,
        'impossible writes were not rejected');
    check(
        read(1) === 'E' && native.lab_workspace_history_count(0, 0) === 1 &&
            native.lab_workspace_history_count(0, 1) === 1,
        'failed mutation changed output or history');
    check(
        read(native.lab_workspace_editor_get(4)) === 'E' &&
            read(native.lab_workspace_previous(4, stage('different'))) === 'C',
        'failed replacement lost an owner');
    check(
        native.lab_workspace_editor_set(4, stage('E')) === 1 && native.lab_workspace_commit(4, stage('C')) === 1 &&
            native.lab_workspace_history_count(0, 0) === 1,
        'identical replacement evicted history');
    check(
        read(native.lab_workspace_navigate(0, 0, stage('N'), 1)) === 'A' && pop(0, 1) === 'N' && pop(0, 1) === 'B',
        'successful navigation lost existing forward state');

    check(native.lab_workspace_prefer_local(1, 20, 10, 1, 0) === 1, 'newer local copy');
    check(native.lab_workspace_prefer_local(1, 10, 20, 1, 0) === 0, 'older local copy');
    check(native.lab_workspace_prefer_local(1, 10, 10, 1, 0) === 0, 'equal timestamp priority');
    check(native.lab_workspace_prefer_local(1, 0, 20, 0, 0) === 1, 'missing server copy');
    check(native.lab_workspace_prefer_local(1, 0, 20, 1, 1) === 1, 'default server copy');
    check(native.lab_workspace_prefer_local(0, 30, 20, 1, 1) === 0, 'empty local copy');
    native.lab_workspace_reset();
    check(
        native.lab_workspace_mode() === 0 && native.lab_workspace_editor_get(0) === 0 &&
            native.lab_workspace_precision(4) === 53,
        'complete reset');

    // Active source fields consume the same arena budget and cannot be evicted to admit history.
    for (let field = 0; field < 5; ++field)
        check(native.lab_workspace_source_set(field, stage(large)), 'pinned source fill');
    for (let mode = 0; mode < 3; ++mode)
        check(native.lab_workspace_editor_set(mode, stage(large)), 'pinned editor fill');
    check(
        native.lab_workspace_history_push(0, 0, stage('new history'), 1, 0) === -1,
        'history cannot evict pinned source fields');
    check(!native.lab_workspace_editor_set(3, stage('new editor')), 'full arena rejects new owner');
    check(native.lab_workspace_source_get(4) === large.length, 'failed allocation retains goal target');
    check(
        native.lab_workspace_source_set(0, 0) && push(0, 0, 'history after release') === 1,
        'source release makes storage available');
    check(native.lab_workspace_source_get(4) === large.length, 'source survives release compaction');
    native.lab_workspace_reset();
    for (let field = 0; field < 5; ++field)
        check(native.lab_workspace_source_get(field) === 0, 'reset clears source fields');

    // Exercise the live browser views too: values are read back from C, not a second JS owner.
    const savedSource = {...labEditorState};
    const savedInput = expr.value;
    try {
        labEditorState.fullText = '{ x | x = 1/3 }';
        labEditorState.displayText = 'displayed source fixture';
        labEditorState.goalSource = 'goal source fixture';
        labEditorState.goalTarget = '7/3';
        expr.value = labEditorState.displayText;
        check(currentExpressionText() === labEditorState.fullText, 'browser restores exact retained source');
        check(currentGoalSeekSource() === labEditorState.goalSource, 'browser restores current goal source');
        expr.value = 'changed source fixture';
        check(
            currentExpressionText() === expr.value && currentGoalSeekSource() === '',
            'browser rejects retained sources after authored input changes');
        check(labEditorState.goalTarget === '7/3', 'exact goal target is retained separately');
    } finally {
        Object.assign(labEditorState, savedSource);
        expr.value = savedInput;
    }

    // Resolve old preparation only after a newer editor owns the UI.
    const originalPrepare = prepareLabEditor;
    const originalBounds = restoreIntegratorBoundsText;
    const visibleBefore = {
        text: expr.value,
        operation: matrixOperation.value,
        operand: matrixOperand.value,
        cap: integratorIntervalCap.value
    };
    try {
        let release;
        let owned = true;
        const guard = () => owned;
        prepareLabEditor = () => new Promise(resolve => {
            release = resolve;
        });
        const pending =
            restoreHistoryState({mode: 'matrix', text: 'stale', operation: 'multiply', operand: 'old'}, guard);
        owned = false;
        expr.value = 'newer editor';
        matrixOperation.value = 'eval';
        matrixOperand.value = 'newer operand';
        release();
        check(
            await pending === false && expr.value === 'newer editor' && matrixOperation.value === 'eval' &&
                matrixOperand.value === 'newer operand',
            'stale editor preparation wrote restored DOM');

        let entered;
        let forwardedGuard;
        const reachedBounds = new Promise(resolve => {
            entered = resolve;
        });
        prepareLabEditor = async () => {};
        restoreIntegratorBoundsText = (_text, isCurrent) => {
            forwardedGuard = isCurrent;
            return new Promise(resolve => {
                release = resolve;
                entered();
            });
        };
        owned = true;
        const boundsPending =
            restoreHistoryState({mode: 'integrator', text: 'stale', bounds: 'x:0:1', intervalCap: '5000'}, guard);
        await reachedBounds;
        owned = false;
        expr.value = 'latest editor';
        integratorIntervalCap.value = '20000';
        release();
        check(
            await boundsPending === false && forwardedGuard === guard && !forwardedGuard() &&
                integratorIntervalCap.value === '20000' && expr.value === 'latest editor',
            'stale bounds restoration wrote cap/editor or did not forward ownership');
    } finally {
        prepareLabEditor = originalPrepare;
        restoreIntegratorBoundsText = originalBounds;
        expr.value = visibleBefore.text;
        matrixOperation.value = visibleBefore.operation;
        matrixOperand.value = visibleBefore.operand;
        integratorIntervalCap.value = visibleBefore.cap;
    }
};
