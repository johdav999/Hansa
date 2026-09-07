import { setTimeout as delay } from "node:timers/promises";
import { WorkerError } from "../errors.js";
import { sha256 } from "../canonical-json.js";
import { staticPropProfile, validateStaticPropGlb, imageDimensions } from "../static-prop.js";

const API = "https://openapi.tripo3d.ai/v3";
const MODEL = "v3.1-20260211";
const fail = (code,message,retryable=false) => new WorkerError(code,message,{retryable});
function taskId(id) { if(typeof id!=="string"||!/^[-A-Za-z0-9_]{1,128}$/.test(id)) throw fail("MalformedProviderOutput","Invalid Tripo task identity."); return id; }
async function bounded(response,maximum) {
  if(Number(response.headers.get("content-length"))>maximum) {await response.body?.cancel();throw fail("OutputBudgetExceeded","Tripo response exceeds byte budget.");}
  const chunks=[];let size=0;const reader=response.body?.getReader();
  if(!reader) throw fail("MalformedProviderOutput","Tripo returned no response body.");
  try {while(true) {const {done,value}=await reader.read();if(done)break;size+=value.length;if(size>maximum)throw fail("OutputBudgetExceeded","Tripo response exceeds byte budget.");chunks.push(Buffer.from(value));}}
  catch(error){await reader.cancel().catch(()=>{});throw error;} finally {reader.releaseLock();}
  return Buffer.concat(chunks);
}
export class TripoStaticPropProvider {
  constructor({apiKey,modelVersion=MODEL,estimatedCredits,minorUnitsPerCredit,currency="USD",downloadHosts=["cdn.tripo3d.ai"],fetchImpl=globalThis.fetch}={}) {
    if(!apiKey||modelVersion!==MODEL||!Number.isSafeInteger(estimatedCredits)||estimatedCredits<1||estimatedCredits>10000||!Number.isSafeInteger(minorUnitsPerCredit)||minorUnitsPerCredit<1||minorUnitsPerCredit>100000||!/^[A-Z]{3}$/.test(currency)) throw fail("TripoConfigurationRequired","Tripo requires credentials, a pinned model and an explicit conservative rate card.");
    if(!Array.isArray(downloadHosts)||!downloadHosts.length||downloadHosts.some(h=>typeof h!=="string"||!/^([a-z0-9-]+\.)*tripo3d\.ai$/.test(h))) throw fail("TripoConfigurationRequired","Download hosts must be explicitly trusted Tripo HTTPS hostnames.");
    Object.assign(this,{apiKey,modelVersion,estimatedCredits,minorUnitsPerCredit,currency,downloadHosts,fetchImpl});
    this.providerId="tripo";this.adapterVersion="1.0.0";this.pollIntervalMs=1500;this.requiresSubmissionGuard=true;
    this.cancellationMessage="Cancelled locally. Tripo exposes no documented remote cancellation endpoint; the remote task may continue and incur charges.";
  }
  getCapabilities() {return {providerId:this.providerId,adapterVersion:this.adapterVersion,models:[{modelVersion:this.modelVersion,pinned:true}],capabilities:["TextToMesh","ImageToMesh"].map(capability=>({capability,asynchronous:true,supportsCancellation:true,cancellationScope:"local",supportsSeed:true,inputMediaTypes:capability==="ImageToMesh"?["image/png","image/jpeg"]:[],outputMediaTypes:["model/gltf-binary"]})),limitations:[this.cancellationMessage,"GLB only; FBX, rigging, animation, retopology and TRELLIS are excluded.","No automatic replay of billable submission. Estimates do not impose a remote account spending cap."]};}
  validateRequest(r) {
    if(r.providerId!=="tripo"||r.modelVersion!==this.modelVersion||!["TextToMesh","ImageToMesh"].includes(r.capability)||r.intendedAssetRole!=="HarborProp") throw fail("ProviderRequestRejected","Select the pinned Tripo text/image static HarborProp workflow.");
    staticPropProfile(r.parameters.staticProp);
    if(Object.keys(r.parameters).some(k=>k!=="staticProp")||r.prompt.length>1024||r.negativePrompt.length>255||sha256(r.prompt)!==r.promptSha256||sha256(r.negativePrompt)!==r.negativePromptSha256) throw fail("ProviderRequestRejected","Use a non-private unredacted prompt of at most 1024 characters, negative prompt at most 255, and only the staticProp parameters.");
    if(r.rights.acknowledged!==true||typeof r.rights.declaration!=="string"||!r.rights.declaration.trim()) throw fail("ProviderRequestRejected","Acknowledge and describe rights before provider submission.");
    if(r.outputContract.maximumArtifacts!==1||r.outputContract.allowedMediaTypes.length!==1||r.outputContract.allowedMediaTypes[0]!=="model/gltf-binary"||r.budgets.maximumOutputBytes>64*1024*1024||r.timeoutMs<1000) throw fail("ProviderRequestRejected","Request one GLB of at most 64 MiB with a bounded timeout of at least one second.");
    if(r.seed!==null&&r.seed>2147483647) throw fail("ProviderRequestRejected","Tripo seed must fit a signed 32-bit integer.");
    if(r.inputArtifacts.length!==(r.capability==="ImageToMesh"?1:0)||r.inputArtifacts.some(i=>!["image/png","image/jpeg"].includes(i.mediaType)||i.sizeBytes>6*1024*1024)) throw fail("ProviderRequestRejected","Image-to-mesh requires exactly one rights-declared PNG or JPEG, at most 6 MiB.");
    return {valid:true};
  }
  estimateCost() {return {currency:this.currency,estimatedMinorUnits:this.estimatedCredits*this.minorUnitsPerCredit,usageUnit:"tripo-credit",estimatedUsage:this.estimatedCredits};}
  async api(route,{method="GET",body,context={}}={}) {
    const signal=AbortSignal.any([AbortSignal.timeout(Math.max(1,Math.min(context.remainingTimeoutMs??30000,30000))),...(context.signal?[context.signal]:[])]);
    // Retry only safe reads; a failed POST can already have spent credits.
    for(let attempt=0;attempt<3;attempt++) {
      try {
        const response=await this.fetchImpl(API+route,{method,headers:{Authorization:"Bearer "+this.apiKey,...(body&&!(body instanceof FormData)?{"Content-Type":"application/json"}:{})},body:body instanceof FormData?body:body?JSON.stringify(body):undefined,redirect:"error",signal});
        if(!response.ok) {
          await response.body?.cancel();
          if(method==="GET"&&[429,500,502,503,504].includes(response.status)&&attempt<2) {await delay(250*(attempt+1),undefined,{signal});continue;}
          throw fail("TripoHttpFailure","Tripo request failed with HTTP "+response.status+".",response.status===429||response.status>=500);
        }
        let json;try{json=JSON.parse((await bounded(response,1024*1024)).toString("utf8"));}catch(error){if(error instanceof WorkerError)throw error;throw fail("MalformedProviderOutput","Invalid Tripo JSON response.");}
        if(json.code!==0||!json.data||typeof json.data!=="object") throw fail("TripoRejected","Tripo rejected the operation. Review the task in the provider account before retrying.");
        return json.data;
      } catch(error) {if(error instanceof WorkerError)throw error;throw fail(method==="POST"?"SubmissionOutcomeUnknown":"TripoTransportFailure",method==="POST"?"Tripo submission outcome is unknown. Inspect the provider account before approving another spend.":"Tripo request interrupted or timed out.",true);}
    }
  }
  async submit(r,context) {
    this.validateRequest(r);
    const body={model:this.modelVersion,texture:true,pbr:true,texture_quality:"standard",geometry_quality:"standard",auto_size:true,quad:false,smart_low_poly:false,generate_parts:false,export_orientation:"+x",export_uv:true,face_limit:r.parameters.staticProp.maximumTriangles};
    if(r.seed!==null)body.model_seed=r.seed;
    if(r.capability==="TextToMesh"){body.prompt=r.prompt;if(r.negativePrompt)body.negative_prompt=r.negativePrompt;}
    else {
      const bytes=await context.readInput(0);imageDimensions(bytes,r.inputArtifacts[0].mediaType);
      const form=new FormData();form.append("file",new Blob([bytes],{type:r.inputArtifacts[0].mediaType}),r.inputArtifacts[0].mediaType==="image/png"?"input.png":"input.jpg");
      const uploaded=await this.api("/files",{method:"POST",body:form,context});
      if(typeof uploaded.file_token!=="string"||!/^[-A-Za-z0-9_]{1,256}$/.test(uploaded.file_token))throw fail("MalformedProviderOutput","Invalid Tripo upload token.");
      body.input=uploaded.file_token;body.enable_image_autofix=false;
    }
    const result=await this.api(r.capability==="TextToMesh"?"/generation/text-to-model":"/generation/image-to-model",{method:"POST",body,context});
    return {providerJobId:taskId(result.task_id),state:{cancellationScope:"local"}};
  }
  async poll(id,state,context) {
    const started=Date.now();
    const data=await this.api("/tasks/"+taskId(id),{context});
    if(data.task_id!==id)throw fail("MalformedProviderOutput","Tripo returned a different task identity.");
    if(["queued","running"].includes(data.status))return {status:"pending",state,progress:Math.max(5,Math.min(95,Number(data.progress)||5))};
    if(data.status==="cancelled")return {status:"cancelled",state};
    if(["failed","banned","expired","unknown"].includes(data.status))return {status:"failed",state,error:{code:"TripoTaskFailed",message:"Tripo task ended with status "+data.status+".",retryable:["failed","expired"].includes(data.status)}};
    if(data.status!=="success")throw fail("MalformedProviderOutput","Unknown Tripo lifecycle status.");
    const credits=data.credits_consumed;
    if(typeof credits!=="number"||!Number.isFinite(credits)||credits<0||credits>1000000000||Math.abs(credits*100-Math.round(credits*100))>0.00001)throw fail("MalformedProviderOutput","Tripo must report bounded credit consumption.");
    const actualMinorUnits=Math.ceil(credits*this.minorUnitsPerCredit);
    const observedUsage={currency:this.currency,actualMinorUnits,usageUnit:"tripo-credit",actualUsage:credits};
    try {
    if(actualMinorUnits>context.request.budgets.maximumCostMinorUnits||this.currency!==context.request.budgets.currency)throw fail("CostBudgetExceeded","Tripo reported usage beyond the approved budget. Inspect account charges.");
    let url;try{url=new URL(data.output?.model_url);}catch{throw fail("MalformedProviderOutput","Missing Tripo model download.");}
    if(url.protocol!=="https:"||url.username||url.password||url.port||!this.downloadHosts.includes(url.hostname)||!url.pathname.toLowerCase().endsWith(".glb"))throw fail("UntrustedDownload","Only allowlisted HTTPS GLB downloads are accepted; FBX and redirects are rejected.");
    let bytes;
    try {
      const signal=AbortSignal.any([AbortSignal.timeout(Math.max(1,Math.min(30000,(context.remainingTimeoutMs??30000)-(Date.now()-started)))),...(context.signal?[context.signal]:[])]);
      const response=await this.fetchImpl(url.href,{redirect:"error",signal}); // Never forward credentials to storage.
      if(!response.ok){await response.body?.cancel();throw fail("TripoDownloadFailed","Tripo GLB download failed.",true);}
      bytes=await bounded(response,context.request.budgets.maximumOutputBytes);
    }catch(error){if(error instanceof WorkerError)throw error;throw fail("TripoDownloadFailed","Tripo GLB download interrupted or timed out.",true);}
    validateStaticPropGlb(bytes,context.request.parameters.staticProp);
    // Signed URLs are deliberately ephemeral. Only validated bytes enter durable state.
    return {status:"completed",state,progress:100,result:{resultContractVersion:1,providerJobId:id,model:this.modelVersion,artifacts:[{logicalName:"harbor-prop",mediaType:"model/gltf-binary",contentBase64:bytes.toString("base64")}],usage:observedUsage}};
    } catch(error) {
      if(error instanceof WorkerError) error.details={observedUsage};
      throw error;
    }
  }
  async cancel(_id,state) {return {...state,cancelledLocally:true,remoteCancellationSupported:false};}
  async download(result) {return result.artifacts;}
  normalizeMetadata(result) {return {providerId:this.providerId,adapterVersion:this.adapterVersion,modelVersion:this.modelVersion,providerJobId:result.providerJobId,apiVersion:"v3",staticPropContractVersion:1,cancellationScope:"local",estimatedCredits:this.estimatedCredits,minorUnitsPerCredit:this.minorUnitsPerCredit};}
}
export function createTripoProviderFromEnvironment(env=process.env) {
  if(env.HANSA_TRIPO_ENABLED!=="1")return null;
  return new TripoStaticPropProvider({apiKey:env.TRIPO_API_KEY,modelVersion:env.HANSA_TRIPO_MODEL,estimatedCredits:Number(env.HANSA_TRIPO_ESTIMATED_CREDITS),minorUnitsPerCredit:Number(env.HANSA_TRIPO_MINOR_UNITS_PER_CREDIT),currency:env.HANSA_TRIPO_CURRENCY??"USD",downloadHosts:env.HANSA_TRIPO_DOWNLOAD_HOSTS?.split(",")??["cdn.tripo3d.ai"]});
}
