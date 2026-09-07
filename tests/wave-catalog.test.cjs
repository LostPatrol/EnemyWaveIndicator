// Verify UI/native catalog coverage against independently extracted stock controller call operands.
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const root=path.join(__dirname,'..');
const audit=JSON.parse(fs.readFileSync(path.join(root,'docs/wave-controller-audit.json'),'utf8'));
const header=fs.readFileSync(path.join(root,'engine/Authoring/NwiAuthoring/Source/NwiAuthoring/Public/NwiWaveTypes.h'),'utf8');
const catalog=[...header.matchAll(/\{L"([^"]+)", L"([^"]+)", L"([^"]*)"\}/g)].map(m=>({key:m[1],title:m[2],path:m[3]}));
test('all concrete audited controller classes have one stable native/UI source entry',()=>{
 assert.equal(catalog[0].key,'Natural');assert.equal(catalog.length,36);
 const concrete=audit.controllers.filter(x=>!x.classPath.endsWith('/EWC_Base.EWC_Base_C'));
 assert.deepEqual(catalog.slice(1).map(x=>x.path).sort(),concrete.map(x=>x.classPath).sort());
 assert.equal(new Set(catalog.map(x=>x.path)).size,catalog.length);
});
test('each concrete type reaches a covered spawn function with the actual initiating self',()=>{
 const covered=new Set(['SpawnEnemiesFromPool','SpawnEnemyGroupDescriptorSpreadOut','SpawnEnemyGroupDescriptor','SpawnEnemyGroupDescriptorWithCallbackSpreadOut','SpawnEnemiesAtLocation','SpawnEnemiesAtLocationWithCallback']);
 const byPath=new Map(audit.controllers.map(x=>[x.classPath,x]));
 for(const row of catalog.slice(1)){
  let node=byPath.get(row.path);const seen=new Set();let calls=[];
  while(node){assert(!seen.has(node.classPath));seen.add(node.classPath);calls.push(...node.calls);node=byPath.get(node.parent);}
  assert(calls.length>0,row.key+' has no audited spawning path');
  for(const call of calls){assert(covered.has(call.callee.split('.').pop()),call.callee);assert(call.args.startsWith('17'),row.key+' lost EX_Self provenance');}
 }
});
