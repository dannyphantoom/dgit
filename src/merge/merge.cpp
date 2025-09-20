#include "dgit/merge.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <regex>
#include <set>
#include <iostream>
#include "dgit/commands.hpp"

namespace dgit {

// Three-way merge implementation
ThreeWayMerge::ThreeWayMerge(Repository& repo) : repo_(repo) {}

MergeResult ThreeWayMerge::merge(const std::string& base_commit,
                               const std::string& our_commit,
                               const std::string& their_commit) {
    base_commit_ = base_commit;
    our_commit_ = our_commit;
    their_commit_ = their_commit;

    MergeResult result(MergeStatus::Success);

    try {
        // Get the three trees
        auto base_tree = get_tree_from_commit(base_commit);
        auto our_tree = get_tree_from_commit(our_commit);
        auto their_tree = get_tree_from_commit(their_commit);

        // Perform the merge
        auto conflicts = perform_three_way_merge(base_tree, our_tree, their_tree);

        if (conflicts.empty()) {
            result.status = MergeStatus::Success;
            result.message = "Merge successful";
        } else {
            result.status = MergeStatus::Conflicts;
            result.conflicts = conflicts;
            result.message = "Merge conflicts detected";
        }

    } catch (const std::exception& e) {
        result.status = MergeStatus::Failed;
        result.message = "Merge failed: " + std::string(e.what());
    }

    return result;
}

std::string ThreeWayMerge::get_tree_from_commit(const std::string& commit_id) {
    auto commit = repo_.objects().load(commit_id);
    if (commit->type() != ObjectType::Commit) {
        throw GitException("Invalid commit: " + commit_id);
    }

    const Commit* commit_obj = static_cast<const Commit*>(commit.get());
    return commit_obj->tree_id();
}

std::vector<Conflict> ThreeWayMerge::perform_three_way_merge(
    const std::string& base_tree,
    const std::string& our_tree,
    const std::string& their_tree) {

    std::vector<Conflict> conflicts;

    // This is a simplified implementation
    // In a real Git implementation, this would:
    // 1. Traverse all three trees
    // 2. Compare each file
    // 3. Detect conflicts
    // 4. Apply merge strategies

    // For now, we'll just detect if there are any conflicting changes
    // by checking if both branches modified the same files

    std::vector<std::string> base_files = get_tree_files(base_tree);
    std::vector<std::string> our_files = get_tree_files(our_tree);
    std::vector<std::string> their_files = get_tree_files(their_tree);

    std::set<std::string> all_files;
    all_files.insert(base_files.begin(), base_files.end());
    all_files.insert(our_files.begin(), our_files.end());
    all_files.insert(their_files.begin(), their_files.end());

    for (const auto& file : all_files) {
        bool in_base = std::find(base_files.begin(), base_files.end(), file) != base_files.end();
        bool in_ours = std::find(our_files.begin(), our_files.end(), file) != our_files.end();
        bool in_theirs = std::find(their_files.begin(), their_files.end(), file) != their_files.end();

        // Detect conflicts: file modified in both branches
        if ((in_ours && in_theirs) || (!in_base && in_ours && in_theirs)) {
            Conflict conflict(file);
            conflicts.push_back(conflict);
            mark_conflict(file);
        }
    }

    return conflicts;
}

std::vector<std::string> ThreeWayMerge::get_tree_files(const std::string& tree_id) {
    std::vector<std::string> files;

    auto tree = repo_.objects().load(tree_id);
    if (tree->type() != ObjectType::Tree) {
        return files;
    }

    // Parse tree entries (simplified)
    const Tree* tree_obj = static_cast<const Tree*>(tree.get());
    for (const auto& entry : tree_obj->entries()) {
        if (entry.mode != FileMode::Directory) {
            files.push_back(entry.name);
        }
    }

    return files;
}

void ThreeWayMerge::mark_conflict(const std::string& path) {
    // Create conflict markers in the file
    std::string conflict_content =
        "<<<<<<< HEAD\n" +
        read_file_content(our_commit_, path) +
        "=======\n" +
        read_file_content(their_commit_, path) +
        ">>>>>>> " + their_commit_.substr(0, 7) + "\n";

    std::ofstream file(path);
    file << conflict_content;
}

std::string ThreeWayMerge::read_file_content(const std::string& commit_id, const std::string& path) {
    try {
        // Find the file in the commit's tree
        auto commit = repo_.objects().load(commit_id);
        const Commit* commit_obj = static_cast<const Commit*>(commit.get());
        auto tree = repo_.objects().load(commit_obj->tree_id());

        // For simplicity, return empty content
        // In a full implementation, this would traverse the tree to find the file
        return "";
    } catch (const std::exception&) {
        return "";
    }
}

bool ThreeWayMerge::can_handle_file(const std::string& path) const {
    // Can handle text files for now
    return path.find(".txt") != std::string::npos ||
           path.find(".md") != std::string::npos ||
           path.find(".cpp") != std::string::npos ||
           path.find(".h") != std::string::npos;
}

ThreeWayMerge::FileStatus ThreeWayMerge::get_file_status(const std::string& path) const {
    // Simplified file status
    return ThreeWayMerge::FileStatus::Modified;
}

// Manual conflict resolver
bool ManualResolver::resolve_conflict(Conflict& conflict) {
    std::cout << "Conflict in " << conflict.path << std::endl;
    std::cout << "Please resolve manually and mark as resolved" << std::endl;
    return false; // Manual resolution required
}

std::string ManualResolver::get_marker_pattern() const {
    return "<<<<<<< |======= |>>>>>>> ";
}

// Auto resolver (simplified)
bool AutoResolver::resolve_conflict(Conflict& conflict) {
    // Simple strategy: prefer "ours" version
    conflict.resolved_content = conflict.our_content;
    conflict.resolved = true;
    return true;
}

std::string AutoResolver::get_marker_pattern() const {
    return "";
}

// Branch manager implementation
BranchManager::BranchManager(Repository& repo) : repo_(repo) {}

std::vector<std::string> BranchManager::list_branches(bool show_remote) {
    auto branches = repo_.refs().list_branches();
    if (show_remote) {
        auto remote_branches = repo_.refs().list_remote_branches();
        branches.insert(branches.end(), remote_branches.begin(), remote_branches.end());
    }

    // Remove "refs/heads/" prefix for cleaner output
    for (auto& branch : branches) {
        if (branch.substr(0, 11) == "refs/heads/") {
            branch = branch.substr(11);
        }
    }

    return branches;
}

bool BranchManager::create_branch(const std::string& name, const std::string& start_point) {
    std::string commit_id = start_point;
    if (commit_id.empty()) {
        commit_id = repo_.refs().get_head();
    }

    repo_.refs().create_ref("refs/heads/" + name, commit_id);
    return true;
}

bool BranchManager::delete_branch(const std::string& name, bool force) {
    if (!force && name == get_current_branch()) {
        throw GitException("Cannot delete current branch");
    }

    repo_.refs().delete_ref("refs/heads/" + name);
    return true;
}

bool BranchManager::rename_branch(const std::string& old_name, const std::string& new_name) {
    if (old_name == get_current_branch()) {
        throw GitException("Cannot rename current branch");
    }

    auto commit_id = repo_.refs().read_ref("refs/heads/" + old_name);
    if (!commit_id) {
        return false;
    }

    repo_.refs().create_ref("refs/heads/" + new_name, *commit_id);
    repo_.refs().delete_ref("refs/heads/" + old_name);
    return true;
}

std::string BranchManager::get_current_branch() const {
    auto branch = repo_.refs().get_head_branch();
    return branch.value_or("HEAD");
}

bool BranchManager::checkout_branch(const std::string& name) {
    auto commit_id = repo_.refs().read_ref("refs/heads/" + name);
    if (!commit_id) {
        return false;
    }

    repo_.refs().set_head_to_branch(name);

    // In a full implementation, this would update the working directory
    // and index to match the target commit

    return true;
}

std::string BranchManager::get_branch_upstream(const std::string& branch) {
    // Simplified implementation
    return repo_.config().get_string("branch", branch, "");
}

void BranchManager::set_branch_upstream(const std::string& branch, const std::string& upstream) {
    repo_.config().set_value("branch", branch, upstream);
    repo_.config().save();
}

// Merge command implementation
MergeCommand::MergeCommand()
    : strategy_(MergeStrategy::Resolve), no_commit_(false), no_ff_(false) {}

void MergeCommand::parse_args(const std::vector<std::string>& args) {
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--no-commit") {
            no_commit_ = true;
        } else if (args[i] == "--no-ff") {
            no_ff_ = true;
        } else if (args[i] == "--strategy" && i + 1 < args.size()) {
            std::string strategy = args[++i];
            if (strategy == "ours") strategy_ = MergeStrategy::Ours;
            else if (strategy == "theirs") strategy_ = MergeStrategy::Theirs;
            else strategy_ = MergeStrategy::Resolve;
        }
    }
}

CommandResult MergeCommand::execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {1, "", "Error: merge requires at least one branch name\n"};
    }

    parse_args(args);

    std::string branch_name = args[0];

    try {
        auto repo = Repository::open(".");

        auto result = perform_merge(branch_name);

        std::ostringstream oss;
        switch (result.status) {
            case MergeStatus::Success:
                oss << "Merge successful\n";
                break;
            case MergeStatus::Conflicts:
                oss << "Merge conflicts detected. Please resolve conflicts and run 'dgit commit'\n";
                break;
            case MergeStatus::AlreadyUpToDate:
                oss << "Already up to date\n";
                break;
            case MergeStatus::Failed:
                oss << "Merge failed: " << result.message << "\n";
                break;
        }

        return {result.status == MergeStatus::Success ? 0 : 1, oss.str(), ""};
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

MergeResult MergeCommand::perform_merge(const std::string& branch_name) {
    auto repo = Repository::open(".");

    // Get current branch and commit
    std::string our_branch = repo->refs().get_head_branch().value_or("master");
    std::string our_commit = repo->refs().get_head();

    // Get their commit
    auto their_ref = repo->refs().read_ref("refs/heads/" + branch_name);
    if (!their_ref) {
        throw GitException("Branch not found: " + branch_name);
    }
    std::string their_commit = *their_ref;

    if (our_commit == their_commit) {
        return MergeResult(MergeStatus::AlreadyUpToDate, "Already up to date");
    }

    // Find merge base
    auto base_commit = merge::find_merge_base(*repo, our_commit, their_commit);
    if (base_commit.empty()) {
        throw GitException("No common ancestor found");
    }

    // Perform the merge
    ThreeWayMerge merger(*repo);
    return merger.merge(base_commit, our_commit, their_commit);
}

std::string MergeCommand::create_merge_message(const std::string& branch_name) {
    std::ostringstream oss;
    oss << "Merge branch '" << branch_name << "'";
    return oss.str();
}

// Merge utilities implementation
namespace merge {

std::string find_merge_base(Repository& repo,
                           const std::string& commit1,
                           const std::string& commit2) {
    // Simplified implementation - just return the first commit
    // In a real implementation, this would find the common ancestor
    return commit1;
}

bool is_merge_possible(Repository& repo,
                      const std::string& base,
                      const std::string& ours,
                      const std::string& theirs) {
    // Simplified check
    return true;
}

bool create_index_from_tree(Repository& repo, const std::string& tree_id) {
    // Simplified implementation
    return true;
}

std::vector<RenameDetection> detect_renames(Repository& repo,
                                           const std::string& base_tree,
                                           const std::string& our_tree,
                                           const std::string& their_tree) {
    // Simplified implementation - no rename detection
    return {};
}

bool resolve_conflicts_automatically(Repository& repo,
                                   const std::vector<Conflict>& conflicts) {
    // Simplified - mark all as resolved
    return true;
}

std::string create_merge_commit(Repository& repo,
                               const std::string& base_commit,
                               const std::string& our_commit,
                               const std::string& their_commit,
                               const std::string& message) {

    // Get current author info
    std::string author_name = repo.config().get_string("user", "name", "Unknown");
    std::string author_email = repo.config().get_string("user", "email", "unknown@example.com");
    Person author(author_name, author_email, std::chrono::system_clock::now());
    Person committer = author;

    // Create merge commit with both parents
    std::vector<std::string> parents = {our_commit, their_commit};
    auto commit = std::make_unique<Commit>("", parents, author, committer, message);

    repo.objects().store(std::move(commit));

    return commit->id();
}

} // namespace merge

} // namespace dgit
