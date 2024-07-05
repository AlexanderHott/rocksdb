#include <rocksdb/memtablerep.h>
#include <rocksdb/slice_transform.h>

#include <iostream>
#include <memory>
#include <chrono>

#include "rocksdb/db.h"

#define LOG(msg) \
  std::cout << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl

// class FlushEventListener : public rocksdb::EventListener  {
//   void OnFlushCompleted(
//       rocksdb::DB* db,
//       const rocksdb::FlushJobInfo& flush_job_info) override {
//     LOG(flush_job_info.table_properties.num_entries);
//   }
// };

using std::chrono::high_resolution_clock;

int main() {
  rocksdb::DB* db;
  rocksdb::Options options;
  std::string db_path = "/tmp/testdb";

  options.memtable_factory.reset(new rocksdb::NeverSortedVectorRepFactory());
  // options.memtable_factory.reset(new rocksdb::VectorRepFactory(0));
  // options.memtable_factory.reset(new rocksdb::SkipListFactory());

  // options.prefix_extractor.reset(rocksdb::NewFixedPrefixTransform(1));
  // options.memtable_factory.reset(rocksdb::NewHashLinkListRepFactory());
  // options.memtable_factory.reset(rocksdb::NewHashSkipListRepFactory());


  // options.listeners.emplace_back(std::make_shared<FlushEventListener>());

  options.allow_concurrent_memtable_write = false;
  options.create_if_missing = true;

  DestroyDB(db_path, options);

  rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db);

  if (!status.ok()) {
    LOG(status.ToString());
  }

  // put
  LOG("PUT ==========");
  auto wo = rocksdb::WriteOptions();
  for (uint32_t i = 0; i < 10; ++i) {
    std::string key = "k1233456789";
    std::string val = "01233456789012334567890123345678901233456789012334567890123";
    auto start = high_resolution_clock::now();
    db->Put(wo, key, val);
    LOG((high_resolution_clock::now() - start).count());
  }

  LOG("GET ==========");
  auto ro = rocksdb::ReadOptions();
  for (uint32_t i = 0; i < 10; ++i) {
    std::string key = "k1233456789";
    std::string out;
    auto start = high_resolution_clock::now();
    db->Get(ro, key, &out);
    LOG((high_resolution_clock::now() - start).count());
    // LOG(out);
  }

  // // get
  // std::string read_value;
  // auto ro = rocksdb::ReadOptions();
  // ro.total_order_seek = true;
  //
  // db->Get(rocksdb::ReadOptions(), "k", &read_value);
  // db->Get(rocksdb::ReadOptions(), "k", &read_value);
  // LOG(read_value);
  //
  // // update
  // db->Put(rocksdb::WriteOptions(), "k", "v2");
  //
  // // get
  // status = db->Get(rocksdb::ReadOptions(), "k", &read_value);
  // LOG(read_value);
  // LOG(status.IsNotFound());
  //
  // // delete
  // db->SingleDelete(rocksdb::WriteOptions(), "k");
  //
  // status = db->Get(rocksdb::ReadOptions(), "k", &read_value);
  // LOG(status.IsNotFound());

  // // range query
  // db->Put(rocksdb::WriteOptions(), "k1", "v1");
  // db->Put(rocksdb::WriteOptions(), "k3", "v3");
  // db->Put(rocksdb::WriteOptions(), "k2", "v2");
  // db->Put(rocksdb::WriteOptions(), "l1", "l1");
  // db->Put(rocksdb::WriteOptions(), "stop", "stop");

  // status = db->Get(rocksdb::ReadOptions(), "k", &read_value);
  // status = db->Get(rocksdb::ReadOptions(), "k", &read_value);
  //
  // rocksdb::Iterator* it = db->NewIterator(rocksdb::ReadOptions());
  // for (it->Seek("k0"); it->Valid() && it->key().ToString() < "stop";
  //      it->Next()) {
  //   LOG(it->key().ToString() + ":" + it->value().ToString());
  // }

  // db->Close();
  delete db;
  // DestroyDB(db_path, options);
  // rocksdb::DB::Open(options, db_path, &db);
  // delete db;
}
