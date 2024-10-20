
(function() {
  const postId = '65f4ee31f851c380fef1b585';
  const bannerUrl = 'https://app.seobotai.com/banner/inline/';
  const popupUrl = 'https://app.seobotai.com/banner/popup/';
  window.addEventListener('message', function(event) {
    if (event.origin === new URL(bannerUrl).origin) {
      const { id, height } = event.data;
      const iframe = document.getElementById(id);
      if (iframe) {
        iframe.style.height = height + 'px';
        iframe.style.marginLeft = 'auto';
        iframe.style.marginRight = 'auto';
      }
    }
  }, false);

  function handleInline(banner) {
    const id = banner.id || banner.innerText;
    const skeleton = document.createElement('div');
    skeleton.id = 'skeleton-' + id;
    skeleton.style.width = '100%';
    skeleton.style.height = '400px';
    skeleton.style.backgroundColor = '#eee';
    skeleton.style.borderRadius = '1em';
    skeleton.style.marginTop = '1em';
    skeleton.style.marginBottom = '1em';
    skeleton.style.marginLeft = 'auto';
    skeleton.style.marginRight = 'auto';

    banner.parentNode.replaceChild(skeleton, banner);

    const iframe = document.createElement('iframe');
    iframe.id = id;
    iframe.style.display = 'block';
    iframe.style.border = 'none';
    iframe.style.width = '100%';
    iframe.style.height = '0';
    iframe.style.marginTop = '1em';
    iframe.style.marginBottom = '1em';
    iframe.style.marginLeft = 'auto';
    iframe.style.marginRight = 'auto';
    iframe.src = bannerUrl + '?id=' + id;
    iframe.onload = function() {
      const skeletonPlaceholder = document.getElementById('skeleton-' + id);
      if (skeletonPlaceholder) {
        skeletonPlaceholder.parentNode.replaceChild(iframe, skeletonPlaceholder);
      }
    };

    skeleton.parentNode.insertBefore(iframe, skeleton.nextSibling);
  }

  async function handlePopup() {
    try {
      const res = await fetch(popupUrl + '?postId=' + postId);
      const result = await res.json();
      if (!result?.data?.success) return;
      const data = result.data;

      const container = document.createElement('div');
      container.innerHTML = data.html;
      document.body.appendChild(container);

      const style = document.createElement('style');
      style.type = 'text/css';
      style.appendChild(document.createTextNode(data.styles));
      document.head.appendChild(style);

      const script = document.createElement('script');
      script.type = 'text/javascript';
      script.textContent = data.script;
      document.body.appendChild(script);
    } catch {}
  }

  const divs = Array.from(document.querySelectorAll('div.sb-banner'));
  const h6 = Array.from(document.querySelectorAll('h6')).filter(el => el.textContent.trim().startsWith('sbb-itb-'));
  const banners = [...divs, ...h6];

  const seenBaseIds = new Set();
  banners.forEach(banner => {
    const id = banner.id || banner.innerText;
    
    const baseIdMatch = id.match(/^(sbb-(itb|ptb|unk)-[a-zA-Z0-9]+)(-d+)?/);
    const baseId = baseIdMatch ? baseIdMatch[1] : null;

    if (baseId && seenBaseIds.has(baseId)) {
      banner.remove();
      return;
    }

    if (baseId) {
      seenBaseIds.add(baseId);
    }

    if (id.startsWith('sbb-itb-')) handleInline(banner);
    handlePopup();
  });
})();
