#include <GCore/algorithms/intersection.h>

#include <GPUContext/raytracing_context.hpp>

#include "GCore/Components/MeshOperand.h"
#include "GPUContext/program_vars.hpp"
#include "RHI/ResourceManager/resource_allocator.hpp"
#include "RHI/internal/resources.hpp"
#include "RHI/rhi.hpp"
#include "Scene/SceneTypes.slang"
#include "nvrhi/nvrhi.h"
#include "pxr/base/gf/matrix4f.h"
USTC_CG_NAMESPACE_OPEN_SCOPE
ResourceAllocator resource_allocator_;
std::shared_ptr<ShaderFactory> shader_factory;

ResourceAllocator& get_resource_allocator()
{
    return resource_allocator_;
}

void init_gpu_geometry_algorithms()
{
    resource_allocator_.set_device(RHI::get_device());
    shader_factory = std::make_shared<ShaderFactory>();
    shader_factory->add_search_path(RENDERER_SHADER_DIR "shaders");
}

void deinit_gpu_geometry_algorithms()
{
    resource_allocator_.terminate();
}

nvrhi::rt::AccelStructHandle get_geomtry_tlas(
    const Geometry& geometry,
    const pxr::GfMatrix4d& transform,
    MeshDesc* out_mesh_desc = nullptr,
    nvrhi::BufferHandle* out_vertex_buffer = nullptr)
{
    auto mesh_component = geometry.get_component<MeshComponent>();
    if (!mesh_component) {
        return nullptr;
    }

    // First build the BLAS for the mesh
    MeshDesc mesh_desc;
    auto device = RHI::get_device();

    unsigned total_buffer_size = 0;
    unsigned index_buffer_offset = 0;
    unsigned normal_buffer_offset = 0;
    unsigned texcoord_buffer_offset = 0;

    auto vertices = mesh_component->get_vertices();
    auto indices = mesh_component->get_face_vertex_indices();
    auto normals = mesh_component->get_normals();
    auto uvs = mesh_component->get_texcoords_array();

    // Calculate buffer offsets and total size
    total_buffer_size = vertices.size() * 3 * sizeof(float);
    index_buffer_offset = total_buffer_size;
    total_buffer_size += indices.size() * sizeof(int);

    if (!normals.empty()) {
        normal_buffer_offset = total_buffer_size;
        total_buffer_size += normals.size() * 3 * sizeof(float);
    }

    if (!uvs.empty()) {
        texcoord_buffer_offset = total_buffer_size;
        total_buffer_size += uvs.size() * 2 * sizeof(float);
    }

    // Create vertex buffer
    nvrhi::BufferDesc desc =
        nvrhi::BufferDesc{}
            .setCanHaveRawViews(true)
            .setByteSize(total_buffer_size)
            .setIsVertexBuffer(true)
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setCpuAccess(nvrhi::CpuAccessMode::None)
            .setIsAccelStructBuildInput(true)
            .setKeepInitialState(true)
            .setDebugName("meshVertexBuffer");

    auto vertexBuffer = device->createBuffer(desc);

    // Create command list and copy data
    auto copy_commandlist =
        device->createCommandList({ .enableImmediateExecution = false });
    copy_commandlist->open();

    // Copy vertices
    copy_commandlist->writeBuffer(
        vertexBuffer, vertices.data(), vertices.size() * 3 * sizeof(float), 0);

    // Copy indices
    copy_commandlist->writeBuffer(
        vertexBuffer,
        indices.data(),
        indices.size() * sizeof(int),
        index_buffer_offset);

    // Copy normals if available
    if (!normals.empty()) {
        copy_commandlist->writeBuffer(
            vertexBuffer,
            normals.data(),
            normals.size() * 3 * sizeof(float),
            normal_buffer_offset);
    }

    // Copy UVs if available
    if (!uvs.empty()) {
        copy_commandlist->writeBuffer(
            vertexBuffer,
            uvs.data(),
            uvs.size() * 2 * sizeof(float),
            texcoord_buffer_offset);
    }

    copy_commandlist->close();
    device->executeCommandList(copy_commandlist);

    // Set up mesh_desc
    mesh_desc.vbOffset = 0;
    mesh_desc.ibOffset = index_buffer_offset;
    mesh_desc.normalOffset = normal_buffer_offset;
    mesh_desc.texCrdOffset = texcoord_buffer_offset;
    mesh_desc.normalInterpolation = InterpolationType::Vertex;
    mesh_desc.texCrdInterpolation = InterpolationType::Vertex;

    // Create BLAS
    nvrhi::rt::AccelStructDesc blas_desc;
    nvrhi::rt::GeometryDesc geometry_desc;
    geometry_desc.geometryType = nvrhi::rt::GeometryType::Triangles;
    nvrhi::rt::GeometryTriangles triangles;
    triangles.setVertexBuffer(vertexBuffer)
        .setVertexOffset(0)
        .setIndexBuffer(vertexBuffer)
        .setIndexOffset(index_buffer_offset)
        .setIndexCount(indices.size())
        .setVertexCount(vertices.size())
        .setVertexStride(3 * sizeof(float))
        .setVertexFormat(nvrhi::Format::RGB32_FLOAT)
        .setIndexFormat(nvrhi::Format::R32_UINT);
    geometry_desc.setTriangles(triangles);
    blas_desc.addBottomLevelGeometry(geometry_desc);
    blas_desc.isTopLevel = false;
    auto BLAS = device->createAccelStruct(blas_desc);

    auto buildCommandList = device->createCommandList();
    buildCommandList->open();
    nvrhi::utils::BuildBottomLevelAccelStruct(
        buildCommandList, BLAS, blas_desc);
    buildCommandList->close();
    device->executeCommandList(buildCommandList);
    device->waitForIdle();

    // Now create TLAS
    nvrhi::rt::AccelStructDesc tlas_desc;
    tlas_desc.isTopLevel = true;
    tlas_desc.topLevelMaxInstances = 1;
    nvrhi::rt::InstanceDesc instance_desc;
    instance_desc.setBLAS(BLAS);
    instance_desc.setInstanceID(0);
    instance_desc.setInstanceMask(0xFF);
    // set the transform
    nvrhi::rt::AffineTransform affine_transform;

    memcpy(
        affine_transform,
        pxr::GfMatrix4f(transform.GetTranspose()).data(),
        sizeof(nvrhi::rt::AffineTransform));

    instance_desc.setTransform(affine_transform);

    auto TLAS = device->createAccelStruct(tlas_desc);

    buildCommandList = device->createCommandList();
    buildCommandList->open();
    buildCommandList->buildTopLevelAccelStruct(
        TLAS, std::vector{ instance_desc }.data(), 1);
    buildCommandList->close();
    device->executeCommandList(buildCommandList);
    device->waitForIdle();

    // Fill output parameters if provided
    if (out_mesh_desc) {
        *out_mesh_desc = mesh_desc;
    }

    if (out_vertex_buffer) {
        *out_vertex_buffer = vertexBuffer;
    }

    assert(TLAS);
    return TLAS;
}

pxr::VtArray<PointSample> Intersect(
    const pxr::VtArray<pxr::GfRay>& rays,
    const Geometry& BaseMesh)
{
    auto mesh_component = BaseMesh.get_component<MeshComponent>();

    if (!mesh_component) {
        return pxr::VtArray<PointSample>(rays.size());
    }

    pxr::VtArray<pxr::GfVec3f> vertices = mesh_component->get_vertices();

    if (vertices.empty()) {
        return pxr::VtArray<PointSample>(rays.size());
    }

    pxr::VtArray<pxr::GfVec3f> normals = mesh_component->get_normals();
    pxr::VtArray<int> indices = mesh_component->get_face_vertex_indices();
    pxr::VtArray<pxr::GfVec2f> uvs = mesh_component->get_texcoords_array();

    MeshDesc mesh_desc;

    auto& resource_allocator = get_resource_allocator();
    auto device = RHI::get_device();

    nvrhi::BufferHandle vertex_buffer;
    auto accel = get_geomtry_tlas(
        BaseMesh,
        pxr::GfMatrix4d(1.0),
        &mesh_desc,
        std::addressof(vertex_buffer));

    auto commandlist = resource_allocator.create(CommandListDesc{});

    ProgramDesc desc;
    desc.shaderType = nvrhi::ShaderType::AllRayTracing;
    desc.set_path(GEOM_COMPUTE_SHADER_DIR "intersection.slang");
    auto program = shader_factory->createProgram(desc);
    // Create ray buffer with input rays
    auto ray_buffer = resource_allocator.create(
        nvrhi::BufferDesc{}
            .setByteSize(rays.size() * sizeof(pxr::GfRay))
            .setStructStride(sizeof(pxr::GfRay))
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true)
            .setCpuAccess(nvrhi::CpuAccessMode::Write)
            .setDebugName("rayBuffer"));

    // Copy rays to the ray buffer
    void* data = device->mapBuffer(ray_buffer, nvrhi::CpuAccessMode::Write);
    memcpy(data, rays.data(), rays.size() * sizeof(pxr::GfRay));
    device->unmapBuffer(ray_buffer);

    // Create output buffer for intersection results
    auto result_buffer = resource_allocator.create(
        nvrhi::BufferDesc{}
            .setByteSize(rays.size() * sizeof(PointSample))
            .setStructStride(sizeof(PointSample))
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true)
            .setCanHaveUAVs(true)
            .setDebugName("resultBuffer"));

    auto mesh_cb = resource_allocator.create(
        nvrhi::BufferDesc{}
            .setByteSize(sizeof(MeshDesc))
            .setStructStride(sizeof(MeshDesc))
            .setInitialState(nvrhi::ResourceStates::ConstantBuffer)
            .setKeepInitialState(true)
            .setCpuAccess(nvrhi::CpuAccessMode::Write)
            .setIsConstantBuffer(true)
            .setDebugName("meshDescBuffer"));

    data = device->mapBuffer(mesh_cb, nvrhi::CpuAccessMode::Write);
    memcpy(data, &mesh_desc, sizeof(MeshDesc));
    device->unmapBuffer(mesh_cb);

    ProgramVars program_vars(resource_allocator, program);
    // Bind resources to the shader
    program_vars["mesh"] = mesh_cb;
    program_vars["g_vertexBuffer"] = vertex_buffer;
    program_vars["g_rays"] = ray_buffer;
    program_vars["g_Result"] = result_buffer;
    program_vars["SceneBVH"] = accel;

    program_vars.finish_setting_vars();

    RaytracingContext raytracing_context(resource_allocator, program_vars);

    // Set up shader names
    raytracing_context.announce_raygeneration("RayGen");
    raytracing_context.announce_hitgroup("ClosestHit");
    raytracing_context.announce_miss("Miss");
    raytracing_context.finish_announcing_shader_names();

    // Execute ray tracing
    raytracing_context.begin();
    raytracing_context.trace_rays({}, program_vars, rays.size(), 1, 1);
    raytracing_context.finish();

    // Read back results
    pxr::VtArray<PointSample> result;
    result.resize(rays.size());

    // Create readback buffer
    auto readback_buffer = resource_allocator.create(
        nvrhi::BufferDesc{}
            .setByteSize(rays.size() * sizeof(PointSample))
            .setCpuAccess(nvrhi::CpuAccessMode::Read)
            .setDebugName("resultReadbackBuffer"));

    // Create command list to copy data
    auto copy_commandlist = device->createCommandList();
    copy_commandlist->open();
    copy_commandlist->copyBuffer(
        readback_buffer,
        0,
        result_buffer,
        0,
        rays.size() * sizeof(PointSample));
    copy_commandlist->close();
    device->executeCommandList(copy_commandlist);
    device->waitForIdle();

    // Map and read the results
    void* mapped_data =
        device->mapBuffer(readback_buffer, nvrhi::CpuAccessMode::Read);
    memcpy(result.data(), mapped_data, rays.size() * sizeof(PointSample));
    device->unmapBuffer(readback_buffer);

    resource_allocator.destroy(accel);
    resource_allocator.destroy(vertex_buffer);
    resource_allocator.destroy(ray_buffer);
    resource_allocator.destroy(result_buffer);
    resource_allocator.destroy(mesh_cb);
    resource_allocator.destroy(readback_buffer);
    resource_allocator.destroy(program);
    resource_allocator.destroy(commandlist);
    resource_allocator.destroy(copy_commandlist);

    return result;
}

USTC_CG_NAMESPACE_CLOSE_SCOPE
