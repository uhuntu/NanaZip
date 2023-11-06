// WimDismCall.h

#ifndef __WIMDISM_CALL_H
#define __WIMDISM_CALL_H

#include "../../../Common/MyString.h"

UString DismGetQuotedString(const UString &s);

HRESULT DismCompressFiles(
    const UString &arcPathPrefix,
    const UString &arcName,
    const UString &arcType,
    bool addExtension,
    const UStringVector &names,
    bool email, bool showDialog, bool waitFinish);

void DismExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog, bool elimDup, UInt32 writeZone);
void DismTestArchives(const UStringVector &arcPaths, bool hashMode = false);

void InfoWimDism(const UStringVector& arcPaths, bool hashMode = false);

void DismCalcChecksum(const UStringVector &paths,
    const UString &methodName,
    const UString &arcPathPrefix,
    const UString &arcFileName);

void DismBenchmark(bool totalMode);

#endif
