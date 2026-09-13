"""Capture the saved isolated production review, with no gameplay writes."""
import unreal, json, math, time, base64
from pathlib import Path
JOB=Path(__file__).resolve().parents[1]
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level("/Game/Hansa/Generated/Staging/BreweryPromotion_20260912/L_Brewery_ProductionReview")
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world,"r.EyeAdaptationQuality 0")
levels.editor_set_viewport_realtime(True)
jobs=[("hero",3600,-28,235,(0,0,700)),("facade",2200,-5,270,(0,550,700)),("roof",1800,-30,235,(0,50,1350)),("yard",1400,-20,180,(550,50,200))]
index=0
view_prepared=False
started=time.monotonic()
def tick(delta):
    global index,started,handle,view_prepared
    try:
        name,distance,pitch,yaw,target=jobs[index]
        p=math.radians(pitch);y=math.radians(yaw)
        location={"x":target[0]-distance*math.cos(p)*math.cos(y),"y":target[1]-distance*math.cos(p)*math.sin(y),"z":target[2]-distance*math.sin(p)}
        if not view_prepared:
            transform={"location":location,"rotation":{"pitch":pitch,"yaw":yaw,"roll":0},"scale":{"x":1,"y":1,"z":1}}
            prepared=unreal.ToolsetRegistry.execute_tool("EditorToolset.EditorAppToolset","SetCameraTransform",json.dumps({"transform":transform}))
            assert prepared.is_complete and not prepared.error,str(prepared.error)
            view_prepared=True
            started=time.monotonic()
            return
        if time.monotonic()-started<10:return
        args={"bShowUI":False,"captureTransform":{"location":location,"rotation":{"pitch":pitch,"yaw":yaw,"roll":0},"scale":{"x":1,"y":1,"z":1}},"annotations":{"gridSpacing":0,"gridExtent":0,"gridHeight":0,"maxLabelDistance":0,"classFilter":{"refPath":"/Script/Engine.Actor"},"maxLabels":0}}
        r=unreal.ToolsetRegistry.execute_tool("EditorToolset.EditorAppToolset","CaptureViewport",json.dumps(args))
        assert r.is_complete and not r.error,str(r.error)
        (JOB/"renders"/("unreal-production-"+name+".png")).write_bytes(base64.b64decode(json.loads(r.value)["returnValue"]["image"]["data"]))
        index+=1
        view_prepared=False
        if index==len(jobs):
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.log("BREWERY_PRODUCTION_CAPTURES_SAVED")
            return
        started=time.monotonic()
    except Exception as error:
        unreal.log_error("BREWERY_CAPTURE_FAILED "+str(error))
        unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
