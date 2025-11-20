void mx_normalmap_vector2(
    float3 value,
    float2 normal_scale,
    float3 N,
    float3 T,
    float3 B,
    out float3 result)
{
    value =
        (dot(value, value) == 0.0) ? float3(0.0, 0.0, 1.0) : value * 2.0 - 1.0;
    value = T * value.x * normal_scale.x + B * value.y * normal_scale.y +
            N * value.z;
    result = normalize(value);
}

void mx_normalmap_float(
    float3 value,
    float normal_scale,
    float3 N,
    float3 T,
    float3 B,
    out float3 result)
{
    mx_normalmap_vector2(value, float2(normal_scale), N, T, B, result);
}
