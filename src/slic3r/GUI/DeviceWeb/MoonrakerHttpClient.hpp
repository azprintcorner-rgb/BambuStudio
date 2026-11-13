#ifndef MOONRAKERHTTPCLIENT_HPP
#define MOONRAKERHTTPCLIENT_HPP

#include "MoonrakerPrinterConfig.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace Slic3r { namespace GUI {

struct MoonrakerHttpResult
{
    bool success{false};
    unsigned int http_status{0};
    std::string message;
    nlohmann::json data{nlohmann::json::object()};
};

class MoonrakerHttpClient
{
public:
    explicit MoonrakerHttpClient(MoonrakerPrinterConfig config);

    const MoonrakerPrinterConfig& Config() const { return m_config; }

    // These calls are synchronous. Invoke them from a worker thread, never the GUI thread.
    MoonrakerHttpResult GetServerInfo() const;
    MoonrakerHttpResult GetPrinterStatus() const;
    MoonrakerHttpResult UploadFile(const std::string& local_path,
                                   const std::string& remote_root = "gcodes") const;
    MoonrakerHttpResult StartPrint(const std::string& filename) const;
    MoonrakerHttpResult PausePrint() const;
    MoonrakerHttpResult ResumePrint() const;
    MoonrakerHttpResult CancelPrint() const;

private:
    MoonrakerHttpResult Request(const std::string& method,
                                const std::string& target,
                                const std::string& body = {},
                                const std::string& content_type = "application/json") const;

    MoonrakerPrinterConfig m_config;
};

}} // namespace Slic3r::GUI

#endif // MOONRAKERHTTPCLIENT_HPP
