#include <gtest/gtest.h>

#include <RHI/rhi.hpp>

#include "GUI/window.h"
#include "rzpython/rzpython.hpp"

using namespace USTC_CG;

TEST(RZPythonRuntimeTest, RHI_package)
{
    python::initialize();

    python::import("RHI_py");
    int result = python::call<int>("RHI_py.init()");
    EXPECT_EQ(result, 0);

    // Now these should work with dynamic type conversion
    nvrhi::IDevice* device =
        python::call<nvrhi::IDevice*>("RHI_py.get_device()");
    EXPECT_NE(device, nullptr);

    nvrhi::GraphicsAPI backend =
        python::call<nvrhi::GraphicsAPI>("RHI_py.get_backend()");
    // GraphicsAPI is an enum, should be a valid value

    result = python::call<int>("RHI_py.shutdown()");
    EXPECT_EQ(result, 0);

    python::finalize();
}

TEST(RZPythonRuntimeTest, GUI_package)
{
    python::initialize();

    python::import("GUI_py");

    Window window;
    // Note: window.run() starts a message loop, so we comment it out for
    // testing window.run();
    python::reference("w", &window);  // Or some other kind of reference
    python::call<void>("print(w.get_elapsed_time())");

    float time = python::call<float>("w.get_elapsed_time()");
    EXPECT_GT(time, 0.0f);

    python::finalize();
}
