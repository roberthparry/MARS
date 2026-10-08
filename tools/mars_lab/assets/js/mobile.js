/**
 * Refreshing private-network access details and the native QR image.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

async function refreshMobileAccess() {
  if (!mobileAccess || !mobileUrl || !mobileQr)
    return;

  try {
    const headers = controlToken ? {'X-Dval-Lab-Control': controlToken} : {};
    const response = await fetch('/mobile-access', {cache: 'no-store', headers});
    if (!response.ok)
      return;
    const data = await response.json();
    const url = String(data.url || '');
    const canControl = Boolean(data.control);
    mobileAccess.classList.remove('hidden');
    if (mobileTitle)
      mobileTitle.textContent = String(data.title || 'Mobile access');
    if (mobileHint)
      mobileHint.textContent = String(data.hint || '');
    mobileUrl.textContent = url || 'Unavailable';
    mobileQr.innerHTML = String(data.qr || '');
  } catch (err) {
    // Network state changes are expected; keep the last known QR until the next poll.
  }
}
