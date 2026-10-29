#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include "fat.h"
#include "fs.h"
#include "ata.h"
#include "block_cache.h"
#include "klog.h"

#define SECTOR_SIZE 512
#define CHECK(expr) do { if (!(expr)) fail(__LINE__, #expr); } while (0)

static FILE* disk;
static uint64_t disk_sectors;
static int selected_drive;
static int checks;

static void fail(int line, const char* expression) {
    fprintf(stderr, "FAIL line %d: %s\n", line, expression);
    exit(1);
}

static uint16_t get16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t get32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void put16(uint8_t* p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t* p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }

void ata_init(void) {}
int ata_is_available(void) { return 1; }
uint32_t ata_get_sector_count(void) { return (uint32_t)disk_sectors; }
void ata_select_drive(int slave) { selected_drive = slave; }

int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    if (selected_drive || lba >= disk_sectors || fseeko(disk, (off_t)lba * SECTOR_SIZE, SEEK_SET) != 0) return -1;
    return fread(buffer, 1, SECTOR_SIZE, disk) == SECTOR_SIZE ? 0 : -1;
}

int ata_write_sector(uint32_t lba, uint8_t* buffer) {
    if (selected_drive || lba >= disk_sectors || fseeko(disk, (off_t)lba * SECTOR_SIZE, SEEK_SET) != 0) return -1;
    if (fwrite(buffer, 1, SECTOR_SIZE, disk) != SECTOR_SIZE) return -1;
    fflush(disk); return 0;
}

void klog(log_level_t level, const char* message) { (void)level; (void)message; }
void klog_info(const char* message) { (void)message; }
void klog_warn(const char* message) { (void)message; }
void klog_error(const char* message) { (void)message; }
void klog_debug(const char* message) { (void)message; }

typedef struct {
    fat_type_t type;
    uint8_t spc, fats;
    uint16_t reserved, root_entries;
    uint32_t total, fat_size, root_sectors, first_data, clusters, root_cluster;
} layout_t;

static void read_sector(uint32_t lba, uint8_t* data) { CHECK(ata_read_sector(lba, data) == 0); checks++; }
static void write_sector(uint32_t lba, uint8_t* data) { CHECK(ata_write_sector(lba, data) == 0); checks++; }

static layout_t layout(void) {
    uint8_t b[512]; layout_t l; read_sector(0, b);
    l.spc=b[13]; l.reserved=get16(b+14); l.fats=b[16]; l.root_entries=get16(b+17);
    l.total=get16(b+19); if (!l.total) l.total=get32(b+32);
    l.fat_size=get16(b+22); if (!l.fat_size) l.fat_size=get32(b+36);
    l.root_sectors=((uint32_t)l.root_entries*32+511)/512;
    l.first_data=l.reserved+l.fats*l.fat_size+l.root_sectors;
    l.clusters=(l.total-l.first_data)/l.spc;
    l.type=l.clusters<4085?FAT_TYPE_12:(l.clusters<65525?FAT_TYPE_16:FAT_TYPE_32);
    l.root_cluster=l.type==FAT_TYPE_32?(get32(b+44)&0x0FFFFFFF):0;
    return l;
}

static uint32_t raw_fat_get(const layout_t* l, uint32_t cluster) {
    uint8_t a[512], b[512];
    uint32_t off=l->type==FAT_TYPE_12?cluster+cluster/2:(l->type==FAT_TYPE_16?cluster*2:cluster*4);
    uint32_t sec=l->reserved+off/512, pos=off%512; read_sector(sec,a);
    if (l->type==FAT_TYPE_12) {
        uint16_t v; if (pos==511) { read_sector(sec+1,b); v=a[pos]|((uint16_t)b[0]<<8); } else v=a[pos]|((uint16_t)a[pos+1]<<8);
        return cluster&1?v>>4:v&0xFFF;
    }
    if (l->type==FAT_TYPE_16) return get16(a+pos);
    return get32(a+pos)&0x0FFFFFFF;
}

static void raw_fat_set_one(const layout_t* l, uint32_t base, uint32_t cluster, uint32_t value) {
    uint8_t a[512], b[512];
    uint32_t off=l->type==FAT_TYPE_12?cluster+cluster/2:(l->type==FAT_TYPE_16?cluster*2:cluster*4);
    uint32_t sec=base+off/512, pos=off%512; read_sector(sec,a);
    if (l->type==FAT_TYPE_12) {
        uint16_t old;
        if (pos==511) { read_sector(sec+1,b); old=a[pos]|((uint16_t)b[0]<<8); old=cluster&1?(old&15)|(value<<4):(old&0xF000)|(value&0xFFF); a[pos]=(uint8_t)old; b[0]=(uint8_t)(old>>8); write_sector(sec+1,b); }
        else { old=a[pos]|((uint16_t)a[pos+1]<<8); old=cluster&1?(old&15)|(value<<4):(old&0xF000)|(value&0xFFF); put16(a+pos,old); }
    } else if (l->type==FAT_TYPE_16) put16(a+pos,(uint16_t)value);
    else put32(a+pos,(get32(a+pos)&0xF0000000)|(value&0x0FFFFFFF));
    write_sector(sec,a);
}

static void raw_fat_set(const layout_t* l, uint32_t cluster, uint32_t value) {
    for (uint32_t i=0;i<l->fats;i++) raw_fat_set_one(l,l->reserved+i*l->fat_size,cluster,value);
}

static int raw_is_eoc(const layout_t* l, uint32_t c) { return c >= (l->type==FAT_TYPE_12?0xFF8:(l->type==FAT_TYPE_16?0xFFF8:0x0FFFFFF8)); }

static void sfn(const char* input, char out[11]) {
    int i=0,p=0; for(i=0;i<11;i++) out[i]=' '; i=0;
    while(input[p]&&input[p]!='.'&&i<8){char c=input[p++];out[i++]=c>='a'&&c<='z'?c-32:c;}
    while(input[p]&&input[p]!='.')p++;
    if(input[p]=='.')p++;
    i=8;
    while(input[p]&&i<11){char c=input[p++];out[i++]=c>='a'&&c<='z'?c-32:c;}
}

static uint32_t raw_root_entry_cluster(const layout_t* l, const char* name) {
    char wanted[11]; uint8_t sec[512]; uint32_t c=l->root_cluster; sfn(name,wanted);
    for(int index=0;index<512;index++) {
        uint32_t sector, offset;
        if(l->type!=FAT_TYPE_32){if((uint32_t)index>=l->root_entries)break;sector=l->reserved+l->fats*l->fat_size+(uint32_t)index/16;offset=((uint32_t)index%16)*32;}
        else { if(index && ((uint32_t)index%(l->spc*16)==0)){c=raw_fat_get(l,c);if(raw_is_eoc(l,c))break;} uint32_t in=(uint32_t)index%(l->spc*16);sector=l->first_data+(c-2)*l->spc+in/16;offset=(in%16)*32; }
        read_sector(sector,sec); if(sec[offset]==0)break; if(sec[offset]==0xE5||sec[offset+11]==0x0F)continue;
        int match=1;for(int j=0;j<11;j++)if((char)sec[offset+j]!=wanted[j])match=0;
        if(match)return get16(sec+offset+26)|(l->type==FAT_TYPE_32?((uint32_t)get16(sec+offset+20)<<16):0);
    }
    return 0;
}

static void fill_pattern(uint8_t* p, uint32_t n, uint32_t seed) { for(uint32_t i=0;i<n;i++)p[i]=(uint8_t)((i*37+seed*13)^(i>>3)); }
static int equal_bytes(const uint8_t* a,const uint8_t* b,uint32_t n){for(uint32_t i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}

static void exercise(fat_type_t expected) {
    layout_t l=layout(); CHECK(l.type==expected); fat_init(); CHECK(fat_available()); CHECK(fat_get_type()==expected);
    uint32_t cluster_bytes=l.spc*512, big_size=cluster_bytes*37+123, got=0;
    uint8_t* input=malloc(big_size),*output=malloc(big_size); CHECK(input&&output); fill_pattern(input,big_size,7);
    uint32_t total0,used0,free0,total1,used1,free1; fat_get_disk_info(&total0,&used0,&free0);
    CHECK(fat_create("BIGFILE.BIN",(char*)input,big_size)==0); CHECK(fat_read("BIGFILE.BIN",(char*)output,&got,big_size)==0); CHECK(got==big_size); CHECK(equal_bytes(input,output,big_size));
    uint32_t first=raw_root_entry_cluster(&l,"BIGFILE.BIN"); CHECK(first>=2); CHECK(fat_delete("BIGFILE.BIN")==0);
    fat_get_disk_info(&total1,&used1,&free1); CHECK(total1==total0); CHECK(used1==used0); CHECK(free1==free0);
    CHECK(fat_create("REUSED.BIN",(char*)input,big_size)==0); CHECK(raw_root_entry_cluster(&l,"REUSED.BIN")==first); CHECK(fat_delete("REUSED.BIN")==0);

    CHECK(fat_mkdir("MANY")==0); CHECK(fat_chdir("MANY")==0);
    char name[16]; uint8_t data[96],readback[96];
    for(int i=0;i<96;i++){snprintf(name,sizeof(name),"F%06d.DAT",i);fill_pattern(data,sizeof(data),(uint32_t)i);CHECK(fat_create(name,(char*)data,sizeof(data))==0);}
    void* entries=0;int count=0;CHECK(fat_list(&entries,&count)==0);CHECK(count==98);
    for(int i=0;i<96;i+=11){snprintf(name,sizeof(name),"F%06d.DAT",i);got=0;fill_pattern(data,sizeof(data),(uint32_t)i);CHECK(fat_read(name,(char*)readback,&got,sizeof(readback))==0);CHECK(got==sizeof(data));CHECK(equal_bytes(data,readback,sizeof(data)));}
    for(int i=0;i<96;i+=2){snprintf(name,sizeof(name),"F%06d.DAT",i);CHECK(fat_delete(name)==0);}
    for(int i=0;i<48;i++){snprintf(name,sizeof(name),"R%06d.DAT",i);fill_pattern(data,sizeof(data),(uint32_t)(i+200));CHECK(fat_create(name,(char*)data,sizeof(data))==0);}
    CHECK(fat_list(&entries,&count)==0);CHECK(count==98);
    fat_init();CHECK(fat_chdir("MANY")==0);got=0;fill_pattern(data,sizeof(data),247);CHECK(fat_read("R000047.DAT",(char*)readback,&got,sizeof(readback))==0);CHECK(got==sizeof(data));CHECK(equal_bytes(data,readback,sizeof(data)));
    free(input);free(output);checks+=30;
}

static void corrupt(fat_type_t expected) {
    layout_t l=layout(); fat_init(); CHECK(fat_get_type()==expected); uint32_t bytes=l.spc*512, size=bytes*4; uint8_t* data=malloc(size),* out=malloc(size);uint32_t got,first;CHECK(data&&out);fill_pattern(data,size,91);
    CHECK(fat_create("LOOP.BIN",(char*)data,size)==0);first=raw_root_entry_cluster(&l,"LOOP.BIN");CHECK(first>=2);raw_fat_set(&l,first,first);block_cache_invalidate();got=0;CHECK(fat_read("LOOP.BIN",(char*)out,&got,size)!=0);
    CHECK(fat_create("SHORT.BIN",(char*)data,size)==0);first=raw_root_entry_cluster(&l,"SHORT.BIN");CHECK(first>=2);raw_fat_set(&l,first,l.type==FAT_TYPE_12?0xFFF:(l.type==FAT_TYPE_16?0xFFFF:0x0FFFFFFF));block_cache_invalidate();got=0;CHECK(fat_read("SHORT.BIN",(char*)out,&got,size)!=0);
    CHECK(fat_create("BAD.BIN",(char*)data,size)==0);first=raw_root_entry_cluster(&l,"BAD.BIN");CHECK(first>=2);raw_fat_set(&l,first,l.type==FAT_TYPE_12?0xFF7:(l.type==FAT_TYPE_16?0xFFF7:0x0FFFFFF7));block_cache_invalidate();got=0;CHECK(fat_read("BAD.BIN",(char*)out,&got,size)!=0);
    free(data);free(out);checks+=12;
}

int main(int argc,char**argv){if(argc!=4){fprintf(stderr,"usage: %s IMAGE TYPE exercise|corrupt\n",argv[0]);return 2;}disk=fopen(argv[1],"r+b");if(!disk){perror(argv[1]);return 2;}CHECK(fseeko(disk,0,SEEK_END)==0);disk_sectors=(uint64_t)ftello(disk)/512;selected_drive=0;block_cache_init();fat_type_t expected=(fat_type_t)atoi(argv[2]);if(argv[3][0]=='e')exercise(expected);else corrupt(expected);printf("PASS FAT%d %s (%d checks)\n",expected,argv[3],checks);fclose(disk);return 0;}
