#include "pgm.h"

/*
IGS Asic3 emulation - Rust bridge

Core logic migrated to Rust (src/pgm_asic3.rs)
Used by: Oriental Legends
*/

// ============================================================================
// Rust FFI 声明
// ============================================================================

extern "C" {
	// 生命周期
	void *asic3Init();
	void asic3Exit(void *handle);
	void asic3Reset(void *handle);

	// 设置全局实例（供 M68K handler 回调使用）
	void asic3SetInstance(void *handle);

	// M68K handler 兼容的读写回调
	UINT16 __fastcall asic3ReadWord(UINT32 address);
	void __fastcall asic3WriteWord(UINT32 address, UINT16 data);

	// 存档扫描
	INT32 asic3Scan(void *handle);
}

// ============================================================================
// 本地状态
// ============================================================================

static void *asic3_handle = NULL;

// ============================================================================
// 回调封装
// ============================================================================

static void reset_asic3() { asic3Reset(asic3_handle); }

static INT32 asic3ScanCallback(INT32 nAction, INT32 *) {
	if (nAction & ACB_DRIVER_DATA) {
		return asic3Scan(asic3_handle);
	}
	return 0;
}

// ============================================================================
// 安装保护
// ============================================================================

void install_protection_asic3_orlegend() {
	// 如果之前有实例则释放
	if (asic3_handle) {
		asic3Exit(asic3_handle);
	}

	asic3_handle = asic3Init();
	asic3SetInstance(asic3_handle);

	pPgmScanCallback = asic3ScanCallback;
	pPgmResetCallback = reset_asic3;

	SekOpen(0);
	SekMapHandler(4, 0xc04000, 0xc0400f, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, asic3ReadWord);
	SekSetWriteWordHandler(4, asic3WriteWord);
	SekClose();
}
