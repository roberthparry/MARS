/**
 * Almanac worksheets, event tables and observer-location refresh.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

async function refreshAlmanacLandTotality(data) {
  const cells = Array.from(rendered.querySelectorAll('[data-almanac-land-totality]'));
  if (!cells.length || !data)
    return;
  const refreshId = ++almanacLandTotalitySequence;
  const fields = data.fields || {};
  const controller = new AbortController();
  const timer = window.setTimeout(() => controller.abort(), ALMANAC_LAND_TOTALITY_SEARCH_TIMEOUT_MS);
  try {
    const response = await fetch('/almanac-land-totality', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      signal: controller.signal,
      body: JSON.stringify({
        event_year: data.event_year || fields.event_year || '',
        jurisdiction: fields.jurisdiction || (almanacJurisdiction && almanacJurisdiction.value) || DEFAULT_DATETIME_JURISDICTION,
        zone: fields.zone || (almanacZone && almanacZone.value) || DEFAULT_ALMANAC_ZONE,
        latitude: fields.latitude || (almanacLatitude && almanacLatitude.value) || DEFAULT_ALMANAC_LATITUDE,
        longitude: fields.longitude || (almanacLongitude && almanacLongitude.value) || DEFAULT_ALMANAC_LONGITUDE,
        events: cells.map((cell) => ({jd: String(cell.dataset.almanacLandTotality || '').trim()}))
      })
    });
    const payload = await response.json();
    if (refreshId !== almanacLandTotalitySequence || currentMode() !== 'almanac')
      return;
    if (!response.ok || !payload.ok)
      throw new Error(payload.error || 'Nearest land totality search failed');
    const items = Array.isArray(payload.items) ? payload.items : [];
    cells.forEach((cell) => {
      const jd = String(cell.dataset.almanacLandTotality || '').trim();
      const match = items.find((item) => String(item.jd || '').trim() === jd);
      if (match && match.nearest_totality) {
        cell.innerHTML = almanacNearestTotalityCellHtml(
          String(match.nearest_totality),
          match.nearest_totality_action || null
        );
      } else {
        cell.textContent = payload.timed_out ? 'Nearest land totality search timed out' : 'No land totality found';
      }
    });
    bindAlmanacTotalityActions(rendered);
  } catch (err) {
    if (refreshId !== almanacLandTotalitySequence || currentMode() !== 'almanac')
      return;
    cells.forEach((cell) => {
      cell.textContent = err && err.name === 'AbortError'
        ? 'Nearest land totality search timed out'
        : 'Nearest land totality unavailable';
    });
  } finally {
    window.clearTimeout(timer);
  }
}

function escapeHtml(text) {
  return String(text || '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function almanacRowsForVisibility(data, visibility) {
  const allRows = Array.isArray(data && data.all_rows)
    ? data.all_rows
    : (Array.isArray(data && data.rows) ? data.rows : []);
  if (visibility === 'visible')
    return allRows.filter((row) => String(row && row.visible || '').trim().toUpperCase() === 'YES');
  return allRows;
}

function almanacBodyTextForVisibility(visibility) {
  return `Location of Navigational Bodies; ${visibility === 'visible' ? 'visible bodies only' : 'all bodies shown'}`;
}

function compactAlmanacLocalTime(value) {
  const text = String(value || '').trim();
  const match = text.match(/^\d{4}-\d{2}-\d{2}\s+(\d{2}:\d{2}:\d{2})(?:\s+GMT(?:[+-]\d{1,2}(?::\d{2})?)?)?$/);
  return match ? match[1] : text;
}

function compactAlmanacGmtTime(value) {
  return compactAlmanacLocalTime(value).replace(/\s+GMT$/, '');
}

function almanacEventDateText(event) {
  if (!event || typeof event !== 'object')
    return '';
  const candidates = [
    event.greatest,
    event.time,
    event.first_contact,
    event.fourth_contact,
    event.gmt_time
  ];
  for (const value of candidates) {
    const match = String(value || '').trim().match(/^(\d{4}-\d{2}-\d{2})\b/);
    if (match)
      return match[1];
  }
  return '';
}

function almanacTotalityActionAttributes(action) {
  if (!action || typeof action !== 'object')
    return '';
  const fields = ['date', 'time', 'zone', 'jurisdiction', 'town', 'latitude', 'longitude', 'elevation'];
  return fields.map((field) =>
    ` data-${field.replace(/_/g, '-')}="${escapeHtml(action[field] || '')}"`
  ).join('');
}

function almanacNearestTotalityCellHtml(text, action) {
  const body = escapeHtml(text || '');
  if (!action || typeof action !== 'object' || !String(action.town || '').trim())
    return body;
  return `
    <span class="almanac-totality-action">
      <span>${body}</span>
      <button type="button" class="almanac-use-totality" data-almanac-use-totality="1"${almanacTotalityActionAttributes(action)}>Use</button>
    </span>`;
}

function applyAlmanacTotalityAction(button) {
  if (!button)
    return;
  const date = validDateText(button.dataset.date, '');
  const time = String(button.dataset.time || '').trim();
  const zone = String(button.dataset.zone || '').trim();
  const jurisdiction = validDatetimeJurisdiction(button.dataset.jurisdiction, DEFAULT_DATETIME_JURISDICTION);
  const town = String(button.dataset.town || '').trim();
  const latitude = String(button.dataset.latitude || '').trim();
  const longitude = String(button.dataset.longitude || '').trim();
  const elevation = String(button.dataset.elevation || '').trim();

  if (almanacDate && date)
    almanacDate.value = date;
  if (almanacTime && time)
    almanacTime.value = time;
  if (almanacZone && zone)
    almanacZone.value = zone;
  if (almanacJurisdiction)
    setSelectValue(almanacJurisdiction, jurisdiction);
  populateTownSelect(almanacTown, jurisdiction, {selectDefault: false});
  if (!restoreTownSelection(almanacTown, jurisdiction, town, latitude, longitude) && almanacTown)
    almanacTown.value = '';
  if (almanacLatitude && latitude)
    almanacLatitude.value = latitude;
  if (almanacLongitude && longitude)
    almanacLongitude.value = longitude;
  if (almanacElevation && elevation)
    almanacElevation.value = elevation;
  saveLastAlmanacState();
  evaluateCurrentMode();
}

function bindAlmanacTotalityActions(root) {
  (root || document).querySelectorAll('[data-almanac-use-totality]').forEach((button) => {
    if (button.dataset.boundTotalityAction === '1')
      return;
    button.dataset.boundTotalityAction = '1';
    button.addEventListener('click', () => applyAlmanacTotalityAction(button));
  });
}

function almanacWorksheetCopyText(data, visibility) {
  const rows = almanacRowsForVisibility(data, visibility);
  const events = Array.isArray(data && data.events) ? data.events : [];
  const showVisibleColumn = visibility === 'all';
  const lines = [
    data && data.worksheet_title || ALMANAC_WORKSHEET_TITLE,
    data && data.moment_text || '',
    data && data.observer_text || '',
    almanacBodyTextForVisibility(visibility),
    `Body filter: ${visibility === 'visible' ? 'visible only' : 'all bodies'}`,
    '',
    showVisibleColumn
      ? 'Body | Declination | GHA | RA | Altitude | Azimuth | s.d. | Vmag. | Visible'
      : 'Body | Declination | GHA | RA | Altitude | Azimuth | s.d. | Vmag.'
  ].filter((line, index) => index >= 4 || String(line || '').trim());

  if (rows.length) {
    rows.forEach((row) => {
      const cells = [
        String(row.name || row.code || '').trim(),
        String(row.declination || '').trim(),
        String(row.gha || '').trim(),
        String(row.right_ascension || '').trim(),
        String(row.altitude || '').trim(),
        String(row.azimuth || '').trim(),
        String(row.semi_diameter || '').trim(),
        String(row.magnitude || '').trim()
      ];
      if (showVisibleColumn)
        cells.push(String(row.visible || '').trim());
      lines.push(cells.join(' | '));
    });
  } else {
    lines.push(showVisibleColumn
      ? 'No bodies found for the current visibility filter. |  |  |  |  |  |  |  | '
      : 'No bodies found for the current visibility filter. |  |  |  |  |  |  | ');
  }

  lines.push('', data && data.event_title || '');
  lines.push('Class | Event | Kind | Magnitude | Obscuration | Date | First contact | Greatest eclipse | Fourth contact | Greatest GMT | Notes | Nearest totality');
  if (events.length) {
    events.forEach((event) => {
      lines.push([
        String(event.category || '').trim(),
        String(event.name || '').trim(),
        String(event.kind || '').trim(),
        String(event.magnitude || '').trim(),
        String(event.obscuration || '').trim(),
        almanacEventDateText(event),
        String(event.first_contact || '').trim(),
        String(event.greatest || event.time || '').trim(),
        String(event.fourth_contact || '').trim(),
        String(event.gmt_time || '').trim(),
        String(event.details || '').trim(),
        String(event.nearest_totality || '').trim()
      ].join(' | '));
    });
  } else {
    lines.push('No events found.');
  }
  return lines.join('\n');
}

function renderAlmanacWorksheet(target, data) {
  data = data || {};
  const events = Array.isArray(data.events) ? data.events : [];
  const visibility = validAlmanacVisibility(data.visibility, almanacVisibilityMode);
  const rows = almanacRowsForVisibility(data, visibility);
  const worksheetTitle = escapeHtml(data.worksheet_title || ALMANAC_WORKSHEET_TITLE);
  const momentText = escapeHtml(data.moment_text || '');
  const observerText = escapeHtml(data.observer_text || '');
  const bodyText = escapeHtml(almanacBodyTextForVisibility(visibility));
  const eventTitle = escapeHtml(data.event_title || '');
  const showVisibleColumn = visibility === 'all';
  const bodyColumnCount = showVisibleColumn ? 9 : 8;
  almanacVisibilityMode = visibility;
  target.dataset.copyText = almanacWorksheetCopyText(data, visibility);
  target.innerHTML = `
    <div class="almanac-sheet">
      <div class="almanac-sheet-header">
        <div class="almanac-sheet-title">${worksheetTitle}</div>
        <div>${momentText}</div>
        <div>${observerText}</div>
        <div>${bodyText}</div>
        <div class="almanac-sheet-toolbar" aria-label="Body list filter">
          <span class="almanac-sheet-toolbar-label">Body list</span>
          <span class="almanac-visibility-toggle" role="group" aria-label="Body list filter">
            <button type="button" class="${visibility === 'all' ? 'active' : ''}" data-almanac-visibility="all" aria-pressed="${visibility === 'all' ? 'true' : 'false'}">All</button>
            <button type="button" class="${visibility === 'visible' ? 'active' : ''}" data-almanac-visibility="visible" aria-pressed="${visibility === 'visible' ? 'true' : 'false'}">Visible</button>
          </span>
        </div>
      </div>
      <div class="almanac-table-scroll" role="region" aria-label="Navigational bodies" tabindex="0">
      <table class="almanac-grid-table">
        <thead>
          <tr>
            <th>Body</th>
            <th>Declination</th>
            <th>GHA</th>
            <th>RA</th>
            <th>Altitude</th>
            <th>Azimuth</th>
            <th>s.d.</th>
            <th>Vmag.</th>
            ${showVisibleColumn ? '<th>Visible</th>' : ''}
          </tr>
        </thead>
        <tbody>
          ${rows.length ? rows.map((row) => {
            const visible = String(row.visible || '').trim().toUpperCase();
            const isVisible = visible === 'YES';
            const visibleLabel = isVisible ? 'Visible' : 'Not visible';
            const visibleIcon = isVisible ? '✓' : '✕';
            const classes = row.kind === 'reference' ? 'reference' : '';
            const nameClass = row.kind === 'reference' ? 'reference-name' : 'body-name';
            return `
              <tr class="${classes}">
                <td class="${nameClass}">${escapeHtml(row.name || row.code || '')}</td>
                <td class="number">${escapeHtml(row.declination || '')}</td>
                <td class="number">${escapeHtml(row.gha || '')}</td>
                <td class="number">${escapeHtml(row.right_ascension || '')}</td>
                <td class="number">${escapeHtml(row.altitude || '')}</td>
                <td class="number">${escapeHtml(row.azimuth || '')}</td>
                <td class="number">${escapeHtml(row.semi_diameter || '')}</td>
                <td class="number">${escapeHtml(row.magnitude || '')}</td>
                ${showVisibleColumn ? `<td class="visible-cell ${isVisible ? 'yes' : 'no'}" title="${escapeHtml(visibleLabel)}"><span class="almanac-visible-icon" aria-label="${escapeHtml(visibleLabel)}" role="img">${visibleIcon}</span></td>` : ''}
              </tr>`;
          }).join('') : `<tr><td colspan="${bodyColumnCount}">No bodies found for the current visibility filter.</td></tr>`}
        </tbody>
      </table>
      </div>
      <div class="almanac-events-title">${eventTitle}</div>
      <div class="almanac-table-scroll" role="region" aria-label="Upcoming astronomical events" tabindex="0">
      <table class="almanac-grid-table almanac-event-table">
        <thead>
          <tr>
            <th class="event-class">Class</th>
            <th class="event-name">Event</th>
            <th class="event-kind">Kind</th>
            <th class="event-measure" title="Magnitude">Mag.</th>
            <th class="event-measure" title="Obscuration">Obsc.</th>
            <th class="event-date">Date</th>
            <th class="event-time">First</th>
            <th class="event-time">Greatest</th>
            <th class="event-time">Fourth</th>
            <th class="event-gmt" title="Greatest GMT">GMT</th>
            <th class="event-totality">Nearest Totality</th>
          </tr>
        </thead>
        <tbody>
          ${events.length ? events.map((event) => {
            const needsLandSearch = String(event.category || '').trim() === 'Solar'
              && String(event.name || '').trim() === 'Solar eclipse'
              && String(event.kind || '').trim() !== 'total'
              && !String(event.nearest_totality || '').trim();
            const nearestTotality = needsLandSearch
              ? 'Searching for nearest location on land...'
              : String(event.nearest_totality || '').trim();
            return `
            <tr data-almanac-event-jd="${escapeHtml(event.jd || '')}">
              <td data-label="Class">${escapeHtml(event.category || '')}</td>
              <td class="body-name" data-label="Event">${escapeHtml(event.name || '')}</td>
              <td class="event-kind" data-label="Kind">${escapeHtml(event.kind || '')}</td>
              <td class="number event-measure" data-label="Magnitude">${escapeHtml(event.magnitude || '')}</td>
              <td class="number event-measure" data-label="Obscuration">${escapeHtml(event.obscuration || '')}</td>
              <td class="number event-date" data-label="Date">${escapeHtml(almanacEventDateText(event))}</td>
              <td class="number event-time" data-label="First" title="${escapeHtml(event.first_contact || '')}">${escapeHtml(compactAlmanacLocalTime(event.first_contact || ''))}</td>
              <td class="number event-time" data-label="Greatest" title="${escapeHtml(event.greatest || event.time || '')}">${escapeHtml(compactAlmanacLocalTime(event.greatest || event.time || ''))}</td>
              <td class="number event-time" data-label="Fourth" title="${escapeHtml(event.fourth_contact || '')}">${escapeHtml(compactAlmanacLocalTime(event.fourth_contact || ''))}</td>
              <td class="number event-gmt" data-label="GMT" title="${escapeHtml(event.gmt_time || '')}">${escapeHtml(compactAlmanacGmtTime(event.gmt_time || ''))}</td>
              <td class="event-details" data-label="Nearest totality" ${needsLandSearch ? `data-almanac-land-totality="${escapeHtml(event.jd || '')}"` : ''}>${almanacNearestTotalityCellHtml(nearestTotality, event.nearest_totality_action || null)}</td>
            </tr>`;
          }).join('') : `
            <tr>
              <td colspan="11">No eclipses or Mercury/Venus transits found in this one-year window.</td>
            </tr>`}
        </tbody>
      </table>
      </div>
    </div>`;
  target.querySelectorAll('[data-almanac-visibility]').forEach((button) => {
    button.addEventListener('click', () => {
      const nextVisibility = validAlmanacVisibility(button.dataset.almanacVisibility, almanacVisibilityMode);
      if (nextVisibility === almanacVisibilityMode)
        return;
      almanacVisibilityMode = nextVisibility;
      saveLastAlmanacState();
      if (almanacLastWorksheetData) {
        renderAlmanacWorksheet(target, {...almanacLastWorksheetData, visibility: nextVisibility});
        refreshAlmanacLandTotality(almanacLastWorksheetData);
        setStatus('Ready');
      } else {
        evaluateAlmanac({skipHistoryUpdate: true});
      }
    });
  });
  bindAlmanacTotalityActions(target);
}

async function refreshDatetimeLocalHolidays() {
  if (currentMode() !== 'datetime')
    return;

  const refreshId = ++datetimeLocalRefreshSequence;
  setStatus('Refreshing holidays...');
  try {
    const {response, data} = await fetchDatetimeEvaluation();
    if (refreshId !== datetimeLocalRefreshSequence || currentMode() !== 'datetime')
      return;
    if (!response.ok || !data.ok) {
      setStatus('Error');
      return;
    }
    setDatetimeLocalText(data.local || '', data.local_sections || []);
    setStatus('Ready');
  } catch (_) {
    if (refreshId !== datetimeLocalRefreshSequence || currentMode() !== 'datetime')
      return;
    setStatus('Error');
  }
}

async function refreshDatetimeJurisdictionLocation({updateCoordinates = true} = {}) {
  if (!datetimeJurisdiction)
    return;

  const jurisdiction = validDatetimeJurisdiction(datetimeJurisdiction.value, DEFAULT_DATETIME_JURISDICTION);
  const date = validDateText(datetimeDate && datetimeDate.value, DEFAULT_DATETIME_DATE);
  let townApplied = false;
  if (updateCoordinates) {
    populateTownSelect(datetimeTown, jurisdiction, {selectDefault: false});
    if (!selectTownByCoordinates(datetimeTown, datetimeLatitude && datetimeLatitude.value, datetimeLongitude && datetimeLongitude.value))
      populateTownSelect(datetimeTown, jurisdiction, {selectDefault: true});
    townApplied = applySelectedTown({
      townSelect: datetimeTown,
      latitudeInput: datetimeLatitude,
      longitudeInput: datetimeLongitude,
      elevationInput: datetimeElevation,
      zoneInput: datetimeGmtOffset,
      dateInput: datetimeDate,
      resetOffsetTouched: true
    });
  } else {
    applySelectedTown({
      townSelect: datetimeTown,
      zoneInput: datetimeGmtOffset,
      dateInput: datetimeDate,
      resetOffsetTouched: true
    });
  }
  try {
    const response = await fetch('/datetime-jurisdiction-location', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({jurisdiction, date})
    });
    const data = await response.json();
    if (!response.ok || !data.ok)
      return;
    if (updateCoordinates && !townApplied && datetimeLatitude && data.latitude)
      datetimeLatitude.value = String(data.latitude);
    if (updateCoordinates && !townApplied && datetimeLongitude && data.longitude)
      datetimeLongitude.value = String(data.longitude);
    if (datetimeGmtOffset) {
      const currentOffset = String(datetimeGmtOffset.value || '').trim();
      const suggestedOffset = String(data.gmt_offset || '').trim();
      if (!townApplied && (!datetimeGmtOffsetTouched || !currentOffset || currentOffset === datetimeAutoGmtOffset)) {
        datetimeGmtOffset.value = suggestedOffset;
        datetimeAutoGmtOffset = suggestedOffset;
        datetimeGmtOffsetTouched = false;
      }
    }
  } catch (_) {
    // Keep the current location if the helper is unavailable.
  }
}

async function refreshAlmanacJurisdictionLocation({updateCoordinates = true} = {}) {
  if (!almanacJurisdiction)
    return;

  const refreshId = ++almanacLocationRefreshSequence;
  const jurisdiction = validDatetimeJurisdiction(almanacJurisdiction.value, DEFAULT_DATETIME_JURISDICTION);
  const date = validDateText(almanacDate && almanacDate.value, DEFAULT_ALMANAC_DATE);
  let townApplied = false;
  if (updateCoordinates) {
    populateTownSelect(almanacTown, jurisdiction, {selectDefault: false});
    if (!selectTownByCoordinates(almanacTown, almanacLatitude && almanacLatitude.value, almanacLongitude && almanacLongitude.value))
      populateTownSelect(almanacTown, jurisdiction, {selectDefault: true});
    townApplied = applySelectedTown({
      townSelect: almanacTown,
      latitudeInput: almanacLatitude,
      longitudeInput: almanacLongitude,
      elevationInput: almanacElevation,
      zoneInput: almanacZone,
      dateInput: almanacDate
    });
  } else {
    applySelectedTown({
      townSelect: almanacTown,
      zoneInput: almanacZone,
      dateInput: almanacDate
    });
  }
  setStatus('Updating almanac location...');
  try {
    const response = await fetch('/datetime-jurisdiction-location', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({jurisdiction, date})
    });
    const data = await response.json();
    if (refreshId !== almanacLocationRefreshSequence || currentMode() !== 'almanac')
      return;
    if (!response.ok || !data.ok)
      return;
    if (updateCoordinates && !townApplied && almanacLatitude && data.latitude)
      almanacLatitude.value = String(data.latitude);
    if (updateCoordinates && !townApplied && almanacLongitude && data.longitude)
      almanacLongitude.value = String(data.longitude);
    if (!townApplied && almanacZone && data.gmt_offset)
      almanacZone.value = String(data.gmt_offset);
  } catch (_) {
    // Keep the current observer if the helper is unavailable.
  }
}

async function triggerDatetimeAutoEvaluation({refreshJurisdiction = false, refreshCoordinates = false} = {}) {
  if (currentMode() !== 'datetime')
    return;
  if (refreshJurisdiction)
    await refreshDatetimeJurisdictionLocation({updateCoordinates: refreshCoordinates});
  saveLastDatetimeState();
  await evaluateDatetime({skipHistoryUpdate: true});
  updateHistoryButtons();
}

async function triggerAlmanacAutoEvaluation({refreshJurisdiction = false, refreshCoordinates = false} = {}) {
  if (currentMode() !== 'almanac')
    return;
  almanacLastWorksheetData = null;
  if (refreshJurisdiction)
    await refreshAlmanacJurisdictionLocation({updateCoordinates: refreshCoordinates});
  saveLastAlmanacState();
  await evaluateAlmanac({skipHistoryUpdate: true});
  updateHistoryButtons();
}
