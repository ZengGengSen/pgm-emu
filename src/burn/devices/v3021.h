/* v3021 Calendar Emulation */

typedef struct V3021 V3021; // 这是一个不透明指针

#ifdef __cplusplus
extern "C" {
#endif

V3021* v3021Init();
void v3021Write(V3021* handle, UINT16 data);
UINT8 v3021Read(V3021* handle);
void v3021Exit(V3021* handle);
INT32 v3021Scan(V3021* handle);

#ifdef __cplusplus
}
#endif
