/*
 * Copyright QTronic GmbH. All rights reserved.
 */

/* ---------------------------------------------------------------------------*
 * xmlVersionParser.h
 * Parse a xml model description of a FMI 1.0 or FMI 2.0 model to extract its
 * fmi version.
 *
 * Author: Adrian Tirea
 * ---------------------------------------------------------------------------*/

#ifndef xmlVersionParser_h
#define xmlVersionParser_h
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#pragma comment(lib, "libxml2.lib")
#pragma comment(lib, "wsock32.lib")
#endif /* _MSC_VER */

char *extractVersion(const char *xmlDescriptionPath);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif
#endif // xmlVersionParser_h
