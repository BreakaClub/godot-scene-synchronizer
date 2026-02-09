#!/usr/bin/env sh
scons target=template_debug debug_symbols=yes instrumentation=true instrumentation_threshold=0.05 && rsync -av demo/addons/godot-scene-synchronizer ../breaka-rpg/addons

