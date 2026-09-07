from pathlib import Path
p=Path(__file__).with_name('import_materials.py');s=p.read_text().replace("link(vc,'RGB',mul,'B')","link(vc,'',mul,'B')")
s=s.replace("  tex=c.call('texture','import_file',{'folder_path':root+'/Textures','asset_name':'T_Mill_'+name+'_'+kind,'source_file':src})[0]", """  tp=root+'/Textures/T_Mill_'+name+'_'+kind
  if c.call('asset','exists',{'path':tp}):tex=c.call('asset','load_asset',{'asset_path':tp})
  else:tex=c.call('texture','import_file',{'folder_path':root+'/Textures','asset_name':'T_Mill_'+name+'_'+kind,'source_file':src})[0]""")
p.write_text(s)
