#pragma once

#include "Layers/xrRender/r_rendertarget_phase.h"
#include "r4_rendertarget_phase_luminance.h"

class CRenderTarget_Phase_Bloom : public IRender_Target_Phase
{
    ref_rt rt_Bloom_1; // 32bit, dim/4	(r,g,b,?)
    ref_rt rt_Bloom_2; // 32bit, dim/4	(r,g,b,?)

    ref_geom g_bloom_build;
    ref_geom g_bloom_filter;

    IBlender* b_bloom{};
    IBlender* b_bloom_msaa{};

    ref_shader s_bloom;
    ref_shader s_bloom_msaa;

    ref_shader s_bloom_dbg_1;
    ref_shader s_bloom_dbg_2;

    float f_bloom_factor{};

    CRenderTarget_Phase_Luminance m_luminance;

public:
    CRenderTarget_Phase_Bloom(CRenderTarget* target);
    ~CRenderTarget_Phase_Bloom() override;

    void Initialize() override;
    void Execute() override;
};
