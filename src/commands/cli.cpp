#include "dgit/commands.hpp"
#include <iostream>
#include <algorithm>

namespace dgit {

CLI::CLI() {
    register_commands();
}

void CLI::register_commands() {
    commands_["init"] = std::make_unique<InitCommand>();
    commands_["add"] = std::make_unique<AddCommand>();
    commands_["commit"] = std::make_unique<CommitCommand>();
    commands_["status"] = std::make_unique<StatusCommand>();
    commands_["log"] = std::make_unique<LogCommand>();
    commands_["branch"] = std::make_unique<BranchCommand>();
    commands_["checkout"] = std::make_unique<CheckoutCommand>();
    commands_["remote"] = std::make_unique<RemoteCommand>();
    commands_["push"] = std::make_unique<PushCommand>();
    commands_["pull"] = std::make_unique<PullCommand>();
    commands_["fetch"] = std::make_unique<FetchCommand>();
    commands_["clone"] = std::make_unique<CloneCommand>();
    commands_["merge"] = std::make_unique<MergeCommand>();
    commands_["pack"] = std::make_unique<PackCommand>();
    commands_["repack"] = std::make_unique<RepackCommand>();
    commands_["gc"] = std::make_unique<GarbageCollectCommand>();
}

int CLI::run(int argc, char* argv[]) {
    auto args = parse_args(argc, argv);

    if (args.empty()) {
        show_help();
        return 1;
    }

    std::string command_name = args[0];
    std::vector<std::string> command_args(args.begin() + 1, args.end());

    // Handle special cases
    if (command_name == "--help" || command_name == "-h") {
        show_help();
        return 0;
    }

    if (command_name == "--version" || command_name == "-v") {
        std::cout << "dgit version 1.0.0\n";
        return 0;
    }

    auto it = commands_.find(command_name);
    if (it == commands_.end()) {
        std::cerr << "Unknown command: " << command_name << "\n";
        show_help();
        return 1;
    }

    CommandResult result = dispatch_command(command_name, command_args);

    if (!result.error.empty()) {
        std::cerr << result.error;
    }

    if (!result.output.empty()) {
        std::cout << result.output;
    }

    return result.exit_code;
}

CommandResult CLI::dispatch_command(const std::string& command_name, const std::vector<std::string>& args) {
    auto it = commands_.find(command_name);
    if (it == commands_.end()) {
        return {1, "", "Unknown command: " + command_name + "\n"};
    }

    return it->second->execute(args);
}

void CLI::show_help() const {
    std::cout << "dgit - A Git implementation in C++\n\n";
    std::cout << "Usage: dgit <command> [options] [arguments]\n\n";
    std::cout << "Available commands:\n";

    for (const auto& pair : commands_) {
        std::cout << "  " << pair.first << "\t" << pair.second->description() << "\n";
    }

    std::cout << "\nFor more information about a specific command, run:\n";
    std::cout << "  dgit <command> --help\n";
}

std::vector<std::string> CLI::parse_args(int argc, char* argv[]) {
    std::vector<std::string> args;

    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    return args;
}

} // namespace dgit
