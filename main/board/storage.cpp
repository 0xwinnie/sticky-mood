#include "board/storage.h"

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"

namespace board {
namespace {

constexpr const char *kTag = "storage";
constexpr const char *kMount = "/data";
constexpr const char *kPartition = "storage";

wl_handle_t s_wl = WL_INVALID_HANDLE;
bool s_mounted = false;

}  // namespace

bool storage_init()
{
    if (s_mounted) {
        return true;
    }

    esp_vfs_fat_mount_config_t cfg = {};
    // First boot on a blank partition: lay down a FATFS rather than failing.
    cfg.format_if_mount_failed = true;
    cfg.max_files = 4;
    cfg.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;

    const esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(kMount, kPartition, &cfg, &s_wl);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "mount '%s' at %s failed: %s", kPartition, kMount, esp_err_to_name(ret));
        s_wl = WL_INVALID_HANDLE;
        return false;
    }

    s_mounted = true;
    ESP_LOGI(kTag, "mounted '%s' at %s: %llu KB free / %llu KB total", kPartition, kMount,
             storage_free_bytes() / 1024, storage_total_bytes() / 1024);
    return true;
}

void storage_deinit()
{
    if (!s_mounted) {
        return;
    }
    esp_vfs_fat_spiflash_unmount_rw_wl(kMount, s_wl);
    s_wl = WL_INVALID_HANDLE;
    s_mounted = false;
}

bool storage_ready()
{
    return s_mounted;
}

const char *storage_mount_point()
{
    return kMount;
}

unsigned long long storage_free_bytes()
{
    if (!s_mounted) {
        return 0;
    }
    uint64_t total = 0, free_b = 0;
    esp_vfs_fat_info(kMount, &total, &free_b);
    return (unsigned long long)free_b;
}

unsigned long long storage_total_bytes()
{
    if (!s_mounted) {
        return 0;
    }
    uint64_t total = 0, free_b = 0;
    esp_vfs_fat_info(kMount, &total, &free_b);
    return (unsigned long long)total;
}

}  // namespace board
