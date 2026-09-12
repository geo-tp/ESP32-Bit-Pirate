// Uses the modules gallery's on-demand image loading and keyboard navigation.
(() => {
  const gallery = document.querySelector('.firmware-gallery');
  if (!gallery) return;
  const items = [...gallery.querySelectorAll('[data-gallery-image]')];
  const box = gallery.querySelector('dialog');
  const image = box.querySelector('[data-gallery-full]');
  let active = 0;
  let trigger;
  let previousOverflow;

  function show(index) {
    active = (index + items.length) % items.length;
    const item = items[active];
    image.src = item.dataset.galleryImage;
    image.alt = item.querySelector('img').alt;
    box.querySelector('#firmware-lightbox-title').textContent = item.dataset.galleryTitle;
    box.querySelector('#firmware-lightbox-caption').textContent = `${active + 1} / ${items.length} — ${item.dataset.galleryCaption}`;
  }

  items.forEach((item, index) => item.addEventListener('click', () => {
    trigger = item;
    previousOverflow = document.documentElement.style.overflow;
    show(index);
    box.showModal();
    document.documentElement.style.overflow = 'hidden';
  }));
  box.querySelector('[data-gallery-close]').addEventListener('click', () => box.close());
  box.querySelector('[data-gallery-prev]').addEventListener('click', () => show(active - 1));
  box.querySelector('[data-gallery-next]').addEventListener('click', () => show(active + 1));
  box.addEventListener('click', event => {
    if (event.target === box || event.target.classList.contains('firmware-lightbox-stage')) box.close();
  });
  box.addEventListener('keydown', event => {
    if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') {
      event.preventDefault();
      show(active + (event.key === 'ArrowLeft' ? -1 : 1));
    }
  });
  box.addEventListener('close', () => {
    image.removeAttribute('src');
    document.documentElement.style.overflow = previousOverflow;
    trigger?.focus();
  });
})();
