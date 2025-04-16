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
    pxr::GfVec3f tangent;
    pxr::GfVec2f uv;
    bool valid;
};

GEOMETRY_API pxr::VtArray<PointSample> Intersect(
    const pxr::VtArray<pxr::GfRay>& rays,
    const Geometry& BaseMesh);

GEOMETRY_API pxr::VtArray<PointSample> Intersect(
    const pxr::VtArray<pxr::GfVec3f>& start_point,
    const pxr::VtArray<pxr::GfVec3f>& next_point,
    const Geometry& BaseMesh);

// result should be of size start_point.size() * next_point.size()
GEOMETRY_API pxr::VtArray<PointSample> IntersectInterweaved(
    const pxr::VtArray<pxr::GfVec3f>& start_point,
    const pxr::VtArray<pxr::GfVec3f>& next_point,
    const Geometry& BaseMesh);

USTC_CG_NAMESPACE_CLOSE_SCOPE
