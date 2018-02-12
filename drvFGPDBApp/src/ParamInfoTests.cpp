#include "gmock/gmock.h"

#include "ParamInfo.h"

using namespace testing;
using namespace std;

//-----------------------------------------------------------------------------
TEST(joinMapKeys, returnsEmptyStringForEmptyMap) {
  const map<string, int> aMap;
  const string separator("--");

  string result = joinMapKeys<int>(aMap, separator);

  ASSERT_THAT(result, Eq(""));
}

//-----------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------
TEST(construct, createNewParam)  {
  ParamInfo param("lcpRegRO_1 0x10002 Int32 U32", __func__);

  ostringstream stream;
  stream << param;
  ASSERT_THAT(stream.str(), Eq("lcpRegRO_1 0x10002 Int32 U32"));
}

//-----------------------------------------------------------------------------
TEST(construct, rejectAndReturnDefConfigForInvalidDef)  {
  ParamInfo param("lcpRegRO_1 0x10002 Int32 X32", __func__);

  ASSERT_THAT(param.regAddr, Eq(0));
  ASSERT_THAT(param.asynType, Eq(asynParamNotDefined));
}

//-----------------------------------------------------------------------------
TEST(conversions, convertsCtlDataFmtToString)  {
  auto strDesc = ParamInfo::ctlrFmtToStr(CtlrDataFmt::U16_16);
  ASSERT_THAT(strDesc, Eq("U16_16"));

  strDesc = ParamInfo::ctlrFmtToStr(CtlrDataFmt::NotDefined);
  ASSERT_THAT(strDesc, Eq("<NotDefined>"));
}

//-----------------------------------------------------------------------------
TEST(conversions, convertsStringToCtlDataFmt)  {
  auto ctlrFmt = ParamInfo::strToCtlrFmt("invalid");
  ASSERT_THAT(ctlrFmt, Eq(CtlrDataFmt::NotDefined));

  ctlrFmt = ParamInfo::strToCtlrFmt("U16_16");
  ASSERT_THAT(ctlrFmt, Eq(CtlrDataFmt::U16_16));
}

//-----------------------------------------------------------------------------
TEST(conversions, convertsAsynTypeToString)  {
  auto strDesc = ParamInfo::asynTypeToStr(asynParamFloat64);
  ASSERT_THAT(strDesc, Eq("Float64"));

  strDesc = ParamInfo::asynTypeToStr(asynParamNotDefined);
  ASSERT_THAT(strDesc, Eq("<NotDefined>"));
}

//-----------------------------------------------------------------------------
TEST(conversions, convertsStringToAsynType)  {
  auto asynType = ParamInfo::strToAsynType("Float64");
  ASSERT_THAT(asynType, Eq(asynParamFloat64));

  asynType = ParamInfo::strToAsynType("invalid");
  ASSERT_THAT(asynType, Eq(asynParamNotDefined));
}

//-----------------------------------------------------------------------------
TEST(conversions, convertsSetStateToString)  {
  ParamInfo param("lcpRegRO_1 0x10002 Int32 U32", __func__);
  param.setState = SetState::Pending;
  ASSERT_THAT(param.setStateToStr(), Eq("Pending"));
}

//-----------------------------------------------------------------------------
TEST(conversions, convertsReadStateToString)  {
  ParamInfo param("lcpRegRO_1 0x10002 Int32 U32", __func__);
  param.readState = ReadState::Current;
  ASSERT_THAT(param.readStateToStr(), Eq("Current"));
}

//-----------------------------------------------------------------------------
TEST(conversions, failsOnConflictingUpdate)  {
  ParamInfo param1("lcpRegRO_1 0x10002 Int32 U32", __func__);
  ParamInfo param2("lcpRegRO_1 0x10002 Float64 U32", __func__);

  auto stat = param1.updateParamDef(__func__, param2);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST(conversions, returns0ForNotDefinedSrcFmt)  {
  double dval = ParamInfo::ctlrFmtToDouble((uint32_t)0xFFFFFFF0, CtlrDataFmt::NotDefined);
  ASSERT_THAT(dval, DoubleEq(0.0));
}
TEST(conversions, convertsCtlrS32toDouble)  {
  double dval = ParamInfo::ctlrFmtToDouble((uint32_t)0xFFFFFFF0, CtlrDataFmt::S32);
  ASSERT_THAT(dval, DoubleEq(-16.0));
}
TEST(conversions, convertsCtlrU32toDouble)  {
  double dval = ParamInfo::ctlrFmtToDouble((uint32_t)0x0000000F, CtlrDataFmt::U32);
  ASSERT_THAT(dval, DoubleEq(15.0));
}
TEST(conversions, convertsCtlrF32toDouble)  {
  double dval = ParamInfo::ctlrFmtToDouble((uint32_t)0x3FA00000, CtlrDataFmt::F32);
  ASSERT_THAT(dval, DoubleEq(1.25));
}
TEST(conversions, convertsCtlrU16_16toDouble)  {
  double dval = ParamInfo::ctlrFmtToDouble((uint32_t)0x00028000, CtlrDataFmt::U16_16);
  ASSERT_THAT(dval, DoubleEq(2.5));
}

//-----------------------------------------------------------------------------
TEST(conversions, returns0ForNotDefinedTargetFmt)  {
  uint32_t cval = ParamInfo::doubleToCtlrFmt(-16.0, CtlrDataFmt::NotDefined);
  ASSERT_THAT(cval, Eq(0x00000000));
}
TEST(conversions, convertsDoubleToS32Fmt)  {
  uint32_t cval = ParamInfo::doubleToCtlrFmt(-16.0, CtlrDataFmt::S32);
  ASSERT_THAT(cval, Eq(0xFFFFFFF0));
}
TEST(conversions, convertsDoubleToU32Fmt)  {
  uint32_t cval = ParamInfo::doubleToCtlrFmt(15.0, CtlrDataFmt::U32);
  ASSERT_THAT(cval, Eq(0x0000000F));
}
TEST(conversions, convertsDoubleToF32Fmt)  {
  uint32_t cval = ParamInfo::doubleToCtlrFmt(1.25, CtlrDataFmt::F32);
  ASSERT_THAT(cval, Eq(0x3FA00000));
}
TEST(conversions, convertsDoubleToU16_16Fmt)  {
  uint32_t cval = ParamInfo::doubleToCtlrFmt(2.5, CtlrDataFmt::U16_16);
  ASSERT_THAT(cval, Eq(0x00028000));
}
