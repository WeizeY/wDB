#include "db/database.h"

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void print_help() {
  std::cout << "Commands:\n"
               "  PUT <key> <value>   Insert or update key (value may contain "
               "spaces)\n"
               "  GET <key>           Retrieve value\n"
               "  DEL <key>           Delete key\n"
               "  STAT                Show page counts\n"
               "  HELP                Show this help\n"
               "  EXIT | QUIT         Exit\n";
}

std::string upper(std::string s) {
  for (auto &c : s)
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

} // namespace

int main(int argc, char **argv) {
  const std::string path = (argc > 1) ? argv[1] : "wdb.data";

  try {
    wdb::Database db(path);
    std::cout << "wDB v0.1 — opened " << path << " (pages=" << db.num_pages()
              << ")\n";
    std::cout << "Type HELP for commands.\n";

    std::string line;
    while (true) {
      std::cout << "wDB> " << std::flush;
      if (!std::getline(std::cin, line)) {
        std::cout << "\n";
        break;
      }
      if (line.empty())
        continue;

      std::istringstream iss(line);
      std::string cmd;
      iss >> cmd;
      cmd = upper(cmd);

      try {
        if (cmd == "EXIT" || cmd == "QUIT") {
          break;
        } else if (cmd == "HELP") {
          print_help();
        } else if (cmd == "STAT") {
          std::cout << "pages=" << db.num_pages()
                    << " btree_root=" << db.btree_root()
                    << " wal_bytes=" << db.wal_size() << "\n";
        } else if (cmd == "PUT") {
          std::string key;
          iss >> key;
          if (key.empty()) {
            std::cout << "ERR: missing key\n";
            continue;
          }
          std::string value;
          std::getline(iss, value);
          const size_t s = value.find_first_not_of(' ');
          value = (s == std::string::npos) ? "" : value.substr(s);
          db.put(key, value);
          std::cout << "OK\n";
        } else if (cmd == "GET") {
          std::string key;
          iss >> key;
          if (key.empty()) {
            std::cout << "ERR: missing key\n";
            continue;
          }
          if (auto v = db.get(key))
            std::cout << *v << "\n";
          else
            std::cout << "(nil)\n";
        } else if (cmd == "DEL") {
          std::string key;
          iss >> key;
          if (key.empty()) {
            std::cout << "ERR: missing key\n";
            continue;
          }
          std::cout << (db.del(key) ? "OK\n" : "(nil)\n");
        } else {
          std::cout << "ERR: unknown command '" << cmd << "' (try HELP)\n";
        }
      } catch (const std::exception &e) {
        std::cout << "ERR: " << e.what() << "\n";
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "fatal: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
