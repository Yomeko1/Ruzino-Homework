#include "path.h"

#include <random>

#include "../surfaceInteraction.h"
#include "../utils/sampling.hpp"
RUZINO_NAMESPACE_OPEN_SCOPE
using namespace pxr;

VtValue PathIntegrator::Li(const GfRay& ray, std::default_random_engine& random)
{
    std::uniform_real_distribution<float> uniform_dist(
        0.0f, 1.0f - std::numeric_limits<float>::epsilon());
    std::function<float()> uniform_float = std::bind(uniform_dist, random);

    auto color = EstimateOutGoingRadiance(ray, uniform_float, 0);

    return VtValue(GfVec3f(color[0], color[1], color[2]));
}

GfVec3f PathIntegrator::EstimateOutGoingRadiance(
    const GfRay& ray,
    const std::function<float()>& uniform_float,
    int recursion_depth)
{
    if (recursion_depth >= 50) {
        return {};
    }

    SurfaceInteraction si;
    if (!Intersect(ray, si)) {
        return IntersectDomeLight(ray);
    }

    // This can be customized : Do we want to see the lights? (Other than dome
    // lights?)
    if (recursion_depth == 0) {
    }

    // Flip the normal if opposite
    if (GfDot(si.shadingNormal, ray.GetDirection()) > 0) {
        si.flipNormal();
        si.PrepareTransforms();
    }

    GfVec3f color{ 0 };
    GfVec3f directLight = EstimateDirectLight(si, uniform_float);

    GfVec3f globalLight = GfVec3f{ 0.f };

    GfVec3f w_i;
    float pdf;
    float rr_prob = 0.9f;
    GfVec3f sampled_dir = UniformSampleHemiSphere(
        GfVec2f(uniform_float(), uniform_float()), pdf);

    w_i = si.TangentToWorld(sampled_dir);

    if (uniform_float() < rr_prob) {
        GfVec3f offsetPos = si.position + si.geometricNormal * 0.0001f;
        GfRay indirect_ray;
        indirect_ray.SetEnds(offsetPos, offsetPos + w_i * 10000.0f);
        GfVec3f indirect_radiance = EstimateOutGoingRadiance(indirect_ray, uniform_float, recursion_depth + 1);
        GfVec3f brdfVal = si.Eval(w_i);
        float cosTheta = std::abs(GfDot(si.shadingNormal, w_i));
        globalLight = GfCompMult(indirect_radiance, brdfVal) * cosTheta / (pdf * rr_prob);
    }

    color = directLight + globalLight;

    return color;
}

RUZINO_NAMESPACE_CLOSE_SCOPE
