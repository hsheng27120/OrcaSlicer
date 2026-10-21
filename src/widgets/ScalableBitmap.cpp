#include "ScalableBitmap.hpp"

#include <widgets/BitmapCache.hpp>
#include <widgets/UiAppearance.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <libslic3r/Exception.hpp>

int em_unit(wxWindow* win)
{
    if (win)
    {
        const int em = Slic3r::GUI::ui().em_unit_of(win);
        if (em > 0)
            return em;
    }

    return Slic3r::GUI::ui().em_unit();
}

// win is used to get a correct em_unit value
// It's important for bitmaps of dialogs.
// if win == nullptr, em_unit value of MainFrame will be used
wxBitmap create_scaled_bitmap(  const std::string& bmp_name_in,
                                wxWindow *win/* = nullptr*/,
                                const int px_cnt/* = 16*/,
                                const bool grayscale/* = false*/,
                                const std::string& new_color/* = std::string()*/, // color witch will used instead of orange
                                const bool menu_bitmap/* = false*/,
                                const bool resize/* = false*/,
                                const bool bitmap2/* = false*/,
                                const std::vector<std::string>& array_new_color/* = vector<std::string>*/)//used for semi transparent material)
{
    static Slic3r::GUI::BitmapCache cache;
    if (win == nullptr)
        win = Slic3r::GUI::ui().mainframe();
    if (bitmap2) {
        return create_scaled_bitmap2(bmp_name_in, cache, win, px_cnt, grayscale, resize, array_new_color);
    }
    unsigned int width = 0;
    unsigned int height = (unsigned int) (win->FromDIP(px_cnt) + 0.5f);

    std::string bmp_name = bmp_name_in;
    boost::replace_last(bmp_name, ".png", "");

    bool dark_mode =
#ifdef _WIN32
    menu_bitmap ? Slic3r::GUI::ui().menu_dark_mode() :
#endif
    Slic3r::GUI::ui().dark_mode();

    // Try loading an SVG first, then PNG if SVG was not found:
    wxBitmap *bmp = cache.load_svg(bmp_name, width, height, grayscale, dark_mode, new_color, resize ? em_unit(win) * 0.1f : 0.f);
    if (bmp == nullptr) {
        bmp = cache.load_png(bmp_name, width, height, grayscale, resize ? win->FromDIP(10) * 0.1f : 0.f);
    }

    if (bmp == nullptr) {
        // Neither SVG nor PNG has been found, raise error
        throw Slic3r::RuntimeError("Could not load bitmap: " + bmp_name);
    }

    return *bmp;
}

wxBitmap create_scaled_bitmap2(const std::string& bmp_name_in, Slic3r::GUI::BitmapCache& cache, wxWindow* win/* = nullptr*/ ,
    const int px_cnt/* = 16*/, const bool grayscale/* = false*/ , const bool resize/* = false*/ ,
    const std::vector<std::string>& array_new_color/* = vector<std::string>()*/) // color witch will used instead of orange
{
    unsigned int width = 0;
    unsigned int height = (unsigned int)(win->FromDIP(px_cnt) + 0.5f);

    std::string bmp_name = bmp_name_in;
    boost::replace_last(bmp_name, ".png", "");

    wxBitmap* bmp = cache.load_svg2(bmp_name, width, height, grayscale, false, array_new_color, resize ? em_unit(win) * 0.1f : 0.f);
    if (bmp == nullptr) {
        // No SVG found
        throw Slic3r::RuntimeError("Could not load bitmap: " + bmp_name);
    }
    return *bmp;
}

// ----------------------------------------------------------------------------
// ScalableBitmap
// ----------------------------------------------------------------------------
ScalableBitmap::ScalableBitmap( wxWindow *parent,
                                const std::string& icon_name/* = ""*/,
                                const int px_cnt/* = 16*/,
                                const bool grayscale/* = false*/,
                                const bool resize/* = false*/,
                                const bool bitmap2/* = false*/,
                                const std::vector<std::string>& new_color/* = vector<std::string>*/) :
    m_parent(parent), m_icon_name(icon_name),
    m_px_cnt(px_cnt), m_grayscale(grayscale), m_resize(resize) // BBS: support resize by fill border
{
    m_bmp = create_scaled_bitmap(icon_name, parent, px_cnt, m_grayscale, std::string(), false, resize, bitmap2, new_color);
    if (px_cnt == 0) {
        m_px_cnt = m_bmp.GetHeight(); // scale
        unsigned int height = (unsigned int) (parent->FromDIP(m_px_cnt) + 0.5f);
        if (height != GetBmpHeight())
            msw_rescale();
    }
}

wxSize ScalableBitmap::GetBmpSize() const
{
#ifdef __APPLE__
    return m_bmp.GetScaledSize();
#else
    return m_bmp.GetSize();
#endif
}

int ScalableBitmap::GetBmpWidth() const
{
#ifdef __APPLE__
    return m_bmp.GetScaledWidth();
#else
    return m_bmp.GetWidth();
#endif
}

int ScalableBitmap::GetBmpHeight() const
{
#ifdef __APPLE__
    return m_bmp.GetScaledHeight();
#else
    return m_bmp.GetHeight();
#endif
}


ScalableBitmap::ScalableBitmap(wxWindow *parent, const wxBitmap &bmp)
    : m_parent(parent), m_bmp(bmp), m_px_cnt(bmp.IsOk() ? bmp.GetHeight() : 16)
{
}

void ScalableBitmap::msw_rescale()
{
    if (m_icon_name.empty()) {
        // Bitmap-constructed instance: there is no resource name to reload,
        // and create_scaled_bitmap("") throws. Keep the current bitmap.
        return;
    }
    // BBS: support resize by fill border
    m_bmp = create_scaled_bitmap(m_icon_name, m_parent, m_px_cnt, m_grayscale, std::string(), false, m_resize);
}
