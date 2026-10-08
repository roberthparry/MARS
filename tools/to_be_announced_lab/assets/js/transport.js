    window.addEventListener('message', (event) => {
      if (event.origin !== location.origin) return;
      const data = event && event.data;
      if (!data || data.kind !== 'to-be-announced-forecast-result') return;
      if (typeof window.__to_be_announcedApplyForecastResult === 'function') {
        window.__to_be_announcedApplyForecastResult(data);
      }
    });

    window.__to_be_announcedRunForecast = async function () {
      const basePath = JSON.parse(document.getElementById("tba-config").textContent).base_path;
      const form = document.getElementById('forecast-form');
      const status = document.getElementById('status');
      const button = document.getElementById('run-button');
      const summaryBox = document.getElementById('summary-box');
      const metricModel = document.getElementById('metric-model');
    const metricFitRows = document.getElementById('metric-fit-rows');
    const metricStationary = document.getElementById('metric-stationary');
    const metricInvertible = document.getElementById('metric-invertible');
    const forecastTableWrap = document.querySelector('.forecast-table-wrap');
    const forecastHead = document.getElementById('forecast-head');
      const forecastBody = document.getElementById('forecast-body');
      const downloadSummary = document.getElementById('download-summary');
      const downloadForecast = document.getElementById('download-forecast');
      if (!form) {
        if (status) {
          status.textContent = 'Forecast form not found.';
          status.className = 'status error';
        }
        return false;
      }
      if (typeof window.syncXregColumnsFromChecks === 'function') window.syncXregColumnsFromChecks();
      if (typeof window.syncOutlierDatesFromChecks === 'function') window.syncOutlierDatesFromChecks();
      const payload = {};
      for (const element of form.elements) {
        if (!element.name) continue;
        payload[element.name] = element.value;
      }
      if (button) button.disabled = true;
      if (status) {
        status.textContent = 'Submitting forecast...';
        status.className = 'status';
      }
      fetch(`${basePath}/state`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      }).catch(() => {});
      try {
        const response = await fetch(`${basePath}/forecast`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload),
        });
        if (status) status.textContent = 'Reading forecast result...';
        const data = await response.json();
        if (!response.ok || !data.ok) {
          throw new Error(data.error || 'Forecast failed');
        }
        if (typeof window.__to_be_announcedApplyForecastResult === 'function') {
          window.__to_be_announcedApplyForecastResult(data);
        }
        return true;
      } catch (error) {
        if (status) {
          status.textContent = error.message || String(error);
          status.className = 'status error';
        }
        return false;
      } finally {
        if (button) button.disabled = false;
      }
    };
