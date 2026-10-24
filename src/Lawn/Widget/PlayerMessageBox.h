#pragma once
#include "LawnApp.h"
#include "Lawn/ModText.h"
#include "Lawn/Widget/OnlineStartDialog.h"
#include "widget/WidgetManager.h"

// Single-button custom information notices only. Keep their existing modal ID,
// wait/close behavior and focus restoration while using the UTF-8 font path.
namespace PlayerMessageBox {
inline OnlineStartDialog* Create(LawnApp* app, int id, const char* title, const char* body) {
    return new OnlineStartDialog(app, title, body, ModText::Tr("知道了", "OK"),
        nullptr, OnlineStartDialog::NOTIFY_NONE, false, id);
}
inline int Show(LawnApp* app, int id, const char* title, const char* body) {
    auto* previousFocus = app->mWidgetManager->mFocusWidget;
    auto* dialog = Create(app, id, title, body);
    app->CenterDialog(dialog, dialog->mWidth, dialog->mHeight);
    app->AddDialog(id, dialog);
    app->mWidgetManager->SetFocus(dialog);
    const int result = dialog->WaitForResult(true);
    app->mWidgetManager->SetFocus(previousFocus);
    return result;
}
}
