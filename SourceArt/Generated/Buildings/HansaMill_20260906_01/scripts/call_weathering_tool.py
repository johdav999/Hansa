from unreal_ops import Client,P
from mcp_client import rpc
import json
c=Client()
out,_=rpc('tools/call',{'name':'describe_toolset','arguments':{'toolset_name':'Temp.GenerationJobs.hansa-mill_20260906_01.scripts.mill_weathering_tool.HansaMillWeatheringTools'}},c.sid,2)
print(json.dumps(out))
print(c.call('Temp.GenerationJobs.hansa-mill_20260906_01.scripts.mill_weathering_tool.HansaMillWeatheringTools','import_weathered_mill',{}))
