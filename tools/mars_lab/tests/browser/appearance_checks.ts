/**
 * @file appearance_checks.ts
 * @brief Exercise skin selection, local artwork and optional browser storage in a real browser.
 *
 * The checks preserve the live worksheet and original preference. Detached controls exercise
 * unavailable storage, cross-tab events and listener disposal without changing application state.
 */
window.checkLabAppearance = async function checkLabAppearance(): Promise<void> {
    function check(ok: unknown, message: string): void {
        if (!ok) throw new Error('Appearance: ' + message);
    }
    const deadline = performance.now() + 30000;
    while (WorksheetRequests.labRequests.busy() && performance.now() < deadline)
        await new Promise(resolve => setTimeout(resolve, 20));
    check(!WorksheetRequests.labRequests.busy(), 'worksheet settles before checking decorative changes');
    await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    const names = ['Earth (Default)', 'Mars', 'Mercury', 'Jupiter', 'Venus', 'Saturn', 'Neptune',
        'Moon', 'Galaxy', 'Andromeda', 'Nebula', 'Quasar', 'Asteroids', 'Exoplanet'];
    const root = document.documentElement;
    const picker = document.getElementById('skinSelect') as HTMLSelectElement;
    const stored = localStorage.getItem(WorksheetAppearance.storageKey);
    const original = WorksheetAppearance.controller.current();
    const editor = WorksheetBindings.editor.currentExpressionText();
    const mode = WorksheetWorkspace.mode.currentMode();
    const rendered = WorksheetContext.rendered.innerHTML;
    const native = labWire.exports();
    const revisions = Array.from({length: 7}, (_, index) => native.lab_workspace_history_revision(index));
    try {
        check(picker.options.length === names.length, 'all requested skins have one selector entry');
        for (let index = 0; index < names.length; ++index) {
            check(picker.options[index].value === String(index) &&
                picker.options[index].textContent === `${index}. ${names[index]}`, 'numbered skin: ' + names[index]);
            picker.value = String(index);
            picker.dispatchEvent(new Event('change', {bubbles: true}));
            check(root.dataset.skin === String(index) &&
                localStorage.getItem(WorksheetAppearance.storageKey) === String(index), 'selection persists: ' + names[index]);
            const style = getComputedStyle(root);
            check(document.querySelector<HTMLMetaElement>('meta[name="theme-color"]')?.content ===
                style.getPropertyValue('--paper').trim(), 'browser chrome follows the palette');
            for (const panel of document.querySelectorAll<HTMLElement>('#workspacePanel, #resultWorkspacePanel')) {
                const frame = getComputedStyle(panel);
                const head = getComputedStyle(panel.querySelector<HTMLElement>('.panel-head')!);
                const expected = parseFloat(frame.borderTopLeftRadius) - parseFloat(frame.borderLeftWidth);
                check(parseFloat(head.borderTopLeftRadius) === expected &&
                    parseFloat(head.borderTopRightRadius) === expected, 'header fill follows the rounded frame');
            }
            check(getComputedStyle(document.getElementById('resultWorkspacePanel')!).overflow === 'visible',
                'flag previews can still extend beyond the result panel');
            if (index) {
                const source = style.getPropertyValue('--skin-art').trim();
                const match = /^url\("(\/skins\/[a-z]+\.svg)"\)$/.exec(source);
                check(match, 'skin uses local vector artwork: ' + names[index]);
                const response = await fetch(match![1]);
                check(response.ok && response.headers.get('Content-Type')?.startsWith('image/svg+xml'),
                    'artwork route serves SVG: ' + names[index]);
                const svg = new DOMParser().parseFromString(await response.text(), 'image/svg+xml');
                check(svg.documentElement.localName === 'svg' &&
                    !svg.querySelector('parsererror, script, image, foreignObject, animate, animateTransform'),
                    'valid static vector artwork: ' + names[index]);
                const image = new Image();
                image.src = match![1];
                await image.decode();
                check(image.naturalWidth > 0 && image.naturalHeight > 0, 'artwork decodes: ' + names[index]);
            }
        }
        check(editor === WorksheetBindings.editor.currentExpressionText(), 'skin changes preserve the editor');
        check(mode === WorksheetWorkspace.mode.currentMode(), 'skin changes preserve the mode');
        check(rendered === WorksheetContext.rendered.innerHTML, 'skin changes preserve the rendered result');
        check(revisions.every((revision, index) => revision === native.lab_workspace_history_revision(index)),
            'skin changes preserve every worksheet timeline');

        const detached = document.createElement('div');
        const select = picker.cloneNode(true) as HTMLSelectElement;
        const events = new EventTarget();
        let saved: string | null = '8', writes = 0;
        const controller = new WorksheetAppearance.SkinController(detached, {
            getItem: () => saved,
            setItem: (_key, value) => { saved = value; ++writes; }
        });
        try {
            controller.restore();
            controller.mount(select, null, events);
            check(controller.current() === '8' && select.value === '8' && writes === 0, 'restore before mounting without rewriting');
            select.value = '5';
            select.dispatchEvent(new Event('change'));
            check(controller.current() === '5' && saved === '5' && writes === 1, 'selector listener saves once');
            events.dispatchEvent(new StorageEvent('storage', {key: WorksheetAppearance.storageKey, newValue: '10'}));
            check(controller.current() === '10' && select.value === '10' && writes === 1, 'cross-tab changes do not echo writes');
            events.dispatchEvent(new StorageEvent('storage', {key: 'unrelated', newValue: '3'}));
            check(controller.current() === '10', 'unrelated storage ignored');
            events.dispatchEvent(new StorageEvent('storage', {key: null, newValue: null}));
            check(controller.current() === '0', 'cleared storage restores Earth');
            for (const invalid of [null, undefined, 5, -1, '14', '-1', '01', ' 1', '1.0', 'mars', '<svg>']) {
                controller.select(invalid, false);
                check(controller.current() === '0', 'invalid preference falls back to Earth');
            }
            controller.select('6', false);
            controller.dispose();
            select.value = '1';
            select.dispatchEvent(new Event('change'));
            events.dispatchEvent(new StorageEvent('storage', {key: WorksheetAppearance.storageKey, newValue: '5'}));
            check(controller.current() === '6' && writes === 1, 'disposed subscriptions cannot mutate the appearance');
        } finally {
            controller.dispose();
        }
        const denied = new WorksheetAppearance.SkinController(detached, {
            getItem: () => { throw new DOMException('Storage denied', 'SecurityError'); },
            setItem: () => { throw new DOMException('Storage denied', 'SecurityError'); }
        });
        try {
            denied.restore();
            check(denied.current() === '0', 'denied storage retains a usable default');
            denied.select('13');
            check(denied.current() === '13', 'live selection survives denied persistence');
        } finally {
            denied.dispose();
        }
    } finally {
        WorksheetAppearance.controller.select(original, false);
        if (stored === null) localStorage.removeItem(WorksheetAppearance.storageKey);
        else localStorage.setItem(WorksheetAppearance.storageKey, stored);
    }
};
