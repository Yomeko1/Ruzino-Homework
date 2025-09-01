
#pragma once

#include <RHI/rhi.hpp>

#include "GUI/api.h"
#include "GUI/widget.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

class GUI_API ImageWidget : public IWidget {
   public:
    explicit ImageWidget(nvrhi::ITexture* texture);
    explicit ImageWidget(nvrhi::TextureHandle texture_handle);
    ~ImageWidget() override = default;

    bool BuildUI() override;

   protected:
    const char* GetWindowName() override;

   private:
    nvrhi::ITexture* texture_;
    nvrhi::TextureHandle texture_handle_;
    bool use_handle_;
};

USTC_CG_NAMESPACE_CLOSE_SCOPE
