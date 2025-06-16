//
// Copyright Contributors to the MaterialX Project
// SPDX-License-Identifier: Apache-2.0
//

#include "ClosureCompoundNodeSlang.h"

#include <MaterialXGenShader/GenContext.h>
#include <MaterialXGenShader/HwShaderGenerator.h>
#include <MaterialXGenShader/ShaderGenerator.h>

#include "Logger/Logger.h"
#include "MaterialXGenShader/Nodes/ClosureCompoundNode.h"

MATERIALX_NAMESPACE_BEGIN
ShaderNodeImplPtr ClosureCompoundNodeSlang::create()
{
    return std::make_shared<ClosureCompoundNodeSlang>();
}

void ClosureCompoundNodeSlang::addClassification(ShaderNode& node) const
{
    // Add classification from the graph implementation.
    node.addClassification(_rootGraph->getClassification());
}

// TODO: add real tangent information
static std::string sample_source_code_fallback = R"(
import Utils.Math.MathHelpers;
#include "utils/random.slangh"

float3 sample_fallback(
    inout uint seed,
    out float pdf,
    in MaterialDataBlob blob_data, 
    in VertexInfo vertexInfo
)
{
    // Sample the direction
    float3 sampledDir = sample_cosine_hemisphere_concentric(random_float2(seed), pdf);

    bool valid;
    ShadingFrame sf = ShadingFrame.createSafe(vertexInfo.normalW, float4(0, 0, 0, 1), valid);
    sampledDir = sf.fromLocal(sampledDir);

    return sampledDir;
}

)";

static std::string sample_source_code_standard_surface = R"(
import pbrlib.genslang.mx_roughness_anisotropy;
import pbrlib.genslang.lib.mx_microfacet;
import pbrlib.genslang.lib.mx_microfacet_specular;
import pbrlib.genslang.lib.mx_microfacet_diffuse;
#include "utils/Math/MathConstants.slangh"
#include "utils/random.slangh"

// Calculate luminance of a color
float luminance(float3 color)
{
    return dot(color, float3(0.212671, 0.715160, 0.072169));
}

// Fresnel reflectance using Schlick's approximation
float3 fresnel_schlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Sample diffuse component using cosine hemisphere sampling
float3 sample_diffuse_lobe(float2 u, out float pdf)
{
    float3 L = sample_cosine_hemisphere_concentric(u, pdf);
    return L;
}

// Sample specular reflection using GGX distribution
float3 sample_specular_reflection(float2 u, float3 V, float2 roughness, out float pdf)
{
    // Sample microfacet normal using GGX VNDF distribution
    float3 H = mx_ggx_importance_sample_VNDF(u, V, roughness);
    
    // Reflect view direction around microfacet normal
    float3 L = reflect(-V, H);
    
    // Check if the reflection is valid (above surface)
    if (L.z <= 0.0) {
        pdf = 0.0;
        return float3(0.0);
    }
    
    // Compute PDF using correct VNDF formulation
    float NdotH = max(H.z, M_FLOAT_EPS);
    float VdotH = max(dot(V, H), M_FLOAT_EPS);
    float NdotV = max(V.z, M_FLOAT_EPS);
    
    // VNDF PDF: D(H) * G1(V) * max(0, V·H) / NdotV
    float D = mx_ggx_NDF(H, roughness);
    float G1 = mx_ggx_smith_G1(NdotV, mx_average_alpha(roughness));
    float vndf_pdf = D * G1 * VdotH / NdotV;
    
    // Transform to reflection direction: PDF_L = PDF_H / (4 * V·H)
    pdf = vndf_pdf / (4.0 * VdotH);
    
    return L;
}

// Sample transmission using GGX distribution and Snell's law
float3 sample_transmission(float2 u, float3 V, float2 roughness, float eta, out float pdf)
{
    // Sample microfacet normal using GGX VNDF distribution
    float3 H = mx_ggx_importance_sample_VNDF(u, V, roughness);
    
    // Compute transmission direction using Snell's law
    float VdotH = dot(V, H);
    float discriminant = 1.0 - eta * eta * (1.0 - VdotH * VdotH);
    
    if (discriminant < 0.0) {
        // Total internal reflection
        pdf = 0.0;
        return float3(0.0);
    }
    
    // Correct transmission direction formula: L = eta * V + (eta * VdotH - sqrt(discriminant)) * H
    float3 L = eta * V + (eta * VdotH - sqrt(discriminant)) * H;
    L = normalize(L);
    
    // Check if transmission is valid (below surface for thin_walled=false)
    if (L.z >= 0.0) {
        pdf = 0.0;
        return float3(0.0);
    }
    
    // Compute PDF using correct VNDF and Jacobian
    float NdotH = max(H.z, M_FLOAT_EPS);
    float NdotV = max(V.z, M_FLOAT_EPS);
    float LdotH = max(abs(dot(L, H)), M_FLOAT_EPS);
    
    // VNDF PDF: D(H) * G1(V) * max(0, V·H) / NdotV
    float D = mx_ggx_NDF(H, roughness);
    float G1 = mx_ggx_smith_G1(NdotV, mx_average_alpha(roughness));
    float vndf_pdf = D * G1 * abs(VdotH) / NdotV;
    
    // Transform to transmission direction with correct Jacobian
    float denominator = abs(VdotH + eta * LdotH);
    float jacobian = eta * eta * LdotH / (denominator * denominator);
    pdf = vndf_pdf * jacobian;
    
    return L;
}

float3 sample_standard_surface(
    VertexData vd, 
    SamplerState sampler, 
    float3 V, 
    float base, 
    float3 base_color, 
    float diffuse_roughness, 
    float metalness, 
    float specular, 
    float3 specular_color, 
    float specular_roughness, 
    float specular_IOR, 
    float specular_anisotropy, 
    float specular_rotation, 
    float transmission, 
    float3 transmission_color, 
    float transmission_depth, 
    float3 transmission_scatter, 
    float transmission_scatter_anisotropy, 
    float transmission_dispersion, 
    float transmission_extra_roughness, 
    float subsurface, 
    float3 subsurface_color, 
    float3 subsurface_radius, 
    float subsurface_scale, 
    float subsurface_anisotropy, 
    float sheen, 
    float3 sheen_color, 
    float sheen_roughness, 
    float coat, 
    float3 coat_color, 
    float coat_roughness, 
    float coat_anisotropy, 
    float coat_rotation, 
    float coat_IOR, 
    float3 coat_normal, 
    float coat_affect_color, 
    float coat_affect_roughness, 
    float thin_film_thickness, 
    float thin_film_IOR, 
    float emission, 
    float3 emission_color, 
    float3 opacity, 
    bool thin_walled, 
    float3 normal, 
    float3 tangent,
    uint eta_flipped, // 0 for normal, 1 for flipped
    inout uint seed,
    out float pdf
)
{
    // Create shading frame
    bool valid;
    ShadingFrame sf = ShadingFrame.createSafe(normal, float4(tangent, 1.0), valid);
    
    // Transform view direction to local space
    float3 V_local = sf.toLocal(V);
    float NdotV = clamp(V_local.z, M_FLOAT_EPS, 1.0);

    // Compute anisotropic roughness
    float2 alpha;
    mx_roughness_anisotropy(specular_roughness, specular_anisotropy, alpha);
    
    // Compute F0 for metallic-roughness workflow
    float3 F0 = lerp(float3(0.04), base_color, metalness);
    
    // Compute Fresnel reflectance at normal incidence
    float3 F = fresnel_schlick(NdotV, F0);
    float fresnel_avg = (F.x + F.y + F.z) / 3.0;
    
    // Compute component weights for multiple importance sampling
    float diffuse_weight = base * (1.0 - metalness) * (1.0 - fresnel_avg) * luminance(base_color);
    float specular_weight = luminance(F);
    float transmission_weight = transmission * (1.0 - metalness) * (1.0 - fresnel_avg) * luminance(transmission_color);
    
    // Normalize weights
    float total_weight = diffuse_weight + specular_weight + transmission_weight;
    if (total_weight < M_FLOAT_EPS) {
        pdf = 0.0;
        return float3(0.0);
    }
    
    diffuse_weight /= total_weight;
    specular_weight /= total_weight;
    transmission_weight /= total_weight;
    
    // Sample component based on weights
    float component_sample = random_float(seed);
    float3 L_local;
    float component_pdf;
    
    if (component_sample < diffuse_weight) {
        // Sample diffuse component
        float2 u = random_float2(seed);
        L_local = sample_diffuse_lobe(u, component_pdf);
    }
    else if (component_sample < diffuse_weight + specular_weight) {
        // Sample specular reflection
        float2 u = random_float2(seed);
        L_local = sample_specular_reflection(u, V_local, alpha, component_pdf);
    }
    else {
        // Sample transmission
        float2 u = random_float2(seed);
        float eta = eta_flipped != 0 ? (1.0 / specular_IOR) : specular_IOR;
        L_local = sample_transmission(u, V_local, alpha, eta, component_pdf);
    }
    
    // If sampling failed, return early
    if (component_pdf <= 0.0) {
        pdf = 0.0;
        return float3(0.0);
    }
    
    // Compute MIS weights for all components
    float diffuse_pdf = 0.0;
    float specular_pdf = 0.0;
    float transmission_pdf = 0.0;
    
    if (L_local.z > M_FLOAT_EPS) {
        // Above surface - can be diffuse or specular reflection
        diffuse_pdf = L_local.z * M_1_PI;
        
        // Compute specular PDF using VNDF
        float3 H = normalize(V_local + L_local);
        float NdotH = max(H.z, M_FLOAT_EPS);
        float VdotH = max(dot(V_local, H), M_FLOAT_EPS);
        
        float D = mx_ggx_NDF(H, alpha);
        float G1 = mx_ggx_smith_G1(NdotV, mx_average_alpha(alpha));
        float vndf_pdf = D * G1 * VdotH / NdotV;
        specular_pdf = vndf_pdf / (4.0 * VdotH);
    }
    else if (L_local.z < -M_FLOAT_EPS) {
        // Below surface - transmission
        float eta = eta_flipped == 0 ? (1.0 / specular_IOR) : specular_IOR;
        float3 H = normalize(V_local + eta * L_local);
        
        float NdotH = max(H.z, M_FLOAT_EPS);
        float VdotH = max(dot(V_local, H), M_FLOAT_EPS);
        float LdotH = max(abs(dot(L_local, H)), M_FLOAT_EPS);
        
        float D = mx_ggx_NDF(H, alpha);
        float G1 = mx_ggx_smith_G1(NdotV, mx_average_alpha(alpha));
        float vndf_pdf = D * G1 * VdotH / NdotV;
        
        float denominator = abs(VdotH + eta * LdotH);
        float jacobian = eta * eta * LdotH / (denominator * denominator);
        transmission_pdf = vndf_pdf * jacobian;
    }
    
    // Compute final MIS PDF
    pdf = diffuse_pdf * diffuse_weight + specular_pdf * specular_weight + transmission_pdf * transmission_weight;
    pdf = max(pdf, M_FLOAT_EPS); // Avoid division by zero
    
    // Transform to world space
    float3 sampledDir = sf.fromLocal(L_local);
    
    return sampledDir;
}

)";

void ClosureCompoundNodeSlang::emitFunctionDefinition(
    const ShaderNode& node,
    GenContext& context,
    ShaderStage& stage) const
{
    DEFINE_SHADER_STAGE(stage, Stage::PIXEL)
    {
        const ShaderGenerator& shadergen = context.getShaderGenerator();

        // Emit functions for all child nodes
        shadergen.emitFunctionDefinitions(*_rootGraph, context, stage);

        // Find any closure contexts used by this node
        // and emit the function for each context.
        vector<ClosureContext*> ccts;
        shadergen.getClosureContexts(node, ccts);
        if (ccts.empty()) {
            emitFunctionDefinition(nullptr, context, stage);
        }
        else {
            for (ClosureContext* cct : ccts) {
                emitFunctionDefinition(cct, context, stage);
            }
        }  // Emit the sample fallback and standard surface sampling
        shadergen.emitLine(sample_source_code_fallback, stage, false);
        shadergen.emitLine(sample_source_code_standard_surface, stage, false);
    }
}

void ClosureCompoundNodeSlang::emitFunctionDefinition(
    ClosureContext* cct,
    GenContext& context,
    ShaderStage& stage) const
{
    const ShaderGenerator& shadergen = context.getShaderGenerator();
    const Syntax& syntax = shadergen.getSyntax();

    string delim = "";

    // Begin function signature
    shadergen.emitLineBegin(stage);
    if (cct) {
        // Use the first output for classifying node type for the closure
        // context. This is only relevent for closures, and they only have a
        // single output.
        const TypeDesc* closureType = _rootGraph->getOutputSocket()->getType();

        shadergen.emitString(
            "void " + _functionName + cct->getSuffix(closureType) + "(", stage);

        // Add any extra argument inputs first
        for (const ClosureContext::Argument& arg :
             cct->getArguments(closureType)) {
            const string& type = syntax.getTypeName(arg.first);
            shadergen.emitString(delim + type + " " + arg.second, stage);
            delim = ", ";
        }
    }
    else {
        shadergen.emitString("void " + _functionName + "(", stage);
    }

    auto& vertexData = stage.getInputBlock(HW::VERTEX_DATA);
    if (!vertexData.empty()) {
        shadergen.emitString(
            delim + vertexData.getName() + " " + vertexData.getInstance(),
            stage);
        delim = ", ";
    }

    shadergen.emitString(delim + "SamplerState sampler", stage);

    const string& type = syntax.getTypeName(Type::VECTOR3);
    shadergen.emitString(delim + type + " " + HW::DIR_L, stage);
    shadergen.emitString(", " + type + " " + HW::DIR_V, stage);
    // eta
    shadergen.emitString(delim + "uint eta_flipped", stage);

    // Add all inputs
    for (ShaderGraphInputSocket* inputSocket : _rootGraph->getInputSockets()) {
        shadergen.emitString(
            delim + syntax.getTypeName(inputSocket->getType()) + " " +
                inputSocket->getVariable(),
            stage);
        delim = ", ";
    }

    // Add all outputs
    for (ShaderGraphOutputSocket* outputSocket :
         _rootGraph->getOutputSockets()) {
        shadergen.emitString(
            delim + syntax.getOutputTypeName(outputSocket->getType()) + " " +
                outputSocket->getVariable(),
            stage);
        delim = ", ";
    }

    // End function signature
    shadergen.emitString(")", stage);
    shadergen.emitLineEnd(stage, false);

    // Begin function body
    shadergen.emitFunctionBodyBegin(*_rootGraph, context, stage);

    if (cct) {
        context.pushClosureContext(cct);
    }

    // Emit all texturing nodes. These are inputs to the
    // closure nodes and need to be emitted first.
    shadergen.emitFunctionCalls(
        *_rootGraph, context, stage, ShaderNode::Classification::TEXTURE);

    // Emit function calls for internal closures nodes connected to the graph
    // sockets. These will in turn emit function calls for any dependent closure
    // nodes upstream.
    for (ShaderGraphOutputSocket* outputSocket :
         _rootGraph->getOutputSockets()) {
        if (outputSocket->getConnection()) {
            const ShaderNode* upstream =
                outputSocket->getConnection()->getNode();
            if (upstream->getParent() == _rootGraph.get() &&
                (upstream->hasClassification(
                     ShaderNode::Classification::CLOSURE) ||
                 upstream->hasClassification(
                     ShaderNode::Classification::SHADER))) {
                shadergen.emitFunctionCall(*upstream, context, stage);
            }
        }
    }

    if (cct) {
        context.popClosureContext();
    }

    // Emit final results
    for (ShaderGraphOutputSocket* outputSocket :
         _rootGraph->getOutputSockets()) {
        const string result =
            shadergen.getUpstreamResult(outputSocket, context);
        shadergen.emitLine(outputSocket->getVariable() + " = " + result, stage);
    }

    // End function body
    shadergen.emitFunctionBodyEnd(*_rootGraph, context, stage);
}

void ClosureCompoundNodeSlang::emitFunctionCall(
    const ShaderNode& node,
    GenContext& context,
    ShaderStage& stage) const
{
    const ShaderGenerator& shadergen = context.getShaderGenerator();
    USTC_CG::log::info(
        "Emitting closure compound function call for node: " + node.getName());

    DEFINE_SHADER_STAGE(stage, Stage::VERTEX)
    {
        // Emit function calls for all child nodes to the vertex shader stage
        shadergen.emitFunctionCalls(*_rootGraph, context, stage);
    }

    DEFINE_SHADER_STAGE(stage, Stage::PIXEL)
    {
        // Emit calls for any closure dependencies upstream from this node.
        shadergen.emitDependentFunctionCalls(
            node, context, stage, ShaderNode::Classification::CLOSURE);

        // Declare the output variables
        emitOutputVariables(node, context, stage);

        shadergen.emitLineBegin(stage);
        string delim = "";

        // Check if we have a closure context to modify the function call.
        ClosureContext* cct = context.getClosureContext();
        if (cct) {
            // Use the first output for classifying node type for the closure
            // context. This is only relevent for closures, and they only have a
            // single output.
            const ShaderGraphOutputSocket* outputSocket =
                _rootGraph->getOutputSocket();
            const TypeDesc* closureType = outputSocket->getType();

            // Check if extra parameters has been added for this node.
            const ClosureContext::ClosureParams* params =
                cct->getClosureParams(&node);
            if (*closureType == *Type::BSDF && params) {
                // Assign the parameters to the BSDF.
                for (auto it : *params) {
                    shadergen.emitLine(
                        outputSocket->getVariable() + "." + it.first + " = " +
                            shadergen.getUpstreamResult(it.second, context),
                        stage);
                }
            }

            // Emit function name.
            shadergen.emitString(
                _functionName + cct->getSuffix(closureType) + "(", stage);

            // Emit extra argument.
            for (const ClosureContext::Argument& arg :
                 cct->getArguments(closureType)) {
                shadergen.emitString(delim + arg.second, stage);
                delim = ", ";
            }
        }
        else {
            // Emit function name.
            shadergen.emitString(_functionName + "(", stage);
        }

        auto& vertexData = stage.getInputBlock(HW::VERTEX_DATA);
        if (!vertexData.empty()) {
            shadergen.emitString(delim + vertexData.getInstance(), stage);
            delim = ", ";
        }

        shadergen.emitString(delim + "sampler", stage);

        shadergen.emitString(delim + HW::DIR_L + ", " + HW::DIR_V, stage);
        shadergen.emitString(delim + "eta_flipped", stage);
        // Emit all inputs.
        for (ShaderInput* input : node.getInputs()) {
            shadergen.emitString(delim, stage);
            shadergen.emitInput(input, context, stage);
            delim = ", ";
        }

        // Emit all outputs.
        for (size_t i = 0; i < node.numOutputs(); ++i) {
            shadergen.emitString(delim, stage);
            shadergen.emitOutput(
                node.getOutput(i), false, false, context, stage);
            delim = ", ";
        }  // End function call
        shadergen.emitString(")", stage);
        shadergen.emitLineEnd(stage);

        // Check if this is a standard surface material
        bool isStandardSurface =
            _functionName.find("standard_surface") != string::npos;
        if (isStandardSurface) {
            // Call the standard surface sampling function
            shadergen.emitLineBegin(stage);
            shadergen.emitString(
                "sampled_direction = sample_standard_surface(vd, sampler, V, ",
                stage);

            // Emit all the standard surface parameters
            string delim = "";
            for (ShaderInput* input : node.getInputs()) {
                shadergen.emitString(delim, stage);
                shadergen.emitInput(input, context, stage);
                delim = ", ";
            }
            shadergen.emitString(delim + "eta_flipped", stage);

            shadergen.emitString(", seed, pdf)", stage);
            shadergen.emitLineEnd(stage);
        }
        else {
            // Call the sample fallback, and eval the sampled direction
            shadergen.emitLine(
                "sampled_direction = sample_fallback(seed, pdf, data, "
                "vertexInfo)",
                stage);
        }
        {
            // Use that direction to replace HW::DIR_L and re-evaluate the
            // material
            shadergen.emitLine(
                "surfaceshader sampled_weight_out = "
                "surfaceshader(float3(0.0),float3(0.0));",
                stage);
            shadergen.emitLineBegin(stage);
            string delim = "";

            // Check if we have a closure context to modify the function call.
            ClosureContext* cct = context.getClosureContext();
            if (cct) {
                // Use the first output for classifying node type for the
                // closure context. This is only relevent for closures, and they
                // only have a single output.
                const ShaderGraphOutputSocket* outputSocket =
                    _rootGraph->getOutputSocket();
                const TypeDesc* closureType = outputSocket->getType();

                // Check if extra parameters has been added for this node.
                const ClosureContext::ClosureParams* params =
                    cct->getClosureParams(&node);
                if (*closureType == *Type::BSDF && params) {
                    // Assign the parameters to the BSDF.
                    for (auto it : *params) {
                        shadergen.emitLine(
                            outputSocket->getVariable() + "." + it.first +
                                " = " +
                                shadergen.getUpstreamResult(it.second, context),
                            stage);
                    }
                }

                // Emit function name.
                shadergen.emitString(
                    _functionName + cct->getSuffix(closureType) + "(", stage);

                // Emit extra argument.
                for (const ClosureContext::Argument& arg :
                     cct->getArguments(closureType)) {
                    shadergen.emitString(delim + arg.second, stage);
                    delim = ", ";
                }
            }
            else {
                // Emit function name.
                shadergen.emitString(_functionName + "(", stage);
            }

            auto& vertexData = stage.getInputBlock(HW::VERTEX_DATA);
            if (!vertexData.empty()) {
                shadergen.emitString(delim + vertexData.getInstance(), stage);
                delim = ", ";
            }

            shadergen.emitString(delim + "sampler", stage);

            shadergen.emitString(
                delim + "sampled_direction" + ", " + HW::DIR_V, stage);

            shadergen.emitString(delim + "eta_flipped", stage);

            // Emit all inputs.
            for (ShaderInput* input : node.getInputs()) {
                shadergen.emitString(delim, stage);
                shadergen.emitInput(input, context, stage);
                delim = ", ";
            }

            // Emit all outputs.
            shadergen.emitString(delim, stage);
            shadergen.emitString("sampled_weight_out", stage);
            delim = ", ";

            // End function call
            shadergen.emitString(")", stage);
            shadergen.emitLineEnd(stage);
            shadergen.emitLine(
                "sampled_weight = sampled_weight_out.color", stage);
        }
    }
}

MATERIALX_NAMESPACE_END
