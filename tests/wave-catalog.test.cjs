// Verify UI/native catalog coverage against independently extracted stock controller call operands.
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const root=path.join(__dirname,'..');
const audit=JSON.parse(fs.readFileSync(path.join(root,'docs/wave-controller-audit.json'),'utf8'));
const eventAudit=JSON.parse(fs.readFileSync(path.join(root,'docs/event-wave-audit.json'),'utf8'));
const missionAudit=JSON.parse(fs.readFileSync(path.join(root,'docs/mission-modifier-wave-audit.json'),'utf8'));
const header=fs.readFileSync(path.join(root,'engine/Authoring/NwiAuthoring/Source/NwiAuthoring/Public/NwiWaveTypes.h'),'utf8');
const settings=fs.readFileSync(path.join(root,'engine/Authoring/NwiAuthoring/Source/NwiAuthoring/Private/NwiSettings.inl'),'utf8');
const waveDocs=fs.readFileSync(path.join(root,'docs/WAVE-TYPES.md'),'utf8');
const catalog=[...header.matchAll(/\{L"([^"]+)", L"([^"]+)", L"([^"]*)"\}/g)].map(m=>({key:m[1],title:m[2],path:m[3]}));
test('all concrete audited controller classes have one stable native/UI source entry',()=>{
 assert.equal(catalog[0].key,'Natural');assert.equal(catalog.length,47);
 const concrete=audit.controllers.filter(x=>!x.classPath.endsWith('/EWC_Base.EWC_Base_C'));
 const eventStart=1+concrete.length;
 assert.deepEqual(catalog.slice(1,eventStart).map(x=>x.path).sort(),concrete.map(x=>x.classPath).sort());
 const missionStart=eventStart+eventAudit.sources.length;
 assert.deepEqual(catalog.slice(eventStart,missionStart).map(x=>x.path),eventAudit.sources.map(x=>x.classPath));
 assert.deepEqual(catalog.slice(eventStart,missionStart).map(x=>x.key),eventAudit.sources.map(x=>x.key));
 assert.deepEqual(catalog.slice(missionStart).map(x=>x.path),missionAudit.sources.map(x=>x.classPath));
 assert.deepEqual(catalog.slice(missionStart).map(x=>x.key),missionAudit.sources.map(x=>x.key));
 assert.equal(new Set(catalog.map(x=>x.path)).size,catalog.length);
});
test('Simplified Chinese UI wave names exactly follow WAVE-TYPES while marker defaults stay English',()=>{
 const block=settings.match(/const TCHAR\* WaveTitlesZhCn\[\] = \{([\s\S]*?)\n\};/);assert(block);
 const chinese=[...block[1].matchAll(/TEXT\("([^"]+)"\)/g)].map(x=>x[1]);assert.equal(chinese.length,catalog.length);
 const documented=new Map([...waveDocs.matchAll(/^\|\s*(\d+)\s*\|[^\n]*?<br>([^|]+?)\s*\|/gm)].map(x=>[Number(x[1]),x[2].trim()]));
 assert.equal(documented.size,catalog.length);assert.deepEqual(chinese,catalog.map((_,i)=>documented.get(i)));
 assert.match(settings,/FString\(TEXT\("\[!\] "\)\) \+ nwi::WaveTypes/);
 assert(!/WaveTitlesZhCn\[\(I-16\)\/2\+1\]/.test(settings.match(/FString SettingDefault[^\n]+/)[0]));
});
test('each concrete type reaches a covered spawn function with the actual initiating self',()=>{
 const covered=new Set(['SpawnEnemiesFromPool','SpawnEnemyGroupDescriptorSpreadOut','SpawnEnemyGroupDescriptor','SpawnEnemyGroupDescriptorWithCallbackSpreadOut','SpawnEnemiesAtLocation','SpawnEnemiesAtLocationWithCallback']);
 const byPath=new Map(audit.controllers.map(x=>[x.classPath,x]));
 const concreteCount=audit.controllers.filter(x=>!x.classPath.endsWith('/EWC_Base.EWC_Base_C')).length;
 for(const row of catalog.slice(1,1+concreteCount)){
  let node=byPath.get(row.path);const seen=new Set();let calls=[];
  while(node){assert(!seen.has(node.classPath));seen.add(node.classPath);calls.push(...node.calls);node=byPath.get(node.parent);}
  assert(calls.length>0,row.key+' has no audited spawning path');
  for(const call of calls){assert(covered.has(call.callee.split('.').pop()),call.callee);assert(call.args.startsWith('17'),row.key+' lost EX_Self provenance');}
 }
 for(const row of [...eventAudit.sources,...missionAudit.sources]){
  assert(row.calls.length>0,row.key+' has no audited spawning path');
  for(const call of row.calls){assert(covered.has(call.callee),call.callee);assert.equal(call.firstArgumentOpcode,'0x17',row.key+' lost EX_Self provenance');}
 }
 for(const alias of [...eventAudit.aliases,...missionAudit.aliases]){
  assert(alias.typeId>0&&alias.typeId<catalog.length,'event alias has an invalid target type');
  assert(alias.calls.length>0,'event alias has no audited spawning path');
  for(const call of alias.calls){assert(covered.has(call.callee),call.callee);assert.equal(call.firstArgumentOpcode,'0x17','event alias lost EX_Self provenance');}
  assert.match(header,new RegExp('\\{'+alias.typeId+', L"'+alias.classPath.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')+'"\\}'));
 }
});
test('mission modifiers prove active MissionBP ownership and exact center routes',()=>{
 assert.equal(missionAudit.gameSteamBuild,'24903151');
 assert.equal(missionAudit.worldContextOpcode.name,'EX_Self');
 assert.deepEqual(missionAudit.sources.map(x=>x.typeId),[39,40,41,42,43,44,45,46]);
 for(const row of missionAudit.sources){
  assert.equal(row.missionBP,row.classPath,row.key+' MissionBP does not own the classified class');
  assert.match(row.missionDefinition,/^\/Game\//);assert.match(row.centers,/exact/);
 }
 assert.deepEqual(missionAudit.sources.find(x=>x.typeId===42).calls.map(x=>x.callee),['SpawnEnemyGroupDescriptorSpreadOut','SpawnEnemiesAtLocation']);
 assert.match(missionAudit.sources.find(x=>x.typeId===44).centers,/distinct points remain distinct/);
});
test('catalog defaults keep the five intentionally quiet sources off',()=>{
 const expression=settings.match(/bool DefaultWaveEnabled\(int32 Type\) \{([^}]+)\}/);assert(expression);
 const disabled=[...expression[1].matchAll(/Type != (\d+)/g)].map(x=>Number(x[1]));
 assert.deepEqual(disabled,[1,6,32,34,46]);
});
