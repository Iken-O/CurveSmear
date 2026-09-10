const {chromium}=require('playwright');
const fs=require('fs'),path=require('path'),{pathToFileURL}=require('url'),{execFileSync}=require('child_process');
const root=path.resolve(__dirname,'..'),out=path.join(root,'test-output');
(async()=>{
 const b=await chromium.launch({channel:'chrome'});
 try {
 const p=await b.newPage();await p.goto(pathToFileURL(path.join(root,'demo/index.html')).href);await p.waitForTimeout(300);
 const source=await p.evaluate(()=>Array.from(pixels));fs.writeFileSync(path.join(out,'source.rgba'),Buffer.from(source));
 console.log(execFileSync(path.join(root,'build/CoreTests.exe'),[path.join(out,'source.rgba'),path.join(out,'native.rgba')],{encoding:'utf8'}));
 const native=Array.from(fs.readFileSync(path.join(out,'native.rgba')));
 const result=await p.evaluate(data=>{
  const ref=ctx.getImageData(0,0,W,H).data;let total=0,max=0,count=0;
  for(let i=0;i<data.length;i++){if(i%4===3||ref[i-i%4+3]>32){const diff=Math.abs(ref[i]-data[i]);total+=diff;max=Math.max(max,diff);count++;}}
  const c=document.createElement('canvas');c.width=W;c.height=H;c.getContext('2d').putImageData(new ImageData(new Uint8ClampedArray(data),W,H),0,0);
  return {mean:total/count,max,png:c.toDataURL()};
 },native);
 fs.writeFileSync(path.join(out,'native.png'),Buffer.from(result.png.split(',')[1],'base64'));delete result.png;
 fs.writeFileSync(path.join(out,'native-comparison.json'),JSON.stringify(result,null,2));console.log('Browser versus native (8bpc):',result);
 if(result.mean>1)throw Error('Native output diverges from the accepted browser demo');
 } finally {await b.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
