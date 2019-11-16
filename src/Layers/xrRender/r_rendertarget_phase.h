#pragma once

class CRenderTarget;

class IRender_Target_Phase
{
protected:
    CRenderTarget* m_target{};

public:
    IRender_Target_Phase() = delete;
    IRender_Target_Phase(CRenderTarget* target) : m_target(target) {}

    virtual ~IRender_Target_Phase() = default;

    virtual void Initialize() = 0;
    virtual void Execute() = 0;
};
