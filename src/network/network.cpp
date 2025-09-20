#include "dgit/network.hpp"
#include <algorithm>
#include <sstream>
#include <regex>
#include <curl/curl.h>
#include <libssh/libssh.h>

namespace dgit {

// HTTP Transport implementation
HTTPTransport::HTTPTransport() : curl_handle_(nullptr), connected_(false) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HTTPTransport::~HTTPTransport() {
    disconnect();
    curl_global_cleanup();
}

bool HTTPTransport::connect(const std::string& url) {
    if (connected_) {
        disconnect();
    }

    curl_handle_ = curl_easy_init();
    if (!curl_handle_) {
        return false;
    }

    curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYHOST, 0L);

    connected_ = true;
    return true;
}

void HTTPTransport::disconnect() {
    if (curl_handle_) {
        curl_easy_cleanup(curl_handle_);
        curl_handle_ = nullptr;
    }
    connected_ = false;
}

bool HTTPTransport::is_connected() const {
    return connected_ && curl_handle_;
}

std::string HTTPTransport::send_command(const std::string& command) {
    if (!connected_) {
        return "";
    }

    // For HTTP transport, commands are sent as POST requests
    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, command.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDSIZE, command.length());

    std::string response;
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION,
        [](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
            auto* str = static_cast<std::string*>(userp);
            str->append(static_cast<char*>(contents), size * nmemb);
            return size * nmemb;
        });
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl_handle_);
    if (res != CURLE_OK) {
        return "";
    }

    return response;
}

std::vector<uint8_t> HTTPTransport::read_data(size_t length) {
    if (!connected_) {
        return {};
    }

    std::vector<uint8_t> data;
    data.resize(length);

    size_t actual_size = 0;
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION,
        [](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
            auto* data_ptr = static_cast<std::vector<uint8_t>*>(userp);
            size_t total_size = size * nmemb;
            data_ptr->insert(data_ptr->end(),
                static_cast<uint8_t*>(contents),
                static_cast<uint8_t*>(contents) + total_size);
            return total_size;
        });
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &data);

    // This is a simplified implementation
    return data;
}

void HTTPTransport::write_data(const std::vector<uint8_t>& data) {
    if (!connected_) {
        return;
    }

    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, data.data());
    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDSIZE, data.size());

    curl_easy_perform(curl_handle_);
}

// SSH Transport implementation (simplified)
SSHTransport::SSHTransport() : ssh_session_(nullptr), connected_(false) {}

SSHTransport::~SSHTransport() {
    disconnect();
}

bool SSHTransport::connect(const std::string& url) {
    // Simplified SSH connection
    // In a real implementation, this would use libssh2 or similar
    connected_ = true;
    return true;
}

void SSHTransport::disconnect() {
    connected_ = false;
}

bool SSHTransport::is_connected() const {
    return connected_;
}

std::string SSHTransport::send_command(const std::string& command) {
    // Simplified command sending
    return "SSH command response";
}

std::vector<uint8_t> SSHTransport::read_data(size_t length) {
    return std::vector<uint8_t>(length, 0);
}

void SSHTransport::write_data(const std::vector<uint8_t>& data) {
    // Simplified data writing
}

// Git Protocol implementation
GitProtocol::GitProtocol(std::unique_ptr<Transport> transport)
    : transport_(std::move(transport)), connected_(false) {}

std::vector<std::string> GitProtocol::get_service_refs(const std::string& service) {
    if (!transport_->connect("git://example.com/repo.git")) {
        return {};
    }

    transport_->send_command("git-" + service + "\0host=example.com\0");

    std::vector<std::string> refs;
    std::string line = read_packet_line();
    while (!line.empty() && line != "0000") {
        refs.push_back(line);
        line = read_packet_line();
    }

    transport_->disconnect();
    return refs;
}

std::pair<std::string, std::vector<uint8_t>> GitProtocol::upload_pack(const PackRequest& request) {
    if (!transport_->connect("git://example.com/repo.git")) {
        return {};
    }

    // Send wants and haves
    for (const auto& want : request.wants) {
        write_packet_line("want " + want);
    }

    for (const auto& have : request.haves) {
        write_packet_line("have " + have);
    }

    write_packet_line("done");

    // Read response
    std::string response;
    std::vector<uint8_t> pack_data;

    std::string line = read_packet_line();
    while (!line.empty()) {
        response += line + "\n";
        if (line.substr(0, 4) == "PACK") {
            // Read packfile data
            pack_data = read_packet_data();
            break;
        }
        line = read_packet_line();
    }

    transport_->disconnect();
    return {response, pack_data};
}

std::string GitProtocol::receive_pack(const std::vector<PushRequest>& requests) {
    if (!transport_->connect("git://example.com/repo.git")) {
        return "";
    }

    std::string response;

    for (const auto& req : requests) {
        write_packet_line("old-sha1 " + req.old_commit_id);
        write_packet_line("new-sha1 " + req.new_commit_id);
        write_packet_line("ref-name " + req.dst_ref);

        // Send pack data
        write_packet_data(req.pack_data);
    }

    write_packet_line("");

    // Read response
    std::string line = read_packet_line();
    while (!line.empty()) {
        response += line + "\n";
        line = read_packet_line();
    }

    transport_->disconnect();
    return response;
}

std::string GitProtocol::read_packet_line() {
    // Simplified packet line reading
    return transport_->send_command("");
}

void GitProtocol::write_packet_line(const std::string& line) {
    transport_->send_command(line);
}

std::vector<uint8_t> GitProtocol::read_packet_data() {
    return transport_->read_data(1024); // Simplified
}

void GitProtocol::write_packet_data(const std::vector<uint8_t>& data) {
    transport_->write_data(data);
}

// Remote implementation
Remote::Remote(Repository& repo, const std::string& name)
    : repo_(repo), name_(name) {
    config_.url = "";
}

void Remote::set_url(const std::string& url) {
    config_.url = url;
}

std::string Remote::get_url() const {
    return config_.url;
}

void Remote::add_fetch_spec(const std::string& spec) {
    config_.fetch_specs.push_back(spec);
}

void Remote::add_push_spec(const std::string& spec) {
    config_.push_specs.push_back(spec);
}

bool Remote::fetch(const std::string& branch) {
    if (!connect()) {
        return false;
    }

    GitProtocol::PackRequest request;
    request.wants = {"refs/heads/" + branch};
    request.haves = {}; // Get all objects

    auto [response, pack_data] = protocol_->upload_pack(request);

    // Process the received packfile
    // In a real implementation, this would parse and store the objects

    disconnect();
    return true;
}

bool Remote::push(const std::string& branch, bool force) {
    if (!connect()) {
        return false;
    }

    // Get the current commit
    std::string head_id = repo_.refs().get_head();

    GitProtocol::PushRequest request;
    request.src_ref = "refs/heads/" + branch;
    request.dst_ref = "refs/heads/" + branch;
    request.old_commit_id = "0000000000000000000000000000000000000000";
    request.new_commit_id = head_id;

    // Create packfile with objects to push
    request.pack_data = network::create_packfile({head_id});

    auto response = protocol_->receive_pack({request});

    disconnect();
    return !response.empty();
}

std::vector<std::string> Remote::get_remote_refs() {
    if (!connect()) {
        return {};
    }

    auto refs = protocol_->get_service_refs("upload-pack");
    disconnect();
    return refs;
}

std::string Remote::resolve_remote_ref(const std::string& ref) {
    auto refs = get_remote_refs();
    for (const auto& r : refs) {
        if (r.find(ref) != std::string::npos) {
            // Extract SHA-1 from ref line
            auto pos = r.find(' ');
            if (pos != std::string::npos) {
                return r.substr(0, pos);
            }
        }
    }
    return "";
}

bool Remote::connect() {
    if (protocol_) {
        return true;
    }

    auto transport_type = detect_transport_type(config_.url);
    auto transport = create_transport(transport_type);

    if (!transport) {
        return false;
    }

    protocol_ = std::make_unique<GitProtocol>(std::move(transport));
    return true;
}

void Remote::disconnect() {
    protocol_.reset();
}

TransportType Remote::detect_transport_type(const std::string& url) {
    if (url.substr(0, 8) == "https://") return TransportType::HTTPS;
    if (url.substr(0, 7) == "http://") return TransportType::HTTP;
    if (url.substr(0, 4) == "git@") return TransportType::SSH;
    if (url.substr(0, 7) == "ssh://") return TransportType::SSH;
    if (url.substr(0, 7) == "git://") return TransportType::GitProtocol;
    return TransportType::Local;
}

std::unique_ptr<Transport> Remote::create_transport(TransportType type) {
    switch (type) {
        case TransportType::HTTP:
        case TransportType::HTTPS:
            return std::make_unique<HTTPTransport>();
        case TransportType::SSH:
        case TransportType::GitProtocol:
            return std::make_unique<SSHTransport>();
        default:
            return nullptr;
    }
}

// Network utilities
namespace network {

ParsedURL parse_url(const std::string& url) {
    ParsedURL parsed;

    std::regex url_regex(R"(^(\w+)://(?:([^:@]+)(?::([^@]+))?@)?([^:/]+)(?::(\d+))?(/.+)?$)");
    std::smatch match;

    if (std::regex_match(url, match, url_regex)) {
        parsed.scheme = match[1];
        parsed.user = match[2];
        parsed.password = match[3];
        parsed.host = match[4];
        parsed.port = match[5].matched ? std::stoi(match[5]) : 0;
        parsed.path = match[6];
    }

    return parsed;
}

std::string url_encode(const std::string& str) {
    // Simple URL encoding
    std::string encoded;
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else {
            char buf[4];
            sprintf(buf, "%%%02X", static_cast<unsigned char>(c));
            encoded += buf;
        }
    }
    return encoded;
}

std::string build_git_url(const std::string& base_url, const std::string& service) {
    return base_url + "/git-" + service;
}

std::string get_credentials(const std::string& url) {
    // This would typically read from .git/credentials or environment
    return "";
}

std::vector<uint8_t> create_packfile(const std::vector<std::string>& object_ids) {
    // Simplified packfile creation
    // In a real implementation, this would create a proper Git packfile
    return std::vector<uint8_t>(1024, 0); // Placeholder
}

bool verify_packfile(const std::vector<uint8_t>& pack_data) {
    // Simplified verification
    return !pack_data.empty();
}

} // namespace network

} // namespace dgit
