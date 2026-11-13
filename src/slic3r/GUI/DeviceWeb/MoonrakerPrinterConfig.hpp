#ifndef MOONRAKERPRINTERCONFIG_HPP
#define MOONRAKERPRINTERCONFIG_HPP

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace Slic3r { namespace GUI {

// A Moonraker printer is deliberately not a Bambu DeviceManager machine.
// Its stable id, endpoint and display name belong to this independent profile.
struct MoonrakerPrinterConfig
{
    std::string id;
    std::string display_name;
    std::string base_url;

    bool IsValid(std::string* reason = nullptr) const
    {
        const auto fail = [reason](const char* message) {
            if (reason) *reason = message;
            return false;
        };

        if (id.empty()) return fail("Printer id is required.");
        if (display_name.empty()) return fail("Printer display name is required.");
        if (base_url.empty()) return fail("Moonraker base URL is required.");

        // Only HTTP(S) endpoints are accepted. Credentials must not be embedded
        // in URLs, and path/query/fragment components are not valid base URLs.
        const bool http = base_url.rfind("http://", 0) == 0;
        if (!http) return fail("URL must start with http://. HTTPS is not supported by this local Moonraker client yet.");

        const std::size_t authority_start = 7;
        const std::string authority = base_url.substr(authority_start);
        if (authority.empty()) return fail("URL host is missing.");
        if (authority.find_first_of("/?#@") != std::string::npos)
            return fail("URL must contain only a host and optional port; do not embed credentials or paths.");
        if (std::any_of(base_url.begin(), base_url.end(), [](unsigned char c) {
                return std::isspace(c) || std::iscntrl(c);
            }))
            return fail("URL cannot contain whitespace or control characters.");

        if (reason) reason->clear();
        return true;
    }

    static std::vector<MoonrakerPrinterConfig> DefaultPrinters()
    {
        return {
            {"twotrees-sk1-1", "TwoTrees SK1 (1)", "http://192.168.68.54"},
            {"twotrees-sk1-2", "TwoTrees SK1 (2)", "http://192.168.68.53"}
        };
    }
};

inline void to_json(nlohmann::json& json, const MoonrakerPrinterConfig& printer)
{
    json = nlohmann::json{
        {"id", printer.id},
        {"display_name", printer.display_name},
        {"base_url", printer.base_url}
    };
}

inline void from_json(const nlohmann::json& json, MoonrakerPrinterConfig& printer)
{
    json.at("id").get_to(printer.id);
    json.at("display_name").get_to(printer.display_name);
    json.at("base_url").get_to(printer.base_url);
}

}} // namespace Slic3r::GUI

#endif // MOONRAKERPRINTERCONFIG_HPP
