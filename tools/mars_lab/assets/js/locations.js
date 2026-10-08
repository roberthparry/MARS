/**
 * Town selectors, observer coordinates and local-time controls.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function formatTownCoordinate(value, width) {
  const number = Number(value);
  if (!Number.isFinite(number))
    return String(value || '').trim();
  const sign = number >= 0 ? '+' : '-';
  const magnitude = Math.abs(number).toFixed(4).padStart(width - 1, '0');
  return `${sign}${magnitude}`;
}

function townOptionDisplay(option) {
  const lat = String(option && option.dataset.latitude || '').trim();
  const lon = String(option && option.dataset.longitude || '').trim();
  return {
    label: option ? option.textContent : '',
    detail: lat && lon ? `${formatTownCoordinate(lat, 8)}  ${formatTownCoordinate(lon, 9)}` : ''
  };
}

function validDateText(value, fallback = DEFAULT_DATETIME_DATE) {
  const text = String(value || '').trim();
  return /^\d{4}-\d{2}-\d{2}$/.test(text) ? text : fallback;
}

function formatAlmanacTimeInput(value) {
  const text = String(value || '').replace(',', '.');
  const decimalAt = text.indexOf('.');
  const clockText = decimalAt >= 0 ? text.slice(0, decimalAt) : text;
  const fraction = decimalAt >= 0
    ? text.slice(decimalAt + 1).replace(/\D/g, '')
    : '';
  const digits = clockText.replace(/\D/g, '').slice(0, 6);
  let formatted = digits.slice(0, 2);

  if (digits.length > 2)
    formatted += ':';
  if (digits.length > 2)
    formatted += digits.slice(2, 4);
  if (digits.length > 4)
    formatted += ':';
  if (digits.length > 4)
    formatted += digits.slice(4, 6);
  if (decimalAt >= 0 && digits.length === 6)
    formatted += `.${fraction}`;
  return formatted;
}

function validDatetimeJurisdiction(value, fallback = DEFAULT_DATETIME_JURISDICTION) {
  const jurisdiction = String(value || '').trim();
  return HOLIDAY_JURISDICTION_SET.has(jurisdiction) ? jurisdiction : fallback;
}

function townsForJurisdiction(jurisdiction) {
  const code = validDatetimeJurisdiction(jurisdiction, DEFAULT_DATETIME_JURISDICTION);
  if (Array.isArray(JURISDICTION_TOWN_OPTIONS[code]))
    return JURISDICTION_TOWN_OPTIONS[code];
  const country = code.split('-', 1)[0];
  return Array.isArray(JURISDICTION_TOWN_OPTIONS[country]) ? JURISDICTION_TOWN_OPTIONS[country] : [];
}

function townOptionValue(town) {
  return [
    String(town && town.name || '').trim(),
    String(town && town.latitude || '').trim(),
    String(town && town.longitude || '').trim(),
    String(town && town.elevation || '').trim()
  ].join('|');
}

function townValueParts(value) {
  const parts = String(value || '').split('|');
  return {
    name: String(parts[0] || '').trim(),
    latitude: String(parts[1] || '').trim(),
    longitude: String(parts[2] || '').trim(),
    elevation: String(parts[3] || '').trim()
  };
}

function townOptionMatchesValue(option, value) {
  if (!option || !value)
    return false;
  const wanted = townValueParts(value);
  const candidate = townValueParts(option.value);
  if (!wanted.name || candidate.name !== wanted.name)
    return false;
  if (!numbersNearlyEqual(candidate.latitude, wanted.latitude))
    return false;
  if (!numbersNearlyEqual(candidate.longitude, wanted.longitude))
    return false;
  return !wanted.elevation || !candidate.elevation || candidate.elevation === wanted.elevation;
}

function numbersNearlyEqual(left, right, tolerance = 0.000001) {
  const a = Number(left);
  const b = Number(right);
  return Number.isFinite(a) && Number.isFinite(b) && Math.abs(a - b) <= tolerance;
}

function syncRoundedSelect(select) {
  if (select && typeof select.__marsRebuildRoundedSelect === 'function')
    select.__marsRebuildRoundedSelect();
  else if (select && typeof select.__marsSyncRoundedSelect === 'function')
    select.__marsSyncRoundedSelect();
}

function selectTownByCoordinates(select, latitude, longitude) {
  if (!select)
    return false;
  const option = Array.from(select.options).find((candidate) =>
    numbersNearlyEqual(candidate.dataset.latitude, latitude) &&
    numbersNearlyEqual(candidate.dataset.longitude, longitude)
  );
  if (!option)
    return false;
  select.value = option.value;
  syncRoundedSelect(select);
  return true;
}

function restoreTownSelection(select, jurisdiction, townValue, latitude, longitude) {
  populateTownSelect(select, jurisdiction, {selectDefault: false});
  const wanted = String(townValue || '').trim();
  if (wanted && Array.from(select.options).some((option) => option.value === wanted)) {
    select.value = wanted;
    syncRoundedSelect(select);
    applyRestoredTownSelection(select);
    return true;
  }
  const compatible = Array.from(select.options).find((option) => townOptionMatchesValue(option, wanted));
  if (compatible) {
    select.value = compatible.value;
    syncRoundedSelect(select);
    applyRestoredTownSelection(select);
    return true;
  }
  if (selectTownByCoordinates(select, latitude, longitude))
    return true;
  select.value = '';
  syncRoundedSelect(select);
  return false;
}

function applyRestoredTownSelection(select) {
  if (select === datetimeTown) {
    applySelectedTown({
      townSelect: datetimeTown,
      latitudeInput: datetimeLatitude,
      longitudeInput: datetimeLongitude,
      elevationInput: datetimeElevation,
      zoneInput: datetimeGmtOffset,
      dateInput: datetimeDate,
      resetOffsetTouched: true
    });
  } else if (select === almanacTown) {
    applySelectedTown({
      townSelect: almanacTown,
      latitudeInput: almanacLatitude,
      longitudeInput: almanacLongitude,
      elevationInput: almanacElevation,
      zoneInput: almanacZone,
      dateInput: almanacDate
    });
  }
}

function populateTownSelect(select, jurisdiction, {selectDefault = true} = {}) {
  if (!select)
    return [];
  const previous = String(select.value || '');
  const towns = townsForJurisdiction(jurisdiction);
  select.textContent = '';

  towns.forEach((town, index) => {
    const option = document.createElement('option');
    option.value = townOptionValue(town) || String(index);
    option.textContent = String(town.name || 'Location');
    option.dataset.latitude = String(town.latitude || '');
    option.dataset.longitude = String(town.longitude || '');
    option.dataset.elevation = String(town.elevation || '');
    option.dataset.timezone = String(town.timezone || '');
    if (town.default)
      option.dataset.default = '1';
    select.appendChild(option);
  });

  if (selectDefault && towns.length) {
    const defaultIndex = towns.findIndex((town) => !!town.default);
    select.value = townOptionValue(towns[defaultIndex >= 0 ? defaultIndex : 0]);
  } else if (previous && Array.from(select.options).some((option) => option.value === previous)) {
    select.value = previous;
  } else if (towns.length) {
    select.value = townOptionValue(towns[0]);
  } else {
    select.value = '';
  }
  syncRoundedSelect(select);
  return towns;
}

function selectedTownOption(select) {
  if (!select || !select.value)
    return null;
  const byValue = Array.from(select.options).find((option) => option.value === select.value);
  return byValue || select.selectedOptions[0] || null;
}

function clearTownForCustomCoordinates(townSelect, latitudeInput, longitudeInput, elevationInput) {
  const option = selectedTownOption(townSelect);
  if (!option)
    return;
  const latitudeMatches = numbersNearlyEqual(option.dataset.latitude, latitudeInput && latitudeInput.value);
  const longitudeMatches = numbersNearlyEqual(option.dataset.longitude, longitudeInput && longitudeInput.value);
  const selectedElevation = String(option.dataset.elevation || '').trim();
  const currentElevation = String(elevationInput && elevationInput.value || '').trim();
  const elevationMatches = !selectedElevation || !currentElevation ||
    numbersNearlyEqual(selectedElevation, currentElevation, 0.01);
  if (latitudeMatches && longitudeMatches && elevationMatches)
    return;
  townSelect.value = '';
  syncRoundedSelect(townSelect);
}

function timeZoneOffsetHours(timeZone, dateText) {
  if (!timeZone)
    return null;
  const parsed = validDateText(dateText, '');
  if (!parsed)
    return null;
  const probe = new Date(`${parsed}T12:00:00Z`);
  if (Number.isNaN(probe.getTime()))
    return null;
  try {
    const formatter = new Intl.DateTimeFormat('en-GB', {
      timeZone,
      hour12: false,
      year: 'numeric',
      month: '2-digit',
      day: '2-digit',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
    const parts = Object.fromEntries(formatter.formatToParts(probe).map((part) => [part.type, part.value]));
    const localAsUtc = Date.UTC(
      Number(parts.year),
      Number(parts.month) - 1,
      Number(parts.day),
      Number(parts.hour),
      Number(parts.minute),
      Number(parts.second)
    );
    return (localAsUtc - probe.getTime()) / 3600000;
  } catch (_) {
    return null;
  }
}

function formatOffsetHours(offset) {
  if (offset === null || !Number.isFinite(offset))
    return '';
  if (Math.abs(offset - Math.round(offset)) < 1e-9)
    return String(Math.round(offset));
  return String(Math.round(offset * 100) / 100);
}

function applySelectedTown({townSelect, latitudeInput, longitudeInput, elevationInput, zoneInput, dateInput, resetOffsetTouched = false} = {}) {
  const option = selectedTownOption(townSelect);
  if (!option)
    return false;
  if (latitudeInput && option.dataset.latitude)
    latitudeInput.value = option.dataset.latitude;
  if (longitudeInput && option.dataset.longitude)
    longitudeInput.value = option.dataset.longitude;
  if (elevationInput && option.dataset.elevation)
    elevationInput.value = option.dataset.elevation;
  if (zoneInput) {
    const offset = timeZoneOffsetHours(option.dataset.timezone || '', dateInput && dateInput.value);
    const offsetText = formatOffsetHours(offset);
    if (offsetText)
      zoneInput.value = offsetText;
  }
  if (resetOffsetTouched) {
    datetimeAutoGmtOffset = String(zoneInput && zoneInput.value || '').trim();
    datetimeGmtOffsetTouched = false;
  }
  syncRoundedSelect(townSelect);
  return true;
}

function syncTownSelectors({selectDefault = false} = {}) {
  populateTownSelect(
    datetimeTown,
    datetimeJurisdiction && datetimeJurisdiction.value,
    {selectDefault}
  );
  populateTownSelect(
    almanacTown,
    almanacJurisdiction && almanacJurisdiction.value,
    {selectDefault}
  );
}

function restoreDatetimeDefaultsIfBlank() {
  if (datetimeDate && !datetimeDate.value)
    datetimeDate.value = DEFAULT_DATETIME_DATE;
  if (datetimeStart && !datetimeStart.value)
    datetimeStart.value = datetimeDate?.value || DEFAULT_DATETIME_DATE;
  if (datetimeEnd && !datetimeEnd.value)
    datetimeEnd.value = datetimeDate?.value || DEFAULT_DATETIME_DATE;
  if (datetimeYear && !datetimeYear.value)
    datetimeYear.value = String((datetimeDate?.value || DEFAULT_DATETIME_DATE).slice(0, 4));
  if (datetimeJurisdiction && !datetimeJurisdiction.value)
    setSelectValue(datetimeJurisdiction, DEFAULT_DATETIME_JURISDICTION);
  if (datetimeLatitude && !datetimeLatitude.value)
    datetimeLatitude.value = DEFAULT_DATETIME_LATITUDE;
  if (datetimeLongitude && !datetimeLongitude.value)
    datetimeLongitude.value = DEFAULT_DATETIME_LONGITUDE;
  if (datetimeElevation && !datetimeElevation.value)
    datetimeElevation.value = DEFAULT_DATETIME_ELEVATION;
}

function currentDatetimeState() {
  restoreDatetimeDefaultsIfBlank();
  const currentOffsetText = String(datetimeGmtOffset && datetimeGmtOffset.value || '').trim();
  const effectiveOffsetText = (!datetimeGmtOffsetTouched || currentOffsetText === datetimeAutoGmtOffset)
    ? ''
    : currentOffsetText;
  return {
    date: validDateText(datetimeDate && datetimeDate.value),
    jdn: String(datetimeJdn && datetimeJdn.value || '').trim(),
    start: validDateText(datetimeStart && datetimeStart.value, datetimeDate && datetimeDate.value || DEFAULT_DATETIME_DATE),
    end: validDateText(datetimeEnd && datetimeEnd.value, datetimeDate && datetimeDate.value || DEFAULT_DATETIME_DATE),
    year: String(datetimeYear && datetimeYear.value || (datetimeDate && datetimeDate.value || DEFAULT_DATETIME_DATE).slice(0, 4)).trim(),
    jurisdiction: validDatetimeJurisdiction(datetimeJurisdiction && datetimeJurisdiction.value),
    town: String(datetimeTown && datetimeTown.value || '').trim(),
    latitude: String(datetimeLatitude && datetimeLatitude.value || DEFAULT_DATETIME_LATITUDE).trim(),
    longitude: String(datetimeLongitude && datetimeLongitude.value || DEFAULT_DATETIME_LONGITUDE).trim(),
    elevation: String(datetimeElevation && datetimeElevation.value || DEFAULT_DATETIME_ELEVATION).trim(),
    gmt_offset: effectiveOffsetText
  };
}

function restoreAlmanacDefaultsIfBlank() {
  if (almanacDate && !almanacDate.value)
    almanacDate.value = DEFAULT_ALMANAC_DATE;
  if (almanacTime && !almanacTime.value)
    almanacTime.value = DEFAULT_ALMANAC_TIME;
  if (almanacZone && !almanacZone.value)
    almanacZone.value = DEFAULT_ALMANAC_ZONE;
  if (almanacJurisdiction && !almanacJurisdiction.value)
    setSelectValue(almanacJurisdiction, DEFAULT_DATETIME_JURISDICTION);
  if (almanacLatitude && !almanacLatitude.value)
    almanacLatitude.value = DEFAULT_ALMANAC_LATITUDE;
  if (almanacLongitude && !almanacLongitude.value)
    almanacLongitude.value = DEFAULT_ALMANAC_LONGITUDE;
  if (almanacElevation && !almanacElevation.value)
    almanacElevation.value = DEFAULT_ALMANAC_ELEVATION;
  almanacVisibilityMode = validAlmanacVisibility(almanacVisibilityMode, DEFAULT_ALMANAC_VISIBILITY);
}

function validAlmanacVisibility(value, fallback = DEFAULT_ALMANAC_VISIBILITY) {
  const raw = String(value || '').trim().toLowerCase();
  return raw === 'visible' || raw === 'all' ? raw : fallback;
}

function currentAlmanacState() {
  restoreAlmanacDefaultsIfBlank();
  return {
    date: validDateText(almanacDate && almanacDate.value, DEFAULT_ALMANAC_DATE),
    time: String(almanacTime && almanacTime.value || DEFAULT_ALMANAC_TIME).trim(),
    zone: String(almanacZone && almanacZone.value || DEFAULT_ALMANAC_ZONE).trim(),
    jurisdiction: validDatetimeJurisdiction(almanacJurisdiction && almanacJurisdiction.value),
    town: String(almanacTown && almanacTown.value || '').trim(),
    latitude: String(almanacLatitude && almanacLatitude.value || DEFAULT_ALMANAC_LATITUDE).trim(),
    longitude: String(almanacLongitude && almanacLongitude.value || DEFAULT_ALMANAC_LONGITUDE).trim(),
    elevation: String(almanacElevation && almanacElevation.value || DEFAULT_ALMANAC_ELEVATION).trim(),
    visibility: validAlmanacVisibility(almanacVisibilityMode, DEFAULT_ALMANAC_VISIBILITY)
  };
}

function almanacSummaryText(state = currentAlmanacState()) {
  return [
    ALMANAC_WORKSHEET_TITLE,
    `Date: ${state.date}`,
    `GMT time: ${state.time}`,
    `Jurisdiction: ${state.jurisdiction}`,
    `Zone: ${state.zone}`,
    `Latitude: ${state.latitude}`,
    `Longitude: ${state.longitude}`,
    `Altitude: ${state.elevation} m`,
    `Show bodies: ${state.visibility === 'visible' ? 'visible only' : 'all bodies'}`
  ].join('\n');
}

function datetimeSummaryText(state = currentDatetimeState()) {
  return [
    'MARS datetime observation',
    `Date: ${state.date}`,
    state.jdn ? `Julian Day Number: ${state.jdn}` : '',
    `Range: ${state.start} to ${state.end}`,
    `Year: ${state.year}`,
    `Holiday jurisdiction: ${state.jurisdiction}`,
    `Location: ${state.latitude}, ${state.longitude}`,
    `GMT offset: ${state.gmt_offset || 'local machine offset'}`
  ].filter(Boolean).join('\n');
}

function setDatetimeLocalText(text, sections = null) {
  const body = String(text || '').trim();
  if (datetimeLocalBody)
    renderDatetimeSections(datetimeLocalBody, null, sections, body);
  if (datetimeLocal)
    datetimeLocal.classList.toggle('hidden', !body || currentMode() !== 'datetime');
}
