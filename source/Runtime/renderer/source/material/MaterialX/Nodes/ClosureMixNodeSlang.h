//
// Copyright Contributors to the MaterialX Project
// SPDX-License-Identifier: Apache-2.0
//
#pragma once
#include <MaterialXGenShader/ShaderNodeImpl.h>

#include "api.h"

MATERIALX_NAMESPACE_BEGIN

/// Closure mix node implementation.
class HD_USTC_CG_API ClosureMixNodeSlang : public ShaderNodeImpl {
   public:
    static ShaderNodeImplPtr create();

    void emitFunctionCall(
        const ShaderNode& node,
        GenContext& context,
        ShaderStage& stage) const override;

    /// String constants
    static const string FG;
    static const string BG;
    static const string MIX;
};

MATERIALX_NAMESPACE_END
