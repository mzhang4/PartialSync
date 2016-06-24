#include <cassert>
#include <sstream>
#include <iostream>

#include "iblt.hpp"
#include "murmurhash3.hpp"

namespace psync {

static const size_t N_HASH = 3;
static const size_t N_HASHCHECK = 11;

template<typename T>
std::vector<unsigned char> ToVec(T number)
{
  std::vector<unsigned char> v(sizeof(T));
  
  for (size_t i = 0; i < sizeof(T); i++) {
    v.at(i) = (number >> i*8) & 0xff;
  }
  
  return v;
}

bool 
HashTableEntry::isPure() const
{
  if (count == 1 || count == -1) {
    uint32_t check = MurmurHash3(N_HASHCHECK, ToVec(keySum));
    return (keyCheck == check);
  }
  
  return false;
}

bool 
HashTableEntry::empty() const
{
  return (count == 0 && keySum == 0 && keyCheck == 0);
}

IBLT::IBLT(size_t _expectedNumEntries)
{
  // 1.5x expectedNumEntries gives very low probability of
  // decoding failure
  size_t nEntries = _expectedNumEntries + _expectedNumEntries/2;
  // ... make nEntries exactly divisible by N_HASH
  while (N_HASH * (nEntries/N_HASH) != nEntries)
    ++nEntries;

  hashTable.resize(nEntries);
}

IBLT::IBLT(const IBLT& other)
{
    hashTable = other.hashTable;
}

IBLT::IBLT(size_t _expectedNumEntries, std::vector <uint32_t> values)
{
  size_t nEntries = _expectedNumEntries + _expectedNumEntries/2;
  while (N_HASH * (nEntries/N_HASH) != nEntries)
    ++nEntries;
  hashTable.resize(nEntries);

  assert(3 * hashTable.size() == values.size());

  for (size_t i = 0; i < hashTable.size(); i++) {
    HashTableEntry& entry = hashTable.at(i);
    if (values[i*3] != 0)
    entry.count = values[i*3];
    entry.keySum = values[i*3+1];
    entry.keyCheck = values[i*3+2];
  }
}

IBLT::~IBLT()
{
}

void 
IBLT::_insert(int plusOrMinus, uint32_t key)
{
  std::vector<uint8_t> kvec = ToVec(key);

  size_t bucketsPerHash = hashTable.size()/N_HASH;
  for (size_t i = 0; i < N_HASH; i++) {
    size_t startEntry = i*bucketsPerHash;
    uint32_t h = MurmurHash3(i, kvec);
    HashTableEntry& entry = hashTable.at(startEntry + (h%bucketsPerHash));
    entry.count += plusOrMinus;
    entry.keySum ^= key;
    entry.keyCheck ^= MurmurHash3(N_HASHCHECK, kvec);
  }
}

void
IBLT::insert(uint32_t key)
{
  _insert(1, key);
}

void
IBLT::erase(uint32_t key)
{
  _insert(-1, key);
}

bool 
IBLT::listEntries(std::set<uint32_t>& positive, std::set<uint32_t>& negative)
{
  IBLT peeled = *this;

  size_t nErased = 0;
  do {
    nErased = 0;
    for (size_t i = 0; i < peeled.hashTable.size(); i++) {
      HashTableEntry& entry = peeled.hashTable.at(i);
      if (entry.isPure()) {
        if (entry.count == 1) {
          positive.insert(entry.keySum);
        }
        else {
          negative.insert(entry.keySum);
        }
        peeled._insert(-entry.count, entry.keySum);
        ++nErased;
      }
    }
  } while (nErased > 0);

  // If any buckets for one of the hash functions is not empty,
  // then we didn't peel them all:
  for (size_t i = 0; i < peeled.hashTable.size(); i++) {
    if (peeled.hashTable.at(i).empty() != true) {
      return false;
    }
  }

  return true;
}

IBLT 
IBLT::operator-(const IBLT& other) const
{
  assert(hashTable.size() == other.hashTable.size());

  IBLT result(*this);
  for (size_t i = 0; i < hashTable.size(); i++) {
    HashTableEntry& e1 = result.hashTable.at(i);
    const HashTableEntry& e2 = other.hashTable.at(i);
    e1.count -= e2.count;
    e1.keySum ^= e2.keySum;
    e1.keyCheck ^= e2.keyCheck;
  }

  return result;
}

bool
IBLT::operator==(const IBLT& other) const
{
  if (this->hashTable.size() != other.hashTable.size())
    return false;

  int N = this->hashTable.size();

  for (size_t i = 0; i < N; i++) {
    if (this->hashTable[i].count != other.hashTable[i].count ||
        this->hashTable[i].keySum != other.hashTable[i].keySum ||
        this->hashTable[i].keyCheck != other.hashTable[i].keyCheck)
      return false;
  }

  return true;
}

std::string
IBLT::DumpTable() const
{
  std::ostringstream result;

  result << "count keySum keyCheckMatch\n";
  for (size_t i = 0; i < hashTable.size(); i++) {
    const HashTableEntry& entry = hashTable.at(i);
    result << entry.count << " " << entry.keySum << " ";
    result << ((MurmurHash3(N_HASHCHECK, ToVec(entry.keySum)) == entry.keyCheck) || 
              (entry.empty())? "true" : "false");
    result << "\n";
  }

  return result.str();
}

}

/*#include <iostream>
#include <map>
#include "parse.hpp"

using namespace std;

int main()
{
  IBLT iblt1(80);
  IBLT iblt2(80);

  map <uint32_t, string> hashToValue;
  map <string, uint32_t> valueToHash;

  for (int i = 0; i < 40; i++) {
    string s = "/a/b/" + to_string(i);
    uint32_t hash = MurmurHash3(N_HASHCHECK, ParseHex(s));
    hashToValue[hash] = s;
    valueToHash[s] = hash;
    if (i & 0x1)
      iblt1.insert(hash);
    else
      iblt2.insert(hash);
  }

  IBLT iblt = iblt1-iblt2;

  std::set<uint32_t> positive, negative;
  assert(iblt.listEntries(positive, negative));

  cout << "positive:" << endl;
  for (auto p : positive) {
    cout << hashToValue[p] << endl;
  }

  cout << "negative:" << endl;
  for (auto n : negative) {
    cout << hashToValue[n] << endl;
  }
}*/
