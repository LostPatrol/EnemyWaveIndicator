// Audit cooked package imports without loading game code; fail on authoring/test/native-backend leaks.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
function inspect(file){
 const b=fs.readFileSync(file); assert.equal(b.readUInt32LE(0),0x9e2a83c1); assert.equal(b.readInt32LE(4),-7);
 const version=b.readInt32LE(12)||522; let at=24+b.readInt32LE(20)*20;
 function str(){const n=b.readInt32LE(at);at+=4;assert(Math.abs(n)<100000);const s=n<0?b.toString('utf16le',at,at+(-n-1)*2):b.toString('utf8',at,at+Math.max(n-1,0));at+=n<0?-n*2:n;return s;}
 at+=4;str();const flags=b.readUInt32LE(at);at+=4;const count=b.readInt32LE(at),namesAt=b.readInt32LE(at+4);at+=8;
 if(version>=516&&!(flags&0x80000000))str();at+=16;const importsCount=b.readInt32LE(at),importsAt=b.readInt32LE(at+4);
 assert(count>0&&count<100000&&importsCount>=0&&importsCount<100000);const names=[];at=namesAt;
 for(let i=0;i<count;i++){names.push(str());at+=4;}
 const name=pos=>{const i=b.readInt32LE(pos);assert(i>=0&&i<names.length);return names[i];};
 const width=version>=520&&!(flags&0x80000000)?36:28;
 const imports=Array.from({length:importsCount},(_,i)=>{const p=importsAt+i*width;return {name:name(p+20),outer:b.readInt32LE(p+16)};});
 function full(i,depth=0){assert(depth<40);if(i===0)return '';assert(i<0&&-i<=imports.length);const x=imports[-i-1];return (x.outer?full(x.outer,depth+1)+'.':'')+x.name;}
 return {names,imports:imports.map((_,i)=>full(-i-1))};
}
const root=process.argv[2];assert(root,'Provide a cooked content directory');
const wanted=['BP_NwiAuto','BP_NwiResources','BP_NwiPulse','WBP_NwiMarker','SG_NwiSettings','WBP_NwiSettings','M_NwiRedPulse','InitCave','InitSpacerig'];
const result=[],nativePackages=new Set(['/Script/CoreUObject','/Script/Engine','/Script/SlateCore','/Script/UMG']);
for(const base of wanted){const p=inspect(path.join(root,base+'.uasset'));for(const n of [...p.names,...p.imports])assert(!/NwiAuthoring|NwiValidation|FixtureWaveManager|BP_NwiVisualTest/.test(n),'Unexpected runtime reference: '+n);for(const n of p.imports)if(n.startsWith('/Script/'))assert(nativePackages.has(n.split('.')[0]),'Unexpected native module: '+n);result.push({asset:base,imports:p.imports});}
const auto=inspect(path.join(root,'BP_NwiAuto.uasset'));for(const field of ['NwiPoll','NativeAbi','NativeTime','RegionScale0'])assert(auto.names.includes(field),'Missing native handoff '+field);for(const old of ['ObserveEnemy','ActiveScriptedWaves','RecordSpawn'])assert(!auto.names.includes(old),'Obsolete approximate capture '+old);
console.log(JSON.stringify({passed:true,contentOnly:false,nativeAbi:590592,assets:result.length,packages:result}));
