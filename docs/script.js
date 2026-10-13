// Intentional, lightweight interaction only. The site should feel like a product page, not a demo of effects.
document.querySelectorAll('a[href^="#"]').forEach((link) => {
  link.addEventListener('click', (event) => {
    const selector = link.getAttribute('href');
    const target = document.querySelector(selector);
    if (!target) return;
    event.preventDefault();
    target.scrollIntoView({ behavior: 'smooth', block: 'start' });
  });
});


// Scroll reveal + progress bar. Kept dependency-free for the static Railway site.
const revealItems = document.querySelectorAll('.reveal');
const progress = document.querySelector('.scroll-progress span');
const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

if (!reducedMotion && 'IntersectionObserver' in window) {
  const observer = new IntersectionObserver((entries) => {
    entries.forEach((entry) => {
      if (entry.isIntersecting) {
        entry.target.classList.add('is-visible');
        observer.unobserve(entry.target);
      }
    });
  }, { threshold: 0.12, rootMargin: '0px 0px -35px 0px' });
  revealItems.forEach((item, i) => {
    item.style.transitionDelay = `${Math.min((i % 5) * 55, 220)}ms`;
    observer.observe(item);
  });
} else {
  revealItems.forEach((item) => item.classList.add('is-visible'));
}

const updateProgress = () => {
  if (!progress) return;
  const scrollable = document.documentElement.scrollHeight - window.innerHeight;
  const amount = scrollable > 0 ? window.scrollY / scrollable : 0;
  progress.style.width = `${Math.min(1, Math.max(0, amount)) * 100}%`;
};
window.addEventListener('scroll', updateProgress, { passive: true });
updateProgress();


const FUMI_LANG = {
 en:{'nav.product':'Product','nav.hardware':'Hardware','nav.voice':'Voice','nav.build':'Build','nav.source':'Source','hero.eyebrow':'OPEN-SOURCE PHYSICAL AI','hero.title':'What if your AI<br><span>could live with you?</span>','hero.lead':'FUMI is a Tamagotchi-inspired AI companion built on an ESP32-S3 display board. It has a face, a body, a voice, needs, memory-like state, and a real place on your desk.','idea.label':'01 / THE IDEA','idea.title':"AI shouldn't only live in a browser.",'features.label':'02 / WHAT FUMI DOES','features.title':'Not just a chat box.','hardware.label':'03 / HARDWARE','hardware.title':'One board. A real device.','voice.label':'04 / VOICE','voice.title':'FUMI can actually talk.','demo.label':'05 / THE REAL THING','demo.title':'Built. Flashed. Talking.','models.label':'06 / BRING YOUR OWN BRAIN','models.title':"The device doesn't care which model you use.",'build.label':'07 / BUILD IT','build.title':'From board to companion.','bodies.label':'08 / ONE BOARD, MANY BODIES','bodies.title':"FUMI doesn't have to look like this.",'hardware.options':'HARDWARE OPTIONS','hardware.options.title':'One brain. Different builds.','face.label':'THE FACE','loop.label':'THE PHYSICAL LOOP','f1.t':'Animated character UI','f1.d':'Expressions, idle states and reactions','f2.t':'Touch interaction','f2.d':'Interact directly with the display','f3.t':'Life system','f3.d':'Hunger, energy, happiness, bond & growth','f4.t':'Mini-game','f4.d':'A small game loop inside the companion','f5.t':'Push-to-talk voice','f5.d':'Speak to FUMI through the onboard mic','f6.t':'Cloud AI chat','f6.d':'Provider-agnostic model backend'},
 ko:{'nav.product':'제품','nav.hardware':'하드웨어','nav.voice':'음성','nav.build':'빌드','nav.source':'소스','hero.eyebrow':'오픈소스 PHYSICAL AI','hero.title':'AI가<br><span>당신과 함께 산다면?</span>','hero.lead':'FUMI는 ESP32-S3 디스플레이 보드로 만든 타마고치 스타일의 AI 컴패니언입니다. 얼굴, 몸, 목소리, 상태와 책상 위의 실제 공간을 가집니다.','idea.label':'01 / 아이디어','idea.title':'AI는 브라우저 안에만 살 필요가 없습니다.','features.label':'02 / FUMI가 하는 일','features.title':'그냥 채팅창이 아닙니다.','hardware.label':'03 / 하드웨어','hardware.title':'하나의 보드. 진짜 디바이스.','voice.label':'04 / 음성','voice.title':'FUMI는 실제로 말할 수 있습니다.','demo.label':'05 / REAL THING','demo.title':'만들고. 플래시하고. 말합니다.','models.label':'06 / YOUR OWN BRAIN','models.title':'어떤 모델을 쓰는지는 중요하지 않습니다.','build.label':'07 / BUILD IT','build.title':'보드에서 컴패니언으로.','bodies.label':'08 / ONE BOARD, MANY BODIES','bodies.title':'FUMI는 꼭 이렇게 생길 필요가 없습니다.','hardware.options':'하드웨어 옵션','hardware.options.title':'하나의 브레인. 다양한 빌드.','face.label':'THE FACE','loop.label':'PHYSICAL LOOP','f1.t':'애니메이션 캐릭터 UI','f1.d':'표정, 대기 상태와 반응','f2.t':'터치 인터랙션','f2.d':'디스플레이를 직접 조작','f3.t':'라이프 시스템','f3.d':'배고픔, 에너지, 행복, 유대감과 성장','f4.t':'미니게임','f4.d':'컴패니언 안의 작은 게임 루프','f5.t':'푸시 투 토크 음성','f5.d':'내장 마이크로 FUMI와 대화','f6.t':'클라우드 AI 채팅','f6.d':'모델 제공자에 종속되지 않는 백엔드'},
 ja:{'nav.product':'製品','nav.hardware':'ハードウェア','nav.voice':'音声','nav.build':'ビルド','nav.source':'ソース','hero.eyebrow':'オープンソース PHYSICAL AI','hero.title':'AIが<br><span>あなたと一緒に暮らしたら？</span>','hero.lead':'FUMIはESP32-S3ディスプレイボードで作る、たまごっちに着想を得たAIコンパニオンです。顔、体、声、状態を持ち、机の上に実際に存在します。','idea.label':'01 / アイデア','idea.title':'AIはブラウザの中だけにいる必要はない。','features.label':'02 / FUMIができること','features.title':'ただのチャットボックスじゃない。','hardware.label':'03 / ハードウェア','hardware.title':'ひとつのボード。本物のデバイス。','voice.label':'04 / 音声','voice.title':'FUMIは実際に話せる。','demo.label':'05 / THE REAL THING','demo.title':'作った。書き込んだ。話した。','models.label':'06 / BRING YOUR OWN BRAIN','models.title':'どのモデルを使うかはデバイスには関係ない。','build.label':'07 / BUILD IT','build.title':'ボードからコンパニオンへ。','bodies.label':'08 / ONE BOARD, MANY BODIES','bodies.title':'FUMIはこの形である必要はない。','hardware.options':'ハードウェアオプション','hardware.options.title':'ひとつの脳。さまざまなビルド。','face.label':'THE FACE','loop.label':'THE PHYSICAL LOOP','f1.t':'アニメーションUI','f1.d':'表情、待機状態、リアクション','f2.t':'タッチ操作','f2.d':'ディスプレイを直接操作','f3.t':'ライフシステム','f3.d':'空腹、エネルギー、幸福、絆と成長','f4.t':'ミニゲーム','f4.d':'コンパニオン内の小さなゲームループ','f5.t':'プッシュ・トゥ・トーク','f5.d':'内蔵マイクでFUMIと話す','f6.t':'クラウドAIチャット','f6.d':'プロバイダーに依存しないバックエンド'},
 zh:{'nav.product':'产品','nav.hardware':'硬件','nav.voice':'语音','nav.build':'构建','nav.source':'源码','hero.eyebrow':'开源 PHYSICAL AI','hero.title':'如果你的 AI<br><span>可以和你一起生活？</span>','hero.lead':'FUMI 是一款基于 ESP32-S3 显示板打造的电子宠物风格 AI 伙伴。它拥有表情、身体、声音、状态，并真正存在于你的桌面上。','idea.label':'01 / 想法','idea.title':'AI 不应该只存在于浏览器里。','features.label':'02 / FUMI 能做什么','features.title':'不只是一个聊天框。','hardware.label':'03 / 硬件','hardware.title':'一块板。一个真实设备。','voice.label':'04 / 语音','voice.title':'FUMI 真的会说话。','demo.label':'05 / 真正运行','demo.title':'构建。刷写。开口说话。','models.label':'06 / 自选大脑','models.title':'设备不在乎你使用哪个模型。','build.label':'07 / 开始构建','build.title':'从开发板到 AI 伙伴。','bodies.label':'08 / ONE BOARD, MANY BODIES','bodies.title':'FUMI 不必长这样。','hardware.options':'硬件选项','hardware.options.title':'一个大脑。不同的形态。','face.label':'THE FACE','loop.label':'THE PHYSICAL LOOP','f1.t':'动画角色 UI','f1.d':'表情、待机状态和反应','f2.t':'触摸互动','f2.d':'直接与显示屏互动','f3.t':'生命系统','f3.d':'饥饿、能量、快乐、羁绊与成长','f4.t':'小游戏','f4.d':'伙伴内部的小型游戏循环','f5.t':'按键说话','f5.d':'通过板载麦克风与 FUMI 交流','f6.t':'云端 AI 聊天','f6.d':'与模型提供商解耦的后端'}
};
function setFumiLanguage(lang){const d=FUMI_LANG[lang]||FUMI_LANG.en;document.querySelectorAll('[data-i18n]').forEach(el=>{if(d[el.dataset.i18n]!==undefined)el.innerHTML=d[el.dataset.i18n]});document.documentElement.lang=lang==='ko'?'ko':lang==='ja'?'ja':lang==='zh'?'zh-CN':'en';localStorage.setItem('fumi-language',lang)}
const fumiLangSelect=document.getElementById('language-select');
if(fumiLangSelect){const saved=localStorage.getItem('fumi-language')||'en';fumiLangSelect.value=saved;setFumiLanguage(saved);fumiLangSelect.addEventListener('change',e=>setFumiLanguage(e.target.value))}
const fumiTheme=document.getElementById('theme-toggle');
if(fumiTheme){const saved=localStorage.getItem('fumi-theme')||'color';if(saved==='bw')document.body.classList.add('theme-bw');fumiTheme.textContent=saved==='bw'?'COLOR':'B/W';fumiTheme.addEventListener('click',()=>{const bw=document.body.classList.toggle('theme-bw');localStorage.setItem('fumi-theme',bw?'bw':'color');fumiTheme.textContent=bw?'COLOR':'B/W'})}
