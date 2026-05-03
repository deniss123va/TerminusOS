#include "cmd_meminfo.h"
#include "../lib/screen.h"
#include "../drivers/fat32.h"
#include "../drivers/disk.h"

void cmd_meminfo() {
    println("--- FAT32 Memory Info ---");

    uint32_t total_clusters = 0;
    uint32_t free_clusters  = 0;

    // Читаем первую FAT таблицу сектор за сектором
    for (uint32_t sec = 0; sec < FAT32_SECTORS_PER_FAT; sec++) {
        ata_read_sector(FAT32_FAT_START + sec);
        uint32_t* entries = (uint32_t*)sector_buffer;
        for (int i = 0; i < FAT32_ENTRIES_PER_FAT_SECTOR; i++) {
            uint32_t cluster_idx = sec * FAT32_ENTRIES_PER_FAT_SECTOR + i;
            if (cluster_idx < 2) continue; // первые две записи — служебные
            total_clusters++;
            if ((entries[i] & FAT32_MASK) == FAT32_FREE)
                free_clusters++;
        }
    }

    uint32_t used_clusters = total_clusters - free_clusters;
    uint32_t bytes_per_cluster = FAT32_SECTORS_PER_CLUSTER * FAT32_BYTES_PER_SECTOR;

    // Печать числа — вспомогательная лямбда через inline
    auto print_u32 = [](uint32_t n) {
        char buf[12]; int i = 0;
        if (n == 0) { print_char('0'); return; }
        char tmp[12]; int t = 0;
        while (n > 0) { tmp[t++] = (char)('0' + n % 10); n /= 10; }
        for (int j = t - 1; j >= 0; j--) print_char(tmp[j]);
    };

    print("Total clusters : "); print_u32(total_clusters); print_char('\n');
    print("Used  clusters : "); print_u32(used_clusters);  print_char('\n');
    print("Free  clusters : "); print_u32(free_clusters);  print_char('\n');
    print("Cluster size   : "); print_u32(bytes_per_cluster); println(" bytes");
    print("Free space     : "); print_u32(free_clusters * (bytes_per_cluster / 1024)); println(" KB");
    print("Used space     : "); print_u32(used_clusters * (bytes_per_cluster / 1024)); println(" KB");
    println("--- End ---");
}