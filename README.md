# SH2R-UEVR

UEVR improvements/motion controls for SILENT HILL 2 (2024)

## Features
* First Person
* Motion controls (no melee swinging yet)
  * Ranged weapon aiming
  * Items are attached to right hand when inspecting or receiving them (like ammo)
  * The map is attached to both hands to simulate holding a map in real life 
* Two handing of rifle and shotgun
* Roomscale movement
* Properly projected crosshair in 3D space
* Automatic disabling of VR features in cutscenes and full body events (though still fully 6DOF and stereoscopic)

Inverse kinematics has been loosely explored, but not working correctly. So body has been hidden for now, and re-enabled in cutscenes and events.

## Install

First, make sure that you are using the very latest **NIGHTLY** version of UEVR, you can get that here: https://github.com/praydog/UEVR-nightly/releases/latest/ or using [Rai Pal](https://github.com/Raicuparta/rai-pal)

Get the [latest release zip](https://github.com/praydog/SH2R-UEVR/releases/latest) and click "Import Config" in UEVR, browse to the zip and click it.

## For curious modders

This uses a hybrid C++ plugin and Lua script, and should serve as a good example of adding VR functionality.
