//----------------------------------------------------------------------
//
//			File:			"ver.h"
//			Created:		19-Dec-2025
//			Author:			N.Tsuda
//			Description:
//
//----------------------------------------------------------------------

#pragma once

#define		VER_STR_0		"ver 0.3.038 dev"

#ifdef BUILD_WITH_CMAKE
#define		VER_STR			QString(VER_STR_0" (CMake ver)")
#else
#define		VER_STR			QString(VER_STR_0" (VS tools ver)")
#endif


