const face = document.querySelector('.face-eyes');
const faces = ['•ᴗ•','•_•','◕ᴗ◕','-ᴗ-','•ᴗ•'];
let i = 0;
setInterval(() => {
  if (!face) return;
  i = (i + 1) % faces.length;
  face.textContent = faces[i];
}, 2400);

const ticker = document.querySelector('.ticker');
if (ticker) {
  let x = 0;
  function drift() {
    x -= 0.25;
    if (x < -300) x = 0;
    ticker.style.transform = `translateX(${x}px)`;
    requestAnimationFrame(drift);
  }
  drift();
}
