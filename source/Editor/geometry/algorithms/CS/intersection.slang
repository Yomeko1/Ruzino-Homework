// RayGen shader
import gfray;
import pointsamples;
import Scene.SceneTypes;

ConstantBuffer<MeshDesc> mesh;
ByteAddressBuffer g_vertexBuffer;  // Vertex buffer
StructuredBuffer<GfRay> g_rays;    // Input rays
RWStructuredBuffer<PointSample> g_Result;
RaytracingAccelerationStructure SceneBVH;

struct RayPayload {
    float3 position;
    float3 normal;
    float3 tangent;
    float2 texCoord;
    bool valid;
};

// Helper function to load a vertex from the vertex buffer
StaticVertexData load_vertex(uint vertex_id)
{
    var vb_offset = mesh.vbOffset;
    var normal_offset = mesh.normalOffset;

    uint actual_vertex_id = vertex_id;
    if (mesh.ibOffset > 0) {
        actual_vertex_id = g_vertexBuffer.Load(mesh.ibOffset + vertex_id * 4);
    }

    float3 pos =
        asfloat(g_vertexBuffer.Load3(3 * actual_vertex_id * 4 + vb_offset));
    float3 normal;

    if (mesh.normalInterpolation == InterpolationType::FaceVarying) {
        normal =
            asfloat(g_vertexBuffer.Load3(3 * vertex_id * 4 + normal_offset));
    }
    else {
        normal = asfloat(
            g_vertexBuffer.Load3(3 * actual_vertex_id * 4 + normal_offset));
    }

    if (normal_offset == 0)
        normal = float3(0, 1, 0);

    float2 texCrd;
    if (mesh.texCrdInterpolation == InterpolationType::FaceVarying) {
        texCrd = asfloat(
            g_vertexBuffer.Load2(2 * vertex_id * 4 + mesh.texCrdOffset));
    }
    else {
        texCrd = asfloat(
            g_vertexBuffer.Load2(2 * actual_vertex_id * 4 + mesh.texCrdOffset));
    }

    uint subset_material_id = -1;
    if (mesh.subsetMatIdOffset != 0) {
        subset_material_id =
            g_vertexBuffer.Load(mesh.subsetMatIdOffset + vertex_id * 4);
    }

    StaticVertexData data = StaticVertexData(
        pos, normal, float4(1, 0, 0, 1), texCrd, 0, subset_material_id);
    return data;
}

[shader("raygeneration")]
void RayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;
    uint linearIndex = launchIndex.y * launchDim.x + launchIndex.x;

    // Ray direction comes from input rays
    float3 origin = g_rays[linearIndex].origin;
    float3 direction = g_rays[linearIndex].direction;

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.0;
    ray.TMax = 10000.0;

    RayPayload payload;
    payload.valid = false;

    TraceRay(
        SceneBVH,       // Acceleration structure
        RAY_FLAG_NONE,  // Ray flags
        0xFF,           // Instance inclusion mask
        0,              // Hit group index
        1,              // Number of hit groups
        0,              // Miss shader index
        ray,            // Ray description
        payload         // Ray payload
    );

    // Store result
    g_Result[linearIndex].position = payload.position;
    g_Result[linearIndex].normal = payload.normal;
    g_Result[linearIndex].valid = payload.valid;
}

// Closest hit shader
[shader("closesthit")]
void ClosestHit(
    inout RayPayload payload,
    in BuiltInTriangleIntersectionAttributes attribs)
{
    // Get the primitive index
    uint primitiveIndex = PrimitiveIndex();

    // Load the three vertices of the hit triangle
    uint i0 = primitiveIndex * 3;
    uint i1 = primitiveIndex * 3 + 1;
    uint i2 = primitiveIndex * 3 + 2;

    StaticVertexData v[3];
    v[0] = load_vertex(i0);
    v[1] = load_vertex(i1);
    v[2] = load_vertex(i2);

    // Barycentric coordinates
    float3 barycentrics = float3(
        1.0 - attribs.barycentrics.x - attribs.barycentrics.y,
        attribs.barycentrics.x,
        attribs.barycentrics.y);

    // Interpolate position
    float3 hitPosition = v[0].position * barycentrics.x +
                         v[1].position * barycentrics.y +
                         v[2].position * barycentrics.z;

    // Interpolate normal
    float3 hitNormal = normalize(
        v[0].normal * barycentrics.x + v[1].normal * barycentrics.y +
        v[2].normal * barycentrics.z);

    // Interpolate texture coordinates
    float2 hitTexCoord = v[0].texCrd * barycentrics.x +
                         v[1].texCrd * barycentrics.y +
                         v[2].texCrd * barycentrics.z;

    var objectToWorld = ObjectToWorld4x3();

    // Transform position and normal to world space
    payload.position = mul(float4(hitPosition, 1.0), objectToWorld).xyz;
    payload.normal = normalize(mul(float4(hitNormal, 0.0), objectToWorld).xyz);
    payload.texCoord = hitTexCoord;
    payload.valid = true;
}

// Miss shader
[shader("miss")]
void Miss(inout RayPayload payload)
{
    // No hit found
    payload.valid = false;
    payload.position = float3(0, 0, 0);
    payload.normal = float3(0, 0, 0);
    payload.texCoord = float2(0, 0);
}
