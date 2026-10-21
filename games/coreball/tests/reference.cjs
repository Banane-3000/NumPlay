/* Execute the pinned game's actual level and motion functions, not a rewrite. */
const fs=require('fs'),vm=require('vm'),crypto=require('crypto'),path=require('path');
const root=path.resolve(__dirname,'..');
const file=process.argv[2]||path.resolve(root,'../../build/coreball-reference/js/index.js');
const source=fs.readFileSync(file,'utf8'),provenance=JSON.parse(fs.readFileSync(path.join(root,'assets/provenance.json'),'utf8'));
if(crypto.createHash('sha256').update(source).digest('hex')!==provenance.javascript_sha256)throw Error('Wrong source revision');
const code=source.slice(source.indexOf('$AJB.general.Levels = function()'),source.indexOf('    $AJB.general.Collide = function()')).trim().replace(/,$/,'');
const frames=1100,bytes=Buffer.alloc(60*frames*4);let offset=0;
for(let level=1;level<=60;level++){
 let now=0,handlers=[];const context={document:{body:{}},Date:class{valueOf(){return Math.floor(now)}},$AJB:{general:{},lib:{addEvent:()=>((element,event,callback)=>handlers.push(callback))}}};
 vm.runInNewContext(code,context);const levels=context.$AJB.general.Levels(),entry=levels[level],recipe=JSON.parse(fs.readFileSync(path.join(root,'assets/levels.json'),'utf8'))[level];
 if(entry.childs.length!==+recipe[0]||entry.queueCount!==recipe[1])throw Error('Level mismatch '+level);
 const motion=entry.round();
 for(let frame=1;frame<=frames;frame++){now=frame*1000/60;if(frame%137===0)handlers.forEach(h=>h());let angle=((motion()%360)+360)%360;bytes.writeFloatLE(angle,offset);offset+=4;}
}
fs.mkdirSync(path.join(root,'output'),{recursive:true});fs.writeFileSync(path.join(root,'output/reference.bin'),bytes);console.log('Actual source: 60 levels, '+(frames*60)+' motion samples');
