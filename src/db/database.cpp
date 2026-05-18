#include "db/database.h"

namespace wdb {

Database::Database(const std::string& path)
    : fm_(path), tree_(fm_, fm_.btree_root()), wal_(path + ".wal") {
    sync_root_if_changed();  // first-time root creation

    const auto records = wal_.replay();
    for (const auto& rec : records) {
        if (rec.op == wal::OpType::Put) {
            tree_.insert(rec.key, rec.value);
        } else {
            tree_.remove(rec.key);
        }
    }
    sync_root_if_changed();  // replay may have grown the tree

    if (!records.empty()) {
        fm_.sync();
        wal_.reset();
    } else {
        wal_.truncate_to_valid();
    }
}

void Database::sync_root_if_changed() {
    if (tree_.root_id() != fm_.btree_root()) {
        fm_.set_btree_root(tree_.root_id());
    }
}

void Database::put(std::string_view k, std::string_view v) {
    wal_.append(wal::OpType::Put, k, v);
    wal_.sync();
    tree_.insert(k, v);
    sync_root_if_changed();
}

std::optional<std::string> Database::get(std::string_view k) const {
    return tree_.get(k);
}

bool Database::del(std::string_view k) {
    wal_.append(wal::OpType::Del, k);
    wal_.sync();
    const bool found = tree_.remove(k);
    sync_root_if_changed();
    return found;
}

}  // namespace wdb
