// Summarize real probe timing without pretending a sampled native handoff is a GPU-rendered frame.
const fs=require('node:fs');
const file=process.argv[2];if(!file)throw Error('Usage: node scripts/Analyze-NativeTiming.cjs <probe-log.jsonl>');
const rows=fs.readFileSync(file,'utf8').replace(/^\uFEFF/,'').trim().split(/\r?\n/).map(s=>JSON.parse(s));
const last=[...rows].reverse().find(r=>Array.isArray(r.timing)&&r.timing[0]>0);
if(!last){console.log(JSON.stringify({samples:0,conclusion:'No eligible natural-wave timing samples; do not infer lead time or working capture.'},null,2));process.exitCode=2;}
else{const [samples,centerMinMs,centerMaxMs,queueMaxMs,handoffMaxMs,excluded]=last.timing;console.log(JSON.stringify({samples,centerMinMs,centerMaxMs,queueMaxMs,handoffMaxMs,excluded,captureFault:last.capture_fault,automaticFault:last.auto_fault,conclusion:'Measured after actual center selection. This does not predict future RNG/navigation, prove a 1-5 second lead, or measure the first GPU frame.'},null,2));}
