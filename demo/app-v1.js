'use strict';
const W=800,H=520,$=id=>document.getElementById(id);
const canvas=$('render'),ctx=canvas.getContext('2d'),overlay=$('overlay'),oc=overlay.getContext('2d');
const source=document.createElement('canvas');source.width=source.height=192;
const sc=source.getContext('2d',{willReadFrequently:true});
let pixels,points,field,curveLength,endTangent,scheduled=false,dirty=true,comparison=false,drag=-1;
const presets={arc:[[135,365],[220,460],[300,100],[585,160]],s:[[110,380],[445,475],[245,70],[645,165]],line:[[100,355],[260,295],[420,235],[580,175]]};
function defaultSource(){
 sc.clearRect(0,0,192,192);sc.fillStyle='#f9d977';sc.strokeStyle='#573944';sc.lineWidth=5;sc.lineJoin='round';
 sc.beginPath();sc.moveTo(21,64);sc.lineTo(78,43);sc.lineTo(111,26);sc.lineTo(155,37);sc.lineTo(176,65);sc.lineTo(164,142);sc.lineTo(130,164);sc.lineTo(69,153);sc.lineTo(19,125);sc.closePath();sc.fill();sc.stroke();
 sc.fillStyle='#b95741';sc.beginPath();sc.moveTo(119,33);sc.lineTo(153,40);sc.lineTo(170,66);sc.lineTo(161,141);sc.lineTo(130,158);sc.lineTo(118,131);sc.closePath();sc.fill();
 sc.fillStyle='#fff0aa';sc.beginPath();sc.moveTo(30,68);sc.lineTo(97,48);sc.lineTo(105,69);sc.lineTo(31,91);sc.closePath();sc.fill();
 sc.strokeStyle='#80513e';sc.lineWidth=4;for(let i=0;i<5;i++){sc.beginPath();sc.moveTo(65+i*9,99);sc.lineTo(59+i*9,125);sc.stroke();}
 updateSource();
}
function updateSource(){pixels=sc.getImageData(0,0,192,192).data;$('sourcePreview').src=source.toDataURL();request();}
function bezier(t){const u=1-t;return [0,1].map(k=>u*u*u*points[0][k]+3*u*u*t*points[1][k]+3*u*t*t*points[2][k]+t*t*t*points[3][k]);}
function buildField(){
 const segments=[];let prev=bezier(0),s=0;
 for(let i=1;i<=100;i++){const p=bezier(i/100),dx=p[0]-prev[0],dy=p[1]-prev[1],len=Math.hypot(dx,dy);if(len>1e-5){segments.push({x:prev[0],y:prev[1],dx,dy,len,s});s+=len;}prev=p;}
 curveLength=s;const last=segments[segments.length-1];endTangent=last?[last.dx/last.len,last.dy/last.len]:[1,0];
 const along=new Float32Array(W*H),normal=new Float32Array(W*H),valid=new Uint8Array(W*H);
 for(let y=0,i=0;y<H;y++)for(let x=0;x<W;x++,i++){
  let best=Infinity,bs=0,bn=0,bv=0;
  for(const g of segments){const rx=x+.5-g.x,ry=y+.5-g.y,raw=(rx*g.dx+ry*g.dy)/(g.len*g.len),t=Math.max(0,Math.min(1,raw));const ex=rx-t*g.dx,ey=ry-t*g.dy,d=ex*ex+ey*ey;
   if(d<best){best=d;bs=g.s+t*g.len;bn=(g.dx*ry-g.dy*rx)/g.len;bv= !((g===segments[0]&&raw<0)||(g===last&&raw>1));}}
  along[i]=bs;normal[i]=bn;valid[i]=bv;
 }
 field={along,normal,valid};dirty=false;
}
function noise(v){return .5+.24*Math.sin(v*1.17+1.4)+.16*Math.sin(v*2.71+.3)+.10*Math.sin(v*5.39+2.1);}
// Bilinear filtering in premultiplied alpha keeps transparent edges clean.
function sample(x,y,out,i,alpha){
 if(x<0||x>=191||y<0||y>=191)return;
 const ix=Math.floor(x),iy=Math.floor(y),fx=x-ix,fy=y-iy;
 let a=0,r=0,g=0,b=0;
 for(let yy=0;yy<2;yy++)for(let xx=0;xx<2;xx++){const j=((iy+yy)*192+ix+xx)*4,w=(xx?fx:1-fx)*(yy?fy:1-fy),aw=pixels[j+3]*w;a+=aw;r+=pixels[j]*aw;g+=pixels[j+1]*aw;b+=pixels[j+2]*aw;}
 if(a>0){out[i]=r/a;out[i+1]=g/a;out[i+2]=b/a;out[i+3]=a*alpha;}
}
function render(){
 const start=performance.now();if(dirty)buildField();
 const length=+$('length').value,width=+$('width').value,strength=+$('streak').value/100,freq=+$('frequency').value,original=+$('original').value/100;
 for(const id of ['length','width','streak','frequency','original'])$(id+'Value').value=$(id).value+(['length','width'].includes(id)?' px':['streak','original'].includes(id)?'%':'');
 ctx.clearRect(0,0,W,H);
 if(!comparison&&length>0){const img=ctx.createImageData(W,H),out=img.data;
  for(let i=0;i<W*H;i++){
   if(!field.valid[i])continue;
   const n=field.normal[i],v=n/width,grain=noise(v*freq*2),reach=Math.min(length,curveLength)*(1-strength*.58*grain),d=curveLength-field.along[i];
   if(d>reach||Math.abs(v)>.55)continue;
   const progress=d/Math.max(1,reach),taper=1-.32*strength*Math.pow(progress,1.5),sy=96+n/width*192/taper;
   const sx=139-progress*122;
   const tip=Math.min(1,Math.max(0,(reach-d)/1.3));
   const split=1-strength*.65*Math.pow(progress,2)*Math.pow(grain,3);
   sample(sx,sy,out,i*4,tip*split);
  }ctx.putImageData(img,0,0);
 }
 if(comparison||original>0){ctx.save();ctx.translate(points[3][0],points[3][1]);ctx.rotate(Math.atan2(endTangent[1],endTangent[0]));ctx.globalAlpha=comparison?1:original;ctx.drawImage(source,-139/192*width,-width/2,width,width);ctx.restore();}
 drawGuide();$('status').textContent=`${comparison?'元絵のみ':'Smear'} · ${Math.round(performance.now()-start)} ms · 800 × 520`;
}
function drawGuide(){oc.clearRect(0,0,W,H);if(!$('guide').checked)return;oc.lineWidth=1;oc.strokeStyle='#c6ed9380';oc.setLineDash([4,5]);oc.beginPath();oc.moveTo(...points[0]);oc.lineTo(...points[1]);oc.moveTo(...points[2]);oc.lineTo(...points[3]);oc.stroke();oc.setLineDash([]);oc.strokeStyle='#d3f8a9';oc.lineWidth=2;oc.beginPath();oc.moveTo(...points[0]);oc.bezierCurveTo(...points[1],...points[2],...points[3]);oc.stroke();points.forEach((p,i)=>{oc.beginPath();oc.arc(...p,i===0||i===3?7:5,0,Math.PI*2);oc.fillStyle=i===0||i===3?'#d3f8a9':'#263139';oc.fill();oc.stroke();oc.fillStyle='#edf5e7';oc.font='12px system-ui';oc.fillText(['始点','ハンドル','ハンドル','終点'][i],p[0]+12,p[1]-12);});}
function request(){if(!scheduled){scheduled=true;requestAnimationFrame(()=>{scheduled=false;render();});}}
function preset(name){points=presets[name].map(p=>p.slice());dirty=true;document.querySelectorAll('[data-preset]').forEach(b=>b.classList.toggle('active',b.dataset.preset===name));request();}
document.querySelectorAll('[data-preset]').forEach(b=>b.onclick=()=>preset(b.dataset.preset));
for(const id of ['length','width','streak','frequency','original'])$(id).oninput=request;
$('guide').onchange=drawGuide;
$('compare').onclick=()=>{comparison=!comparison;$('compare').classList.toggle('active',comparison);$('compare').textContent=comparison?'Smearに戻す':'元絵と比較';request();};
$('save').onclick=()=>{render();const a=document.createElement('a');a.download='curve-smear.png';a.href=canvas.toDataURL();a.click();};
$('resetSource').onclick=defaultSource;
$('file').onchange=()=>{const file=$('file').files[0];if(!file)return;const url=URL.createObjectURL(file),img=new Image();img.onload=()=>{sc.clearRect(0,0,192,192);const scale=192/Math.max(img.width,img.height);sc.drawImage(img,(192-img.width*scale)/2,(192-img.height*scale)/2,img.width*scale,img.height*scale);URL.revokeObjectURL(url);updateSource();};img.onerror=()=>{URL.revokeObjectURL(url);$('status').textContent='画像を読み込めませんでした。PNGまたはJPEGでお試しください。';};img.src=url;};
function pointer(e){const r=overlay.getBoundingClientRect();return [(e.clientX-r.left)*W/r.width,(e.clientY-r.top)*H/r.height];}
overlay.onpointerdown=e=>{if(!$('guide').checked)return;const p=pointer(e);drag=points.findIndex(q=>Math.hypot(q[0]-p[0],q[1]-p[1])<18);if(drag>=0)overlay.setPointerCapture(e.pointerId);};
overlay.onpointermove=e=>{if(drag<0)return;points[drag]=pointer(e).map((v,i)=>Math.max(12,Math.min((i?H:W)-12,v)));dirty=true;request();};
overlay.onpointerup=overlay.onpointercancel=()=>drag=-1;
preset('arc');defaultSource();
