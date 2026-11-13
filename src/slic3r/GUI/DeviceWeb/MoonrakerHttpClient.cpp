#include "MoonrakerHttpClient.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <utility>

namespace Slic3r { namespace GUI {

namespace {

struct ParsedEndpoint
{
    std::string host;
    std::string port{"7125"};
};

bool ParseHttpEndpoint(const std::string& url, ParsedEndpoint& endpoint, std::string& error)
{
    constexpr const char* prefix = "http://";
    if (url.rfind(prefix, 0) != 0) {
        error = "Moonraker currently supports plain HTTP endpoints only.";
        return false;
    }

    const std::string authority = url.substr(7);
    if (authority.empty() || authority.find_first_of("/?#@") != std::string::npos) {
        error = "Invalid Moonraker endpoint.";
        return false;
    }

    const auto colon = authority.rfind(':');
    if (colon == std::string::npos) {
        endpoint.host = authority;
    } else {
        endpoint.host = authority.substr(0, colon);
        endpoint.port = authority.substr(colon + 1);
    }
    if (endpoint.host.empty() || endpoint.port.empty()) {
        error = "Moonraker host or port is missing.";
        return false;
    }
    return true;
}

std::string JsonBody(const nlohmann::json& value)
{
    return value.dump();
}

MoonrakerHttpResult ErrorResult(const std::string& message)
{
    MoonrakerHttpResult result;
    result.message = message;
    return result;
}

} // namespace

MoonrakerHttpClient::MoonrakerHttpClient(MoonrakerPrinterConfig config)
    : m_config(std::move(config))
{
}

MoonrakerHttpResult MoonrakerHttpClient::Request(const std::string& method,
                                                  const std::string& target,
                                                  const std::string& body,
                                                  const std::string& content_type) const
{
    std::string validation_error;
    if (!m_config.IsValid(&validation_error))
        return ErrorResult(validation_error);

    ParsedEndpoint endpoint;
    if (!ParseHttpEndpoint(m_config.base_url, endpoint, validation_error))
        return ErrorResult(validation_error);

    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace net = boost::asio;
    using tcp = net::ip::tcp;

    try {
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);
        stream.expires_after(std::chrono::seconds(12));
        const auto endpoints = resolver.resolve(endpoint.host, endpoint.port);
        stream.connect(endpoints);

        http::request<http::string_body> request{
            method == "POST" ? http::verb::post : http::verb::get, target, 11};
        request.set(http::field::host, endpoint.host + (endpoint.port == "80" ? "" : ":" + endpoint.port));
        request.set(http::field::user_agent, "BambuStudio-TwoTreesSK1/1.0");
        request.set(http::field::connection, "close");
        if (!body.empty()) {
            request.set(http::field::content_type, content_type);
            request.body() = body;
            request.prepare_payload();
        }

        http::write(stream, request);
        beast::flat_buffer buffer;
        http::response<http::string_body> response;
        stream.expires_after(std::chrono::seconds(20));
        http::read(stream, buffer, response);

        MoonrakerHttpResult result;
        result.http_status = response.result_int();
        const std::string response_body = response.body();
        boost::system::error_code shutdown_error;
        stream.socket().shutdown(tcp::socket::shutdown_both, shutdown_error);

        if (!response_body.empty()) {
            result.data = nlohmann::json::parse(response_body, nullptr, false);
        }
        if (response.result() != http::status::ok &&
            response.result() != http::status::created) {
            if (result.data.is_object() && result.data.contains("error")) {
                const auto& error = result.data["error"];
                result.message = error.is_string() ? error.get<std::string>() : error.dump();
            } else {
                const auto reason = response.reason();
                result.message.assign(reason.data(), reason.size());
            }
            return result;
        }
        if (result.data.is_discarded()) {
            result.message = "Moonraker returned an invalid JSON response.";
            return result;
        }
        if (result.data.is_object() && result.data.contains("error")) {
            result.message = result.data["error"].is_string()
                ? result.data["error"].get<std::string>() : result.data["error"].dump();
            return result;
        }
        result.success = true;
        return result;
    } catch (const std::exception& exception) {
        return ErrorResult(exception.what());
    }
}

MoonrakerHttpResult MoonrakerHttpClient::GetServerInfo() const
{
    return Request("GET", "/server/info");
}

MoonrakerHttpResult MoonrakerHttpClient::GetPrinterStatus() const
{
    return Request("GET",
        "/printer/objects/query?print_stats&display_status&toolhead&heater_bed&extruder");
}

MoonrakerHttpResult MoonrakerHttpClient::UploadFile(const std::string& local_path,
                                                     const std::string& remote_root) const
{
    const std::filesystem::path source_path = std::filesystem::u8path(local_path);
    std::ifstream input(source_path, std::ios::binary);
    if (!input)
        return ErrorResult("Could not open the selected G-code file.");

    const std::string filename = source_path.filename().u8string();
    if (filename.empty() || filename.find_first_of("\r\n\"") != std::string::npos)
        return ErrorResult("Invalid G-code filename.");
    if (remote_root != "gcodes")
        return ErrorResult("Only the Moonraker gcodes root is supported.");

    const std::string boundary = "----BambuStudioMoonrakerBoundary7MA4YWxk";
    std::ostringstream prefix;
    prefix << "--" << boundary << "\r\n"
           << "Content-Disposition: form-data; name=\"file\"; filename=\"" << filename << "\"\r\n"
           << "Content-Type: application/octet-stream\r\n\r\n";
    std::ostringstream suffix;
    suffix << "\r\n--" << boundary << "\r\n"
           << "Content-Disposition: form-data; name=\"root\"\r\n\r\n"
           << remote_root << "\r\n--" << boundary << "--\r\n";

    const std::string file_data((std::istreambuf_iterator<char>(input)),
                                std::istreambuf_iterator<char>());
    const std::string body = prefix.str() + file_data + suffix.str();
    return Request("POST", "/server/files/upload", body,
                   "multipart/form-data; boundary=" + boundary);
}

MoonrakerHttpResult MoonrakerHttpClient::StartPrint(const std::string& filename) const
{
    if (filename.empty() || filename.find_first_of("\r\n") != std::string::npos)
        return ErrorResult("Invalid G-code filename.");
    return Request("POST", "/printer/print/start",
                   JsonBody({{"filename", filename}}));
}

MoonrakerHttpResult MoonrakerHttpClient::PausePrint() const
{
    return Request("POST", "/printer/print/pause", "{}");
}

MoonrakerHttpResult MoonrakerHttpClient::ResumePrint() const
{
    return Request("POST", "/printer/print/resume", "{}");
}

MoonrakerHttpResult MoonrakerHttpClient::CancelPrint() const
{
    return Request("POST", "/printer/print/cancel", "{}");
}

}} // namespace Slic3r::GUI
