#include "saltysd_core.h"
#include "saltysd_ipc.h"
#include "saltysd_dynamic.h"
#include <switch_min.h>
#include <dirent.h>
#include <stdio.h>

int load_mod(char* path, uint64_t offset, FILE* arc) {
    void* copy_buffer = malloc(0x100);
    FILE* f = fopen(path, "rb");
    if(f) {
        // Set file pointers to start of file and offset respectively
        fseek(f, 0, SEEK_SET);
        fseek(arc, offset, SEEK_SET);

        // Copy in up to 0x100 byte chunks
        uint64_t size;
        do {
            size = fread(copy_buffer, 1, 0x100, f);
            fwrite(copy_buffer, 1, size, arc);
        } while(size == 0x100);

        fclose(f);
    } else {
        SaltySD_printf("SaltySD Mod Installer: Found file '%s', failed to get file handle\n", path, offset);
    }
}

int load_mods(char* path) {
    char* tmp = malloc(0x80);
    DIR *d;
    struct dirent *dir;

    SaltySD_printf("SaltySD Mod Installer: Searching mod dir '%s'...\n", path);
    
    snprintf(tmp, 0x80, "sdmc:/SaltySD/mods/%s", path);

    FILE* f_arc = fopen("sdmc:/atmosphere/titles/01006A800016E000/romfs/data.arc", "r+b");
    if(!f_arc){
        SaltySD_printf("SaltySD Mod Installer: Failed to get file handle to data.arc\n", path);
        return 0;
    }

    d = opendir(tmp);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            char* dot = strrchr(dir->d_name, '.');
            if(dot) {
                uint64_t offset = strtol(dir->d_name, dot, 16);
                if(offset){
                    SaltySD_printf("SaltySD Mod Installer: Found file '%s', offset = %x\n", dir->d_name, offset);
                    snprintf(tmp, 0x80, "sdmc:/SaltySD/mods/%s%s", path, dir->d_name);
                    load_mod(tmp, offset, f_arc);
                } else {
                    SaltySD_printf("SaltySD Mod Installer: Found file '%s', offset not parsable\n", dir->d_name);
                }
            }
        }
        closedir(d);
    }
    free(tmp);
}

int main(int argc, char *argv[])
{
    SaltySD_printf("Mod installer: alive\n");
    load_mods("");

    __libnx_exit(0);
}

