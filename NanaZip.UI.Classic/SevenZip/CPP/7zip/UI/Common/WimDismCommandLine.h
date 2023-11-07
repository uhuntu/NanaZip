// WimDismCommandLine.h

#ifndef __WIMDISM_COMMAND_LINE_H
#define __WIMDISM_COMMAND_LINE_H

#include "../../../Common/CommandLineParser.h"
#include "../../../Common/Wildcard.h"

#include "EnumDirItems.h"

#include "Extract.h"
#include "HashCalc.h"
#include "Update.h"

typedef CMessagePathException DismCArcCmdLineException;

namespace NDismCommandType { enum EEnum
{
  kDismAdd = 0,
  kDismUpdate,
  kDismDelete,
  kDismTest,
  kDismExtract,
  kDismExtractFull,
  kDismList,
  kDismBenchmark,
  kDismInfo,
  kDismHash,
  kDismRename
};}

struct CWimCommand
{
  NDismCommandType::EEnum CommandType;

  bool IsFromExtractGroup() const;
  bool IsFromWimDismGroup() const;
  bool IsFromUpdateGroup() const;
  bool IsTestCommand() const { return CommandType == NDismCommandType::kDismTest; }
  NExtract::NPathMode::EEnum GetPathMode() const;
};

enum
{
  k_DismOutStream_disabled = 0,
  k_DismOutStream_stdout = 1,
  k_DismOutStream_stderr = 2
};

struct CWimCmdLineOptions
{
  bool HelpMode;

  // bool LargePages;
  bool CaseSensitive_Change;
  bool CaseSensitive;

  bool IsInTerminal;
  bool IsStdOutTerminal;
  bool IsStdErrTerminal;
  bool StdInMode;
  bool StdOutMode;
  bool EnableHeaders;

  bool YesToAll;
  bool ShowDialog;
  bool TechMode;
  bool ShowTime;

  AString ListFields;

  int ConsoleCodePage;

  NWildcard::CCensor Censor;

  CWimCommand Command;
  UString ArchiveName;

  #ifndef _NO_CRYPTO
  bool PasswordEnabled;
  UString Password;
  #endif

  UStringVector HashMethods;
  // UString HashFilePath;

  bool AppendName;
  // UStringVector ArchivePathsSorted;
  // UStringVector ArchivePathsFullSorted;
  NWildcard::CCensor arcCensor;
  UString ArcName_for_StdInMode;

  CObjectVector<CProperty> Properties;

  CExtractOptionsBase ExtractOptions;

  CBoolPair NtSecurity;
  CBoolPair AltStreams;
  CBoolPair HardLinks;
  CBoolPair SymLinks;

  CBoolPair StoreOwnerId;
  CBoolPair StoreOwnerName;

  CUpdateOptions UpdateOptions;
  CHashOptions HashOptions;
  UString ArcType;
  UStringVector ExcludedArcTypes;

  unsigned Number_for_Out;
  unsigned Number_for_Errors;
  unsigned Number_for_Percents;
  unsigned LogLevel;

  // bool IsOutAllowed() const { return Number_for_Out != k_OutStream_disabled; }

  // Benchmark
  UInt32 NumIterations;
  bool NumIterations_Defined;

  CWimCmdLineOptions():
      HelpMode(false),
      // LargePages(false),
      CaseSensitive_Change(false),
      CaseSensitive(false),

      IsInTerminal(false),
      IsStdOutTerminal(false),
      IsStdErrTerminal(false),

      StdInMode(false),
      StdOutMode(false),

      EnableHeaders(false),

      YesToAll(false),
      ShowDialog(false),
      TechMode(false),
      ShowTime(false),

      ConsoleCodePage(-1),

      Number_for_Out(k_DismOutStream_stdout),
      Number_for_Errors(k_DismOutStream_stderr),
      Number_for_Percents(k_DismOutStream_stdout),

      LogLevel(0)
  {
  };
};

class CWimCmdLineParser
{
  NCommandLineParser::CParser parser;
public:
  UString Parse1Log;
  void Parse1(const UStringVector &commandStrings, CWimCmdLineOptions &options);
  void Parse2(CWimCmdLineOptions&options);
};

#endif
