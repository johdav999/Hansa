"""P31 task-scoped MCP endpoint; does not change shared P13 client files/settings."""
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
JOB = REPO / 'Saved/GenerationJobs/city-life_P20_20260908'
sys.path.insert(0, str(REPO / 'SourceArt/Generated/Buildings/HansaSawmill_P13_20260908/scripts'))
import mcp_client
mcp_client.URL = 'http://127.0.0.1:8002/mcp'
from ue_batch import invoke, call

if __name__ == '__main__':
    print(invoke('list_toolsets', {}))
