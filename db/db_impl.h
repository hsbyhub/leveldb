// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_DB_IMPL_H_
#define STORAGE_LEVELDB_DB_DB_IMPL_H_

#include <atomic>
#include <deque>
#include <set>
#include <string>

#include "db/dbformat.h"
#include "db/log_writer.h"
#include "db/snapshot.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "port/port.h"
#include "port/thread_annotations.h"

namespace leveldb {

class MemTable;
class TableCache;
class Version;
class VersionEdit;
class VersionSet;

class DBImpl : public DB {
 public:
  DBImpl(const Options& options, const std::string& dbname);                    //xsx// 构造方法，传入DB选项(options)和DB名(dbname)

  DBImpl(const DBImpl&) = delete;                                               //xsx// 禁用拷贝构造
  DBImpl& operator=(const DBImpl&) = delete;                                    //xsx// 禁用赋值运算符

  ~DBImpl() override;

  // Implementations of the DB interface
  Status Put(const WriteOptions&, const Slice& key,
             const Slice& value) override;                                      //xsx// 写入数据(覆盖)
  Status Delete(const WriteOptions&, const Slice& key) override;                //xsx// 删除数据(覆盖)
  Status Write(const WriteOptions& options, WriteBatch* updates) override;      //xsx// 写入数据的实现
  Status Get(const ReadOptions& options, const Slice& key,
             std::string* value) override;                                      //xsx// 获取数据的实现
  Iterator* NewIterator(const ReadOptions&) override;                           //xsx// 创建DB的迭代器，用于遍历所有数据
  const Snapshot* GetSnapshot() override;                                       //xsx// 获取快照
  void ReleaseSnapshot(const Snapshot* snapshot) override;                      //xsx// 释放快照
  bool GetProperty(const Slice& property, std::string* value) override;         //xsx// 用于获取当前DB的信息，例如 stats(SSTable文件读写信息)、sstables(SSTable文件列表)
  void GetApproximateSizes(const Range* range, int n, uint64_t* sizes) override;//xsx// 获取n个range大致占用的存储字节数(压缩后)
  void CompactRange(const Slice* begin, const Slice* end) override;             //xsx// 手动请求合并某范围

  // Extra methods (for testing) that are not in the public DB interface

  // Compact any files in the named level that overlap [*begin,*end]
  void TEST_CompactRange(int level, const Slice* begin, const Slice* end);

  // Force current memtable contents to be compacted.
  Status TEST_CompactMemTable();

  // Return an internal iterator over the current state of the database.
  // The keys of this iterator are internal keys (see format.h).
  // The returned iterator should be deleted when no longer needed.
  Iterator* TEST_NewInternalIterator();

  // Return the maximum overlapping data (in bytes) at next level for any
  // file at a level >= 1.
  int64_t TEST_MaxNextLevelOverlappingBytes();

  // Record a sample of bytes read at the specified internal key.
  // Samples are taken approximately once every config::kReadBytesPeriod
  // bytes.
  void RecordReadSample(Slice key);

 private:
  friend class DB;
  struct CompactionState;
  struct Writer;

  // Information for a manual compaction
  struct ManualCompaction {
    int level;
    bool done;
    const InternalKey* begin;  // null means beginning of key range
    const InternalKey* end;    // null means end of key range
    InternalKey tmp_storage;   // Used to keep track of compaction progress
  };

  // Per level compaction stats.  stats_[level] stores the stats for
  // compactions that produced data for the specified "level".
  struct CompactionStats {                                                      //xsx// 统计合并的读写和时间消耗
    CompactionStats() : micros(0), bytes_read(0), bytes_written(0) {}

    void Add(const CompactionStats& c) {
      this->micros += c.micros;
      this->bytes_read += c.bytes_read;
      this->bytes_written += c.bytes_written;
    }

    int64_t micros;
    int64_t bytes_read;
    int64_t bytes_written;
  };

  Iterator* NewInternalIterator(const ReadOptions&,
                                SequenceNumber* latest_snapshot,
                                uint32_t* seed);

  Status NewDB();                                                               //xsx// 创建DB

  // Recover the descriptor from persistent storage.  May do a significant
  // amount of work to recover recently logged updates.  Any changes to
  // be made to the descriptor are added to *edit.
  Status Recover(VersionEdit* edit, bool* save_manifest)                        //xsx// 从已有的MANIFEST中恢复DB到内存实例中
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void MaybeIgnoreError(Status* s) const;                                       //xsx// 忽略某些异常，使DB实例继续可用，例如在插入数据到memtable时的异常

  // Delete any unneeded files and stale in-memory entries.
  void RemoveObsoleteFiles() EXCLUSIVE_LOCKS_REQUIRED(mutex_);                  //xsx// 清理无关紧要的文件，比如 被合并的文件

  // Compact the in-memory write buffer to disk.  Switches to a new
  // log-file/memtable and writes a new descriptor iff successful.
  // Errors are recorded in bg_error_.
  void CompactMemTable() EXCLUSIVE_LOCKS_REQUIRED(mutex_);                      //xsx// 将内存memtable刷出到SSTable，并记录到版本

  Status RecoverLogFile(uint64_t log_number, bool last_log, bool* save_manifest,//xsx// 恢复WAL-log日志
                        VersionEdit* edit, SequenceNumber* max_sequence)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Status WriteLevel0Table(MemTable* mem, VersionEdit* edit, Version* base)      //xsx// 将memtable刷出到SSTable文件
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Status MakeRoomForWrite(bool force /* compact even if there is room? */)      //xsx// 为写操作创建空间，例如转移 memtable->immemtable，并创建新的memtable
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  WriteBatch* BuildBatchGroup(Writer** last_writer)                             //xsx// 聚合多个写操作的数据
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void RecordBackgroundError(const Status& s);                                  //xsx// 登记后台线程的异常并通知其它线程

  void MaybeScheduleCompaction() EXCLUSIVE_LOCKS_REQUIRED(mutex_);              //xsx// 调度后台进程进行合并，包括：level-0的刷出、手动触发合并、size_compaction、seek_compaction
  static void BGWork(void* db);                                                 //xsx// 后台线程回调
  void BackgroundCall();                                                        //xsx// 后台进程实际worker
  void BackgroundCompaction() EXCLUSIVE_LOCKS_REQUIRED(mutex_);                 //xsx// 处理合并
  void CleanupCompaction(CompactionState* compact)                              //xsx// 回收合并状态资源，释放pending文件
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  Status DoCompactionWork(CompactionState* compact)                             //xsx// 实际的合并处理逻辑
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Status OpenCompactionOutputFile(CompactionState* compact);                    //xsx// 合并文件时新建新SSTable文件
  Status FinishCompactionOutputFile(CompactionState* compact, Iterator* input); //xsx// 合并文件时刷出新SSTable文件
  Status InstallCompactionResults(CompactionState* compact)                     //xsx// 合并文件时将提交版本变动，使其生效
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  const Comparator* user_comparator() const {                                   //xsx// 从internal_comparator拿出userkey_comparator，区别在于 internal_key 在user_key的基础上追加了value_type:8和seq_num:56
    return internal_comparator_.user_comparator();
  }

  // Constant after construction
  Env* const env_;                                                              //xsx// 与os环境相关的操作接口，例如读写文件
  const InternalKeyComparator internal_comparator_;                             //xsx// 内部key(<user_key><sequence_num><value_type>)的对比方法
  const InternalFilterPolicy internal_filter_policy_;                           //xsx// 内部key(<user_key><sequence_num><value_type>)过滤规则，例如布隆过滤器
  const Options options_;  // options_.comparator == &internal_comparator_
  const bool owns_info_log_;
  const bool owns_cache_;
  const std::string dbname_;

  // table_cache_ provides its own synchronization
  TableCache* const table_cache_;                                               //xsx// sstable文件的读缓存，主要在get和compact阶段读文件使用

  // Lock over the persistent DB state.  Non-null iff successfully acquired.
  FileLock* db_lock_;                                                           //xsx// 锁住整个DB的目录(db_name_)

  // State below is protected by mutex_
  port::Mutex mutex_;                                                           //xsx// DB实例全局锁
  std::atomic<bool> shutting_down_;                                             //xsx// DB实例销毁标识
  port::CondVar background_work_finished_signal_ GUARDED_BY(mutex_);            //xsx// 后台进程完成信号
  MemTable* mem_;                                                               //xsx// memtable
  MemTable* imm_ GUARDED_BY(mutex_);  // Memtable being compacted               //xsx// immemtable
  std::atomic<bool> has_imm_;         // So bg thread can detect non-null imm_  //xsx// immemtable标识
  WritableFile* logfile_;                                                       //xsx// WAL-log文件操作模块，属于env层，支持多种平台实现，典型的有PosixWritableFile
  uint64_t logfile_number_ GUARDED_BY(mutex_);                                  //xsx// WAL-log文件的标号
  log::Writer* log_;                                                            //xsx// WAL-log编码器, 与MemTable的数据同步，持久化在磁盘上，Put数据的时候，只有WAL-log写成功才会继续执行
  uint32_t seed_ GUARDED_BY(mutex_);  // For sampling.                          //xsx// 抽样时的种子，自动自增

  // Queue of writers.
  std::deque<Writer*> writers_ GUARDED_BY(mutex_);                              //xsx// 写任务队列, 插入队列头部的线程获得执行权，其它线程等待自己的任务被执行结束后直接返回
  WriteBatch* tmp_batch_ GUARDED_BY(mutex_);                                    //xsx// 写入数据时，收集批量数据的临时内存

  SnapshotList snapshots_ GUARDED_BY(mutex_);                                   //xsx// 快照列表

  // Set of table files to protect from deletion because they are
  // part of ongoing compactions.
  std::set<uint64_t> pending_outputs_ GUARDED_BY(mutex_);                       //xsx// 这里保持一些未提交到version_set但是再内存中有操作的新文件的file_number, 比如 新的level-0文件、合并产生的新文件

  // Has a background compaction been scheduled or is running?
  bool background_compaction_scheduled_ GUARDED_BY(mutex_);                     //xsx// 标识后台线程(合并)正在运行

  ManualCompaction* manual_compaction_ GUARDED_BY(mutex_);                      //xsx// 支持手动合并

  VersionSet* const versions_ GUARDED_BY(mutex_);                               //xsx// 版本管理器

  // Have we encountered a background error in paranoid mode?
  Status bg_error_ GUARDED_BY(mutex_);                                          //xsx// 后台线程异常标识

  CompactionStats stats_[config::kNumLevels] GUARDED_BY(mutex_);                //xsx// 统计合并操作的开销数据
};

// Sanitize db options.  The caller should delete result.info_log if
// it is not equal to src.info_log.
Options SanitizeOptions(const std::string& db,
                        const InternalKeyComparator* icmp,
                        const InternalFilterPolicy* ipolicy,
                        const Options& src);

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_DB_IMPL_H_
