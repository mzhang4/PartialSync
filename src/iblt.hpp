#ifndef IBLT_H
#define IBLT_H

#include <inttypes.h>
#include <set>
#include <vector>
#include <string>

namespace psync {

class HashTableEntry
{
public:
  int32_t count;
  uint32_t keySum;
  uint32_t keyCheck;

  bool isPure() const;
  bool empty() const;
};

class IBLT
{
public:
  IBLT(size_t _expectedNumEntries);
  IBLT(size_t _expectedNumEntries, std::vector <uint32_t> values);
  virtual ~IBLT();

  void insert(uint32_t key);
  void erase(uint32_t key);
  bool listEntries(std::set<uint32_t>& positive, std::set<uint32_t>& negative);
  IBLT operator-(const IBLT& other) const;

  std::vector <HashTableEntry>
  getHashTable()
  {
    return hashTable; 
  }

  std::size_t
  getNumEntry() {
    return hashTable.size();
  }

public:
  // for debugging
  std::string DumpTable() const;

private:
  void _insert(int plusOrMinus, uint32_t key);
  std::vector<HashTableEntry> hashTable;
};

}

#endif