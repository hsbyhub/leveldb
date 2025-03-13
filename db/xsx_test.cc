//
// Created by xiesenxin on 2025/2/26.
//

#include <string>
#include <random>
#include <iostream>
#include <unistd.h>

#include "gtest/gtest.h"
#include "leveldb/db.h"

const static std::string chars =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";
std::random_device rd;  // 获取真实随机数种子
std::mt19937 gen(rd()); // 以种子初始化 Mersenne Twister 随机数生成器
std::uniform_int_distribution<> dis(0, chars.size() - 1);

// 生成长度为 n 的随机字符串，字符集包含大小写字母和数字
void gen_rand_string(size_t n, std::string &ret) {
  if (ret.size() != n) {
    ret.resize(n);
  }
  for (size_t i = 0; i < n; ++i) {
    ret[i] = chars[dis(gen)];
  }
}

class DBTest : public testing::Test {
 protected:
  std::string db_name_;
  leveldb::DB *db_ = nullptr;

 public:
  DBTest() {
    this->db_name_ = "/tmp/db_test_xsx";

    leveldb::Options options;
    options.create_if_missing = true;
    auto status = leveldb::DB::Open(options, this->db_name_, &this->db_);
    if (!status.ok()) {
      GTEST_LOG_(ERROR) << "Open db error";
      exit(-1);
    }
  }

  ~DBTest() override {
    delete db_;
  }
};

TEST_F(DBTest, Put) {
  leveldb::WriteOptions write_options;
//  write_options.sync = true;
  std::string key;
  std::string value;
  for (int i = 0; i < 10 * 1000 * 1000; i++) {
    gen_rand_string(3, key);
    gen_rand_string(4, value);

    this->db_->Put(write_options, key, value);

    if (i % 1000 == 0) {
      GTEST_LOG_(INFO) << key << ":" << value;
    }
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}