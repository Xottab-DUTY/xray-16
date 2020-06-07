#include "stdafx.h"
#include "Layers/xrRender/dxRenderFactory.h"
#include "Layers/xrRender/dxUIRender.h"
#include "Layers/xrRender/dxDebugRender.h"
#include "Layers/xrRender/D3DUtils.h"

constexpr pcstr RENDERER_R11_MODE = "renderer_r11";

class R11RendererModule final : public RendererModule
{
    xr_vector<pcstr> modes;

public:
    const xr_vector<pcstr>& ObtainSupportedModes() override
    {
        if (!modes.empty())
        {
            return modes;
        }
        //CHW hw;
        //hw.CreateD3D();
        //if (hw.pD3D)
        {
            modes.emplace_back(RENDERER_R11_MODE);
        }
        return modes;
    }

    void CheckModeConsistency(pcstr mode) const
    {
        R_ASSERT3(0 == xr_strcmp(mode, RENDERER_R11_MODE),
            "Wrong mode passed to xrRender_R11.dll", mode);
    }

    void SetupEnv(pcstr mode) override
    {
        CheckModeConsistency(mode);
        GEnv.Render = &RImplementation;
        GEnv.RenderFactory = &RenderFactoryImpl;
        GEnv.DU = &DUImpl;
        GEnv.UIRender = &UIRenderImpl;
#ifdef DEBUG
        GEnv.DRender = &DebugRenderImpl;
#endif
        xrRender_initconsole();
    }
} static s_r11_module;

extern "C"
{
XR_EXPORT RendererModule* GetRendererModule()
{
    return &s_r11_module;
}
}
