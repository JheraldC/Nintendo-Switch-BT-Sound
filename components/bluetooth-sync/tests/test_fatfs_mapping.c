/* Exercise the actual bundled FatFs, not a mock FIL or guessed cluster chain. */
#include <libs/fatfs/ff.h>
#include <libs/fatfs/diskio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static unsigned checks,read_calls,write_calls;
static unsigned char *image;static const size_t image_size=70000u*512;
#define CHECK(e) do{checks++;assert(e);}while(0)

void gfx_printf(const char *s,...){(void)s;}
void *ff_memalloc(UINT n){return malloc(n);}
void ff_memfree(void *p){free(p);}
DSTATUS disk_initialize(BYTE d){return d==DRIVE_SD?0:STA_NOINIT;}
DSTATUS disk_status(BYTE d){return disk_initialize(d);}
DRESULT disk_read(BYTE d,BYTE *out,DWORD sector,UINT count){
    CHECK(d==DRIVE_SD&&sector<70000&&count<=70000-sector);read_calls++;
    memcpy(out,image+(size_t)sector*512,count*512);return RES_OK;
}
DRESULT disk_write(BYTE d,const BYTE *p,DWORD s,UINT n){(void)d;(void)p;(void)s;(void)n;write_calls++;return RES_WRPRT;}
DRESULT disk_ioctl(BYTE d,BYTE cmd,void *p){(void)d;(void)cmd;(void)p;return RES_PARERR;}
static void put16(size_t at,unsigned n){image[at]=(BYTE)n;image[at+1]=(BYTE)(n>>8);}
static void put32(size_t at,unsigned n){for(unsigned i=0;i<4;i++)image[at+i]=(BYTE)(n>>(8*i));}
int main(void){
    _Static_assert(sizeof(DWORD)==4,"match device ABI");
    image=calloc(1,image_size);CHECK(image!=NULL);
    image[0]=0xEB;image[1]=0x58;image[2]=0x90;memcpy(image+3,"SYNTHETI",8);
    put16(11,512);image[13]=1;put16(14,32);image[16]=2;image[21]=0xF8;
    put32(32,70000);put32(36,600);put32(44,2);put16(48,1);put16(50,6);image[66]=0x29;
    memcpy(image+82,"FAT32   ",8);image[510]=0x55;image[511]=0xAA;
    for(unsigned fat=0;fat<2;fat++){
        size_t base=(32+fat*600)*512;put32(base,0x0FFFFFF8);put32(base+4,0x0FFFFFFF);put32(base+8,0x0FFFFFFF);
        for(unsigned i=0;i<25;i++)put32(base+(10+i*2)*4,i==24?0x0FFFFFFF:12+i*2);
    }
    size_t root=1232*512;memcpy(image+root,"SOURCE  BTS",11);image[root+11]=0x20;put16(root+26,10);put32(root+28,12800);
    unsigned char expected[12800];for(size_t i=0;i<sizeof(expected);i++)expected[i]=(BYTE)(i*37+11);
    for(unsigned i=0;i<25;i++)memcpy(image+(1240+i*2)*512,expected+i*512,512);
    FATFS fs;FIL f;CHECK(f_mount(&fs,"sd:",1)==FR_OK);CHECK(fs.fs_type==FS_FAT32&&fs.pdrv==DRIVE_SD);
    CHECK(f_open(&f,"sd:/SOURCE.BTS",FA_READ)==FR_OK);CHECK(f_size(&f)==sizeof(expected));
    unsigned char actual[12800];DWORD sectors[25];UINT got;
    for(unsigned i=0;i<25;i++){
        CHECK(f_read(&f,actual+i*512,1,&got)==FR_OK&&got==1);sectors[i]=f.sect;
        CHECK(sectors[i]==1240+i*2);CHECK(f.clust==10+i*2);
        CHECK(f_read(&f,actual+i*512+1,511,&got)==FR_OK&&got==511);
    }
    CHECK(!memcmp(expected,actual,sizeof(actual)));CHECK(f_close(&f)==FR_OK);
    for(unsigned i=0;i<25;i++){memset(image+(size_t)sectors[i]*512,(BYTE)i,512);memset(expected+i*512,(BYTE)i,512);}
    CHECK(f_open(&f,"sd:/SOURCE.BTS",FA_READ)==FR_OK);
    CHECK(f_read(&f,actual,sizeof(actual),&got)==FR_OK&&got==sizeof(actual));CHECK(!memcmp(expected,actual,sizeof(actual)));
    CHECK(f_close(&f)==FR_OK);CHECK(!write_calls);CHECK(f_mount(NULL,"sd:",0)==FR_OK);
    free(image);printf("Actual FatFs mapping: %u checks passed; fragmented 25-sector file, correct FIL sector mapping, fresh readback, zero metadata writes\n",checks);return 0;
}

