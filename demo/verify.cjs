// Run with NODE_PATH pointing to an installation of playwright.
const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
const path=require('node:path');
(async()=>{
 const browser=await chromium.launch({channel:'chrome',headless:true});
 try {
 const page=await browser.newPage({viewport:{width:1400,height:1080},acceptDownloads:true});
 const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.goto(pathToFileURL(path.join(__dirname,'index.html')).href);
 const settle=()=>page.waitForTimeout(240);
 const snapshot=()=>page.locator('#render').evaluate(c=>c.toDataURL());
 const slider=async(id,value)=>{await page.locator('#'+id).fill(String(value));await page.locator('#'+id).dispatchEvent('input');await settle();};
 const audit=()=>page.evaluate(()=>{
  // Compare through the same canvas round trip: browsers quantize low-alpha RGB.
  const base=document.createElement('canvas');base.width=W;base.height=H;const bc=base.getContext('2d',{willReadFrequently:true});bc.putImageData(new ImageData(new Uint8ClampedArray(pixels),W,H),0,0);
  const before=bc.getImageData(0,0,W,H).data,out=ctx.getImageData(0,0,W,H).data;let outside=0,inside=0,endcaps=0;
  for(let i=0;i<W*H;i++){let diff=false;for(let k=0;k<4;k++)if(out[i*4+k]!==before[i*4+k])diff=true;if(diff){if(field.distance[i]>=Number(document.getElementById('radius').value))outside++;else inside++;if(field.along[i]<=1e-3||field.along[i]>=curveLength-1e-3)endcaps++;}}
  return {outside,inside,endcaps};
 });
 await settle();const initial=await snapshot();let a=await audit();assert.equal(a.outside,0);assert.equal(a.endcaps,0);assert(a.inside>100);console.log('Flat pixel audit:',a);
 await page.click('[data-cap=round]');await settle();const round=await snapshot();assert.notEqual(round,initial);a=await audit();assert(a.endcaps>0);console.log('Round endcap pixels:',a.endcaps);
 await page.click('[data-cap=flat]');await settle();assert.equal(await snapshot(),initial);
 await page.click('#compare');await settle();const original=await snapshot();assert.notEqual(initial,original);
 await page.click('[data-preset=s]');await settle();assert.equal(await snapshot(),original);
 await page.click('#compare');await slider('length',0);assert.equal(await snapshot(),original);
 await slider('length',250);await page.click('[data-preset=arc]');await settle();assert.equal(await snapshot(),initial);
 await page.click('#reverse');await settle();assert.notEqual(await snapshot(),initial);assert.equal((await audit()).outside,0);await page.click('#reverse');
 await slider('radius',20);assert.equal((await audit()).outside,0);await slider('radius',65);
 const box=await page.locator('#overlay').boundingBox();await page.mouse.move(box.x+460/800*box.width,box.y+190/520*box.height);await page.mouse.down();await page.mouse.move(box.x+480/800*box.width,box.y+170/520*box.height);await page.mouse.up();await settle();assert.notEqual(await snapshot(),initial);assert.equal((await audit()).outside,0);
 await page.setInputFiles('#file',path.join(__dirname,'../image.png'));await settle();assert.equal((await audit()).outside,0);
 await page.click('#resetSource');await page.click('[data-preset=arc]');await settle();assert.equal(await snapshot(),initial);
 const download=page.waitForEvent('download');await page.click('#save');await(await download).saveAs(path.join(__dirname,'smear-example.png'));
 await page.screenshot({path:path.join(__dirname,'preview.png')});assert.deepEqual(errors,[]);
 // Exercise the profile through UI controls and compare actual rendered pixels.
 await page.click('#swordScene');await settle();const swordRamp=await snapshot();
 const swordSource=await page.locator('#sourcePreview').getAttribute('src');
 await page.click('[data-profile=uniform]');await settle();const swordUniform=await snapshot();assert.notEqual(swordUniform,swordRamp);
 await page.screenshot({path:path.join(__dirname,'preview-profile-uniform.png'),fullPage:true});
 await page.click('#compare');await settle();const swordOriginal=await snapshot();await page.click('#compare');
 await slider('profileLeft',0);await slider('profileRight',0);assert.equal(await snapshot(),swordOriginal,'0% must preserve all source pixels');
 await page.click('[data-profile=ramp]');await settle();assert.equal(await snapshot(),swordRamp);
 assert.equal(await page.locator('#sourcePreview').getAttribute('src'),swordSource,'Profile must not relocate source');
 await page.click('#flipProfile');await settle();assert.notEqual(await snapshot(),swordRamp);
 await page.click('#flipProfile');await settle();assert.equal(await snapshot(),swordRamp);
 const gains=await page.evaluate(()=>[widthGain(100,100),widthGain(0,100),widthGain(-100,100)]);
 assert(Math.abs(gains[0]-.01)<1e-6&&Math.abs(gains[1]-.505)<1e-6&&gains[2]===1);
 await page.click('#reverse');await settle();assert.deepEqual(await page.evaluate(()=>[widthGain(100,100),widthGain(0,100),widthGain(-100,100)]),gains);await page.click('#reverse');await settle();
 const graph=await page.locator('#profile').boundingBox();
 const gp=(x,y)=>({x:graph.x+x/760*graph.width,y:graph.y+y/220*graph.height});
 const middle=gp(396,152),moved=gp(451,133);
 await page.mouse.click(middle.x,middle.y);await settle();assert.equal(await page.evaluate(()=>profilePoints.length),3);assert.notEqual(await snapshot(),swordRamp);
 await page.mouse.move(middle.x,middle.y);await page.mouse.down();await page.mouse.move(moved.x,moved.y);await page.mouse.up();await settle();
 const curved=await snapshot();await page.locator('#profileSmooth').uncheck();await settle();assert.notEqual(await snapshot(),curved);
 await page.locator('#profileSmooth').check();await page.click('#deleteProfilePoint');await settle();assert.equal(await page.evaluate(()=>profilePoints.length),2);assert.equal(await snapshot(),swordRamp);
 await page.click('[data-profile=tip]');await settle();assert.notEqual(await snapshot(),swordRamp);
 assert.equal((await audit()).outside,0);assert.equal((await audit()).endcaps,0);
 assert(await page.evaluate(()=>Array.from(profileLut).every(v=>Number.isFinite(v)&&v>=0&&v<=1)));
 await page.click('[data-profile=ramp]');await settle();
 await page.screenshot({path:path.join(__dirname,'preview-profile-ramp.png'),fullPage:true});
 assert.deepEqual(errors,[]);
 await page.goto(pathToFileURL(path.join(__dirname,'v1.html')).href);await settle();assert.deepEqual(errors,[]);
 console.log('PASS: Flat/Round caps, local influence, fixed source, zero amount, reverse, radius, drag, upload, reset, PNG export, width profile identity/ramp/flip, graph add/drag/delete/smoothing, sword scene, legacy demo');
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
