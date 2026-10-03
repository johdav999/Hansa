"""Normalize supplied clips with native bone track editing; retain source takes in FBX."""
import unreal as u, json, math
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve()
out=root/'Docs/Development/AmbientPeople'
base='/Game/Hansa/Characters/Laborer01'
marker=out/'animation-normalization.json'
# Always sample immutable source takes, so normalization is repeatable.
def vec(v): return [v.x,v.y,v.z]
def quat(q): return [q.x,q.y,q.z,q.w]
report={}
for name in ['Walk','Idle','LookAround']:
    clip=u.load_asset(base+'/A_Laborer01_'+name)
    source=u.load_asset('/Game/Hansa/Generated/Staging/AmbientPeopleSource/Source'+{'Walk':'walk','Idle':'idle','LookAround':'look_around'}[name])
    assert source
    frames=u.AnimationLibrary.get_num_frames(source)
    duration=clip.get_editor_property('sequence_length')
    bones=u.AnimationLibrary.get_animation_track_names(clip)
    poses={str(b):[u.AnimationLibrary.get_bone_pose_for_frame(source,b,i,False) for i in range(frames+1)] for b in bones}
    pelvis=poses['pelvis']; start=vec(pelvis[0].translation); end=vec(pelvis[-1].translation)
    drift=[end[i]-start[i] for i in range(3)]
    rootpose=poses['root'][0]
    report[name]={'frames':frames,'seconds':duration,'pelvis_local_displacement':drift,'root_scale':vec(rootpose.scale3d),'root_rotation':quat(rootpose.rotation)}
    controller=clip.controller; controller.open_bracket('Normalize ambient clip in place',False)
    for bone,track in poses.items():
        positions=[vec(p.translation) for p in track]; rotations=[quat(p.rotation) for p in track]; scales=[vec(p.scale3d) for p in track]
        if bone=='pelvis':
            # The supplied root maps local X/Z to the horizontal plane and local Y to vertical.
            for i,p in enumerate(positions):
                p[0]-=drift[0]*i/frames; p[2]-=drift[2]*i/frames
        # Blend the short tail to the initial pose to remove the source endpoint jump.
        blend=max(2,round(frames/duration*.18))
        for i in range(frames-blend+1,frames+1):
            alpha=(i-(frames-blend))/blend
            positions[i]=[a*(1-alpha)+b*alpha for a,b in zip(positions[i],positions[0])]
            scales[i]=[a*(1-alpha)+b*alpha for a,b in zip(scales[i],scales[0])]
            q0=rotations[0]; qi=rotations[i]
            sign=1 if sum(a*b for a,b in zip(qi,q0))>=0 else -1
            q=[a*(1-alpha)+b*sign*alpha for a,b in zip(qi,q0)]; length=math.sqrt(sum(v*v for v in q)); rotations[i]=[v/length for v in q]
        assert controller.set_bone_track_keys(bone,[u.Vector(*v) for v in positions],[u.Quat(*q) for q in rotations],[u.Vector(*v) for v in scales],False)
    controller.close_bracket(False)
    clip.set_editor_property('force_root_lock',True)
    clip.set_editor_property('root_motion_root_lock',u.RootMotionRootLock.ANIM_FIRST_FRAME)
    u.EditorAssetLibrary.save_loaded_asset(clip)
marker.write_text(json.dumps(report,indent=2))
u.log('AMBIENT_NORMALIZED '+json.dumps(report))
