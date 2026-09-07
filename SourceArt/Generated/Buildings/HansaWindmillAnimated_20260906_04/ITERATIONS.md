1. Split the cap-fit model into body and rotor, including hub fasteners. Corrected deferred Blender transform evaluation before export; verified origin-centred rotor.
2. Added native Unreal rotating movement and tested forward/pause/reverse. Corrected the isolated preview game mode to suppress unrelated scenario actors.
3. Full surface sweep detected blade intersections with the lower taper. Moved rotor forward 0.6 m and extended the shaft back into the cap; reran 72-angle intersection checks successfully.
4. Corrected an inherited child-actor mesh override exposed by saved-instance readback, respawned the preview actor, and repeated runtime and saved-state checks.
