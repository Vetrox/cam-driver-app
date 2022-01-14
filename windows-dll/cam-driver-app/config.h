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
static const auto& MSUBTYPE = MEDIASUBTYPE_RGB24;//(GUID)FOURCCMap(COMPRESSSION);

constexpr auto NUMBYTES = NUMBITS / 8; // make sure it is a multiple pls
// BI_RGB, BI_BITFIELDS, FCC('YUY2')
const DWORD COMPRESSSION = BI_RGB;

constexpr auto FPS_NANO = 33'3333;
