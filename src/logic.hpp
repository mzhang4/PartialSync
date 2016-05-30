#ifndef LOGIC_HPP
#define LOGIC_HPP

#include "util/iblt.hpp"
#include <map>

namespace psync {

class Logic {
public:
	Logic(size_t _expectedNumEntries);
  ~Logic();

  void
  addSyncNode(std::string prefix);

  void
  removeSyncNode(std::string prefix);

private:
  //void
  //onHelloInterest(const Name& prefix, const Interest& interest);

  /*void
  onSyncInterest(const Name& prefix, const Interest& interest);

  void
  onSyncData(const Interest& interest, Data& data);

  void
  onSyncRegisterFailed(const Name& prefix, const std::string& msg);

  void
  onSyncTimeout(const Interest& interest);

  void
  publishData(const Block& content, const ndn::time::milliseconds& freshness, 
  		const Name& prefix);
  		*/

private:
	IBLT m_iblt;
	std::map <std::string, uint32_t> m_prefixes; // prefix and sequence number
	std::map <std::string, uint32_t> m_prefix2hash; 
	std::map <uint32_t, std::string> m_hash2prefix;
};

}

#endif