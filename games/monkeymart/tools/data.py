"""Compile the checked-in, original v2.1 economy/layout tables into flash data."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def c(v):return '{'+','.join(c(x)if isinstance(x,(list,tuple))else str(x)for x in v)+'}'
def main():
 d=json.loads((ROOT/'assets/reference.json').read_text());ids=json.loads((ROOT/'assets/sprite-ids.json').read_text())
 lines=['#include "data.h"']
 for mi,m in enumerate(d['marts']):
  boxes=m['listPlaceBoxes'];worker_ids=m['aimPlaces'];cash=next(b['id']for b in boxes if b['link']=='placebox_cashbox');up=m['listUpgradesId']
  lines.append('static const PlaceDef places%d[]={\n'%mi)
  for b in boxes:
   p=b['pos'];off=b['offsetWork'];buy=b.get('offsetBuy',{'x':0,'y':0});flags=(1 if b.get('isZavod')else 0)|(2 if b.get('isSklad')else 0)|(4 if b.get('isForBuyer')else 0)|(8 if b.get('isHideSprite')else 0)|(16 if b['id']==cash else 0)|(32 if b['id']in worker_ids else 0)|(64 if b['link']=='placebox_nextmart'else 0)
   inputs=b.get('inputs',[]);activate=0
   # Buying an animal activates its feed dish; buying a machine activates output.
   if inputs:
    if mi==0 and b['id']in [13,16]:activate=inputs[0]
   for machine in m['machines']:
    if b['id']==machine[2]and not (mi==0 and machine[1]in [13,16]):activate=machine[1]
   if mi==1 and b['id']==41:activate=43
   stamina=b.get('stamina_max',8)if inputs else 0
   if not isinstance(stamina,int):stamina=8
   sprite=ids.get('game__'+b['sprites'][0],0);sprite2=ids.get('game__'+b['sprites'][1],0)if len(b['sprites'])>1 else 0
   poly=[[p['x']+q['x'],p['y']+q['y']]for q in b['polygon']];poly+= [[0,0]]*(4-len(poly))
   mask=sum(1<<(x-1)for x in m['unlocks'][b['id']-1]);upgrade=up.index(b['id'])if b['id']in up else 255
   vals=[f'.pos={c([p["x"],p["y"]])}',f'.work={c([p["x"]+off["x"],p["y"]+off["y"]])}',f'.buy={c([p["x"]+buy["x"],p["y"]+buy["y"]])}',f'.poly={c(poly)}',f'.unlock=UINT64_C({mask})',f'.price={b["price"]}',f'.radius={b.get("radius",3000)}',f'.sprite={sprite}',f'.sprite2={sprite2}',f'.food={b.get("typeFood",0)}',f'.flags={flags}',f'.capacity={b.get("maxCount",0)}',f'.period={b.get("zavodBuild",0)}',f'.inputs={c([(x-1)for x in inputs]+[255]*(2-len(inputs)))}',f'.activate={activate-1 if activate else 255}',f'.stamina={stamina}',f'.upgrade={upgrade}',f'.poly_count={len(b["polygon"])}']
   offset=b.get('offsetGraphic',{'x':0,'y':0});vals.append('.second='+c([p['x']-offset['x'],p['y']-offset['y']]))
   # The second feed dish is part of the chocolate bar machine purchase.
   if mi==1 and b['id']==43:vals=[x if not x.startswith('.activate=')else'.activate=41'for x in vals]
   if mi==1 and b['id']==42:vals=[x if not x.startswith('.activate=')else'.activate=1'for x in vals]
   lines.append('{'+','.join(vals)+'},')
  lines.append('};')
  lines.append('static const uint8_t order%d[]=%s;'%(mi,c([x-1 for x in m['listSalesMain']])))
  lines.append('static const uint8_t upgrade_places%d[]=%s;'%(mi,c([x-1 if x else 255 for x in up])))
  workers=[]
  for wi,pid in enumerate(worker_ids):
   if wi==len(worker_ids)-1:w={'hat':5};ug=(9 if mi==0 else 6)
   else:w=m['workers'][wi];ug=next((i for i,x in enumerate(up)if x==pid),255)
   workers.append('{UINT64_C(%d),%d,%d,%d}'%(sum(1<<(x-1)for x in w.get('destinations',[])),pid-1,w.get('hat',0),ug))
  lines.append('static const WorkerDef workers%d[]={%s};'%(mi,','.join(workers)))
  decor=[]
  for v in m['decor']:decor.append('{'+c([v['pos']['x'],v['pos']['y']])+',%d,%d,0,100}'%(v.get('depth',-v['pos']['y']),ids['game__'+v['sprite']]))
  for v in m.get('extra_graphics',[]):
   depth=v.get('depth',0);depth=-32000 if abs(depth)<5 else depth
   decor.append('{'+c([v['x'],v['y']])+',%d,%d,%d,%d}'%(depth,ids['game__'+v['sprite']],v.get('condition',0),round(v.get('scale',1)*100)))
  lines.append('static const Decor decor%d[]={%s};'%(mi,','.join(decor)))
  g=d['global'];suffix=''if mi==0 else'2'
  prices=[[[0 if isinstance(x,str)else x for x in level]for level in row]for row in g['upgrades_price'+suffix]]
  lines.append('static const uint16_t prices%d[][3][3]=%s;'%(mi,c(prices)))
  lines.append('static const uint8_t maximum%d[][3]=%s;'%(mi,c(g['upgrades_max'+suffix])))
 lines.append('const MartDef marts[]={')
 for mi,m in enumerate(d['marts']):
  lines.append('{places%d,decor%d,workers%d,order%d,prices%d,maximum%d,upgrade_places%d,%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d},'%(mi,mi,mi,mi,mi,mi,mi,c([m['posDodik']['x'],m['posDodik']['y']]),c([m['posStart']['x'],m['posStart']['y']]),c([m['posTrash']['x'],m['posTrash']['y']]),m['limitMoveX']['min'],m['limitMoveX']['max'],m['limitMoveY']['min'],m['limitMoveY']['max'],len(m['listPlaceBoxes']),len(m['decor'])+len(m.get('extra_graphics',[])),len(m['aimPlaces']),len(m['listSalesMain']),next(i for i,b in enumerate(m['listPlaceBoxes'])if b['link']=='placebox_cashbox'),len(m['listUpgradesId'])))
 lines.append('};')
 # Rewarded-ad-only hats remain obtainable offline through equivalent saved tokens.
 lines.append('const uint16_t hat_prices[21]='+c([v['amount']if v['type']=='coins'else 0 for v in d['global']['data_hats']])+';')
 lines.append('const uint8_t hat_tokens[21]='+c([v['amount']if v['type']=='ads'else 0 for v in d['global']['data_hats']])+';')
 lines.append('const uint8_t food_prices[15]='+c([0]+d['global']['priceFood'])+';')
 lines.append('const uint8_t food_max[15]='+c([0]+d['global']['maxTypeFood'])+';')
 lines.append('const char*const food_names[]={"","Bananas","Corn","Eggs","Milk","Popcorn","Yogurt","Cocoa","Coffee beans","Peanuts","Chocolate","Ground coffee","Peanut butter","Cappuccino","Chocolate bars"};')
 lines.append('const char*const upgrade_names[2][12]={{"You","Assistant A","Assistant B","Farmer","Chef","Chickens","Cow","Popcorn machine","Yogurt machine","Manager","",""},{"You","Assistant A","Assistant B","Assistant C","Chef B","Chef A","Manager","Coffee grinder","Coffee machine","Peanut machine","Chocolate machine","Chocolate bars"}};')
 (ROOT/'src/data.c').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
if __name__=='__main__':main()
