#include "dgit/packfile.hpp"
#include "dgit/object.hpp"
#include <arpa/inet.h>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cstring>
#include <zlib.h>

namespace dgit {
// Provide 64-bit host-to-network conversion if not available
#ifndef htonll
static inline uint64_t htonll(uint64_t value) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (static_cast<uint64_t>(htonl(static_cast<uint32_t>(value & 0xFFFFFFFFULL))) << 32) |
           htonl(static_cast<uint32_t>(value >> 32));
#else
    return value;
#endif
}
#endif

// PackWriter implementation
PackWriter::PackWriter(const std::string& packfile_path, const std::string& index_path)
    : packfile_path_(packfile_path), index_path_(index_path) {

    pack_file_.open(packfile_path, std::ios::binary);
    if (!pack_file_) {
        throw GitException("Cannot create packfile: " + packfile_path);
    }

    index_file_.open(index_path, std::ios::binary);
    if (!index_file_) {
        throw GitException("Cannot create index file: " + index_path);
    }
}

PackWriter::~PackWriter() {
    if (pack_file_.is_open()) {
        finalize();
    }
}

bool PackWriter::add_object(const ObjectId& sha1, std::unique_ptr<Object> object) {
    PackObjectEntry entry;
    entry.sha1 = sha1;
    entry.offset = pack_file_.tellp();

    // Determine object type
    switch (object->type()) {
        case ObjectType::Commit:
            entry.type = PackObjectType::Commit;
            break;
        case ObjectType::Tree:
            entry.type = PackObjectType::Tree;
            break;
        case ObjectType::Blob:
            entry.type = PackObjectType::Blob;
            break;
        case ObjectType::Tag:
            entry.type = PackObjectType::Tag;
            break;
        default:
            return false;
    }

    // Serialize and compress object
    std::string data = object->serialize();
    std::string compressed = compress_data(data);
    entry.size = data.size();

    // Write object to packfile
    if (write_object(entry, compressed) == 0) {
        return false;
    }

    objects_.push_back(entry);
    return true;
}

bool PackWriter::add_delta(const ObjectId& sha1, const ObjectId& base_sha1,
                          const std::string& delta_data) {
    PackObjectEntry entry;
    entry.sha1 = sha1;
    entry.base_sha1 = base_sha1;
    entry.offset = pack_file_.tellp();

    // This is a simplified implementation
    // In a real Git implementation, delta objects would be properly handled

    return false;
}

bool PackWriter::finalize() {
    if (pack_file_.is_open()) {
        write_trailer();
        pack_file_.close();
    }

    if (index_file_.is_open()) {
        write_index();
        index_file_.close();
    }

    return true;
}

uint32_t PackWriter::write_object(const PackObjectEntry& entry, const std::string& data) {
    // Write object header
    uint8_t byte = (static_cast<uint8_t>(entry.type) << 4) | (entry.size & 0x0F);
    pack_file_.put(static_cast<char>(byte));

    size_t size = entry.size;
    while (size >= 0x80) {
        byte = (size & 0x7F) | 0x80;
        pack_file_.put(static_cast<char>(byte));
        size >>= 7;
    }
    pack_file_.put(static_cast<char>(size));

    // Write compressed data
    pack_file_.write(data.c_str(), data.size());

    return static_cast<uint32_t>(pack_file_.tellp());
}

void PackWriter::write_header() {
    // Write packfile header
    pack_file_.write(packfile_format::PACK_SIGNATURE, 4);
    uint32_t version = htonl(static_cast<uint32_t>(PackVersion::V2));
    pack_file_.write(reinterpret_cast<char*>(&version), 4);
    uint32_t count = htonl(static_cast<uint32_t>(objects_.size()));
    pack_file_.write(reinterpret_cast<char*>(&count), 4);
}

void PackWriter::write_trailer() {
    // Calculate and write SHA-1 of the packfile
    // This is a simplified implementation
    std::string trailer = "0000000000000000000000000000000000000000";
    pack_file_.write(trailer.c_str(), 20);
}

void PackWriter::write_index() {
    // Write index file header
    index_file_.write(packfile_format::IDX_SIGNATURE, 4);
    uint32_t version = htonl(2);  // Index version 2
    index_file_.write(reinterpret_cast<char*>(&version), 4);

    // Write fanout table (simplified)
    for (int i = 0; i < 256; ++i) {
        uint32_t fanout = htonl(static_cast<uint32_t>(objects_.size()));
        index_file_.write(reinterpret_cast<char*>(&fanout), 4);
    }

    // Write object entries
    for (const auto& obj : objects_) {
        // Write SHA-1
        index_file_.write(obj.sha1.c_str(), 20);

        // Write CRC32 (placeholder)
        uint32_t crc32 = 0;
        index_file_.write(reinterpret_cast<char*>(&crc32), 4);

        // Write offset
        uint64_t offset = htonll(static_cast<uint64_t>(obj.offset));
        index_file_.write(reinterpret_cast<char*>(&offset), 8);
    }

    // Write packfile checksum and trailer
    std::string checksum = "0000000000000000000000000000000000000000";
    index_file_.write(checksum.c_str(), 20);
    uint32_t trailer = htonl(objects_.size());
    index_file_.write(reinterpret_cast<char*>(&trailer), 4);
}

std::string PackWriter::compress_data(const std::string& data) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
        throw GitException("Failed to initialize compression");
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

std::string PackWriter::create_delta(const std::string& base_data, const std::string& target_data) {
    // Simplified delta creation
    // In a real implementation, this would use proper delta compression
    return "";
}

// PackReader implementation
PackReader::PackReader(const std::string& packfile_path, const std::string& index_path)
    : packfile_path_(packfile_path), index_path_(index_path) {

    pack_file_.open(packfile_path, std::ios::binary);
    if (!pack_file_) {
        throw GitException("Cannot open packfile: " + packfile_path);
    }

    index_ = std::make_unique<PackIndex>(index_path);
    if (!index_) {
        throw GitException("Cannot open index file: " + index_path);
    }
}

PackReader::~PackReader() = default;

std::unique_ptr<Object> PackReader::get_object(const ObjectId& sha1) {
    auto offset = index_->find_offset(sha1);
    if (!offset) {
        return nullptr;
    }

    return read_object_at_offset(*offset);
}

bool PackReader::has_object(const ObjectId& sha1) const {
    return index_->has_object(sha1);
}

std::vector<ObjectId> PackReader::get_all_objects() const {
    std::vector<ObjectId> objects;
    auto entries = index_->get_entries();
    for (const auto& entry : entries) {
        objects.push_back(entry.sha1);
    }
    return objects;
}

std::unique_ptr<Object> PackReader::read_object_at_offset(size_t offset) {
    pack_file_.seekg(offset);

    // Read object header
    uint8_t byte;
    pack_file_.read(reinterpret_cast<char*>(&byte), 1);

    PackObjectType type = static_cast<PackObjectType>(byte >> 4);
    size_t size = byte & 0x0F;

    // Read size continuation bytes
    int shift = 4;
    while (byte & 0x80) {
        pack_file_.read(reinterpret_cast<char*>(&byte), 1);
        size |= (byte & 0x7F) << shift;
        shift += 7;
    }

    // Read compressed data
    std::string compressed_data;
    compressed_data.resize(1024); // Simplified

    // In a real implementation, this would read the actual compressed data
    // and decompress it properly

    // Create object based on type
    switch (type) {
        case PackObjectType::Commit:
            return std::make_unique<Blob>("");
        case PackObjectType::Tree:
            return std::make_unique<Tree>();
        case PackObjectType::Blob:
            return std::make_unique<Blob>("");
        case PackObjectType::Tag:
            return std::make_unique<Tag>("", ObjectType::Blob, "", Person("", "", {}), "");
        default:
            return nullptr;
    }
}

std::string PackReader::decompress_data(const std::string& compressed_data) {
    // Simplified decompression
    return compressed_data;
}

std::string PackReader::apply_delta(const std::string& base_data, const std::string& delta_data) {
    // Simplified delta application
    return base_data;
}

// PackIndex implementation
PackIndex::PackIndex(const std::string& index_path) : index_path_(index_path) {
    read_index_file();
}

PackIndex::~PackIndex() = default;

std::vector<PackIndexEntry> PackIndex::get_entries() const {
    return entries_;
}

std::optional<size_t> PackIndex::find_offset(const ObjectId& sha1) const {
    for (const auto& entry : entries_) {
        if (entry.sha1 == sha1) {
            return entry.offset;
        }
    }
    return std::nullopt;
}

bool PackIndex::has_object(const ObjectId& sha1) const {
    return find_offset(sha1).has_value();
}

void PackIndex::read_index_file() {
    std::ifstream file(index_path_, std::ios::binary);
    if (!file) {
        throw GitException("Cannot read index file: " + index_path_);
    }

    // Read and verify signature
    char signature[4];
    file.read(signature, 4);

    if (std::memcmp(signature, packfile_format::IDX_SIGNATURE, 4) != 0) {
        throw GitException("Invalid index file signature");
    }

    // Read version
    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), 4);
    version = ntohl(version);

    if (version != 2) {
        throw GitException("Unsupported index version: " + std::to_string(version));
    }

    // Skip fanout table for now (simplified implementation)
    file.seekg(256 * 4, std::ios::cur);

    // Read object entries
    while (file) {
        PackIndexEntry entry;

        // Read SHA-1
        char sha1_bytes[20];
        if (!file.read(sha1_bytes, 20)) break;

        // Convert to hex string
        for (int i = 0; i < 20; ++i) {
            char buf[3];
            sprintf(buf, "%02x", static_cast<unsigned char>(sha1_bytes[i]));
            entry.sha1 += buf;
        }

        // Skip CRC32 and offset for now (simplified)
        file.seekg(12, std::ios::cur);

        entries_.push_back(entry);
    }
}

// PackObject implementation
PackObject::PackObject(PackObjectType type, const std::string& data)
    : type_(type), data_(data) {}

std::string PackObject::serialize() const {
    return data_;
}

std::unique_ptr<PackObject> PackObject::deserialize(const std::string& data) {
    // Simplified deserialization
    return std::make_unique<PackObject>(PackObjectType::Blob, data);
}

// Delta implementation
std::string Delta::encode(const std::string& base_data, const std::string& target_data) {
    // Simplified delta encoding
    return target_data;
}

std::string Delta::decode(const std::string& base_data, const std::string& delta_data) {
    // Simplified delta decoding
    return base_data + delta_data;
}

// Packfile utilities
namespace packfile {

bool create_packfile(const std::string& packfile_path,
                    const std::string& index_path,
                    const std::vector<std::string>& object_shas) {

    PackWriter writer(packfile_path, index_path);

    // This is a placeholder implementation
    // In a real implementation, this would:
    // 1. Load the objects from the object database
    // 2. Compute deltas between similar objects
    // 3. Write them to the packfile

    return writer.finalize();
}

bool verify_packfile(const std::string& packfile_path,
                    const std::string& index_path) {
    try {
        PackReader reader(packfile_path, index_path);
        return reader.get_object_count() > 0;
    } catch (const std::exception&) {
        return false;
    }
}

std::unique_ptr<Object> extract_object(const std::string& packfile_path,
                                     const std::string& index_path,
                                     const ObjectId& sha1) {
    try {
        PackReader reader(packfile_path, index_path);
        return reader.get_object(sha1);
    } catch (const std::exception&) {
        return nullptr;
    }
}

bool garbage_collect(Repository& repo) {
    // Simplified garbage collection
    // In a real implementation, this would:
    // 1. Find all reachable objects
    // 2. Identify unreachable objects
    // 3. Remove unreachable objects
    return true;
}

bool repack_repository(Repository& repo) {
    // Simplified repack
    // In a real implementation, this would:
    // 1. Read all objects
    // 2. Create new packfiles with better compression
    // 3. Update references
    return true;
}

PackStats get_packfile_stats(Repository& repo) {
    PackStats stats;
    stats.object_count = 0;
    stats.packfile_size = 0;
    stats.index_size = 0;
    stats.compression_ratio = 1.0;
    return stats;
}

bool cleanup_redundant_packs(Repository& repo) {
    return true;
}

bool consolidate_packs(Repository& repo) {
    return true;
}

std::string compute_delta(const std::string& base, const std::string& target) {
    return Delta::encode(base, target);
}

std::string apply_delta(const std::string& base, const std::string& delta) {
    return Delta::decode(base, delta);
}

} // namespace packfile

} // namespace dgit
