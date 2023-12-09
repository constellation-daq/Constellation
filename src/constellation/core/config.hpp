/**
 * @file
 * @brief Compile-time configuration
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#if(defined _WIN32 && !defined __CYGWIN__)
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
