/**version.h
 *
 * Copyright 2023.
 */

#ifndef VERSION_H
#define VERSION_H

// Macro for version string
#define VERSION_STR_EX(number) #number
#define VERSION_STR(number) VERSION_STR_EX(number)

// Version number
#define VERSION_MAJOR 2
#define VERSION_MINOR 0
#define VERSION_PATCH 0

// Version number final
#define VERSION_NUMBER                                           \
  ((unsigned long)(VERSION_MAJOR << 16) | (VERSION_MINOR << 8) | \
   (VERSION_PATCH))

// Version string
#define VERSION_STRING       \
  VERSION_STR(VERSION_MAJOR) \
  "." VERSION_STR(VERSION_MINOR) "." VERSION_STR(VERSION_PATCH)

#endif  // VERSION_H
