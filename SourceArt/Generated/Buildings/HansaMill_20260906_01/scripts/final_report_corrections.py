from pathlib import Path
P=Path(__file__).resolve().parents[1]
p=P/'scripts/write_report.py';s=p.read_text(encoding='utf-8')
s=s.replace('/Meshes/SM_HansaMill`','/Meshes/SM_HansaMill_Weathered`')
s=s.replace('Increased camera distance and widened the field of view, inspected the corrected rear frame, then rerendered all 48 native 800-square frames and replaced the encoded turntable.', 'Changed to a generous orthographic orbit, tested every world-bounds corner against every frame with a 3% safety margin, then rerendered all 48 native 800-square frames and replaced the encoded turntable.')
s=s.replace('6. **Turntable framing correction.**','6. **Vertex weathering import correction.** The standard Unreal mesh importer defaulted to Ignore for FBX vertex colors. A bounded job-local ToolsetDefinition imported a new staging mesh with Replace through MCP. All five materials were assigned and the isolated preview was switched to this mesh. Save/reopen and import-data readback confirmed Replace; fresh engine captures verify the result. The original staging import remains a superseded comparison. No production asset was overwritten.\n7. **Turntable framing correction.**')
s=s.replace('The importer changes the triangle count;', 'The importer has remove-degenerates enabled and changes the triangle count;')
p.write_text(s,encoding='utf-8')
