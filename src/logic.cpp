#include "logic.hpp"
#include "util/murmurhash3.hpp"
#include "util/parse.hpp"

namespace psync {

static const size_t N_HASHCHECK = 11;

Logic::Logic(size_t expectedNumEntries)
: m_iblt(expectedNumEntries)
{

}

Logic::~Logic() {};

void
Logic::addSyncNode(std::string prefix)
{
	if (m_prefixes.find(prefix) == m_prefixes.end())
		m_prefixes[prefix] = 0;	
	prefix += "/" + std::to_string(m_prefixes[prefix]);
	uint32_t hash = MurmurHash3(N_HASHCHECK, ParseHex(prefix));
	m_prefix2hash[prefix] = hash;
	m_hash2prefix[hash] = prefix;
	m_iblt.insert(hash);
}

void
Logic::removeSyncNode(std::string prefix)
{
	if (m_prefixes.find(prefix) != m_prefixes.end()) {
		uint32_t seqNo = m_prefixes[prefix];
		m_prefixes.erase(prefix);
		prefix += "/" + std::to_string(seqNo);
		uint32_t hash = MurmurHash3(N_HASHCHECK, ParseHex(prefix));
		m_prefix2hash.erase(prefix);
		m_hash2prefix.erase(hash);
		m_iblt.erase(hash);
	}
}

}

int main() {
	;
}