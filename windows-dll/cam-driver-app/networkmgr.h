#pragma once

namespace cda {
    HRESULT setup();
    HRESULT cleanup();
    HRESULT recv_img(uint8_t *dst);
    HRESULT send_beacon();
}