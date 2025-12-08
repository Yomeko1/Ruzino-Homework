import callable_data;
#include "utils/random.slangh"
import utils.Math.ShadingFrame;
import Utils.Math.MathHelpers;

// Custom checkerboard material shader - EVAL callable
// Procedural checkerboard pattern based on UV coordinates

[shader("callable")]
void eval_custom_checkerboard_material(inout EvalCallableData data)
{
    // Create checkerboard pattern
    float2 uv = data.vertexInfo.texC;
    float scale = 40.0;
    float2 scaledUV = uv * scale;
    float checker = ((int(floor(scaledUV.x)) + int(floor(scaledUV.y))) % 2) == 0 ? 1.0 : 0.0;
    
    // Mix between two colors
    float3 color1 = float3(0.2, 0.5, 1.0); // Blue
    float3 color2 = float3(1.0, 0.8, 0.2); // Orange
    float3 baseColor = lerp(color1, color2, checker);
    
    // Lambertian BRDF evaluation
    float NdotL = max(0.0, dot(data.vertexInfo.normalW, data.L));
    data.color = float4(baseColor * NdotL / 3.14159265359, 1.0);
    
    // Sample next direction using cosine-weighted hemisphere sampling
    data.sampledDir = sample_cosine_hemisphere_concentric(
        random_float2(data.seed), data.pdf);
    
    // Transform from local to world space
    bool valid;
    ShadingFrame sf = ShadingFrame.createSafe(
        data.vertexInfo.normalW, 
        data.vertexInfo.tangentW, 
        valid);
    data.sampledDir = sf.fromLocal(data.sampledDir);
    
    // Lambertian BRDF for throughput
    float NdotSampled = max(0.0, dot(data.vertexInfo.normalW, data.sampledDir));
    data.throughput = baseColor * NdotSampled / 3.14159265359;
}
