#pragma once

struct RGBImg {
    unsigned char* buf = 0;
    int width, height;
};

extern RGBImg* buffered_image;
extern bool running;
void setup();
void setup_beacon();