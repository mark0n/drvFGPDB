#include "gmock/gmock.h"

#include "ParamInfo.h"

using namespace testing;
using namespace std;

TEST(joinMapKeys, returnsEmptyStringForEmptyMap) {
  const map<string, int> aMap;
  const string separator("--");

  string result = joinMapKeys<int>(aMap, separator);

  ASSERT_THAT(result, Eq(""));
}

TEST(joinMapKeys, concatenatesMapKeysAndSeparators) {
  const int arbitraryInt = 0;
  const string key1("KEY1");
  const string key2("KEY2");
  const string key3("KEY3");
  const map<string, int> aMap = {
    { key1, arbitraryInt },
    { key2, arbitraryInt },
    { key3, arbitraryInt }
  };
  const string separator("--");

  string result = joinMapKeys<int>(aMap, separator);

  ASSERT_THAT(result, Eq(key1 + separator + key2 + separator + key3));
}