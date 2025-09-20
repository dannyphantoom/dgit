#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "dgit/repository.hpp"
#include "dgit/sha1.hpp"
#include "dgit/object.hpp"
#include "dgit/config.hpp"
#include "dgit/index.hpp"

namespace fs = std::filesystem;

// Test fixture for repository tests
class RepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for tests
        test_dir_ = fs::temp_directory_path() / "dgit_test";
        fs::create_directories(test_dir_);

        // Change to test directory
        original_dir_ = fs::current_path();
        fs::current_path(test_dir_);
    }

    void TearDown() override {
        // Clean up test directory
        fs::current_path(original_dir_);
        fs::remove_all(test_dir_);
    }

    fs::path test_dir_;
    fs::path original_dir_;
};

// Test SHA-1 implementation
TEST(SHA1Test, HashString) {
    std::string input = "hello world";
    std::string expected = "2aae6c35c94fcfb415dbe95f408b9ce91ee846ed";

    std::string actual = dgit::SHA1::hash(input);
    EXPECT_EQ(actual, expected);
}

TEST(SHA1Test, HashEmptyString) {
    std::string input = "";
    std::string expected = "da39a3ee5e6b4b0d3255bfef95601890afd80709";

    std::string actual = dgit::SHA1::hash(input);
    EXPECT_EQ(actual, expected);
}

TEST(SHA1Test, HashFile) {
    std::string filename = "test.txt";
    std::string content = "test content";

    std::ofstream file(filename);
    file << content;
    file.close();

    std::string hash = dgit::SHA1::hash_file(filename);
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.length(), 40); // SHA-1 hash length

    fs::remove(filename);
}

// Test Git object model
TEST(ObjectTest, BlobCreation) {
    std::string content = "test blob content";
    auto blob = std::make_unique<dgit::Blob>(content);

    EXPECT_EQ(blob->type(), dgit::ObjectType::Blob);
    EXPECT_EQ(blob->data(), content);
    EXPECT_FALSE(blob->id().empty());
    EXPECT_EQ(blob->id().length(), 40);
}

TEST(ObjectTest, TreeCreation) {
    auto tree = std::make_unique<dgit::Tree>();

    // Add some entries
    tree->add_entry(dgit::FileMode::Regular, "abc123", "file1.txt");
    tree->add_entry(dgit::FileMode::Executable, "def456", "file2.sh");

    EXPECT_EQ(tree->type(), dgit::ObjectType::Tree);
    EXPECT_FALSE(tree->id().empty());
}

TEST(ObjectTest, CommitCreation) {
    std::string tree_id = "abc123";
    std::vector<std::string> parents = {"def456"};
    dgit::Person author("Test Author", "author@example.com", std::chrono::system_clock::now());
    dgit::Person committer("Test Committer", "committer@example.com", std::chrono::system_clock::now());
    std::string message = "Test commit";

    auto commit = std::make_unique<dgit::Commit>(tree_id, parents, author, committer, message);

    EXPECT_EQ(commit->type(), dgit::ObjectType::Commit);
    EXPECT_EQ(commit->tree_id(), tree_id);
    EXPECT_EQ(commit->parent_ids(), parents);
    EXPECT_EQ(commit->author().name, author.name);
    EXPECT_EQ(commit->message(), message);
    EXPECT_FALSE(commit->id().empty());
}

// Test configuration system
TEST(ConfigTest, BasicOperations) {
    dgit::Config config;

    // Set values
    config.set_value("core", "repositoryformatversion", "0");
    config.set_value("user", "name", "Test User");
    config.set_value("user", "email", "test@example.com");

    // Get values
    EXPECT_EQ(config.get_string("core", "repositoryformatversion", "1"), "0");
    EXPECT_EQ(config.get_string("user", "name", ""), "Test User");
    EXPECT_EQ(config.get_string("user", "email", ""), "test@example.com");

    // Test non-existent values
    EXPECT_EQ(config.get_string("nonexistent", "key", "default"), "default");
}

// Test repository operations
TEST_F(RepositoryTest, RepositoryCreation) {
    auto repo = dgit::Repository::create(".");

    EXPECT_TRUE(fs::exists(".git"));
    EXPECT_TRUE(fs::exists(".git/objects"));
    EXPECT_TRUE(fs::exists(".git/refs/heads"));
    EXPECT_TRUE(fs::exists(".git/HEAD"));

    // Check HEAD content
    std::ifstream head_file(".git/HEAD");
    std::string head_content;
    std::getline(head_file, head_content);
    EXPECT_EQ(head_content, "ref: refs/heads/master");
}

TEST_F(RepositoryTest, RepositoryOpening) {
    // Create repository
    auto repo1 = dgit::Repository::create(".");

    // Open existing repository
    auto repo2 = dgit::Repository::open(".");

    EXPECT_EQ(repo1->path(), repo2->path());
    EXPECT_EQ(repo1->git_dir(), repo2->git_dir());
}

TEST_F(RepositoryTest, ConfigOperations) {
    auto repo = dgit::Repository::create(".");

    // Set config values
    repo.config().set_value("user", "name", "Test User");
    repo.config().set_value("user", "email", "test@example.com");
    repo.config().save();

    // Check if config file exists and contains values
    EXPECT_TRUE(fs::exists(".git/config"));

    std::ifstream config_file(".git/config");
    std::string content((std::istreambuf_iterator<char>(config_file)),
                        std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("Test User"), std::string::npos);
    EXPECT_NE(content.find("test@example.com"), std::string::npos);
}

TEST_F(RepositoryTest, BlobCreation) {
    auto repo = dgit::Repository::create(".");

    // Create a test file
    std::string filename = "test.txt";
    std::string content = "test file content";

    std::ofstream file(filename);
    file << content;
    file.close();

    // Write blob
    auto blob_id = repo.write_blob(filename);

    EXPECT_FALSE(blob_id.empty());
    EXPECT_EQ(blob_id.length(), 40);

    // Read blob content
    std::string read_content = repo.read_file(blob_id, "read_test.txt");

    EXPECT_EQ(read_content, content);
    EXPECT_TRUE(fs::exists("read_test.txt"));

    // Clean up
    fs::remove(filename);
    fs::remove("read_test.txt");
}

// Test index operations
TEST_F(RepositoryTest, IndexOperations) {
    auto repo = dgit::Repository::create(".");

    // Create test files
    std::string file1 = "file1.txt";
    std::string file2 = "file2.txt";

    std::ofstream f1(file1);
    f1 << "content 1";
    f1.close();

    std::ofstream f2(file2);
    f2 << "content 2";
    f2.close();

    // Add files to index
    repo.index().add_file(file1);
    repo.index().add_file(file2);

    // Check index
    EXPECT_TRUE(repo.index().has_entry(file1));
    EXPECT_TRUE(repo.index().has_entry(file2));
    EXPECT_EQ(repo.index().entry_count(), 2);

    // Save index
    repo.index().save();
    EXPECT_TRUE(fs::exists(".git/index"));

    // Clean up
    fs::remove(file1);
    fs::remove(file2);
}

// Test reference management
TEST_F(RepositoryTest, RefManagement) {
    auto repo = dgit::Repository::create(".");

    // Create a commit first (simplified)
    std::string commit_id = "abc123";

    // Create branch
    repo.refs().create_ref("refs/heads/test-branch", commit_id);

    // Check branch
    EXPECT_TRUE(repo.refs().ref_exists("refs/heads/test-branch"));
    auto ref_value = repo.refs().read_ref("refs/heads/test-branch");
    EXPECT_TRUE(ref_value.has_value());
    EXPECT_EQ(*ref_value, commit_id);

    // List branches
    auto branches = repo.refs().list_branches();
    EXPECT_FALSE(branches.empty());

    // Clean up
    repo.refs().delete_ref("refs/heads/test-branch");
    EXPECT_FALSE(repo.refs().ref_exists("refs/heads/test-branch"));
}

// Test CLI functionality
TEST(CLITest, CommandRegistration) {
    dgit::CLI cli;

    // Test that commands are registered
    // This would normally test the internal command map
    // For now, just test that CLI can be instantiated
    SUCCEED();
}

TEST(CLITest, HelpCommand) {
    dgit::CLI cli;
    int result = cli.run(2, new char*[2] {"dgit", "--help"});

    // Should return success for help
    EXPECT_EQ(result, 0);
}

// Performance tests
TEST(PerformanceTest, SHA1Speed) {
    std::string data(1000, 'a');

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        dgit::SHA1::hash(data);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should be reasonably fast (< 1 second for 1000 hashes)
    EXPECT_LT(duration.count(), 1000);
}

TEST(PerformanceTest, ObjectCreation) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        std::string content(100, 'a' + (i % 26));
        auto blob = std::make_unique<dgit::Blob>(content);
        EXPECT_FALSE(blob->id().empty());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should be reasonably fast
    EXPECT_LT(duration.count(), 500);
}

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
