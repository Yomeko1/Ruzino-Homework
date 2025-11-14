#include "RHI/rhi.hpp"

#include <gtest/gtest.h>

TEST(CreateRHI, create_rhi)
{
    EXPECT_EQ(USTC_CG::RHI::init(), 0);
    EXPECT_TRUE(USTC_CG::RHI::get_device() != nullptr);
    EXPECT_EQ(USTC_CG::RHI::shutdown(), 0);
}

TEST(CreateRHI, create_rhi_with_window)
{
    EXPECT_EQ(USTC_CG::RHI::init(true), 0);
    EXPECT_TRUE(USTC_CG::RHI::get_device() != nullptr);
    EXPECT_TRUE(USTC_CG::RHI::internal::get_device_manager() != nullptr);
    EXPECT_EQ(USTC_CG::RHI::shutdown(), 0);
}

#ifndef __linux__
TEST(CreateRHI, create_rhi_with_dx12)
{
    EXPECT_EQ(USTC_CG::RHI::init(false, true), 0);
    EXPECT_TRUE(USTC_CG::RHI::get_device() != nullptr);
    EXPECT_TRUE(USTC_CG::RHI::internal::get_device_manager() != nullptr);
    EXPECT_EQ(USTC_CG::RHI::shutdown(), 0);
}

TEST(CreateRHI, create_rhi_with_window_and_dx12)
{
    EXPECT_EQ(USTC_CG::RHI::init(true, true), 0);
    EXPECT_TRUE(USTC_CG::RHI::get_device() != nullptr);
    EXPECT_TRUE(USTC_CG::RHI::internal::get_device_manager() != nullptr);
    EXPECT_EQ(USTC_CG::RHI::shutdown(), 0);
}
#endif