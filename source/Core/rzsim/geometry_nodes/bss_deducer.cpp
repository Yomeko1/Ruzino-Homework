#include <memory>

#include "GCore/Components/MeshComponent.h"
#include "GCore/geom_payload.hpp"
#include "nodes/core/def/node_def.hpp"
#include "rzsim/reduced_order_basis.h"
#include "rzpython/rzpython.hpp"

using namespace Ruzino;

NODE_DEF_OPEN_SCOPE

NODE_DECLARATION_FUNCTION(bss_deducer)
{
    b.add_input<Geometry>("Geometry");
    b.add_output<Geometry>("Geometry");
    b.add_output<std::shared_ptr<ReducedOrderedBasis>>("Reduced Basis");
}

NODE_EXECUTION_FUNCTION(bss_deducer)
{
    auto input_geom = params.get_input<Geometry>("Geometry");
    auto reduced_basis = std::make_shared<ReducedOrderedBasis>();

    try {
        // Import torch and test module
        python::call<void>("import torch");
        python::call<void>(
            "import sys\n"
            "sys.path.insert(0, r'C:\\Users\\Pengfei\\WorkSpace\\Ruzino\\source\\Core\\rzsim\\geometry_nodes')\n"
            "import test");

        // Get CUDA tensor from Python
        python::call<void>("cuda_tensor = test.get_cuda_tensor()");
        auto tensor = python::call<python::cuda_array_f32>("cuda_tensor");

        spdlog::info("Retrieved CUDA tensor: shape=[{}], device={}", 
                     tensor.shape(0), static_cast<int>(tensor.device_type()));

        // Copy to CPU for data access
        python::call<void>("cpu_tensor = cuda_tensor.cpu()");
        auto cpu_tensor = python::call<python::cpu_array_f32>("cpu_tensor");

        float* data = cpu_tensor.data();
        spdlog::info("Tensor values: [{}, {}, {}, {}, {}]", 
                     data[0], data[1], data[2], data[3], data[4]);
    }
    catch (const std::exception& e) {
        spdlog::error("Python call failed: {}", e.what());
    }

    params.set_output<Geometry>("Geometry", std::move(input_geom));
    params.set_output<std::shared_ptr<ReducedOrderedBasis>>(
        "Reduced Basis", std::move(reduced_basis));

    return true;
}

NODE_DECLARATION_UI(bss_deducer);

NODE_DEF_CLOSE_SCOPE