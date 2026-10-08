/**
 * Searchable selection controls and accessible button tooltips.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function buttonCardName(button) {
  const card = button.closest('.result-card');
  const title = card ? card.querySelector('.card-title > span:first-child') : null;
  return String(title && title.textContent || 'result').trim().toLowerCase();
}

function buttonTooltipText(button) {
  const title = String(button.getAttribute('title') || '').trim();
  if (button.hasAttribute('title')) {
    if (title)
      button.dataset.marsTooltipTitle = title;
    else
      delete button.dataset.marsTooltipTitle;
  }
  if (title)
    return title;
  const savedTitle = String(button.dataset.marsTooltipTitle || '').trim();
  if (savedTitle)
    return savedTitle;
  if (BUTTON_TOOLTIP_TEXT[button.id])
    return BUTTON_TOOLTIP_TEXT[button.id];
  if (button.classList.contains('mode-tab'))
    return `Switch to ${String(button.textContent || '').trim()} mode`;
  if (button.matches('[data-expand-card]'))
    return `${button.textContent.trim()} the ${buttonCardName(button)} card`;
  if (button.classList.contains('more-digits'))
    return `Show the full value in the ${buttonCardName(button)} card`;
  if (button.dataset.copyTarget === 'mobile')
    return 'Copy the private mobile-access URL';
  if (button.classList.contains('copy-result'))
    return `Copy the ${buttonCardName(button)}`;
  if (button.classList.contains('variable-copy'))
    return 'Copy this binding value';
  if (button.classList.contains('select-button'))
    return `Choose ${String(button.textContent || 'an option').trim()}`;

  const ariaLabel = String(button.getAttribute('aria-label') || '').trim();
  if (ariaLabel)
    return ariaLabel;
  const text = String(button.textContent || '').replace(/\s+/g, ' ').trim();
  return text || 'Activate this control';
}

function positionButtonTooltip(button) {
  const buttonRect = button.getBoundingClientRect();
  const tooltipRect = buttonTooltip.getBoundingClientRect();
  const gap = 8;
  const viewportMargin = 8;
  const centredLeft = buttonRect.left + (buttonRect.width - tooltipRect.width) / 2;
  const left = Math.max(
    viewportMargin,
    Math.min(window.innerWidth - tooltipRect.width - viewportMargin, centredLeft)
  );
  const below = buttonRect.bottom + gap;
  const top = below + tooltipRect.height <= window.innerHeight - viewportMargin
    ? below
    : Math.max(viewportMargin, buttonRect.top - tooltipRect.height - gap);
  buttonTooltip.style.left = `${left}px`;
  buttonTooltip.style.top = `${top}px`;
}

function showButtonTooltip(button) {
  if (!button || button.closest('.hidden'))
    return;
  if (activeTooltipButton && activeTooltipButton !== button)
    hideButtonTooltip();

  const text = buttonTooltipText(button);
  if (!text)
    return;
  if (button.hasAttribute('title'))
    button.removeAttribute('title');
  activeTooltipButton = button;
  activeTooltipDescribedBy = button.getAttribute('aria-describedby');
  const descriptions = new Set(String(activeTooltipDescribedBy || '').split(/\s+/).filter(Boolean));
  descriptions.add(buttonTooltip.id);
  button.setAttribute('aria-describedby', Array.from(descriptions).join(' '));
  buttonTooltip.textContent = text;
  positionButtonTooltip(button);
  buttonTooltip.classList.add('visible');
}

function hideButtonTooltip() {
  if (activeTooltipButton) {
    if (activeTooltipDescribedBy)
      activeTooltipButton.setAttribute('aria-describedby', activeTooltipDescribedBy);
    else
      activeTooltipButton.removeAttribute('aria-describedby');
  }
  activeTooltipButton = null;
  activeTooltipDescribedBy = null;
  buttonTooltip.classList.remove('visible');
}

function eventButton(target) {
  return target && typeof target.closest === 'function' ? target.closest('button') : null;
}

function setSelectValue(select, value) {
  if (!select)
    return;
  select.value = value;
  syncRoundedSelect(select);
}

function normaliseSelectSearchText(value) {
  return String(value || '')
    .normalize('NFD')
    .replace(/[\u0300-\u036f]/g, '')
    .toLowerCase();
}

function enhanceRoundedSelect(select, options = {}) {
  if (!select)
    return null;

  const label = document.querySelector(`label[for="${select.id}"]`);
  const shell = document.createElement('div');
  const button = document.createElement('button');
  const menu = document.createElement('div');
  const searchable = options && options.searchable;
  const searchInput = searchable ? document.createElement('input') : null;
  const optionsWrap = document.createElement('div');
  const emptyState = document.createElement('div');
  const optionRenderer = options && typeof options.renderOption === 'function' ? options.renderOption : null;
  const placeholder = String(options && options.placeholder || 'Select option');

  shell.className = 'select-shell';
  button.type = 'button';
  button.className = 'select-button';
  button.setAttribute('aria-haspopup', 'listbox');
  button.setAttribute('aria-expanded', 'false');
  if (label && label.id)
    button.setAttribute('aria-labelledby', label.id);
  else
    button.setAttribute('aria-label', 'Select option');

  menu.className = 'select-menu hidden';
  menu.setAttribute('role', 'listbox');
  optionsWrap.className = 'select-options';
  emptyState.className = 'select-empty';
  emptyState.textContent = (options && options.emptyText) || 'No matches';

  if (searchInput) {
    searchInput.type = 'search';
    searchInput.className = 'select-search';
    searchInput.placeholder = (options && options.searchPlaceholder) || 'Search';
    searchInput.setAttribute('aria-label', searchInput.placeholder);
    searchInput.autocomplete = 'off';
    searchInput.spellcheck = false;
    menu.appendChild(searchInput);
  }

  select.classList.add('select-native-source');
  select.tabIndex = -1;
  select.setAttribute('aria-hidden', 'true');
  select.parentNode.insertBefore(shell, select);
  shell.appendChild(select);
  shell.appendChild(button);
  shell.appendChild(menu);
  menu.appendChild(optionsWrap);
  menu.appendChild(emptyState);

  let optionButtons = [];

  function optionParts(option) {
    if (!optionRenderer)
      return null;
    return optionRenderer(option) || null;
  }

  function renderOptionContent(target, parts, classPrefix) {
    target.textContent = '';
    if (!parts || !parts.detail) {
      target.textContent = parts && parts.label ? parts.label : '';
      return;
    }
    const labelSpan = document.createElement('span');
    const detailSpan = document.createElement('span');
    labelSpan.className = `${classPrefix}-label`;
    detailSpan.className = `${classPrefix}-detail`;
    labelSpan.textContent = parts.label || '';
    detailSpan.textContent = parts.detail || '';
    target.appendChild(labelSpan);
    target.appendChild(detailSpan);
  }

  function rebuildOptionButtons() {
    optionsWrap.textContent = '';
    optionButtons = Array.from(select.options).map((option) => {
      const parts = optionParts(option);
      const item = document.createElement('button');
      item.type = 'button';
      item.className = parts && parts.detail ? 'select-option two-column' : 'select-option';
      item.setAttribute('role', 'option');
      item.dataset.value = option.value;
      item.dataset.searchText = [option.textContent, option.dataset.latitude, option.dataset.longitude, parts && parts.detail]
        .filter(Boolean)
        .join(' ');
      renderOptionContent(item, parts || {label: option.textContent}, 'select-option');
      item.addEventListener('click', () => {
        const changed = select.value !== option.value;
        select.value = option.value;
        sync();
        close();
        button.focus();
        if (changed)
          select.dispatchEvent(new Event('change', {bubbles: true}));
      });
      optionsWrap.appendChild(item);
      return item;
    });
  }

  rebuildOptionButtons();

  function visibleOptionButtons() {
    return optionButtons.filter((item) => !item.classList.contains('hidden'));
  }

  function selectedOption() {
    return select.selectedOptions[0] || select.options[select.selectedIndex] || null;
  }

  function filterOptions() {
    const query = normaliseSelectSearchText(searchInput && searchInput.value).trim();
    let visibleCount = 0;

    optionButtons.forEach((item) => {
      const haystack = normaliseSelectSearchText(
        `${item.dataset.searchText || item.textContent || ''} ${item.dataset.value || ''}`
      );
      const visible = !query || haystack.includes(query);
      item.classList.toggle('hidden', !visible);
      if (visible)
        visibleCount += 1;
    });
    emptyState.classList.toggle('visible', visibleCount === 0);
  }

  function selectedOptionButton() {
    return optionButtons.find((item) =>
      item.dataset.value === select.value && !item.classList.contains('hidden')
    );
  }

  function scrollSelectedOptionIntoView() {
    const selected = selectedOptionButton();
    if (!selected)
      return;
    selected.scrollIntoView({block: 'nearest', inline: 'nearest'});
  }

  function sync() {
    const selected = selectedOption();
    const parts = selected ? optionParts(selected) : null;
    button.textContent = parts && parts.label
      ? parts.label
      : (selected ? selected.textContent : placeholder);
    optionButtons.forEach((item) => {
      const selectedItem = item.dataset.value === select.value;
      item.classList.toggle('selected', selectedItem);
      item.setAttribute('aria-selected', selectedItem ? 'true' : 'false');
    });
    filterOptions();
  }

  function close() {
    shell.classList.remove('open');
    button.setAttribute('aria-expanded', 'false');
    menu.classList.add('hidden');
  }

  function open() {
    if (searchInput)
      searchInput.value = '';
    sync();
    shell.classList.add('open');
    button.setAttribute('aria-expanded', 'true');
    menu.classList.remove('hidden');
    requestAnimationFrame(scrollSelectedOptionIntoView);
  }

  function focusSelectedOption() {
    const visible = visibleOptionButtons();
    const selected = selectedOptionButton();
    (selected || visible[0] || searchInput || button).focus();
  }

  function focusRelativeOption(step) {
    const visible = visibleOptionButtons();
    if (!visible.length)
      return;
    const currentIndex = visible.indexOf(document.activeElement);
    const selectedIndex = visible.findIndex((item) => item.dataset.value === select.value);
    const index = currentIndex >= 0 ? currentIndex : Math.max(0, selectedIndex);
    const nextIndex = (index + step + visible.length) % visible.length;
    visible[nextIndex].focus();
  }

  button.addEventListener('click', () => {
    if (shell.classList.contains('open'))
      close();
    else
      open();
  });

  button.addEventListener('keydown', (event) => {
    if (event.key === 'ArrowDown' || event.key === 'Enter' || event.key === ' ') {
      event.preventDefault();
      open();
      if (searchInput)
        searchInput.focus();
      else
        focusSelectedOption();
    } else if (event.key === 'Escape') {
      close();
    }
  });

  if (searchInput) {
    searchInput.addEventListener('input', () => {
      filterOptions();
      scrollSelectedOptionIntoView();
    });
    searchInput.addEventListener('keydown', (event) => {
      if (event.key === 'Escape') {
        event.preventDefault();
        close();
        button.focus();
      } else if (event.key === 'ArrowDown') {
        event.preventDefault();
        focusSelectedOption();
      }
    });
  }

  menu.addEventListener('keydown', (event) => {
    if (event.key === 'Escape') {
      event.preventDefault();
      close();
      button.focus();
    } else if (event.key === 'ArrowDown') {
      event.preventDefault();
      focusRelativeOption(1);
    } else if (event.key === 'ArrowUp') {
      event.preventDefault();
      focusRelativeOption(-1);
    }
  });

  if (label)
    label.addEventListener('click', (event) => {
      event.preventDefault();
      button.focus();
    });

  document.addEventListener('click', (event) => {
    if (!shell.contains(event.target))
      close();
  });

  select.addEventListener('change', sync);
  select.__marsSyncRoundedSelect = sync;
  select.__marsRebuildRoundedSelect = () => {
    rebuildOptionButtons();
    sync();
  };
  sync();
  return {sync, close};
}
