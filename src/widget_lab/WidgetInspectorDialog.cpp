#include "WidgetInspectorDialog.hpp"

#include "widgets/UiAppearance.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/CheckBox.hpp"
#include "slic3r/GUI/Widgets/ComboBox.hpp"
#include "slic3r/GUI/Widgets/ErrorMsgStaticText.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/ProgressBar.hpp"
#include "slic3r/GUI/Widgets/RadioBox.hpp"
#include "slic3r/GUI/Widgets/RadioGroup.hpp"
#include "slic3r/GUI/Widgets/ScrolledWindow.hpp"
#include "slic3r/GUI/Widgets/SegmentedToggle.hpp"
#include "slic3r/GUI/Widgets/SideButton.hpp"
#include "slic3r/GUI/Widgets/StaticBox.hpp"
#include "slic3r/GUI/Widgets/SwitchButton.hpp"
#include "slic3r/GUI/Widgets/TextInput.hpp"
#include "slic3r/GUI/Widgets/SpinInput.hpp"

#include <wx/app.h>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/clipbrd.h>
#include <wx/combobox.h>
#include <wx/dataobj.h>
#include <wx/filefn.h>
#include <wx/gauge.h>
#include <wx/image.h>
#include <wx/listbox.h>
#include <wx/radiobut.h>
#include <wx/scrolwin.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/statbmp.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tooltip.h>
#include <wx/tglbtn.h>
#include <wx/utils.h>

#include <algorithm>
#include <cstring>
#include <memory>

namespace Slic3r {
namespace GUI {

namespace {

// Shared enum value lists (must outlive the registry vector, hence static).
const std::vector<wxString> ENUM_BUTTON_STYLE = {"Regular", "Confirm", "Alert"};
const std::vector<wxString> ENUM_BUTTON_TYPE  = {"Compact", "Window", "Choice", "Parameter", "Expanded"};
const std::vector<wxString> ENUM_SEG_STYLE    = {"Boxed", "Plain"};
const std::vector<wxString> ENUM_DIRECTION    = {"Horizontal", "Vertical"};
const std::vector<wxString> ENUM_LABEL_FONT   = {"Body_8", "Body_9", "Body_10", "Body_11", "Body_12", "Body_13",
    "Body_14", "Body_15", "Body_16", "Head_10", "Head_11", "Head_12", "Head_13", "Head_14", "Head_15", "Head_16",
    "Head_18", "Head_20", "Head_24", "Head_32", "Head_48"};

const wxString BUTTON_STYLES[]  = {"Regular", "Confirm", "Alert"};
const wxString BUTTON_TYPES[]   = {"Compact", "Window", "Choice", "Parameter", "Expanded"};

// Designer-facing documentation migrated from the removed gallery dialog
// (keyed by registry name; widgets without an entry just omit those lines
// in the info panel).
struct WidgetDoc {
    const char *desc;    // 控件说明
    const char *usage;   // 使用场景
    const char *caveats; // 使用注意事项
};

static const WidgetDoc *doc_for(const wxString &name)
{
    static const std::map<wxString, WidgetDoc> DOCS = {
        {"Button", {"自绘圆角按钮，支持 Regular/Confirm/Alert 三种语义样式与 Compact/Window/Choice/Parameter/Expanded 五种尺寸类型。",
                    "对话框确认/取消、参数行内操作、整宽主操作按钮。",
                    "必须调用 SetStyle 指定样式与类型；文案用 _L() 包裹。SetIcon 支持资源名或本地图片路径(png/jpg/bmp/gif)，无效路径保留原图标。"}},
        {"CheckBox", {"自绘位图复选框（wxBitmapToggleButton 派生）。",
                      "布尔参数开关、列表多选。",
                      "勾选态图标由资源按 DPI 缩放，换算后需 Rescale()。"}},
        {"SwitchButton", {"自绘滑动开关。",
                          "设置页即时生效的布尔开关。",
                          "是瞬间切换（toggle），点击即翻转，不是\"编辑后提交\"式控件。"}},
        {"SegmentedToggle", {"分段式互斥选项切换。",
                             "2-4 个互斥选项的平铺切换（视图模式、排序方式等）。",
                             "选项数量/文案变化需要重建控件；选中态颜色由主题驱动。"}},
        {"TextInput", {"带标签/图标的前缀式文本输入框。",
                       "表单文本录入（名称、搜索等）。",
                       "默认宽度随内容伸展，建议用 size 参数约束宽度。"}},
        {"SpinInput", {"带步进按钮的数值输入框。",
                       "数值参数录入（速度、比例等）。",
                       "超出 min/max 自动钳制；标签与输入框并排。"}},
        {"wxTextCtrl", {"wxWidgets 原生文本控件。",
                        "多行文本、不可主题化的场景。",
                        "原生外观不随应用主题，混排时注意背景色差异。"}},
        {"ComboBox", {"自绘下拉单选框（内嵌 DropDown 弹层）。",
                      "选项 5-20 个的下拉单选（材料、机型等）。",
                      "选项用 Append 添加；弹层是独立 PopupWindow。"}},
        {"RadioGroup", {"平铺的单选组。",
                        "少量互斥选项（喷嘴口径、档位等）。",
                        "labels 向量定义选项；GetSelection() 取当前项。"}},
        {"RadioBox", {"纯图形化的开/关切换（单选样式的位图按钮）。",
                      "只表达 on/off 的图形开关。",
                      "不承载多个选项——需要多选一请用 RadioGroup 或 ComboBox。"}},
        {"ProgressBar", {"自绘圆角进度条。",
                         "切片、打印、固件传输等进度展示。",
                         "SetValue 范围 0..max；Disable(text) 显示中断态。"}},
        {"ErrorMsgStaticText", {"错误提示文本（自动换行）。",
                                "表单校验失败、设备错误的提示文案。",
                                "paint 时按当前宽度自动换行；SetLabel 空文本会越界（历史缺陷，勿传空串）。"}},
        {"Label", {"自绘静态文本，支持 Head 系列预设字体。",
                   "标题、说明文字、数值标签。",
                   "Head_x 静态字体在 GUI_App::initSysFont() 之后才可用。"}},
        {"StaticBox", {"自绘底色容器底板（圆角+描边）。",
                       "卡片式布局的底板。",
                       "底色由 StateColor 状态驱动。"}},
    };
    auto it = DOCS.find(name);
    return it == DOCS.end() ? nullptr : &it->second;
}

// Client-data memos for properties whose widgets expose a setter but no
// getter. Attached to the live instance so they die with it.
struct ButtonShape : wxClientData {
    ButtonStyle style = ButtonStyle::Regular;
    ButtonType  type  = ButtonType::Window;
    wxString    icon; // last applied icon value (resource name or local path)
};
struct MemoBool : wxClientData {
    explicit MemoBool(bool v = false) : value(v) {}
    bool value;
};
struct MemoString : wxClientData {
    explicit MemoString(const wxString &v = {}) : value(v) {}
    wxString value;
};
struct MemoInt : wxClientData {
    explicit MemoInt(long v = 0) : value(v) {}
    long value;
};
struct MemoRange : wxClientData {
    MemoRange(long lo, long hi) : minv(lo), maxv(hi) {}
    long minv, maxv;
};

template <typename T>
static T *memo(wxWindow *w) { return static_cast<T *>(w->GetClientData()); }

static wxString b2s(bool v) { return v ? "1" : "0"; }
static bool s2b(const wxString &v) { return v == "1"; }
static long s2l(const wxString &v) { long out = 0; v.ToLong(&out); return out; }

// Index of a value inside an enum list (falls back to 0).
static long enum_index(const std::vector<wxString> &values, const wxString &v)
{
    for (size_t i = 0; i < values.size(); ++i)
        if (values[i] == v)
            return long(i);
    return 0;
}

// Splits a StringList value into non-empty trimmed lines.
static std::vector<wxString> split_lines(const wxString &v)
{
    std::vector<wxString> out;
    wxString              cur;
    for (size_t i = 0; i <= v.size(); ++i) {
        if (i == v.size() || v[i] == '\n') {
            cur.Trim(true).Trim(false);
            if (!cur.empty())
                out.push_back(cur);
            cur.Clear();
        } else if (v[i] != '\r') {
            cur += v[i];
        }
    }
    return out;
}

// ---------------------------------------------------------- local images
// SetIcon and the image properties accept either a resource name or a local
// image file path. Values are validated BEFORE they reach any loader:
// create_scaled_bitmap throws on unknown names and a failed wxImage decode
// must never surface as an uncaught exception inside an event handler.

static bool image_ext_supported(const wxString &path)
{
    // wxInitAllImageHandlers() registers the raster formats at app start.
    // Local SVG is deliberately NOT in this list: wxImage cannot decode SVG
    // (the project's SVG support is BitmapCache's resource lookup only), so
    // SVG stays resource-name-only.
    static const char *EXTS[] = {"png", "jpg", "jpeg", "bmp", "gif", "tif", "tiff"};
    wxString ext = path.AfterLast('.').Lower();
    for (const char *e : EXTS)
        if (ext == e)
            return true;
    return false;
}

enum class ImgSrc { Empty, File, Resource, Invalid };

// Classifies an icon/image editor value: Empty clears, File is a validated
// local path, Resource is a (path-less) resource name, Invalid is a
// path-shaped value that fails validation with *err explaining why.
static ImgSrc classify_image_value(const wxString &raw, wxString *err = nullptr)
{
    wxString p = raw;
    p.Trim(true).Trim(false);
    if (p.empty())
        return ImgSrc::Empty;
    bool path_like = p.Contains("\\") || p.Contains("/") || p.Contains(":");
    if (!path_like)
        return ImgSrc::Resource;
    if (!wxFileExists(p)) {
        if (err) *err = _L("文件不存在: ") + p;
        return ImgSrc::Invalid;
    }
    if (!image_ext_supported(p)) {
        if (err) *err = _L("不支持的图片格式(SVG 请用资源名): ") + p;
        return ImgSrc::Invalid;
    }
    return ImgSrc::File;
}

// Loads a local image and fits it into a max_box_dip square, aspect kept.
static wxBitmap load_image_fitted(const wxString &path, wxWindow *dip_ref, int max_box_dip)
{
    wxImage img;
    if (!img.LoadFile(path, wxBITMAP_TYPE_ANY) || !img.IsOk())
        return wxBitmap();
    wxSize box = dip_ref->FromDIP(wxSize(max_box_dip, max_box_dip));
    int w = img.GetWidth(), h = img.GetHeight();
    if (w <= 0 || h <= 0)
        return wxBitmap();
    double scale = std::min(box.x / double(w), box.y / double(h));
    if (scale < 1.0)
        img = img.Scale(std::max(1, int(w * scale + 0.5)), std::max(1, int(h * scale + 0.5)),
            wxIMAGE_QUALITY_HIGH);
    return wxBitmap(img);
}

// Surfaces a validation failure on the preview instance itself (hover to
// read); successful applies clear the tooltip again.
static void apply_hint(wxWindow *w, const wxString &err)
{
    w->SetToolTip(err);
    wxBell();
}

// Shared setter for the image property of wxStaticBitmap / wxBitmapButton.
static void apply_bitmap_image(wxWindow *w, const wxString &v, int box_dip, bool is_button)
{
    wxString err;
    ImgSrc src = classify_image_value(v, &err);
    if (src == ImgSrc::Invalid) {
        apply_hint(w, err);
        return;
    }
    if (src == ImgSrc::Empty)
        return; // these natives have no meaningful clear
    wxBitmap bmp;
    if (src == ImgSrc::File) {
        bmp = load_image_fitted(v, w, box_dip);
    } else {
        try {
            bmp = create_scaled_bitmap(v.ToStdString(), w, 24);
        } catch (...) {
            apply_hint(w, _L("资源不存在: ") + v);
            return;
        }
    }
    if (!bmp.IsOk()) {
        apply_hint(w, _L("图片解码失败: ") + v);
        return;
    }
    w->SetToolTip(nullptr);
    if (is_button)
        static_cast<wxBitmapButton *>(w)->SetBitmap(bmp);
    else
        static_cast<wxStaticBitmap *>(w)->SetBitmap(bmp);
}

static wxString width_get(wxWindow *w) { return wxString::Format("%d", w->GetSize().x); }
static void     width_set(wxWindow *w, const wxString &v)
{
    w->SetMinSize(wxSize(s2l(v), w->GetMinSize().y));
    w->SetSize(s2l(v), w->GetSize().y);
}
static wxString height_get(wxWindow *w) { return wxString::Format("%d", w->GetSize().y); }
static void     height_set(wxWindow *w, const wxString &v)
{
    w->SetMinSize(wxSize(w->GetMinSize().x, s2l(v)));
    w->SetSize(w->GetSize().x, s2l(v));
}

// Store lookup helper: construction-time values for recreate-flagged props.
static wxString stored(const std::map<wxString, wxString> &store, const char *name, const wxString &fallback = {})
{
    auto it = store.find(name);
    return it == store.end() ? fallback : it->second;
}

} // namespace

const char *const WidgetInspectorDialog::CATEGORY_NAMES[5] = {"Buttons", "Inputs", "Selectors", "Indicators",
    "Containers"};

// The registry: one entry per selectable leaf. Adding a widget = adding an
// entry here plus (if it has more than the common properties) a branch in
// fill_property_schemas below; tree, preview and property list are generic
// machinery.
const std::vector<WidgetInspectorDialog::WidgetEntry> &WidgetInspectorDialog::registry()
{
    static const std::vector<WidgetEntry> REGISTRY = [] {
        std::vector<WidgetEntry> r = {
        // ------------------------------------------------------ Buttons
        {"Button", 0,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                auto *w = new Button(p, "Confirm");
                w->SetStyle(ButtonStyle::Regular, ButtonType::Window);
                w->SetClientData(new ButtonShape());
                return static_cast<wxWindow *>(w);
            },
            "Button > StaticBox > wxWindow", "GUI/Widgets/Button.hpp", {}},
        {"SideButton", 0,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                auto *w = new SideButton(p, "Side");
                w->SetClientData(new MemoInt{0});
                return static_cast<wxWindow *>(w);
            },
            "SideButton > wxWindow", "GUI/Widgets/SideButton.hpp", {}},
        {"RadioBox", 0,
            [](wxWindow *p, const std::map<wxString, wxString> &) { return static_cast<wxWindow *>(new RadioBox(p)); },
            "RadioBox > wxBitmapToggleButton > wxToggleButton > wxControl", "GUI/Widgets/RadioBox.hpp", {}},
        {"SwitchButton", 0,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                auto *w = new SwitchButton(p);
                w->SetClientData(new MemoString{"On"});
                return static_cast<wxWindow *>(w);
            },
            "SwitchButton > wxBitmapToggleButton > wxToggleButton > wxControl", "GUI/Widgets/SwitchButton.hpp", {}},
        {"wxButton", 0,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(new wxButton(p, wxID_ANY, "wxButton"));
            },
            "wxButton > wxAnyButton > wxControl > wxWindow", "<wx/button.h>", {}},
        {"wxToggleButton", 0,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(new wxToggleButton(p, wxID_ANY, "wxToggleButton"));
            },
            "wxToggleButton > wxToggleButtonBase > wxControl > wxWindow", "<wx/tglbtn.h>", {}},
        {"wxBitmapButton", 0,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                // The bitmap must outlive the widget; heap-owned like the
                // gallery's specimens (tiny, one per selection - acceptable
                // for this temp tool).
                auto *bmp = new ScalableBitmap(p, "monitor_lamp_off", 24);
                return static_cast<wxWindow *>(new wxBitmapButton(p, wxID_ANY, bmp->bmp()));
            },
            "wxBitmapButton > wxButton > wxAnyButton > wxControl > wxWindow", "<wx/bmpbuttn.h>", {}},

        // ------------------------------------------------------- Inputs
        {"TextInput", 1,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(
                    new TextInput(p, "Snapmaker Orca", "Label", "", wxDefaultPosition, wxSize(220, -1)));
            },
            "TextInput > wxNavigationEnabled<StaticBox> > StaticBox > wxWindow", "GUI/Widgets/TextInput.hpp", {}},
        {"SpinInput", 1,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                auto *w = new SpinInput(p, "50", "Speed", wxDefaultPosition, wxSize(220, -1), 0, 0, 100, 50, 1);
                w->SetClientData(new MemoRange{0, 100});
                return static_cast<wxWindow *>(w);
            },
            "SpinInput > wxNavigationEnabled<StaticBox> > StaticBox > wxWindow", "GUI/Widgets/SpinInput.hpp", {}},
        {"wxTextCtrl", 1,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(
                    new wxTextCtrl(p, wxID_ANY, "The quick brown fox", wxDefaultPosition, wxSize(220, -1)));
            },
            "wxTextCtrl > wxTextCtrlBase > wxControl > wxWindow", "<wx/textctrl.h>", {}},
        {"wxSpinCtrl", 1,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(
                    new wxSpinCtrl(p, wxID_ANY, "50", wxDefaultPosition, wxSize(140, -1), 0, 0, 100, 50));
            },
            "wxSpinCtrl > wxSpinCtrlBase > wxWindow", "<wx/spinctrl.h>", {}},
        {"wxSlider", 1,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(
                    new wxSlider(p, wxID_ANY, 30, 0, 100, wxDefaultPosition, wxSize(220, -1), wxSL_HORIZONTAL));
            },
            "wxSlider > wxSliderBase > wxWindow", "<wx/slider.h>", {}},

        // ---------------------------------------------------- Selectors
        {"CheckBox", 2,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(new CheckBox(p));
            },
            "CheckBox > wxBitmapToggleButton > wxToggleButton > wxControl", "GUI/Widgets/CheckBox.hpp", {}},
        {"ComboBox", 2,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                wxString choices[3] = {"PLA", "PETG", "ABS"};
                auto *   w          = new ComboBox(p, wxID_ANY, "PLA", wxDefaultPosition, wxSize(220, -1), 3, choices);
                w->SetMinSize(wxSize(220, -1)); // shrinks to ~25px without it
                return static_cast<wxWindow *>(w);
            },
            "ComboBox > TextInput > StaticBox > wxWindow", "GUI/Widgets/ComboBox.hpp", {}},
        {"RadioGroup", 2,
            [](wxWindow *p, const std::map<wxString, wxString> &store) {
                // Construction-time-only values come from the session store.
                std::vector<wxString> labels = split_lines(stored(store, "items", "0.4mm\n0.6mm\n0.8mm"));
                if (labels.empty())
                    labels = {"0.4mm", "0.6mm", "0.8mm"};
                long dir = enum_index(ENUM_DIRECTION, stored(store, "direction", "Horizontal"));
                return static_cast<wxWindow *>(
                    new RadioGroup(p, labels, dir == 1 ? wxVERTICAL : wxHORIZONTAL));
            },
            "RadioGroup > wxPanel > wxWindow", "GUI/Widgets/RadioGroup.hpp", {}},
        {"SegmentedToggle", 2,
            [](wxWindow *p, const std::map<wxString, wxString> &store) {
                std::vector<wxString> options = split_lines(stored(store, "options", "Option A\nOption B\nOption C"));
                if (options.empty())
                    options = {"Option A", "Option B", "Option C"};
                long style = enum_index(ENUM_SEG_STYLE, stored(store, "style", "Boxed"));
                return static_cast<wxWindow *>(new SegmentedToggle(p, options, 0,
                    style == 1 ? SegmentedToggle::Style::Plain : SegmentedToggle::Style::Boxed));
            },
            "SegmentedToggle > wxPanel > wxWindow", "GUI/Widgets/SegmentedToggle.hpp", {}},
        {"wxCheckBox", 2,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(new wxCheckBox(p, wxID_ANY, "wxCheckBox"));
            },
            "wxCheckBox > wxCheckBoxBase > wxControl > wxWindow", "<wx/checkbox.h>", {}},
        {"wxRadioButton", 2,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(new wxRadioButton(p, wxID_ANY, "wxRadioButton"));
            },
            "wxRadioButton > wxControl > wxWindow", "<wx/radiobut.h>", {}},
        {"wxComboBox", 2,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                wxString choices[3] = {"PLA", "PETG", "ABS"};
                return static_cast<wxWindow *>(
                    new wxComboBox(p, wxID_ANY, "PLA", wxDefaultPosition, wxSize(180, -1), 3, choices));
            },
            "wxComboBox > wxComboBoxBase > wxControl > wxWindow", "<wx/combobox.h>", {}},
        {"wxChoice", 2,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                wxString choices[3] = {"PLA", "PETG", "ABS"};
                auto *   w          = new wxChoice(p, wxID_ANY, wxDefaultPosition, wxSize(180, -1), 3, choices);
                w->SetSelection(0);
                return static_cast<wxWindow *>(w);
            },
            "wxChoice > wxControl > wxWindow", "<wx/choice.h>", {}},
        {"wxListBox", 2,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                auto *w = new wxListBox(p, wxID_ANY, wxDefaultPosition, wxSize(180, 90), 0, nullptr);
                w->Append("PLA");
                w->Append("PETG");
                w->Append("ABS");
                w->SetSelection(0);
                return static_cast<wxWindow *>(w);
            },
            "wxListBox > wxListBoxBase > wxControl > wxWindow", "<wx/listbox.h>", {}},

        // --------------------------------------------------- Indicators
        {"Label", 3,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                auto *w = new Label(p, "Label text");
                w->SetClientData(new MemoString{"Body_12"});
                return static_cast<wxWindow *>(w);
            },
            "Label > wxStaticText > wxControl > wxWindow", "GUI/Widgets/Label.hpp", {}},
        {"ProgressBar", 3,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                auto *w = new ProgressBar(p, wxID_ANY, 100, wxDefaultPosition, wxSize(200, 18));
                w->SetValue(30);
                return static_cast<wxWindow *>(w);
            },
            "ProgressBar > wxWindow", "GUI/Widgets/ProgressBar.hpp", {}},
        {"ErrorMsgStaticText", 3,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                auto *w = new ErrorMsgStaticText(p);
                // SetLabel("") reads past the end (historical defect, see
                // gallery doc notes) - never seed it with an empty text.
                w->SetLabel("Sample error message");
                return static_cast<wxWindow *>(w);
            },
            "ErrorMsgStaticText > wxPanel > wxWindow", "GUI/Widgets/ErrorMsgStaticText.hpp", {}},
        {"wxStaticText", 3,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(new wxStaticText(p, wxID_ANY, "wxStaticText"));
            },
            "wxStaticText > wxControl > wxWindow", "<wx/stattext.h>", {}},
        {"wxGauge", 3,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(new wxGauge(p, wxID_ANY, 100, wxDefaultPosition, wxSize(200, -1)));
            },
            "wxGauge > wxGaugeBase > wxControl > wxWindow", "<wx/gauge.h>", {}},
        {"wxStaticBitmap", 3,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                auto *bmp = new ScalableBitmap(p, "monitor_lamp_off", 24);
                return static_cast<wxWindow *>(new wxStaticBitmap(p, wxID_ANY, bmp->bmp()));
            },
            "wxStaticBitmap > wxControl > wxWindow", "<wx/statbmp.h>", {}},

        // --------------------------------------------------- Containers
        {"StaticBox", 4,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(new StaticBox(p, wxID_ANY, wxDefaultPosition, wxSize(160, 80), 0));
            },
            "StaticBox > wxWindow", "GUI/Widgets/StaticBox.hpp", {}},
        {"wxPanel", 4,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(new wxPanel(p, wxID_ANY, wxDefaultPosition, wxSize(160, 60)));
            },
            "wxPanel > wxWindow", "<wx/panel.h>", {}},
        {"wxStaticBox", 4,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(new wxStaticBox(p, wxID_ANY, "wxStaticBox", wxDefaultPosition,
                    wxSize(160, 80)));
            },
            "wxStaticBox > wxStaticBoxBase > wxWindow", "<wx/statbox.h>", {}},
        {"wxStaticLine", 4,
            [](wxWindow *p, const std::map<wxString, wxString> &) {
                return static_cast<wxWindow *>(
                    new wxStaticLine(p, wxID_ANY, wxDefaultPosition, wxSize(220, -1), wxLI_HORIZONTAL));
            },
            "wxStaticLine > wxControl > wxWindow", "<wx/statline.h>", {}},
        };
        fill_property_schemas(r);
        return r;
    }();
    return REGISTRY;
}

// Class-specific property schemas, keyed by entry name. Order matters for
// recreate-flagged props: a recreate re-applies at factory time, then the
// remaining properties are re-applied from the store in schema order.
void WidgetInspectorDialog::fill_property_schemas(std::vector<WidgetEntry> &reg)
{
    for (auto &e : reg) {
        std::vector<PropDef> &P = e.props;
        if (strcmp(e.name, "Button") == 0) {
            P.push_back({"text", EditorKind::Text, nullptr, nullptr,
                [](wxWindow *w) { return w->GetLabel(); },
                [](wxWindow *w, const wxString &v) { w->SetLabel(v); }});
            P.push_back({"style", EditorKind::Enum, &ENUM_BUTTON_STYLE,
                "ButtonStyle::Disabled is a style, not a state - it stacks with Enable(false)",
                [](wxWindow *w) { return BUTTON_STYLES[long(memo<ButtonShape>(w)->style)]; },
                [](wxWindow *w, const wxString &v) {
                    ButtonShape *s = memo<ButtonShape>(w);
                    s->style       = static_cast<ButtonStyle>(enum_index(ENUM_BUTTON_STYLE, v));
                    static_cast<Button *>(w)->SetStyle(s->style, s->type);
                }});
            P.push_back({"type", EditorKind::Enum, &ENUM_BUTTON_TYPE, nullptr,
                [](wxWindow *w) { return BUTTON_TYPES[long(memo<ButtonShape>(w)->type)]; },
                [](wxWindow *w, const wxString &v) {
                    ButtonShape *s = memo<ButtonShape>(w);
                    s->type        = static_cast<ButtonType>(enum_index(ENUM_BUTTON_TYPE, v));
                    static_cast<Button *>(w)->SetStyle(s->style, s->type);
                }});
            P.push_back({"icon", EditorKind::Text, nullptr,
                "resource name, or local image path (png/jpg/bmp/gif); empty = none",
                [](wxWindow *w) { return memo<ButtonShape>(w) ? memo<ButtonShape>(w)->icon : wxString(); },
                [](wxWindow *w, const wxString &v) {
                    wxString err;
                    switch (classify_image_value(v, &err)) {
                    case ImgSrc::Invalid:
                        apply_hint(w, err);
                        return;
                    case ImgSrc::Resource:
                        // Pre-flight the name: Button::SetIcon goes through
                        // create_scaled_bitmap, which throws on unknown names.
                        try {
                            create_scaled_bitmap(v.ToStdString(), w, 20);
                        } catch (...) {
                            apply_hint(w, _L("资源不存在: ") + v);
                            return;
                        }
                        break;
                    default:
                        break; // Empty and validated File are both safe for SetIcon
                    }
                    w->SetToolTip(nullptr);
                    if (memo<ButtonShape>(w))
                        memo<ButtonShape>(w)->icon = v;
                    static_cast<Button *>(w)->SetIcon(v);
                }});
            P.push_back({"value", EditorKind::Bool, nullptr, nullptr,
                [](wxWindow *w) { return b2s(static_cast<Button *>(w)->GetValue()); },
                [](wxWindow *w, const wxString &v) { static_cast<Button *>(w)->SetValue(s2b(v)); }});
        } else if (strcmp(e.name, "SideButton") == 0) {
            P.push_back({"text", EditorKind::Text, nullptr, nullptr,
                [](wxWindow *w) { return w->GetLabel(); },
                [](wxWindow *w, const wxString &v) { w->SetLabel(v); }});
            P.push_back({"corner_radius", EditorKind::Int, nullptr, nullptr,
                [](wxWindow *w) { return wxString::Format("%ld", memo<MemoInt>(w)->value); },
                [](wxWindow *w, const wxString &v) {
                    memo<MemoInt>(w)->value = s2l(v);
                    static_cast<SideButton *>(w)->SetCornerRadius(s2l(v));
                },
                false, 0, 24});
        } else if (strcmp(e.name, "SwitchButton") == 0) {
            P.push_back({"value", EditorKind::Bool, nullptr, nullptr,
                [](wxWindow *w) { return b2s(static_cast<SwitchButton *>(w)->GetValue()); },
                [](wxWindow *w, const wxString &v) { static_cast<SwitchButton *>(w)->SetValue(s2b(v)); }});
            P.push_back({"label_on", EditorKind::Text, nullptr, nullptr,
                [](wxWindow *w) { return memo<MemoString>(w)->value; },
                [](wxWindow *w, const wxString &v) {
                    memo<MemoString>(w)->value = v;
                    static_cast<SwitchButton *>(w)->SetLabels(v, "Off");
                }});
            P.push_back({"label_off", EditorKind::Text, nullptr, nullptr,
                [](wxWindow *) { return wxString("Off"); },
                [](wxWindow *w, const wxString &v) {
                    static_cast<SwitchButton *>(w)->SetLabels(memo<MemoString>(w)->value, v);
                }});
        } else if (strcmp(e.name, "CheckBox") == 0) {
            P.push_back({"value", EditorKind::Bool, nullptr, nullptr,
                [](wxWindow *w) { return b2s(static_cast<CheckBox *>(w)->GetValue()); },
                [](wxWindow *w, const wxString &v) { static_cast<CheckBox *>(w)->SetValue(s2b(v)); }});
            P.push_back({"half_checked", EditorKind::Bool, nullptr, nullptr,
                [](wxWindow *w) { return b2s(memo<MemoBool>(w) ? memo<MemoBool>(w)->value : false); },
                [](wxWindow *w, const wxString &v) {
                    if (!memo<MemoBool>(w))
                        w->SetClientData(new MemoBool{false});
                    memo<MemoBool>(w)->value = s2b(v);
                    static_cast<CheckBox *>(w)->SetHalfChecked(s2b(v));
                }});
        } else if (strcmp(e.name, "RadioBox") == 0) {
            P.push_back({"value", EditorKind::Bool, nullptr, nullptr,
                [](wxWindow *w) { return b2s(static_cast<RadioBox *>(w)->GetValue()); },
                [](wxWindow *w, const wxString &v) { static_cast<RadioBox *>(w)->SetValue(s2b(v)); }});
        } else if (strcmp(e.name, "TextInput") == 0) {
            P.push_back({"text", EditorKind::Text, nullptr, nullptr,
                [](wxWindow *w) { return static_cast<TextInput *>(w)->GetTextCtrl()->GetValue(); },
                [](wxWindow *w, const wxString &v) { static_cast<TextInput *>(w)->GetTextCtrl()->SetValue(v); }});
            P.push_back({"label", EditorKind::Text, nullptr, nullptr,
                [](wxWindow *w) { return w->GetLabel(); },
                [](wxWindow *w, const wxString &v) { w->SetLabel(v); }});
            P.push_back({"corner_radius", EditorKind::Int, nullptr, nullptr,
                [](wxWindow *) { return wxString("4"); },
                [](wxWindow *w, const wxString &v) { static_cast<TextInput *>(w)->SetCornerRadius(s2l(v)); },
                false, 0, 24});
        } else if (strcmp(e.name, "SpinInput") == 0) {
            P.push_back({"value", EditorKind::Int, nullptr, nullptr,
                [](wxWindow *w) { return wxString::Format("%d", static_cast<SpinInput *>(w)->GetValue()); },
                [](wxWindow *w, const wxString &v) { static_cast<SpinInput *>(w)->SetValue(int(s2l(v))); },
                false, 0, 300});
            P.push_back({"min", EditorKind::Int, nullptr, "range edits re-apply min then max",
                [](wxWindow *w) { return wxString::Format("%ld", memo<MemoRange>(w)->minv); },
                [](wxWindow *w, const wxString &v) {
                    MemoRange *r = memo<MemoRange>(w);
                    r->minv      = s2l(v);
                    static_cast<SpinInput *>(w)->SetRange(int(r->minv), int(r->maxv));
                },
                false, 0, 300});
            P.push_back({"max", EditorKind::Int, nullptr, nullptr,
                [](wxWindow *w) { return wxString::Format("%ld", memo<MemoRange>(w)->maxv); },
                [](wxWindow *w, const wxString &v) {
                    MemoRange *r = memo<MemoRange>(w);
                    r->maxv      = s2l(v);
                    static_cast<SpinInput *>(w)->SetRange(int(r->minv), int(r->maxv));
                },
                false, 0, 300});
            P.push_back({"step", EditorKind::Int, nullptr, nullptr,
                [](wxWindow *w) { return wxString::Format("%d", static_cast<SpinInput *>(w)->GetStep()); },
                [](wxWindow *w, const wxString &v) { static_cast<SpinInput *>(w)->SetStep(int(s2l(v))); },
                false, 1, 20});
            P.push_back({"label", EditorKind::Text, nullptr, nullptr,
                [](wxWindow *w) { return w->GetLabel(); },
                [](wxWindow *w, const wxString &v) { w->SetLabel(v); }});
        } else if (strcmp(e.name, "ComboBox") == 0) {
            P.push_back({"items", EditorKind::StringList, nullptr, "one item per line; Enter applies", nullptr,
                [](wxWindow *w, const wxString &v) {
                    ComboBox *cb = static_cast<ComboBox *>(w);
                    cb->Clear();
                    for (const wxString &item : split_lines(v))
                        cb->Append(item);
                    if (cb->GetCount() > 0)
                        cb->SetSelection(0);
                },
                true});
            P.push_back({"selection", EditorKind::Int, nullptr, nullptr,
                [](wxWindow *w) { return wxString::Format("%d", static_cast<ComboBox *>(w)->GetSelection()); },
                [](wxWindow *w, const wxString &v) { static_cast<ComboBox *>(w)->SetSelection(int(s2l(v))); },
                false, 0, 32});
            P.push_back({"value", EditorKind::Text, nullptr, nullptr,
                [](wxWindow *w) { return static_cast<ComboBox *>(w)->GetValue(); },
                [](wxWindow *w, const wxString &v) { static_cast<ComboBox *>(w)->SetValue(v); }});
        } else if (strcmp(e.name, "RadioGroup") == 0) {
            P.push_back({"items", EditorKind::StringList, nullptr, "one label per line; Enter applies",
                nullptr, nullptr, true});
            P.push_back({"direction", EditorKind::Enum, &ENUM_DIRECTION, nullptr, nullptr, nullptr, true});
            P.push_back({"selection", EditorKind::Int, nullptr, nullptr,
                [](wxWindow *w) { return wxString::Format("%d", static_cast<RadioGroup *>(w)->GetSelection()); },
                [](wxWindow *w, const wxString &v) { static_cast<RadioGroup *>(w)->SetSelection(int(s2l(v))); },
                false, 0, 32});
        } else if (strcmp(e.name, "SegmentedToggle") == 0) {
            P.push_back({"options", EditorKind::StringList, nullptr, "one option per line; Enter applies",
                nullptr, nullptr, true});
            P.push_back({"selected", EditorKind::Int, nullptr, nullptr,
                [](wxWindow *w) { return wxString::Format("%d", static_cast<SegmentedToggle *>(w)->getSelected()); },
                [](wxWindow *w, const wxString &v) { static_cast<SegmentedToggle *>(w)->setSelected(int(s2l(v))); },
                false, 0, 32});
            P.push_back({"style", EditorKind::Enum, &ENUM_SEG_STYLE, nullptr, nullptr, nullptr, true});
        } else if (strcmp(e.name, "Label") == 0) {
            P.push_back({"text", EditorKind::Text, nullptr, nullptr,
                [](wxWindow *w) { return w->GetLabel(); },
                [](wxWindow *w, const wxString &v) { w->SetLabel(v); }});
            P.push_back({"font", EditorKind::Enum, &ENUM_LABEL_FONT, "Label::Head_* / Body_* presets",
                [](wxWindow *w) { return memo<MemoString>(w) ? memo<MemoString>(w)->value : wxString("Body_12"); },
                [](wxWindow *w, const wxString &v) {
                    static const std::map<wxString, wxFont> FONTS = {
                        {"Body_8", Label::Body_8},   {"Body_9", Label::Body_9},   {"Body_10", Label::Body_10},
                        {"Body_11", Label::Body_11}, {"Body_12", Label::Body_12}, {"Body_13", Label::Body_13},
                        {"Body_14", Label::Body_14}, {"Body_15", Label::Body_15}, {"Body_16", Label::Body_16},
                        {"Head_10", Label::Head_10}, {"Head_11", Label::Head_11}, {"Head_12", Label::Head_12},
                        {"Head_13", Label::Head_13}, {"Head_14", Label::Head_14}, {"Head_15", Label::Head_15},
                        {"Head_16", Label::Head_16}, {"Head_18", Label::Head_18}, {"Head_20", Label::Head_20},
                        {"Head_24", Label::Head_24}, {"Head_32", Label::Head_32}, {"Head_48", Label::Head_48}};
                    auto it = FONTS.find(v);
                    if (it != FONTS.end())
                        static_cast<Label *>(w)->SetFont(it->second);
                    if (!memo<MemoString>(w))
                        w->SetClientData(new MemoString{});
                    memo<MemoString>(w)->value = v;
                }});
            P.push_back({"auto_wrap", EditorKind::Bool, nullptr, nullptr,
                [](wxWindow *) { return wxString("0"); },
                [](wxWindow *w, const wxString &v) {
                    long style = w->GetWindowStyleFlag();
                    w->SetWindowStyleFlag(s2b(v) ? (style | LB_AUTO_WRAP) : (style & ~LB_AUTO_WRAP));
                }});
        } else if (strcmp(e.name, "ProgressBar") == 0) {
            P.push_back({"value", EditorKind::Int, nullptr, "0..max",
                [](wxWindow *w) { return wxString::Format("%d", static_cast<ProgressBar *>(w)->m_step); },
                [](wxWindow *w, const wxString &v) { static_cast<ProgressBar *>(w)->SetValue(int(s2l(v))); },
                false, 0, 100});
            P.push_back({"max", EditorKind::Int, nullptr, "value slider upper bound follows this",
                [](wxWindow *w) { return wxString::Format("%d", static_cast<ProgressBar *>(w)->m_max); },
                [](wxWindow *w, const wxString &v) {
                    static_cast<ProgressBar *>(w)->m_max = int(s2l(v));
                    static_cast<ProgressBar *>(w)->Refresh();
                },
                false, 1, 1000, "value"});
            P.push_back({"show_number", EditorKind::Bool, nullptr, nullptr,
                [](wxWindow *w) { return b2s(static_cast<ProgressBar *>(w)->m_shownumber); },
                [](wxWindow *w, const wxString &v) { static_cast<ProgressBar *>(w)->ShowNumber(s2b(v)); }});
        } else if (strcmp(e.name, "ErrorMsgStaticText") == 0) {
            P.push_back({"msg", EditorKind::Text, nullptr, "empty text crashes (historical defect) - guarded",
                [](wxWindow *w) { return static_cast<ErrorMsgStaticText *>(w)->m_msg; },
                [](wxWindow *w, const wxString &v) {
                    if (!v.empty())
                        static_cast<ErrorMsgStaticText *>(w)->SetLabel(v);
                }});
        } else if (strcmp(e.name, "wxBitmapButton") == 0) {
            P.push_back({"image", EditorKind::Text, nullptr,
                "local image path (png/jpg/bmp/gif) or resource name; empty = keep current",
                [](wxWindow *) { return wxString(); },
                [](wxWindow *w, const wxString &v) { apply_bitmap_image(w, v, 20, true); }});
        } else if (strcmp(e.name, "wxStaticBitmap") == 0) {
            P.push_back({"image", EditorKind::Text, nullptr,
                "local image path (png/jpg/bmp/gif) or resource name; empty = keep current",
                [](wxWindow *) { return wxString(); },
                [](wxWindow *w, const wxString &v) { apply_bitmap_image(w, v, 220, false); }});
        }
        add_common_props(P);
    }
}

void WidgetInspectorDialog::add_common_props(std::vector<WidgetInspectorDialog::PropDef> &props)
{
    props.push_back({"width", EditorKind::Int, nullptr, nullptr, width_get, width_set, false, 10, 600});
    props.push_back({"height", EditorKind::Int, nullptr, nullptr, height_get, height_set, false, 10, 600});
    props.push_back({"enabled", EditorKind::Bool, nullptr, nullptr,
        [](wxWindow *w) { return b2s(w->IsEnabled()); },
        [](wxWindow *w, const wxString &v) { w->Enable(s2b(v)); }});
    props.push_back({"tooltip", EditorKind::Text, nullptr, nullptr,
        [](wxWindow *) { return wxString(); },
        [](wxWindow *w, const wxString &v) { w->SetToolTip(v.empty() ? nullptr : new wxToolTip(v)); }});
}

WidgetInspectorDialog::WidgetInspectorDialog(wxWindow *parent)
    : DPIDialog(parent, wxID_ANY, _L("UI Inspector"), wxDefaultPosition, wxSize(1100, 780),
                wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX)
{
    // Remember the application theme so closing can restore it.
    m_initial_dark = Slic3r::GUI::ui().dark_mode();

    SetBackgroundColour(StateColor::lightModeColorFor(*wxWHITE));

    auto *top_sizer = new wxBoxSizer(wxVERTICAL);
    build_top_bar(top_sizer);

    m_splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
    m_splitter->SetSashGravity(0.32);
    top_sizer->Add(m_splitter, 1, wxEXPAND | wxALL, 6);

    rebuild_tree();

    SetSizer(top_sizer);

    build_right_pane(); // creates the resident m_right (plate/grid/info)
    update_for_selection(); // m_current == -1: shows the hint
    m_splitter->SplitVertically(m_tree, m_right, 300);

    Bind(wxEVT_CLOSE_WINDOW, &WidgetInspectorDialog::on_close, this);

    CenterOnScreen();
}

WidgetInspectorDialog::~WidgetInspectorDialog()
{
    // Safety net if the dialog is destroyed without going through on_close:
    // never leave the global theme flipped.
    StateColor::SetDarkMode(m_initial_dark);
}

void WidgetInspectorDialog::build_top_bar(wxSizer *sizer)
{
    auto *bar = new wxBoxSizer(wxHORIZONTAL);

    auto *title = new Label(this, _L("控件属性实验台 — 左侧选择控件，右侧编辑属性"));
    title->SetFont(Label::Head_16);
    bar->Add(title, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);

    bar->Add(new Label(this, _L("主题")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    // Dogfoods SegmentedToggle as the theme switch (carried over from the
    // gallery dialog this one replaces).
    m_theme_toggle = new SegmentedToggle(this, {_L("亮色"), _L("暗色")}, m_initial_dark ? 1 : 0);
    m_theme_toggle->bindSelectionCallback([this](int index) { on_theme_toggle(index); });
    bar->Add(m_theme_toggle, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

    sizer->Add(bar, 0, wxEXPAND | wxTOP | wxBOTTOM, 8);
}

namespace {
// Registry index riding inside a tree item's client data.
struct TreeItemIndex : wxClientData {
    explicit TreeItemIndex(int i) : index(i) {}
    int index;
};
} // namespace

void WidgetInspectorDialog::rebuild_tree()
{
    if (m_tree) {
        m_splitter->Unsplit(m_tree); // remove the old left pane first
        m_tree->Hide();
        m_tree->Destroy(); // deferred: dies in the event loop
        m_tree = nullptr;
    }

    m_tree = new wxDataViewTreeCtrl(m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxDV_ROW_LINES | wxDV_NO_HEADER);
    m_tree->SetBackgroundColour(StateColor::darkModeColorFor(*wxWHITE));
    m_tree->SetForegroundColour(StateColor::darkModeColorFor(*wxBLACK));

    for (int cat = 0; cat < 5; ++cat) {
        wxDataViewItem parent = m_tree->AppendContainer(wxDataViewItem(nullptr), CATEGORY_NAMES[cat]);
        for (size_t i = 0; i < registry().size(); ++i) {
            if (registry()[i].category != cat)
                continue;
            m_tree->AppendItem(parent, registry()[i].name, -1, new TreeItemIndex(int(i)));
        }
        m_tree->Expand(parent);
    }

    m_tree->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &WidgetInspectorDialog::on_widget_selected, this);
    m_tree->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, [this](wxDataViewEvent &event) {
        wxDataViewItem item = event.GetItem();
        if (!item.IsOk() || !m_tree)
            return;
        auto *data = static_cast<TreeItemIndex *>(m_tree->GetItemData(item));
        if (!data)
            return;
        // Target the right-clicked leaf, not the current selection.
        wxString name = registry()[data->index].name;
        wxMenu   menu;
        menu.Append(wxID_ANY, _L("复制控件名"));
        menu.Bind(wxEVT_MENU, [name](wxCommandEvent &) {
            // SetData fails harmlessly if the clipboard could not be opened.
            wxClipboardLocker locker;
            wxTheClipboard->SetData(new wxTextDataObject(name));
        });
        PopupMenu(&menu);
    });
    m_splitter->Initialize(m_tree);
}

void WidgetInspectorDialog::on_widget_selected(wxDataViewEvent &event)
{
    wxDataViewItem item = event.GetItem();
    if (!item.IsOk() || !m_tree)
        return;
    auto *data = static_cast<TreeItemIndex *>(m_tree->GetItemData(item));
    if (!data || data->index == m_current)
        return;

    m_current = data->index;
    m_prop_values.clear(); // spec: No persistence - switching discards edits
    update_for_selection();
}

void WidgetInspectorDialog::restore_selection()
{
    // After a tree rebuild (theme toggle) the visual selection is gone; walk
    // the categories to re-select the leaf matching m_current.
    // NB: this wx fork's wxDataViewTreeStore::GetNthChild dropped upstream's
    // pos < size() check, so its out-of-range result is a garbage item that
    // still passes IsOk() - the historical iterate-until-invalid walk here
    // read past the end of each category's child array and GetItemData then
    // dereferenced the garbage ID (0xC0000005 at 0xFFFFFFFFFFFFFFFF). Bound
    // both walks with GetChildCount instead of relying on an invalid marker.
    if (m_current < 0 || !m_tree)
        return;
    for (int cat = 0; cat < 5; ++cat) {
        wxDataViewItem parent = m_tree->GetNthChild(wxDataViewItem(nullptr), cat);
        if (!parent.IsOk())
            break;
        int count = m_tree->GetChildCount(parent);
        for (int child = 0; child < count; ++child) {
            wxDataViewItem leaf = m_tree->GetNthChild(parent, unsigned(child));
            auto *data = static_cast<TreeItemIndex *>(m_tree->GetItemData(leaf));
            if (data && data->index == m_current) {
                m_tree->Select(leaf);
                return;
            }
        }
    }
}

void WidgetInspectorDialog::build_right_pane()
{
    // One-time construction of the resident right pane. Selection changes
    // only rebind it (update_for_selection); this runs from the ctor and
    // from the theme toggle, where native editors must be recreated to
    // re-resolve their colors. The old pane dies deferred through
    // ReplaceWindow + Destroy - the pool structs are dropped immediately,
    // their windows with the old pane.
    m_plate = nullptr;
    m_instance = nullptr;
    m_info = nullptr;
    m_hint = nullptr;
    m_grid = nullptr;
    m_rows.clear();
    m_used.clear();
    m_int_editors.clear();

    auto *fresh = new wxScrolled<wxPanel>(m_splitter);
    fresh->SetScrollRate(10, 10);
    fresh->SetBackgroundColour(StateColor::lightModeColorFor(*wxWHITE));

    auto *s = new wxBoxSizer(wxVERTICAL);
    fresh->SetSizer(s);

    // Flicker-free plate: with BG_STYLE_PAINT wx never erase-fills the
    // window (the erase-then-paint double pass is what flickers while size
    // sliders drag), and every paint must cover the full area itself. A
    // non-zero corner radius routes StaticBox::render through its
    // memory-bitmap path, which clears the bitmap with the background
    // colour first - i.e. full coverage. (radius == 0 may draw nothing.)
    m_plate = new StaticBox(fresh, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0);
    m_plate->SetMinSize(wxSize(-1, 170));
    m_plate->SetBackgroundColour(StateColor::darkModeColorFor(wxColour(245, 245, 245)));
    m_plate->SetCornerRadius(12);
    m_plate->SetBackgroundStyle(wxBG_STYLE_PAINT);
    auto *plate_sizer = new wxBoxSizer(wxVERTICAL);
    plate_sizer->AddStretchSpacer();
    plate_sizer->AddStretchSpacer(); // the preview instance is inserted between the spacers
    m_plate->SetSizer(plate_sizer);
    s->Add(m_plate, 0, wxEXPAND | wxALL, 6);

    // Property row grid: rows are pooled, rebind detaches/attaches them.
    auto *grid = new wxFlexGridSizer(2, 8, 6);
    grid->AddGrowableCol(1, 1);
    m_grid = grid;
    s->Add(grid, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);

    // No-selection hint: the spacers around it are toggled by index in
    // update_for_selection (idx 2 and 4 of this sizer).
    s->AddStretchSpacer();
    m_hint = new Label(fresh, _L("左侧选择一个控件"));
    s->Add(m_hint, 0, wxALIGN_CENTER);
    s->AddStretchSpacer();

    auto *info = new wxTextCtrl(fresh, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
        wxTE_READONLY | wxTE_MULTILINE);
    info->SetMinSize(wxSize(-1, FromDIP(150)));
    info->SetBackgroundColour(StateColor::darkModeColorFor(wxColour(248, 248, 248)));
    s->Add(info, 0, wxEXPAND | wxALL, 6);
    m_info = info;

    fresh->Layout();
    // Virtual size from the sizer minimums: shorter dialog => scrollbar.
    fresh->FitInside();

    // Atomic swap: `fresh` was built hidden, so ReplaceWindow + Show exposes
    // the fully-built pane in one step. The old pane is NOT hidden first -
    // it stays visible underneath until its deferred Destroy runs, hiding
    // it early would flash an empty gap between old and new.
    if (m_right) {
        m_splitter->ReplaceWindow(m_right, fresh);
        m_right->Destroy(); // deferred: old pane + all children die in the event loop
    }
    fresh->Show();
    m_right = fresh;
}

void WidgetInspectorDialog::update_for_selection()
{
    // Selection hot path: rebind, don't rebuild.
    const bool has_sel = m_current >= 0 && m_current < int(registry().size());
    if (has_sel) {
        const WidgetEntry &entry = registry()[m_current];
        rebuild_preview();
        rebind_rows(entry);

        // Info panel: read-only reference text - selectable and copyable,
        // but not editable (wxTE_READONLY gives exactly that semantics).
        // Doc literals are UTF-8 bytes; stream them through FromUTF8 (a raw
        // const char* would convert via the local ANSI codepage and come
        // out empty/garbled on a Chinese locale).
        wxString info;
        info << _L("类继承: ") << (entry.inherits ? entry.inherits : "") << "\n"
             << _L("头文件: ") << (entry.header ? entry.header : "");
        if (const WidgetDoc *doc = doc_for(entry.name)) {
            info << "\n" << _L("说明: ") << wxString::FromUTF8(doc->desc)
                 << "\n" << _L("使用场景: ") << wxString::FromUTF8(doc->usage)
                 << "\n" << _L("注意: ") << wxString::FromUTF8(doc->caveats);
        }
        m_info->ChangeValue(info);
    } else {
        m_grid->Clear(false);
        for (auto &r : m_rows) {
            r->label->Hide();
            r->editor->Hide();
            if (r->note)
                r->note->Hide();
        }
        m_used.clear();
        m_int_editors.clear();
    }

    m_plate->Show(has_sel);
    m_info->Show(has_sel);
    m_hint->Show(!has_sel);
    wxSizer *ps = m_right->GetSizer();
    ps->Show(2, !has_sel); // hint spacers (indices fixed in build_right_pane)
    ps->Show(4, !has_sel);
    m_right->Layout();
    m_right->FitInside();
    m_right->Refresh();
}

void WidgetInspectorDialog::rebuild_preview()
{
    if (m_instance) {
        m_plate->GetSizer()->Detach(m_instance);
        m_instance->Destroy(); // deferred: dies in the event loop
        m_instance = nullptr;
    }
    if (m_current < 0 || m_current >= int(registry().size()))
        return;
    const WidgetEntry &entry = registry()[m_current];
    m_instance = entry.factory(m_plate, m_prop_values);
    m_plate->GetSizer()->Insert(1, m_instance, 0, wxALIGN_CENTER | wxALL, 16);
    m_plate->Layout();
}

WidgetInspectorDialog::Row *WidgetInspectorDialog::create_row(EditorKind kind)
{
    // Pool row factory: wires the editor's event handlers ONCE to the stable
    // Row - they call row->apply, which rebind_rows reassigns per selection.
    auto *row     = new Row();
    row->kind     = kind;
    row->label    = new Label(m_right, "");
    row->guard    = std::make_shared<bool>(false);

    switch (kind) {
    case EditorKind::Text: {
        auto *input = new TextInput(m_right, "", "", "", wxDefaultPosition, wxSize(-1, 34));
        input->SetCornerRadius(4);
        input->GetTextCtrl()->Bind(wxEVT_TEXT, [row](wxCommandEvent &e) {
            if (row->apply)
                row->apply(e.GetString());
        });
        row->editor = input;
        break;
    }
    case EditorKind::Int: {
        // SliderSpin composite: dragging the slider or typing in the spin
        // both apply immediately; the shared guard flag prevents the two
        // from re-triggering each other in a loop (spin->SetValue updates
        // its text through SetValue, which fires wxEVT_TEXT).
        auto *box = new wxPanel(m_right, wxID_ANY);
        box->SetBackgroundColour(m_right->GetBackgroundColour());
        auto *sz = new wxBoxSizer(wxHORIZONTAL);

        row->slider = new wxSlider(box, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxDefaultSize,
            wxSL_HORIZONTAL);
        row->spin   = new SpinInput(box, "0", "", wxDefaultPosition, wxSize(FromDIP(92), -1), 0, 0, 100, 0, 1);

        wxSlider *slider = row->slider;
        SpinInput *spin = row->spin;
        auto guard = row->guard;
        row->slider->Bind(wxEVT_SLIDER, [row, spin, guard](wxCommandEvent &e) {
            if (*guard) return;
            *guard = true;
            spin->SetValue(e.GetInt());
            *guard = false;
            if (row->apply)
                row->apply(wxString::Format("%d", e.GetInt()));
        });
        row->spin->GetTextCtrl()->Bind(wxEVT_TEXT, [row, slider, guard](wxCommandEvent &e) {
            if (*guard) return;
            *guard = true;
            long v = s2l(e.GetString());
            v = std::clamp(v, long(slider->GetMin()), long(slider->GetMax()));
            slider->SetValue(int(v));
            *guard = false;
            if (row->apply)
                row->apply(wxString::Format("%ld", v));
        });

        sz->Add(slider, 1, wxALIGN_CENTER_VERTICAL);
        sz->Add(spin, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
        box->SetSizer(sz);
        row->editor = box;
        break;
    }
    case EditorKind::Bool: {
        auto *chk = new CheckBox(m_right);
        chk->Bind(wxEVT_TOGGLEBUTTON, [row, chk](wxCommandEvent &e) {
            bool v = e.GetInt() != 0;
            if (row->apply)
                row->apply(b2s(v));
            // Re-sync the editor bitmaps: CheckBox's own in-event update()
            // runs while GetValue() still holds the pre-toggle value and
            // leaves the wrong bitmap behind.
            chk->SetValue(v);
        });
        row->editor = chk;
        break;
    }
    case EditorKind::Enum: {
        auto *combo = new ComboBox(m_right, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0, nullptr);
        combo->SetMinSize(wxSize(-1, 34)); // shrinks to ~25px without it
        combo->Bind(wxEVT_COMBOBOX, [row](wxCommandEvent &e) {
            if (row->apply)
                row->apply(e.GetString());
        });
        row->editor = combo;
        break;
    }
    case EditorKind::StringList: {
        // Commit on Enter (wxTE_PROCESS_ENTER): recreate-flagged edits
        // rebuild only the preview instance now, but Enter-commit keeps the
        // same explicit semantics as before.
        auto *text = new wxTextCtrl(m_right, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 64),
            wxTE_MULTILINE | wxTE_PROCESS_ENTER);
        text->Bind(wxEVT_TEXT_ENTER, [row](wxCommandEvent &e) {
            if (row->apply)
                row->apply(e.GetString());
        });
        row->editor = text;
        break;
    }
    }
    return row;
}

void WidgetInspectorDialog::rebind_rows(const WidgetEntry &entry)
{
    // Detach everything (windows survive), then attach the bound rows back
    // in schema order; sizers ignore hidden children, so hiding unused pool
    // rows is enough. Rebind assigns `apply` BEFORE touching editor values -
    // a fired event must never reach the previous selection's binding.
    m_grid->Clear(false);
    m_used.clear();
    m_int_editors.clear();

    for (size_t i = 0; i < entry.props.size(); ++i) {
        const PropDef &prop = entry.props[i];

        Row *row = nullptr;
        for (auto &r : m_rows) {
            if (r->kind != prop.kind)
                continue;
            bool taken = std::any_of(m_used.begin(), m_used.end(),
                [p = r.get()](const std::pair<Row *, size_t> &u) { return u.first == p; });
            if (!taken) {
                row = r.get();
                break;
            }
        }
        if (!row) {
            row = create_row(prop.kind);
            m_rows.emplace_back(row);
        }

        row->label->SetLabel(prop.name);
        int entry_index = m_current;
        row->apply      = [this, entry_index, i](const wxString &v) { apply_property(entry_index, i, v); };

        if (row->kind == EditorKind::Enum) {
            // Enum value lists are per property - refill the pooled combo.
            auto *combo = static_cast<ComboBox *>(row->editor);
            combo->Clear();
            for (const wxString &v : *prop.enum_values)
                combo->Append(v);
        }
        if (row->kind == EditorKind::Int) {
            row->slider->SetRange(prop.slider_min, prop.slider_max);
            row->spin->SetRange(prop.slider_min, prop.slider_max);
            m_int_editors[prop.name] = {row->slider, row->spin};
        }

        // Editor init value: session edit if present, else read from the
        // fresh instance (get may be null for recreate-flagged props).
        wxString value;
        auto     it = m_prop_values.find(prop.name);
        if (it != m_prop_values.end())
            value = it->second;
        else if (prop.get)
            value = prop.get(m_instance);
        set_row_value(*row, value);

        row->label->Show();
        row->editor->Show();
        if (prop.note && *prop.note) {
            if (!row->note) {
                row->note = new Label(m_right, prop.note);
                row->note->SetForegroundColour(wxColour(128, 128, 128));
            } else {
                row->note->SetLabel(prop.note);
            }
            row->note->Show();
        } else if (row->note) {
            row->note->Hide();
        }

        m_grid->Add(row->label, 0, wxALIGN_CENTER_VERTICAL);
        m_grid->Add(row->editor, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
        if (row->note && row->note->IsShown()) {
            m_grid->AddSpacer(0);
            m_grid->Add(row->note, 1, wxBOTTOM, 2);
        }
        m_used.push_back({row, i});
    }

    // Hide rows this entry does not use.
    for (auto &r : m_rows) {
        bool used = std::any_of(m_used.begin(), m_used.end(),
            [p = r.get()](const std::pair<Row *, size_t> &u) { return u.first == p; });
        if (!used) {
            r->label->Hide();
            r->editor->Hide();
            if (r->note)
                r->note->Hide();
        }
    }
}

void WidgetInspectorDialog::set_row_value(Row &row, const wxString &value)
{
    switch (row.kind) {
    case EditorKind::Text:
        static_cast<TextInput *>(row.editor)->GetTextCtrl()->ChangeValue(value);
        break;
    case EditorKind::Int: {
        // Guarded: SpinInput::SetValue updates its text through SetValue,
        // which fires wxEVT_TEXT - without the guard this re-enters the
        // row's own spin handler and applies a non-edited value.
        long v = s2l(value);
        v = std::clamp(v, long(row.slider->GetMin()), long(row.slider->GetMax()));
        *row.guard = true;
        row.slider->SetValue(int(v));
        row.spin->SetValue(int(v));
        *row.guard = false;
        break;
    }
    case EditorKind::Bool:
        static_cast<CheckBox *>(row.editor)->SetValue(s2b(value));
        break;
    case EditorKind::Enum:
        static_cast<ComboBox *>(row.editor)->SetValue(value);
        break;
    case EditorKind::StringList:
        static_cast<wxTextCtrl *>(row.editor)->ChangeValue(value);
        break;
    }
}

void WidgetInspectorDialog::refresh_row_values(const WidgetEntry &entry, size_t skip_prop_index)
{
    for (auto &u : m_used) {
        if (u.second == skip_prop_index)
            continue;
        const PropDef &prop = entry.props[u.second];
        wxString value;
        auto     it = m_prop_values.find(prop.name);
        if (it != m_prop_values.end())
            value = it->second;
        else if (prop.get)
            value = prop.get(m_instance);
        set_row_value(*u.first, value);
    }
}

void WidgetInspectorDialog::apply_property(int entry_index, size_t prop_index, const wxString &value)
{
    if (entry_index < 0 || entry_index >= int(registry().size()))
        return;
    const WidgetEntry &entry = registry()[entry_index];
    if (!m_instance || prop_index >= entry.props.size())
        return;
    const PropDef &prop = entry.props[prop_index];

    m_prop_values[prop.name] = value;
    if (prop.set) {
        // Freeze the plate during the change: intermediate paints of the
        // resize/enable leave stale pixels (ghosting) and per-child repaints
        // flash the plate; one Thaw + Refresh ends with a single clean frame.
        if (m_plate)
            m_plate->Freeze();
        prop.set(m_instance, value);
        if (m_plate) {
            m_plate->Layout();
            m_plate->Thaw();
            m_plate->Refresh();
        }
    }

    // Range dependency (e.g. ProgressBar max -> value: upper bound = max).
    if (prop.refresh_range_of) {
        auto it = m_int_editors.find(prop.refresh_range_of);
        if (it != m_int_editors.end()) {
            long hi = s2l(value);
            it->second.first->SetRange(it->second.first->GetMin(), int(hi));
            it->second.second->SetRange(int(s2l(wxString::Format("%d", it->second.first->GetMin()))), int(hi));
        }
    }

    if (prop.recreate) {
        // Items/direction/style exist only at construction time: rebuild
        // only the preview instance (the factory reads the new value from
        // the store). The rows - and the editor being typed in - stay.
        rebuild_preview();
        refresh_row_values(entry, prop_index);
        if (m_plate) {
            m_plate->Layout();
            m_plate->Refresh();
        }
        m_right->Layout();
        m_right->FitInside();
    }
}

void WidgetInspectorDialog::on_theme_toggle(int index)
{
    StateColor::SetDarkMode(index == 1);
    // Widgets resolve colors through StateColor at render time; rebuilding
    // the right pane (and with it the editor row pool - native editors
    // resolve colors at creation) guarantees every editor and the preview
    // pick up the new palette. The tree is recreated too so its colors
    // re-resolve. The dialog's own chrome must re-resolve as well - it kept
    // the color resolved at open time, leaving light strips around a dark
    // dialog.
    SetBackgroundColour(StateColor::lightModeColorFor(*wxWHITE));
    m_splitter->SetBackgroundColour(StateColor::lightModeColorFor(*wxWHITE));
    rebuild_tree();
    build_right_pane();
    update_for_selection();
    m_splitter->SplitVertically(m_tree, m_right, m_splitter->GetSashPosition());
    restore_selection();
    m_theme_toggle->Refresh(); // top bar persists across rebuilds
    Refresh();
}

void WidgetInspectorDialog::on_close(wxCloseEvent &)
{
    if (m_standalone) {
        // widget_lab mode: this dialog is the main window - closing it ends
        // the process (no application theme to restore).
        Destroy();
        if (wxTheApp)
            wxTheApp->ExitMainLoop();
        return;
    }
    StateColor::SetDarkMode(m_initial_dark);
    // Modeless + MainFrame-owned: hide instead of destroy so the menu entry
    // can simply re-raise the same instance.
    Hide();
}

void WidgetInspectorDialog::on_dpi_changed(const wxRect &)
{
    Layout();
    Fit();
}

} // namespace GUI
} // namespace Slic3r
