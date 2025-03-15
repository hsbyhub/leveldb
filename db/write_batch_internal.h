// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_WRITE_BATCH_INTERNAL_H_
#define STORAGE_LEVELDB_DB_WRITE_BATCH_INTERNAL_H_

#include "db/dbformat.h"
#include "leveldb/write_batch.h"

namespace leveldb {

class MemTable;

// WriteBatchInternal provides static methods for manipulating a
// WriteBatch that we don't want in the public WriteBatch interface.
class WriteBatchInternal {
 public:
  // Return the number of entries in the batch.
  static int Count(const WriteBatch* batch);                                        //xsx// 获取WriteBatch的条目数

  // Set the count for the number of entries in the batch.
  static void SetCount(WriteBatch* batch, int n);                                   //xsx// 设置WriteBatch的条目数

  // Return the sequence number for the start of this batch.
  static SequenceNumber Sequence(const WriteBatch* batch);                          //xsx// 获取WriteBatch的起始序列号

  // Store the specified number as the sequence number for the start of
  // this batch.
  static void SetSequence(WriteBatch* batch, SequenceNumber seq);                   //xsx// 设置WriteBatch的起始序列号

  static Slice Contents(const WriteBatch* batch) { return Slice(batch->rep_); }     //xsx// 获取WriteBatch的内容(as Slice)

  static size_t ByteSize(const WriteBatch* batch) { return batch->rep_.size(); }    //xsx// 获取WriteBatch的内容大小

  static void SetContents(WriteBatch* batch, const Slice& contents);                //xsx// 替换WriteBatch的内容

  static Status InsertInto(const WriteBatch* batch, MemTable* memtable);            //xsx// 插入WriteBatch的数据到MemTable

  static void Append(WriteBatch* dst, const WriteBatch* src);                       //xsx// 追加WriteBatch的数据到另一个WriteBatch
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_WRITE_BATCH_INTERNAL_H_
