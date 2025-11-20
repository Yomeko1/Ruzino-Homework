import stdlib.genslang.lib.mx_transform_uv;

void mx_image_vector2(

    Texture2D texture,
    int layer,
    float2 defaultval,
    float2 texcoord,
    int uaddressmode,
    int vaddressmode,
    int filtertype,
    int framerange,
    int frameoffset,
    int frameendaction,
    float2 uv_scale,
    float2 uv_offset,
    out float2 result)
{
    float2 uv = mx_transform_uv(texcoord, uv_scale, uv_offset);
    int samplerIndex = (uaddressmode * 3) + filtertype;
    samplerIndex = clamp(samplerIndex, 0, 8);
    result = texture.SampleLevel(samplers[samplerIndex], uv, 0.0).rg;
}
