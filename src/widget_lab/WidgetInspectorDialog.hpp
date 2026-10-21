#ifndef slic3r_GUI_WidgetInspectorDialog_hpp_
#define slic3r_GUI_WidgetInspectorDialog_hpp_

#include "widgets/DpiAware.hpp"

#include <wx/dataview.h>
#include <wx/splitter.h>

#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

// NB: StaticBox and Label are global classes (Widgets/StaticBox.hpp and
// Widgets/Label.hpp have no namespace); SegmentedToggle lives in Slic3r::GUI.
class StaticBox;
class Label;
class SpinInput;
class wxSlider;

namespace Slic3r {
namespace GUI {

class SegmentedToggle;

// Designer-facing widget inspector: pick a control from the categorized tree
// on the left, tweak its properties on the right, see the result on a live
// instance. Covers both the project's custom widgets (GUI/Widgets/) and plain
// wxWidgets native controls. Temporary internal tool for design review: not
// localized beyond _L() wrapping, no persistence of any kind.
// Reached from Help menu -> "UI Inspector". Modeless so it can be compared
// against the live application UI.
class WidgetInspectorDialog : public DPIDialog
{
public:
    WidgetInspectorDialog(wxWindow *parent);
    ~WidgetInspectorDialog() override;

    // widget_lab mode: closing the dialog ends the process (main-app mode hides it).
    void set_standalone(bool s) { m_standalone = s; }

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    enum class EditorKind { Text, Int, Bool, Enum, StringList };

    // One editable property of one widget type. All values travel as strings;
    // Int parses, Bool uses "1"/"0", Enum indexes enum_values, StringList
    // splits on newlines.
    struct PropDef {
        const char *name;
        EditorKind  kind;
        // Value list for kind == Enum (must outlive the registry).
        const std::vector<wxString> *enum_values{nullptr};
        // Optional designer-facing note shown under the editor row.
        const char *note{nullptr};
        // Reads the current value from a fresh instance (editor init).
        std::function<wxString(wxWindow *)> get;
        // Applies an edited value to the instance.
        std::function<void(wxWindow *, const wxString &)> set;
        // Editing this property re-creates the preview instance: needed for
        // widgets whose item lists only exist at construction time. The
        // factory reads the construction-time values from the value store.
        bool recreate{false};
        // SliderSpin editor range for Int properties (semantic per property).
        int slider_min{0};
        int slider_max{100};
        // After this property applies, refresh the slider/spin range of the
        // named sibling property (e.g. ProgressBar max -> value: 0..max).
        const char *refresh_range_of{nullptr};
    };

    // One selectable leaf of the tree.
    struct WidgetEntry {
        const char *name;
        int         category; // index into CATEGORY_NAMES
        std::function<wxWindow *(wxWindow *parent, const std::map<wxString, wxString> &store)> factory;
        // Static reference text for the info panel (transcribed from the
        // headers; nullptr = unknown).
        const char *inherits;
        const char *header;
        std::vector<PropDef> props;
    };

    static const char *const CATEGORY_NAMES[5];
    static const std::vector<WidgetEntry> &registry();
    // Fills the class-specific property schemas of every registry entry.
    static void fill_property_schemas(std::vector<WidgetEntry> &reg);

    // One pooled property editor row. The editor's event handlers are bound
    // once (capturing the stable Row) and call `apply`, which rebind_rows
    // reassigns for every selection - no window is created on a switch once
    // the pool covers the entry.
    struct Row {
        EditorKind  kind{EditorKind::Text};
        Label      *label{nullptr};  // property name (left grid column)
        wxWindow   *editor{nullptr}; // editor control (right grid column)
        Label      *note{nullptr};   // optional note line under the editor
        std::function<void(const wxString &)> apply;
        // Int rows: the two-way-sync pair and its feedback guard.
        wxSlider   *slider{nullptr};
        SpinInput  *spin{nullptr};
        std::shared_ptr<bool> guard;
    };

    void build_top_bar(wxSizer *sizer);
    void rebuild_tree();
    void on_widget_selected(wxDataViewEvent &event);
    void restore_selection();

    // One-time construction of the resident right pane (plate + property
    // grid + info panel). Called from the ctor and again on theme toggle
    // (the only full rebuild - native editors resolve colors at creation).
    void build_right_pane();
    // The selection hot path: fresh preview instance + pooled row rebind +
    // info update. No editor window creation once the pool covers the entry.
    void update_for_selection();
    // Destroys and recreates only the preview instance inside the plate.
    void rebuild_preview();
    Row *create_row(EditorKind kind);
    void rebind_rows(const WidgetEntry &entry);
    void set_row_value(Row &row, const wxString &value);
    // Re-reads visible rows' values (store, else fresh instance), skipping
    // skip_prop_index - the row being typed in keeps focus and content.
    void refresh_row_values(const WidgetEntry &entry, size_t skip_prop_index);
    void apply_property(int entry_index, size_t prop_index, const wxString &value);
    // Appends the properties every widget shares (size, enabled, tooltip).
    static void add_common_props(std::vector<PropDef> &props);

    void on_theme_toggle(int index);
    void on_close(wxCloseEvent &event);

    wxSplitterWindow   *m_splitter{nullptr};
    wxDataViewTreeCtrl *m_tree{nullptr};
    wxWindow           *m_right{nullptr};  // resident right pane root (wxScrolled)
    StaticBox          *m_plate{nullptr};  // preview base plate (resident)
    wxWindow           *m_instance{nullptr};
    wxSizer            *m_grid{nullptr};   // property row grid; rows detached/attached on rebind
    wxTextCtrl         *m_info{nullptr};   // resident info panel
    wxWindow           *m_hint{nullptr};   // "select a widget" hint (no selection)
    // Editor row pool - grows on demand to the largest entry's needs.
    std::vector<std::unique_ptr<Row>> m_rows;
    // Rows bound to the current selection, with their prop indices.
    std::vector<std::pair<Row *, size_t>> m_used;
    // Live Int editor rows by property name, for range dependencies
    // (ProgressBar max -> value SetRange). Rebuilt on every rebind.
    std::map<wxString, std::pair<wxSlider *, SpinInput *>> m_int_editors;
    SegmentedToggle    *m_theme_toggle{nullptr};
    bool                m_initial_dark{false};
    bool                m_standalone{false};
    int                 m_current{-1};     // registry index; -1 = no selection
    // Edits of the currently selected widget. Session-scoped: cleared when a
    // different leaf is selected (spec: No persistence). Survives rebuilds of
    // the same selection (theme toggle, recreate-flagged property edits).
    std::map<wxString, wxString> m_prop_values;
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_GUI_WidgetInspectorDialog_hpp_
