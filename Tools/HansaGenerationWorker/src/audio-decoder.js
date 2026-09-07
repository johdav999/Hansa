import {spawn,execFile} from "node:child_process";
import {promisify} from "node:util";
import {WorkerError} from "./errors.js";

// Decode only an in-memory MP3 stream, with no URL/file protocol or shell.
// Preserve rate/channels; neither downmixing nor resampling hides invalid output.
export function decodeMp3(bytes,{executable="ffmpeg",signal,timeoutMs=10000}={}) {
  return new Promise((resolve,reject)=>{
    const child=spawn(executable,["-hide_banner","-loglevel","error","-nostdin","-threads","1","-protocol_whitelist","pipe","-f","mp3","-i","pipe:0","-map","0:a:0","-vn","-map_metadata","-1","-fflags","+bitexact","-flags:a","+bitexact","-c:a","pcm_s16le","-f","wav","pipe:1"],{shell:false,windowsHide:true,stdio:["pipe","pipe","pipe"]});
    let done=false,size=0,diagnosticBytes=0;const chunks=[];
    const finish=(error,value)=>{if(done)return;done=true;clearTimeout(timer);signal?.removeEventListener("abort",abort);if(error)child.kill();error?reject(error):resolve(value);};
    const bad=()=>new WorkerError("AudioDecodeFailed","Bounded MP3 decoding failed; inspect the retained source.",{retryable:false});
    const abort=()=>finish(bad());
    const timer=setTimeout(abort,Math.max(1,timeoutMs));
    if(signal?.aborted)abort();else signal?.addEventListener("abort",abort,{once:true});
    child.on("error",()=>finish(bad()));
    child.stdin.on("error",()=>{});child.stdin.end(bytes);
    child.stderr.on("data",b=>{diagnosticBytes+=b.length;if(diagnosticBytes>65536)finish(bad());});
    child.stdout.on("data",b=>{size+=b.length;if(size>4*1024*1024)finish(bad());else chunks.push(b);});
    child.on("close",code=>{
      if(done)return;if(code!==0)return finish(bad());
      const wav=Buffer.concat(chunks);
      if(wav.length<44||wav.toString("ascii",0,4)!=="RIFF"||wav.toString("ascii",8,12)!=="WAVE")return finish(bad());
      // FFmpeg's nonseekable WAV uses sentinel sizes. Resolve them from bounded bytes.
      wav.writeUInt32LE(wav.length-8,4);let found=false;
      for(let o=12;o+8<=wav.length;) {
        const name=wav.toString("ascii",o,o+4),n=wav.readUInt32LE(o+4);
        if(name==="data"){wav.writeUInt32LE(wav.length-o-8,o+4);found=true;break;}
        if(n> wav.length-o-8)return finish(bad());o+=8+n+(n%2);
      }
      if(!found)return finish(bad());finish(null,wav);
    });
  });
}

export async function probeAudioDecoder(executable, signal) {
  try {
    const result = await promisify(execFile)(executable, ["-version"], {
      shell: false, windowsHide: true, timeout: 2000, maxBuffer: 65536, signal,
    });
    const firstLine = result.stdout.split(/\r?\n/)[0];
    if (!/^ffmpeg version [A-Za-z0-9._+-]+ /.test(firstLine)) throw new Error("Unexpected decoder");
    return firstLine.slice(0, 256);
  } catch {
    throw new WorkerError("AudioDecoderUnavailable", "Configure a working FFmpeg decoder before approving live audio generation.");
  }
}
