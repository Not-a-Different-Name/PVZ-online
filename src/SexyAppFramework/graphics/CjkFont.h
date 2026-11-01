#pragma once
#include "Font.h"

namespace Sexy
{
// Trusted, bundled font only; never parse imported user font files with stb_truetype.
_Font* GetCjkFont(int pixelHeight);
_Font* GetCjkUiFont(int emHeight);
void ClearCjkFonts();
}
