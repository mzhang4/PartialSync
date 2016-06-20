#include <iostream>
#include <cstring>

#include <ndn-cxx/common.hpp>

#include "logic_repo.hpp"
#include "murmurhash3.hpp"
#include "parse.hpp"

namespace psync {

static const size_t N_HASHCHECK = 11;

LogicRepo::LogicRepo(size_t expectedNumEntries, 
                     ndn::Face& face,
                     ndn::Name& prefix,
                     ndn::time::milliseconds helloReplyFreshness,
                     ndn::time::milliseconds syncReplyFreshness)
: m_iblt(expectedNumEntries)
, m_expectedNumEntries(expectedNumEntries)
, m_threshold(expectedNumEntries/2)
, m_face(face)
, m_syncPrefix(prefix)
, m_scheduler(m_face.getIoService())
, m_helloReplyFreshness(helloReplyFreshness)
, m_syncReplyFreshness(syncReplyFreshness)
{
  ndn::Name helloName = m_syncPrefix;
  helloName.append("hello");
  m_face.setInterestFilter(helloName,
                             bind(&LogicRepo::onHelloInterest, this, _1, _2),
                             bind(&LogicRepo::onSyncRegisterFailed, this, _1, _2));

  m_face.setInterestFilter(m_syncPrefix,
                             bind(&LogicRepo::onSyncInterest, this, _1, _2),
                             bind(&LogicRepo::onSyncRegisterFailed, this, _1, _2));
}

LogicRepo::~LogicRepo()
{
  m_face.shutdown();
};

void
LogicRepo::addSyncNode(std::string prefix)
{
  if (m_prefixes.find(prefix) == m_prefixes.end())
    m_prefixes[prefix] = 0; 
  std::string prefixWithSeq = prefix + "/" + std::to_string(m_prefixes[prefix]);
  uint32_t hash = MurmurHash3(N_HASHCHECK, ParseHex(prefixWithSeq));
  m_prefix2hash[prefixWithSeq] = hash;
  m_hash2prefix[hash] = prefix;
  m_iblt.insert(hash);

  m_face.setInterestFilter(prefix,
                           bind(&LogicRepo::onInterest, this, _1, _2),
                           [] (const ndn::Name& prefix, const std::string& msg) {});
}

void
LogicRepo::removeSyncNode(std::string prefix)
{
  if (m_prefixes.find(prefix) != m_prefixes.end()) {
    uint32_t seqNo = m_prefixes[prefix];
    m_prefixes.erase(prefix);
    std::string prefixWithSeq = prefix + "/" + std::to_string(seqNo);
    uint32_t hash = m_prefix2hash[prefixWithSeq];
    m_prefix2hash.erase(prefixWithSeq);
    m_hash2prefix.erase(hash);
    m_iblt.erase(hash);
  }
}

void
LogicRepo::publishData(const ndn::Block& content, const ndn::time::milliseconds& freshness, 
                      std::string& prefix)
{
  if (m_prefixes.find(prefix) == m_prefixes.end()) {
    std::cout << "Cannot Publish Data Under an Unknow prefix" << std::endl;
    return;
  }

  ndn::shared_ptr<ndn::Data> data = ndn::make_shared<ndn::Data>();
  data->setContent(content);
  data->setFreshnessPeriod(freshness);

  uint32_t newSeq = m_prefixes[prefix] + 1;
  ndn::Name dataName;
  dataName.append(ndn::Name(prefix).appendNumber(newSeq));
  data->setName(dataName);
  m_keyChain.sign(*data);
  m_ims.insert(*data);

  uint32_t hash = m_prefix2hash[prefix + "/" + std::to_string(m_prefixes[prefix])];
  m_prefix2hash.erase(prefix + "/" + std::to_string(m_prefixes[prefix]));
  m_hash2prefix.erase(hash);

  std::string prefixWithSeq = prefix + "/" + std::to_string(m_prefixes[prefix]);
  uint32_t newHash = MurmurHash3(N_HASHCHECK, ParseHex(prefixWithSeq));
  m_prefix2hash[prefixWithSeq] = newHash;
  m_hash2prefix[newHash] = prefix;
  m_iblt.insert(newHash);

  for (auto pendingInterest : m_pendingEntries) {
    // go through each pendingEntries
    PendingEntryInfo entry = pendingInterest.second; 
    IBLT diff = m_iblt - entry.iblt;
    std::set<uint32_t> positive;
    std::set<uint32_t> negative;

    if (!diff.listEntries(positive, negative)) {
      this->sendNack(pendingInterest.first);
      m_pendingEntries.erase(pendingInterest.first);
      m_scheduler.cancelEvent(entry.expirationEvent);
      return;
    }

    if (entry.bf.contains(prefix) || positive.size() + negative.size() >= m_threshold) {
      // generate sync data and cancel the scheduler
      ndn::Name syncDataName = pendingInterest.first;
      appendIBLT(syncDataName);
      std::string syncContent;
      if (entry.bf.contains(prefix)) {
        syncContent = prefix + " " + std::to_string(m_prefixes[prefix]);
      }
      ndn::shared_ptr<ndn::Data> syncData = ndn::make_shared<ndn::Data>(syncDataName);
      syncData->setContent(reinterpret_cast<const uint8_t*>(syncContent.c_str()), syncContent.length());
      syncData->setFreshnessPeriod(m_syncReplyFreshness);
      m_face.put(*syncData);
      m_pendingEntries.erase(pendingInterest.first);
      m_scheduler.cancelEvent(entry.expirationEvent);
    }
  }
}

void
LogicRepo::onInterest(const ndn::Name& prefix, const ndn::Interest& interest)
{
  ndn::shared_ptr<const ndn::Data>data = m_ims.find(interest);
  if (static_cast<bool>(data)) {
    m_face.put(*data);
  }
}

void
LogicRepo::onHelloInterest(const ndn::Name& prefix, const ndn::Interest& interest)
{
  // generate hello data with NO_CACHE
  std::string content;
  for (auto p : m_prefixes) {
    content += p.first + " " + std::to_string(p.second) + "\n";
  }

  ndn::shared_ptr<ndn::Data> data = ndn::make_shared<ndn::Data>();
  ndn::Name helloInterestName = prefix;
  appendIBLT(helloInterestName);
  data->setName(helloInterestName);
  data->setFreshnessPeriod(m_helloReplyFreshness);
  data->setContent(reinterpret_cast<const uint8_t*>(content.c_str()), content.length());
  data->setCachingPolicy(ndn::lp::LocalControlHeaderFacade::CachingPolicy::NO_CACHE);

  m_keyChain.sign(*data);
  m_face.put(*data);
}

void
LogicRepo::onSyncInterest(const ndn::Name& prefix, const ndn::Interest& interest)
{
  // parser BF and IBLT Not finished yet
  ndn::Name interestName = interest.getName();
  std::string parameters = interestName.getSubName(interestName.size()-3, 1).toUri();
  ndn::Block bfName = interestName.get(interestName.size()-2).wireEncode();
  ndn::Block ibltName = interestName.get(interestName.size()-1).wireEncode();

  std::size_t idx = parameters.find("%");

  bloom_parameters opt;
  opt.false_positive_probability = std::stoi(parameters.substr(1, idx-1))/1000.;
  opt.projected_element_count = std::stoi(parameters.substr(idx+1));
  opt.compute_optimal_parameters();
  bloom_filter bf(opt);
  bf.setTable(std::vector <uint8_t>(bfName.begin(), bfName.end()));

  std::vector <uint8_t> ibltValues(ibltName.begin(), ibltName.end());
  std::size_t N = ibltValues.size()/4;
  std::vector <uint32_t> values(N, 0);

  for (int i = 0; i < N; i += 4) {
    uint32_t t = (ibltValues[i+3] << 24) + (ibltValues[i+2] << 16) + (ibltValues[i+1] << 8) + ibltValues[i];
    values[i/4] = t; 
  }

  IBLT iblt(m_expectedNumEntries, values);
  
  // get the difference
  IBLT diff = m_iblt - iblt;
  std::set<uint32_t> positive;
  std::set<uint32_t> negative;

  if (!diff.listEntries(positive, negative)) {
      this->sendNack(interest);
      return;
  }

  // generate content in Sync reply
  std::string content;
  for (auto hash : positive) {
    std::string prefix = m_hash2prefix[hash];
    if (bf.contains(prefix)) {
      // generate data
      content += prefix + " " + std::to_string(m_prefixes[prefix]);
    }
  }

  if (positive.size() + negative.size() >= m_threshold || !content.empty()) {
    // send back data
    ndn::shared_ptr<ndn::Data> data = ndn::make_shared<ndn::Data>();
    ndn::Name syncDataName = prefix;
    appendIBLT(syncDataName);
    data->setName(syncDataName);
    data->setFreshnessPeriod(m_syncReplyFreshness);
    data->setContent(reinterpret_cast<const uint8_t*>(content.c_str()), content.length());

    m_keyChain.sign(*data);
    m_face.put(*data);
    return;
  }
  
  // add the entry to the pending entry
  PendingEntryInfo entry(bf, iblt);
  m_pendingEntries.insert(std::map<ndn::Name, PendingEntryInfo>::value_type(interest.getName(), entry));
  m_pendingEntries.find(interest.getName())->second.expirationEvent = m_scheduler.scheduleEvent(interest.getInterestLifetime(), 
                                                [=] () { m_pendingEntries.erase(interest.getName()); });
}

void
LogicRepo::onSyncRegisterFailed(const ndn::Name& prefix, const std::string& msg)
{
  std::cout << ">> Logic::onSyncRegisterFailed" << std::endl;
}

void
LogicRepo::appendIBLT(ndn::Name& name)
{
  std::vector <HashTableEntry> hashTable = m_iblt.getHashTable();
  size_t N = hashTable.size();
  size_t unitSize = sizeof(HashTableEntry::count) + sizeof(HashTableEntry::keySum) + sizeof(HashTableEntry::keyCheck);
  size_t tableSize = unitSize/8*N;
  int k = sizeof(HashTableEntry::count)/sizeof(uint8_t);

  uint8_t* table = new uint8_t(tableSize);

  for (int i = 0; i < N; i++) {
    std::memcpy(table+i*3*k, &hashTable[i].count, sizeof(HashTableEntry::count));
    std::memcpy(table+(i*3+1)*k, &hashTable[i].keySum, sizeof(HashTableEntry::keySum));
    std::memcpy(table+(i*3+2)*k, &hashTable[i].keyCheck, sizeof(HashTableEntry::keyCheck));
  }

  name.append(table, tableSize);
  delete table;
}

void
LogicRepo::sendNack(const ndn::Interest interest)
{
  std::string content = "NACK 0";
  ndn::shared_ptr<ndn::Data> data = ndn::make_shared<ndn::Data>();
  data->setName(interest.getName());
  data->setFreshnessPeriod(m_syncReplyFreshness);
  data->setContent(reinterpret_cast<const uint8_t*>(content.c_str()), content.length());
  m_face.put(*data);
}

}