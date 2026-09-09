'use strict';
const W=800,H=520,$=id=>document.getElementById(id);
const canvas=$('render'),ctx=canvas.getContext('2d',{willReadFrequently:true}),overlay=$('overlay'),oc=overlay.getContext('2d');
const source=document.createElement('canvas');source.width=W;source.height=H;
const sc=source.getContext('2d',{willReadFrequently:true});
let pixels,points,field,curve=[],curveLength=0,scheduled=false,dirty=true,comparison=false,drag=-1,reverse=false;
const presets={arc:[[365,220],[460,190],[540,350],[700,310]],s:[[320,200],[580,100],[400,420],[710,330]],line:[[330,235],[450,235],[570,235],[710,235]]};
function defaultSource(){
 sc.clearRect(0,0,W,H);sc.lineJoin='round';sc.lineCap='round';
 sc.strokeStyle='#313f50';sc.lineWidth=40;sc.beginPath();sc.moveTo(325,340);sc.lineTo(290,443);sc.moveTo(365,340);sc.lineTo(407,443);sc.stroke();
 sc.fillStyle='#eee3bd';sc.strokeStyle='#4a3742';sc.lineWidth=5;
 sc.beginPath();sc.moveTo(300,195);sc.lineTo(370,195);sc.lineTo(394,351);sc.lineTo(286,351);sc.closePath();sc.fill();sc.stroke();
 sc.fillStyle='#c66950';sc.fillRect(300,268,75,35);
 sc.strokeStyle='#4a3742';sc.lineWidth=47;sc.beginPath();sc.moveTo(302,220);sc.lineTo(251,285);sc.stroke();sc.strokeStyle='#f8d47a';sc.lineWidth=37;sc.stroke();
 sc.strokeStyle='#4a3742';sc.lineWidth=51;sc.beginPath();sc.moveTo(370,220);sc.lineTo(470,228);sc.stroke();sc.strokeStyle='#f8d47a';sc.lineWidth=41;sc.stroke();
 sc.fillStyle='#f8d47a';sc.strokeStyle='#4a3742';sc.lineWidth=4;sc.beginPath();sc.moveTo(459,202);sc.lineTo(490,198);sc.lineTo(507,213);sc.lineTo(503,246);sc.lineTo(470,252);sc.closePath();sc.fill();sc.stroke();
 sc.strokeStyle='#99523f';sc.lineWidth=3;for(let x=471;x<494;x+=7){sc.beginPath();sc.moveTo(x,208);sc.lineTo(x+1,222);sc.stroke();}
 sc.fillStyle='#f8d47a';sc.strokeStyle='#4a3742';sc.lineWidth=5;sc.beginPath();sc.arc(333,144,43,0,Math.PI*2);sc.fill();sc.stroke();
 sc.fillStyle='#4a3742';sc.beginPath();sc.arc(347,139,4,0,Math.PI*2);sc.fill();sc.beginPath();sc.moveTo(307,112);sc.lineTo(340,93);sc.lineTo(375,120);sc.lineTo(330,116);sc.closePath();sc.fill();
 sc.fillStyle='#86b9bc';sc.fillRect(105,113,51,51);sc.fillStyle='#c66950';sc.beginPath();sc.arc(648,435,23,0,Math.PI*2);sc.fill();updateSource();
}
function updateSource(){pixels=sc.getImageData(0,0,W,H).data;$('sourcePreview').src=source.toDataURL();request();}
function bezier(t){const u=1-t;return [0,1].map(k=>u*u*u*points[0][k]+3*u*u*t*points[1][k]+3*u*t*t*points[2][k]+t*t*t*points[3][k]);}
function buildField(){
 const segments=[];let prev=bezier(0),s=0;
 for(let k=1;k<=100;k++){const p=bezier(k/100),dx=p[0]-prev[0],dy=p[1]-prev[1],len=Math.hypot(dx,dy);if(len>1e-5){segments.push({x:prev[0],y:prev[1],dx,dy,len,s});s+=len;}prev=p;}
 curveLength=s;curve=segments;
 const along=new Float32Array(W*H),normal=new Float32Array(W*H),distance=new Float32Array(W*H);
 for(let y=0,i=0;y<H;y++)for(let x=0;x<W;x++,i++){
  let best=Infinity,bs=0,bn=0;
  for(const g of segments){const rx=x-g.x,ry=y-g.y,t=Math.max(0,Math.min(1,(rx*g.dx+ry*g.dy)/(g.len*g.len))),ex=rx-t*g.dx,ey=ry-t*g.dy,d=ex*ex+ey*ey;
   if(d<best){best=d;bs=g.s+t*g.len;bn=(g.dx*ry-g.dy*rx)/g.len;}}
  along[i]=bs;normal[i]=bn;distance[i]=Math.sqrt(best);
 }field={along,normal,distance};dirty=false;
}
function frame(s,n){
 if(!curve.length)return [0,0];let lo=0,hi=curve.length-1;
 while(lo<hi){const m=(lo+hi)>>1;if(curve[m].s+curve[m].len<s)lo=m+1;else hi=m;}
 const g=curve[lo],t=(s-g.s)/g.len;return [g.x+t*g.dx-n*g.dy/g.len,g.y+t*g.dy+n*g.dx/g.len];
}
function noise(v){return .5+.24*Math.sin(v*1.17+1.4)+.16*Math.sin(v*2.71+.3)+.10*Math.sin(v*5.39+2.1);}
function smooth(v){v=Math.max(0,Math.min(1,v));return v*v*(3-2*v);}
function maskAt(d,r,f){return d>=r?0:f===0?1:smooth((r-d)/(r*f));}
// Bilinear interpolation and original-image blending use premultiplied alpha.
function sample(x,y,out,i,mix){
 const ix=Math.floor(x),iy=Math.floor(y),fx=x-ix,fy=y-iy;let a=0,r=0,g=0,b=0;
 for(let yy=0;yy<2;yy++)for(let xx=0;xx<2;xx++){const px=ix+xx,py=iy+yy;if(px<0||py<0||px>=W||py>=H)continue;const j=(py*W+px)*4,w=(xx?fx:1-fx)*(yy?fy:1-fy),aw=pixels[j+3]*w;a+=aw;r+=pixels[j]*aw;g+=pixels[j+1]*aw;b+=pixels[j+2]*aw;}
 const oa=pixels[i+3]*mix;a=a*(1-mix)+oa;r=r*(1-mix)+pixels[i]*oa;g=g*(1-mix)+pixels[i+1]*oa;b=b*(1-mix)+pixels[i+2]*oa;
 out[i]=a?r/a:0;out[i+1]=a?g/a:0;out[i+2]=a?b/a:0;out[i+3]=a;
}
function render(){
 const start=performance.now();if(dirty)buildField();
 const amount=+$('length').value,radius=+$('radius').value,feather=+$('feather').value/100,strength=+$('streak').value/100,freq=+$('frequency').value,mix=+$('original').value/100;
 for(const id of ['length','radius','feather','streak','frequency','original'])$(id+'Value').value=$(id).value+(['length','radius'].includes(id)?' px':id==='frequency'?'':'%');
 const img=new ImageData(new Uint8ClampedArray(pixels),W,H),out=img.data;
 if(!comparison&&amount>0&&curveLength>0&&mix<1)for(let i=0;i<W*H;i++){
  const mask=maskAt(field.distance[i],radius,feather);if(!mask)continue;
  const s=field.along[i],n=field.normal[i],grain=noise(n/Math.max(20,radius)*freq),travel=reverse?curveLength-s:s;
  // Compress upstream curve coordinates to stretch the fixed source downstream.
  // A frame delta (rather than absolute placement) preserves identity at zero influence.
  const shift=travel*amount/(curveLength+amount)*mask*(1-strength*.85*grain),ss=s+(reverse?shift:-shift);
  const a=frame(s,n),b=frame(ss,n),x=i%W,y=Math.floor(i/W);
  sample(x+b[0]-a[0],y+b[1]-a[1],out,i*4,mix);
 }
 ctx.putImageData(img,0,0);drawGuide();$('status').textContent=`${comparison?'元画像':'局所 Smear'} · ${Math.round(performance.now()-start)} ms · 800 × 520`;
}
function drawGuide(){
 oc.clearRect(0,0,W,H);
 if($('region').checked&&field){const img=oc.createImageData(W,H),r=+$('radius').value,f=+$('feather').value/100;for(let i=0;i<W*H;i++){const m=maskAt(field.distance[i],r,f);img.data[i*4]=157;img.data[i*4+1]=211;img.data[i*4+2]=245;img.data[i*4+3]=m*35;}oc.putImageData(img,0,0);}
 if(!$('guide').checked)return;
 oc.lineWidth=1;oc.strokeStyle='#c6ed9380';oc.setLineDash([4,5]);oc.beginPath();oc.moveTo(...points[0]);oc.lineTo(...points[1]);oc.moveTo(...points[2]);oc.lineTo(...points[3]);oc.stroke();oc.setLineDash([]);oc.strokeStyle='#d3f8a9';oc.lineWidth=2;oc.beginPath();oc.moveTo(...points[0]);oc.bezierCurveTo(...points[1],...points[2],...points[3]);oc.stroke();
 points.forEach((p,i)=>{oc.beginPath();oc.arc(...p,i===0||i===3?7:5,0,Math.PI*2);oc.fillStyle=i===0||i===3?'#d3f8a9':'#263139';oc.fill();oc.stroke();oc.fillStyle='#edf5e7';oc.font='12px system-ui';oc.fillText(['始点','ハンドル','ハンドル','終点'][i],p[0]+12,p[1]-12);});
 for(const t of [.25,.5,.75]){const p=bezier(t),q=bezier(t+.005),angle=Math.atan2(q[1]-p[1],q[0]-p[0])+(reverse?Math.PI:0);oc.save();oc.translate(...p);oc.rotate(angle);oc.beginPath();oc.moveTo(-7,-4);oc.lineTo(0,0);oc.lineTo(-7,4);oc.stroke();oc.restore();}
}
function request(){if(!scheduled){scheduled=true;requestAnimationFrame(()=>{scheduled=false;render();});}}
function preset(name){points=presets[name].map(p=>p.slice());dirty=true;document.querySelectorAll('[data-preset]').forEach(b=>b.classList.toggle('active',b.dataset.preset===name));request();}
document.querySelectorAll('[data-preset]').forEach(b=>b.onclick=()=>preset(b.dataset.preset));
for(const id of ['length','radius','feather','streak','frequency','original'])$(id).oninput=request;
$('guide').onchange=$('region').onchange=drawGuide;
$('reverse').onclick=()=>{reverse=!reverse;$('reverse').classList.toggle('active',reverse);request();};
$('compare').onclick=()=>{comparison=!comparison;$('compare').classList.toggle('active',comparison);$('compare').textContent=comparison?'Smearに戻す':'元画像と比較';request();};
$('save').onclick=()=>{render();const a=document.createElement('a');a.download='curve-smear.png';a.href=canvas.toDataURL();a.click();};
$('resetSource').onclick=defaultSource;
$('file').onchange=()=>{const file=$('file').files[0];if(!file)return;const url=URL.createObjectURL(file),img=new Image();img.onload=()=>{sc.clearRect(0,0,W,H);const scale=Math.min(W/img.width,H/img.height);sc.drawImage(img,(W-img.width*scale)/2,(H-img.height*scale)/2,img.width*scale,img.height*scale);URL.revokeObjectURL(url);updateSource();};img.onerror=()=>{URL.revokeObjectURL(url);$('status').textContent='画像を読み込めませんでした。PNGまたはJPEGでお試しください。';};img.src=url;};
function pointer(e){const r=overlay.getBoundingClientRect();return [(e.clientX-r.left)*W/r.width,(e.clientY-r.top)*H/r.height];}
overlay.onpointerdown=e=>{if(!$('guide').checked)return;const p=pointer(e);drag=points.findIndex(q=>Math.hypot(q[0]-p[0],q[1]-p[1])<18);if(drag>=0)overlay.setPointerCapture(e.pointerId);};
overlay.onpointermove=e=>{if(drag<0)return;points[drag]=pointer(e).map((v,i)=>Math.max(12,Math.min((i?H:W)-12,v)));dirty=true;request();};
overlay.onpointerup=overlay.onpointercancel=()=>drag=-1;
preset('arc');defaultSource();
