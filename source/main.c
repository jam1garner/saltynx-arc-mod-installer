#include "saltysd_core.h"
#include "saltysd_ipc.h"
#include "saltysd_dynamic.h"
#include <switch_min.h>
#include <dirent.h>
#include <stdio.h>

extern u32 __start__;

static char g_heap[0x8000];

void __libnx_init(void* ctx, Handle main_thread, void* saved_lr);
void __attribute__((weak)) NORETURN __libnx_exit(int rc);
void __nx_exit(int, void*);
void __libc_fini_array(void);
void __libc_init_array(void);

u32 __nx_applet_type = AppletType_None;

Handle orig_main_thread;
void* orig_ctx;
void* orig_saved_lr;

void __libnx_init(void* ctx, Handle main_thread, void* saved_lr) {
	extern char* fake_heap_start;
	extern char* fake_heap_end;

	fake_heap_start = &g_heap[0];
	fake_heap_end   = &g_heap[sizeof g_heap];
	
	orig_ctx = ctx;
	orig_main_thread = main_thread;
	orig_saved_lr = saved_lr;
	
	// Call constructors.
	//void __libc_init_array(void);
	__libc_init_array();
}

void __attribute__((weak)) NORETURN __libnx_exit(int rc) {
	// Call destructors.
	//void __libc_fini_array(void);
	__libc_fini_array();

	SaltySD_printf("SaltySD Plugin: jumping to %p\n", orig_saved_lr);

	__nx_exit(0, orig_saved_lr);
	while (true);
}

int load_mod(char* path, uint64_t offset, FILE* arc) {
    void* copy_buffer = malloc(0x100);
    FILE* f = SaltySDCore_fopen(path, "rb");
    if(f) {
        // Set file pointers to start of file and offset respectively
        int ret = SaltySDCore_fseek(f, 0, SEEK_SET);
        if (ret)
            SaltySD_printf("SaltySD Mod Installer: Failed to seek file with errno %d\n", ret);

        ret = SaltySDCore_fseek(arc, offset, SEEK_SET);
        if (ret)
            SaltySD_printf("SaltySD Mod Installer: Failed to seek offset %ld from start of data.arc with errno %d\n", offset, ret);

        uint64_t offset_sought = SaltySDCore_ftell(arc);
        SaltySD_printf("SaltySD Mod Installer: Current data.arc seek offset: %ld\n", offset_sought);        

        uint64_t total_size = 0;

        // Copy in up to 0x100 byte chunks
        size_t size;
        do {
            size = SaltySDCore_fread(copy_buffer, 1, 0x100, f);
            total_size += size;

            size_t bytes_written = SaltySDCore_fwrite(copy_buffer, 1, size, arc);
        } while(size == 0x100);

        SaltySD_printf("SaltySD Mod Installer: Installed file '%s' with 0x%llx bytes\n", path, total_size);
        offset_sought = SaltySDCore_ftell(arc);
        SaltySD_printf("SaltySD Mod Installer: Current data.arc seek offset: %ld\n", offset_sought);        

        SaltySDCore_fclose(f);
    } else {
        SaltySD_printf("SaltySD Mod Installer: Found file '%s', failed to get file handle\n", path, offset);
    }
    free(copy_buffer);
}

int load_mods(char* path) {
    char* tmp = malloc(0x80);
    DIR *d;
    struct dirent *dir;

    SaltySD_printf("SaltySD Mod Installer: Searching mod dir '%s'...\n", path);
    
    snprintf(tmp, 0x80, "sdmc:/SaltySD/mods/%s", path);

    FILE* f_arc = SaltySDCore_fopen("sdmc:/atmosphere/titles/01006A800016E000/romfs/data.arc", "r+b");
    if(!f_arc){
        SaltySD_printf("SaltySD Mod Installer: Failed to get file handle to data.arc\n", path);
        free(tmp);
        return 0;
    }

    d = SaltySDCore_opendir(tmp);
    if (d)
    {
        SaltySD_printf("SaltySD Mod Installer: Opened mod directory\n");
        while ((dir = SaltySDCore_readdir(d)) != NULL)
        {
            char* dot = strrchr(dir->d_name, '.');
            if(dot) {
                uint64_t offset = strtoull(dir->d_name, NULL, 16);
                if(offset){
                    SaltySD_printf("SaltySD Mod Installer: Found file '%s', offset = %ld\n", dir->d_name, offset);
                    snprintf(tmp, 0x80, "sdmc:/SaltySD/mods/%s%s", path, dir->d_name);
                    load_mod(tmp, offset, f_arc);
                } else {
                    SaltySD_printf("SaltySD Mod Installer: Found file '%s', offset not parsable\n", dir->d_name);
                }
            }
        }
        SaltySDCore_closedir(d);
    } else {
        SaltySD_printf("SaltySD Mod Installer: Failed to open mod directory\n");
    }

    SaltySDCore_fclose(f_arc);
    free(tmp);
}

int main(int argc, char *argv[])
{
    SaltySD_printf("Mod installer: alive\n");
    load_mods("");

    __libnx_exit(0);
}

