// WimDismCall.cpp

#include "StdAfx.h"

#include <wchar.h>

#include "../../../Common/IntToString.h"
#include "../../../Common/MyCom.h"
#include "../../../Common/Random.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileMapping.h"
#include "../../../Windows/MemoryLock.h"
#include "../../../Windows/ProcessUtils.h"
#include "../../../Windows/Synchronization.h"

#include "../FileManager/StringUtils.h"
#include "../FileManager/RegistryUtils.h"

#include "ZipRegistry.h"
#include "WimDismCall.h"

using namespace NWindows;

#define MY_TRY_BEGIN try {

#define MY_TRY_FINISH } \
  catch(...) { DismErrorMessageHRESULT(E_FAIL); return E_FAIL; }

#define MY_TRY_FINISH_VOID } \
  catch(...) { DismErrorMessageHRESULT(E_FAIL); }

#define k7zGui  "NanaZipG.exe"

// 21.07 : we can disable wildcard
// #define ISWITCH_NO_WILDCARD_POSTFIX "w-"
#define ISWITCH_NO_WILDCARD_POSTFIX

#define kShowDialogSwitch  " -ad"
#define kEmailSwitch  " -seml."
#define kArchiveTypeSwitch  " -t"
#define kIncludeSwitch  " -i" ISWITCH_NO_WILDCARD_POSTFIX
#define kArcIncludeSwitches  " -an -ai" ISWITCH_NO_WILDCARD_POSTFIX
#define kHashIncludeSwitches  kIncludeSwitch
#define kStopSwitchParsing  " --"

static NCompression::CInfo m_RegistryInfo;
extern HWND g_HWND;

UString DismGetQuotedString(const UString &s)
{
  UString s2 ('\"');
  s2 += s;
  s2 += '\"';
  return s2;
}

static void DismErrorMessage(LPCWSTR message)
{
  MessageBoxW(g_HWND, message, L"NanaZip", MB_ICONERROR | MB_OK);
}

static void DismErrorMessageHRESULT(HRESULT res, LPCWSTR s = NULL)
{
  UString s2 = NError::MyFormatMessage(res);
  if (s)
  {
    s2.Add_LF();
    s2 += s;
  }
  DismErrorMessage(s2);
}

static HRESULT DismCall7zGui(const UString &params,
    // LPCWSTR curDir,
    bool waitFinish,
    NSynchronization::CBaseEvent *event)
{
  UString imageName = fs2us(NWindows::NDLL::GetModuleDirPrefix());
  imageName += k7zGui;

  CProcess process;
  const WRes wres = process.Create(imageName, params, NULL); // curDir);
  if (wres != 0)
  {
    HRESULT hres = HRESULT_FROM_WIN32(wres);
    DismErrorMessageHRESULT(hres, imageName);
    return hres;
  }
  if (waitFinish)
    process.Wait();
  else if (event != NULL)
  {
    HANDLE handles[] = { process, *event };
    ::WaitForMultipleObjects(ARRAY_SIZE(handles), handles, FALSE, INFINITE);
  }
  return S_OK;
}

static void DismAddLagePagesSwitch(UString &params)
{
  if (ReadLockMemoryEnable())
  #ifndef UNDER_CE
  if (NSecurity::Get_LargePages_RiskLevel() == 0)
  #endif
    params += " -slp";
}

class DismCRandNameGenerator
{
  CRandom _random;
public:
  DismCRandNameGenerator() { _random.Init(); }
  void GenerateName(UString &s, const char *prefix)
  {
    s += prefix;
    s.Add_UInt32((UInt32)(unsigned)_random.Generate());
  }
};

static HRESULT DismCreateMap(const UStringVector &names,
    CFileMapping &fileMapping, NSynchronization::CManualResetEvent &event,
    UString &params)
{
  size_t totalSize = 1;
  {
    FOR_VECTOR (i, names)
      totalSize += (names[i].Len() + 1);
  }
  totalSize *= sizeof(wchar_t);

  DismCRandNameGenerator random;

  UString mappingName;
  for (;;)
  {
    random.GenerateName(mappingName, "7zMap");
    const WRes wres = fileMapping.Create(PAGE_READWRITE, totalSize, GetSystemString(mappingName));
    if (fileMapping.IsCreated() && wres == 0)
      break;
    if (wres != ERROR_ALREADY_EXISTS)
      return HRESULT_FROM_WIN32(wres);
    fileMapping.Close();
  }

  UString eventName;
  for (;;)
  {
    random.GenerateName(eventName, "7zEvent");
    const WRes wres = event.CreateWithName(false, GetSystemString(eventName));
    if (event.IsCreated() && wres == 0)
      break;
    if (wres != ERROR_ALREADY_EXISTS)
      return HRESULT_FROM_WIN32(wres);
    event.Close();
  }

  params += '#';
  params += mappingName;
  params += ':';
  char temp[32];
  ConvertUInt64ToString(totalSize, temp);
  params += temp;

  params += ':';
  params += eventName;

  LPVOID data = fileMapping.Map(FILE_MAP_WRITE, 0, totalSize);
  if (!data)
    return E_FAIL;
  CFileUnmapper unmapper(data);
  {
    wchar_t *cur = (wchar_t *)data;
    *cur++ = 0; // it means wchar_t strings (UTF-16 in WIN32)
    FOR_VECTOR (i, names)
    {
      const UString &s = names[i];
      unsigned len = s.Len() + 1;
      wmemcpy(cur, (const wchar_t *)s, len);
      cur += len;
    }
  }
  return S_OK;
}

int DismFindRegistryFormat(const UString &name)
{
  FOR_VECTOR (i, m_RegistryInfo.Formats)
  {
    const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[i];
    if (name.IsEqualTo_NoCase(GetUnicodeString(fo.FormatID)))
      return i;
  }
  return -1;
}

int DismFindRegistryFormatAlways(const UString &name)
{
  int index = DismFindRegistryFormat(name);
  if (index < 0)
  {
    NCompression::CFormatOptions fo;
    fo.FormatID = GetSystemString(name);
    index = m_RegistryInfo.Formats.Add(fo);
  }
  return index;
}

HRESULT DismCompressFiles(
    const UString &arcPathPrefix,
    const UString &arcName,
    const UString &arcType,
    bool addExtension,
    const UStringVector &names,
    bool email, bool showDialog, bool waitFinish)
{
  MY_TRY_BEGIN
  UString params ('a');

  CFileMapping fileMapping;
  NSynchronization::CManualResetEvent event;
  params += kIncludeSwitch;
  RINOK(DismCreateMap(names, fileMapping, event, params));

  if (!arcType.IsEmpty() && arcType == L"7z")
  {
    int index;
    params += kArchiveTypeSwitch;
    params += arcType;
    m_RegistryInfo.Load();
    index = DismFindRegistryFormatAlways(arcType);
    if (index >= 0)
    {
      char temp[64];
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];

      if (!fo.Method.IsEmpty())
      {
        params += " -m0=";
        params += fo.Method;
      }

      /* Level = 0 is meaningful */
      if (fo.Level != static_cast<UInt32>(-1))
      {
        params += " -mx=";
        ConvertUInt32ToString(fo.Level, temp);
        params += temp;
      }

      if (fo.Dictionary && fo.Dictionary != static_cast<UInt32>(-1))
      {
        params += " -md=";
        ConvertUInt32ToString(fo.Dictionary, temp);
        params += temp;
        params += "b";
      }

      if (fo.BlockLogSize && fo.BlockLogSize != static_cast<UInt32>(-1))
      {
        params += " -ms=";
        ConvertUInt64ToString(1ULL << fo.BlockLogSize, temp);
        params += temp;
        params += "b";
      }

      if (fo.NumThreads && fo.NumThreads != static_cast<UInt32>(-1))
      {
        params += " -mmt=";
        ConvertUInt32ToString(fo.NumThreads, temp);
        params += temp;
      }

      if (!fo.Options.IsEmpty())
      {
        UStringVector strings;
        SplitString(fo.Options, strings);
        FOR_VECTOR (i, strings)
        {
          params += " -m";
          params += strings[i];
        }
      }
    }
  }

  if (email)
    params += kEmailSwitch;

  if (showDialog)
    params += kShowDialogSwitch;

  DismAddLagePagesSwitch(params);

  if (arcName.IsEmpty())
    params += " -an";

  if (addExtension)
    params += " -saa";
  else
    params += " -sae";

  params += kStopSwitchParsing;
  params.Add_Space();

  if (!arcName.IsEmpty())
  {
    params += DismGetQuotedString(
    // #ifdef UNDER_CE
      arcPathPrefix +
    // #endif
    arcName);
  }

  return DismCall7zGui(params,
      // (arcPathPrefix.IsEmpty()? 0: (LPCWSTR)arcPathPrefix),
      waitFinish, &event);
  MY_TRY_FINISH
}

static void DismExtractGroupCommand(const UStringVector &arcPaths, UString &params, bool isHash)
{
  DismAddLagePagesSwitch(params);
  params += (isHash ? kHashIncludeSwitches : kArcIncludeSwitches);
  CFileMapping fileMapping;
  NSynchronization::CManualResetEvent event;
  HRESULT result = DismCreateMap(arcPaths, fileMapping, event, params);
  if (result == S_OK)
    result = DismCall7zGui(params, false, &event);
  if (result != S_OK)
    DismErrorMessageHRESULT(result);
}

void DismExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog, bool elimDup, UInt32 writeZone)
{
  MY_TRY_BEGIN
  UString params ('x');
  if (!outFolder.IsEmpty())
  {
    params += " -o";
    params += DismGetQuotedString(outFolder);
  }
  if (elimDup)
    params += " -spe";
  if (writeZone != (UInt32)(Int32)-1)
  {
    params += " -snz";
    params.Add_UInt32(writeZone);
  }
  if (showDialog)
    params += kShowDialogSwitch;
  DismExtractGroupCommand(arcPaths, params, false);
  MY_TRY_FINISH_VOID
}


void DismTestArchives(const UStringVector &arcPaths, bool hashMode)
{
  MY_TRY_BEGIN
  UString params ('t');
  if (hashMode)
  {
    params += kArchiveTypeSwitch;
    params += "hash";
  }
  DismExtractGroupCommand(arcPaths, params, false);
  MY_TRY_FINISH_VOID
}


void DismCalcChecksum(const UStringVector &paths,
    const UString &methodName,
    const UString &arcPathPrefix,
    const UString &arcFileName)
{
  MY_TRY_BEGIN

  if (!arcFileName.IsEmpty())
  {
    DismCompressFiles(
      arcPathPrefix,
      arcFileName,
      UString("hash"),
      false, // addExtension,
      paths,
      false, // email,
      false, // showDialog,
      false  // waitFinish
      );
    return;
  }

  UString params ('h');
  if (!methodName.IsEmpty())
  {
    params += " -scrc";
    params += methodName;
    /*
    if (!arcFileName.IsEmpty())
    {
      // not used alternate method of generating file
      params += " -scrf=";
      params += GetQuotedString(arcPathPrefix + arcFileName);
    }
    */
  }
  DismExtractGroupCommand(paths, params, true);
  MY_TRY_FINISH_VOID
}

void DismBenchmark(bool totalMode)
{
  MY_TRY_BEGIN
  UString params ('b');
  if (totalMode)
    params += " -mm=*";
  DismAddLagePagesSwitch(params);
  HRESULT result = DismCall7zGui(params, false, NULL);
  if (result != S_OK)
    DismErrorMessageHRESULT(result);
  MY_TRY_FINISH_VOID
}
