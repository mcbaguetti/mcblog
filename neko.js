// Lightweight Neko: follows the cursor using /assets/black.gif
(function(){
  const src = '/assets/black.gif'
  const img = new Image()
  img.src = src
  img.style.position = 'fixed'
  img.style.left = '0px'
  img.style.top = '0px'
  img.style.pointerEvents = 'none'
  img.style.zIndex = 9999
  img.style.transformOrigin = '50% 50%'
  img.style.transition = 'transform 0.08s linear'
  img.style.opacity = '0'
  img.className = 'neko-js'

  let x = window.innerWidth/2, y = window.innerHeight/2
  let tx = x, ty = y
  let visible = false
  const speed = 0.18

  img.onload = () => {
    img.style.width = Math.max(32, Math.min(128, img.naturalWidth)) + 'px'
    img.style.height = 'auto'
    img.style.opacity = '1'
    document.body.appendChild(img)
    visible = true
    requestAnimationFrame(loop)
  }
  img.onerror = () => { console.warn('neko.js: failed to load', src) }

  function loop() {
    x += (tx - x) * speed
    y += (ty - y) * speed
    const dx = tx - x, dy = ty - y
    const angle = Math.atan2(dy, dx) * 180 / Math.PI
    img.style.transform = `translate(${x - img.width/2}px, ${y - img.height/2}px) rotate(${angle}deg)`
    requestAnimationFrame(loop)
  }

  let lastMove = 0
  window.addEventListener('mousemove', (e) => {
    tx = e.clientX
    ty = e.clientY
    lastMove = Date.now()
    if (!visible && img.complete) { document.body.appendChild(img); visible = true }
  }, { passive: true })

  // hide when idle for a while
  setInterval(() => {
    if (Date.now() - lastMove > 8000) {
      img.style.opacity = '0'
    } else {
      img.style.opacity = '1'
    }
  }, 1000)
})();
