#pragma once

struct RGBImg {
    unsigned char* buf = 0;
    int width = 0, height = 0;
};

extern RGBImg* buffered_image;
extern bool running;

namespace cda {
    HRESULT setup();
    HRESULT cleanup();
    int send_beacon();
}