/**
 * MARS Lab startup. Definition-only scripts load first; workspace references and native
 * event registration follow the catalogue. Ordered classic scripts deliberately retain
 * the existing shared lexical state without a bundler or mathematical rewriting.
 */
let labConfig;
let labCatalogue;
let labDefinitionScripts = [];

async function labLoadScript(path) {
    await new Promise((resolve, reject) => {
        const script = document.createElement('script');
        const executionError = event => reject(new Error(event.message || `Could not execute ${path}`));
        script.src = path;
        window.addEventListener('error', executionError);
        script.onload = () => {
            window.removeEventListener('error', executionError);
            resolve();
        };
        script.onerror = () => {
            window.removeEventListener('error', executionError);
            reject(new Error(`Could not load ${path}`));
        };
        document.head.appendChild(script);
    });
}

async function labStart() {
    await labLoadScript('/js/transport.js');
    await labWire.start();
    return labDOM.flow(64, {}, {}, {
        fetch: url => labFetch(url),
        decode: response => response.labData(),
        native: (entry, ...args) => labDOM.call(entry, ...args),
        config: value => labConfig = value,
        catalogue: value => labCatalogue = value,
        definitions: names => {
            labDefinitionScripts = names;
            return Promise.all(names.map(name => labLoadScript(`/js/${name}.js`)));
        },
        script: path => labLoadScript(path),
        events: () => labStartEventBindings(),
        widgets: () => labInitialiseBrowserWidgets(),
        worksheet: () => {
            labInitialiseWorksheet();
            return window.labInitialEvaluation;
        },
        fail: message => {
            throw new Error(message);
        }
    });
}

window.labReady = labStart().catch((error) => {
    document.getElementById('status').textContent = 'Startup failed';
    document.getElementById('rendered').textContent = error.message;
    document.getElementById('run').disabled = true;
    throw error;
});

/** Bind generic host services; C owns control subscriptions and dispatch policy. */
function labStartEventBindings() {
    labDOM.bindEvents(Object.freeze({
        clearForwardHistory,
        clearGoalSeekRequest,
        hideTargetEntry,
        evaluateCurrentMode,
        navigateHistoryFromEvent,
        evaluateFromKeyboard,
        selectWorksheetMode,
        normaliseWorksheetControl,
        startGoalSeekFromEvent,
        changeWorksheetPrecision,
        toggleTextDigits,
        copyResultFromEvent,
        flushWorksheetState,
        scheduleWorkspacePanelFit,
        openMarsDatePicker,
        closeMarsDatePicker,
        applyCalendarControlEvent,
        refreshWorksheetFromInput,
        clearWorksheetFromEvent,
        toggleHelp,
        formatAlmanacTimeFromEvent,
        toggleRenderedDigits,
        runFunctionCard,
        sendResultExpressionToInput,
        copyInputFromEvent,
        scheduleEditorResizeGrip,
        commitMarsDateValue,
        shiftMarsDatePickerMonth,
        shiftMarsDatePickerYear,
        setMarsDatePickerMonthYear,
        commitMarsTodayValue,
        placeMarsDatePicker,
        showButtonTooltip,
        hideButtonTooltip,
        markDatetimeOffsetTouched,
        setAlmanacVisibility,
        saveLastAlmanacState,
        renderAlmanacWorksheet,
        refreshAlmanacLandTotality,
        evaluateAlmanac,
        applyAlmanacTotalityAction,
        setStatus
    }));
    labDOM.call('lab_events_install', document, window);
    labDOM.call('lab_widget_events_install', document, window, marsDatePickerState);
    if (typeof ResizeObserver === 'function') {
        const observer = new ResizeObserver(scheduleEditorResizeGrip);
        labTextareas.forEach(textarea => observer.observe(textarea));
    }
}

/** Browser observers and host callbacks for controls whose policies are native. */
function labInitialiseBrowserWidgets() {
    labDOM.services(labDOM.call('lab_bootstrap_widgets'), {enhance: (...args) => enhanceRoundedSelect(...args)});
    if (typeof ResizeObserver === 'function') {
        new ResizeObserver(scheduleRenderedTeXFit).observe(rendered);
        new ResizeObserver(scheduleSolverTexFit).observe(functionStyle);
    } else {
        window.addEventListener('resize', scheduleRenderedTeXFit);
        window.addEventListener('resize', scheduleSolverTexFit);
    }
}

/** Record an authored offset independently of asynchronous location responses. */
function markDatetimeOffsetTouched() {
    datetimeGmtOffsetTouched = true;
}

/** Start periodic browser services and await the initial native calculation. */
function labInitialiseWorksheet() {
    syncModeTabs();
    syncModeUI();
    setStatus('Ready');
    refreshMobileAccess();
    setInterval(refreshMobileAccess, 5000);
    window.labInitialEvaluation = labFlowContinue(62, {});
}
