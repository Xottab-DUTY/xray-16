#include "StdAfx.h"

#include "UIEditKeyBind.h"
#include "xrEngine/xr_level_controller.h"
#include "Common/object_broker.h"
#include "xrEngine/XR_IOConsole.h"

CUIEditKeyBind::CUIEditKeyBind(bool primary, bool isGamepadBinds /*= false*/)
    : CUIStatic("CUIEditKeyBind")
{
    m_primary = primary;
    m_isGamepadBinds = isGamepadBinds;
    m_isEditMode = false;
    TextItemControl()->SetTextComplexMode(false);
    m_keyboard = NULL;
    m_opt_backup_value = NULL;
    m_action = NULL;
}

u32 CutStringByLength(CGameFont* font, LPCSTR src, pstr dst, u32 dst_size, float length)
{
    if (font->IsMultibyte())
    {
        u16 nPos = font->GetCutLengthPos(length, src);
        VERIFY(nPos < dst_size);
        strncpy_s(dst, dst_size, src, nPos);
        dst[nPos] = '\0';
        return nPos;
    }
    else
    {
        float text_len = font->SizeOf_(src);
        UI().ClientToScreenScaledWidth(text_len);
        VERIFY(xr_strlen(src) <= dst_size);
        xr_strcpy(dst, dst_size, src);

        while (text_len > length)
        {
            dst[xr_strlen(dst) - 1] = 0;
            VERIFY(xr_strlen(dst));
            text_len = font->SizeOf_(dst);
            UI().ClientToScreenScaledWidth(text_len);
        }

        return xr_strlen(dst);
    }
}

void CUIEditKeyBind::SetText(const char* text)
{
    if (!text || 0 == xr_strlen(text))
        TextItemControl()->SetText("---");
    else
    {
        string256 buff;

        CutStringByLength(TextItemControl()->GetFont(), text, buff, sizeof(buff), GetWidth());

        TextItemControl()->SetText(buff);
    }
}

void CUIEditKeyBind::InitKeyBind(Fvector2 pos, Fvector2 size)
{
    CUIStatic::SetWndPos(pos);
    CUIStatic::SetWndSize(size);
    InitTexture("ui_listline2");
    TextItemControl()->SetFont(UI().Font().pFontLetterica16Russian);
    SetStretchTexture(true);
    SetEditMode(false);
}

void CUIEditKeyBind::OnFocusLost()
{
    CUIStatic::OnFocusLost();
    SetEditMode(false);
    TextItemControl()->SetTextColor((subst_alpha(TextItemControl()->GetTextColor(), color_get_A(0xffffffff))));
}

bool CUIEditKeyBind::OnMouseDown(int mouse_btn)
{
    if (m_isEditMode)
    {
        string64 message;

        keyboard_key* prev_key = m_keyboard;

        m_keyboard = DikToPtr(mouse_btn, true);
        if (!m_keyboard)
            return true;

        if (prev_key != m_keyboard || Device.dwTimeContinual - m_press_time >= DOUBLE_CLICK_TIME)
        {
            m_press_time = Device.dwTimeContinual;
            m_wait_double_click = true;
            return false;
        }
        m_double_click = true;

        OnActionBind();
        return true;
    }

    if (mouse_btn == MOUSE_1)
        SetEditMode(m_bCursorOverWindow);

    return CUIStatic::OnMouseDown(mouse_btn);
}

bool CUIEditKeyBind::OnKeyboardAction(int dik, EUIMessages keyboard_action)
{
    if (dik == MOUSE_1 || dik == MOUSE_2 || dik == MOUSE_3)
        return false;

    if (CUIStatic::OnKeyboardAction(dik, keyboard_action))
        return true;

    string64 message;
    if (m_isEditMode)
    {
        const bool is_gamepad_key = dik > XR_CONTROLLER_BUTTON_INVALID && dik < XR_CONTROLLER_BUTTON_MAX;

        // strictly separate keyboard/mouse from gamepad bindings
        if (m_isGamepadBinds != is_gamepad_key)
            return true;

        keyboard_key* prev_key = m_keyboard;

        m_keyboard = DikToPtr(dik, true);
        if (!m_keyboard)
            return true;

        if (prev_key != m_keyboard || Device.dwTimeContinual - m_press_time >= DOUBLE_CLICK_TIME)
        {
            m_press_time = Device.dwTimeContinual;
            m_wait_double_click = true;
            return false;
        }
        m_double_click = true;

        OnActionBind();
        return true;
    }
    return false;
}

bool CUIEditKeyBind::OnControllerAction(int axis, float x, float y, EUIMessages controller_action)
{
    if (CUIStatic::OnControllerAction(axis, x, y, controller_action))
        return true;

    if (m_isEditMode)
    {
        // strictly separate keyboard/mouse from gamepad bindings
        if (!m_isGamepadBinds)
            return true;

        keyboard_key* prev_key = m_keyboard;

        m_keyboard = DikToPtr(axis, true);
        if (!m_keyboard)
            return true;

        if (prev_key != m_keyboard || Device.dwTimeContinual - m_press_time >= DOUBLE_CLICK_TIME)
        {
            m_press_time = Device.dwTimeContinual;
            m_wait_double_click = true;
            return false;
        }

        m_double_click = true;

        OnActionBind();
        return true;
    }

    return false;
}

void CUIEditKeyBind::Update()
{ 
    CUIStatic::Update();

    if (m_wait_double_click)
    {
        if (Device.dwTimeContinual - m_press_time >= DOUBLE_CLICK_TIME)
        {
            m_double_click = false;
            OnActionBind();
        }
    }
}

void CUIEditKeyBind::SetEditMode(bool b)
{
    m_isEditMode = b;

    if (b)
    {
        SetColorAnimation("ui_map_area_anim", LA_CYCLIC | LA_ONLYALPHA | LA_TEXTCOLOR);
        TextureOn();
    }
    else
    {
        SetColorAnimation(NULL, 0);
        TextureOff();
    }
}

void CUIEditKeyBind::AssignProps(const shared_str& entry, const shared_str& group)
{
    CUIOptionsItem::AssignProps(entry, group);
    m_action = ActionNameToPtr(entry.c_str());
}

void CUIEditKeyBind::SetValue()
{
    if (m_keyboard)
    {
        string64 text;
        strconcat(text, m_keyboard->key_local_name.c_str(), m_double_click ? " (x2)" : "");
        SetText(text);
    }
    else
        SetText(NULL);
}

void CUIEditKeyBind::SetCurrentOptValue()
{
    VERIFY(m_action);
    if (!m_action)
        return;
    key_binding* binding = &g_key_bindings[m_action->id];

    int idx = (!m_isGamepadBinds) ? ((m_primary) ? 0 : 1) : 2;
    m_keyboard = binding->m_keyboard[idx];

    SetValue();
}

void CUIEditKeyBind::SaveOptValue()
{
    CUIOptionsItem::SaveOptValue();
    BindAction2Key();
}

void CUIEditKeyBind::SaveBackUpOptValue()
{
    CUIOptionsItem::SaveBackUpOptValue();
    m_opt_backup_value = m_keyboard;
}

void CUIEditKeyBind::UndoOptValue()
{
    m_keyboard = m_opt_backup_value;
    CUIOptionsItem::UndoOptValue();
}

bool CUIEditKeyBind::IsChangedOptValue() const
{
    return m_keyboard != m_opt_backup_value;
}

void CUIEditKeyBind::OnActionBind()
{
    m_wait_double_click = false;
    m_press_time = 0;

    SetValue();
    OnFocusLost();

    string64 message;
    strconcat(message, m_action->action_name, "=", m_keyboard->key_name, "=", m_double_click ? "true" : "false");
    SendMessage2Group("key_binding", message);
}

void CUIEditKeyBind::BindAction2Key()
{
    if (m_keyboard)
    {
        xr_string comm_bind = (!m_isGamepadBinds) ? ((m_primary) ? "bind " : "bind_sec ") : "bind_gpad ";
        comm_bind += m_action->action_name;
        comm_bind += " ";
        comm_bind += m_keyboard->key_name;
        if (m_double_click)
            comm_bind += "double";
        Console->Execute(comm_bind.c_str());
    }
}

void CUIEditKeyBind::OnMessage(LPCSTR message)
{
    // message = "command=key=double click"
    string64 command{};
    string64 key{};
    string64 double_click_str{};
    sscanf(message, "%s=%s=%s", command, key, double_click_str);

    if (!m_keyboard)
        return;

    if (0 != xr_strcmp(m_keyboard->key_name, key))
        return;

    if (0 == xr_strcmp(m_action->action_name, command))
        return; // fuck

    key_binding& binding = g_key_bindings[m_action->id];

    for (u8 i = 0; i < bindtypes_count; ++i)
    {
        if (binding.m_keyboard[i] == m_keyboard)
        {
            const bool double_click = xr_stricmp(double_click_str, "true") == 0;
            if (binding.m_double_click[i] != double_click)
                return;
        }
    }

    game_action* other_action = ActionNameToPtr(command);
    if (IsGroupNotConflicted(m_action->key_group, other_action->key_group))
        return;

    SetText("---");
    m_keyboard = NULL;
}
