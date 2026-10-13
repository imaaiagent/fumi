const face=document.querySelector('.big-face');
const heroFace=document.querySelector('.speech');
const moods=['•ᴗ•','◕ᴗ◕','•_•','-ᴗ-'];let n=0;
setInterval(()=>{n=(n+1)%moods.length;if(face)face.textContent=moods[n]},2600);
const io=new IntersectionObserver(es=>es.forEach(e=>{if(e.isIntersecting)e.target.classList.add('seen')}),{threshold:.12});
document.querySelectorAll('section').forEach(s=>io.observe(s));
