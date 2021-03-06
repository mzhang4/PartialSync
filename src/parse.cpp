#include "parse.hpp"

namespace psync {

std::vector<unsigned char> ParseHex(const std::string& str)
{
  int len = str.length();
  std::vector <unsigned char> vch;

  for (int i = 0; i < len; i++) {
    vch.push_back((unsigned char)str[i]);
  }

  return vch;
}

}