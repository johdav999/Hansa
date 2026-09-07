import {WorkerError} from "./errors.js";
export function requireAudio(ok,message) {if(!ok)throw new WorkerError("AudioTakeRejected",message);}
export function audioProfile(p) {
  const keys=["version","kind","stableId","variants","minimumDurationMs","maximumDurationMs","maximumLeadingSilenceMs","maximumTrailingSilenceMs","subtitle","speakerId","language","loop"];
  requireAudio(p&&Object.keys(p).length===keys.length&&Object.keys(p).every(k=>keys.includes(k))&&p.version===1&&["SFX","Speech"].includes(p.kind),"Use the complete AudioTake v1 profile.");
  const prefix=p.kind==="SFX"?"SFX":"Dialogue";
  requireAudio(typeof p.stableId==="string"&&p.stableId.length<=100&&new RegExp("^"+prefix+"\\.[A-Za-z0-9_]+(?:\\.[A-Za-z0-9_]+)*$").test(p.stableId),"Invalid stable audio identity.");
  for(const [k,min,max] of [["variants",1,2],["minimumDurationMs",100,15000],["maximumDurationMs",100,15000],["maximumLeadingSilenceMs",0,500],["maximumTrailingSilenceMs",0,1000]])
    requireAudio(Number.isSafeInteger(p[k])&&p[k]>=min&&p[k]<=max,"Invalid audio bound: "+k);
  requireAudio(p.minimumDurationMs<=p.maximumDurationMs&&p.loop===false,"Only short non-looping takes are supported.");
  if(p.kind==="Speech") requireAudio(p.language==="en"&&typeof p.subtitle==="string"&&p.subtitle.trim().length>0&&p.subtitle.length<=300&&!/[\r\n\[\]]/.test(p.subtitle)&&typeof p.speakerId==="string"&&p.speakerId.length<=100&&/^Speaker\.[A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+)*$/.test(p.speakerId),"Speech needs one English subtitle and stable Speaker.* identity.");
  else requireAudio(p.subtitle===""&&p.speakerId===""&&p.language==="","SFX cannot contain a speaker or subtitle.");
  return {...p};
}
export function parsePcmWav(bytes) {
  requireAudio(Buffer.isBuffer(bytes)&&bytes.length>=44&&bytes.length<=4*1024*1024&&bytes.toString("ascii",0,4)==="RIFF"&&bytes.toString("ascii",8,12)==="WAVE"&&bytes.readUInt32LE(4)+8===bytes.length,"Invalid bounded WAV container.");
  let fmt,data;
  for(let o=12;o<bytes.length;) {
    requireAudio(o+8<=bytes.length,"Truncated WAV chunk.");const n=bytes.readUInt32LE(o+4),name=bytes.toString("ascii",o,o+4);
    requireAudio(o+8+n+(n%2)<=bytes.length,"Invalid WAV chunk size.");
    if(name==="fmt "){requireAudio(!fmt&&n===16,"WAV requires one PCM format.");fmt=bytes.subarray(o+8,o+8+n);}
    else if(name==="data"){requireAudio(!data&&n>0,"WAV requires one audio payload.");data=bytes.subarray(o+8,o+8+n);}
    else requireAudio(["JUNK","LIST"].includes(name),"Unsupported WAV chunk.");
    o+=8+n+(n%2);
  }
  requireAudio(fmt&&data&&fmt.readUInt16LE(0)===1&&fmt.readUInt16LE(14)===16,"Only decoded PCM16 WAV is accepted.");
  const channels=fmt.readUInt16LE(2),sampleRate=fmt.readUInt32LE(4);
  requireAudio([1,2].includes(channels)&&[24000,44100,48000].includes(sampleRate)&&fmt.readUInt16LE(12)===channels*2&&fmt.readUInt32LE(8)===sampleRate*channels*2&&data.length%(channels*2)===0,"Invalid audio channels, rate or alignment.");
  return {channels,sampleRate,data};
}
export function validateAudioTake(bytes,profile) {
  const p=audioProfile(profile),{channels,sampleRate,data}=parsePcmWav(bytes);
  requireAudio(p.kind!=="Speech"||channels===1,"Speech must be mono.");
  const frames=data.length/(channels*2),durationMs=frames*1000/sampleRate;
  requireAudio(durationMs>=p.minimumDurationMs&&durationMs<=p.maximumDurationMs,"Audio duration exceeds the approved bounds.");
  let first=-1,last=-1,peak=0,clipped=0;
  for(let f=0;f<frames;f++) {
    let active=false;
    for(let c=0;c<channels;c++){const value=Math.abs(data.readInt16LE((f*channels+c)*2));peak=Math.max(peak,value);if(value>=32760)clipped++;if(value>104)active=true;}
    if(active){if(first<0)first=f;last=f;}
  }
  requireAudio(first>=0,"Take is silent below the -50 dBFS activity threshold.");
  const leadingSilenceMs=first*1000/sampleRate,trailingSilenceMs=(frames-last-1)*1000/sampleRate;
  requireAudio(clipped===0,"Take contains clipped samples (absolute PCM value >= 32760).");
  requireAudio(leadingSilenceMs<=p.maximumLeadingSilenceMs&&trailingSilenceMs<=p.maximumTrailingSilenceMs,"Take contains excessive leading or trailing silence.");
  return {version:1,channels,sampleRate,durationMs,leadingSilenceMs,trailingSilenceMs,peakAbsolute:peak,clippedSamples:clipped,activityThresholdPcm:104};
}
