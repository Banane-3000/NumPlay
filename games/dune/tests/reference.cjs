/* Compare native controls and collision terrain with the pinned original JS. */
const fs=require('fs'),vm=require('vm'),crypto=require('crypto'),path=require('path');
const root=path.resolve(__dirname,'..'),file=process.argv[2]||path.resolve(root,'../../build/dune-reference/js/game.js');
const source=fs.readFileSync(file,'utf8'),provenance=JSON.parse(fs.readFileSync(path.join(root,'assets/provenance.json'),'utf8'));
if(crypto.createHash('sha256').update(source).digest('hex')!==provenance.javascript_sha256)throw Error('Wrong source revision');
const terrainCode=source.slice(source.indexOf('function CreateTerrainSegment('),source.indexOf('function ReskinSegment('));
const inputStart=source.indexOf('update: function () {',source.indexOf('var InputHandler ='));
const inputCode='({'+source.slice(inputStart,source.indexOf('        OnDown: function',inputStart)).trim().replace(/,$/,'')+'})';
const values=[];let seed=1234567;function rand(){seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return ((seed>>>0)>>>8)/16777216;}
for(let i=0;i<100;i++){
 const r=[rand(),rand(),rand()],flat=i%11===0,q=[...r];const math=Object.create(Math);math.random=()=>q.shift();
 const ctx={TERRAIN_RES_FACTOR:1,Math:math,currentLevel:0,DrawTerrainToBitmap(){},makeSprite:()=>({}),game:{add:{bitmapData:()=>({generateTexture(){}})},rnd:{uuid:()=>''}}};
 vm.runInNewContext('Array.prototype.last=function(){return this[this.length-1]};'+terrainCode,ctx);
 const t=ctx.CreateTerrainSegment(false,flat,0);values.push(...r,+flat,t.width);
 for(let n=0;n<=63;n++)values.push(t.points[n].y);
}
for(let i=0;i<1000;i++){
 const vx=Math.fround(rand()*4000),vy=Math.fround(rand()*6000-2500),grounded=i%2===0,held=i%7!==0;
 const ctx={levelRunning:true,gameRunning:true,clickValid:true,ballMoveInput:false,cursors:{right:{justDown:false,isDown:held}},game:{input:{activePointer:{isDown:false}}},ball:{body:{velocity:{x:vx,y:vy}},touchingGround:grounded},currentTerrain:{curveFunction:()=>({x:1,y:2})},BallOnCurrentTerrainNormalized:()=>.7};
 const update=vm.runInNewContext(inputCode,ctx).update;update();values.push(vx,vy,+grounded,+held,ctx.ball.body.velocity.x,ctx.ball.body.velocity.y);
}
const bytes=Buffer.alloc(values.length*4);values.forEach((v,i)=>bytes.writeFloatLE(v,i*4));fs.mkdirSync(path.join(root,'output'),{recursive:true});fs.writeFileSync(path.join(root,'output/reference.bin'),bytes);console.log('Actual source: 100 terrains and 1000 control samples');
