#include "db/database.h"

namespace wdb {

Database::Database(const std::string& path)
    : fm_(path), heap_(fm_) {}

void Database::put(std::string_view k, std::string_view v) { heap_.put(k, v); }
std::optional<std::string> Database::get(std::string_view k) { return heap_.get(k); }
bool Database::del(std::string_view k) { return heap_.del(k); }

}  // namespace wdb
