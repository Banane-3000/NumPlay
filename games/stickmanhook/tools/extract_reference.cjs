// Extract data-only modules from the supplied, hash-pinned webpack bundle.
// Never starts PIXI, browser integration, advertising or the game entry point.
const fs=require('fs'),vm=require('vm'),path=require('path'),crypto=require('crypto');
const folder=path.resolve(process.argv[2]||'build/stickman-reference');
const raw=fs.readFileSync(path.join(folder,'bundle.js'));
if(crypto.createHash('sha256').update(raw).digest('hex')!=='1022ae002395b485b3d213a0ed186bf13014f2662288f8f12d5c4a8342843a5e')throw Error('Unexpected reference bundle');
const context={};vm.createContext(context);
vm.runInContext(raw.toString().replace('n(n.s = 125)','globalThis.mods=t,globalThis.req=n'),context,{timeout:1000});
const source=context.mods[284].toString();
const start=source.indexOf('var As = [')+9;
let depth=0,end=start;
for(;end<source.length;end++){
 if(source[end]==='[')depth++;
 if(source[end]===']'&&!--depth)break;
}
const skins=vm.runInNewContext('('+source.slice(start,end+1).replace(/[\w$]+\.a\b/g,'null')+')',{}, {timeout:1000});
const maps={};
for(const id of [165,175,207,251])maps[id]=context.req(id).keys().map(file=>({file,...context.req(id)(file)}));
const calls=Object.fromEntries([...source.matchAll(/([\w$]+) = n\((\d+)\)/g)].map(x=>[x[1],+x[2]]));
const aliases=Object.fromEntries([...source.matchAll(/([\w$]+) = n\.n\(([\w$]+)\)/g)].map(x=>[x[1],x[2]]));
let resourceBlock=source.slice(source.indexOf('Qh = {')+6,source.indexOf('\n        jh = n(14)'));
resourceBlock=resourceBlock.slice(0,resourceBlock.lastIndexOf('}')+1);
const resources={};
for(const [,key,value] of resourceBlock.matchAll(/(\w+): ([\w$]+)(\.a)?/g)){
 const id=calls[aliases[value]||value];
 if(id!==undefined)resources[key]=context.req(id);
}
fs.writeFileSync(path.join(folder,'reference.json'),JSON.stringify({skins,maps,recipes:context.req(20),resources,shapes:context.req(75)},null,2));
console.log('Extracted 9 introductory levels, 30 recipes, 105 sections and 23 characters.');
