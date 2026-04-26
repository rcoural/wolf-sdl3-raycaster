// Pass-through vertex shader for a fullscreen textured quad.
// Vertex layout matches the C++ Vertex struct: vec3 position, vec2 uv.

struct Input {
    float3 Position : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
};

struct Output {
    float2 TexCoord : TEXCOORD0;
    float4 Position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.TexCoord = input.TexCoord;
    output.Position = float4(input.Position, 1.0f);
    return output;
}
