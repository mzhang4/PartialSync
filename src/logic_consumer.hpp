#ifndef LOGIC_CONSUMER_HPP
#define LOGIC_CONSUMER_HPP

#include <utility>
#include <map>
#include <vector>
#include <functional>

#include "bloom_filter.hpp"

#include <ndn-cxx/common.hpp>
#include <ndn-cxx/face.hpp>

namespace psync{

struct MissingData
{
  MissingData(std::string prefix, uint32_t seq1, uint32_t seq2)
  :prefix(prefix)
  ,seq1(seq1)
  ,seq2(seq2)
  {
  }

  std::string prefix;
  uint32_t seq1;
  uint32_t seq2;
};

typedef std::function<void(const std::vector<MissingData>)> UpdateCallback;
typedef std::function<void()> RecieveHelloCallback;

class LogicConsumer
{
public:
  LogicConsumer(ndn::Name& prefix,
                ndn::Face& face,
                RecieveHelloCallback& onRecieveHelloData,
                UpdateCallback& onUpdate,
                unsigned int count,
                double false_postive);

  ~LogicConsumer();

  void stop();

  void sendHelloInterest();
  void sendSyncInterest();
  void fetchData(const ndn::Name& sessionName, const uint32_t& seq);

  bool haveSentHello();
  std::set <std::string> getSL();
  void addSL(std::string s);
  std::vector <std::string> getNS();
  bool isSub(std::string prefix) {
    return m_suball || m_sl.find(prefix) != m_sl.end();
  }

  void setSeq(std::string prefix, const uint32_t& seq) {
    m_prefixes[prefix] = seq;
  }

  uint32_t getSeq(std::string prefix) {
    return m_prefixes[prefix];
  }

private:
  void onHelloData(const ndn::Interest& interest, const ndn::Data& data);
  void onSyncData(const ndn::Interest& interest, const ndn::Data& data);
  void onHelloTimeout(const ndn::Interest& interest);
  void onSyncTimeout(const ndn::Interest& interest);
  void onData(const ndn::Interest& interest, const ndn::Data& data);
  void onDataTimeout(const ndn::Interest interest);
  void appendBF(ndn::Name& name);

private:
  ndn::Name m_syncPrefix;
  ndn::Face& m_face;
  RecieveHelloCallback m_onRecieveHelloData;
  UpdateCallback m_onUpdate;
  unsigned int m_count;
  double m_false_positive;
  bool m_suball;
  ndn::Name m_iblt;
  std::map <std::string, uint32_t> m_prefixes;
  bool m_helloSent;
  std::set <std::string> m_sl;
  std::vector <std::string> m_ns;
  bloom_filter m_bf;
};

}

# endif