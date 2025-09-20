#include "dgit/commands.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>

namespace dgit {

// InitCommand implementation
CommandResult InitCommand::execute(const std::vector<std::string>& args) {
    std::string path = args.empty() ? "." : args[0];

    try {
        // For now, just create the directory structure manually
        std::string git_dir = path + "/.git";
        std::filesystem::create_directories(git_dir);
        std::filesystem::create_directories(git_dir + "/objects");
        std::filesystem::create_directories(git_dir + "/refs/heads");
        std::filesystem::create_directories(git_dir + "/refs/tags");

        // Create HEAD file
        std::ofstream head_file(git_dir + "/HEAD");
        if (head_file) {
            head_file << "ref: refs/heads/master\n";
        }

        // Create basic config
        std::ofstream config_file(git_dir + "/config");
        if (config_file) {
            config_file << "[core]\n";
            config_file << "\trepositoryformatversion = 0\n";
            config_file << "\tfilemode = false\n";
            config_file << "\tbare = false\n";
        }

        return {0, "Initialized empty Git repository in " + git_dir + "\n", ""};
    } catch (const std::exception& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// AddCommand implementation
CommandResult AddCommand::execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {1, "", "Error: 'add' requires at least one file\n"};
    }

    try {
        auto repo = Repository::open(".");

        for (const auto& filepath : args) {
            repo->index().add_file(filepath);
        }

        repo->index().save();

        std::ostringstream oss;
        oss << "Added " << args.size() << " file(s) to staging area\n";
        return {0, oss.str(), ""};
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// CommitCommand implementation
CommandResult CommitCommand::execute(const std::vector<std::string>& args) {
    std::string message = "-m";
    bool has_message = false;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-m" && i + 1 < args.size()) {
            message = args[++i];
            has_message = true;
            break;
        }
    }

    if (!has_message) {
        return {1, "", "Error: commit message required (use -m)\n"};
    }

    try {
        auto repo = Repository::open(".");

        // Get author information
        std::string author_name = repo->config().get_string("user", "name", "Unknown");
        std::string author_email = repo->config().get_string("user", "email", "unknown@example.com");

        Person author(author_name, author_email, std::chrono::system_clock::now());
        Person committer = author; // For simplicity, same as author

        repo->commit(message, author, committer);
        return {0, "", ""};
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// StatusCommand implementation
CommandResult StatusCommand::execute(const std::vector<std::string>& args) {
    try {
        auto repo = Repository::open(".");

        std::ostringstream oss;

        // Branch information
        auto head_branch = repo->refs().get_head_branch();
        if (head_branch) {
            oss << "On branch " << *head_branch << "\n\n";
        } else {
            oss << "HEAD detached\n\n";
        }

        // Staged changes
        auto staged = repo->index().get_staged_files();
        if (!staged.empty()) {
            oss << "Changes to be committed:\n";
            for (const auto& file : staged) {
                oss << "  " << file << "\n";
            }
            oss << "\n";
        }

        // Modified files
        auto modified = repo->index().get_modified_files();
        if (!modified.empty()) {
            oss << "Changes not staged for commit:\n";
            for (const auto& file : modified) {
                oss << "  " << file << "\n";
            }
            oss << "\n";
        }

        // Untracked files
        auto untracked = repo->index().get_untracked_files();
        if (!untracked.empty()) {
            oss << "Untracked files:\n";
            for (const auto& file : untracked) {
                oss << "  " << file << "\n";
            }
            oss << "\n";
        }

        if (staged.empty() && modified.empty() && untracked.empty()) {
            oss << "nothing to commit, working tree clean\n";
        }

        return {0, oss.str(), ""};
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// LogCommand implementation
CommandResult LogCommand::execute(const std::vector<std::string>& args) {
    try {
        auto repo = Repository::open(".");

        std::ostringstream oss;
        ObjectId commit_id = repo->refs().get_head();

        int count = 10; // Default to last 10 commits
        for (const auto& arg : args) {
            if (arg.substr(0, 2) == "-n" && arg.length() > 2) {
                count = std::stoi(arg.substr(2));
                break;
            }
        }

        int commits_shown = 0;
        while (!commit_id.empty() && commits_shown < count) {
            auto commit = repo->objects().load(commit_id);
            if (commit->type() != ObjectType::Commit) {
                break;
            }

            // Cast to Commit
            const Commit* commit_obj = static_cast<const Commit*>(commit.get());

            oss << "commit " << commit_id.substr(0, 7) << "\n";
            oss << "Author: " << commit_obj->author().name << " <" << commit_obj->author().email << ">\n";
            oss << "Date: " << std::chrono::duration_cast<std::chrono::seconds>(
                commit_obj->author().when.time_since_epoch()).count() << "\n\n";
            oss << "    " << commit_obj->message() << "\n\n";

            // Get parent
            auto parents = commit_obj->parent_ids();
            if (parents.empty()) {
                break;
            }
            commit_id = parents[0];
            commits_shown++;
        }

        return {0, oss.str(), ""};
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// BranchCommand implementation
CommandResult BranchCommand::execute(const std::vector<std::string>& args) {
    try {
        auto repo = Repository::open(".");

        if (args.empty()) {
            // List branches
            auto branches = repo->refs().list_branches();
            std::string current = repo->refs().get_head_branch().value_or("");

            for (const auto& branch : branches) {
                std::string short_name = branch.substr(11); // Remove "refs/heads/"
                if (short_name == current) {
                    std::cout << "* " << short_name << "\n";
                } else {
                    std::cout << "  " << short_name << "\n";
                }
            }

            return {0, "", ""};
        } else if (args[0] == "-a") {
            // List all branches including remotes
            auto branches = repo->refs().list_branches();
            auto remote_branches = repo->refs().list_remote_branches();

            for (const auto& branch : branches) {
                std::string short_name = branch.substr(11);
                std::cout << "* " << short_name << "\n";
            }

            for (const auto& branch : remote_branches) {
                std::cout << "  " << branch << "\n";
            }

            return {0, "", ""};
        } else {
            // Create new branch
            std::string branch_name = args[0];
            ObjectId head_id = repo->refs().get_head();

            repo->refs().create_ref("refs/heads/" + branch_name, head_id);

            std::ostringstream oss;
            oss << "Created branch " << branch_name << "\n";
            return {0, oss.str(), ""};
        }
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// CheckoutCommand implementation
CommandResult CheckoutCommand::execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {1, "", "Error: 'checkout' requires a branch name\n"};
    }

    try {
        auto repo = Repository::open(".");

        std::string branch_name = args[0];
        ObjectId commit_id = repo->refs().resolve_ref("refs/heads/" + branch_name);

        // Update HEAD
        repo->refs().set_head_to_branch(branch_name);

        std::ostringstream oss;
        oss << "Switched to branch " << branch_name << "\n";
        return {0, oss.str(), ""};
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// RemoteCommand implementation
CommandResult RemoteCommand::execute(const std::vector<std::string>& args) {
    try {
        auto repo = Repository::open(".");

        if (args.empty()) {
            // List remotes
            auto remotes = repo.config().get_sections();
            std::ostringstream oss;

            for (const auto& remote : remotes) {
                if (remote.substr(0, 7) == "remote ") {
                    std::string remote_name = remote.substr(7);
                    std::string url = repo.config().get_string("remote", remote_name, "");
                    oss << remote_name << "\t" << url << "\n";
                }
            }

            return {0, oss.str(), ""};
        }

        std::string subcommand = args[0];

        if (subcommand == "add" && args.size() >= 3) {
            std::string name = args[1];
            std::string url = args[2];

            repo.config().set_value("remote", name, url);
            repo.config().save();

            std::ostringstream oss;
            oss << "Remote '" << name << "' added: " << url << "\n";
            return {0, oss.str(), ""};
        }

        if (subcommand == "remove" && args.size() >= 2) {
            std::string name = args[1];

            // Remove remote config
            repo.config().unset_value("remote", name);
            repo.config().save();

            std::ostringstream oss;
            oss << "Remote '" << name << "' removed\n";
            return {0, oss.str(), ""};
        }

        return {1, "", "Error: Unknown remote subcommand\n"};
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// PushCommand implementation
CommandResult PushCommand::execute(const std::vector<std::string>& args) {
    try {
        auto repo = Repository::open(".");

        std::string remote_name = "origin";
        std::string branch_name = "master";
        bool force = false;

        // Parse arguments
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "--force" || args[i] == "-f") {
                force = true;
            } else if (args[i].find('/') != std::string::npos) {
                // remote/branch format
                auto slash_pos = args[i].find('/');
                remote_name = args[i].substr(0, slash_pos);
                branch_name = args[i].substr(slash_pos + 1);
            } else if (i == 0) {
                remote_name = args[i];
            }
        }

        // Get remote URL
        std::string remote_url = repo.config().get_string("remote", remote_name, "");
        if (remote_url.empty()) {
            return {1, "", "Error: Remote '" + remote_name + "' not found\n"};
        }

        // Create remote and push
        Remote remote(repo, remote_name);
        remote.set_url(remote_url);

        if (remote.push(branch_name, force)) {
            std::ostringstream oss;
            oss << "Pushed to " << remote_name << "/" << branch_name << "\n";
            return {0, oss.str(), ""};
        } else {
            return {1, "", "Error: Push failed\n"};
        }
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// PullCommand implementation
CommandResult PullCommand::execute(const std::vector<std::string>& args) {
    try {
        auto repo = Repository::open(".");

        std::string remote_name = "origin";
        std::string branch_name = "master";

        // Parse arguments
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i].find('/') != std::string::npos) {
                auto slash_pos = args[i].find('/');
                remote_name = args[i].substr(0, slash_pos);
                branch_name = args[i].substr(slash_pos + 1);
            } else if (i == 0) {
                remote_name = args[i];
            }
        }

        // Get remote URL
        std::string remote_url = repo.config().get_string("remote", remote_name, "");
        if (remote_url.empty()) {
            return {1, "", "Error: Remote '" + remote_name + "' not found\n"};
        }

        // Create remote and fetch
        Remote remote(repo, remote_name);
        remote.set_url(remote_url);

        if (remote.fetch(branch_name)) {
            std::ostringstream oss;
            oss << "Pulled from " << remote_name << "/" << branch_name << "\n";
            return {0, oss.str(), ""};
        } else {
            return {1, "", "Error: Pull failed\n"};
        }
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// FetchCommand implementation
CommandResult FetchCommand::execute(const std::vector<std::string>& args) {
    try {
        auto repo = Repository::open(".");

        std::string remote_name = "origin";
        std::string branch_name = "master";

        // Parse arguments
        if (!args.empty()) {
            remote_name = args[0];
        }

        // Get remote URL
        std::string remote_url = repo.config().get_string("remote", remote_name, "");
        if (remote_url.empty()) {
            return {1, "", "Error: Remote '" + remote_name + "' not found\n"};
        }

        // Create remote and fetch
        Remote remote(repo, remote_name);
        remote.set_url(remote_url);

        if (remote.fetch(branch_name)) {
            std::ostringstream oss;
            oss << "Fetched from " << remote_name << "\n";
            return {0, oss.str(), ""};
        } else {
            return {1, "", "Error: Fetch failed\n"};
        }
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// CloneCommand implementation
CommandResult CloneCommand::execute(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return {1, "", "Error: clone requires source and destination arguments\n"};
    }

    std::string source_url = args[0];
    std::string dest_path = args[1];

    try {
        // Create destination directory
        std::filesystem::create_directories(dest_path);

        // Initialize repository
        auto repo = Repository::create(dest_path);

        // Set up remote
        repo.config().set_value("remote", "origin", source_url);
        repo.config().save();

        // Create remote object and fetch
        Remote remote(repo, "origin");
        remote.set_url(source_url);

        if (remote.fetch("master")) {
            std::ostringstream oss;
            oss << "Cloned repository from " << source_url << " to " << dest_path << "\n";
            return {0, oss.str(), ""};
        } else {
            return {1, "", "Error: Clone failed during fetch\n"};
        }
    } catch (const std::exception& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// MergeCommand implementation
CommandResult MergeCommand::execute(const std::vector<std::string>& args) {
    try {
        auto repo = Repository::open(".");

        std::string branch_name = "master";
        bool no_commit = false;
        bool no_ff = false;

        // Parse arguments
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "--no-commit") {
                no_commit = true;
            } else if (args[i] == "--no-ff") {
                no_ff = true;
            } else if (args[i] != "--abort" && args[i] != "--continue") {
                branch_name = args[i];
            }
        }

        // Get current branch and commit
        auto current_branch = repo.refs().get_head_branch();
        if (!current_branch) {
            return {1, "", "Error: Not on a branch\n"};
        }

        std::string our_commit = repo.refs().get_head();

        // Get their commit
        auto their_ref = repo.refs().read_ref("refs/heads/" + branch_name);
        if (!their_ref) {
            return {1, "", "Error: Branch '" + branch_name + "' not found\n"};
        }

        std::string their_commit = *their_ref;

        if (our_commit == their_commit) {
            std::ostringstream oss;
            oss << "Already up to date\n";
            return {0, oss.str(), ""};
        }

        // Perform the merge
        ThreeWayMerge merger(repo);
        auto result = merger.merge(our_commit, our_commit, their_commit);

        std::ostringstream oss;
        switch (result.status) {
            case MergeStatus::Success:
                oss << "Merge successful\n";
                break;
            case MergeStatus::Conflicts:
                oss << "Merge conflicts detected in:\n";
                for (const auto& conflict : result.conflicts) {
                    oss << "  " << conflict.path << "\n";
                }
                oss << "Please resolve conflicts and run 'dgit commit'\n";
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

// PackCommand implementation
CommandResult PackCommand::execute(const std::vector<std::string>& args) {
    try {
        auto repo = Repository::open(".");

        std::ostringstream oss;
        oss << "Packing objects...\n";

        // Create packfile
        std::string packfile_path = repo.git_dir() + "/objects/pack/pack-" +
                                   SHA1::hash("packfile") + ".pack";
        std::string index_path = packfile_path.substr(0, packfile_path.length() - 4) + "idx";

        std::vector<std::string> object_ids;
        // In a real implementation, this would collect all loose objects

        if (packfile::create_packfile(packfile_path, index_path, object_ids)) {
            oss << "Pack created: " << packfile_path << "\n";
            oss << "Index created: " << index_path << "\n";
            return {0, oss.str(), ""};
        } else {
            return {1, "", "Error: Failed to create packfile\n"};
        }
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// RepackCommand implementation
CommandResult RepackCommand::execute(const std::vector<std::string>& args) {
    try {
        auto repo = Repository::open(".");

        std::ostringstream oss;
        oss << "Repacking repository...\n";

        if (packfile::repack_repository(repo)) {
            oss << "Repository repacked successfully\n";
            return {0, oss.str(), ""};
        } else {
            return {1, "", "Error: Repack failed\n"};
        }
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

// GarbageCollectCommand implementation
CommandResult GarbageCollectCommand::execute(const std::vector<std::string>& args) {
    try {
        auto repo = Repository::open(".");

        std::ostringstream oss;
        oss << "Running garbage collection...\n";

        if (packfile::garbage_collect(repo)) {
            auto stats = packfile::get_packfile_stats(repo);
            oss << "Garbage collection completed\n";
            oss << "Objects: " << stats.object_count << "\n";
            oss << "Packfiles: " << stats.packfiles.size() << "\n";
            return {0, oss.str(), ""};
        } else {
            return {1, "", "Error: Garbage collection failed\n"};
        }
    } catch (const GitException& e) {
        return {1, "", "Error: " + std::string(e.what()) + "\n"};
    }
}

} // namespace dgit
