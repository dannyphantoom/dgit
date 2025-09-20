#include <gtest/gtest.h>
#include "dgit/object.hpp"
#include "dgit/repository.hpp"
#include "dgit/object_database.hpp"
#include <filesystem>

namespace fs = std::filesystem;

class ObjectTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "dgit_objects_test";
        fs::create_directories(test_dir_);
        original_dir_ = fs::current_path();
        fs::current_path(test_dir_);
    }

    void TearDown() override {
        fs::current_path(original_dir_);
        fs::remove_all(test_dir_);
    }

    fs::path test_dir_;
    fs::path original_dir_;
};

TEST_F(ObjectTest, BlobSerialization) {
    std::string content = "test blob content\nwith multiple lines\n";
    auto blob = std::make_unique<dgit::Blob>(content);

    std::string serialized = blob->serialize();
    EXPECT_EQ(serialized, content);

    auto deserialized = dgit::Object::deserialize(serialized);
    EXPECT_EQ(deserialized->type(), dgit::ObjectType::Blob);
    EXPECT_EQ(deserialized->data(), content);
}

TEST_F(ObjectTest, TreeSerialization) {
    auto tree = std::make_unique<dgit::Tree>();

    tree->add_entry(dgit::FileMode::Regular, "abc123", "file1.txt");
    tree->add_entry(dgit::FileMode::Executable, "def456", "script.sh");

    auto entries = tree->entries();
    EXPECT_EQ(entries.size(), 2);
    EXPECT_EQ(entries[0].name, "file1.txt");
    EXPECT_EQ(entries[1].name, "script.sh");
}

TEST_F(ObjectTest, CommitSerialization) {
    std::string tree_id = "abc123";
    std::vector<std::string> parents = {"def456", "ghi789"};
    dgit::Person author("John Doe", "john@example.com", std::chrono::system_clock::now());
    dgit::Person committer("Jane Smith", "jane@example.com", std::chrono::system_clock::now());
    std::string message = "Test commit\n\nThis is a test commit message.";

    auto commit = std::make_unique<dgit::Commit>(tree_id, parents, author, committer, message);

    EXPECT_EQ(commit->tree_id(), tree_id);
    EXPECT_EQ(commit->parent_ids(), parents);
    EXPECT_EQ(commit->author().name, author.name);
    EXPECT_EQ(commit->committer().name, committer.name);
    EXPECT_EQ(commit->message(), message);
}

TEST_F(ObjectTest, ObjectDatabase) {
    // Create repository
    auto repo = dgit::Repository::create(".");

    // Create test objects
    auto blob1 = std::make_unique<dgit::Blob>("content 1");
    auto blob2 = std::make_unique<dgit::Blob>("content 2");

    // Store objects
    repo.objects().store(blob1);
    repo.objects().store(blob2);

    // Retrieve objects
    auto retrieved1 = repo.objects().load(blob1->id());
    auto retrieved2 = repo.objects().load(blob2->id());

    EXPECT_EQ(retrieved1->type(), dgit::ObjectType::Blob);
    EXPECT_EQ(retrieved1->data(), "content 1");
    EXPECT_EQ(retrieved2->data(), "content 2");

    // Test existence
    EXPECT_TRUE(repo.objects().exists(blob1->id()));
    EXPECT_TRUE(repo.objects().exists(blob2->id()));
    EXPECT_FALSE(repo.objects().exists("nonexistent"));
}

TEST_F(ObjectTest, TreeWithEntries) {
    auto tree = std::make_unique<dgit::Tree>();

    // Add various types of entries
    tree->add_entry(dgit::FileMode::Regular, "abc123", "readme.txt");
    tree->add_entry(dgit::FileMode::Executable, "def456", "build.sh");
    tree->add_entry(dgit::FileMode::Directory, "ghi789", "src");

    auto entries = tree->entries();
    EXPECT_EQ(entries.size(), 3);

    // Check that entries are sorted
    EXPECT_EQ(entries[0].name, "build.sh");
    EXPECT_EQ(entries[1].name, "readme.txt");
    EXPECT_EQ(entries[2].name, "src");

    // Check types
    EXPECT_EQ(entries[0].mode, dgit::FileMode::Executable);
    EXPECT_EQ(entries[1].mode, dgit::FileMode::Regular);
    EXPECT_EQ(entries[2].mode, dgit::FileMode::Directory);
}

TEST_F(ObjectTest, CommitWithParents) {
    std::string tree_id = "tree123";
    std::vector<std::string> parents = {"parent1", "parent2", "parent3"};

    dgit::Person author("Multi Author", "multi@example.com", std::chrono::system_clock::now());
    dgit::Person committer("Merge Committer", "merge@example.com", std::chrono::system_clock::now());

    std::string message = "Merge commit with multiple parents\n\n"
                         "This commit merges three parent commits.";

    auto commit = std::make_unique<dgit::Commit>(tree_id, parents, author, committer, message);

    EXPECT_EQ(commit->parent_ids().size(), 3);
    EXPECT_EQ(commit->parent_ids()[0], "parent1");
    EXPECT_EQ(commit->parent_ids()[1], "parent2");
    EXPECT_EQ(commit->parent_ids()[2], "parent3");
}

TEST(ObjectTest, TagCreation) {
    std::string object_id = "abc123";
    dgit::ObjectType object_type = dgit::ObjectType::Commit;
    std::string tag_name = "v1.0.0";
    dgit::Person tagger("Tagger Name", "tagger@example.com", std::chrono::system_clock::now());
    std::string message = "Release version 1.0.0\n\nThis is the first stable release.";

    auto tag = std::make_unique<dgit::Tag>(object_id, object_type, tag_name, tagger, message);

    EXPECT_EQ(tag->object_id(), object_id);
    EXPECT_EQ(tag->object_type(), object_type);
    EXPECT_EQ(tag->tag_name(), tag_name);
    EXPECT_EQ(tag->tagger().name, tagger.name);
    EXPECT_EQ(tag->message(), message);
}

TEST(ObjectTest, ObjectIDGeneration) {
    // Objects with same content should have same ID
    auto blob1 = std::make_unique<dgit::Blob>("same content");
    auto blob2 = std::make_unique<dgit::Blob>("same content");

    EXPECT_EQ(blob1->id(), blob2->id());

    // Objects with different content should have different IDs
    auto blob3 = std::make_unique<dgit::Blob>("different content");
    EXPECT_NE(blob1->id(), blob3->id());
}

TEST(ObjectTest, LargeBlobHandling) {
    // Test handling of large content
    std::string large_content;
    for (int i = 0; i < 10000; ++i) {
        large_content += "Large content line " + std::to_string(i) + "\n";
    }

    auto blob = std::make_unique<dgit::Blob>(large_content);

    EXPECT_EQ(blob->data(), large_content);
    EXPECT_FALSE(blob->id().empty());
    EXPECT_EQ(blob->id().length(), 40);
}

TEST(ObjectTest, SpecialCharacters) {
    // Test content with special characters
    std::string special_content = "Content with special chars: \n\t\r\x00\x01\x7F";
    auto blob = std::make_unique<dgit::Blob>(special_content);

    EXPECT_EQ(blob->data(), special_content);
    EXPECT_FALSE(blob->id().empty());
}

TEST(ObjectTest, EmptyObjects) {
    // Test empty blob
    auto empty_blob = std::make_unique<dgit::Blob>("");
    EXPECT_EQ(empty_blob->data(), "");
    EXPECT_FALSE(empty_blob->id().empty());

    // Test empty tree
    auto empty_tree = std::make_unique<dgit::Tree>();
    EXPECT_TRUE(empty_tree->entries().empty());
    EXPECT_FALSE(empty_tree->id().empty());
}
