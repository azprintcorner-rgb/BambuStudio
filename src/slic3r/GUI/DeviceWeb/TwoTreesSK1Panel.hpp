#ifndef TWOTREESSK1PANEL_HPP
#define TWOTREESSK1PANEL_HPP

#include "MoonrakerPrinterConfig.hpp"
#include "MoonrakerHttpClient.hpp"

#include <wx/button.h>
#include <wx/panel.h>
#include <wx/stattext.h>
#include <wx/weakref.h>

#include <array>
#include <cstddef>
#include <functional>
#include <string>

class wxSizer;

namespace Slic3r { namespace GUI {

class TwoTreesSK1Panel : public wxPanel
{
public:
    explicit TwoTreesSK1Panel(wxWindow* parent);
    ~TwoTreesSK1Panel() override = default;

private:
    struct PrinterCard
    {
        MoonrakerPrinterConfig config;
        wxStaticText* connection{nullptr};
        wxStaticText* details{nullptr};
        wxButton* refresh{nullptr};
        wxButton* upload{nullptr};
        wxButton* start{nullptr};
        wxButton* pause{nullptr};
        wxButton* resume{nullptr};
        wxButton* cancel{nullptr};
        std::string uploaded_filename;
        bool busy{false};
    };

    void BuildCard(std::size_t index, wxSizer* parent_sizer);
    void RefreshPrinter(std::size_t index);
    void UploadGcode(std::size_t index);
    void StartPrint(std::size_t index);
    void RunOperation(std::size_t index,
                      const std::string& operation_name,
                      std::function<MoonrakerHttpResult(const MoonrakerHttpClient&)> operation,
                      std::function<void(const MoonrakerHttpResult&)> on_success = {});
    void SetBusy(std::size_t index, bool busy);
    void ApplyResult(std::size_t index, const std::string& operation_name,
                     const MoonrakerHttpResult& result);

    std::array<PrinterCard, 2> m_printers;
};

}} // namespace Slic3r::GUI

#endif // TWOTREESSK1PANEL_HPP
