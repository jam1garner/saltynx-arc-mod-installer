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

uint64_t install_file(FILE* to, FILE* from) {
    void* copy_buffer = malloc(0x100);
    uint64_t total_size = 0;

    // Copy in up to 0x100 byte chunks
    size_t size;
    do {
        size = SaltySDCore_fread(copy_buffer, 1, 0x100, from);
        total_size += size;

        SaltySDCore_fwrite(copy_buffer, 1, size, to);
    } while(size == 0x100);

    free(copy_buffer);

    return total_size;
}

int seek_files(FILE* f, uint64_t offset, FILE* arc) {
    // Set file pointers to start of file and offset respectively
    int ret = SaltySDCore_fseek(f, 0, SEEK_SET);
    if (ret) {
        SaltySD_printf("SaltySD Mod Installer: Failed to seek file with errno %d\n", ret);
        return ret;
    }

    ret = SaltySDCore_fseek(arc, offset, SEEK_SET);
    if (ret) {
        SaltySD_printf("SaltySD Mod Installer: Failed to seek offset %llx from start of data.arc with errno %d\n", offset, ret);
        return ret;
    }

    return 0;
}

int load_mod(char* path, uint64_t offset, FILE* arc) {
    FILE* f = SaltySDCore_fopen(path, "rb");
    if(f) {
        int ret = seek_files(f, offset, arc);
        if (!ret) {
            uint64_t total_size = install_file(arc, f);
            SaltySD_printf("SaltySD Mod Installer: Installed file '%s' with 0x%llx bytes\n", path, total_size);
        }

        SaltySDCore_fclose(f);
    } else {
        SaltySD_printf("SaltySD Mod Installer: Found file '%s', failed to get file handle\n", path, offset);
        return -1;
    }

    return 0;
}

int create_backup(char* path, uint64_t offset, FILE* arc) {
    FILE* f = SaltySDCore_fopen(path, "wb");
    if(f) {
        int ret = seek_files(f, offset, arc);
        if (!ret) {
            uint64_t total_size = install_file(f, arc);
            SaltySD_printf("SaltySD Mod Installer: Created backup '%s' with 0x%llx bytes\n", path, total_size);
        }

        SaltySDCore_fclose(f);
    } else {
        SaltySD_printf("SaltySD Mod Installer: Attempted to create backup file '%s', failed to get file handle\n", path, offset);
        return -1;
    }

    return 0;
}

#define UC(c) ((unsigned char)c)

char _isxdigit (unsigned char c)
{
    if (( c >= UC('0') && c <= UC('9') ) ||
        ( c >= UC('a') && c <= UC('f') ) ||
        ( c >= UC('A') && c <= UC('F') ))
        return 1;
    return 0;
}

unsigned char xtoc(char x) {
    if (x >= UC('0') && x <= UC('9'))
        return x - UC('0');
    else if (x >= UC('a') && x <= UC('f'))
        return (x - UC('a')) + 0xa;
    else if (x >= UC('A') && x <= UC('F'))
        return (x - UC('A')) + 0xA;
    return -1;
}

uint64_t hex_to_u64(char* str) {
    if(str[0] == '0' && str[1] == 'x')
        str += 2;
    uint64_t value = 0;
    while(_isxdigit(*str)) {
        value *= 0x10;
        value += xtoc(*str);
        str++;
    }
    return value;
}

int load_mods(FILE* f_arc, char* mod_dir) {
    const int filename_size = 0x120;
    char* tmp = malloc(filename_size);
    DIR *d;
    struct dirent *dir;

    SaltySD_printf("SaltySD Mod Installer: Searching mod dir '%s'...\n", mod_dir);
    
    snprintf(tmp, filename_size, "sdmc:/SaltySD/%s/", mod_dir);

    d = SaltySDCore_opendir(tmp);
    if (d)
    {
        SaltySD_printf("SaltySD Mod Installer: Opened mod directory\n");
        while ((dir = SaltySDCore_readdir(d)) != NULL)
        {
            char* dot = strrchr(dir->d_name, '.');
            if(dot) {
                uint64_t offset = hex_to_u64(dir->d_name);
                if(offset){
                    SaltySD_printf("SaltySD Mod Installer: Found file '%s', offset = %ld\n", dir->d_name, offset);
                    if (strcmp(mod_dir, "backups") == 0) {
                        snprintf(tmp, filename_size, "sdmc:/SaltySD/backups/%s", dir->d_name);
                        load_mod(tmp, offset, f_arc);

                        SaltySDCore_remove(tmp);
                    } else {
                        snprintf(tmp, filename_size, "sdmc:/SaltySD/backups/%s", dir->d_name);
                        create_backup(tmp, offset, f_arc);

                        snprintf(tmp, filename_size, "sdmc:/SaltySD/mods/%s", dir->d_name);
                        load_mod(tmp, offset, f_arc);
                    }
                } else {
                    SaltySD_printf("SaltySD Mod Installer: Found file '%s', offset not parsable\n", dir->d_name);
                }
            }
        }
        SaltySDCore_closedir(d);
    } else {
        SaltySD_printf("SaltySD Mod Installer: Failed to open mod directory\n");
    }

    free(tmp);
    return 0;
}

int main(int argc, char *argv[])
{
    SaltySD_printf("Mod installer: alive\n");

    FILE* f_arc = SaltySDCore_fopen("sdmc:/atmosphere/titles/01006A800016E000/romfs/data.arc", "r+b");
    if(!f_arc){
        SaltySD_printf("SaltySD Mod Installer: Failed to get file handle to data.arc\n");
        return 0;
    }

    // restore backups -> delete backups -> make backups for current mods -> install current mods
    load_mods(f_arc, "backups");
    load_mods(f_arc, "mods");

    SaltySDCore_fclose(f_arc);

    __libnx_exit(0);
}
