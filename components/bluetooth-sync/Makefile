CC ?= cc
CFLAGS = -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
policy-test:
	mkdir -p output
	$(CC) $(CFLAGS) -Iinclude src/nand_policy.c tests/test_gpt_v2.c -o output/test_gpt_v2
	./output/test_gpt_v2
native-test:
	mkdir -p output
	$(CC) $(CFLAGS) -DBTD_HOST_TEST -Iinclude -Itests -Imodule/include src/nand_policy.c src/native_storage.c module/src/bt_only.c tests/test_native_storage.c -o output/test_native_storage
	./output/test_native_storage
report-test:
	mkdir -p output
	$(CC) $(CFLAGS) -Iinclude src/diag_report.c tests/test_report.c -o output/test_report
	./output/test_report
headers-test:
	mkdir -p output
	$(CC) $(CFLAGS) -Iinclude -Imodule/include -Imodule/vendor/core/include src/header_probe.c module/src/bt_only.c module/vendor/core/src/sha256.c tests/test_header_probe.c -o output/test_header_probe
	./output/test_header_probe
adapter-test:
	mkdir -p output
	$(CC) $(CFLAGS) -DBTO_HOST_TEST -DBTO_DIAGNOSTIC -Iinclude -Imodule/include -Imodule/tegra -Imodule/tests -Imodule/vendor/core/include src/header_probe.c module/vendor/core/src/sha256.c module/src/bt_only.c module/src/bt_keys.c module/tegra/tegra_reader.c module/tegra/tegra_keys.c module/tegra/io_denials.c module/tests/test_tegra_adapter.c -Wl,--wrap=f_open,--wrap=f_write,--wrap=f_unlink,--wrap=f_mkdir,--wrap=f_rename,--wrap=emummc_storage_write,--wrap=sdmmc_storage_write,--wrap=nx_emmc_bis_write,--wrap=emummc_storage_set_mmc_partition -o output/test_tegra_adapter
	./output/test_tegra_adapter
CORE = module/vendor/core
CORE_INCLUDES = -Iinclude -I$(CORE)/include -I$(CORE)/src
CORE_BASE = $(CORE)/src/reader.c $(CORE)/src/storage.c $(CORE)/src/sha256.c
CORE_COMMON = $(CORE_BASE) $(CORE)/src/filesystem.c
ivfc-test:
	mkdir -p output
	python3 tests/core_corpus.py
	python3 tests/generate_ivfc_vectors.py
	$(CC) $(CFLAGS) $(CORE_INCLUDES) $(CORE_COMMON) $(CORE)/src/integrity.c tests/test_ivfc_legacy.c -o output/test_ivfc_legacy
	./output/test_ivfc_legacy
	$(CC) $(CFLAGS) $(CORE_INCLUDES) src/ivfc_salt.c $(CORE)/src/sha256.c tests/test_ivfc_hmac.c -o output/test_ivfc_hmac
	./output/test_ivfc_hmac
	$(CC) $(CFLAGS) $(CORE_INCLUDES) $(CORE_BASE) src/save_filesystem.c src/ivfc_salt.c src/ivfc_bridge.c tests/test_core_regression.c -Wl,--wrap=malloc -o output/test_core_regression
	./output/test_core_regression
	$(CC) $(CFLAGS) -DBTD_LEGACY_FS $(CORE_INCLUDES) $(CORE_COMMON) src/ivfc_salt.c src/ivfc_bridge.c tests/test_fs_geometry.c -o output/test_fs_legacy
	./output/test_fs_legacy
	$(CC) $(CFLAGS) $(CORE_INCLUDES) $(CORE_BASE) src/save_filesystem.c src/ivfc_salt.c src/ivfc_bridge.c tests/test_fs_geometry.c -o output/test_fs_geometry
	./output/test_fs_geometry
	$(CC) $(CFLAGS) -DBTD_V4_REGRESSION $(CORE_INCLUDES) $(CORE_BASE) tests/v4_filesystem.c src/ivfc_salt.c src/ivfc_bridge.c tests/test_console_geometry.c -o output/test_console_geometry_v4
	./output/test_console_geometry_v4
	$(CC) $(CFLAGS) $(CORE_INCLUDES) $(CORE_BASE) src/save_filesystem.c src/ivfc_salt.c src/ivfc_bridge.c tests/test_console_geometry.c -o output/test_console_geometry
	./output/test_console_geometry
bt-probe-test:
	mkdir -p output
	$(CC) $(CFLAGS) -Iinclude -Imodule/include -Imodule/vendor/core/include src/bt_probe.c src/transfer.c module/src/bt_only.c tests/v6_bt_probe.c tests/test_bt_probe.c -Wl,--wrap=bto_wipe -o output/test_bt_probe
	./output/test_bt_probe
test: bt-probe-test policy-test native-test report-test headers-test adapter-test ivfc-test
.PHONY: test policy-test native-test report-test headers-test adapter-test ivfc-test

fatfs-map-test:
	$(CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections -Wl,--gc-sections -include tests/fatfs_host_types.h '-DFFCFG_INC="source/libs/fatfs/ffconf.h"' -Ivendor/tegraexplorer/bdk -Ivendor/tegraexplorer tests/test_fatfs_mapping.c vendor/tegraexplorer/bdk/libs/fatfs/ff.c vendor/tegraexplorer/bdk/libs/fatfs/ffunicode.c -o output/test_fatfs_mapping
	./output/test_fatfs_mapping



auto-test:
	$(CC) $(CFLAGS) -Iinclude -Imodule/vendor/core/src src/transfer.c src/auto_sync.c src/auto_transaction.c module/vendor/core/src/sha256.c tests/test_auto_sync.c -o output/test_auto_sync
	./output/test_auto_sync

auto-platform-test:
	python3 tests/make_auto_fatfs.py
	$(CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections -Wl,--gc-sections -include tests/fatfs_host_types.h '-DFFCFG_INC="source/libs/fatfs/ffconf.h"' -Itests -Iinclude -Imodule/vendor/core/src -Ivendor/tegraexplorer/bdk -Ivendor/tegraexplorer tests/test_auto_platform.c src/transfer.c src/auto_sync.c src/auto_transaction.c module/vendor/core/src/sha256.c vendor/tegraexplorer/bdk/libs/fatfs/ff.c vendor/tegraexplorer/bdk/libs/fatfs/ffunicode.c -Wl,--wrap=f_open,--wrap=f_write,--wrap=f_unlink,--wrap=f_rename -o output/test_auto_platform
	./output/test_auto_platform

auto-boot-test:
	$(CC) $(CFLAGS) -Iinclude -Imodule/vendor/core/src src/transfer.c src/auto_boot_config.c module/vendor/core/src/sha256.c tests/test_auto_boot.c -o output/test_auto_boot
	./output/test_auto_boot
