import stdlib.genslang.mx_displacementshader;

void mx_displacement_vector3(
    float3 disp,
    float scale,
    out displacementshader result)
{
    result.offset = disp;
    result.scale = scale;
}
