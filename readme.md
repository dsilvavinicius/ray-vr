# Ray VR

[Ray VR](https://www.visgraf.impa.br/ray-vr/) is a real-time ray tracer for visualization of euclidean and non-euclidean spaces in VR. This repository implements the paper [Immersive Visualization of the Classical Non-Euclidean Spaces using Real-Time Ray Tracing in VR](https://graphicsinterface.org/proceedings/gi2020/gi2020-42/).

## Project organization

The project consists of two repos:

- [Ray-VR](git@github.com:dsilvavinicius/rayvr.git): original main repo, with support for stereo ray tracing on Torus and Dodecahedron. Supports the following manifolds and orbifolds: Mirrored Cube, Torus, Mirrored Dodecahedron, Seifert-Weber, and Poincaré Sphere.
- [Falcor](git@github.com:dsilvavinicius/Falcor.git): prerequisite. Fork of NVidia's scientifical framework and is used as a library to access RTX ray tracing capabilities.

## Disclaimer

This project is highly experimental. Do not expect sophisticated user interfaces.

## Setup and building

```
Prerequisites
------------------------
- GPU that supports DirectX 12
- Windows 10 RS2 (version 1703) or newer
- HMD supported by Steam VR (tested on Quest 2, and Vive)
- Visual Studio (VS) 2022
- [Microsoft Windows SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
```

In Git Bash, type the following instructions:

```
1. git clone git@github.com:dsilvavinicius/rayvr.git
2. cd ray-vr
3. git submodule init
4. git submodule update
5. Open the solution in VS 2022, compile the ReleaseD3D12 configuration
6. Turn your HMD on and link it with your PC. Our current tests used a Quest 2 with Airlink. (If your HMD is not linked, SteamVR will not recognize it and the app will crash)
7. You should use the application in front of your PC to access mouse and keyboard so you will be able to change settings, load scenes or translate your camera. We used the Stationary mode on Quest 2, in front of the computer.
8. Run the ReleaseD3D12 configuration in VS 2022.
9. You should now see the Seifert-Weber space. You have an avatar with a head and two hands, which should be centered in the fundamental domain dodecahedron.
10. Use the QWASDE keys to translate the camera if you appear outside of the dodecahedron. It works as a free-camera in a FPS game.
```

## Options

The `Load Scene` button loads a new scene. Scenes are in the `Media` folder (`.fscene` files):

* `Torus/mirrored_cube.fscene` is the mirrored cube.
* `Torus/torus.fscene` is the Torus.
* `Dodecahedron/mirrored_dodecahedron.fscene` is the mirrored dodecahedron.
* `Dodecahedron/seifert-weber.fscene` is the Seifert-Weber dodecahedron.
* `Dodecahedron/poincare.fscene` is the Poincaré dodecahedron.

You may also toggle the visualization of the boundaries of the fundamental domain clicking on the button Toggle Boundaries.