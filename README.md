# web-engine

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

A hand-written 3D graphics engine that runs in a web browser. Written in C, compiled to WebAssembly with Emscripten, rendering through WebGL2.

The current demo renders a Lamborghini Revuelto on a reflective ground plane with full global illumination, real-time reflections, temporal anti-aliasing, bloom, depth of field, and shadows — all running at interactive framerates on integrated GPUs.

## Features

**Rendering**
- Physically-based rendering (Cook-Torrance BRDF, split-sum IBL)
- HDR rendering into `RGBA16F` targets
- ACES filmic tonemapping, sRGB output
- Deferred-style scene capture for post-processing

**Global illumination**
- Spherical Harmonic (SH L2) diffuse GI, prebaked into a 3D probe grid
- Spherical Gaussian (SG) specular GI — 32 lobes per probe, ray-traced against the actual car mesh with a uniform-triangle-grid acceleration structure
- Probe-grid parallax: reflections change as objects move through the world

**Reflections**
- Screen-space reflections (SSR) with Fresnel-weighted compositing
- SG probe reflections as off-screen fallback
- Reflective ground plane

**Anti-aliasing & post**
- TAA with Halton sub-pixel jitter, history buffer, neighborhood clamp
- Bloom with a dual-filter additive pyramid
- Depth of field with adjustable focus and bokeh radius
- Directional shadow map

**Asset pipeline**
- glTF 2.0 model loading (.glb), materials, textures, and mesh merging
- HDRI environment loading (equirectangular .hdr)
- Procedural sky with sun disk and atmospheric gradient

## Architecture

- src/main.c        - engine setup, frame loop, input
- src/pass.c        - fullscreen-triangle "fake compute" abstraction
- src/framebuffer.c - FBO and float-texture helpers
- src/mesh.c        - glTF loader + merge by material
- src/capture.c     - octahedral scene capture
- src/tri_grid.c    - uniform triangle grid for ray tracing
- src/probe_grid.c  - DDGI-style SH/SG probe bake
- src/sh.c, sg.c    - SH and SG utilities
- src/bloom.c       - dual-filter bloom pipeline
- src/shadow.c      - directional shadow map
- src/ground.c      - reflective ground plane mesh
- src/hdr_env.c     - equirectangular HDRI loader
- src/texture.c     - texture creation helpers
- src/asset.c       - virtual filesystem asset loading
- shaders/fullscreen.vert - gl_VertexID fullscreen triangle
- shaders/pbr.vert, pbr.frag - main PBR shader
- shaders/probe_bake.frag - ray-traced SG probe bake
- shaders/octa_capture.frag - octahedral scene capture
- shaders/sh_project.frag, sh_reconstruct.frag - SH passes
- shaders/sg_fit.frag - SG fitting
- shaders/sky_background.frag - procedural sky
- shaders/ssr.frag, ssr_composite.frag - SSR
- shaders/taa_resolve.frag - TAA
- shaders/dof.frag - depth of field
- shaders/bloom_*.frag - bloom
- shaders/shadow_depth.vert, .frag - shadow map

## Requirements

- Emscripten SDK (emsdk): https://emscripten.org/docs/getting_started/downloads.html
- Python 3 (for the local development server)
- A browser with WebGL2 support (Chrome, Firefox, Edge, Safari 15+)

## Build

Set up emsdk once:

    git clone https://github.com/emscripten-core/emsdk.git
    cd emsdk
    ./emsdk install latest
    ./emsdk activate latest
    source ./emsdk_env.sh

Then build and run:

    make
    make serve

Open http://localhost:8000/triangle.html

## Controls

- M: cycle debug modes (0-13)
- T: toggle TAA
- O: toggle depth of field
- [ / ]: lower / raise roughness scale
- - / =: pull / push DOF focus distance
- ; / ': decrease / increase DOF blur radius

## Debug modes

- 0: Full PBR render
- 1: Diffuse only
- 2: Specular only
- 3: Fresnel term
- 4: Roughness
- 5: Metalness
- 6: SH irradiance (indirect diffuse)
- 7: World-space normals
- 8: SG reflections (indirect specular)
- 9: Reflection vector
- 10: Shadow factor
- 11: Grid coordinates
- 12: SSR buffer
- 13: Bloom pyramid

## Performance notes

The engine is designed to run on modest hardware. It targets 60 fps at 1080p on Intel HD Graphics 400 (2012 integrated GPU) with the following optimizations:

- Prebaked GI: SH probes and SG reflections are computed once at startup, not per frame
- Mesh merging: glTF primitives with identical materials are merged into a single draw call (560 to about 10 draws for the demo car)
- Triangle grid: a uniform 32x16x32 grid accelerates the SG bake's ray-scene intersection
- TAA: sub-pixel jitter lets the pipeline average multiple samples over time instead of paying for them per frame

Startup time is dominated by the SG bake (about 2 s on Intel HD 400) and mesh merging.

## The demo

- Car: Lamborghini Revuelto (free low-poly 3D model) by AnatolianStudios on CGTrader: https://www.cgtrader.com/items/7588572
- HDRI: studio_small_09 from Poly Haven (CC0): https://polyhaven.com/a/studio_small_09

See NOTICE for full attribution.

## License

The engine source is licensed under the Apache License, Version 2.0. See LICENSE for the full text.

Third-party software and asset attributions are listed in NOTICE.

## Contributing

Issues and pull requests are welcome. By submitting a contribution, you agree to license it under the Apache License, Version 2.0 (per Section 5 of the license).
