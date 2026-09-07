import assert from "node:assert/strict";
import {test} from "node:test";
import {readFile,writeFile,mkdtemp,rm} from "node:fs/promises";
import path from "node:path";
import os from "node:os";
import {randomUUID} from "node:crypto";
import {ElevenLabsAudioProvider,createElevenLabsProviderFromEnvironment} from "../src/providers/elevenlabs-audio-provider.js";
import {audioProfile,validateAudioTake} from "../src/audio-take.js";
import {decodeMp3} from "../src/audio-decoder.js";
import {normalizeGenerationRequest} from "../src/request-contract.js";
import {JobStore} from "../src/job-store.js";
import {GenerationWorkerService} from "../src/worker-service.js";
const mp3=await readFile(new URL("../../../Tests/Golden/Media/audio-take.mp3",import.meta.url));
const wav=await readFile(new URL("../../../Tests/Golden/Media/audio-take.wav",import.meta.url));
const profile=(speech=false)=>({version:1,kind:speech?"Speech":"SFX",stableId:speech?"Dialogue.DockGreeting":"SFX.HarborBell",variants:2,minimumDurationMs:100,maximumDurationMs:2000,maximumLeadingSilenceMs:100,maximumTrailingSilenceMs:150,subtitle:speech?"Welcome to the harbor.":"",speakerId:speech?"Speaker.Dockworker":"",language:speech?"en":"",loop:false});
const request=(speech=false)=>({schemaVersion:1,idempotencyKey:randomUUID(),providerId:"elevenlabs",modelVersion:speech?"eleven_flash_v2_5":"eleven_text_to_sound_v2",capability:speech?"TextToSpeech":"TextToSoundEffect",intendedAssetRole:speech?"SpeechLine":"HarborSFX",prompt:speech?"Welcome to the harbor.":"One small harbor bell, no speech",inputArtifacts:[],rights:{acknowledged:true,declaration:"Original fixture and authorized library voice",voiceAcknowledged:true,englishTextAcknowledged:true},budgets:{currency:"USD",maximumCostMinorUnits:100,maximumOutputBytes:1048576},outputContract:{version:1,maximumArtifacts:2,allowedMediaTypes:["audio/wav"]},parameters:speech?{audioTake:profile(true),voiceId:"ApprovedVoice",voiceSettings:{stability:0.5,similarity_boost:0.7,style:0}}:{audioTake:profile(),durationSeconds:1,promptInfluence:0.3},timeoutMs:10000,spendApproval:{approved:true,approvedBy:"Automation",approvedAt:"2026-09-06"}});
function provider(options={}) {
 const calls=[];const adapter=new ElevenLabsAudioProvider({apiKey:"private-mock-credential",allowedVoiceIds:["ApprovedVoice"],sfxUnitsPerSecond:100,speechUnitsPerCharacter:1,minorUnitsPer1000:10,decoder:async()=>wav,fetchImpl:async(url,init)=>{calls.push({url,init});return new Response(mp3,{headers:{"content-type":"audio/mpeg","character-cost":"100","request-id":"request_"+calls.length}});},...options});
 return {adapter,calls};
}
async function setup(t,adapter,autoProcess=true){
 const root=await mkdtemp(path.join(os.tmpdir(),"hansa-eleven-"));t.after(()=>rm(root,{recursive:true,force:true}));
 const store=new JobStore(path.join(root,"Saved/GenerationJobs"));const worker=new GenerationWorkerService({store,providers:[adapter],projectRoot:root,autoProcess});await worker.initialize();t.after(()=>worker.closeForRestart());return {root,store,worker};
}
test("audio profile and PCM validation reject clipping, silence, duration, channels and malformed data",()=>{
 assert.equal(validateAudioTake(wav,profile()).sampleRate,44100);
 for(const p of [{...profile(),variants:3},{...profile(),loop:true},{...profile(true),language:"sv"},{...profile(true),subtitle:"[speaker] hello"},{...profile(),extra:true}])assert.throws(()=>audioProfile(p));
 const clipped=Buffer.from(wav);clipped.writeInt16LE(32767,44);assert.throws(()=>validateAudioTake(clipped,profile()),/clipped/);
 const quiet=Buffer.from(wav);quiet.fill(0,44);assert.throws(()=>validateAudioTake(quiet,profile()),/silent/);
 const leading=Buffer.from(wav);leading.fill(0,44,44+44100);assert.throws(()=>validateAudioTake(leading,profile()),/silence/);
 const trailing=Buffer.from(wav);trailing.fill(0,44+44100);assert.throws(()=>validateAudioTake(trailing,profile()),/silence/);
 assert.throws(()=>validateAudioTake(wav,{...profile(),maximumDurationMs:500}),/duration/);
 const rate=Buffer.from(wav);rate.writeUInt32LE(8000,24);assert.throws(()=>validateAudioTake(rate,profile()),/rate/);
 assert.throws(()=>validateAudioTake(wav.subarray(0,100),profile()),/container/);
});
test("actual bounded FFmpeg decoder decodes MP3 and rejects malformed input",async()=>{
 const decoded=await decodeMp3(mp3);const qa=validateAudioTake(decoded,profile());
 assert.equal(qa.channels,1);assert.equal(qa.sampleRate,44100);assert.ok(qa.durationMs>=1000&&qa.durationMs<1200);
 await assert.rejects(decodeMp3(Buffer.from("not MP3")),/decoding failed/);
});
test("adapter is opt-in and rejects missing rights, voice permission, unsupported capabilities or third takes",()=>{
 assert.equal(createElevenLabsProviderFromEnvironment({}),null);assert.throws(()=>createElevenLabsProviderFromEnvironment({HANSA_ELEVENLABS_ENABLED:"1"}));
 const {adapter}=provider();
 for(const mutate of [r=>r.rights.acknowledged=false,r=>r.rights.voiceAcknowledged=false,r=>r.rights.englishTextAcknowledged=false,r=>r.parameters.voiceId="Unapproved",r=>r.parameters.audioTake.variants=3,r=>r.parameters.audioTake.subtitle="Different",r=>r.parameters.voiceSettings.clone=true,r=>r.capability="TextToDialogue",r=>r.modelVersion="latest",r=>r.privacy={promptPrivate:true}]){
 const r=request(true);mutate(r);assert.throws(()=>adapter.validateRequest(normalizeGenerationRequest(r).safeRequest));
 }
});
for(const speech of [false,true])test((speech?"speech":"SFX")+" creates two immutable takes with original MP3 and canonical WAV provenance",async t=>{
 const {adapter,calls}=provider(),{worker,store,root}=await setup(t,adapter);const {job}=await worker.submit(request(speech));await worker.waitForIdle();
 const done=await worker.get(job.jobId);assert.equal(done.status,"Review",JSON.stringify(done.errors));assert.equal(done.outputs.length,2);assert.equal(calls.length,2);
 for(const c of calls){assert.ok(c.url.endsWith("output_format=mp3_44100_128"));assert.equal(c.init.redirect,"error");const body=JSON.parse(c.init.body);assert.equal(body.model_id,request(speech).modelVersion);if(speech)assert.equal(body.language_code,"en");else assert.equal(body.loop,false);}
 for(let i=0;i<2;i++){const source=await worker.retainMedia(job.jobId,i);assert.equal(source.validation.audioTake.clippedSamples,0);assert.deepEqual(await readFile(path.join(root,path.dirname(source.sourcePath),"provider-original.mp3")),mp3);const text=await readFile(path.join(root,source.manifestPath),"utf8");assert.ok(!text.includes("private-mock-credential"));assert.equal(JSON.parse(text).provenance.takes.length,2);}
 const source=await worker.retainMedia(job.jobId,0);await rm(path.join(root,"Saved"),{recursive:true});assert.deepEqual(await readFile(path.join(root,source.sourcePath)),wav);
});
test("no spend when estimate exceeds budget; retry requires new explicit approval",async t=>{
 const {adapter,calls}=provider(),{worker}=await setup(t,adapter);const r=request();r.budgets.maximumCostMinorUnits=0;await assert.rejects(worker.submit(r),/budget/);assert.equal(calls.length,0);
});
test("restart ambiguity cannot duplicate a billable batch",async t=>{
 const {adapter,calls}=provider(),{worker,store}=await setup(t,adapter,false);const {job}=await worker.submit(request());
 await store.transition(job.jobId,"Running",{});await store.updateJob(job.jobId,s=>{s.submissionIntentRevision=1;});
 await worker.schedule(job.jobId);assert.equal((await worker.get(job.jobId)).errors[0].code,"SubmissionOutcomeUnknown");assert.equal(calls.length,0);
 await assert.rejects(worker.retry(job.jobId,"next"),/spend approval/);
 await worker.retry(job.jobId,"next","Account inspected",{approved:true,approvedBy:"Automation",approvedAt:"2026-09-06"});await worker.schedule(job.jobId);assert.equal((await worker.get(job.jobId)).status,"Review");
});
test("known completed batch resumes without additional generation",async t=>{
 const {adapter,calls}=provider(),{worker,store}=await setup(t,adapter,false);const {job}=await worker.submit(request());
 const internal=await store.getJob(job.jobId);const completed=await adapter.submit(internal.request,{jobId:job.jobId,revision:1,remainingTimeoutMs:10000,retainProviderSource:(...args)=>store.retainProviderSource(job.jobId,...args)});
 await store.transition(job.jobId,"Running",{providerJobId:completed.providerJobId,providerState:completed.state,submissionIntentRevision:1});
 await worker.schedule(job.jobId);assert.equal((await worker.get(job.jobId)).status,"Review");assert.equal(calls.length,2);
});
test("billing overrun, missing usage, HTTP failures, malformed media and size overrun fail closed",async t=>{
 for(const kind of ["cost","usage","http","mime","size","decode"]){
 let calls=0;const {adapter}=provider({fetchImpl:async()=>{calls++;return new Response(mp3,{status:kind==="http"?429:200,headers:{"content-type":kind==="mime"?"application/json":"audio/mpeg",...(kind==="usage"?{}:{"character-cost":kind==="cost"?"99999":"100"}),...(kind==="size"?{"content-length":"9999999"}:{})}});},...(kind==="decode"?{decoder:async()=>Buffer.from("bad WAV")}:{})});
 const {worker}=await setup(t,adapter);const {job}=await worker.submit(request());await worker.waitForIdle();const failed=await worker.get(job.jobId);assert.equal(failed.status,"Failed",kind);assert.equal(calls,1);if(kind==="cost")assert.equal(failed.errors[0].details.observedUsage.actualUsage,99999);
 }
});
test("second-take failure preserves first-take hash and charges without publishing partial Review",async t=>{
 let calls=0;const {adapter}=provider({fetchImpl:async()=>++calls===1?new Response(mp3,{headers:{"content-type":"audio/mpeg","character-cost":"100"}}):new Response("failure",{status:500})});
 const {worker,store}=await setup(t,adapter);const {job}=await worker.submit(request());await worker.waitForIdle();const failed=await worker.get(job.jobId);assert.equal(failed.status,"Failed");assert.equal(failed.errors[0].details.takes.length,1);assert.match(failed.errors[0].details.takes[0].source.sha256,/^[a-f0-9]{64}$/);assert.deepEqual(await readFile(path.join(store.jobDirectory(job.jobId),"output/original-001.mp3")),mp3);
});
test("cancel aborts in-flight generation and records local billing semantics",async t=>{
 let entered;const started=new Promise(r=>entered=r);
 const {adapter}=provider({fetchImpl:async(_url,{signal})=>{entered();return new Promise((_r,reject)=>signal.addEventListener("abort",()=>reject(new Error("abort")),{once:true}));}});
 const {worker}=await setup(t,adapter);const {job}=await worker.submit(request());await started;await worker.cancel(job.jobId);await worker.waitForIdle();const cancelled=await worker.get(job.jobId);assert.equal(cancelled.status,"Cancelled");assert.match(cancelled.errors[0].message,/incurred charges/);
});

test("CLI confirms total spend and retains the selected second take",async t=>{
 const {WorkerPipeServer}=await import("../src/pipe-server.js");const {WorkerRequestHandler}=await import("../src/request-handler.js");
 const {execFile}=await import("node:child_process");const {promisify}=await import("node:util");const {fileURLToPath}=await import("node:url");
 const {adapter,calls}=provider(),{worker,root}=await setup(t,adapter);
 const pipeName="hansa-audio-cli-"+randomUUID(),authenticationToken="mock-audio-cli-token";
 const server=new WorkerPipeServer({pipeName,handler:new WorkerRequestHandler({service:worker,authenticationToken})});await server.listen();t.after(()=>server.close());
 const file=path.join(root,"request.json");await writeFile(file,JSON.stringify(request()));
 const cli=fileURLToPath(new URL("../scripts/elevenlabs.js",import.meta.url));
 const run=(...args)=>promisify(execFile)(process.execPath,[cli,...args],{env:{...process.env,HANSA_GENERATION_WORKER_PIPE:pipeName,HANSA_GENERATION_WORKER_TOKEN:authenticationToken}});
 const estimate=JSON.parse((await run("estimate",file)).stdout);assert.equal(estimate.estimate.estimatedMinorUnits,2);
 await assert.rejects(run("submit",file,"--confirm-cost=2"),/approve-spend/);await assert.rejects(run("submit",file,"--confirm-cost=1","--approve-spend=Automation"),/confirm-cost=2/);assert.equal(calls.length,0);
 const submitted=JSON.parse((await run("submit",file,"--confirm-cost=2","--approve-spend=Automation")).stdout);await worker.waitForIdle();
 const retained=JSON.parse((await run("retain",submitted.job.jobId,"--take=2")).stdout);assert.equal(retained.outputIndex,1);
 await assert.rejects(run("retain",submitted.job.jobId,"--take=3"),/take=1 or --take=2/);
});
test("original MP3 tampering prevents durable source retention",async t=>{
 const {adapter}=provider(),{worker,store}=await setup(t,adapter);const {job}=await worker.submit(request());await worker.waitForIdle();
 await writeFile(path.join(store.jobDirectory(job.jobId),"output/original-001.mp3"),"tampered");
 await assert.rejects(worker.retainMedia(job.jobId,0),/Original provider source integrity/);
});
test("real decoder-backed adapter produces reviewable audio",async t=>{
 const {adapter}=provider({decoder:decodeMp3}),{worker}=await setup(t,adapter);const r=request();r.parameters.audioTake.variants=1;r.outputContract.maximumArtifacts=1;
 const {job}=await worker.submit(r);await worker.waitForIdle();const result=await worker.get(job.jobId);assert.equal(result.status,"Review",JSON.stringify(result.errors));
});

test("missing decoder fails before any billable request",async t=>{
 const {adapter,calls}=provider({decoder:decodeMp3,decoderExecutable:"hansa-nonexistent-audio-decoder"}),{worker}=await setup(t,adapter);
 const {job}=await worker.submit(request());await worker.waitForIdle();assert.equal((await worker.get(job.jobId)).errors[0].code,"AudioDecoderUnavailable");assert.equal(calls.length,0);
});
test("cancellation after a billed first take keeps observed cost in its manifest",async t=>{
 let calls=0,entered;const second=new Promise(r=>entered=r);
 const {adapter}=provider({fetchImpl:async(_url,{signal})=>{
  if(++calls===1)return new Response(mp3,{headers:{"content-type":"audio/mpeg","character-cost":"100"}});
  entered();return new Promise((_r,reject)=>signal.addEventListener("abort",()=>reject(new Error("aborted")),{once:true}));
 }});
 const {worker,store}=await setup(t,adapter);const {job}=await worker.submit(request());await second;await worker.cancel(job.jobId);await worker.waitForIdle();
 const result=await store.getJob(job.jobId);assert.equal(result.status,"Cancelled");assert.equal(result.errors[0].details.observedUsage.actualUsage,100);
 const manifest=JSON.parse(await readFile(path.join(store.jobDirectory(job.jobId),result.manifests[0].relativePath)));
 assert.equal(manifest.errors[0].details.takes.length,1);
});
