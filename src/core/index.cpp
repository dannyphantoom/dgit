#include "dgit/index.hpp"
#include "dgit/sha1.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <cstring>

namespace fs = std::filesystem;
namespace dgit {

Index::Index(const std::string& git_dir) : git_dir_(git_dir), index_file_(git_dir + "/index") {
    load();
}

void Index::add_entry(const std::string& path, const ObjectId& blob_id, FileMode mode) {
    // Remove existing entry if present
    auto it = path_index_.find(path);
    if (it != path_index_.end()) {
        entries_.erase(entries_.begin() + it->second);
    }

    // Add new entry
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        throw GitException("Cannot stat file: " + path);
    }

    IndexEntry entry(path, blob_id, mode, st.st_mtime, st.st_size);
    entries_.push_back(entry);
    update_path_index();
}

void Index::remove_entry(const std::string& path) {
    auto it = path_index_.find(path);
    if (it != path_index_.end()) {
        entries_.erase(entries_.begin() + it->second);
        update_path_index();
    }
}

bool Index::has_entry(const std::string& path) const {
    return path_index_.find(path) != path_index_.end();
}

IndexEntry Index::get_entry(const std::string& path) const {
    auto it = path_index_.find(path);
    if (it == path_index_.end()) {
        throw GitException("Entry not found: " + path);
    }
    return entries_[it->second];
}

void Index::add_file(const std::string& filepath) {
    // Create blob object for the file
    ObjectId blob_id = dgit::SHA1::hash_file(filepath);

    // Determine file mode
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        throw GitException("Cannot stat file: " + filepath);
    }

    FileMode mode = static_cast<FileMode>(st.st_mode & 0777);
    if (S_ISDIR(st.st_mode)) {
        mode = FileMode::Directory;
    } else if (st.st_mode & S_IXUSR) {
        mode = FileMode::Executable;
    } else {
        mode = FileMode::Regular;
    }

    add_entry(filepath, blob_id, mode);
}

void Index::remove_file(const std::string& filepath) {
    remove_entry(filepath);
}

std::vector<std::string> Index::list_files() const {
    std::vector<std::string> files;
    for (const auto& entry : entries_) {
        files.push_back(entry.path);
    }
    return files;
}

std::vector<std::string> Index::get_modified_files() const {
    std::vector<std::string> modified;

    for (const auto& entry : entries_) {
        if (is_file_modified(entry, entry.path)) {
            modified.push_back(entry.path);
        }
    }

    return modified;
}

std::vector<std::string> Index::get_staged_files() const {
    return list_files();
}

std::vector<std::string> Index::get_untracked_files() const {
    std::vector<std::string> untracked;
    std::vector<std::string> tracked = list_files();

    // Get all files in working directory
    for (const auto& entry : fs::recursive_directory_iterator(".")) {
        if (entry.is_regular_file()) {
            std::string path = fs::relative(entry.path()).string();

            // Skip files in .git directory
            if (path.find(".git/") == 0) {
                continue;
            }

            // Check if file is tracked
            bool is_tracked = false;
            for (const auto& tracked_file : tracked) {
                if (tracked_file == path) {
                    is_tracked = true;
                    break;
                }
            }

            if (!is_tracked) {
                untracked.push_back(path);
            }
        }
    }

    return untracked;
}

void Index::load() {
    if (!fs::exists(index_file_)) {
        return;
    }

    std::ifstream file(index_file_, std::ios::binary);
    if (!file) {
        throw GitException("Cannot read index file: " + index_file_);
    }

    // Read header
    char header[12];
    file.read(header, 12);

    if (std::string(header, 4) != "DIRC") {
        throw GitException("Invalid index file header");
    }

    // Read version and entry count
    uint32_t version, entry_count;
    std::memcpy(&version, header + 4, 4);
    std::memcpy(&entry_count, header + 8, 4);

    entries_.clear();
    for (uint32_t i = 0; i < entry_count; ++i) {
        // Read entry data (simplified - real Git index is more complex)
        IndexEntry entry("", "", FileMode::Regular, 0, 0);

        // Read path length
        uint16_t path_len;
        file.read(reinterpret_cast<char*>(&path_len), 2);

        // Read path
        std::string path(path_len, '\0');
        file.read(&path[0], path_len);

        // Skip null terminator
        file.read(reinterpret_cast<char*>(&path_len), 1);

        // Read other fields (simplified)
        char id_bytes[20];
        file.read(id_bytes, 20);

        // Convert to hex string
        std::string id_str;
        for (int j = 0; j < 20; ++j) {
            char buf[3];
            sprintf(buf, "%02x", static_cast<unsigned char>(id_bytes[j]));
            id_str += buf;
        }

        entry.path = path;
        entry.blob_id = id_str;

        entries_.push_back(entry);
    }

    update_path_index();
}

void Index::save() {
    std::ofstream file(index_file_, std::ios::binary);
    if (!file) {
        throw GitException("Cannot write index file: " + index_file_);
    }

    // Write header
    std::string header = "DIRC";
    uint32_t version = 2;
    uint32_t entry_count = entries_.size();

    file.write(header.c_str(), 4);
    file.write(reinterpret_cast<char*>(&version), 4);
    file.write(reinterpret_cast<char*>(&entry_count), 4);

    // Write entries (simplified)
    for (const auto& entry : entries_) {
        // Write path
        uint16_t path_len = entry.path.length();
        file.write(reinterpret_cast<char*>(&path_len), 2);
        file.write(entry.path.c_str(), path_len);
        file.write("\0", 1);

        // Write SHA-1
        std::string id_hex = entry.blob_id;
        for (size_t i = 0; i < 20; ++i) {
            uint8_t byte = std::stoi(id_hex.substr(i * 2, 2), nullptr, 16);
            file.write(reinterpret_cast<char*>(&byte), 1);
        }
    }
}

void Index::clear() {
    entries_.clear();
    path_index_.clear();
}

void Index::update_path_index() {
    path_index_.clear();
    for (size_t i = 0; i < entries_.size(); ++i) {
        path_index_[entries_[i].path] = i;
    }
}

bool Index::is_file_modified(const IndexEntry& entry, const std::string& filepath) const {
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        return true; // File doesn't exist
    }

    return st.st_mtime != entry.mtime || st.st_size != entry.size;
}

// Utility functions for index format
std::string Index::serialize_entry(const IndexEntry& entry) const {
    // Simplified serialization
    return entry.path + "\0" + entry.blob_id;
}

IndexEntry Index::deserialize_entry(const std::string& data) {
    size_t null_pos = data.find('\0');
    if (null_pos == std::string::npos) {
        throw GitException("Invalid index entry format");
    }

    std::string path = data.substr(0, null_pos);
    std::string blob_id = data.substr(null_pos + 1);

    return IndexEntry(path, blob_id, FileMode::Regular, 0, 0);
}

} // namespace dgit
