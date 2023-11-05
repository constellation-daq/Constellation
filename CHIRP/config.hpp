#pragma once

#if (defined _WIN32 && !defined __CYGWIN__)
#if CHIRP_BUILDLIB
#define CHIRP_API __declspec(dllexport)
#else
#define CHIRP_API __declspec(dllimport)
#endif
#else
#if CHIRP_BUILDLIB
#define CHIRP_API __attribute__((__visibility__("default")))
#else
#define CHIRP_API
#endif
#endif
