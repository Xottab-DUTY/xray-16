#pragma once

#include "Layers/xrRender/r_rendertarget_phase.h"

class CRenderTarget_Phase_Luminance : public IRender_Target_Phase
{
    IBlender* b_luminance{};
    ref_shader s_luminance;
    float f_luminance_adapt{};

public:
    CRenderTarget_Phase_Luminance(CRenderTarget* target);
    ~CRenderTarget_Phase_Luminance() override;

    void Initialize() override;
    void Execute() override;
};
