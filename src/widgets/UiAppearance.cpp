#include "UiAppearance.hpp"

#include <cassert>

namespace Slic3r {
namespace GUI {

namespace {
UiAppearance *s_impl = nullptr;
}

UiAppearance &ui()
{
    assert(s_impl != nullptr && "UiAppearance not installed - call set_ui_appearance() before creating widgets");
    return *s_impl;
}

void set_ui_appearance(UiAppearance *impl)
{
    s_impl = impl;
}

}} // namespace Slic3r::GUI
