#include <gtest/gtest.h>
#include "dgit/sha1.hpp"

TEST(SHA1Test, KnownHashValues) {
    // Test cases from SHA-1 specification and common test vectors

    // Empty string
    EXPECT_EQ(dgit::SHA1::hash(""),
              "da39a3ee5e6b4b0d3255bfef95601890afd80709");

    // Single character
    EXPECT_EQ(dgit::SHA1::hash("a"),
              "86f7e437faa5a7fce15d1ddcb9eaeaea377667b8");

    // Short string
    EXPECT_EQ(dgit::SHA1::hash("abc"),
              "a9993e364706816aba3e25717850c26c9cd0d89d");

    // Longer string
    EXPECT_EQ(dgit::SHA1::hash("hello world"),
              "2aae6c35c94fcfb415dbe95f408b9ce91ee846ed");

    // Very long string (test streaming)
    std::string long_string;
    for (int i = 0; i < 10000; ++i) {
        long_string += "test data chunk ";
    }

    std::string hash1 = dgit::SHA1::hash(long_string);
    EXPECT_FALSE(hash1.empty());
    EXPECT_EQ(hash1.length(), 40);
}

TEST(SHA1Test, FileHashing) {
    // Create temporary files for testing
    std::string filename1 = "/tmp/test1.txt";
    std::string filename2 = "/tmp/test2.txt";

    // Write different content to files
    std::ofstream file1(filename1);
    file1 << "This is test file 1";
    file1.close();

    std::ofstream file2(filename2);
    file2 << "This is test file 2";
    file2.close();

    // Hash files
    std::string hash1 = dgit::SHA1::hash_file(filename1);
    std::string hash2 = dgit::SHA1::hash_file(filename2);

    EXPECT_FALSE(hash1.empty());
    EXPECT_FALSE(hash2.empty());
    EXPECT_NE(hash1, hash2); // Different content should produce different hashes

    // Clean up
    std::remove(filename1.c_str());
    std::remove(filename2.c_str());
}

TEST(SHA1Test, Consistency) {
    // Same input should always produce same output
    std::string input = "consistency test";

    std::string hash1 = dgit::SHA1::hash(input);
    std::string hash2 = dgit::SHA1::hash(input);
    std::string hash3 = dgit::SHA1::hash(std::string(input));

    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash2, hash3);
}

TEST(SHA1Test, BinaryData) {
    // Test with binary data
    std::vector<uint8_t> binary_data = {0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD};

    std::string data(reinterpret_cast<char*>(binary_data.data()), binary_data.size());
    std::string hash = dgit::SHA1::hash(data);

    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.length(), 40);

    // Hash should be consistent
    std::string hash2 = dgit::SHA1::hash(data);
    EXPECT_EQ(hash, hash2);
}

TEST(SHA1Test, StreamingAPI) {
    dgit::SHA1 sha1;

    // Hash data in chunks
    sha1.update("hello");
    sha1.update(" ");
    sha1.update("world");

    std::string hash1 = sha1.final();

    // Hash all at once
    std::string hash2 = dgit::SHA1::hash("hello world");

    EXPECT_EQ(hash1, hash2);
}

TEST(SHA1Test, LargeData) {
    // Test with larger data
    std::string large_data;
    for (int i = 0; i < 100000; ++i) {
        large_data += "This is a test string for large data hashing. ";
    }

    auto start = std::chrono::high_resolution_clock::now();
    std::string hash = dgit::SHA1::hash(large_data);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.length(), 40);

    // Should be reasonably fast (less than 5 seconds for 100k iterations)
    EXPECT_LT(duration.count(), 5000);
}
