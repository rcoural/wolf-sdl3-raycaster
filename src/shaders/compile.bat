@rem Recompile HLSL shader sources to SPIRV / MSL / DXIL.
@rem Requires `shadercross` from external/SDL_shadercross to be on PATH
@rem (or run from a build dir where shadercross.exe is available).
@echo off
for %%S in (TexturedQuad.vert TexturedQuad.frag) do (
    shadercross "%%S.hlsl" -o "%%S.spv"
    shadercross "%%S.hlsl" -o "%%S.msl"
    shadercross "%%S.hlsl" -o "%%S.dxil"
)
