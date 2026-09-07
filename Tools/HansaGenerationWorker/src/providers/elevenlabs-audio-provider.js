import {WorkerError} from "../errors.js";
import {sha256,hashDocument} from "../canonical-json.js";
import {audioProfile,validateAudioTake} from "../audio-take.js";
import {decodeMp3,probeAudioDecoder} from "../audio-decoder.js";

const MODELS={TextToSoundEffect:"eleven_text_to_sound_v2",TextToSpeech:"eleven_flash_v2_5"};
function fail(code,message,details){return new WorkerError(code,message,{retryable:false,details});}
export class ElevenLabsAudioProvider {
  constructor({apiKey,allowedVoiceIds=[],sfxUnitsPerSecond,speechUnitsPerCharacter,minorUnitsPer1000,currency="USD",decoderExecutable="ffmpeg",fetchImpl=globalThis.fetch,decoder=decodeMp3}={}) {
    if(typeof apiKey!=="string"||!apiKey||!Array.isArray(allowedVoiceIds)||allowedVoiceIds.some(v=>!/^[A-Za-z0-9]{1,128}$/.test(v))||![sfxUnitsPerSecond,speechUnitsPerCharacter,minorUnitsPer1000].every(n=>Number.isSafeInteger(n)&&n>0&&n<=1000000)||!/^[A-Z]{3}$/.test(currency))
      throw fail("ElevenLabsConfigurationRequired","Configure worker credentials, approved voices and an explicit conservative billing rate card.");
    Object.assign(this,{apiKey,allowedVoiceIds,sfxUnitsPerSecond,speechUnitsPerCharacter,minorUnitsPer1000,currency,decoderExecutable,fetchImpl,decoder});
    this.providerId="elevenlabs";this.adapterVersion="1.0.0";this.requiresSubmissionGuard=true;
    this.cancellationMessage="Cancelled locally. ElevenLabs generation may already have incurred charges; no remote cancellation is claimed.";
  }
  getCapabilities(){return {providerId:this.providerId,adapterVersion:this.adapterVersion,models:Object.values(MODELS).map(modelVersion=>({modelVersion,pinned:true})),capabilities:Object.entries(MODELS).filter(([c])=>c!=="TextToSpeech"||this.allowedVoiceIds.length).map(([capability])=>({capability,asynchronous:false,supportsCancellation:true,cancellationScope:"local",supportsSeed:capability==="TextToSpeech",inputMediaTypes:[],outputMediaTypes:["audio/wav"],maximumVariants:2}))};}
  validateRequest(r){
    const p=audioProfile(r.parameters.audioTake),speech=r.capability==="TextToSpeech";
    if(r.providerId!==this.providerId||r.modelVersion!==MODELS[r.capability]||p.kind!==(speech?"Speech":"SFX")||r.intendedAssetRole!==(speech?"SpeechLine":"HarborSFX"))
      throw fail("ProviderRequestRejected","Select the pinned ElevenLabs single SFX or English speech workflow.");
    const allowed=speech?["audioTake","voiceId","voiceSettings"]:["audioTake","durationSeconds","promptInfluence"];
    if(Object.keys(r.parameters).some(k=>!allowed.includes(k))||r.inputArtifacts.length||r.negativePrompt||r.prompt.length> (speech?300:1000)||sha256(r.prompt)!==r.promptSha256)
      throw fail("ProviderRequestRejected","Use bounded unredacted text and only the supported audio parameters.");
    if(r.rights.acknowledged!==true||typeof r.rights.declaration!=="string"||!r.rights.declaration.trim())
      throw fail("RightsRequired","Acknowledge the source and output-use rights before generation.");
    if(r.outputContract.maximumArtifacts!==p.variants||r.outputContract.allowedMediaTypes.length!==1||r.outputContract.allowedMediaTypes[0]!=="audio/wav"||r.budgets.maximumOutputBytes>8*1024*1024)
      throw fail("ProviderRequestRejected","Request exactly one or two PCM WAV takes, at most 8 MiB combined.");
    if(speech) {
      if(p.subtitle!==r.prompt||r.rights.voiceAcknowledged!==true||r.rights.englishTextAcknowledged!==true||!this.allowedVoiceIds.includes(r.parameters.voiceId))
        throw fail("VoiceRightsRequired","Speech requires the exact English subtitle, explicit voice permission and a configured approved voice.");
      const v=r.parameters.voiceSettings;
      if(!v||Object.keys(v).length!==3||!["stability","similarity_boost","style"].every(k=>typeof v[k]==="number"&&Number.isFinite(v[k])&&v[k]>=0&&v[k]<=1))
        throw fail("ProviderRequestRejected","Specify only bounded stability, similarity_boost and style settings.");
      if(r.seed!==null&&r.seed>4294967294)throw fail("ProviderRequestRejected","Speech seed must leave room for two uint32 takes.");
    } else {
      if(r.seed!==null||!Number.isFinite(r.parameters.durationSeconds)||r.parameters.durationSeconds<0.5||r.parameters.durationSeconds>10||r.parameters.durationSeconds*1000>p.maximumDurationMs||r.parameters.durationSeconds*1000<p.minimumDurationMs||!Number.isFinite(r.parameters.promptInfluence)||r.parameters.promptInfluence<0||r.parameters.promptInfluence>1)
        throw fail("ProviderRequestRejected","SFX requires duration 0.5–10 seconds, bounded prompt influence and no unsupported seed.");
    }
    return {valid:true};
  }
  unitsPerTake(r){return Math.ceil(r.capability==="TextToSpeech"?r.prompt.length*this.speechUnitsPerCharacter:r.parameters.durationSeconds*this.sfxUnitsPerSecond);}
  cost(units){return Math.ceil(units*this.minorUnitsPer1000/1000);}
  estimateCost(r){return {currency:this.currency,estimatedMinorUnits:this.cost(this.unitsPerTake(r))*r.parameters.audioTake.variants,usageUnit:"elevenlabs-billing-character",estimatedUsage:this.unitsPerTake(r)*r.parameters.audioTake.variants};}
  async submit(r,context) {
    this.validateRequest(r);
    const decoderVersion=this.decoder===decodeMp3?await probeAudioDecoder(this.decoderExecutable,context.signal):"injected-test-decoder";
    const started=Date.now(),takes=[],artifacts=[];
    let actualUnits=0,actualMinorUnits=0,totalOutput=0;
    const remaining=()=>Math.max(1,context.remainingTimeoutMs-(Date.now()-started));
    const details=()=>({takes,observedUsage:{currency:this.currency,actualMinorUnits,actualUsage:actualUnits,usageUnit:"elevenlabs-billing-character"}});
    try {
      for(let index=0;index<r.parameters.audioTake.variants;index++){
        if(context.signal?.aborted||Date.now()-started>=context.remainingTimeoutMs)throw fail("ProviderTimeout","Audio generation interrupted or expired.",details());
        if(actualMinorUnits+this.cost(this.unitsPerTake(r))>r.budgets.maximumCostMinorUnits)throw fail("CostBudgetExceeded","Remaining budget cannot admit another take.",details());
        const speech=r.capability==="TextToSpeech";
        const route=speech?"/text-to-speech/"+r.parameters.voiceId:"/sound-generation";
        const body=speech?{text:r.prompt,model_id:r.modelVersion,language_code:"en",voice_settings:r.parameters.voiceSettings,...(r.seed===null?{}:{seed:r.seed+index})}:{text:r.prompt,model_id:r.modelVersion,loop:false,duration_seconds:r.parameters.durationSeconds,prompt_influence:r.parameters.promptInfluence};
        const signal=AbortSignal.any([AbortSignal.timeout(remaining()),...(context.signal?[context.signal]:[])]);
        let response;
        try {response=await this.fetchImpl("https://api.elevenlabs.io/v1"+route+"?output_format=mp3_44100_128",{method:"POST",headers:{"xi-api-key":this.apiKey,"Content-Type":"application/json",Accept:"audio/mpeg"},body:JSON.stringify(body),redirect:"error",signal});}
        catch{throw fail("SubmissionOutcomeUnknown","ElevenLabs may have accepted a billable request. Inspect the account before approving a retry.",details());}
        if(!response.ok){await response.body?.cancel();throw fail("ElevenLabsHttpFailure","ElevenLabs returned HTTP "+response.status+". No automatic billable retry.",details());}
        const rawUnits=response.headers.get("character-cost"),requestId=response.headers.get("request-id");
        if(!/^[0-9]{1,10}(?:\.[0-9]{1,2})?$/.test(rawUnits??"")){await response.body?.cancel();throw fail("UsageUnreported","ElevenLabs omitted valid billing usage; inspect account charges before retrying.",details());}
        const units=Number(rawUnits);
        actualUnits+=units;actualMinorUnits+=this.cost(units);
        const take={variant:index+1,providerRequestId:/^[A-Za-z0-9_-]{1,128}$/.test(requestId??"")?requestId:null,billingUnits:units,settings:body,outputFormat:"mp3_44100_128"};
        takes.push(take);
        if(actualMinorUnits>r.budgets.maximumCostMinorUnits){await response.body?.cancel();throw fail("CostBudgetExceeded","Observed ElevenLabs cost exceeds the approved ceiling.",details());}
        if(!/^audio\/(mpeg|mp3)(;|$)/i.test(response.headers.get("content-type")??"")){await response.body?.cancel();throw fail("MalformedProviderOutput","Expected MP3 audio response.",details());}
        const maximum=2*1024*1024;
        if(Number(response.headers.get("content-length"))>maximum){await response.body?.cancel();throw fail("OutputBudgetExceeded","Encoded take exceeds 2 MiB.",details());}
        const reader=response.body?.getReader();if(!reader)throw fail("MalformedProviderOutput","Empty audio response.",details());
        const chunks=[];let size=0;
        try {while(true){const {done,value}=await reader.read();if(done)break;size+=value.length;if(size>maximum)throw fail("OutputBudgetExceeded","Encoded take exceeds 2 MiB.");chunks.push(Buffer.from(value));}}
        catch(error){await reader.cancel().catch(()=>{});throw error;}finally{reader.releaseLock();}
        const original=Buffer.concat(chunks);
        take.source=await context.retainProviderSource(index,original,"audio/mpeg");
        const wav=await this.decoder(original,{executable:this.decoderExecutable,signal:context.signal,timeoutMs:Math.min(10000,remaining())});
        take.qa=validateAudioTake(wav,r.parameters.audioTake);
        if(take.qa.sampleRate!==44100)throw fail("MalformedProviderOutput","Decoded sample rate differs from requested mp3_44100_128.");
        take.outputSha256=sha256(wav);
        totalOutput+=wav.length;if(totalOutput>r.budgets.maximumOutputBytes)throw fail("OutputBudgetExceeded","Decoded takes exceed the output ceiling.");
        artifacts.push({logicalName:"take-"+(index+1),mediaType:"audio/wav",contentBase64:wav.toString("base64")});
      }
      return {providerJobId:"elevenlabs-local-batch-"+context.jobId+"-r"+context.revision,state:{status:"completed",takes,artifacts,decoderVersion,usage:{currency:this.currency,actualMinorUnits,actualUsage:actualUnits,usageUnit:"elevenlabs-billing-character"}}};
    }catch(error){
      if(error instanceof WorkerError){error.details=details();throw error;}
      throw fail("AudioGenerationFailed","Audio transport or decoding failed. Inspect retained evidence before retrying.",details());
    }
  }
  async poll(providerJobId,state,context) {
    if(state?.status!=="completed"||!Array.isArray(state.artifacts))throw fail("MalformedProviderOutput","Missing completed audio batch.");
    state.artifacts.forEach(a=>validateAudioTake(Buffer.from(a.contentBase64,"base64"),context.request.parameters.audioTake));
    return {status:"completed",state,progress:100,result:{resultContractVersion:1,providerJobId,model:context.request.modelVersion,contractHash:hashDocument(context.request.parameters.audioTake),artifacts:state.artifacts,usage:state.usage}};
  }
  async cancel(_id,state){return {...state,status:"cancelled",artifacts:undefined};}
  async download(result){return result.artifacts;}
  normalizeMetadata(result,context){return {providerId:this.providerId,adapterVersion:this.adapterVersion,modelVersion:result.model,providerJobId:result.providerJobId,providerJobIdKind:"local-batch-correlation",audioContractVersion:1,takes:context.providerState.takes,rateCard:{minorUnitsPer1000:this.minorUnitsPer1000,sfxUnitsPerSecond:this.sfxUnitsPerSecond,speechUnitsPerCharacter:this.speechUnitsPerCharacter},decoder:{version:context.providerState.decoderVersion,contract:"MP3 to PCM16 WAV; no resampling or downmixing"}};}
}
export function createElevenLabsProviderFromEnvironment(env=process.env){
  if(env.HANSA_ELEVENLABS_ENABLED!=="1")return null;
  return new ElevenLabsAudioProvider({apiKey:env.ELEVENLABS_API_KEY,allowedVoiceIds:(env.HANSA_ELEVENLABS_APPROVED_VOICES??"").split(",").filter(Boolean),sfxUnitsPerSecond:Number(env.HANSA_ELEVENLABS_SFX_UNITS_PER_SECOND),speechUnitsPerCharacter:Number(env.HANSA_ELEVENLABS_SPEECH_UNITS_PER_CHARACTER),minorUnitsPer1000:Number(env.HANSA_ELEVENLABS_MINOR_UNITS_PER_1000),currency:env.HANSA_ELEVENLABS_CURRENCY??"USD",decoderExecutable:env.HANSA_AUDIO_DECODER??"ffmpeg"});
}
