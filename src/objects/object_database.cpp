#include "dgit/object_database.hpp"
#include "dgit/sha1.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <zlib.h>
#include <cstring>

namespace fs = std::filesystem;
namespace dgit {

ObjectDatabase::ObjectDatabase(const std::string& git_dir)
    : git_dir_(git_dir), objects_dir_(git_dir + "/objects") {

    // Create objects directory structure
    fs::create_directories(objects_dir_ + "/info");
    fs::create_directories(objects_dir_ + "/pack");
}

void ObjectDatabase::store(std::unique_ptr<Object>&& object) {
    const ObjectId& id = object->id();

    if (exists(id)) {
        return; // Object already exists
    }

    std::string data = object->serialize();
    std::string header;
    switch (object->type()) {
        case ObjectType::Blob:
            header = "blob";
            break;
        case ObjectType::Tree:
            header = "tree";
            break;
        case ObjectType::Commit:
            header = "commit";
            break;
        case ObjectType::Tag:
            header = "tag";
            break;
    }

    std::ostringstream oss;
    oss << header << " " << data.size() << "\0" << data;
    std::string full_data = oss.str();

    // Compress the data
    std::string compressed = compress_data(full_data);

    write_object(id, compressed);

    // Cache the object
    cache_[id] = object->clone();
}

std::unique_ptr<Object> ObjectDatabase::load(const ObjectId& id) {
    // Check cache first
    auto it = cache_.find(id);
    if (it != cache_.end()) {
        return it->second->clone();
    }

    if (!exists(id)) {
        throw GitException("Object not found: " + id);
    }

    std::string compressed_data = read_object(id);
    std::string decompressed_data = decompress_data(compressed_data);

    auto object = Object::deserialize(decompressed_data);

    // Cache the object
    cache_[id] = object->clone();

    return object;
}

bool ObjectDatabase::exists(const ObjectId& id) {
    return fs::exists(get_object_path(id));
}

std::string ObjectDatabase::get_object_path(const ObjectId& id) const {
    if (id.length() < 2) {
        throw GitException("Invalid object ID: " + id);
    }

    std::string dir1 = id.substr(0, 2);
    std::string dir2 = id.substr(2);

    return objects_dir_ + "/" + dir1 + "/" + dir2;
}

void ObjectDatabase::write_object(const ObjectId& id, const std::string& data) {
    std::string path = get_object_path(id);
    fs::create_directories(fs::path(path).parent_path());

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw GitException("Cannot write object: " + path);
    }

    file.write(data.c_str(), data.size());
}

std::string ObjectDatabase::read_object(const ObjectId& id) {
    std::string path = get_object_path(id);
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file) {
        throw GitException("Cannot read object: " + path);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string data(size, '\0');
    if (!file.read(&data[0], size)) {
        throw GitException("Failed to read object data");
    }

    return data;
}

// Compression utilities
std::string compress_data(const std::string& data) {
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));

    if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
        throw GitException("Failed to initialize zlib compression");
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.c_str()));
    zs.avail_in = data.size();

    std::string compressed;
    int ret;
    char outbuffer[32768];

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = deflate(&zs, Z_FINISH);

        if (compressed.size() < zs.total_out) {
            compressed.append(outbuffer, zs.total_out - compressed.size());
        }
    } while (ret == Z_OK);

    deflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        throw GitException("Failed to compress data");
    }

    return compressed;
}

std::string decompress_data(const std::string& compressed_data) {
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK) {
        throw GitException("Failed to initialize zlib decompression");
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed_data.c_str()));
    zs.avail_in = compressed_data.size();

    std::string decompressed;
    int ret;
    char outbuffer[32768];

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = inflate(&zs, 0);

        if (decompressed.size() < zs.total_out) {
            decompressed.append(outbuffer, zs.total_out - decompressed.size());
        }
    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        throw GitException("Failed to decompress data");
    }

    return decompressed;
}

// Packfile operations (simplified)
void ObjectDatabase::create_packfile() {
    // TODO: Implement packfile creation
    // This would involve:
    // 1. Finding all loose objects
    // 2. Creating delta compression
    // 3. Writing packfile format
    // 4. Creating packfile index
}

void ObjectDatabase::cleanup() {
    // TODO: Remove loose objects that are in packfiles
}

} // namespace dgit
