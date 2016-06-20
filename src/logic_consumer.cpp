#include "logic_consumer.hpp"

#include <ndn-cxx/util/time.hpp>

namespace psync{

LogicConsumer::LogicConsumer(ndn::Name& prefix,
														 ndn::Face& face,
														 UpdateCallback& onUpdate,
														 unsigned int& count,
														 double& false_positve)
: m_syncPrefix(prefix)
, m_face(face)
, m_onUpdate(onUpdate)
, m_count(count)
, m_false_positive(false_positve)
{
	bloom_parameters opt;
  opt.false_positive_probability = m_false_positive;
  opt.projected_element_count = m_count;
  opt.compute_optimal_parameters();
  m_bf = bloom_filter(opt);
}

LogicConsumer::~LogicConsumer()
{
	m_face.shutdown();
}

void
LogicConsumer::stop()
{
	m_face.shutdown();
}

void
LogicConsumer::sendHelloInterest()
{
	ndn::Name helloInterestName = m_syncPrefix;
	helloInterestName.append("hello");

	ndn::Interest helloInterest(helloInterestName);
 	helloInterest.setInterestLifetime(ndn::time::milliseconds(1000));
  helloInterest.setMustBeFresh(true);

  m_face.expressInterest(helloInterest,
                           bind(&LogicConsumer::onHelloData, this, _1, _2),
                           bind(&LogicConsumer::onHelloTimeout, this, _1));
}

void
LogicConsumer::sendSyncInterest()
{
	// name last component is the IBF and content should be the prefix with the version numbers
	assert(m_helloSent);
	assert(!m_iblt.empty());

	ndn::Name syncInterestName = m_syncPrefix;
	appendBF(syncInterestName);
	syncInterestName.append(m_iblt);

	ndn::Interest syncInterest(syncInterestName);
 	syncInterest.setInterestLifetime(ndn::time::milliseconds(1000));
  syncInterest.setMustBeFresh(true);

  m_face.expressInterest(syncInterest,
                           bind(&LogicConsumer::onSyncData, this, _1, _2),
                           bind(&LogicConsumer::onSyncTimeout, this, _1));
}

void
LogicConsumer::fetchData(const ndn::Name& sessionName, const uint32_t& seq)
{

	ndn::Name interestName;
  interestName.append(sessionName).append(std::to_string(seq));

  ndn::Interest interest(interestName);
  interest.setMustBeFresh(true);

  m_face.expressInterest(interest,
                         bind(&LogicConsumer::onData, this, _1, _2),
                         bind(&LogicConsumer::onDataTimeout, this, _1));
}

bool
LogicConsumer::haveSentHello()
{
	return m_helloSent;
}

std::set <std::string>
LogicConsumer::getSL()
{
	return m_sl;
}

void
LogicConsumer::addSL(std::string s)
{
	m_sl.insert(s);
	m_bf.insert(s);
}

std::vector <std::string>
LogicConsumer::getNS()
{
	return m_ns;
}

void
LogicConsumer::onHelloData(const ndn::Interest& interest, const ndn::Data& data)
{
	ndn::Name helloDataName = data.getName();
	m_iblt = helloDataName.getSubName(helloDataName.size()-1, 1);
	std::string content(reinterpret_cast<const char*>(data.getContent().value()),
                        data.getContent().value_size());
	std::stringstream ss(content);
	std::string prefix;
	uint32_t seq;

	while (ss >> prefix >> seq) {
		m_prefixes[prefix] = seq;
		m_ns.push_back(prefix);
	}

	m_helloSent = true;
}

void
LogicConsumer::onSyncData(const ndn::Interest& interest, const ndn::Data& data)
{
	ndn::Name syncDataName = data.getName();
	m_iblt = syncDataName.getSubName(syncDataName.size() - 1, 1);

	std::string content(reinterpret_cast<const char*>(data.getContent().value()),
                        data.getContent().value_size());

	std::stringstream ss(content);
	std::string prefix;
	uint32_t seq;
	std::vector <MissingData> updates;

	while (ss >> prefix >> seq) {
		if (m_prefixes.find(prefix) == m_prefixes.end() || m_prefixes[prefix] < seq) {
			updates.push_back(MissingData(prefix, m_prefixes[prefix], seq));
			m_prefixes[prefix] = seq;
		}
	}

	m_onUpdate(updates);

	this->sendSyncInterest();
}

void
LogicConsumer::onHelloTimeout(const ndn::Interest& interest)
{
  m_face.expressInterest(interest,
                           bind(&LogicConsumer::onSyncData, this, _1, _2),
                           bind(&LogicConsumer::onSyncTimeout, this, _1));
}

void
LogicConsumer::onSyncTimeout(const ndn::Interest& interest)
{
  m_face.expressInterest(interest,
                           bind(&LogicConsumer::onSyncData, this, _1, _2),
                           bind(&LogicConsumer::onSyncTimeout, this, _1));
}

void
LogicConsumer::appendBF(ndn::Name& name)
{
	name.append(std::to_string(m_count) + "%" + std::to_string((int)(m_false_positive*1000)));
	name.append(m_bf.table().begin(), m_bf.table().end());
}

void
LogicConsumer::onData(const ndn::Interest& interest, const ndn::Data& data)
{
	// on data
}

void
LogicConsumer::onDataTimeout(const ndn::Interest interest)
{
  m_face.expressInterest(interest,
                         bind(&LogicConsumer::onData, this, _1, _2),
                         bind(&LogicConsumer::onDataTimeout, this, _1));
}

}