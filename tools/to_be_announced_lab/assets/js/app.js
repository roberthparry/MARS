const tbaConfig = JSON.parse(document.getElementById("tba-config").textContent);
const basePath = tbaConfig.base_path;
const defaults = tbaConfig.state;
const initialTargetColumns = tbaConfig.target_columns;
const initialTargetMetaMap = tbaConfig.target_meta_map;
const initialXregColumns = tbaConfig.xreg_columns;
const initialTargetMeta = tbaConfig.target_meta;
const initialXregMeta = tbaConfig.xreg_meta;
const controlToken = tbaConfig.control_token;
const form = document.getElementById('forecast-form');
const statusNode = document.getElementById('status');
const runButton = document.getElementById('run-button');
const suggestButton = document.getElementById('suggest-button');
const chartToggle = document.getElementById('chart-toggle');
const helpToggle = document.getElementById('help-toggle');
const summaryBox = document.getElementById('summary-box');
const forecastChartBody = document.getElementById('forecast-chart-body');
const forecastHead = document.getElementById('forecast-head');
const forecastBody = document.getElementById('forecast-body');
const metricModel = document.getElementById('metric-model');
const metricFitRows = document.getElementById('metric-fit-rows');
const metricStationary = document.getElementById('metric-stationary');
const metricInvertible = document.getElementById('metric-invertible');
const downloadSummary = document.getElementById('download-summary');
const downloadForecast = document.getElementById('download-forecast');
const layout = document.querySelector('.layout');
const resultGrid = document.querySelector('.result-grid');
const chartPane = document.getElementById('chart-pane');
const helpPane = document.getElementById('help-pane');
const mobileTitle = document.getElementById('mobile-title');
const mobileStatus = document.getElementById('mobile-status');
const mobileUrl = document.getElementById('mobile-url');
const mobileHint = document.getElementById('mobile-hint');
const qrBox = document.getElementById('qr-box');
const refreshMobile = document.getElementById('refresh-mobile');
const targetUpload = document.getElementById('target-upload');
const targetUploadName = document.getElementById('target-upload-name');
const targetPathReadout = document.getElementById('target-path-readout');
const targetMetaBox = document.getElementById('target-meta');
const xregUpload = document.getElementById('xreg-upload');
const xregUploadName = document.getElementById('xreg-upload-name');
const xregPathReadout = document.getElementById('xreg-path-readout');
const xregMetaBox = document.getElementById('xreg-meta');
const xregColumnsInput = form.elements.namedItem('xreg_columns');
const xregColumnsSummary = document.getElementById('xreg-columns-summary');
const xregColumnsList = document.getElementById('xreg-columns-list');
const xregColumnsStatus = document.getElementById('xreg-columns-status');
const xregColumnsDebug = document.getElementById('xreg-columns-debug');
const modelSummary = document.getElementById('model-summary');
const modelList = document.getElementById('model-list');
const targetPickerStatus = document.getElementById('target-picker-status');
const targetValueSelect = form.elements.namedItem('target_value_column');
const forecastEndInput = form.elements.namedItem('forecast_end_date');
const detectedFrequencyReadout = document.getElementById('detected-frequency-readout');
const seasonalSetupWarning = document.getElementById('seasonal-setup-warning');
const outlierPanel = document.querySelector('.outlier-panel');
const outlierList = document.getElementById('outlier-list');
const outlierStatus = document.getElementById('outlier-status');
const outlierModeSelect = form.elements.namedItem('outlier_mode');
const outlierDatesInput = form.elements.namedItem('outlier_dates');
const driversUsedNote = document.getElementById('drivers-used-note');
const targetDateSelect = form.elements.namedItem('target_date_column');
const xregDateSelect = form.elements.namedItem('xreg_date_column');
const modelsField = form.elements.namedItem('models');
let forecastEndOptionsHydrated = false;
let targetMetaRequestId = 0;
const targetMetaCache = Object.assign({}, initialTargetMetaMap || {});
let latestSummary = '';
let latestForecastCsv = '';
window.__to_be_announcedLatestSummary = '';
window.__to_be_announcedLatestForecastCsv = '';
let saveTimer = null;
let initialHydrating = true;
let currentOutliers = tbaConfig.outliers;
let seasonPeriodManuallyEdited = false;
const modelOrderEdited = {
    p: false,
    d: false,
    q: false,
    P: false,
    D: false,
    Q: false,
};
const modelLabels = {
    'regression': 'Regression',
    'arima': 'ARIMA',
    'arimax': 'ARIMAX',
    'sarima': 'SARIMA',
    'sarimax': 'SARIMAX',
    'auto-arima': 'Auto-ARIMA',
};

function seasonPeriodOptionsForFrequency(frequency) {
    const token = String(frequency || '').trim().toLowerCase();
    if (token === 'monthly') {
        return [
            {value: '0', label: 'None'},
            {value: '1', label: 'Weak / unclear'},
            {value: '3', label: 'Quarterly'},
            {value: '4', label: 'Every 4 months'},
            {value: '6', label: 'Half-yearly'},
            {value: '12', label: 'Yearly'},
        ];
    }
    if (token === 'quarterly') {
        return [
            {value: '0', label: 'None'},
            {value: '1', label: 'Weak / unclear'},
            {value: '2', label: 'Half-yearly'},
            {value: '4', label: 'Yearly'},
        ];
    }
    if (token === 'yearly') {
        return [
            {value: '0', label: 'None'},
            {value: '1', label: 'Weak / unclear'},
        ];
    }
    return [
        {value: '0', label: 'None'},
        {value: '1', label: 'Weak / unclear'},
    ];
}

function renderSeasonPeriodOptions(frequency, selectedValue = '') {
    const seasonField = form.elements.namedItem('season_period');
    const options = seasonPeriodOptionsForFrequency(frequency);
    const selected = String(selectedValue || '').trim();
    const fallback = options.length ? options[0].value : '0';
    const chosen = options.some((option) => option.value === selected) ? selected : fallback;
    seasonField.innerHTML = options
                                .map((option) => {
                                    const selectedAttr = option.value === chosen ? ' selected' : '';
                                    return `<option value="${escapeHtml(option.value)}"${selectedAttr}>${
                                        escapeHtml(option.label)}</option>`;
                                })
                                .join('');
    seasonField.value = chosen;
}

function updateSeasonalSetupWarning() {
    if (!seasonalSetupWarning)
        return;
    const selectedModels = currentSelectedModels();
    const strength = String(targetMetaBox && targetMetaBox.dataset.seasonalityStrength || '').trim().toLowerCase();
    const label = String(targetMetaBox && targetMetaBox.dataset.seasonalityLabel || '').trim();
    const lag = Number.parseInt(String(targetMetaBox && targetMetaBox.dataset.seasonalityLag || '').trim(), 10);
    const seasonPeriod = String(form.elements.namedItem('season_period').value || '').trim();
    const P = String(form.elements.namedItem('P').value || '').trim();
    const D = String(form.elements.namedItem('D').value || '').trim();
    const Q = String(form.elements.namedItem('Q').value || '').trim();
    const seasonalModel = selectedModels.some((model) => isSeasonalModel(model));

    let level = '';
    let title = '';
    let message = '';

    if (seasonalModel) {
        if (strength === 'none') {
            if (seasonPeriod !== '0' || P !== '0' || D !== '0' || Q !== '0') {
                level = 'rating-mediocre';
                title = 'Seasonal settings check';
                message =
                    'No seasonality was detected, so seasonal settings may be adding unnecessary complexity. A safer first try is Season length = None and P, D, Q all set to 0.';
            } else {
                level = 'rating-good';
                title = 'Seasonal settings check';
                message =
                    'No seasonality was detected, and your seasonal settings are currently turned off. That is a sensible starting point.';
            }
        } else if (strength === 'weak') {
            if (seasonPeriod !== '1' || P !== '0') {
                level = 'rating-mediocre';
                title = 'Seasonal settings check';
                message =
                    'Only a weak seasonal signal was detected. Keep the seasonal structure light: Weak / unclear season length and usually P = 0 are safer starting choices.';
            } else {
                level = 'rating-good';
                title = 'Seasonal settings check';
                message =
                    'Only a weak seasonal signal was detected, and your seasonal settings are staying light. That is a sensible starting point.';
            }
        } else if (lag > 1) {
            if (seasonPeriod !== String(lag) || P !== '1') {
                level = 'rating-poor';
                title = 'Seasonal settings check';
                message = `${
                    label ||
                    'A strong repeating seasonal pattern was detected.'} Your current seasonal settings do not match that pattern closely. A better first try is Season length = ${
                    lag} and P = 1.`;
            } else {
                level = 'rating-good';
                title = 'Seasonal settings check';
                message = `${
                    label ||
                    'A repeating seasonal pattern was detected.'} Your current seasonal settings match that pattern reasonably well for a first run.`;
            }
        }
    } else if ((strength === 'moderate' || strength === 'strong' || strength === 'very strong') && lag > 1) {
        level = 'rating-mediocre';
        title = 'Seasonality note';
        message = `${
            label ||
            'A repeating seasonal pattern was detected.'} The selected models are non-seasonal, so they will not use Season length or P, D, Q. Consider SARIMA or SARIMAX if you want the model to use that seasonal pattern.`;
    }

    if (!message) {
        seasonalSetupWarning.innerHTML = '';
        seasonalSetupWarning.className = 'setup-advisory hidden';
        return;
    }
    seasonalSetupWarning.innerHTML = `<strong>${escapeHtml(title)}:</strong> ${escapeHtml(message)}`;
    seasonalSetupWarning.className = `setup-advisory ${level}`.trim();
}

function suggestedSeasonLength(frequency) {
    const strength = String(targetMetaBox && targetMetaBox.dataset.seasonalityStrength || '').trim().toLowerCase();
    const lag = Number.parseInt(String(targetMetaBox && targetMetaBox.dataset.seasonalityLag || '').trim(), 10);
    if ((strength === 'very strong' || strength === 'strong' || strength === 'moderate') && Number.isFinite(lag) &&
        lag > 1) {
        return String(lag);
    }
    if (strength === 'none')
        return '0';
    if (strength === 'weak')
        return '1';
    const token = String(frequency || '').trim().toLowerCase();
    if (token === 'monthly')
        return '12';
    if (token === 'quarterly')
        return '4';
    if (token === 'yearly')
        return '1';
    return '';
}

function suggestedSeasonalLookback() {
    const strength = String(targetMetaBox && targetMetaBox.dataset.seasonalityStrength || '').trim().toLowerCase();
    if (strength === 'none' || strength === 'weak')
        return '0';
    if (strength === 'moderate' || strength === 'strong' || strength === 'very strong')
        return '1';
    return '0';
}

function hasClearSeasonalitySuggestion() {
    const strength = String(targetMetaBox && targetMetaBox.dataset.seasonalityStrength || '').trim().toLowerCase();
    const lag = Number.parseInt(String(targetMetaBox && targetMetaBox.dataset.seasonalityLag || '').trim(), 10);
    return (strength === 'moderate' || strength === 'strong' || strength === 'very strong') && Number.isFinite(lag) &&
        lag > 1 && currentSelectedModels().some((model) => isSeasonalModel(model));
}

function suggestedModelSettings() {
    const model = preferredSuggestionModel();
    const frequency = String(form.elements.namedItem('frequency').value || '').trim().toLowerCase();
    const seasonPeriod =
        suggestedSeasonLength(frequency) || String(form.elements.namedItem('season_period').value || '').trim() || '0';
    const seasonalLookback = suggestedSeasonalLookback();
    const suggested = {
        criterion: 'aic',
        level: '0.95',
        season_period: seasonPeriod,
        p: '1',
        d: '1',
        q: '0',
        P: '0',
        D: '0',
        Q: '0',
    };

    if (model === 'regression') {
        suggested.p = '0';
        suggested.d = '0';
        suggested.q = '0';
        suggested.P = '0';
        suggested.D = '0';
        suggested.Q = '0';
    } else if (model === 'sarima' || model === 'sarimax') {
        suggested.P = seasonalLookback;
        suggested.D = '0';
        suggested.Q = '0';
    } else if (model === 'auto-arima') {
        suggested.p = '2';
        suggested.d = '1';
        suggested.q = '1';
        suggested.P = seasonalLookback;
        suggested.D = '0';
        suggested.Q = '0';
    }

    return suggested;
}

function applySuggestedModelSettings(force = false) {
    const suggested = suggestedModelSettings();
    const seasonField = form.elements.namedItem('season_period');
    renderSeasonPeriodOptions(form.elements.namedItem('frequency').value, String(seasonField.value || '').trim());
    const clearSeasonalitySuggestion = hasClearSeasonalitySuggestion();
    let changed = false;
    const setFieldValue = (field, value) => {
        const next = String(value ?? '');
        if (String(field.value || '') !== next) {
            field.value = next;
            changed = true;
        }
    };
    const applyValue = (name, value, edited = false) => {
        const field = form.elements.namedItem(name);
        if (!field)
            return;
        const current = String(field.value || '').trim();
        const canReplaceUntouchedDefault = clearSeasonalitySuggestion && (current === '' || current === '0');
        if (force || canReplaceUntouchedDefault || !edited || !current) {
            setFieldValue(field, value);
        }
    };

    applyValue('criterion', suggested.criterion);
    applyValue('level', suggested.level);
    applyValue('p', suggested.p, modelOrderEdited.p);
    applyValue('d', suggested.d, modelOrderEdited.d);
    applyValue('q', suggested.q, modelOrderEdited.q);
    applyValue('P', suggested.P, modelOrderEdited.P);
    applyValue('D', suggested.D, modelOrderEdited.D);
    applyValue('Q', suggested.Q, modelOrderEdited.Q);
    if (seasonField &&
        (force || clearSeasonalitySuggestion || !seasonPeriodManuallyEdited ||
         !String(seasonField.value || '').trim())) {
        setFieldValue(seasonField, suggested.season_period);
    }
    updateSeasonalSetupWarning();
    if (changed && !initialHydrating) {
        scheduleStateSave();
    }
}

function applyState(state) {
    for (const [key, value] of Object.entries(state || {})) {
        const field = form.elements.namedItem(key);
        if (!field)
            continue;
        field.value = value;
    }
}

function renderModelChecksFromState(selectedText) {
    const selected = splitSelectedModels(selectedText);
    const set = new Set(selected);
    const inputs = modelList ? Array.from(modelList.querySelectorAll('input[name="model-choice"]')) : [];
    if (!inputs.length)
        return;
    let checkedCount = 0;
    inputs.forEach((input, index) => {
        const shouldCheck = set.size ? set.has(String(input.value || '').trim().toLowerCase()) : index === 0;
        input.checked = shouldCheck;
        if (shouldCheck)
            checkedCount += 1;
    });
    if (!checkedCount) {
        inputs[0].checked = true;
    }
}

function collectState() {
    const payload = {};
    for (const element of form.elements) {
        if (!element.name)
            continue;
        payload[element.name] = element.value;
    }
    const selectedModels = currentSelectedModels();
    payload.models = selectedModels.join(', ');
    payload.xreg_columns = Array.from(xregColumnsList.querySelectorAll('input[name="xreg-column-choice"]:checked'))
                               .map((input) => input.value)
                               .join(', ');
    payload.outlier_dates = Array.from(outlierList.querySelectorAll('input[name="outlier-choice"]:checked'))
                                .map((input) => input.value)
                                .join(', ');
    return payload;
}

function targetFilenameFromPath(pathText) {
    const raw = String(pathText || '').trim();
    if (!raw)
        return 'No file chosen';
    const parts = raw.split(/[\\/]/).filter(Boolean);
    return parts.length ? parts[parts.length - 1] : raw;
}

function syncTargetUploadName(pathText, displayName = '') {
    const label = String(displayName || '').trim() || targetFilenameFromPath(pathText);
    form.elements.namedItem('target_path').value = String(pathText || '').trim();
    form.elements.namedItem('target_display_name').value = String(displayName || '').trim();
    targetPathReadout.value = label;
    targetPathReadout.title = label;
    targetUploadName.textContent = label;
}

function syncXregUploadName(pathText, displayName = '') {
    const label = String(displayName || '').trim() || targetFilenameFromPath(pathText);
    form.elements.namedItem('xreg_path').value = String(pathText || '').trim();
    form.elements.namedItem('xreg_display_name').value = String(displayName || '').trim();
    xregPathReadout.value = label;
    xregPathReadout.title = label;
    xregUploadName.textContent = label;
}

function syncTargetValueTitle() {
    targetValueSelect.title = String(targetValueSelect.value || '').trim();
}

function renderSeriesMeta(meta) {
    const data = meta && typeof meta === 'object' ? meta : {};
    const frequency = String(data.detected_frequency_label || 'Unknown').trim() || 'Unknown';
    const start = String(data.start_date || '').trim() || 'Unknown';
    const end = String(data.end_date || '').trim() || 'Unknown';
    const usableStart = String(data.usable_start_date || '').trim();
    const usableEnd = String(data.usable_end_date || '').trim();
    const dateColumn = String(data.date_column || '').trim() || 'Unknown';
    const valueColumn = String(data.value_column || '').trim();
    const seasonality = String(data.seasonality_label || '').trim();
    const seasonalityStrength = String(data.seasonality_strength || '').trim() || 'none';
    const seasonalityScore = String(data.seasonality_score ?? '').trim();
    const seasonalityLag = String(data.seasonality_lag ?? '').trim();
    const seasonalityClass = `seasonality-${seasonalityStrength.replace(/\s+/g, '-').toLowerCase()}`;
    const seasonalityDetail = [];
    if (seasonalityScore)
        seasonalityDetail.push(`score ${seasonalityScore}`);
    if (seasonalityLag && seasonalityLag !== '0')
        seasonalityDetail.push(`lag ${seasonalityLag}`);
    const seasonalityText = seasonalityDetail.length ? `${seasonality} (${seasonalityDetail.join(', ')})` : seasonality;
    return `` +
        `<div><strong>Detected frequency:</strong> ${escapeHtml(frequency)}</div>` +
        `<div><strong>Date column:</strong> ${escapeHtml(dateColumn)}</div>` +
        (valueColumn ? `<div><strong>Series:</strong> ${escapeHtml(valueColumn)}</div>` : '') +
        `<div><strong>File date range:</strong> ${escapeHtml(start)} to ${escapeHtml(end)}</div>` +
        `<div><strong>Usable series range:</strong> ${escapeHtml(usableStart || start)} to ${
               escapeHtml(usableEnd || end)}</div>` +
        (seasonality ? `<div class="seasonality-line"><strong>Seasonality:</strong> <span class="seasonality-badge ${
                           escapeHtml(seasonalityClass)}">${escapeHtml(seasonalityText)}</span></div>` :
                       '');
}

function applyTargetMeta(meta) {
    const data = meta && typeof meta === 'object' ? meta : {};
    const valueColumn = String(data.value_column || '').trim();
    if (valueColumn) {
        targetMetaCache[valueColumn] = data;
    }
    targetMetaBox.innerHTML = renderSeriesMeta(data);
    targetMetaBox.dataset.endDate = String(data.usable_end_date_iso || data.end_date_iso || '').trim();
    targetMetaBox.dataset.seasonalityLabel = String(data.seasonality_label || '').trim();
    targetMetaBox.dataset.seasonalityStrength = String(data.seasonality_strength || '').trim().toLowerCase();
    targetMetaBox.dataset.seasonalityLag = String(data.seasonality_lag || '').trim();
    renderDateColumnPicker(
        targetDateSelect, data.date_candidates || [], targetDateSelect.value, data.date_column || '');
    renderOutlierPicker(data.outliers || [], outlierDatesInput.value);
    const token = String(data.detected_frequency || '').trim();
    if (token) {
        form.elements.namedItem('frequency').value = token;
        detectedFrequencyReadout.value = String(data.detected_frequency_label || token);
        detectedFrequencyReadout.title = String(data.detected_frequency_label || token);
        renderSeasonPeriodOptions(token, String(form.elements.namedItem('season_period').value || '').trim());
        if (!initialHydrating) {
            applySuggestedModelSettings(false);
        }
    }
    updateSeasonalSetupWarning();
    buildForecastEndOptions();
}

function applyXregMeta(meta) {
    const data = meta && typeof meta === 'object' ? meta : {};
    xregMetaBox.innerHTML = renderSeriesMeta(data);
    xregMetaBox.dataset.endDate = String(data.usable_end_date_iso || data.end_date_iso || '').trim();
    renderDateColumnPicker(xregDateSelect, data.date_candidates || [], xregDateSelect.value, data.date_column || '');
    buildForecastEndOptions();
}

function parseIsoDate(text) {
    const raw = String(text || '').trim();
    if (!raw)
        return null;
    const value = new Date(`${raw}T00:00:00`);
    return Number.isNaN(value.getTime()) ? null : value;
}

function formatIsoDate(value) {
    if (!(value instanceof Date) || Number.isNaN(value.getTime()))
        return '';
    const year = value.getFullYear();
    const month = String(value.getMonth() + 1).padStart(2, '0');
    const day = String(value.getDate()).padStart(2, '0');
    return `${year}-${month}-${day}`;
}

function periodEndDate(value, frequency, yearType) {
    const out = new Date(value.getTime());
    if (frequency === 'daily') {
        return out;
    }
    if (frequency === 'monthly') {
        return new Date(out.getFullYear(), out.getMonth() + 1, 0);
    }
    if (frequency === 'quarterly') {
        const month = out.getMonth() + 1;
        const quarterEnds = yearType === 'fiscal' ? [6, 9, 12, 3] : [3, 6, 9, 12];
        for (const endMonth of quarterEnds) {
            let year = out.getFullYear();
            if (yearType === 'fiscal' && endMonth === 3 && month >= 4) {
                year += 1;
            }
            const candidate = new Date(year, endMonth, 0);
            if (candidate >= out)
                return candidate;
        }
        return new Date(out.getFullYear(), 12, 0);
    }
    if (frequency === 'yearly') {
        if (yearType === 'fiscal') {
            const year = out.getMonth() + 1 <= 3 ? out.getFullYear() : out.getFullYear() + 1;
            return new Date(year, 3, 0);
        }
        return new Date(out.getFullYear(), 12, 0);
    }
    return out;
}

function addPeriods(value, frequency, count, yearType) {
    const out = new Date(value.getTime());
    if (frequency === 'daily') {
        out.setDate(out.getDate() + count);
        return out;
    }
    if (frequency === 'monthly') {
        return new Date(out.getFullYear(), out.getMonth() + count + 1, 0);
    }
    if (frequency === 'quarterly') {
        return periodEndDate(new Date(out.getFullYear(), out.getMonth() + (count * 3), 1), frequency, yearType);
    }
    if (frequency === 'yearly') {
        if (yearType === 'fiscal') {
            const startYear = out.getMonth() + 1 <= 3 ? out.getFullYear() - 1 : out.getFullYear();
            return new Date(startYear + count + 1, 3, 0);
        }
        return new Date(out.getFullYear() + count, 12, 0);
    }
    return out;
}

function formatUkDate(value) {
    if (!(value instanceof Date) || Number.isNaN(value.getTime()))
        return '';
    const day = String(value.getDate()).padStart(2, '0');
    const month = String(value.getMonth() + 1).padStart(2, '0');
    const year = value.getFullYear();
    return `${day}/${month}/${year}`;
}

function buildForecastEndOptions() {
    if (!forecastEndOptionsHydrated && forecastEndInput && forecastEndInput.options &&
        forecastEndInput.options.length > 1) {
        forecastEndOptionsHydrated = true;
        return;
    }
    forecastEndOptionsHydrated = true;
    const targetEnd = parseIsoDate(targetMetaBox.dataset.endDate || '');
    const xregEnd = parseIsoDate(xregMetaBox.dataset.endDate || '');
    const frequency = String(form.elements.namedItem('frequency').value || '').trim();
    const yearType = String(form.elements.namedItem('year_type').value || '').trim();
    if (!targetEnd || !frequency) {
        forecastEndInput.innerHTML = '<option value="">Choose a forecast end date</option>';
        return;
    }
    let limit = addPeriods(targetEnd, frequency, 24, yearType);
    if (xregEnd && xregEnd > targetEnd)
        limit = xregEnd;
    const current = String(forecastEndInput.value || '').trim();
    const options = [];
    let cursor = periodEndDate(addPeriods(targetEnd, frequency, 1, yearType), frequency, yearType);
    while (cursor <= limit && options.length < 1200) {
        const iso = formatIsoDate(cursor);
        options.push({value: iso, label: formatUkDate(cursor)});
        cursor = periodEndDate(addPeriods(cursor, frequency, 1, yearType), frequency, yearType);
    }
    const selected = options.some((item) => item.value === current) ?
        current :
        (options.length ? options[options.length - 1].value : '');
    if (!options.length) {
        forecastEndInput.innerHTML = '<option value="">Choose a forecast end date</option>';
        return;
    }
    const grouped = new Map();
    for (const item of options) {
        const year = item.value.slice(0, 4) || 'Other';
        if (!grouped.has(year))
            grouped.set(year, []);
        grouped.get(year).push(item);
    }
    const chunks = [];
    for (const year of Array.from(grouped.keys()).sort()) {
        chunks.push(`<optgroup label="${escapeHtml(year)}">`);
        for (const item of grouped.get(year)) {
            chunks.push(`<option value="${escapeHtml(item.value)}"${item.value === selected ? ' selected' : ''}>${
                escapeHtml(item.label)}</option>`);
        }
        chunks.push('</optgroup>');
    }
    forecastEndInput.innerHTML = chunks.join('');
}

function splitSelectedColumns(text) {
    return String(text || '').split(',').map((value) => value.trim()).filter(Boolean);
}

function splitSelectedModels(text) {
    return String(text || '').split(',').map((value) => value.trim().toLowerCase()).filter(Boolean);
}

function currentSelectedModels() {
    const checked = modelList ? Array.from(modelList.querySelectorAll('input[name="model-choice"]:checked'))
                                    .map((input) => String(input.value || '').trim().toLowerCase())
                                    .filter(Boolean) :
                                [];
    if (checked.length)
        return checked;
    const hidden = splitSelectedModels(modelsField && modelsField.value);
    if (hidden.length)
        return hidden;
    return [];
}

function isSeasonalModel(model) {
    const token = String(model || '').trim().toLowerCase();
    return token === 'sarima' || token === 'sarimax' || token === 'auto-arima';
}

function isTimeSeriesModel(model) {
    const token = String(model || '').trim().toLowerCase();
    return token === 'arima' || token === 'arimax' || token === 'sarima' || token === 'sarimax' ||
        token === 'auto-arima';
}

function preferredSuggestionModel() {
    const selected = currentSelectedModels();
    if (!selected.length)
        return 'sarimax';
    const strength = String(targetMetaBox && targetMetaBox.dataset.seasonalityStrength || '').trim().toLowerCase();
    if (strength === 'moderate' || strength === 'strong' || strength === 'very strong') {
        const seasonal = selected.find((model) => isSeasonalModel(model));
        if (seasonal)
            return seasonal;
    }
    const timeSeries = selected.find((model) => isTimeSeriesModel(model));
    if (timeSeries)
        return timeSeries;
    return selected[0];
}

function formatModelSummary(values) {
    if (!values.length)
        return '<strong>none yet</strong>';
    return escapeHtml(values.map((value) => modelLabels[value] || value).join(', '));
}

function syncModelsFromChecks() {
    const selected =
        Array.from(modelList.querySelectorAll('input[name="model-choice"]:checked')).map((input) => input.value);
    if (!selected.length) {
        const first = modelList.querySelector('input[name="model-choice"]');
        if (first) {
            first.checked = true;
            selected.push(first.value);
        }
    }
    modelsField.value = selected.join(', ');
    modelSummary.innerHTML = `Selected: ${formatModelSummary(selected)}`;
    modelSummary.title = selected.join(', ');
}

function formatXregSummary(values) {
    if (!values.length)
        return '<strong>none yet</strong>';
    if (values.length === 1)
        return `<strong>1 column</strong>: ${escapeHtml(values[0])}`;
    if (values.length <= 3)
        return `<strong>${values.length} columns</strong>: ${escapeHtml(values.join(', '))}`;
    return `<strong>${values.length} columns</strong>: ${escapeHtml(values.slice(0, 3).join(', '))} + ${
        values.length - 3} more`;
}

function splitSelectedDates(text) {
    return String(text || '').split(',').map((value) => value.trim()).filter(Boolean);
}

function renderOutlierPicker(outliers, selectedText) {
    currentOutliers = Array.isArray(outliers) ? outliers : [];
    const selected = new Set(splitSelectedDates(selectedText));
    if (!currentOutliers.length) {
        outlierPanel.classList.add('no-outliers');
        outlierList.innerHTML =
            '<div class="multi-select-placeholder">No target points are currently flagged here.</div>';
        outlierDatesInput.value = '';
        outlierStatus.textContent = 'No outliers detected.';
        return;
    }
    outlierPanel.classList.remove('no-outliers');
    if (!selected.size) {
        for (const item of currentOutliers) {
            const iso = String(item.date_iso || '').trim();
            if (iso)
                selected.add(iso);
        }
    }
    outlierList.innerHTML = currentOutliers
                                .map((item) => {
                                    const iso = String(item.date_iso || '').trim();
                                    const checked = selected.has(iso) ? ' checked' : '';
                                    const score = Math.abs(Number(item.score || 0));
                                    const likelihood = score >= 20 ?
                                        'extremely likely' :
                                        (score >= 10 ? 'very likely' : (score >= 6 ? 'likely' : 'possibly'));
                                    const label = `${item.date || ''}: ${item.value || ''} (${likelihood})`;
                                    return `<label class="multi-select-item" title="${
                                        escapeHtml(label)}"><input type="checkbox" name="outlier-choice" value="${
                                        escapeHtml(iso)}"${checked}><span>${escapeHtml(label)}</span></label>`;
                                })
                                .join('');
    syncOutlierDatesFromChecks();
}

function syncOutlierDatesFromChecks() {
    const selected =
        Array.from(outlierList.querySelectorAll('input[name="outlier-choice"]:checked')).map((input) => input.value);
    outlierDatesInput.value = selected.join(', ');
    outlierStatus.textContent = selected.length ?
        `${selected.length} outlier${selected.length === 1 ? '' : 's'} selected for possible handling.` :
        (currentOutliers.length ? 'No outliers selected for handling.' : 'No outliers detected.');
}

function syncXregColumnsFromChecks() {
    const selected = Array.from(xregColumnsList.querySelectorAll('input[name="xreg-column-choice"]:checked'))
                         .map((input) => input.value);
    xregColumnsInput.value = selected.join(', ');
    xregColumnsSummary.innerHTML = `Selected: ${formatXregSummary(selected)}`;
    xregColumnsSummary.title = selected.join(', ');
    xregColumnsStatus.textContent = selected.length ?
        `${selected.length} exogenous column${selected.length === 1 ? '' : 's'} selected.` :
        'Choose one or more driver columns from the list.';
    if (xregColumnsDebug) {
        xregColumnsDebug.textContent = `Will send: ${selected.length ? selected.join(', ') : '(none)'}`;
    }
}

function renderXregColumnsPicker(columns, selectedText) {
    const selected = new Set(splitSelectedColumns(selectedText));
    const options = (Array.isArray(columns) ? columns : [])
                        .map((column) => {
                            if (column && typeof column === 'object') {
                                return {
                                    name: String(column.name || '').trim(),
                                    usable: Boolean(column.usable),
                                    reason: String(column.reason || '').trim(),
                                };
                            }
                            return {
                                name: String(column || '').trim(),
                                usable: true,
                                reason: '',
                            };
                        })
                        .filter((column) => column.name);
    if (!options.length) {
        xregColumnsList.innerHTML =
            '<div class="multi-select-placeholder">Upload or choose an exogenous CSV to load a scrollable list of driver columns here.</div>';
        xregColumnsInput.value = '';
        xregColumnsSummary.innerHTML = 'Selected: <strong>none yet</strong>';
        xregColumnsSummary.title = 'No exogenous columns selected';
        xregColumnsStatus.textContent = (Array.isArray(columns) ? columns.length : 0) ?
            'No selectable driver columns were found in this CSV.' :
            'Upload or choose an exogenous CSV to load its available columns.';
        return;
    }

    xregColumnsList.innerHTML =
        options
            .map((column) => {
                const checked = column.usable && selected.has(column.name) ? ' checked' : '';
                const disabled = column.usable ? '' : ' disabled';
                const disabledClass = column.usable ? '' : ' is-disabled';
                const title = column.reason || column.name;
                return `<label class="multi-select-item${disabledClass}" title="${
                    escapeHtml(title)}"><input type="checkbox" name="xreg-column-choice" value="${
                    escapeHtml(column.name)}"${checked}${disabled}><span>${escapeHtml(column.name)}</span></label>`;
            })
            .join('');
    syncXregColumnsFromChecks();
}

function renderTargetColumnPicker(columns, selected) {
    const items = Array.isArray(columns) ? columns : [];
    const chosen = String(selected || '').trim();

    if (!items.length) {
        targetValueSelect.innerHTML = '<option value="">Choose a column</option>';
        targetPickerStatus.textContent = 'Upload a CSV to load the available variables.';
        return;
    }

    targetValueSelect.innerHTML =
        items
            .map((column) => {
                const isSelected = column === chosen ? ' selected' : '';
                return `<option value="${escapeHtml(column)}"${isSelected}>${escapeHtml(column)}</option>`;
            })
            .join('');
    if (chosen && items.includes(chosen)) {
        targetValueSelect.value = chosen;
    } else {
        targetValueSelect.value = items[0];
    }
    syncTargetValueTitle();
    targetPickerStatus.textContent = `Chosen target: ${targetValueSelect.value}`;
}

function renderDateColumnPicker(selectNode, columns, selected, fallback = '') {
    const items = Array.isArray(columns) ? columns.map((column) => String(column || '').trim()).filter(Boolean) : [];
    let chosen = String(selected || '').trim() || String(fallback || '').trim();
    if (!items.length) {
        selectNode.innerHTML = chosen ?
            `<option value="${escapeHtml(chosen)}" selected>${escapeHtml(chosen)}</option>` :
            '<option value="">No date columns found</option>';
        return;
    }
    if (items.length === 1)
        chosen = items[0];
    if (!chosen || !items.includes(chosen))
        chosen = items[0];
    selectNode.innerHTML =
        items
            .map((column) => {
                const selectedAttr = column === chosen ? ' selected' : '';
                return `<option value="${escapeHtml(column)}"${selectedAttr}>${escapeHtml(column)}</option>`;
            })
            .join('');
    selectNode.value = chosen;
}

async function uploadTargetCsv() {
    const file = targetUpload.files && targetUpload.files[0];
    if (!file) {
        setStatus('Choose a CSV file first.', 'error');
        return;
    }

    setStatus('Uploading target CSV and reading its headers...', '');
    targetPickerStatus.textContent = 'Reading headers...';

    try {
        const body = new FormData();
        body.append('file', file);
        const response = await fetch(`${basePath}/upload-target`, {
            method: 'POST',
            body,
        });
        const data = await response.json();
        if (!response.ok || !data.ok) {
            throw new Error(data.error || 'Could not upload the CSV');
        }

        form.elements.namedItem('target_path').value = data.path || '';
        syncTargetUploadName(data.path || '', data.original_name || '');
        Object.keys(targetMetaCache).forEach((key) => {
            delete targetMetaCache[key];
        });
        applyTargetMeta(data);

        renderTargetColumnPicker(data.value_columns || [], targetValueSelect.value);
        scheduleStateSave();
        await saveState();
        setStatus('Target CSV uploaded. Choose the series to forecast from the dropdown.', 'ok');
    } catch (error) {
        targetValueSelect.innerHTML = '<option value="">Choose a column</option>';
        targetPickerStatus.textContent = 'Upload failed.';
        setStatus(error.message || String(error), 'error');
    } finally {
        targetUpload.value = '';
    }
}

async function uploadXregCsv() {
    const file = xregUpload.files && xregUpload.files[0];
    if (!file) {
        setStatus('Choose an exogenous CSV file first.', 'error');
        return;
    }

    setStatus('Uploading exogenous CSV...', '');
    try {
        const body = new FormData();
        body.append('file', file);
        const response = await fetch(`${basePath}/upload-xreg`, {
            method: 'POST',
            body,
        });
        const data = await response.json();
        if (!response.ok || !data.ok) {
            throw new Error(data.error || 'Could not upload the exogenous CSV');
        }

        form.elements.namedItem('xreg_path').value = data.path || '';
        syncXregUploadName(data.path || '', data.original_name || '');
        applyXregMeta(data);
        renderXregColumnsPicker(data.value_column_details || data.value_columns || [], xregColumnsInput.value);
        scheduleStateSave();
        await saveState();
        setStatus('Exogenous CSV uploaded.', 'ok');
    } catch (error) {
        setStatus(error.message || String(error), 'error');
    } finally {
        xregUpload.value = '';
    }
}

async function loadExistingXregColumns() {
    const currentPath = String(form.elements.namedItem('xreg_path').value || '').trim();
    if (!currentPath) {
        renderXregColumnsPicker([], xregColumnsInput.value);
        return;
    }

    try {
        const dateColumn = String(form.elements.namedItem('xreg_date_column').value || '').trim();
        const selectedColumns = String(xregColumnsInput.value || '').trim();
        const response = await fetch(
            `${basePath}/target-columns?path=${encodeURIComponent(currentPath)}&date_column=${
                encodeURIComponent(dateColumn)}&selected_columns=${encodeURIComponent(selectedColumns)}`,
            {
                cache: 'no-store',
            });
        const data = await response.json();
        if (!response.ok || !data.ok) {
            applyXregMeta({});
            renderXregColumnsPicker([], xregColumnsInput.value);
            return;
        }
        applyXregMeta(data);
        renderXregColumnsPicker(data.value_column_details || data.value_columns || [], xregColumnsInput.value);
    } catch (_) {
        applyXregMeta({});
        renderXregColumnsPicker([], xregColumnsInput.value);
    }
}

async function loadExistingTargetColumns(requestedValue = '') {
    const currentPath = String(form.elements.namedItem('target_path').value || '').trim();
    if (!currentPath) {
        return;
    }

    try {
        const dateColumn = String(form.elements.namedItem('target_date_column').value || '').trim();
        const valueColumn = String(requestedValue || form.elements.namedItem('target_value_column').value || '').trim();
        if (valueColumn && targetMetaCache[valueColumn]) {
            applyTargetMeta(targetMetaCache[valueColumn]);
            return;
        }
        if (valueColumn) {
            targetMetaBox.innerHTML = '' +
                `<div><strong>Series:</strong> ${escapeHtml(valueColumn)}</div>` +
                '<div class="summary-note">Refreshing seasonality and outlier checks for the selected target series...</div>';
        }
        const requestId = ++targetMetaRequestId;
        const response = await fetch(
            `${basePath}/target-columns?path=${encodeURIComponent(currentPath)}&date_column=${
                encodeURIComponent(dateColumn)}&value_column=${encodeURIComponent(valueColumn)}`,
            {
                cache: 'no-store',
            });
        const data = await response.json();
        if (requestId !== targetMetaRequestId) {
            return;
        }
        if (valueColumn && String(data.value_column || '').trim() &&
            String(data.value_column || '').trim() !== valueColumn) {
            return;
        }
        if (!response.ok || !data.ok) {
            applyTargetMeta({});
            return;
        }
        applyTargetMeta(data);
    } catch (_) {
        applyTargetMeta({});
    }
}

async function saveState() {
    const payload = collectState();
    await fetch(`${basePath}/state`, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload),
    });
}

function scheduleStateSave() {
    if (saveTimer)
        window.clearTimeout(saveTimer);
    saveTimer = window.setTimeout(() => {
        saveState().catch(() => {});
    }, 250);
}

function setStatus(text, kind = '') {
    statusNode.textContent = text;
    statusNode.className = kind ? `status ${kind}` : 'status';
}

function showResults() {
    resultGrid.classList.remove('hidden');
    chartPane.classList.add('hidden');
    helpPane.classList.add('hidden');
    if (layout)
        layout.classList.remove('chart-mode');
    if (chartToggle)
        chartToggle.textContent = 'Chart';
    helpToggle.textContent = 'Help';
    setStatus('Ready', '');
}

function showHelp() {
    resultGrid.classList.add('hidden');
    chartPane.classList.add('hidden');
    helpPane.classList.remove('hidden');
    if (layout)
        layout.classList.remove('chart-mode');
    if (chartToggle)
        chartToggle.textContent = 'Chart';
    helpToggle.textContent = 'Results';
    setStatus('Help', '');
}

function showChart() {
    resultGrid.classList.add('hidden');
    helpPane.classList.add('hidden');
    chartPane.classList.remove('hidden');
    if (layout)
        layout.classList.add('chart-mode');
    if (chartToggle)
        chartToggle.textContent = 'Results';
    helpToggle.textContent = 'Help';
    setStatus('Chart', '');
}

function csvToRows(text) {
    const rows = text.trim().split(/\r?\n/).map(line => line.split(','));
    if (rows.length < 2)
        return [];
    return rows.slice(1);
}

function escapeHtml(text) {
    return String(text)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

function summaryRatingClass(line) {
    const lower = line.toLowerCase();
    if (lower.includes('(exceptional'))
        return 'rating-exceptional';
    if (lower.includes('(excellent'))
        return 'rating-excellent';
    if (lower.includes('(very good'))
        return 'rating-very-good';
    if (lower.includes('(good'))
        return 'rating-good';
    if (lower.includes('(mediocre'))
        return 'rating-mediocre';
    if (lower.includes('(poor'))
        return 'rating-poor';
    if (lower.includes('(comparison only'))
        return 'rating-comparison';
    return '';
}

function coefficientRatingClassFromRatio(ratio) {
    if (!isFinite(ratio))
        return 'rating-comparison';
    if (ratio >= 4.0)
        return 'rating-exceptional';
    if (ratio >= 3.0)
        return 'rating-excellent';
    if (ratio >= 2.0)
        return 'rating-very-good';
    if (ratio >= 1.5)
        return 'rating-good';
    if (ratio >= 1.0)
        return 'rating-mediocre';
    return 'rating-poor';
}

function coefficientSupportText(ratio) {
    if (!isFinite(ratio))
        return 'comparison only';
    if (ratio >= 4.0)
        return 'extremely likely helping';
    if (ratio >= 3.0)
        return 'very likely helping';
    if (ratio >= 2.0)
        return 'likely helping';
    if (ratio >= 1.5)
        return 'possibly helping';
    if (ratio >= 1.0)
        return 'weak evidence';
    return 'little evidence';
}

function selectedXregLabels() {
    return String(xregColumnsInput && xregColumnsInput.value || '')
        .split(',')
        .map((part) => part.trim())
        .filter(Boolean);
}

function coefficientDisplayName(index) {
    const labels = selectedXregLabels();
    if (index === 0)
        return 'Intercept';
    if (index - 1 < labels.length)
        return `Driver: ${labels[index - 1]}`;
    return `Coefficient ${index}`;
}

function parseCoefficientLine(line) {
    const betaMatch = line.match(/^beta\[(\d+)\] = ([^\s]+)\s+stderr = ([^\s]+)$/);
    const phiMatch = line.match(/^phi\[(\d+)\] = ([^\s]+)$/);
    if (betaMatch) {
        const index = Number(betaMatch[1]);
        const estimate = Number(betaMatch[2]);
        const stderr = Number(betaMatch[3]);
        const ratio = stderr ? Math.abs(estimate) / Math.abs(stderr) : Number.POSITIVE_INFINITY;
        return {
            type: 'beta',
            index,
            title: coefficientDisplayName(index),
            estimate,
            stderr,
            ratio,
        };
    }
    if (phiMatch) {
        return {
            type: 'phi',
            index: Number(phiMatch[1]),
            title: `AR term ${phiMatch[1]}`,
            estimate: Number(phiMatch[2]),
            stderr: NaN,
            ratio: NaN,
        };
    }
    return null;
}

function renderCoefficientCard(line) {
    const parsed = parseCoefficientLine(line.trim());
    if (!parsed) {
        return `<div class="summary-line"><span class="summary-v">${escapeHtml(line)}</span></div>`;
    }
    const ratingClass = coefficientRatingClassFromRatio(parsed.ratio);
    const supportText = parsed.type === 'beta' ? coefficientSupportText(parsed.ratio) : 'model term';
    const estimateText = Number.isFinite(parsed.estimate) ? parsed.estimate.toFixed(4) : String(parsed.estimate);
    const stderrText = Number.isFinite(parsed.stderr) ? parsed.stderr.toFixed(4) : '';
    const meta = parsed.type === 'beta' ? `Estimate ${escapeHtml(estimateText)} · StdErr ${escapeHtml(stderrText)}` :
                                          `Estimate ${escapeHtml(estimateText)}`;
    const note = parsed.type === 'beta' ? supportText : 'Autoregressive carry-over term';
    return `<div class="summary-coeff-card ${ratingClass}">
        <div class="summary-coeff-title">${escapeHtml(parsed.title)}</div>
        <div class="summary-coeff-value">${meta}</div>
        <div class="summary-coeff-meta">${escapeHtml(note)}</div>
      </div>`;
}

function renderSummaryText(text) {
    const source = normalizeSummaryText((text || '').trim());
    if (!source) {
        summaryBox.innerHTML = '<div class="summary-line note-line">No summary returned.</div>';
        return;
    }
    const lines = source.split(/\r?\n/).filter(line => line.length > 0);
    let inCoeffSection = false;
    let coeffCards = [];

    const html = lines
                     .map((line, index) => {
                         const trimmed = line.trim();
                         let classes = ['summary-line'];
                         let content = escapeHtml(line);
                         const parsedCoeff = parseCoefficientLine(trimmed);

                         if (/^Coefficients$/i.test(trimmed) || /^AR parameters$/i.test(trimmed)) {
                             inCoeffSection = true;
                             coeffCards = [];
                         } else if (parsedCoeff && inCoeffSection) {
                             coeffCards.push(renderCoefficientCard(trimmed));
                             return '';
                         } else if (inCoeffSection && coeffCards.length) {
                             const grid = `<div class="summary-coeff-grid">${coeffCards.join('')}</div>`;
                             coeffCards = [];
                             inCoeffSection = false;
                             return `${grid}${renderSummaryTextLine(line, index)}`;
                         }

                         if (index === 0 && /summary$/i.test(trimmed)) {
                             classes.push('title-line');
                         } else if (
                             /^How to read this:/i.test(trimmed) || /^Coefficients$/i.test(trimmed) ||
                             /^AR parameters$/i.test(trimmed) || /^Model:/i.test(trimmed)) {
                             classes.push('section-line');
                         } else if (/^- /.test(trimmed)) {
                             classes.push('note-line');
                         } else {
                             const ratingClass = summaryRatingClass(trimmed);
                             if (ratingClass)
                                 classes.push(ratingClass);
                         }

                         if (line.includes(': ') && !/^Model:/i.test(trimmed) && !/^How to read this:/i.test(trimmed)) {
                             const splitAt = line.indexOf(': ');
                             const key = escapeHtml(line.slice(0, splitAt + 1));
                             const value = escapeHtml(line.slice(splitAt + 2));
                             content = `<span class="summary-k">${key}</span> <span class="summary-v">${value}</span>`;
                         } else if (/^beta\[\d+\] = /.test(trimmed) || /^phi\[\d+\] = /.test(trimmed)) {
                             content = `<span class="summary-v">${escapeHtml(line)}</span>`;
                         }

                         return `<div class="${classes.join(' ')}">${content}</div>`;
                     })
                     .join('');

    const trailingCoeffGrid = coeffCards.length ? `<div class="summary-coeff-grid">${coeffCards.join('')}</div>` : '';

    summaryBox.innerHTML =
        (html + trailingCoeffGrid) || '<div class="summary-line note-line">No summary returned.</div>';
}

function normalizeSummaryText(text) {
    let source = String(text || '').trim();
    const markers = [
        'Overall assessment:', 'Overall fit score (R2):', 'Adjusted fit score (Adj R2):',
        'Typical forecast error (RMSE):', 'Model comparison score (AIC):', 'Model comparison score (BIC):',
        'Unexplained variation left in the model (Sigma2):', 'How to read this:', 'Coefficients', 'AR parameters',
        'Model:'
    ];

    if (!source)
        return source;
    if (!/[\r\n]/.test(source)) {
        markers.forEach((marker, index) => {
            const replacement = index === 0 && source.startsWith(marker) ? marker : `\n${marker}`;
            source = source.replaceAll(` ${marker}`, replacement);
        });
        source = source.replace(/ (beta\[\d+\] = )/g, '\n$1');
        source = source.replace(/ (phi\[\d+\] = )/g, '\n$1');
        source = source.replace(/ How to read this: - /g, '\nHow to read this:\n- ');
        source = source.replace(/ - (?=[A-Z])/g, '\n- ');
    }

    source = source.replace(/\r\n/g, '\n');
    source = source.replace(/\n{3,}/g, '\n\n');
    return source;
}

function renderSummaryTextLine(line, index) {
    const trimmed = line.trim();
    let classes = ['summary-line'];
    let content = escapeHtml(line);

    if (index === 0 && /summary$/i.test(trimmed)) {
        classes.push('title-line');
    } else if (
        /^How to read this:/i.test(trimmed) || /^Coefficients$/i.test(trimmed) || /^AR parameters$/i.test(trimmed) ||
        /^Model:/i.test(trimmed)) {
        classes.push('section-line');
    } else if (/^- /.test(trimmed)) {
        classes.push('note-line');
    } else {
        const ratingClass = summaryRatingClass(trimmed);
        if (ratingClass)
            classes.push(ratingClass);
    }

    if (line.includes(': ') && !/^Model:/i.test(trimmed) && !/^How to read this:/i.test(trimmed)) {
        const splitAt = line.indexOf(': ');
        const key = escapeHtml(line.slice(0, splitAt + 1));
        const value = escapeHtml(line.slice(splitAt + 2));
        content = `<span class="summary-k">${key}</span> <span class="summary-v">${value}</span>`;
    }
    return `<div class="${classes.join(' ')}">${content}</div>`;
}

window.__to_be_announcedRenderSummary = renderSummaryText;

function renderForecastTable(csvText) {
    const source = String(csvText || '').trim();
    if (!source) {
        forecastHead.innerHTML =
            '<tr><th>Date</th><th>Actual</th><th>Mean</th><th>StdErr</th><th>Lower</th><th>Upper</th></tr>';
        forecastBody.innerHTML = '<tr><td colspan="6">No forecast rows returned.</td></tr>';
        return;
    }

    const lines = source.split(/\r?\n/).filter(Boolean);
    const header = (lines[0] || '').split(',');
    const rows = lines.slice(1).map(line => line.split(','));
    const colCount = header.length || 1;

    forecastHead.innerHTML = `<tr>${header.map(cell => `<th>${escapeHtml(cell || '')}</th>`).join('')}</tr>`;
    if (!rows.length) {
        forecastBody.innerHTML = `<tr><td colspan="${colCount}">No forecast rows returned.</td></tr>`;
        return;
    }
    forecastBody.innerHTML =
        rows.map(row => (`<tr>${header.map((_, index) => `<td>${escapeHtml(row[index] || '')}</td>`).join('')}</tr>`))
            .join('');
}

function forecastTableMarkup(csvText) {
    const source = String(csvText || '').trim();
    if (!source) {
        return (
            '<div class="table-wrap comparison-table-wrap">' +
            '<div class="table-caption">No forecast rows returned.</div>' +
            '<div class="table-scroll"><table><thead><tr><th>Date</th><th>Actual</th><th>Mean</th><th>StdErr</th><th>Lower</th><th>Upper</th></tr></thead>' +
            '<tbody><tr><td colspan="6">No forecast rows returned.</td></tr></tbody></table></div></div>');
    }
    const lines = source.split(/\r?\n/).filter(Boolean);
    const header = (lines[0] || '').split(',');
    const rows = lines.slice(1).map(line => line.split(','));
    const head = `<tr>${header.map(cell => `<th>${escapeHtml(cell || '')}</th>`).join('')}</tr>`;
    const body = rows.length ?
        rows.map(row => `<tr>${header.map((_, index) => `<td>${escapeHtml(row[index] || '')}</td>`).join('')}</tr>`)
            .join('') :
        `<tr><td colspan="${header.length || 1}">No forecast rows returned.</td></tr>`;
    return (
        '<div class="table-wrap comparison-table-wrap">' +
        '<div class="table-caption">Actual vs forecast comparison for this model.</div>' +
        `<div class="table-scroll"><table><thead>${head}</thead><tbody>${body}</tbody></table></div></div>`);
}

function parseForecastCsvRowsForComparison(csvText) {
    const source = String(csvText || '').trim();
    if (!source)
        return [];
    const lines = source.split(/\r?\n/).filter(Boolean);
    if (!lines.length)
        return [];
    const header = (lines[0] || '').split(',').map((cell) => String(cell || '').trim().toLowerCase());
    return lines.slice(1).map((line) => {
        const cells = line.split(',');
        const row = {};
        header.forEach((key, index) => {
            row[key] = String(cells[index] || '').trim();
        });
        return row;
    });
}

const chartPalette = [
    '#2f6fed',
    '#e0529c',
    '#1f9d78',
    '#ef8a17',
    '#7757d6',
    '#c94c4c',
    '#0f7c95',
    '#7a6a18',
];
const chartLowerColor = '#cc6b19';
const chartUpperColor = '#b33951';

function parseChartNumber(value) {
    const text = String(value ?? '').trim();
    if (!text || /^nan$/i.test(text))
        return null;
    const parsed = Number(text.replace(/,/g, ''));
    return Number.isFinite(parsed) ? parsed : null;
}

function formatChartNumber(value) {
    const absolute = Math.abs(value);
    if (absolute >= 1000)
        return value.toLocaleString(undefined, {maximumFractionDigits: 0});
    if (absolute >= 100)
        return value.toLocaleString(undefined, {maximumFractionDigits: 1});
    if (absolute >= 10)
        return value.toLocaleString(undefined, {maximumFractionDigits: 2});
    return value.toLocaleString(undefined, {maximumFractionDigits: 3});
}

function chartDateMillis(label) {
    const text = String(label || '').trim();
    if (!text)
        return null;
    let match = text.match(/^(\d{1,2})\/(\d{1,2})\/(\d{4})$/);
    if (match) {
        const day = Number(match[1]);
        const month = Number(match[2]);
        const year = Number(match[3]);
        const value = Date.UTC(year, month - 1, day);
        return Number.isFinite(value) ? value : null;
    }
    match = text.match(/^(\d{4})-(\d{1,2})-(\d{1,2})$/);
    if (match) {
        const year = Number(match[1]);
        const month = Number(match[2]);
        const day = Number(match[3]);
        const value = Date.UTC(year, month - 1, day);
        return Number.isFinite(value) ? value : null;
    }
    const parsed = Date.parse(text);
    return Number.isFinite(parsed) ? parsed : null;
}

function formatChartAxisDate(value) {
    const date = new Date(value);
    if (Number.isNaN(date.getTime()))
        return '';
    const day = String(date.getUTCDate()).padStart(2, '0');
    const month = String(date.getUTCMonth() + 1).padStart(2, '0');
    const year = date.getUTCFullYear();
    return `${day}/${month}/${year}`;
}

function selectedForecastEndLabel() {
    if (!forecastEndInput)
        return '';
    const selected = forecastEndInput.options && forecastEndInput.selectedIndex >= 0 ?
        forecastEndInput.options[forecastEndInput.selectedIndex] :
        null;
    return String((selected && selected.textContent) || forecastEndInput.value || '').trim();
}

function closestChartTickIndexes(dateValues, desiredCount) {
    const usable = dateValues.map((value, index) => ({value, index})).filter((item) => Number.isFinite(item.value));
    if (!usable.length)
        return [];
    if (usable.length <= desiredCount) {
        return usable.map((item) => item.index);
    }
    const start = usable[0].value;
    const end = usable[usable.length - 1].value;
    if (start === end)
        return [usable[0].index];
    const ticks = [];
    for (let slot = 0; slot < desiredCount; slot += 1) {
        const target = start + ((end - start) * slot / (desiredCount - 1));
        let best = usable[0];
        let bestDistance = Math.abs(best.value - target);
        usable.forEach((item) => {
            const distance = Math.abs(item.value - target);
            if (distance < bestDistance) {
                best = item;
                bestDistance = distance;
            }
        });
        if (!ticks.includes(best.index))
            ticks.push(best.index);
    }
    return ticks.sort((a, b) => a - b);
}

function chartPath(values, xForIndex, yForValue) {
    let path = '';
    let active = false;
    values.forEach((value, index) => {
        if (!Number.isFinite(value)) {
            active = false;
            return;
        }
        const x = xForIndex(index).toFixed(2);
        const y = yForValue(value).toFixed(2);
        path += `${active ? 'L' : 'M'}${x},${y}`;
        active = true;
    });
    return path;
}

function lastFiniteIndex(values) {
    for (let index = values.length - 1; index >= 0; index -= 1) {
        if (Number.isFinite(values[index]))
            return index;
    }
    return -1;
}

function nearestChartPoint(points, x, y) {
    let best = null;
    let bestDistance = Infinity;
    (Array.isArray(points) ? points : []).forEach((point) => {
        const dx = Number(point.x) - x;
        const dy = Number(point.y) - y;
        const distance = (dx * dx) + (dy * dy * 0.35);
        if (distance < bestDistance) {
            best = point;
            bestDistance = distance;
        }
    });
    return best;
}

function attachChartTooltip(hoverSeries) {
    if (!forecastChartBody)
        return;
    const svg = forecastChartBody.querySelector('.forecast-chart-svg');
    const tooltip = forecastChartBody.querySelector('.chart-tooltip');
    const marker = svg ? svg.querySelector('.chart-hover-marker') : null;
    if (!svg || !tooltip || !marker)
        return;

    const hideTooltip = () => {
        tooltip.classList.add('hidden');
        marker.setAttribute('visibility', 'hidden');
    };

    svg.querySelectorAll('.chart-hover-path').forEach((path) => {
        path.addEventListener('mousemove', (event) => {
            const seriesIndex = Number(path.getAttribute('data-series-index'));
            const seriesInfo = hoverSeries[seriesIndex];
            if (!seriesInfo || !Array.isArray(seriesInfo.points) || !seriesInfo.points.length) {
                hideTooltip();
                return;
            }
            const matrix = svg.getScreenCTM();
            if (!matrix) {
                hideTooltip();
                return;
            }
            const svgPoint = svg.createSVGPoint();
            svgPoint.x = event.clientX;
            svgPoint.y = event.clientY;
            const cursor = svgPoint.matrixTransform(matrix.inverse());
            const point = nearestChartPoint(seriesInfo.points, cursor.x, cursor.y);
            if (!point) {
                hideTooltip();
                return;
            }
            marker.setAttribute('visibility', 'visible');
            marker.setAttribute('cx', String(point.x));
            marker.setAttribute('cy', String(point.y));
            marker.setAttribute('fill', seriesInfo.color);
            tooltip.innerHTML =
                (`<div class="chart-tooltip-title">${escapeHtml(seriesInfo.label)}</div>` +
                    `<div><strong>${escapeHtml(point.date || '')}</strong></div>` +
                    `<div class="chart-tooltip-meta">Value: ${escapeHtml(formatChartNumber(point.value))}</div>`);
            const bodyRect = forecastChartBody.getBoundingClientRect();
            const left = event.clientX - bodyRect.left;
            const top = event.clientY - bodyRect.top;
            tooltip.style.left = `${Math.min(Math.max(left, 8), Math.max(bodyRect.width - 180, 8))}px`;
            tooltip.style.top = `${Math.min(Math.max(top, 34), Math.max(bodyRect.height - 34, 34))}px`;
            tooltip.classList.remove('hidden');
        });
        path.addEventListener('mouseleave', hideTooltip);
    });
    svg.addEventListener('mouseleave', hideTooltip);
}

function chartToggleChecked(seriesId) {
    if (!forecastChartBody)
        return false;
    let found = null;
    forecastChartBody.querySelectorAll('input[name="chart-series-toggle"]').forEach((input) => {
        if (input.getAttribute('data-series-id') === seriesId)
            found = input;
    });
    return found ? found.checked : false;
}

function attachChartControls(context = null) {
    if (!forecastChartBody)
        return;
    const updateVisibility = () => {
        const lowerVisible = chartToggleChecked('confidence-lower');
        const upperVisible = chartToggleChecked('confidence-upper');
        forecastChartBody.querySelectorAll('[data-chart-series-id]').forEach((element) => {
            const confidenceKind = element.getAttribute('data-confidence-kind') || '';
            const parentId = element.getAttribute('data-chart-parent-id') || '';
            const seriesId = element.getAttribute('data-chart-series-id') || '';
            let visible;
            if (confidenceKind === 'lower') {
                visible = lowerVisible && chartToggleChecked(parentId);
            } else if (confidenceKind === 'upper') {
                visible = upperVisible && chartToggleChecked(parentId);
            } else {
                visible = chartToggleChecked(seriesId);
            }
            element.style.display = visible ? '' : 'none';
        });
    };
    forecastChartBody.querySelectorAll('input[name="chart-series-toggle"]').forEach((input) => {
        input.addEventListener('change', () => {
            const visible = input.checked;
            const item = input.closest('.chart-legend-item');
            if (item)
                item.classList.toggle('is-off', !visible);
            updateVisibility();
            const marker = forecastChartBody.querySelector('.chart-hover-marker');
            const tooltip = forecastChartBody.querySelector('.chart-tooltip');
            if (marker)
                marker.setAttribute('visibility', 'hidden');
            if (tooltip)
                tooltip.classList.add('hidden');
        });
    });
    updateVisibility();
}

function chartDataFromSingleCsv(csvText, modelLabel, forecastEndLabel = '') {
    const rows = parseForecastCsvRowsForComparison(csvText);
    return {
        labels: rows.map((row) => row.date || ''),
        forecastEndLabel: String(forecastEndLabel || '').trim() || selectedForecastEndLabel(),
        actual: rows.map((row) => parseChartNumber(row.actual)),
        series: [{
            label: modelLabel || 'Forecast',
            values: rows.map((row) => parseChartNumber(row.mean)),
            lower: rows.map((row) => parseChartNumber(row.lower)),
            upper: rows.map((row) => parseChartNumber(row.upper)),
        }],
    };
}

function chartDataFromMultiResults(results, forecastEndLabel = '') {
    const successful =
        (Array.isArray(results) ? results : []).filter((result) => result && result.ok && result.forecast_csv);
    const models = uniqueModelLabels(successful);
    const order = [];
    const byDate = new Map();
    successful.forEach((result, index) => {
        const model = models[index];
        const rows = parseForecastCsvRowsForComparison(result.forecast_csv || '');
        rows.forEach((row) => {
            const date = String(row.date || '').trim();
            if (!date)
                return;
            if (!byDate.has(date)) {
                byDate.set(date, {actual: '', values: {}});
                order.push(date);
            }
            const entry = byDate.get(date);
            if (!entry.actual && row.actual)
                entry.actual = row.actual;
            entry.values[model] = {
                mean: row.mean || '',
                lower: row.lower || '',
                upper: row.upper || '',
            };
        });
    });
    return {
        labels: order,
        forecastEndLabel: String(forecastEndLabel || '').trim() ||
            String(successful[0] && (successful[0].forecast_end_label || successful[0].forecast_end_date) || '')
                .trim() ||
            selectedForecastEndLabel(),
        actual: order.map((date) => parseChartNumber(byDate.get(date).actual)),
        series:
            models.map((model) => ({
                           label: model,
                           values: order.map((date) => parseChartNumber((byDate.get(date).values[model] || {}).mean)),
                           lower: order.map((date) => parseChartNumber((byDate.get(date).values[model] || {}).lower)),
                           upper: order.map((date) => parseChartNumber((byDate.get(date).values[model] || {}).upper)),
                       })),
    };
}

function renderForecastChartEmpty(message) {
    if (!forecastChartBody)
        return;
    forecastChartBody.className = 'forecast-chart-body forecast-chart-empty';
    forecastChartBody.innerHTML = escapeHtml(message || 'Run a forecast to draw the chart.');
}

function renderForecastChart(chartData) {
    if (!forecastChartBody)
        return;
    const labels = Array.isArray(chartData && chartData.labels) ? chartData.labels : [];
    const actual = Array.isArray(chartData && chartData.actual) ? chartData.actual : [];
    const series = Array.isArray(chartData && chartData.series) ? chartData.series : [];
    const forecastEndLabel = String(chartData && chartData.forecastEndLabel || '').trim();
    const allValues = [].concat(actual)
                          .concat(series.flatMap((item) => Array.isArray(item.values) ? item.values : []))
                          .concat(series.flatMap((item) => Array.isArray(item.lower) ? item.lower : []))
                          .concat(series.flatMap((item) => Array.isArray(item.upper) ? item.upper : []))
                          .filter((value) => Number.isFinite(value));
    if (!labels.length || !allValues.length) {
        renderForecastChartEmpty('No plottable forecast values were returned.');
        return;
    }

    const width = 1040;
    const height = 430;
    const margin = {top: 24, right: 28, bottom: 52, left: 68};
    const plotWidth = width - margin.left - margin.right;
    const plotHeight = height - margin.top - margin.bottom;
    const dateValues = labels.map((label) => chartDateMillis(label));
    const forecastEndValue = chartDateMillis(forecastEndLabel);
    const useTimeAxis = dateValues.length === labels.length && dateValues.every((value) => Number.isFinite(value)) &&
        Math.max(...dateValues) > Math.min(...dateValues);
    const axisStartValue = useTimeAxis ? Math.min(...dateValues) : null;
    const axisEndValue = useTimeAxis ?
        Math.max(
            Math.max(...dateValues), Number.isFinite(forecastEndValue) ? forecastEndValue : Math.max(...dateValues)) :
        null;
    let yMin = Math.min(...allValues);
    let yMax = Math.max(...allValues);
    if (yMin === yMax) {
        const pad = Math.max(Math.abs(yMin) * 0.1, 1);
        yMin -= pad;
        yMax += pad;
    } else {
        const pad = (yMax - yMin) * 0.08;
        yMin -= pad;
        yMax += pad;
    }
    const xForIndex = (index) => {
        if (useTimeAxis) {
            return margin.left + ((dateValues[index] - axisStartValue) * plotWidth / (axisEndValue - axisStartValue));
        }
        return labels.length > 1 ? margin.left + (index * plotWidth / (labels.length - 1)) :
                                   margin.left + (plotWidth / 2);
    };
    const yForValue = (value) => margin.top + ((yMax - value) * plotHeight / (yMax - yMin));

    const yTicks = Array.from({length: 5}, (_, index) => yMax - ((yMax - yMin) * index / 4));
    const yGrid =
        yTicks
            .map((value) => {
                const y = yForValue(value);
                return (
                    `<line class="chart-grid-line" x1="${margin.left}" y1="${y.toFixed(2)}" x2="${
                        width - margin.right}" y2="${y.toFixed(2)}"></line>` +
                    `<text class="chart-axis-label" x="${margin.left - 10}" y="${
                        (y + 4).toFixed(2)}" text-anchor="end">${escapeHtml(formatChartNumber(value))}</text>`);
            })
            .join('');
    const targetTickCount = Math.min(6, labels.length);
    let xTickEntries;
    if (useTimeAxis) {
        const forecastEndX =
            margin.left + ((axisEndValue - axisStartValue) * plotWidth / (axisEndValue - axisStartValue));
        xTickEntries = closestChartTickIndexes(dateValues, targetTickCount)
                           .map((index) => ({x: xForIndex(index), label: labels[index] || ''}))
                           .filter((item) => Math.abs(item.x - forecastEndX) > 1);
        xTickEntries.push({
            x: forecastEndX,
            label: forecastEndLabel || formatChartAxisDate(axisEndValue),
            forecastEnd: true,
        });
        xTickEntries.sort((a, b) => a.x - b.x);
    } else {
        xTickEntries =
            Array
                .from(new Set(Array.from(
                    {length: targetTickCount},
                    (_, index) =>
                        (targetTickCount > 1 ? Math.round((labels.length - 1) * index / (targetTickCount - 1)) : 0))))
                .filter((index) => index >= 0 && index < labels.length)
                .map((index) => ({x: xForIndex(index), label: labels[index] || ''}));
    }
    const xTicks = xTickEntries
                       .map((item, tickIndex) => {
                           const anchor =
                               tickIndex === 0 ? 'start' : (tickIndex === xTickEntries.length - 1 ? 'end' : 'middle');
                           return (
                               `<line class="chart-grid-line" x1="${item.x.toFixed(2)}" y1="${margin.top}" x2="${
                                   item.x.toFixed(2)}" y2="${height - margin.bottom}"></line>` +
                               `<text class="chart-axis-label" x="${item.x.toFixed(2)}" y="${
                                   height - 18}" text-anchor="${anchor}">${escapeHtml(item.label)}</text>`);
                       })
                       .join('');
    const forecastEndMarker = useTimeAxis ?
        (`<line class="chart-forecast-end-line" x1="${(width - margin.right).toFixed(2)}" y1="${margin.top}" x2="${
             (width - margin.right).toFixed(2)}" y2="${height - margin.bottom}"></line>` +
            `<text class="chart-forecast-end-label" x="${(width - margin.right - 8).toFixed(2)}" y="${
                (margin.top + 16).toFixed(2)}" text-anchor="end">Forecast until</text>`) :
        '';
    const actualPath = chartPath(actual, xForIndex, yForValue);
    const hoverSeries = [];
    const actualColor = 'rgba(49, 20, 61, 0.9)';
    const actualMarkup = actualPath ? (() => {
        const seriesIndex = hoverSeries.length;
        const seriesId = 'actual';
        hoverSeries.push({
            label: 'Actual',
            color: actualColor,
            points: actual
                        .map(
                            (value, index) =>
                                (Number.isFinite(value) ?
                                     {date: labels[index] || '', value, x: xForIndex(index), y: yForValue(value)} :
                                     null))
                        .filter(Boolean),
        });
        return (
            `<path class="chart-line chart-actual-line" data-chart-series-id="${seriesId}" d="${actualPath}"></path>` +
            `<path class="chart-hover-path" data-chart-series-id="${seriesId}" data-series-index="${seriesIndex}" d="${
                actualPath}"></path>`);
    })() :
                                      '';
    const modelMarkup =
        series
            .map((item, index) => {
                const color = chartPalette[index % chartPalette.length];
                const seriesId = `model-${index}`;
                const values = Array.isArray(item.values) ? item.values : [];
                const path = chartPath(values, xForIndex, yForValue);
                const lastIndex = lastFiniteIndex(values);
                const marker = lastIndex >= 0 ?
                    `<circle class="chart-point" data-chart-series-id="${seriesId}" cx="${
                        xForIndex(lastIndex).toFixed(
                            2)}" cy="${yForValue(values[lastIndex]).toFixed(2)}" r="4.4" fill="${color}"></circle>` :
                    '';
                if (!path)
                    return '';
                const seriesIndex = hoverSeries.length;
                hoverSeries.push({
                    label: item.label || `Forecast ${index + 1}`,
                    color,
                    points: values
                                .map(
                                    (value, valueIndex) =>
                                        (Number.isFinite(value) ? {
                                            date: labels[valueIndex] || '',
                                            value,
                                            x: xForIndex(valueIndex),
                                            y: yForValue(value)
                                        } :
                                                                  null))
                                .filter(Boolean),
                });
                return (
                    `<path class="chart-line chart-model-line" data-chart-series-id="${seriesId}" style="stroke:${
                        color}" d="${path}"></path>${marker}` +
                    `<path class="chart-hover-path" data-chart-series-id="${seriesId}" data-series-index="${
                        seriesIndex}" d="${path}"></path>`);
            })
            .join('');
    const confidenceMarkup =
        series
            .map((item, index) => {
                const color = chartPalette[index % chartPalette.length];
                const parentId = `model-${index}`;
                const label = item.label || `Forecast ${index + 1}`;
                const parts = [];
                [{
                    kind: 'lower',
                    label: `${label} lower confidence`,
                    values: Array.isArray(item.lower) ? item.lower : [],
                    className: 'chart-confidence-line'
                },
                 {
                     kind: 'upper',
                     label: `${label} upper confidence`,
                     values: Array.isArray(item.upper) ? item.upper : [],
                     className: 'chart-confidence-line chart-confidence-upper'
                 },
                ].forEach((band) => {
                    const path = chartPath(band.values, xForIndex, yForValue);
                    if (!path)
                        return;
                    const seriesIndex = hoverSeries.length;
                    hoverSeries.push({
                        label: band.label,
                        color,
                        points: band.values
                                    .map(
                                        (value, valueIndex) =>
                                            (Number.isFinite(value) ? {
                                                date: labels[valueIndex] || '',
                                                value,
                                                x: xForIndex(valueIndex),
                                                y: yForValue(value)
                                            } :
                                                                      null))
                                    .filter(Boolean),
                    });
                    parts.push(
                        `<path class="chart-line ${band.className}" data-chart-series-id="confidence-${
                            band.kind}" data-confidence-kind="${band.kind}" data-chart-parent-id="${
                            parentId}" style="stroke:${color};display:none;" d="${path}"></path>` +
                        `<path class="chart-hover-path" data-chart-series-id="confidence-${
                            band.kind}" data-confidence-kind="${band.kind}" data-chart-parent-id="${
                            parentId}" data-series-index="${seriesIndex}" style="display:none;" d="${path}"></path>`);
                });
                return parts.join('');
            })
            .join('');
    const legendItems =
        [{id: 'actual', label: 'Actual', color: actualColor, checked: true}]
            .concat(series.map((item, index) => ({
                                   id: `model-${index}`,
                                   label: item.label || `Forecast ${index + 1}`,
                                   color: chartPalette[index % chartPalette.length],
                                   checked: true,
                               })))
            .concat([
                {id: 'confidence-lower', label: 'Lower confidence', color: chartLowerColor, checked: false},
                {id: 'confidence-upper', label: 'Upper confidence', color: chartUpperColor, checked: false},
            ]);
    const legend =
        ('<div class="chart-legend">' +
         '<span class="chart-legend-title">Legend</span>' +
         legendItems
             .map(
                 (item) =>
                     (`<label class="chart-legend-item${item.checked ? '' : ' is-off'}">` +
                         `<input type="checkbox" name="chart-series-toggle" data-series-id="${escapeHtml(item.id)}"${
                             item.checked ? ' checked' : ''}>` +
                         `<span class="chart-legend-swatch" style="--swatch:${item.color}"></span>${
                             escapeHtml(item.label)}` +
                         '</label>'))
             .join('') +
         '</div>');
    forecastChartBody.className = 'forecast-chart-body';
    forecastChartBody.innerHTML =
        (legend +
         `<svg class="forecast-chart-svg" viewBox="0 0 ${width} ${
             height}" role="img" aria-label="Actual and forecast chart">` +
         `<rect x="0" y="0" width="${width}" height="${height}" fill="rgba(8,29,22,0.52)"></rect>` + yGrid + xTicks +
         `<line class="chart-axis" x1="${margin.left}" y1="${height - margin.bottom}" x2="${
             width - margin.right}" y2="${height - margin.bottom}"></line>` +
         `<line class="chart-axis" x1="${margin.left}" y1="${margin.top}" x2="${margin.left}" y2="${
             height - margin.bottom}"></line>` +
         forecastEndMarker + confidenceMarkup + modelMarkup + actualMarkup +
         '<circle class="chart-hover-marker" r="5.5" visibility="hidden"></circle>' +
         '</svg>' +
         '<div class="chart-tooltip hidden"></div>');
    attachChartTooltip(hoverSeries);
    attachChartControls();
}

function renderSingleForecastChart(csvText, modelLabel, forecastEndLabel = '') {
    renderForecastChart(chartDataFromSingleCsv(csvText, modelLabel, forecastEndLabel));
}

function renderMultiForecastChart(results, forecastEndLabel = '') {
    renderForecastChart(chartDataFromMultiResults(results, forecastEndLabel));
}

function uniqueModelLabels(results) {
    const counts = new Map();
    return (Array.isArray(results) ? results : []).map((result, index) => {
        const raw = String(result && result.model || `Forecast ${index + 1}`).trim() || `Forecast ${index + 1}`;
        const seen = counts.get(raw) || 0;
        counts.set(raw, seen + 1);
        return seen ? `${raw} ${seen + 1}` : raw;
    });
}

function comparisonTableMarkup(results) {
    const successful =
        (Array.isArray(results) ? results : []).filter((result) => result && result.ok && result.forecast_csv);
    if (!successful.length) {
        return (
            '<div class="table-wrap comparison-table-wrap">' +
            '<div class="table-caption">No forecast rows returned for the selected models.</div>' +
            '<div class="table-scroll"><table><thead><tr><th>Date</th><th>Actual</th></tr></thead><tbody><tr><td colspan="2">No comparison rows returned.</td></tr></tbody></table></div></div>');
    }
    const models = uniqueModelLabels(successful);
    const order = [];
    const byDate = new Map();
    successful.forEach((result, index) => {
        const model = models[index];
        const rows = parseForecastCsvRowsForComparison(result.forecast_csv || '');
        rows.forEach((row) => {
            const date = String(row.date || '').trim();
            if (!date)
                return;
            if (!byDate.has(date)) {
                byDate.set(date, {date, actual: '', values: {}});
                order.push(date);
            }
            const entry = byDate.get(date);
            if (!entry.actual && row.actual)
                entry.actual = row.actual;
            entry.values[model] = {
                mean: String(row.mean || '').trim(),
                stderr: String(row.stderr || '').trim(),
            };
        });
    });
    const head =
        ('<tr><th>Date</th><th>Actual</th>' +
         models.map((model) => `<th>${escapeHtml(model)} mean</th><th>${escapeHtml(model)} stderr</th>`).join('') +
         '</tr>');
    const body = order.length ?
        order
            .map((date) => {
                const entry = byDate.get(date);
                return (
                    '<tr>' +
                    `<td>${escapeHtml(date)}</td>` +
                    `<td>${escapeHtml(entry.actual || '')}</td>` +
                    models
                        .map((model) => {
                            const cell = entry.values[model] || {mean: '', stderr: ''};
                            return `<td>${escapeHtml(cell.mean || '')}</td><td>${escapeHtml(cell.stderr || '')}</td>`;
                        })
                        .join('') +
                    '</tr>');
            })
            .join('') :
        `<tr><td colspan="${2 + (models.length * 2)}">No comparison rows returned.</td></tr>`;
    return (
        '<div class="table-wrap comparison-table-wrap">' +
        '<div class="table-caption">Actual vs forecast comparison across the selected models. Lower and upper bands are left out here so the model means and standard errors are easier to compare side by side.</div>' +
        `<div class="table-scroll"><table><thead>${head}</thead><tbody>${body}</tbody></table></div></div>`);
}

function renderComparisonResults(data) {
    const results = Array.isArray(data.results) ? data.results : [];
    if (!results.length) {
        return '<div class="summary-line note-line">No comparison results returned.</div>';
    }
    const structuredSummary = String(data.summary_html || '').trim();
    if (structuredSummary) {
        return ('<div class="comparison-stack">' + structuredSummary + comparisonTableMarkup(results) + '</div>');
    }
    return (
        '<div class="comparison-stack">' +
        results
            .map((result) => {
                const model = escapeHtml(result.model || 'Forecast');
                const note = `Target modelled: ${result.target_value_column_used || '(none)'} | Drivers used: ${
                    result.xreg_columns_used || '(none)'}`;
                const fitRows = escapeHtml(String(result.fit_rows ?? '-'));
                const stable = result.stationary ? 'Yes' : 'No';
                const invertible = result.invertible ? 'Yes' : 'No';
                const status = result.ok ? '' : ' rating-poor';
                const summary = result.ok ?
                    (result.summary_html || '<div class="summary-line note-line">No summary returned.</div>') :
                    `<div class="summary-overall rating-poor"><h4>${model}</h4><p>${
                        escapeHtml(result.error || 'Run failed.')}</p></div>`;
                return (
                    `<section class="comparison-run${status}">` +
                    `<div class="comparison-run-head"><h4>${model}</h4></div>` +
                    `<p class="summary-note">${escapeHtml(note)}</p>` +
                    '<div class="comparison-metrics">' +
                    `<div class="comparison-metric"><span>Historic rows</span><strong>${fitRows}</strong></div>` +
                    `<div class="comparison-metric"><span>Stationary</span><strong>${stable}</strong></div>` +
                    `<div class="comparison-metric"><span>Invertible</span><strong>${invertible}</strong></div>` +
                    `<div class="comparison-metric"><span>Run status</span><strong>${
                        result.ok ? 'OK' : 'Failed'}</strong></div>` +
                    '</div>' + summary + '</section>');
            })
            .join('') +
        comparisonTableMarkup(results) + '</div>');
}

function activateComparisonTab(button) {
    const tabs = button && button.closest ? button.closest('.comparison-tabs') : null;
    if (!tabs)
        return;
    const targetId = button.getAttribute('data-tab-target') || '';
    tabs.querySelectorAll('.comparison-tab').forEach((tab) => {
        const active = tab === button;
        tab.classList.toggle('is-active', active);
        tab.setAttribute('aria-selected', active ? 'true' : 'false');
    });
    tabs.querySelectorAll('.comparison-tab-panel').forEach((panel) => {
        panel.classList.toggle('is-active', panel.id === targetId);
    });
}

if (summaryBox) {
    summaryBox.addEventListener('click', (event) => {
        const target = event.target;
        const button = target && target.closest ? target.closest('.comparison-tab') : null;
        if (!button || !summaryBox.contains(button))
            return;
        event.preventDefault();
        activateComparisonTab(button);
    });
}

function applyForecastResult(data) {
    if (runButton)
        runButton.disabled = false;
    if (!data || !data.ok) {
        renderForecastChartEmpty('Run a forecast successfully to draw the chart.');
        setStatus((data && data.error) || 'Forecast failed.', 'error');
        return;
    }
    latestSummary = data.summary_text || '';
    latestForecastCsv = data.forecast_csv || '';
    window.__to_be_announcedLatestSummary = latestSummary;
    window.__to_be_announcedLatestForecastCsv = latestForecastCsv;
    if (data.multi) {
        renderMultiForecastChart(data.results || [], data.forecast_end_label || data.forecast_end_date || '');
        if (metricModel)
            metricModel.textContent = `${(data.results || []).length} models`;
        if (metricFitRows)
            metricFitRows.textContent = 'Varies';
        if (metricStationary)
            metricStationary.textContent = 'Mixed';
        if (metricInvertible)
            metricInvertible.textContent = 'Mixed';
        if (summaryBox)
            summaryBox.innerHTML = renderComparisonResults(data);
        if (forecastTableWrap)
            forecastTableWrap.classList.add('hidden');
        if (driversUsedNote) {
            driversUsedNote.textContent = `Target modelled in this run: ${
                data.target_value_column_used || '(none)'} | Drivers used in this run: ${
                data.xreg_columns_used ||
                '(none)'} | Downloads contain the combined multi-model summary and comparison CSV.`;
        }
    } else {
        renderSingleForecastChart(
            latestForecastCsv, data.model || 'Forecast', data.forecast_end_label || data.forecast_end_date || '');
        if (metricModel)
            metricModel.textContent = data.model || 'Forecast';
        if (metricFitRows)
            metricFitRows.textContent = String(data.fit_rows ?? '-');
        if (metricStationary)
            metricStationary.textContent = data.stationary ? 'Yes' : 'No';
        if (metricInvertible)
            metricInvertible.textContent = data.invertible ? 'Yes' : 'No';
        if (summaryBox) {
            if (data.summary_html)
                summaryBox.innerHTML = data.summary_html;
            else
                renderSummaryText(latestSummary);
        }
        renderForecastTable(latestForecastCsv);
        if (forecastTableWrap)
            forecastTableWrap.classList.remove('hidden');
        if (driversUsedNote) {
            driversUsedNote.textContent = `Target modelled in this run: ${
                data.target_value_column_used ||
                '(none)'} | Drivers used in this run: ${data.xreg_columns_used || '(none)'}`;
        }
    }
    if (downloadSummary) {
        downloadSummary.classList.toggle('disabled', !latestSummary);
        downloadSummary.setAttribute('aria-disabled', latestSummary ? 'false' : 'true');
    }
    if (downloadForecast) {
        downloadForecast.classList.toggle('disabled', !latestForecastCsv);
        downloadForecast.setAttribute('aria-disabled', latestForecastCsv ? 'false' : 'true');
    }
    setStatus('Forecast complete.', 'ok');
}

window.__to_be_announcedApplyForecastResult = applyForecastResult;

function downloadText(filename, content, type) {
    const blob = new Blob([content], {type});
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = filename;
    document.body.appendChild(link);
    link.click();
    link.remove();
    URL.revokeObjectURL(url);
}

function prepareForecastSubmit() {
    try {
        if (!form) {
            throw new Error('Forecast form not found.');
        }
        syncXregColumnsFromChecks();
        syncOutlierDatesFromChecks();
        const payload = collectState();
        for (const [key, value] of Object.entries(payload)) {
            const field = form.elements.namedItem(key);
            if (!field || !('value' in field))
                continue;
            field.value = value;
        }
        if (driversUsedNote) {
            driversUsedNote.textContent = `Target selected for this run: ${
                payload.target_value_column || '(none)'} | Models selected for this run: ${
                payload.models || payload.model ||
                '(none)'} | Drivers selected for this run: ${payload.xreg_columns || '(none)'}`;
        }
        runButton.disabled = true;
        setStatus('Submitting forecast...', '');
        saveState().catch(() => {});
        return true;
    } catch (error) {
        setStatus(error.message || String(error), 'error');
        runButton.disabled = false;
        return false;
    }
}

function runForecast() {
    if (!prepareForecastSubmit())
        return false;
    if (typeof form.requestSubmit === 'function') {
        form.requestSubmit(runButton);
    } else {
        form.submit();
    }
    return true;
}

window.__to_be_announcedPrepareForecastSubmit = prepareForecastSubmit;
window.__to_be_announcedUiRunForecast = runForecast;

async function refreshMobileDetails() {
    mobileStatus.textContent = 'Refreshing phone access…';
    try {
        const headers = controlToken ? {'X-Dval-Lab-Control': controlToken} : {};
        const response = await fetch(`${basePath}/mobile-access`, {cache: 'no-store', headers});
        const data = await response.json();
        mobileStatus.textContent = data.title || 'Phone access ready.';
        if (mobileTitle)
            mobileTitle.textContent = data.title || 'Phone access';
        mobileHint.textContent = data.hint || '';
        mobileUrl.textContent = data.url || '';
        mobileUrl.href = data.url || '#';
        qrBox.innerHTML = data.qr || '';
    } catch (error) {
        mobileStatus.textContent = error.message || String(error);
    }
}

async function copyMobileUrl() {
    const url = (mobileUrl.textContent || '').trim();
    if (!url)
        return;
    try {
        await navigator.clipboard.writeText(url);
        mobileStatus.textContent = 'Copied phone URL.';
    } catch (error) {
        mobileStatus.textContent = error.message || String(error);
    }
}

form.addEventListener('input', scheduleStateSave);
form.addEventListener('change', scheduleStateSave);

targetUpload.addEventListener('change', () => {
    const file = targetUpload.files && targetUpload.files[0];
    if (file) {
        targetUploadName.textContent = file.name;
        setStatus(`Selected ${file.name}. Uploading it now...`, '');
        uploadTargetCsv();
    } else {
        targetUploadName.textContent = 'No file chosen';
    }
});
xregUpload.addEventListener('change', () => {
    const file = xregUpload.files && xregUpload.files[0];
    if (file) {
        xregUploadName.textContent = file.name;
        setStatus(`Selected ${file.name}. Uploading it now...`, '');
        uploadXregCsv();
    } else {
        xregUploadName.textContent = 'No file chosen';
    }
});
targetValueSelect.addEventListener('change', () => {
    const chosen = String(targetValueSelect.value || '').trim();
    syncTargetValueTitle();
    targetPickerStatus.textContent = chosen ? `Chosen target: ${chosen}` : 'Choose the series you want to forecast.';
    loadExistingTargetColumns(chosen);
    scheduleStateSave();
});
forecastEndInput.addEventListener('change', () => {
    saveState().catch(() => {});
});
outlierModeSelect.addEventListener('change', () => {
    scheduleStateSave();
});
['p', 'd', 'q', 'P', 'D', 'Q'].forEach((name) => {
    const field = form.elements.namedItem(name);
    if (!field)
        return;
    field.addEventListener('input', () => {
        modelOrderEdited[name] = true;
        updateSeasonalSetupWarning();
        scheduleStateSave();
    });
});
form.elements.namedItem('season_period').addEventListener('change', () => {
    seasonPeriodManuallyEdited = true;
    updateSeasonalSetupWarning();
    scheduleStateSave();
});
modelList.addEventListener('change', (event) => {
    const input = event.target;
    if (!(input instanceof HTMLInputElement) || input.name !== 'model-choice') {
        return;
    }
    syncModelsFromChecks();
    applySuggestedModelSettings(false);
    updateSeasonalSetupWarning();
    saveState().catch(() => {});
});
outlierList.addEventListener('change', (event) => {
    const input = event.target;
    if (!(input instanceof HTMLInputElement) || input.name !== 'outlier-choice') {
        return;
    }
    syncOutlierDatesFromChecks();
    scheduleStateSave();
});
xregColumnsList.addEventListener('change', (event) => {
    const input = event.target;
    if (!(input instanceof HTMLInputElement) || input.name !== 'xreg-column-choice') {
        return;
    }
    syncXregColumnsFromChecks();
    saveState().catch(() => {});
});
form.elements.namedItem('target_date_column').addEventListener('change', () => {
    loadExistingTargetColumns();
    scheduleStateSave();
});
form.elements.namedItem('xreg_date_column').addEventListener('change', () => {
    loadExistingXregColumns();
    scheduleStateSave();
});

suggestButton.addEventListener('click', () => {
    Object.keys(modelOrderEdited).forEach((key) => {
        modelOrderEdited[key] = false;
    });
    seasonPeriodManuallyEdited = false;
    applySuggestedModelSettings(true);
    scheduleStateSave();
    setStatus('Suggested settings applied.', 'ok');
});

helpToggle.addEventListener('click', () => {
    if (helpPane.classList.contains('hidden'))
        showHelp();
    else
        showResults();
});

chartToggle.addEventListener('click', () => {
    if (chartPane.classList.contains('hidden'))
        showChart();
    else
        showResults();
});

refreshMobile.addEventListener('click', copyMobileUrl);
applyState(defaults);
renderModelChecksFromState(form.elements.namedItem('models').value);
syncModelsFromChecks();
syncTargetUploadName(
    form.elements.namedItem('target_path').value, form.elements.namedItem('target_display_name').value);
syncXregUploadName(form.elements.namedItem('xreg_path').value, form.elements.namedItem('xreg_display_name').value);
applyTargetMeta(initialTargetMeta);
applyXregMeta(initialXregMeta);
syncTargetValueTitle();
renderTargetColumnPicker(initialTargetColumns, form.elements.namedItem('target_value_column').value);
renderXregColumnsPicker(initialXregColumns, xregColumnsInput.value);
renderSeasonPeriodOptions(form.elements.namedItem('frequency').value, form.elements.namedItem('season_period').value);
updateSeasonalSetupWarning();
if (!initialTargetColumns.length)
    loadExistingTargetColumns();
if (!initialXregColumns.length)
    loadExistingXregColumns();
form.elements.namedItem('models').value = tbaConfig.state.models;
renderModelChecksFromState(form.elements.namedItem('models').value);
syncModelsFromChecks();
form.elements.namedItem('frequency').value = tbaConfig.state.frequency;
form.elements.namedItem('year_type').value = tbaConfig.state.year_type;
form.elements.namedItem('criterion').value = tbaConfig.state.criterion;
initialHydrating = false;
applySuggestedModelSettings(false);
refreshMobileDetails();
