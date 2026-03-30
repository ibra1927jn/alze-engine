#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void stbi_set_flip_vertically_on_load(int flag);
const char* stbi_failure_reason();

unsigned char* stbi_load(const char* path, int* w, int* h, int* ch, int req_ch);
float*         stbi_loadf(const char* path, int* w, int* h, int* ch, int req_ch);
void           stbi_image_free(void* data);

unsigned char* stbi_load_from_memory(const unsigned char* buf, int bufLen, int* w, int* h, int* ch, int req_ch);
float*         stbi_loadf_from_memory(const unsigned char* buf, int bufLen, int* w, int* h, int* ch, int req_ch);

#ifdef __cplusplus
}
#endif
