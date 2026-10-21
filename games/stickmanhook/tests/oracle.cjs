// Compare independent single-frame results with the actual supplied JavaScript physics.
const fs=require('fs'),path=require('path');
const {Vec,install,bumper,wall}=require('./original.js');
const root=path.join(__dirname,'..'),levels=JSON.parse(fs.readFileSync(path.join(root,'assets/levels.json')));
const rawShapes=JSON.parse(fs.readFileSync(path.join(root,'assets/geometry.json')));
const allParts=['165','175','207','251'].flatMap(k=>levels.maps[k]);
const parts=JSON.parse(fs.readFileSync(path.join(root,'src/levels_index.json'))).map(name=>allParts.find(p=>p.file===name));
const cases=JSON.parse(fs.readFileSync(process.argv[2]));
const keys=['x','y','vx','vy','theta','omega','r','pull','rotation','spin'];let maxError=0;const maxByField=Object.fromEntries(keys.map(k=>[k,0]));
function player(s){
 const n={position:new Vec(s.x,s.y),speed:new Vec(s.vx,s.vy),acceleration:new Vec(),radius:.017,r:s.r,theta:s.theta,vtheta:s.omega,
   hookPullRemaining:s.pull,rotation:s.rotation,angularVelocity:s.spin,applyGravity:true,hook:s.hooked?{position:new Vec(s.hook_x,s.hook_y)}:null,
   rope:{position:new Vec(),scale:{}},scale:{x:1,y:1},shadow:{x:0},parent:{addChildAt(){},removeChild(){}},gotoFrame(){}};
 install(n,{instance:{gravity:new Vec(0,1)},AIR_FRICTION:0});return n;
}
let wallCases=0,bumperCases=0;
for(const c of cases){
 const s=c.before,n=player(s);let hooks=[],bumpers=[],walls=[];
 for(const section of s.sections){const p=parts[section.part],offset=section.x;
  hooks.push(...p.hooks.map(h=>({position:new Vec(h.x+offset,h.y)})));
  bumpers.push(...p.bumpers.map(b=>({position:new Vec(b.x+offset,b.y),rotation:b.rotation||0,w:b.w||.15,h:b.h||.04,
    radius:Math.abs(b.w||.15),bounciness:b.bounce?.bounce||1.2,maxSpeed:3,minSpeed:.1,bounceForce:b.bounce?.force,bounceAngle:b.bounce?.angle})));
  walls.push(...(p.obstacles||[]).map(o=>{const sn=Math.sin(o.rotation||0),cs=Math.cos(o.rotation||0);let radius=0;
   const shapes=rawShapes[o.name].map(poly=>{let v=[];for(let j=0;j<poly.shape.length;j+=2){let x=(poly.shape[j]-256)*.0005,y=(poly.shape[j+1]-256)*.0005;radius=Math.max(radius,Math.hypot(x,y));v.push({x:o.x+offset+x*cs-y*sn,y:o.y+x*sn+y*cs});}return v;});
   return{position:new Vec(o.x+offset,o.y),radius,shapes};}));
 }
 if(c.held&&!s.held){hooks.sort((a,b)=>n.position.distanceToSquared(a.position)-n.position.distanceToSquared(b.position));n.hookTo(hooks[0]);}
 if(!c.held&&s.held)n.unhook();
 n.ignoreNextCollision=false;
 for(let i=0;i<4;i++){n.step(1/240);bumpers.forEach(b=>bumper(n,b));walls.forEach(o=>wall(n,o));}
 if(n.position.x>s.finish)n.unhook();
 const actual={x:n.position.x,y:n.position.y,vx:n.speed.x,vy:n.speed.y,theta:n.theta,omega:n.vtheta,r:n.r,pull:n.hookPullRemaining,rotation:n.rotation,spin:n.angularVelocity};
 // Completion sets a victory pose, without changing the trajectory.
 if(n.position.x>s.finish)actual.spin=0,actual.rotation=Math.atan2(n.speed.y,n.speed.x)+Math.PI/2;
 for(const key of keys){let error=Math.abs(actual[key]-c.after[key]);maxError=Math.max(maxError,error);maxByField[key]=Math.max(maxByField[key],error);
  if(error>(['vx','vy','omega','spin'].includes(key)?2e-3:2e-4)+Math.abs(actual[key])*2e-5){console.error({level:s.level+1,key,error,native:c.after[key],original:actual[key],before:s,held:c.held});process.exit(1);}}
 if(c.after.wall_hits>s.wall_hits)wallCases++;
 if(c.after.bumper_hits>s.bumper_hits)bumperCases++;
}
const report={frames:cases.length,wall_collision_frames:wallCases,bumper_collision_frames:bumperCases,max_absolute_error:maxError,max_error_by_field:maxByField,result:'PASS'};
fs.writeFileSync(path.join(root,'output/fidelity-report.json'),JSON.stringify(report,null,2)+'\n');console.log(report);
