// widget_lab: standalone designer tool hosting the widget inspector as the
// main window. Depends only on libslic3r_widgets - no slicing backend, no
// user configuration.
//
// Launch: widget_lab.exe [--data-dir <path>]
//   <path> is resolved for fonts (resources/fonts) and widget icons
//   (resources/...); defaults to the executable's directory. For a dev
//   build pass the OrcaSlicer source root (it contains resources/).

#include "WidgetInspectorDialog.hpp"

#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/StateColor.hpp"
#include "widgets/UiAppearance.hpp"

#include <libslic3r/Utils.hpp>

#include <boost/filesystem/path.hpp>

#include <wx/app.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/settings.h>

namespace {
using namespace Slic3r::GUI;

// Appearance implementation for the lab: answers from the system and the
// inspector dialog itself. Project widgets self-style through StateColor,
// so dark restyle is a no-op here.
class LabAppearance final : public UiAppearance
{
public:
    void set_main_window(wxWindow *w) { m_main = w; }

    int em_unit() const override { return m_main ? m_main->FromDIP(10) : 10; }
    int em_unit_of(wxWindow *win) const override
    {
        // The lab has a single top-level window: the inspector dialog
        // (a DPIDialog) answers for itself and its children.
        for (wxWindow *w = win; w; w = w->GetParent())
            if (w == m_main)
                return m_dlg_em;
        return 0;
    }
    bool dark_mode() const override { return wxSystemSettings::GetAppearance().IsDark(); }
    bool menu_dark_mode() const override { return dark_mode(); }
    void update_dark_ui(wxWindow *, bool) const override {}
    const wxFont &bold_font() const override { return Label::Body_14; }
    const wxFont &normal_font() const override { return Label::Body_12; }
    wxWindow *mainframe() const override { return m_main; }

    void set_dlg_em(int em) { m_dlg_em = em; }

private:
    wxWindow *m_main  = nullptr;
    int       m_dlg_em = 10;
};

} // namespace

class LabApp : public wxApp
{
public:
    bool OnInit() override
    {
        // Resources live in <exe dir>/resources (installed layout) or in the
        // OrcaSlicer source root for a dev build - point --data-dir at the
        // directory CONTAINING resources/.
        boost::filesystem::path res_dir = boost::filesystem::path(
            wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath().ToUTF8().data());
        for (int i = 1; i < argc; ++i) {
            if (argv[i] == "--data-dir" && i + 1 < argc) {
                res_dir = boost::filesystem::path(argv[++i].ToUTF8().data());
                break;
            }
        }
        res_dir /= "resources";

        Slic3r::set_resources_dir(res_dir.string());
        Slic3r::set_var_dir((res_dir / "images").string());
        Slic3r::set_local_dir((res_dir / "i18n").string());
        Slic3r::set_data_dir(res_dir.string());
        Label::initSysFont();
        wxInitAllImageHandlers();

        const bool dark = wxSystemSettings::GetAppearance().IsDark();
        StateColor::SetDarkMode(dark);
        m_appearance.set_main_window(nullptr); // set below once the dialog exists
        Slic3r::GUI::set_ui_appearance(&m_appearance);

        m_dlg = new Slic3r::GUI::WidgetInspectorDialog(nullptr);
        m_dlg->set_standalone(true);
        m_appearance.set_main_window(m_dlg);
        m_appearance.set_dlg_em(m_dlg->em_unit());
        m_dlg->Show();
        return true;
    }

private:
    LabAppearance m_appearance;
    Slic3r::GUI::WidgetInspectorDialog *m_dlg = nullptr;
};

wxIMPLEMENT_APP(LabApp);
