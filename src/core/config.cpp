#include "dgit/config.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

#include <sys/stat.h>

// Using C-style file operations for compatibility
namespace dgit {

Config::Config() : config_file_(".git/config") {
    load();
}

Config::Config(const std::string& git_dir) : git_dir_(git_dir), config_file_(git_dir + "/config") {
    load();
}

void Config::set_value(const std::string& section, const std::string& key, const std::string& value) {
    std::string normalized_key = normalize_key(section, key);
    config_[normalized_key] = value;
}

std::optional<std::string> Config::get_value(const std::string& section, const std::string& key) const {
    std::string normalized_key = normalize_key(section, key);
    auto it = config_.find(normalized_key);
    if (it != config_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Config::unset_value(const std::string& section, const std::string& key) {
    std::string normalized_key = normalize_key(section, key);
    config_.erase(normalized_key);
}

std::string Config::get_string(const std::string& section, const std::string& key, const std::string& default_value) const {
    auto value = get_value(section, key);
    return value.value_or(default_value);
}

bool Config::get_bool(const std::string& section, const std::string& key, bool default_value) const {
    auto value = get_value(section, key);
    if (!value) return default_value;

    std::string lower = *value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    return lower == "true" || lower == "yes" || lower == "on" || lower == "1";
}

int Config::get_int(const std::string& section, const std::string& key, int default_value) const {
    auto value = get_value(section, key);
    if (!value) return default_value;

    try {
        return std::stoi(*value);
    } catch (const std::invalid_argument&) {
        return default_value;
    }
}

std::vector<std::string> Config::get_sections() const {
    std::vector<std::string> sections;
    std::string current_section;

    for (const auto& entry : config_) {
        size_t dot_pos = entry.first.find('.');
        if (dot_pos != std::string::npos) {
            std::string section = entry.first.substr(0, dot_pos);
            if (section != current_section) {
                sections.push_back(section);
                current_section = section;
            }
        }
    }

    return sections;
}

std::vector<std::pair<std::string, std::string>> Config::get_entries(const std::string& section) const {
    std::vector<std::pair<std::string, std::string>> entries;

    for (const auto& entry : config_) {
        size_t dot_pos = entry.first.find('.');
        if (dot_pos != std::string::npos) {
            std::string entry_section = entry.first.substr(0, dot_pos);
            if (entry_section == section) {
                std::string key = entry.first.substr(dot_pos + 1);
                entries.emplace_back(key, entry.second);
            }
        }
    }

    return entries;
}

void Config::load() {
    struct stat buffer;
    if (stat(config_file_.c_str(), &buffer) != 0) {
        return;
    }

    std::ifstream file(config_file_);
    if (!file) {
        return;
    }

    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        // Remove comments
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        // Trim whitespace
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](int ch) {
            return !std::isspace(ch);
        }));
        line.erase(std::find_if(line.rbegin(), line.rend(), [](int ch) {
            return !std::isspace(ch);
        }).base(), line.end());

        if (line.empty()) {
            continue;
        }

        // Parse section headers
        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }

        // Parse key-value pairs
        size_t equals_pos = line.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = line.substr(0, equals_pos);
            std::string value = line.substr(equals_pos + 1);

            // Trim whitespace from key and value
            key.erase(key.begin(), std::find_if(key.begin(), key.end(), [](int ch) {
                return !std::isspace(ch);
            }));
            key.erase(std::find_if(key.rbegin(), key.rend(), [](int ch) {
                return !std::isspace(ch);
            }).base(), key.end());

            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](int ch) {
                return !std::isspace(ch);
            }));
            value.erase(std::find_if(value.rbegin(), value.rend(), [](int ch) {
                return !std::isspace(ch);
            }).base(), value.end());

            if (!current_section.empty() && !key.empty()) {
                set_value(current_section, key, value);
            }
        }
    }
}

void Config::save() {
    std::ofstream file(config_file_, std::ios::trunc);
    if (!file) {
        throw GitException("Cannot write config file: " + config_file_);
    }

    std::string current_section;
    for (const auto& entry : config_) {
        size_t dot_pos = entry.first.find('.');
        if (dot_pos != std::string::npos) {
            std::string section = entry.first.substr(0, dot_pos);
            std::string key = entry.first.substr(dot_pos + 1);

            if (section != current_section) {
                if (!current_section.empty()) {
                    file << "\n";
                }
                file << "[" << section << "]\n";
                current_section = section;
            }

            file << "\t" << key << " = " << entry.second << "\n";
        }
    }
}

std::string Config::normalize_key(const std::string& section, const std::string& key) const {
    std::string normalized_section = section;
    std::string normalized_key = key;

    // Normalize case
    std::transform(normalized_section.begin(), normalized_section.end(),
                   normalized_section.begin(), ::tolower);
    std::transform(normalized_key.begin(), normalized_key.end(),
                   normalized_key.begin(), ::tolower);

    return normalized_section + "." + normalized_key;
}

// Global config instances
Config& Config::global() {
    static Config global_config;
    return global_config;
}

Config& Config::system() {
    static Config system_config("/etc/gitconfig");
    return system_config;
}

} // namespace dgit
