//
// Created by xiesenxin on 2025/2/26.
//

#include <string>
#include <iostream>

#include "leveldb/db.h"

int main() {
  leveldb::Options options;
  options.create_if_missing = true;
  std::string db_name = "/tmp/db_test_xsx";
  leveldb::DB *db = nullptr;
  auto status = leveldb::DB::Open(options, db_name, &db);
  if (!status.ok()) {
    std::cout << "Open db error" << std::endl;
    exit(-1);
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;
  for (int i = 0; i < 10000; i++) {
    db->Put(write_options, "key" + std::to_string(i), "value" + std::to_string(i));
  }
}