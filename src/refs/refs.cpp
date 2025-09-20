#include "dgit/refs.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>

namespace fs = std::filesystem;
namespace dgit {

Refs::Refs(const std::string& git_dir)
    : git_dir_(git_dir), refs_dir_(git_dir + "/refs"),
      heads_dir_(refs_dir_ + "/heads"), tags_dir_(refs_dir_ + "/tags"),
      remotes_dir_(refs_dir_ + "/remotes") {

    // Create refs directory structure
    fs::create_directories(heads_dir_);
    fs::create_directories(tags_dir_);
    fs::create_directories(remotes_dir_);

    // Load reference cache
    load_ref_cache();
}

void Refs::create_ref(const RefName& name, const ObjectId& target, bool symbolic) {
    std::string path = get_ref_path(name);

    if (symbolic) {
        // Create symbolic ref
        std::string target_path = get_ref_path(target);
        if (!fs::exists(target_path)) {
            throw GitException("Symbolic ref target does not exist: " + target);
        }

        std::ofstream file(path);
        if (!file) {
            throw GitException("Cannot create ref: " + path);
        }

        file << "ref: " << target << "\n";
    } else {
        // Create direct ref
        write_ref_file(path, target);
    }

    // Update cache
    ref_cache_[name] = target;

    // Log the change
    log_ref_change(name, "", target);
}

void Refs::update_ref(const RefName& name, const ObjectId& target) {
    std::string path = get_ref_path(name);

    if (!fs::exists(path)) {
        throw GitException("Ref does not exist: " + name);
    }

    ObjectId old_target = resolve_ref(name);
    write_ref_file(path, target);

    // Update cache
    ref_cache_[name] = target;

    // Log the change
    log_ref_change(name, old_target, target);
}

void Refs::delete_ref(const RefName& name) {
    std::string path = get_ref_path(name);

    if (!fs::exists(path)) {
        throw GitException("Ref does not exist: " + name);
    }

    ObjectId old_target = resolve_ref(name);
    fs::remove(path);

    // Remove from cache
    ref_cache_.erase(name);

    // Log the change
    log_ref_change(name, old_target, "");
}

std::optional<ObjectId> Refs::read_ref(const RefName& name) {
    auto it = ref_cache_.find(name);
    if (it != ref_cache_.end()) {
        return it->second;
    }

    std::string path = get_ref_path(name);
    if (!fs::exists(path)) {
        return std::nullopt;
    }

    return read_ref_file(path);
}

bool Refs::ref_exists(const RefName& name) {
    std::string path = get_ref_path(name);
    return fs::exists(path);
}

ObjectId Refs::get_head() {
    std::string head_path = git_dir_ + "/HEAD";
    std::ifstream file(head_path);

    if (!file) {
        throw GitException("HEAD file not found. Run 'dgit init' first.");
    }

    std::string line;
    std::getline(file, line);

    if (line.substr(0, 5) == "ref: ") {
        RefName branch = line.substr(5);
        return resolve_ref(branch);
    } else {
        return line; // Detached HEAD
    }
}

void Refs::set_head(const ObjectId& commit_id) {
    std::string head_path = git_dir_ + "/HEAD";
    std::ofstream file(head_path);

    if (!file) {
        throw GitException("Cannot write to HEAD file");
    }

    file << commit_id << "\n";
    ref_cache_["HEAD"] = commit_id;
}

void Refs::set_head_to_branch(const RefName& branch_name) {
    std::string head_path = git_dir_ + "/HEAD";
    std::ofstream file(head_path);

    if (!file) {
        throw GitException("Cannot write to HEAD file");
    }

    file << "ref: refs/heads/" << branch_name << "\n";
    ref_cache_["HEAD"] = resolve_ref("refs/heads/" + branch_name);
}

std::optional<RefName> Refs::get_head_branch() {
    std::string head_path = git_dir_ + "/HEAD";
    std::ifstream file(head_path);

    if (!file) {
        return std::nullopt;
    }

    std::string line;
    std::getline(file, line);

    if (line.substr(0, 5) == "ref: ") {
        RefName branch = line.substr(5);
        if (branch.substr(0, 11) == "refs/heads/") {
            return branch.substr(11);
        }
    }

    return std::nullopt;
}

std::vector<RefName> Refs::list_branches() {
    std::vector<RefName> branches;

    if (fs::exists(heads_dir_)) {
        for (const auto& entry : fs::directory_iterator(heads_dir_)) {
            if (entry.is_regular_file()) {
                branches.push_back("refs/heads/" + entry.path().filename().string());
            }
        }
    }

    return branches;
}

std::vector<RefName> Refs::list_remote_branches() {
    std::vector<RefName> branches;

    if (fs::exists(remotes_dir_)) {
        for (const auto& remote_dir : fs::directory_iterator(remotes_dir_)) {
            if (remote_dir.is_directory()) {
                std::string remote_name = remote_dir.path().filename().string();
                for (const auto& branch_file : fs::directory_iterator(remote_dir)) {
                    if (branch_file.is_regular_file()) {
                        std::string branch_name = branch_file.path().filename().string();
                        branches.push_back("refs/remotes/" + remote_name + "/" + branch_name);
                    }
                }
            }
        }
    }

    return branches;
}

std::vector<RefName> Refs::list_tags() {
    std::vector<RefName> tags;

    if (fs::exists(tags_dir_)) {
        for (const auto& entry : fs::directory_iterator(tags_dir_)) {
            if (entry.is_regular_file()) {
                tags.push_back("refs/tags/" + entry.path().filename().string());
            }
        }
    }

    return tags;
}

void Refs::create_symbolic_ref(const RefName& name, const RefName& target) {
    create_ref(name, target, true);
}

std::optional<RefName> Refs::read_symbolic_ref(const RefName& name) {
    std::string path = get_ref_path(name);

    if (!fs::exists(path)) {
        return std::nullopt;
    }

    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    std::string line;
    std::getline(file, line);

    if (line.substr(0, 5) == "ref: ") {
        return line.substr(5);
    }

    return std::nullopt;
}

std::string Refs::resolve_ref(const RefName& name) {
    auto cached = ref_cache_.find(name);
    if (cached != ref_cache_.end()) {
        return cached->second;
    }

    std::string path = get_ref_path(name);
    if (!fs::exists(path)) {
        throw GitException("Ref not found: " + name);
    }

    auto target = read_ref_file(path);
    if (!target) {
        throw GitException("Cannot resolve ref: " + name);
    }

    ref_cache_[name] = *target;
    return *target;
}

std::string Refs::get_ref_path(const RefName& name) const {
    // Handle special refs
    if (name == "HEAD") {
        return git_dir_ + "/HEAD";
    }

    // Handle full ref paths
    if (name.substr(0, 5) == "refs/") {
        return git_dir_ + "/" + name;
    }

    // Handle shorthand refs
    if (name.find('/') == std::string::npos) {
        return heads_dir_ + "/" + name;
    }

    throw GitException("Invalid ref name: " + name);
}

void Refs::write_ref_file(const std::string& path, const ObjectId& target) {
    std::ofstream file(path);
    if (!file) {
        throw GitException("Cannot write ref file: " + path);
    }

    file << target << "\n";
}

std::optional<ObjectId> Refs::read_ref_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    std::string line;
    std::getline(file, line);

    // Handle symbolic refs
    if (line.substr(0, 5) == "ref: ") {
        RefName target = line.substr(5);
        return read_ref_file(get_ref_path(target));
    }

    // Validate SHA-1
    if (line.length() == 40 && std::regex_match(line, std::regex("^[0-9a-fA-F]+$"))) {
        return line;
    }

    return std::nullopt;
}

void Refs::load_ref_cache() {
    // Load HEAD
    try {
        ObjectId head_target = get_head();
        ref_cache_["HEAD"] = head_target;
    } catch (...) {
        // HEAD might not exist yet
    }

    // Load branches
    for (const auto& branch : list_branches()) {
        try {
            ObjectId target = resolve_ref(branch);
            ref_cache_[branch] = target;
        } catch (...) {
            // Skip invalid refs
        }
    }

    // Load tags
    for (const auto& tag : list_tags()) {
        try {
            ObjectId target = resolve_ref(tag);
            ref_cache_[tag] = target;
        } catch (...) {
            // Skip invalid refs
        }
    }
}

void Refs::log_ref_change(const RefName& name, const ObjectId& old_id, const ObjectId& new_id) {
    std::string reflog_dir = git_dir_ + "/logs";
    fs::create_directories(reflog_dir);

    std::string reflog_path = reflog_dir + "/" + name;

    std::ofstream file(reflog_path, std::ios::app);
    if (!file) {
        return; // Silently fail reflog writes
    }

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    file << new_id << " " << old_id << " " << "user"
         << " <user@example.com> " << timestamp << " +0000"
         << "\tref update\n";
}

} // namespace dgit
