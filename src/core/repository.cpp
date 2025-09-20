#include "dgit/repository.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
namespace dgit {

std::unique_ptr<Repository> Repository::create(const std::string& path) {
    auto repo = std::unique_ptr<Repository>(new Repository(path, path + "/.git"));
    repo->init();
    return repo;
}

std::unique_ptr<Repository> Repository::open(const std::string& path) {
    std::string git_dir = path + "/.git";
    if (!fs::exists(git_dir)) {
        throw GitException("Not a git repository: " + path);
    }

    return std::unique_ptr<Repository>(new Repository(path, git_dir));
}

bool Repository::exists(const std::string& path) {
    return fs::exists(path + "/.git");
}

Repository::Repository(const std::string& path, const std::string& git_dir)
    : path_(path), git_dir_(git_dir) {

    // Initialize components
    objects_ = std::make_unique<ObjectDatabase>(git_dir_);
    refs_ = std::make_unique<Refs>(git_dir_);
    config_ = std::make_unique<Config>(git_dir_);
    index_ = std::make_unique<Index>(git_dir_);
}

void Repository::init() {
    // Create directory structure
    fs::create_directories(git_dir_);
    fs::create_directories(git_dir_ + "/objects");
    fs::create_directories(git_dir_ + "/refs/heads");
    fs::create_directories(git_dir_ + "/refs/tags");

    // Create HEAD file
    std::string head_path = git_dir_ + "/HEAD";
    std::ofstream head_file(head_path);
    if (!head_file) {
        throw GitException("Cannot create HEAD file");
    }
    head_file << "ref: refs/heads/master\n";

    // Create initial config
    config_->set_value("core", "repositoryformatversion", "0");
    config_->set_value("core", "filemode", "false");
    config_->set_value("core", "bare", "false");
    config_->save();

    // Create initial branch
    refs_->create_ref("refs/heads/master", "");

    std::cout << "Initialized empty Git repository in " << git_dir_ << "\n";
}

void Repository::commit(const std::string& message, const Person& author, const Person& committer) {
    // Check if there are staged changes
    if (index_->entry_count() == 0) {
        throw GitException("Nothing to commit");
    }

    // Get current HEAD
    ObjectId head_id;
    try {
        head_id = refs_->get_head();
    } catch (const GitException&) {
        // No commits yet
    }

    // Create tree object
    ObjectId tree_id = write_tree();

    // Create parent list
    std::vector<std::string> parents;
    if (!head_id.empty()) {
        parents.push_back(head_id);
    }

    // Create commit object
    auto commit = std::make_unique<Commit>(tree_id, parents, author, committer, message);
    ObjectId commit_id = commit->id();
    objects_->store(std::move(commit));

    // Update HEAD
    refs_->update_ref("refs/heads/master", commit_id);

    // Clear index
    index_->clear();
    index_->save();

    std::cout << "Created commit " << commit_id.substr(0, 7) << "\n";
}

ObjectId Repository::write_blob(const std::string& filepath) {
    // Create blob from file
    auto blob = std::make_unique<Blob>("");
    std::ifstream file(filepath, std::ios::binary);

    if (!file) {
        throw GitException("Cannot read file: " + filepath);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    blob = std::make_unique<Blob>(content);
    ObjectId blob_id = blob->id();
    objects_->store(std::move(blob));

    return blob_id;
}

ObjectId Repository::write_tree(const std::string& directory) {
    return write_tree_recursive(directory);
}

ObjectId Repository::write_tree_recursive(const std::string& directory, const std::string& base_path) {
    auto tree = std::make_unique<Tree>();

    for (const auto& entry : fs::directory_iterator(directory)) {
        std::string name = entry.path().filename().string();
        std::string full_path = entry.path().string();

        // Skip .git directory
        if (name == ".git") {
            continue;
        }

        if (entry.is_regular_file()) {
            // Check if file is in index
            std::string relative_path = base_path.empty() ? name : base_path + "/" + name;
            if (index_->has_entry(relative_path)) {
                IndexEntry index_entry = index_->get_entry(relative_path);
                tree->add_entry(FileMode::Regular, index_entry.blob_id, name);
            } else {
                // Create blob for new file
                ObjectId blob_id = write_blob(full_path);
                tree->add_entry(FileMode::Regular, blob_id, name);
            }
        } else if (entry.is_directory()) {
            // Recursively create subtree
            ObjectId subtree_id = write_tree_recursive(full_path, base_path.empty() ? name : base_path + "/" + name);
            tree->add_entry(FileMode::Directory, subtree_id, name);
        }
    }

    ObjectId tree_id = tree->id();
    objects_->store(std::move(tree));
    return tree_id;
}

std::string Repository::read_file(const ObjectId& blob_id, const std::string& filepath) {
    auto blob = objects_->load(blob_id);
    if (blob->type() != ObjectType::Blob) {
        throw GitException("Object is not a blob: " + blob_id);
    }

    // Write content to file if path provided
    if (!filepath.empty()) {
        std::ofstream file(filepath, std::ios::binary);
        if (!file) {
            throw GitException("Cannot write file: " + filepath);
        }
        file << blob->data();
    }

    return blob->data();
}

} // namespace dgit
