#include "dgit/object.hpp"
#include "dgit/sha1.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>

namespace dgit {

// Base Object implementation
Object::Object(ObjectType type, const std::string& data)
    : type_(type), data_(data) {
    compute_id();
}

void Object::compute_id() {
    std::string header;
    switch (type_) {
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
    oss << header << " " << data_.size() << "\0" << data_;
    id_ = SHA1::hash(oss.str());
}

std::string Object::serialize() const {
    return data_;
}

std::unique_ptr<Object> Object::deserialize(const std::string& raw_data) {
    // Find null terminator
    size_t null_pos = raw_data.find('\0');
    if (null_pos == std::string::npos) {
        throw GitException("Invalid object data: no null terminator");
    }

    std::string header = raw_data.substr(0, null_pos);
    std::string content = raw_data.substr(null_pos + 1);

    // Parse header
    size_t space_pos = header.find(' ');
    if (space_pos == std::string::npos) {
        throw GitException("Invalid object header: no space");
    }

    std::string type_str = header.substr(0, space_pos);
    ObjectType type;

    if (type_str == "blob") {
        type = ObjectType::Blob;
    } else if (type_str == "tree") {
        type = ObjectType::Tree;
    } else if (type_str == "commit") {
        type = ObjectType::Commit;
    } else if (type_str == "tag") {
        type = ObjectType::Tag;
    } else {
        throw GitException("Unknown object type: " + type_str);
    }

    // Create object based on type
    switch (type) {
        case ObjectType::Blob:
            return std::make_unique<Blob>(content);
        case ObjectType::Tree:
            return std::make_unique<Tree>();
        case ObjectType::Commit:
            return std::make_unique<Commit>();
        case ObjectType::Tag:
            return std::make_unique<Tag>("", ObjectType::Blob, "", Person("", "", {}), "");
        default:
            throw GitException("Unsupported object type for deserialization");
    }
}

// Tree implementation
Tree::Tree() : Object(ObjectType::Tree, "") {
    // Tree data will be built from entries
}

void Tree::add_entry(FileMode mode, const ObjectId& id, const std::string& name) {
    entries_.emplace_back(mode, id, name);

    // Sort entries for consistent ordering
    std::sort(entries_.begin(), entries_.end(),
              [](const TreeEntry& a, const TreeEntry& b) {
                  return a.name < b.name;
              });

    // Rebuild tree data
    std::ostringstream oss;
    for (const auto& entry : entries_) {
        oss << static_cast<int>(entry.mode) << " " << entry.name << "\0";
        // write binary 20-byte id from hex
        for (size_t i = 0; i < 20; ++i) {
            uint8_t byte = std::stoi(entry.id.substr(i * 2, 2), nullptr, 16);
            oss.put(static_cast<char>(byte));
        }
    }
    data_ = oss.str();
    recompute_id();
}

// Commit implementation
Commit::Commit()
    : Object(ObjectType::Commit, ""),
      tree_id_(""), parent_ids_({}), author_(Person("", "", {})),
      committer_(Person("", "", {})), message_("") {
}

Commit::Commit(const std::string& tree_id, const std::vector<std::string>& parent_ids,
               const Person& author, const Person& committer, const std::string& message)
    : Object(ObjectType::Commit, ""),
      tree_id_(tree_id), parent_ids_(parent_ids), author_(author),
      committer_(committer), message_(message) {

    std::ostringstream oss;
    oss << "tree " << tree_id_ << "\n";

    for (const auto& parent_id : parent_ids_) {
        oss << "parent " << parent_id << "\n";
    }

    oss << "author " << author_.name << " <" << author_.email << "> "
        << std::chrono::duration_cast<std::chrono::seconds>(
            author_.when.time_since_epoch()).count() << "\n";

    oss << "committer " << committer_.name << " <" << committer_.email << "> "
        << std::chrono::duration_cast<std::chrono::seconds>(
            committer_.when.time_since_epoch()).count() << "\n";

    oss << "\n" << message_;

    data_ = oss.str();
    recompute_id();
}

// Tag implementation
Tag::Tag(const std::string& object_id, ObjectType object_type, const std::string& tag_name,
         const Person& tagger, const std::string& message)
    : Object(ObjectType::Tag, ""),
      object_id_(object_id), object_type_(object_type), tag_name_(tag_name),
      tagger_(tagger), message_(message) {

    std::ostringstream oss;
    oss << "object " << object_id_ << "\n";

    oss << "type ";
    switch (object_type_) {
        case ObjectType::Blob:
            oss << "blob";
            break;
        case ObjectType::Tree:
            oss << "tree";
            break;
        case ObjectType::Commit:
            oss << "commit";
            break;
        case ObjectType::Tag:
            oss << "tag";
            break;
    }
    oss << "\n";

    oss << "tag " << tag_name_ << "\n";

    oss << "tagger " << tagger_.name << " <" << tagger_.email << "> "
        << std::chrono::duration_cast<std::chrono::seconds>(
            tagger_.when.time_since_epoch()).count() << "\n";

    oss << "\n" << message_;

    data_ = oss.str();
    recompute_id();
}

// Object methods
void Object::recompute_id() {
    compute_id();
}

} // namespace dgit
