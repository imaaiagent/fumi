document.querySelectorAll('a[href^="#"]').forEach(a=>{
  a.addEventListener('click',e=>{
    const el=document.querySelector(a.getAttribute('href'));
    if(el){e.preventDefault();el.scrollIntoView({behavior:'smooth'});}
  });
});
const observer=new IntersectionObserver(entries=>{
  entries.forEach(e=>{if(e.isIntersecting)e.target.classList.add('in');});
},{threshold:.08});
document.querySelectorAll('.section,.hero-copy,.hero-art').forEach(el=>observer.observe(el));
