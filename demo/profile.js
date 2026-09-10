'use strict';
// Profile coordinates: x=0 is positive normal (L), x=1 is negative normal (R).
// Use a bounded, shape-preserving Hermite curve so edits cannot overshoot 0..100%.
let profilePoints=[[0,1],[1,1]],profileSelected=-1,profileDrag=-1;
const profileCanvas=document.getElementById('profile'),pc=profileCanvas.getContext('2d');
const profilePlot={x:54,y:18,w:684,h:168},profileLut=new Float32Array(1025);
function profileSlopes(){
 const p=profilePoints,h=[],d=[],m=[];
 for(let i=0;i<p.length-1;i++){h[i]=p[i+1][0]-p[i][0];d[i]=(p[i+1][1]-p[i][1])/h[i];}
 m[0]=d[0];m[p.length-1]=d[d.length-1];
 for(let i=1;i<p.length-1;i++){
  const a=d[i-1],b=d[i],w1=2*h[i]+h[i-1],w2=h[i]+2*h[i-1];
  m[i]=a*b<=0?0:(w1+w2)/(w1/a+w2/b);
 }
 return m;
}
function rebuildProfile(){
 const p=profilePoints,m=profileSlopes(),curved=document.getElementById('profileSmooth').checked;
 let j=0;
 for(let k=0;k<profileLut.length;k++){
  const u=k/(profileLut.length-1);while(j<p.length-2&&u>p[j+1][0])j++;
  const h=p[j+1][0]-p[j][0],t=(u-p[j][0])/h,t2=t*t,t3=t2*t;
  const v=curved?(2*t3-3*t2+1)*p[j][1]+(t3-2*t2+t)*h*m[j]+(-2*t3+3*t2)*p[j+1][1]+(t3-t2)*h*m[j+1]:p[j][1]*(1-t)+p[j+1][1]*t;
  profileLut[k]=Math.max(0,Math.min(1,v));
 }
 document.getElementById('profileLeft').value=Math.round(p[0][1]*100);
 document.getElementById('profileRight').value=Math.round(p[p.length-1][1]*100);
 document.querySelectorAll('[data-profile]').forEach(b=>b.classList.toggle('active',b.dataset.profile==='uniform'&&p.every(q=>q[1]===1)));
 drawProfile();
}
function profileAt(u){
 const v=Math.max(0,Math.min(1,u))*(profileLut.length-1),i=Math.floor(v),f=v-i;
 return profileLut[i]*(1-f)+profileLut[Math.min(i+1,profileLut.length-1)]*f;
}
function widthGain(n,r){return profileAt((1-n/r)/2);}
function drawProfile(){
 const {x,y,w,h}=profilePlot;
 pc.clearRect(0,0,760,220);pc.fillStyle='#111519';pc.fillRect(x,y,w,h);
 pc.font='12px system-ui';pc.lineWidth=1;
 for(let i=0;i<=4;i++){
  pc.strokeStyle='#354047';pc.beginPath();pc.moveTo(x,y+i*h/4);pc.lineTo(x+w,y+i*h/4);pc.moveTo(x+i*w/4,y);pc.lineTo(x+i*w/4,y+h);pc.stroke();
  pc.fillStyle='#aab5b9';pc.textAlign='right';pc.fillText(`${100-i*25}%`,x-9,y+i*h/4+4);
 }
 pc.strokeStyle='#d3f8a9';pc.lineWidth=2;pc.beginPath();
 for(let k=0;k<=684;k++){const px=x+k,py=y+h*(1-profileAt(k/684));if(k===0)pc.moveTo(px,py);else pc.lineTo(px,py);}pc.stroke();
 profilePoints.forEach((p,i)=>{pc.beginPath();pc.arc(x+p[0]*w,y+(1-p[1])*h,i===profileSelected?7:5,0,Math.PI*2);pc.fillStyle=i===0?'#f5c78b':i===profilePoints.length-1?'#a7c9ff':'#d3f8a9';pc.fill();if(i===profileSelected){pc.strokeStyle='#fff';pc.lineWidth=1;pc.stroke();}});
 pc.textAlign='left';pc.fillStyle='#f5c78b';pc.fillText('L',x,y+h+24);pc.textAlign='center';pc.fillStyle='#aab5b9';pc.fillText('中心',x+w/2,y+h+24);pc.textAlign='right';pc.fillStyle='#a7c9ff';pc.fillText('R',x+w,y+h+24);
 document.getElementById('deleteProfilePoint').disabled=profileSelected<=0||profileSelected>=profilePoints.length-1;
}
function changeProfile(){rebuildProfile();request();}
function setProfile(name){
 profilePoints=name==='ramp'?[[0,.01],[1,1]]:name==='tip'?[[0,.01],[.55,.03],[.8,.35],[1,1]]:[[0,1],[1,1]];
 profileSelected=profileDrag=-1;changeProfile();
 document.querySelectorAll('[data-profile]').forEach(b=>b.classList.toggle('active',b.dataset.profile===name));
}
function profilePointer(e){const r=profileCanvas.getBoundingClientRect();return [(e.clientX-r.left)*760/r.width,(e.clientY-r.top)*220/r.height];}
profileCanvas.onpointerdown=e=>{
 const [x,y]=profilePointer(e),g=profilePlot;
 let i=profilePoints.findIndex(p=>Math.hypot(g.x+p[0]*g.w-x,g.y+(1-p[1])*g.h-y)<13);
 if(i<0){
  const u=(x-g.x)/g.w;if(u<.015||u>.985||y<g.y||y>g.y+g.h)return;
  i=profilePoints.findIndex(p=>p[0]>u);
  if(u-profilePoints[i-1][0]<.015||profilePoints[i][0]-u<.015)return;
  profilePoints.splice(i,0,[u,Math.max(0,Math.min(1,1-(y-g.y)/g.h))]);
 }
 profileSelected=profileDrag=i;profileCanvas.focus({preventScroll:true});profileCanvas.setPointerCapture(e.pointerId);changeProfile();
};
profileCanvas.onpointermove=e=>{
 if(profileDrag<0)return;const [x,y]=profilePointer(e),g=profilePlot,i=profileDrag;
 if(i>0&&i<profilePoints.length-1)profilePoints[i][0]=Math.max(profilePoints[i-1][0]+.015,Math.min(profilePoints[i+1][0]-.015,(x-g.x)/g.w));
 profilePoints[i][1]=Math.max(0,Math.min(1,1-(y-g.y)/g.h));changeProfile();
};
profileCanvas.onpointerup=profileCanvas.onpointercancel=profileCanvas.onlostpointercapture=()=>{profileDrag=-1;};
function deleteProfilePoint(){if(profileSelected>0&&profileSelected<profilePoints.length-1){profilePoints.splice(profileSelected,1);profileSelected=profileDrag=-1;changeProfile();}}
profileCanvas.onkeydown=e=>{if(e.key==='Delete'||e.key==='Backspace'){e.preventDefault();deleteProfilePoint();}};
document.getElementById('deleteProfilePoint').onclick=deleteProfilePoint;
document.querySelectorAll('[data-profile]').forEach(b=>b.onclick=()=>setProfile(b.dataset.profile));
document.getElementById('flipProfile').onclick=()=>{profilePoints=profilePoints.map(p=>[1-p[0],p[1]]).reverse();profileSelected=profileDrag=-1;changeProfile();};
for(const [id,end] of [['profileLeft',false],['profileRight',true]])document.getElementById(id).oninput=e=>{
 if(e.target.value==='')return;const v=e.target.valueAsNumber;if(!Number.isFinite(v))return;
 profilePoints[end?profilePoints.length-1:0][1]=Math.max(0,Math.min(100,v))/100;changeProfile();
};
document.getElementById('profileSmooth').onchange=changeProfile;
rebuildProfile();
