#pragma once
#include <uuids.h>
#include <wingdi.h>
#include <aviriff.h>

// "{8E14549A-DB61-4309-AFA1-3578E927E933}" see dll.h
constexpr auto CAMERA_NAME = L"cam-driver-app";

constexpr auto CAM_WIDTH = 1280;
constexpr auto CAM_HEIGHT = 720;

// BI_RGB, BI_BITFIELDS, FCC('YUY2')
const DWORD COMPRESSSION = BI_RGB;
constexpr auto NUMBITS = 24;
constexpr auto NUMBYTES = NUMBITS / 8; // make sure it is a multiple pls
static const auto& MSUBTYPE = MEDIASUBTYPE_RGB24; //(GUID)FOURCCMap(COMPRESSSION);

constexpr auto FPS_NANO = 33'3333;