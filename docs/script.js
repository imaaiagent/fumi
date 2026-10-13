const face = document.querySelector('#heroFace');
const eyes = face ? [...face.querySelectorAll('.eye span')] : [];
const maxX = 17, maxY = 12;
function moveEyes(x,y){
  if(!face) return;
  const r=face.getBoundingClientRect();
  const nx=Math.max(-1,Math.min(1,(x-(r.left+r.width/2))/(r.width/2)));
  const ny=Math.max(-1,Math.min(1,(y-(r.top+r.height/2))/(r.height/2)));
  eyes.forEach(e=>e.style.transform=`translate3d(${nx*maxX}px,${ny*maxY}px,0)`);
}
document.addEventListener('mousemove',e=>moveEyes(e.clientX,e.clientY),{passive:true});
face?.addEventListener('mouseleave',()=>eyes.forEach(e=>e.style.transform='translate3d(0,0,0)'));
const smallEyes=[...document.querySelectorAll('.open-face-wrap .eye span')];
const openFace=document.querySelector('.open-face-wrap');
openFace?.addEventListener('mousemove',e=>{
 const r=openFace.getBoundingClientRect(), nx=Math.max(-1,Math.min(1,(e.clientX-(r.left+r.width/2))/(r.width/2))), ny=Math.max(-1,Math.min(1,(e.clientY-(r.top+r.height/2))/(r.height/2)));
 smallEyes.forEach(el=>el.style.transform=`translate3d(${nx*9}px,${ny*7}px,0)`);
});
