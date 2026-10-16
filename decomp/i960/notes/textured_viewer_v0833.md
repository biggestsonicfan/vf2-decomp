# textured_viewer_v0833 — SDL_GPU textured model viewer

**celsowm Option 1, slice 3b** ("resolve the blocker until the very end"):
display the player-1 textured mesh in the viewer with **SDL_GPU** (SDL3's GPU
API — not raw Vulkan, no software rasterizer). Completes the goal's end state:
a player-1 textured model exported (slice 3a) and displayed.

## Usage

`vf2_model_viewer <model.obj> [--tex <bank.bin>] [--selftest]`

- `<model.obj>`: the mesh from `vf2i960 export-textured` (positions + UVs).
- `--tex <bank.bin>`: a raw texture-RAM bank (e.g. `vf2_fight_tex1_f1808.bin`);
  the viewer de-swizzles it to a 2048×1024 grayscale sheet (`vf2_texture_sheet_
  texel`) and samples it. Without `--tex` the mesh renders white.
- Controls: drag / arrows rotate, wheel or +/- zoom, R reset, Esc quit.
- `--selftest`: loads the OBJ, inits SDL, probes for a GPU device, exits 0
  (headless CI has no GPU backend — not a failure).

## Rendering [SDL_GPU]

- `SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, …)`, claim the window, build a
  graphics pipeline: vertex input `float3 pos` (loc 0) + `float2 uv` (loc 1),
  depth test (D16), a perspective MVP pushed as a vertex uniform, and a textured
  fragment shader sampling the R8 sheet (grayscale out). Vertex buffer and the
  sheet texture are uploaded via transfer buffers; a linear clamped sampler.
- Per frame: acquire command buffer + swapchain, (re)create the depth target to
  match, begin render pass (clear), bind pipeline/uniform/vertex buffer/sampler,
  draw, submit.

## Shaders [HLSL → SPIR-V]

`tools/vf2_model_viewer/shaders/model.{vert,frag}.hlsl`, compiled to SPIR-V at
build time and embedded via `bin2c.cmake` (no runtime shader files).

> **Toolchain note:** celsowm chose "dxc → SPIR-V", but the only `dxc` here is
> the Windows SDK's, which is **DXIL-only** ("SPIR-V CodeGen not available").
> The same intent (HLSL → SPIR-V at build time, Vulkan, no committed blobs) is
> met with **glslangValidator** (`pacman -S mingw-w64-ucrt-x86_64-glslang`);
> `glslc` also works. CMake picks whichever is found (`VF2_HLSL2SPV`).

The bindings follow SDL_GPU's SPIR-V convention and were verified with
`spirv-dis`: the vertex uniform buffer is **DescriptorSet 1** (`register(b0,
space1)`), the fragment texture+sampler are **DescriptorSet 2** (`register(t0/s0,
space2)`), vertex inputs at locations 0/1. UV V is flipped in the fragment shader
(OBJ V-up → top-origin GPU texture).

## Build

`VF2_BUILD_VIEWER=ON` (default **OFF**, so CI is unaffected) needs SDL3 (3.2+,
for SDL_GPU) and a HLSL→SPIR-V compiler. Builds green under gcc and clang; the
headless `--selftest` passes (loads the mesh, reports no-GPU and exits 0). On a
real GPU the device, shaders, pipeline, vertex buffer, texture and sampler all
construct without error; the final visual check (textured Akira on screen) is
celsowm's.

## Status of the objective

- Export (3a) + SDL_GPU display (3b) are in. What is **not** done, by design:
  **colour** (grayscale luma index only; full colour = luma RAM + palette) and
  **posing** (pieces are per-part model-local; a standing Akira needs the TGP
  joint transforms). Both are follow-ups beyond this objective's slices. See
  `textured_export_v0832.md`, `texture_polygon_link_v0831.md`.
