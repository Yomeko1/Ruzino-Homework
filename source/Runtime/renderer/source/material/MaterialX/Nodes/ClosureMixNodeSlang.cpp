//
// Copyright Contributors to the MaterialX Project
// SPDX-License-Identifier: Apache-2.0
//

#include "ClosureMixNodeSlang.h"

#include <MaterialXGenShader/GenContext.h>
#include <MaterialXGenShader/ShaderGenerator.h>
#include <MaterialXGenShader/TypeDesc.h>

MATERIALX_NAMESPACE_BEGIN

const string ClosureMixNodeSlang::FG = "fg";
const string ClosureMixNodeSlang::BG = "bg";
const string ClosureMixNodeSlang::MIX = "mix";

ShaderNodeImplPtr ClosureMixNodeSlang::create()
{
    return std::make_shared<ClosureMixNodeSlang>();
}

void ClosureMixNodeSlang::emitFunctionCall(
    const ShaderNode& _node,
    GenContext& context,
    ShaderStage& stage) const
{
    DEFINE_SHADER_STAGE(stage, Stage::PIXEL)
    {
        const ShaderGenerator& shadergen = context.getShaderGenerator();
        ClosureContext* cct = context.getClosureContext();

        ShaderNode& node = const_cast<ShaderNode&>(_node);

        ShaderInput* fg = node.getInput(FG);
        ShaderInput* bg = node.getInput(BG);
        ShaderInput* mix = node.getInput(MIX);

        // If the add node has closure parameters set,
        // we pass this on to both components.

        if (fg->getConnection()) {
            // Make sure it's a connection to a sibling and not the graph
            // interface.
            ShaderNode* fgNode = fg->getConnection()->getNode();
            if (fgNode->getParent() == node.getParent()) {
                ScopedSetClosureParams setParams(&node, fgNode, cct);
                shadergen.emitFunctionCall(*fgNode, context, stage);
            }
        }
        if (bg->getConnection()) {
            // Make sure it's a connection to a sibling and not the graph
            // interface.
            ShaderNode* bgNode = bg->getConnection()->getNode();
            if (bgNode->getParent() == node.getParent()) {
                ScopedSetClosureParams setParams(&node, bgNode, cct);
                shadergen.emitFunctionCall(*bgNode, context, stage);
            }
        }

        const string fgResult = shadergen.getUpstreamResult(fg, context);
        const string bgResult = shadergen.getUpstreamResult(bg, context);
        const string mixResult = shadergen.getUpstreamResult(mix, context);

        ShaderOutput* output = node.getOutput();
        if (*output->getType() == *Type::BSDF) {
            emitOutputVariables(node, context, stage);
            shadergen.emitLine(
                output->getVariable() + ".response = lerp(" + bgResult +
                    ".response, " + fgResult + ".response, " + mixResult + ")",
                stage);
            shadergen.emitLine(
                output->getVariable() + ".throughput = lerp(" + bgResult +
                    ".throughput, " + fgResult + ".throughput, " + mixResult +
                    ")",
                stage);
        }
        else if (*output->getType() == *Type::EDF) {
            shadergen.emitLine(
                shadergen.getSyntax().getTypeName(Type::EDF) + " " +
                    output->getVariable() + " = lerp(" + bgResult + ", " +
                    fgResult + ", " + mixResult + ")",
                stage);
        }
    }
}

MATERIALX_NAMESPACE_END
