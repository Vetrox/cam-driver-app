#pragma once
#include <uuids.h>
#include <wingdi.h>
#include <aviriff.h>

constexpr auto CAMERA_NAME = L"cam-driver-app";
// CAM_CLSID_STR = "{8E14549A-DB61-4309-AFA1-3578E927E933}" see dll.h

// 4*180 = 720
constexpr auto CAM_WIDTH = 1280;
constexpr auto CAM_HEIGHT = 720;

constexpr auto NUMBITS = 24;
// BI_RGB, BI_BITFIELDS, FCC('YUY2')
const DWORD COMPRESSSION = BI_RGB;
static const auto& MSUBTYPE = MEDIASUBTYPE_RGB24;//(GUID)FOURCCMap(COMPRESSSION);

constexpr auto FPS_MIN = 30; // CHANGE AVGTIMEperFRAME to match this fps
constexpr auto FPS_MAX = 30;
