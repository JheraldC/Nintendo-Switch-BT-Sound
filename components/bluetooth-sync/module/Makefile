CC ?= cc
CFLAGS ?= -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror
all: test
build/test_bt_only: src/bt_only.c tests/test_bt_only.c include/bt_only.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude src/bt_only.c tests/test_bt_only.c -o $@
test: build/test_bt_only
	./build/test_bt_only
sanitizers:
	mkdir -p build
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude src/bt_only.c tests/test_bt_only.c -o build/test_bt_only_sanitized
	./build/test_bt_only_sanitized
adapter-test:
	mkdir -p build
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -DBTO_HOST_TEST -Iinclude -Itegra -Itests -Ivendor/core/include src/bt_only.c src/bt_keys.c vendor/core/src/sha256.c tegra/tegra_reader.c tegra/tegra_keys.c tegra/io_denials.c tests/test_tegra_adapter.c -Wl,--wrap=f_open,--wrap=f_write,--wrap=f_unlink,--wrap=f_mkdir,--wrap=f_rename,--wrap=emummc_storage_write,--wrap=sdmmc_storage_write,--wrap=nx_emmc_bis_write,--wrap=emummc_storage_set_mmc_partition -o build/test_tegra_adapter
	./build/test_tegra_adapter
keys-test:
	mkdir -p build
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude src/bt_only.c src/bt_keys.c tests/test_bt_keys.c -o build/test_bt_keys
	./build/test_bt_keys
.PHONY: all test sanitizers adapter-test keys-test
