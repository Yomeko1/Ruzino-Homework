import Scene.SceneTypes;

struct CallableData {
    float4 color;

    float3 throughput;
    float3 L;
    float3 V;
    float3 sampledDir;
    float pdf;
    uint seed;
    uint eta_flipped;
    uint materialBlobID;
    VertexInfo vertexInfo;
};

