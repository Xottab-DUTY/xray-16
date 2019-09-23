#pragma once

class IRender_Target_Phase
{
public:
    virtual ~IRender_Target_Phase() = default;
    virtual void Initialize() = 0;
    virtual void Execute() = 0;
};
