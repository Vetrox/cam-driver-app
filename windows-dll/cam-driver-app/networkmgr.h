#pragma once

namespace cda {
    HRESULT setup();
    HRESULT cleanup();
    HRESULT recv_img(uint8_t *dst, size_t dst_len);
    HRESULT send_beacon();
}