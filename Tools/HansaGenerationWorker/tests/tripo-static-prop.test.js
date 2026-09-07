import assert from "node:assert/strict";
import {test} from "node:test";
import {mkdtemp,rm,readFile,writeFile} from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import {randomUUID} from "node:crypto";
import {TripoStaticPropProvider,createTripoProviderFromEnvironment} from "../src/providers/tripo-static-prop-provider.js";
import {staticPropProfile,validateStaticPropGlb,imageDimensions} from "../src/static-prop.js";
import {normalizeGenerationRequest} from "../src/request-contract.js";
import {JobStore} from "../src/job-store.js";
import {GenerationWorkerService} from "../src/worker-service.js";

export const profile={version:1,role:"HarborProp",stableId:"Prop.HarborCrate",heightCm:100,maximumTriangles:1000,maximumMaterials:2,maximumTextureSize:1024,forwardAxis:"+X",upAxis:"+Z",pivot:"bottom-center",collision:"box"};
const raw=()=>({schemaVersion:1,idempotencyKey:randomUUID(),providerId:"tripo",modelVersion:"v3.1-20260211",capability:"TextToMesh",intendedAssetRole:"HarborProp",prompt:"A plain wooden harbor crate",negativePrompt:"",inputArtifacts:[],rights:{acknowledged:true,declaration:"Original prompt and owned references"},budgets:{currency:"USD",maximumCostMinorUnits:100,maximumOutputBytes:65536},outputContract:{version:1,maximumArtifacts:1,allowedMediaTypes:["model/gltf-binary"]},parameters:{staticProp:profile},timeoutMs:5000,spendApproval:{approved:true,approvedBy:"Automation",approvedAt:"2026-09-06T00:00:00Z"}});
function glb(mutator=()=>{}) {
  // Original mathematical tetrahedron; a mock payload, never a claimed live result.
  const bin=Buffer.alloc(48);[0,0,0,1,0,0,0,1,0,0,0,1].forEach((x,i)=>bin.writeFloatLE(x,i*4));
  const d={asset:{version:"2.0"},scene:0,scenes:[{nodes:[0]}],nodes:[{mesh:0}],meshes:[{primitives:[{attributes:{POSITION:0},indices:1}]}],buffers:[{byteLength:60}],bufferViews:[{buffer:0,byteOffset:0,byteLength:48},{buffer:0,byteOffset:48,byteLength:12}],accessors:[{bufferView:0,componentType:5126,count:4,type:"VEC3",min:[0,0,0],max:[1,1,1]},{bufferView:1,componentType:5121,count:12,type:"SCALAR"}]};
  mutator(d,bin);
  let json=Buffer.from(JSON.stringify(d));json=Buffer.concat([json,Buffer.alloc((4-json.length%4)%4,32)]);
  const payload=Buffer.concat([bin,Buffer.from([0,1,2,0,3,1,0,2,3,1,3,2])]);
  const h=Buffer.alloc(20);h.write("glTF");h.writeUInt32LE(2,4);h.writeUInt32LE(28+json.length+payload.length,8);h.writeUInt32LE(json.length,12);h.writeUInt32LE(0x4e4f534a,16);
  const bh=Buffer.alloc(8);bh.writeUInt32LE(payload.length,0);bh.writeUInt32LE(0x004e4942,4);return Buffer.concat([h,json,bh,payload]);
}
const envelope=data=>new Response(JSON.stringify({code:0,data}),{headers:{"content-type":"application/json"}});
function harness(overrides={}) {
  const calls=[];
  const fetchImpl=async(url,options={})=>{
    calls.push({url:String(url),options});
    if(overrides.fetch)return overrides.fetch(String(url),options,calls);
    if(String(url).endsWith("/files"))return envelope({file_token:"file_mock"});
    if(options.method==="POST")return envelope({task_id:"task_mock"});
    if(String(url).includes("/tasks/"))return envelope({task_id:"task_mock",status:"success",credits_consumed:10,output:{model_url:"https://cdn.tripo3d.ai/mock/prop.glb?signed=do-not-retain"}});
    return new Response(overrides.bytes??glb(),{headers:{"content-type":"model/gltf-binary"}});
  };
  const provider=new TripoStaticPropProvider({apiKey:"test-private-api-key",estimatedCredits:10,minorUnitsPerCredit:2,fetchImpl});
  provider.pollIntervalMs=1;
  return {provider,calls};
}
async function service(t,provider,autoProcess=true) {
  const root=await mkdtemp(path.join(os.tmpdir(),"hansa-tripo-"));t.after(()=>rm(root,{recursive:true,force:true}));
  const store=new JobStore(path.join(root,"Saved/GenerationJobs"));
  const worker=new GenerationWorkerService({store,providers:[provider],projectRoot:root,pollIntervalMs:1,autoProcess});
  await worker.initialize();t.after(()=>worker.closeForRestart());return {root,store,worker};
}
test("Tripo is opt-in and validates pinned model, rights, spend and static-only contract",()=>{
  assert.equal(createTripoProviderFromEnvironment({}),null);
  assert.throws(()=>createTripoProviderFromEnvironment({HANSA_TRIPO_ENABLED:"1"}));
  const {provider}=harness();
  for(const change of [
    r=>r.modelVersion="latest",r=>r.capability="AutoRig",r=>r.rights.acknowledged=false,
    r=>r.parameters.staticProp={...profile,maximumTriangles:50001},r=>r.parameters.retopology=true,
    r=>r.outputContract.allowedMediaTypes=["model/fbx"],r=>r.privacy={promptPrivate:true},r=>r.prompt="x".repeat(1025),
    r=>r.parameters.staticProp={...profile,upAxis:"+Y"},
  ]) {const r=raw();change(r);assert.throws(()=>provider.validateRequest(normalizeGenerationRequest(r).safeRequest));}
  const r=raw();r.spendApproval.approved=false;assert.throws(()=>normalizeGenerationRequest(r),/spend approval/);
  assert.equal(provider.getCapabilities().capabilities[0].cancellationScope,"local");
});
test("static GLB validates accessors, finite geometry, topology, buffers, images and bounded profiles",()=>{
  assert.equal(validateStaticPropGlb(glb(),profile).triangles,4);
  for(const change of [
    d=>d.nodes[0].scale=[2,1,1],d=>d.animations=[{}],d=>d.meshes[0].primitives[0].mode=1,
    d=>d.accessors[0].count=100,d=>d.accessors[0].sparse={},d=>d.bufferViews[0].byteOffset=-1,
    (_d,b)=>b.writeFloatLE(NaN,0),d=>d.images=[{uri:"https://evil.invalid/a.png"}],
    d=>d.materials=[{},{},{}],d=>d.extensionsUsed=["KHR_draco_mesh_compression"],
    d=>d.textures=[{source:99}],d=>d.meshes[0].primitives[0].material=999,
  ]) assert.throws(()=>validateStaticPropGlb(glb(change),profile));
  assert.throws(()=>validateStaticPropGlb(glb(),{...profile,maximumTriangles:1}));
  assert.throws(()=>validateStaticPropGlb(Buffer.from("Kaydara FBX Binary"),profile));
  assert.throws(()=>imageDimensions(Buffer.from("not image"),"image/png"));
  assert.throws(()=>staticPropProfile({...profile,rig:true}));
});
test("text generation reaches immutable Review, retains profile and keeps signed URLs and keys out of manifests",async t=>{
  const {provider,calls}=harness(),{worker,store,root}=await service(t,provider);
  const submitted=await worker.submit(raw());await worker.waitForIdle();
  const job=await store.getJob(submitted.job.jobId);assert.equal(job.status,"Review",JSON.stringify(job.errors));
  assert.equal(job.actualUsage.actualMinorUnits,20);assert.equal(job.submissionIntentRevision,1);
  const post=calls.find(c=>c.options.method==="POST");const body=JSON.parse(post.options.body);
  assert.equal(body.quad,false);assert.equal(body.smart_low_poly,false);assert.equal(body.auto_size,true);assert.equal(body.export_orientation,"+x");
  const download=calls.find(c=>c.url.includes("cdn.tripo3d.ai"));assert.equal(download.options.headers,undefined);assert.equal(download.options.redirect,"error");
  const retained=await worker.retainMedia(job.jobId,0);assert.equal(retained.validation.staticProp.triangles,4);
  const manifest=await readFile(path.join(root,retained.manifestPath),"utf8");
  assert.ok(!manifest.includes("signed=")&&!manifest.includes("test-private-api-key"));
  assert.ok(manifest.includes('"staticPropContractVersion":1'));
});
test("image upload uses verified private bytes and never a user-controlled download URL",async t=>{
  const png=Buffer.alloc(33);Buffer.from([137,80,78,71,13,10,26,10]).copy(png);png.write("IHDR",12);png.writeUInt32BE(64,16);png.writeUInt32BE(64,20);
  const r=raw();r.capability="ImageToMesh";r.inputArtifacts=[{role:"ReferenceImage",mediaType:"image/png",rightsDeclaration:"Original fixture",contentBase64:png.toString("base64")}];
  const {provider,calls}=harness(),{worker,store}=await service(t,provider);
  const {job}=await worker.submit(r);await worker.waitForIdle();assert.equal((await worker.get(job.jobId)).status,"Review");
  const upload=calls.find(c=>c.url.endsWith("/files"));assert.ok(upload.options.body instanceof FormData);
  assert.deepEqual(Buffer.from(await upload.options.body.get("file").arrayBuffer()),png);
  const generation=calls.find(c=>c.url.endsWith("/generation/image-to-model"));assert.equal(JSON.parse(generation.options.body).input,"file_mock");
  await writeFile(path.join(store.jobDirectory(job.jobId),"private/input-001.bin"),"tampered");
  await assert.rejects(store.readPrivateInput(job.jobId,0),/size changed/);
});
test("no automatic billable replay after restart in the submission ambiguity window",async t=>{
  const {provider,calls}=harness(),{worker,store}=await service(t,provider,false);
  const {job}=await worker.submit(raw());
  await store.transition(job.jobId,"Running",{});
  await store.updateJob(job.jobId,s=>{s.submissionIntentRevision=1;});
  await worker.schedule(job.jobId);
  const failed=await worker.get(job.jobId);assert.equal(failed.status,"Failed");assert.equal(failed.errors[0].code,"SubmissionOutcomeUnknown");assert.equal(calls.length,0);
  await assert.rejects(worker.retry(job.jobId,"retry-1"),/spend approval/);
  await worker.retry(job.jobId,"retry-1","Reviewed provider account",{approved:true,approvedBy:"Automation",approvedAt:"2026-09-06"});
  await worker.schedule(job.jobId);assert.equal((await worker.get(job.jobId)).status,"Review");
  assert.equal(calls.filter(c=>c.options.method==="POST").length,1);
});
test("restart with persisted task ID resumes polling without a new submission",async t=>{
  const {provider,calls}=harness(),{worker,store}=await service(t,provider,false);
  const {job}=await worker.submit(raw());await store.transition(job.jobId,"Running",{providerJobId:"task_mock",providerState:{},submissionIntentRevision:1});
  await worker.schedule(job.jobId);assert.equal((await worker.get(job.jobId)).status,"Review");assert.equal(calls.filter(c=>c.options.method==="POST").length,0);
});
test("cancel aborts an in-flight task download and reports local-only billing semantics",async t=>{
  let began;const downloading=new Promise(resolve=>began=resolve);
  const {provider}=harness({fetch:async(url,options)=>{
    if(options.method==="POST")return envelope({task_id:"task_mock"});
    if(url.includes("/tasks/"))return envelope({task_id:"task_mock",status:"success",credits_consumed:10,output:{model_url:"https://cdn.tripo3d.ai/p.glb"}});
    began();return new Promise((_resolve,reject)=>options.signal.addEventListener("abort",()=>reject(new Error("aborted")),{once:true}));
  }});
  const {worker}=await service(t,provider);const {job}=await worker.submit(raw());await downloading;await worker.cancel(job.jobId);await worker.waitForIdle();
  const cancelled=await worker.get(job.jobId);assert.equal(cancelled.status,"Cancelled");assert.match(cancelled.errors[0].message,/may continue and incur charges/);
});
test("safe polling retries rate limits but billable POST is attempted only once",async()=>{
  let count=0;const {provider}=harness({fetch:async(_url,options)=>{count++;return options.method==="GET"&&count===3?envelope({task_id:"task_mock",status:"running",progress:30}):new Response("rate limited",{status:429});}});
  const context={request:normalizeGenerationRequest(raw()).safeRequest,remainingTimeoutMs:5000};
  assert.equal((await provider.poll("task_mock",{},context)).status,"pending");assert.equal(count,3);
  count=0;await assert.rejects(provider.submit(context.request,context),/HTTP 429/);assert.equal(count,1);
});
test("untrusted URLs, FBX, malformed output, excess actual cost and downloads fail closed",async t=>{
  for(const variant of ["url","fbx","bytes","cost","identity","malformed","oversize"]) {
    const {provider}=harness({fetch:async(url,options)=>{
      if(options.method==="POST")return envelope({task_id:"task_mock"});
      if(url.includes("/tasks/"))return envelope({task_id:variant==="identity"?"wrong":"task_mock",status:variant==="malformed"?"made-up":"success",credits_consumed:variant==="cost"?999:10,output:{model_url:variant==="url"?"https://127.0.0.1/p.glb":variant==="fbx"?"https://cdn.tripo3d.ai/p.fbx":"https://cdn.tripo3d.ai/p.glb"}});
      return new Response(variant==="bytes"?Buffer.from("bad"):glb(),{headers:variant==="oversize"?{"content-length":"999999"}:{}});
    }});
    const {worker}=await service(t,provider);const {job}=await worker.submit(raw());await worker.waitForIdle();assert.equal((await worker.get(job.jobId)).status,"Failed",variant);
  }
});
test("revised rate card cannot bypass retry spend ceiling",async t=>{
  const {provider}=harness({bytes:Buffer.from("bad")}),{worker}=await service(t,provider);
  const {job}=await worker.submit(raw());await worker.waitForIdle();provider.estimatedCredits=999;
  await assert.rejects(worker.retry(job.jobId,"retry-expensive","Changed rate",{approved:true,approvedBy:"Automation",approvedAt:"2026-09-06"}),/ceiling/);
  assert.equal((await worker.get(job.jobId)).revision,1);
});

test("identity hierarchy is accepted and cyclic or multiply-instanced scenes are rejected",()=>{
  assert.equal(validateStaticPropGlb(glb(d=>{d.nodes=[{children:[1]},{mesh:0,scale:[1,1,1],translation:[0,0,0],rotation:[0,0,0,1]}];}),profile).triangles,4);
  assert.throws(()=>validateStaticPropGlb(glb(d=>{d.nodes[0].children=[0];}),profile));
  assert.throws(()=>validateStaticPropGlb(glb(d=>{d.nodes.push({mesh:0});d.scenes[0].nodes.push(1);}),profile));
});
test("deadline expiry before submission does not spend and pending tasks expire",async t=>{
  const {provider,calls}=harness(),{worker,store}=await service(t,provider,false);
  const {job}=await worker.submit(raw());await store.updateJob(job.jobId,s=>{s.deadlineAt="2000-01-01T00:00:00Z";});
  await worker.schedule(job.jobId);assert.equal((await worker.get(job.jobId)).status,"Expired");assert.equal(calls.length,0);
  const next=await worker.submit(raw());await store.transition(next.job.jobId,"Running",{providerJobId:"task_mock",providerState:{},deadlineAt:"2000-01-01T00:00:00Z"});
  await worker.schedule(next.job.jobId);assert.equal((await worker.get(next.job.jobId)).status,"Expired");assert.equal(calls.length,0);
});
test("cost overrun remains in the immutable failure manifest",async t=>{
  const {provider}=harness({fetch:async(_url,options)=> options.method==="POST"?envelope({task_id:"task_mock"}):envelope({task_id:"task_mock",status:"success",credits_consumed:101,output:{model_url:"https://cdn.tripo3d.ai/p.glb"}})});
  const {worker,store}=await service(t,provider);
  const {job}=await worker.submit(raw());await worker.waitForIdle();const failed=await store.getJob(job.jobId);
  assert.equal(failed.errors[0].details.observedUsage.actualMinorUnits,202);
  const manifest=JSON.parse(await readFile(path.join(store.jobDirectory(job.jobId),failed.manifests[0].relativePath)));
  assert.equal(manifest.errors[0].details.observedUsage.actualUsage,101);
});
test("CLI refuses missing reviewer or stale cost, then submits through the authenticated mock pipe",async t=>{
  const {WorkerPipeServer}=await import("../src/pipe-server.js");
  const {WorkerRequestHandler}=await import("../src/request-handler.js");
  const {execFile}=await import("node:child_process");const {promisify}=await import("node:util");const {fileURLToPath}=await import("node:url");
  const {provider,calls}=harness(),{worker,root}=await service(t,provider);
  const pipeName="hansa-tripo-cli-"+randomUUID(),authenticationToken="mock-cli-session-token";
  const server=new WorkerPipeServer({pipeName,handler:new WorkerRequestHandler({service:worker,authenticationToken})});await server.listen();t.after(()=>server.close());
  const file=path.join(root,"request.json");await writeFile(file,JSON.stringify(raw()));
  const cli=fileURLToPath(new URL("../scripts/tripo.js",import.meta.url));
  const run=(...args)=>promisify(execFile)(process.execPath,[cli,...args],{env:{...process.env,HANSA_GENERATION_WORKER_PIPE:pipeName,HANSA_GENERATION_WORKER_TOKEN:authenticationToken}});
  const estimate=JSON.parse((await run("estimate",file)).stdout);assert.equal(estimate.estimate.estimatedMinorUnits,20);
  await assert.rejects(run("submit",file,"--confirm-cost=20"),/approve-spend/);
  await assert.rejects(run("submit",file,"--confirm-cost=1","--approve-spend=Automation"),/confirm-cost=20/);
  assert.equal(calls.length,0);
  const submitted=JSON.parse((await run("submit",file,"--confirm-cost=20","--approve-spend=Automation")).stdout);
  await worker.waitForIdle();assert.equal((await worker.get(submitted.job.jobId)).status,"Review");
});
