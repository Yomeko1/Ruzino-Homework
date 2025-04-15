#pragma once

#include <pxr/base/gf/ray.h>

#include "GCore/GOP.h"
#include "GCore/api.h"

USTC_CG_NAMESPACE_OPEN_SCOPE
GEOMETRY_API void init_gpu_geometry_algorithms();
GEOMETRY_API void deinit_gpu_geometry_algorithms();

struct GEOMETRY_API PointSample {
    pxr::GfVec3f position;
    pxr::GfVec3f normal;
    bool valid;
};

GEOMETRY_API pxr::VtArray<PointSample> Intersect(
    const pxr::VtArray<pxr::GfRay>& rays,
    const Geometry& BaseMesh);

USTC_CG_NAMESPACE_CLOSE_SCOPE
