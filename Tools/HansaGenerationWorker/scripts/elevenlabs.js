import { readFile } from "node:fs/promises";
import { WorkerPipeClient } from "../src/pipe-client.js";

const [action, target, ...args] = process.argv.slice(2);
const option = name => args.find(s => s.startsWith("--"+name+"="))?.slice(name.length+3);
const client = new WorkerPipeClient({
  pipeName: process.env.HANSA_GENERATION_WORKER_PIPE ?? "hansa-generation-worker-v1",
  authenticationToken: process.env.HANSA_GENERATION_WORKER_TOKEN ?? "",
  timeoutMs: 30000,
});
const approval = () => {
  const approvedBy = option("approve-spend");
  if (!approvedBy?.trim() || approvedBy.length > 128) throw new Error("An explicit --approve-spend=Reviewer is required.");
  return {approved:true,approvedBy,approvedAt:new Date().toISOString()};
};
let result;
if (["estimate","submit"].includes(action) && target) {
  const bytes=await readFile(target);
  if(bytes.length>900000)throw new Error("Request file exceeds the bounded local protocol; use a smaller native reference image.");
  const request=JSON.parse(bytes.toString("utf8"));
  if(request.providerId!=="elevenlabs"||!["HarborSFX","SpeechLine"].includes(request.intendedAssetRole))throw new Error("This CLI accepts only ElevenLabs SFX/speech requests.");
  request.spendApproval={approved:false};
  const estimate=await client.call("job.estimate",{request});
  if(action==="estimate")result=estimate;
  else {
    const confirmed=option("confirm-cost");
    if(!/^[0-9]+$/.test(confirmed??"") || Number(confirmed)!==estimate.estimate.estimatedMinorUnits)
      throw new Error("Run estimate first, then provide --confirm-cost="+estimate.estimate.estimatedMinorUnits+" (minor units of "+estimate.estimate.currency+").");
    request.spendApproval=approval();
    result=await client.call("job.submit",{request});
  }
} else if (["status","cancel","retain"].includes(action)&&target) {
  const take=Number(option("take")??1);
  if(!Number.isInteger(take)||take<1||take>2)throw new Error("Select --take=1 or --take=2.");
  result=await client.call({status:"job.get",cancel:"job.cancel",retain:"job.media.retain"}[action],{jobId:target,...(action==="retain"?{outputIndex:take-1}:{})});
} else if(action==="retry"&&target) {
  const prior=await client.call("job.get",{jobId:target});
  // Estimate through the original safe request cannot reconstruct private image
  // bytes; the service independently validates the fresh estimate before retry.
  const confirmed=option("confirm-budget");
  if(!/^[0-9]+$/.test(confirmed??"")||Number(confirmed)!==prior.job.request.budgets.maximumCostMinorUnits)
    throw new Error("Retry can create another billable task. Inspect the provider account, then confirm the original ceiling with --confirm-budget=<minor units>.");
  const key=option("retry-key");
  if(!key)throw new Error("Provide a stable --retry-key=<unique action key> for retry idempotency.");
  result=await client.call("job.retry",{jobId:target,retryIdempotencyKey:key,reason:"Explicit CLI retry after provider account inspection",spendApproval:approval()});
} else {
  throw new Error("Usage: elevenlabs.js estimate|submit <request.json> [--approve-spend=Reviewer --confirm-cost=N], status|cancel|retain <job-id>, or retry <job-id> --approve-spend=Reviewer --confirm-budget=N --retry-key=Unique.");
}
console.log(JSON.stringify(result,null,2));
