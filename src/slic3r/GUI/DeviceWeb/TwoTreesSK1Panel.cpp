#include "TwoTreesSK1Panel.hpp"

#include <wx/app.h>
#include <wx/filedlg.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/msgdlg.h>

#include <filesystem>
#include <thread>
#include <utility>

namespace Slic3r { namespace GUI {

TwoTreesSK1Panel::TwoTreesSK1Panel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
    , m_poll_timer(this)
{
    SetBackgroundColour(*wxWHITE);
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* heading = new wxStaticText(this, wxID_ANY, "TwoTrees SK1 — Moonraker");
    wxFont heading_font = heading->GetFont();
    heading_font.SetPointSize(heading_font.GetPointSize() + 3);
    heading_font.SetWeight(wxFONTWEIGHT_BOLD);
    heading->SetFont(heading_font);
    root->Add(heading, 0, wxALL, FromDIP(12));

    auto* description = new wxStaticText(this, wxID_ANY,
        "Two independent printer connections. Select a printer card before uploading or controlling a print.");
    root->Add(description, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    const auto defaults = MoonrakerPrinterConfig::DefaultPrinters();
    for (std::size_t i = 0; i < m_printers.size(); ++i) {
        m_printers[i].config = defaults[i];
        BuildCard(i, root);
    }

    root->AddStretchSpacer(1);
    SetSizer(root);
    Layout();

    Bind(wxEVT_TIMER, &TwoTreesSK1Panel::OnPollTimer, this, m_poll_timer.GetId());
    Bind(wxEVT_SHOW, [this](wxShowEvent& event) {
        if (event.IsShown()) {
            m_poll_timer.Start(10000);
            for (std::size_t i = 0; i < m_printers.size(); ++i)
                RefreshPrinter(i);
        } else {
            m_poll_timer.Stop();
        }
        event.Skip();
    });

    // Safe startup check: only reads Moonraker status; it never starts a print.
    if (IsShownOnScreen()) {
        m_poll_timer.Start(10000);
        for (std::size_t i = 0; i < m_printers.size(); ++i)
            RefreshPrinter(i);
    }
}

void TwoTreesSK1Panel::BuildCard(std::size_t index, wxSizer* parent_sizer)
{
    auto& card = m_printers[index];
    auto* group = new wxStaticBoxSizer(wxVERTICAL, this, card.config.display_name);
    auto* status_row = new wxBoxSizer(wxHORIZONTAL);
    card.connection = new wxStaticText(this, wxID_ANY, "Not checked");
    card.details = new wxStaticText(this, wxID_ANY, card.config.base_url);
    status_row->Add(card.connection, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    status_row->Add(card.details, 1, wxALIGN_CENTER_VERTICAL);
    group->Add(status_row, 0, wxEXPAND | wxALL, FromDIP(8));

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    card.refresh = new wxButton(this, wxID_ANY, "Refresh");
    card.upload = new wxButton(this, wxID_ANY, "Upload G-code");
    card.start = new wxButton(this, wxID_ANY, "Start");
    card.pause = new wxButton(this, wxID_ANY, "Pause");
    card.resume = new wxButton(this, wxID_ANY, "Resume");
    card.cancel = new wxButton(this, wxID_ANY, "Stop");
    for (auto* button : {card.refresh, card.upload, card.start, card.pause, card.resume, card.cancel})
        buttons->Add(button, 0, wxRIGHT, FromDIP(6));
    group->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));
    parent_sizer->Add(group, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    card.refresh->Bind(wxEVT_BUTTON, [this, index](wxCommandEvent&) { RefreshPrinter(index); });
    card.upload->Bind(wxEVT_BUTTON, [this, index](wxCommandEvent&) { UploadGcode(index); });
    card.start->Bind(wxEVT_BUTTON, [this, index](wxCommandEvent&) { StartPrint(index); });
    card.pause->Bind(wxEVT_BUTTON, [this, index](wxCommandEvent&) {
        RunOperation(index, "Pause", [](const MoonrakerHttpClient& client) { return client.PausePrint(); });
    });
    card.resume->Bind(wxEVT_BUTTON, [this, index](wxCommandEvent&) {
        RunOperation(index, "Resume", [](const MoonrakerHttpClient& client) { return client.ResumePrint(); });
    });
    card.cancel->Bind(wxEVT_BUTTON, [this, index](wxCommandEvent&) {
        const wxString prompt = wxString::Format(
            "Cancel the current print on %s?", wxString::FromUTF8(card.config.display_name));
        if (wxMessageBox(prompt, "Confirm stop", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) == wxYES) {
            RunOperation(index, "Stop", [](const MoonrakerHttpClient& client) { return client.CancelPrint(); });
        }
    });
}

void TwoTreesSK1Panel::SetBusy(std::size_t index, bool busy)
{
    auto& card = m_printers[index];
    card.busy = busy;
    for (auto* button : {card.refresh, card.upload, card.start, card.pause, card.resume, card.cancel})
        button->Enable(!busy);
    if (busy)
        card.connection->SetLabel("Working…");
}

void TwoTreesSK1Panel::RunOperation(
    std::size_t index,
    const std::string& operation_name,
    std::function<MoonrakerHttpResult(const MoonrakerHttpClient&)> operation,
    std::function<void(const MoonrakerHttpResult&)> on_success)
{
    if (index >= m_printers.size() || m_printers[index].busy)
        return;

    SetBusy(index, true);
    const auto config = m_printers[index].config;
    wxWeakRef<TwoTreesSK1Panel> weak_this(this);

    std::thread([weak_this, index, operation_name, config,
                 operation = std::move(operation), on_success = std::move(on_success)]() mutable {
        MoonrakerHttpClient client(config);
        MoonrakerHttpResult result = operation(client);
        if (wxTheApp) {
            wxTheApp->CallAfter([weak_this, index, operation_name,
                                 result = std::move(result), on_success = std::move(on_success)]() mutable {
                if (!weak_this)
                    return;
                weak_this->SetBusy(index, false);
                weak_this->ApplyResult(index, operation_name, result);
                if (result.success && on_success)
                    on_success(result);
            });
        }
    }).detach();
}

void TwoTreesSK1Panel::ApplyResult(std::size_t index, const std::string& operation_name,
                                   const MoonrakerHttpResult& result)
{
    auto& card = m_printers[index];
    if (!result.success) {
        card.connection->SetLabel("Offline / error");
        card.details->SetLabel(operation_name + ": " + result.message);
        return;
    }

    card.connection->SetLabel("Connected");
    std::string detail = operation_name + " successful";
    if (result.data.is_object() && result.data.contains("printer_status")) {
        const auto& status = result.data["printer_status"];
        if (status.contains("result") && status["result"].contains("status") &&
            status["result"]["status"].contains("print_stats")) {
            const auto& print_stats = status["result"]["status"]["print_stats"];
            if (print_stats.contains("state") && print_stats["state"].is_string())
                detail = "Print state: " + print_stats["state"].get<std::string>();
        }
    } else if (result.data.is_object() && result.data.contains("result") &&
               result.data["result"].is_object() &&
               result.data["result"].contains("klippy_state")) {
        detail = "Klipper: " + result.data["result"]["klippy_state"].get<std::string>();
    }
    card.details->SetLabel(detail);
}

void TwoTreesSK1Panel::OnPollTimer(wxTimerEvent& event)
{
    (void) event;
    for (std::size_t i = 0; i < m_printers.size(); ++i)
        RefreshPrinter(i);
}

void TwoTreesSK1Panel::RefreshPrinter(std::size_t index)
{
    RunOperation(index, "Status refresh", [](const MoonrakerHttpClient& client) {
        auto info = client.GetServerInfo();
        if (!info.success)
            return info;
        auto status = client.GetPrinterStatus();
        if (!status.success)
            return status;
        info.data["printer_status"] = status.data;
        return info;
    });
}

void TwoTreesSK1Panel::UploadGcode(std::size_t index)
{
    if (m_printers[index].busy)
        return;

    wxFileDialog dialog(this, "Select G-code file", wxEmptyString, wxEmptyString,
                        "G-code files (*.gcode;*.gco;*.g)|*.gcode;*.gco;*.g|All files (*.*)|*.*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
        return;

    const std::string path = std::string(dialog.GetPath().ToUTF8().data());
    const std::string filename = std::filesystem::u8path(path).filename().u8string();
    RunOperation(index, "Upload", [path](const MoonrakerHttpClient& client) {
        return client.UploadFile(path);
    }, [this, index, filename](const MoonrakerHttpResult&) {
        m_printers[index].uploaded_filename = filename;
        m_printers[index].details->SetLabel("Uploaded: " + filename);
    });
}

void TwoTreesSK1Panel::StartPrint(std::size_t index)
{
    const std::string filename = m_printers[index].uploaded_filename;
    if (filename.empty()) {
        m_printers[index].details->SetLabel("Upload a G-code file before starting a print.");
        return;
    }
    RunOperation(index, "Start print", [filename](const MoonrakerHttpClient& client) {
        return client.StartPrint(filename);
    });
}

}} // namespace Slic3r::GUI
