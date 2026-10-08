/**
 * MARS Lab startup. Definition-only scripts load first; worksheet state and event
 * wiring load after the catalogue. Ordered classic scripts deliberately retain
 * the existing shared lexical state without a bundler or mathematical rewriting.
 */
let labConfig;
let labCatalogue;
const labDefinitionScripts = [
  'almanac', 'api', 'bindings', 'calculus', 'controls', 'date_picker', 'editor',
  'evaluation', 'integrator', 'locations', 'mobile', 'result_layout', 'result_text',
  'results', 'state', 'workspace'
];

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
  labConfig = JSON.parse(document.getElementById('lab-config').textContent);
  const response = await fetch('/jurisdictions');
  if (!response.ok)
    throw new Error(`Could not load the Lab catalogue (${response.status})`);
  labCatalogue = await response.json();
  if (!Array.isArray(labCatalogue.options) || !labCatalogue.towns)
    throw new Error('The Lab catalogue is invalid');
  for (const id of ['datetimeJurisdiction', 'almanacJurisdiction']) {
    const select = document.getElementById(id);
    select.replaceChildren(...labCatalogue.options.map(([code, label]) => new Option(label, code)));
    select.disabled = !labCatalogue.available;
    if (!labCatalogue.available) {
      const notice = document.createElement('p');
      notice.className = 'mode-hint';
      notice.dataset.jurisdictionNotice = 'true';
      notice.textContent = labCatalogue.error || 'Jurisdiction database unavailable';
      select.closest('.mode-panel').prepend(notice);
    }
  }
  await Promise.all(labDefinitionScripts.map(name => labLoadScript(`/js/${name}.js`)));
  await labLoadScript('/js/worksheet.js');
  await labLoadScript('/js/events.js');
  await window.labInitialEvaluation;
}

window.labReady = labStart().catch((error) => {
  document.getElementById('status').textContent = 'Startup failed';
  document.getElementById('rendered').textContent = error.message;
  document.getElementById('run').disabled = true;
  throw error;
});
