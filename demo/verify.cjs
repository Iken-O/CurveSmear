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
  const before=bc.getImageData(0,0,W,H).data,out=ctx.getImageData(0,0,W,H).data;let outside=0,inside=0;
  for(let i=0;i<W*H;i++){let diff=false;for(let k=0;k<4;k++)if(out[i*4+k]!==before[i*4+k])diff=true;if(diff){if(field.distance[i]>=Number(document.getElementById('radius').value))outside++;else inside++;}}
  return {outside,inside};
 });
 await settle();const initial=await snapshot();let a=await audit();assert.equal(a.outside,0);assert(a.inside>100);console.log('Pixel audit:',a);
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
 await page.goto(pathToFileURL(path.join(__dirname,'v1.html')).href);await settle();assert.deepEqual(errors,[]);
 console.log('PASS: local influence, fixed source, zero amount, reverse, radius, drag, upload, reset, PNG export, legacy demo');
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
