#include "framebuffer.h"
#include "fb_console.h"
#include "keyboard.h"
#include "mouse.h"
#include "button.h"
#include "marquee.h"
#include "pic.h"
#include "idt.h"
#include "pit.h"
#include "alloc.h"
#include "heap.h"
#include "task.h"
#include "sched.h"
#include "string.h"
#include "types.h"
#include "pcspk.h"
#include "fat16.h"
#include "ata.h"
#include "exec.h"
/* ATA_DRIVE_MASTER / ATA_DRIVE_SLAVE icin (fat16.h zaten iceriyor ama netlik icin) */

/* Iki bagimsiz FAT16 mount noktasi: g_vol0 = birincil disk (tkos.img'in
 * kendisi, ATA_DRIVE_MASTER), g_vol1 = ikincil disk (varsa, ATA_DRIVE_SLAVE).
 * shell_exec() icindeki ls/cat bu ikisini secmek icin kullanir. */
static fat16_volume_t g_vol0;
static fat16_volume_t g_vol1;

/*dikkat!!! bu kodda bazi yerlerde yorumlar yanlis renk belirtebilir.
 * ancak sistemde her zaman beyaz arkaplan kullanilir.
 * renkler hakkindaki yorumlari dikkate almayiniz.
 *
 * NOT (duzeltildi): "tskdemo" komutu daha once klavye IRQ handler'i
 * ICINDE blokluyordu (multitask_start() sonsuz dongude tikanip
 * kaliyordu), bu yuzden IF hic acilmiyor ve hicbir tusla
 * durdurulamiyordu. Artik multitask_start() hemen geri donuyor,
 * task'lari kernel_main()'in idle dongusu (multitask_pump())
 * suruyor - bkz. asagidaki ilgili yorumlar. */

/*
 * TKOS - Kernel Ana Baslangic
 * TempleOS KMain.HC KMain() fonksiyonundan uyarlanmistir.
 *
 * Baslangic sirasi (KMain ile paralel):
 *   fb_init()              -> SysGrInit (grafik altyapisi)
 *   con_init()             -> text.font / text.cols / text.rows
 *   alloc_init()           -> BlkPoolsInit
 *   pic_init()             -> IntsInit
 *   idt_init()             -> IntInit1 + IntInit2
 *   pit_init()             -> TimersInit
 *   keyboard_set_callback  -> KbdInit
 *   sched_init()           -> Core0Init
 *   sti                    -> SetRFlags(RFLAGG_NORMAL)
 *   shell_loop()           -> SrvTaskCont
 */

/* ------------------------------------------------
 * Port I/O yardimcisi (reboot icin)
 * ------------------------------------------------ */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void sti(void) { __asm__ volatile ("sti"); }
static inline void cli(void) { __asm__ volatile ("cli"); }

/* ------------------------------------------------
 * CMOS RTC (Gercek Zaman Saati) okuma
 * Standart PC CMOS: adres portu 0x70, veri portu 0x71
 * (reboot kodu da ayni portlari kullanir - burada NMI-disable
 * biti (0x80) gerekmiyor, sadece register okuyoruz).
 *
 * Register haritasi (standart MC146818 RTC):
 *   0x00 Saniye   0x02 Dakika   0x04 Saat
 *   0x07 Gun      0x08 Ay       0x09 Yil (2 haneli)
 *   0x0A Status A (bit7 = Update-In-Progress)
 *   0x0B Status B (bit2 = binary(1)/BCD(0), bit1 = 24s(1)/12s(0))
 * ------------------------------------------------ */
typedef struct {
    uint8_t  sec, min, hour, day, month;
    uint16_t year;
} rtc_time_t;

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static int cmos_update_in_progress(void) {
    return (cmos_read(0x0A) & 0x80) != 0;
}

static inline uint8_t bcd2bin(uint8_t v) {
    return (uint8_t)((v & 0x0F) + (v >> 4) * 10);
}

static void rtc_read_raw(rtc_time_t *t) {
    while (cmos_update_in_progress()) { /* RTC guncelleniyor, bekle */ }
    t->sec   = cmos_read(0x00);
    t->min   = cmos_read(0x02);
    t->hour  = cmos_read(0x04);
    t->day   = cmos_read(0x07);
    t->month = cmos_read(0x08);
    t->year  = cmos_read(0x09);
}

/* "Yirtik" (torn) okumayi engellemek icin: bir kez oku, ayni
 * degerler tekrar cikana kadar tekrarla (RTC tam guncellenirken
 * ortasindan okumus olma ihtimaline karsi). */
static void rtc_read(rtc_time_t *out) {
    rtc_time_t a, b;
    rtc_read_raw(&a);
    for (;;) {
        rtc_read_raw(&b);
        if (a.sec == b.sec && a.min == b.min && a.hour == b.hour &&
            a.day == b.day && a.month == b.month && a.year == b.year) {
            break;
        }
        a = b;
    }

    uint8_t status_b = cmos_read(0x0B);

    if (!(status_b & 0x04)) {   /* BCD modu -> binary'ye cevir */
        b.sec   = bcd2bin(b.sec);
        b.min   = bcd2bin(b.min);
        b.hour  = (uint8_t)(bcd2bin((uint8_t)(b.hour & 0x7F)) | (b.hour & 0x80));
        b.day   = bcd2bin(b.day);
        b.month = bcd2bin(b.month);
        b.year  = bcd2bin((uint8_t)b.year);
    }

    if (!(status_b & 0x02) && (b.hour & 0x80)) {  /* 12s modu + PM biti */
        b.hour = (uint8_t)(((b.hour & 0x7F) + 12) % 24);
    }

    out->sec   = b.sec;
    out->min   = b.min;
    out->hour  = b.hour;
    out->day   = b.day;
    out->month = b.month;
    out->year  = (uint16_t)(2000 + b.year); /* 2000-2099 varsayimi, hobi OS icin yeterli */
}

/* Ust lacivert cubugun sag tarafina "YYYY-AA-GG SS:DD:sn" formatinda
 * saat/tarih yazar. print_banner() o satiri zaten tamamen mavi(1)
 * arka planla doldurmus durumda; sadece uzerine ayni renklerle
 * (beyaz/mavi) yaziyoruz, satirin geri kalanini bozmuyoruz. */
static void draw_clock(void) {
    rtc_time_t t;
    rtc_read(&t);

    char buf[20];
    int  p = 0;
    #define PUT2(v) do { buf[p++] = (char)('0' + ((v) / 10) % 10); \
                          buf[p++] = (char)('0' + (v) % 10); } while (0)

    buf[p++] = '2'; buf[p++] = '0';
    PUT2(t.year % 100);
    buf[p++] = '-';
    PUT2(t.month);
    buf[p++] = '-';
    PUT2(t.day);
    buf[p++] = ' ';
    PUT2(t.hour);
    buf[p++] = ':';
    PUT2(t.min);
    buf[p++] = ':';
    PUT2(t.sec);
    buf[p] = '\0';

    #undef PUT2

    con_print_at(58, 0, buf, 15, 1);  /* beyaz yazi, lacivert (1) zemin */
}

/* Idle dongudeki saat guncellemesi: her dongude CMOS'a gitmemek
 * icin pit_get_ticks() ile saniye siniri kontrol edilir, sadece
 * yeni bir saniyeye gecildiginde RTC tekrar okunup yazilir. */
static uint64_t clock_last_sec = (uint64_t)-1;

static void maybe_update_clock(void) {
    uint64_t secs = pit_get_ticks() / 1000;
    if (secs != clock_last_sec) {
        clock_last_sec = secs;
        draw_clock();
    }
}

/* ------------------------------------------------
 * Palet: VBE 8bpp modu icin BIOS VGA standart 16 renk
 * 0x9000 blougunda VBE aktif; palet DAC uzerinden ayarlanir.
 * TempleOS text.attr / CBGR48 palette karsiligi.
 *
 * Her renk: (R, G, B) - 6-bit DAC (0-63 arasi deger)
 * ------------------------------------------------ */
static const uint8_t palette16[16][3] = {
    {  0,  0,  0 },   /*  0 Siyah        */
    {  0,  0, 42 },   /*  1 Koyu Mavi    */
    {  0, 42,  0 },   /*  2 Koyu Yesil   */
    {  0, 42, 42 },   /*  3 Koyu Cyan    */
    { 42,  0,  0 },   /*  4 Koyu Kirmizi */
    { 42,  0, 42 },   /*  5 Koyu Magenta */
    { 42, 21,  0 },   /*  6 Kahverengi   */
    { 42, 42, 42 },   /*  7 Acik Gri     */
    { 21, 21, 21 },   /*  8 Koyu Gri     */
    { 21, 21, 63 },   /*  9 Mavi         */
    {  0, 50,  8 },   /* 10 Yesil        */
    {  0, 45, 45 },   /* 11 Cyan         */
    { 63, 21, 21 },   /* 12 Kirmizi      */
    { 63, 21, 63 },   /* 13 Magenta      */
    { 51, 38,  0 },   /* 14 Sari         */
    { 63, 63, 63 },   /* 15 Beyaz        */
};

/*
 * set_palette() - VGA DAC uzerinden 8bpp palet yukle.
 * VBE 8bpp modunda varsayilan palet tanimsizdir;
 * DAC'e yazarak istedigimiz renkleri tanimliyoruz.
 * TempleOS CBGR48 palette[] mantigi.
 */
static void set_palette(void) {
    outb(0x3C8, 0);                 /* DAC yazma indeksi: 0'dan baslat */
    for (int i = 0; i < 16; i++) {
        outb(0x3C9, palette16[i][0]);   /* R */
        outb(0x3C9, palette16[i][1]);   /* G */
        outb(0x3C9, palette16[i][2]);   /* B */
    }
}

/* ------------------------------------------------
 * Klavye IRQ handler
 * TempleOS KbdInit -> KbdHndlr mantigi.
 *
 * IDT'ye dogrudan baglanmaz; idt_dispatch() uzerinden
 * keyboard_handler() cagrisi yapilir. IRQ1 handler'i
 * olarak idt_set_handler(I_KEYBOARD, kbd_irq) ile
 * kayit edilir.
 * ------------------------------------------------ */
static void kbd_irq_handler(int_frame_t *frame) {
    (void)frame;
    keyboard_handler();     /* PS/2 scancode oku, callback'i tetikle */
    /* EOI: idt_dispatch() tarafindan gonderilir */
}

/* ------------------------------------------------
 * Shell - basit komut satiri
 * TempleOS SrvTaskCont / Adam Task giris dongusunun
 * minimal karsiligi.
 *
 * Simdilik: karakterleri ekrana yazar, Enter'da
 * komutu islemeye calisir.
 * ------------------------------------------------ */
#define SHELL_BUF_SIZE 256

static char    shell_buf[SHELL_BUF_SIZE];
static uint32_t shell_len = 0;
static uint8_t  shell_fg  = 0;   /* Beyaz  */
static uint8_t  shell_bg  = 15;    /* Siyah  */

/* Cok-gorevli demo - asagida tanimli, shell_exec icin ileri bildirim */
static void multitask_start(void);
static void multitask_stop(void);
static void shell_prompt(void);  /* asagida tanimli, melody_pump icin ileri bildirim */

/* ------------------------------------------------
 * Melodi calma - IRQ baglami disina tasima
 *
 * 'play' komutu klavye IRQ handler'i icinden geldigi icin
 * pcspk_play_melody() burada DOGRUDAN cagrilamaz (pit_sleep_ms
 * duzgun ilerleyemez, bkz. play komutunun ustundeki yorum).
 * Bunun yerine shell_exec sadece melody_pending'i set eder;
 * asil calma islemi kernel_main() idle dongusunden
 * melody_pump() ile, interrupt baglaminin DISINDA yapilir.
 * ------------------------------------------------ */
static const music_note_t *melody_pending  = 0;   /* calinmayi bekleyen melodi (varsa) */
static volatile int         melody_playing = 0;    /* su an calma islemi surer mi */

/*
 * melody_pump() - kernel_main() idle dongusunden HER TURDA cagrilir.
 * Bekleyen bir melodi varsa, interrupt baglami disinda oldugumuz
 * icin pit_sleep_ms() guvenle calisir; pcspk_play_melody() burada
 * bloklayarak (fonksiyonun kendi tasarimi geregi) calinir, biter
 * bitmez shell_prompt() tekrar basilir.
 */
static void melody_pump(void) {
    if (!melody_pending) return;

    const music_note_t *m = melody_pending;
    melody_pending = 0;
    melody_playing = 1;

    pcspk_play_melody(m);

    melody_playing = 0;
    con_set_color(8, 15);
    con_print("[melodi bitti]\n");
    con_set_color(0, 15);
    shell_prompt();
}

/* ------------------------------------------------
 * alloc / free / allocs komutlari
 *
 * heap.c (kmalloc/kfree) uzerine kurulu, kucuk ama guvenlikli bir
 * demo katmani. TempleOS shell'inde oldugu gibi "MAlloc(64)" yazip
 * sonucu elle Free() edebilme fikrinin shell karsiligi.
 *
 * GUVENLIK NOTU: "free <adres>" kullanicidan gelen HAM bir pointer'i
 * dogrudan kfree()'ye vermez. Kernel bellek uzayinda yanlis/rastgele
 * bir adresi serbest birakmak heap'i bozabilir ve tum sistemi
 * cokertebilir. Bu yuzden shell, kendi tahsis ettigi adresleri kucuk
 * bir tabloda izler; "free" sadece bu tabloda kayitli bir adrese
 * izin verir. Boylece bir yazim hatasi ("free 0x1234") kernel'i
 * cokertmek yerine sadece "boyle bir adres yok" hatasi verir.
 * ------------------------------------------------ */
#define SHELL_ALLOC_MAX  32

typedef struct {
    void    *ptr;
    uint64_t size;   /* kullanicinin istedigi boyut (istenen, gercek degil) */
    int      used;
} shell_alloc_t;

static shell_alloc_t shell_allocs[SHELL_ALLOC_MAX];

/* Bos bir slot bul ve kaydi ekle. Basarisizsa -1 doner (tablo dolu). */
static int shell_alloc_track_add(void *ptr, uint64_t size) {
    for (int i = 0; i < SHELL_ALLOC_MAX; i++) {
        if (!shell_allocs[i].used) {
            shell_allocs[i].used = 1;
            shell_allocs[i].ptr  = ptr;
            shell_allocs[i].size = size;
            return i;
        }
    }
    return -1;
}

/* ptr'ye ait kaydi bul. Bulunamazsa -1 doner. */
static int shell_alloc_track_find(void *ptr) {
    for (int i = 0; i < SHELL_ALLOC_MAX; i++)
        if (shell_allocs[i].used && shell_allocs[i].ptr == ptr)
            return i;
    return -1;
}

/*
 * shell_arg_after() - cmd, "word" ile TAM KELIME olarak basliyorsa
 * (ardindan bosluk veya string sonu geliyorsa) argumanin basladigi
 * yeri dondurur (bosluklar atlanmis halde; arguman yoksa bos string).
 * cmd, word ile baslamiyorsa veya "allocX" gibi yanlis-eslesme
 * durumundaysa NULL doner.
 *
 * Ornekler ("alloc" icin):
 *   "alloc 64"  -> "64"
 *   "alloc"     -> ""              (arguman yok, cagiran kontrol etmeli)
 *   "allocs"    -> NULL            (farkli komut, yanlis eslesme degil)
 */
static const char *shell_arg_after(const char *cmd, const char *word) {
    size_t wlen = strlen(word);
    if (strncmp(cmd, word, wlen) != 0) return NULL;
    if (cmd[wlen] == '\0') return cmd + wlen;
    if (cmd[wlen] != ' ')  return NULL;
    const char *p = cmd + wlen + 1;
    while (*p == ' ') p++;
    return p;
}

/*
 * shell_cmd_ls() - Belirtilen FAT16 volume'unun root dizinini listeler.
 * ls ve ls2 komutlari tarafindan ortak kullanilir (bkz. shell_exec).
 *
 * @vol   : fat16_init() ile mount edilmis (ya da mount basarisiz
 *          olmus) volume; mounted=0 ise kullaniciya bilgi verilir.
 * @label : hata/bilgi mesajlarinda gosterilecek disk adi ("birincil
 *          disk" / "ikincil disk").
 */
static void shell_cmd_ls(fat16_volume_t *vol, const char *label) {
    if (!vol->mounted) {
        con_set_color(12, 15);
        con_print("FAT16 baglanmamis (");
        con_print(label);
        con_print("). Disk yok ya da formatlanmamis.\n");
        con_set_color(0, 15);
        return;
    }

    fat16_dirent_t entries[64];
    int count = fat16_list_root(vol, entries, 64);

    con_set_color(11, 15);  /* Cyan */
    con_print("Dosyalar (");
    con_print_dec(count);
    con_print("):\n");
    con_set_color(0, 15);

    for (int i = 0; i < count; i++) {
        if (entries[i].attr & FAT_ATTR_DIRECTORY) {
            con_set_color(9, 15);  /* Mavi - dizin */
            con_print("  [DIR]  ");
        } else {
            /* NOT: daha once burada con_set_color(15, 15) vardi -
             * beyaz yazi + beyaz arka plan, yani metin gorunmuyordu.
             * Duzeltildi: siyah yazi (0), beyaz arka plan (15). */
            con_set_color(0, 15);
            con_print("         ");
        }
        con_print(entries[i].name);
        con_print("  (");
        con_print_dec(entries[i].size);
        con_print(" byte)\n");
    }
    con_set_color(0, 15);
}

/*
 * shell_cmd_cat() - Belirtilen FAT16 volume'undan bir dosyayi okuyup
 * icerigini ekrana basar. cat ve cat2 komutlari tarafindan ortak
 * kullanilir (bkz. shell_exec).
 */
static void shell_cmd_cat(fat16_volume_t *vol, const char *arg) {
    if (!vol->mounted) {
        con_set_color(12, 15);
        con_print("FAT16 baglanmamis. Disk yok ya da formatlanmamis.\n");
        con_set_color(0, 15);
        return;
    }

    if (arg[0] == '\0') {
        con_set_color(12, 15);
        con_print("Usage: cat <dosya>   (ex: cat HELLO.TXT)\n");
        con_set_color(0, 15);
        return;
    }

    fat16_dirent_t entry;
    if (!fat16_find_in_root(vol, arg, &entry)) {
        con_set_color(12, 15);
        con_print("Dosya bulunamadi: ");
        con_print(arg);
        con_print("\n");
        con_set_color(0, 15);
        return;
    }

    if (entry.attr & FAT_ATTR_DIRECTORY) {
        con_set_color(12, 15);
        con_print("Bu bir dizin, dosya degil.\n");
        con_set_color(0, 15);
        return;
    }

    /* Basit, sabit boyutlu bir okuma arabellegi. Buyuk dosyalar
     * icin ileride sayfa/heap allocator ile dinamik boyut
     * kullanilabilir; simdilik 4KB shell'den goruntulemek icin
     * fazlasiyla yeterli. static: cat ve cat2 ayni anda cagrilmadigi
     * (tek cekirdek, senkron shell) icin paylasilan tek bir arabellek
     * yeterli ve yigin (stack) kullanimini azaltir. */
    static uint8_t file_buf[4096];
    int n = fat16_read_file(vol, &entry, file_buf, sizeof(file_buf) - 1);

    if (n < 0) {
        con_set_color(12, 15);
        con_print("Dosya okuma hatasi (disk I/O).\n");
        con_set_color(0, 15);
        return;
    }

    file_buf[n] = '\0';
    con_set_color(0, 15);
    con_print((const char *)file_buf);
    if (n > 0 && file_buf[n - 1] != '\n') con_print("\n");

    if ((uint32_t)n < entry.size) {
        con_set_color(14, 15);
        con_print("[... dosya 4KB sinirindan buyuk, kesildi ...]\n");
        con_set_color(0, 15);
    }
}

/* write <dosya> <metin> - metni bir dosyaya yaz (varsa ustune yazar) */
static void shell_cmd_write(fat16_volume_t *vol, const char *arg) {
    if (!vol->mounted) {
        con_set_color(12, 15);
        con_print("FAT16 baglanmamis. Disk yok ya da formatlanmamis.\n");
        con_set_color(0, 15);
        return;
    }

    /* Ilk kelime dosya adi, kalani (bosluklardan sonra) icerik */
    const char *p = arg;
    char fname[FAT16_MAX_NAME];
    int  fi = 0;
    while (*p && *p != ' ' && fi < FAT16_MAX_NAME - 1) fname[fi++] = *p++;
    fname[fi] = '\0';

    if (fi == 0) {
        con_set_color(12, 15);
        con_print("Usage: write <dosya> <metin>   (ex: write TEST.TXT Merhaba)\n");
        con_set_color(0, 15);
        return;
    }

    while (*p == ' ') p++;

    uint32_t len = (uint32_t)strlen(p);
    int rc = fat16_write_file(vol, fname, p, len);

    if (rc != 0) {
        con_set_color(12, 15);
        con_print("HATA: dosya yazilamadi (disk dolu ya da root dizini dolu olabilir).\n");
        con_set_color(0, 15);
        return;
    }

    con_set_color(10, 15);
    con_print("Yazildi: ");
    con_print(fname);
    con_print(" (");
    con_print_dec(len);
    con_print(" byte)\n");
    con_set_color(0, 15);
}

/* rm <dosya> - dosyayi sil */
static void shell_cmd_rm(fat16_volume_t *vol, const char *arg) {
    if (!vol->mounted) {
        con_set_color(12, 15);
        con_print("FAT16 baglanmamis. Disk yok ya da formatlanmamis.\n");
        con_set_color(0, 15);
        return;
    }

    if (arg[0] == '\0') {
        con_set_color(12, 15);
        con_print("Usage: rm <dosya>   (ex: rm TEST.TXT)\n");
        con_set_color(0, 15);
        return;
    }

    int rc = fat16_delete_file(vol, arg);
    if (rc != 0) {
        con_set_color(12, 15);
        con_print("HATA: dosya silinemedi (bulunamadi olabilir).\n");
        con_set_color(0, 15);
        return;
    }

    con_set_color(10, 15);
    con_print("Silindi: ");
    con_print(arg);
    con_print("\n");
    con_set_color(0, 15);
}

/* exec <dosya> - TKX uygulamasini yukle ve calistir */
static void shell_cmd_exec(fat16_volume_t *vol, const char *arg) {
    if (arg[0] == '\0') {
        con_set_color(12, 15);
        con_print("Usage: exec <dosya>   (ex: exec HELLO.TKX)\n");
        con_set_color(0, 15);
        return;
    }

    task_t *t = NULL;
    int rc = exec_load(vol, arg, &t);

    if (rc != EXEC_OK) {
        con_set_color(12, 15);
        con_print("HATA: ");
        con_print(exec_strerror(rc));
        con_print("\n");
        con_set_color(0, 15);
        return;
    }

    sched_add(t);

    con_set_color(10, 15);
    con_print("Calistirildi (gorev #");
    con_print_dec(t->task_num);
    con_print("): ");
    con_print(arg);
    con_print("\n");
    con_set_color(0, 15);
}

/* alloc <boyut> - kmalloc() ile bellek tahsis et */
static void shell_cmd_alloc(const char *arg) {
    int ok;
    uint64_t size = str_to_dec(arg, &ok);

    if (!ok || size == 0) {
        con_set_color(12, 15);
        con_print("Usage: alloc <size>   (ex: alloc 64)\n");
        con_set_color(0, 15);
        return;
    }

    void *ptr = kmalloc(size);
    if (!ptr) {
        con_set_color(12, 15);
        con_print("HATA: kmalloc basarisiz (bellek yetersiz olabilir)\n");
        con_set_color(0, 15);
        return;
    }

    if (shell_alloc_track_add(ptr, size) < 0) {
        /* Izleme tablosu dolu: guvenlik icin hemen geri ver, yoksa
         * shell bu blogu asla free edemez (kayipta kalir). */
        kfree(ptr);
        con_set_color(12, 15);
        con_print("HATA: izleme tablosu dolu, once bazi bloklari 'free' ile birakin\n");
        con_set_color(0, 15);
        return;
    }

    con_set_color(10, 15);
    con_print("Allocated! address: ");
    con_print_hex((uint64_t)(uintptr_t)ptr);
    con_print("  Desired: ");
    con_print_dec(size);
    con_print(" byte  real: ");
    con_print_dec(ksize(ptr));
    con_print(" byte\n");
    con_set_color(0, 15);
}

/* free <adres> - kfree() ile bellek serbest birak (sadece shell'in
 * kendi tahsis ettigi, hala gecerli adresler icin) */
static void shell_cmd_free(const char *arg) {
    int ok;
    uint64_t addr = str_to_hex(arg, &ok);

    if (!ok) {
        con_set_color(12, 15);
        con_print("Usage: free <address>   (ex: free 0x400010)\n");
        con_set_color(0, 15);
        return;
    }

    void *ptr  = (void *)(uintptr_t)addr;
    int   slot = shell_alloc_track_find(ptr);

    if (slot < 0) {
        con_set_color(12, 15);
        con_print("HATA: Bu adres 'alloc' ile tahsis edilmis gorunmuyor;\n");
        con_print("      guvenlik icin serbest birakilmadi.\n");
        con_print("      Mevcut bloklar icin 'allocs' yazabilirsiniz.\n");
        con_set_color(0, 15);
        return;
    }

    uint64_t size = shell_allocs[slot].size;
    kfree(ptr);
    shell_allocs[slot].used = 0;

    con_set_color(10, 15);
    con_print("Serbest birakildi -> adres: ");
    con_print_hex(addr);
    con_print("  (");
    con_print_dec(size);
    con_print(" byte)\n");
    con_set_color(0, 15);
}

/* allocs - shell'in izledigi aktif tahsisleri listele */
static void shell_cmd_allocs(void) {
    int count = 0;

    con_set_color(11, 15);
    con_print("Active Shell Allocates:\n");
    for (int i = 0; i < SHELL_ALLOC_MAX; i++) {
        if (!shell_allocs[i].used) continue;
        count++;
        con_print("  ");
        con_print_hex((uint64_t)(uintptr_t)shell_allocs[i].ptr);
        con_print("  ");
        con_print_dec(shell_allocs[i].size);
        con_print(" byte\n");
    }
    if (count == 0) con_print("  (yok)\n");

    con_print("Heap Usage: ");
    con_print_dec(heap_get_used());
    con_print(" / ");
    con_print_dec(heap_get_total());
    con_print(" byte (OS'tan alinan toplam)\n");
    con_set_color(0, 15);
}

/* Komut isleme */
static void shell_exec(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return;

    /* clear - ekrani temizle */
    if (strcmp(cmd, "clear") == 0) {
        con_clear();
        return;
    }

    /* help - komut listesi */
    if (strcmp(cmd, "help") == 0) {
        con_set_color(14, 15);  /* Sari */
        con_print("Komutlar:\n");
        con_set_color(10, 15);
        con_print("  help    - This sentence\n");
        con_print("  clear   - Clear the screen\n");
        con_print("  mem     - Memory situation, page/alloc\n");
        con_print("  heap    - Show heap(byte) allocations\n");
        con_print("  alloc <boyut> - Allocate memory\n");
        con_print("  free <adres>  - Release/free allocated mem\n");
        con_print("  allocs  - Active allocations list\n");
        con_print("  tasks   - Show task list\n");
        con_print("  uptime  - Show runtime duration\n");
        con_print("  tskdemo - Start multitask test(demo)\n");
        con_print("  play    - Play custom melody\n");
        con_print("  ls      - List files (FAT16, primary disk)\n");
        con_print("  cat <dosya> - Show file contents (primary disk)\n");
        con_print("  ls2     - List files (FAT16, secondary disk)\n");
        con_print("  cat2 <dosya> - Show file contents (secondary disk)\n");
        con_print("  write <dosya> <metin> - Write file (primary disk)\n");
        con_print("  rm <dosya>    - Delete file (primary disk)\n");
        con_print("  write2 <dosya> <metin> - Write file (secondary disk)\n");
        con_print("  rm2 <dosya>   - Delete file (secondary disk)\n");
        con_print("  reboot  - Restart system\n");
        return;
    }

    /* mem - sayfa allocator istatistikleri */
    if (strcmp(cmd, "mem") == 0) {
        con_set_color(11, 15);  /* Cyan */
        alloc_print_stats();
        con_set_color(0, 15);
        return;
    }

    /* heap - byte-granulariteli heap istatistikleri */
    if (strcmp(cmd, "heap") == 0) {
        con_set_color(11, 15);  /* Cyan */
        heap_print_stats();
        con_set_color(0, 15);
        return;
    }

    /* alloc <boyut> */
    {
        const char *arg = shell_arg_after(cmd, "alloc");
        if (arg) { shell_cmd_alloc(arg); return; }
    }

    /* free <adres> */
    {
        const char *arg = shell_arg_after(cmd, "free");
        if (arg) { shell_cmd_free(arg); return; }
    }

    /* allocs */
    if (strcmp(cmd, "allocs") == 0) {
        shell_cmd_allocs();
        return;
    }

    /* tasks - gorev listesi */
    if (strcmp(cmd, "tasks") == 0) {
        con_set_color(11, 15);  /* Cyan */
        sched_print_tasks();
        con_set_color(0, 15);
        return;
    }

    /* multitask - demo task'lari baslat */
    if (strcmp(cmd, "tskdemo") == 0) {
        multitask_start();
        return;
    }

    /* multistop - demo task'lari durdur */
    if (strcmp(cmd, "multistop") == 0) {
        multitask_stop();
        return;
    }

    /* play - ozel melodiyi cal
     * ONEMLI: bu komut kbd_irq_handler() (klavye IRQ'su) icinden
     * cagriliyor. pcspk_play_melody() burada DOGRUDAN cagrilirsa,
     * ic ice pit_sleep_ms() cagrilari interrupt baglaminin
     * disina hic cikamaz; PIT (IRQ0) bu sirada duzgun islenemedigi
     * icin notalar pratikte ust uste/ard arda cok hizli calinir ve
     * kulak sadece tek bir "bip" duyar (bkz. tskdemo/multitask_start
     * ustundeki ayni sorunu aciklayan yorum).
     *
     * Cozum ayni desen: burada sadece bir "calinsin" bayragi/pointer
     * set edilir, gercek pcspk_play_melody() cagrisi asagidaki
     * melody_pump() icinde -kernel_main() idle dongusunden, yani
     * interrupt baglaminin DISINDAN- yapilir. */
    if (strcmp(cmd, "play") == 0) {
        if (melody_playing) {
            con_set_color(14, 15);
            con_print("Zaten bir melodi calmakta.\n");
            con_set_color(0, 15);
            return;
        }
        con_set_color(11, 15);  /* Cyan */
        con_print("Melodi calmiyor... (idle dongude calisir)\n");
        con_set_color(0, 15);
        melody_pending = melody_custom;
        return;
    }

    /* ls - birincil disk (ATA_DRIVE_MASTER) FAT16 root dizinini listele */
    if (strcmp(cmd, "ls") == 0) {
        shell_cmd_ls(&g_vol0, "birincil disk");
        return;
    }

    /* ls2 - ikincil disk (ATA_DRIVE_SLAVE) FAT16 root dizinini listele */
    if (strcmp(cmd, "ls2") == 0) {
        shell_cmd_ls(&g_vol1, "ikincil disk");
        return;
    }

    /* cat <dosya> - birincil diskteki bir dosyanin icerigini ekrana yaz */
    {
        const char *arg = shell_arg_after(cmd, "cat");
        if (arg) {
            shell_cmd_cat(&g_vol0, arg);
            return;
        }
    }

    /* cat2 <dosya> - ikincil diskteki bir dosyanin icerigini ekrana yaz */
    {
        const char *arg = shell_arg_after(cmd, "cat2");
        if (arg) {
            shell_cmd_cat(&g_vol1, arg);
            return;
        }
    }

    /* write <dosya> <metin> - birincil diske dosya yaz */
    {
        const char *arg = shell_arg_after(cmd, "write");
        if (arg) { shell_cmd_write(&g_vol0, arg); return; }
    }

    /* write2 <dosya> <metin> - ikincil diske dosya yaz */
    {
        const char *arg = shell_arg_after(cmd, "write2");
        if (arg) { shell_cmd_write(&g_vol1, arg); return; }
    }

    /* rm <dosya> - birincil diskten dosya sil */
    {
        const char *arg = shell_arg_after(cmd, "rm");
        if (arg) { shell_cmd_rm(&g_vol0, arg); return; }
    }

    /* rm2 <dosya> - ikincil diskten dosya sil */
    {
        const char *arg = shell_arg_after(cmd, "rm2");
        if (arg) { shell_cmd_rm(&g_vol1, arg); return; }
    }

   /* exec <dosya> - birincil diskten TKX uygulamasi calistir */
    {
        const char *arg = shell_arg_after(cmd, "exec");
        if (arg) { shell_cmd_exec(&g_vol0, arg); return; }
    }

    /* exec2 <dosya> - ikincil diskten TKX uygulamasi calistir */
    {
        const char *arg = shell_arg_after(cmd, "exec2");
        if (arg) { shell_cmd_exec(&g_vol1, arg); return; }
    } 

    /* uptime - calisma suresi */
    if (strcmp(cmd, "uptime") == 0) {
        uint64_t ticks = pit_get_ticks();
        uint64_t secs  = ticks / 1000;
        uint64_t mins  = secs / 60;
        uint64_t hrs   = mins / 60;
        con_set_color(11, 15);
        con_print("Uptime: ");
        con_print_dec(hrs);   con_print("s ");
        con_print_dec(mins % 60); con_print("d ");
        con_print_dec(secs % 60); con_print("s\n");
        con_set_color(0, 15);
        return;
    }

    /* reboot - TempleOS Reboot() ile ayni mantik */
    if (strcmp(cmd, "reboot") == 0) {
        cli();
        /* CMOS reset */
        outb(0x70, 0x8F);
        outb(0x71, 0x00);
        /* Klavye denetleyicisi uzerinden reset */
        outb(0x92, inb(0x92) | 1);
        for (;;) __asm__ volatile ("hlt");
    }

    /* Bilinmeyen komut */
    con_set_color(12, 15);  /* Kirmizi */
    con_print("Bilinmeyen komut: ");
    con_print(cmd);
    con_print("\n");
    con_set_color(0, 15);
}

/* Prompt yazdir */
static void shell_prompt(void) {
    con_set_color(10, 15);   /* Yesil */
    con_print("TK: ");
    con_set_color(0, 15);   /* Beyaz */
}

/*
 * Klavye callback'i
 * TempleOS KbdHndlr -> CFifoU8 -> task input mantigindan
 * uyarlanmistir. Karakteri shell tamponuna ekler.
 */
static void shell_key_handler(char c) {
    if (c == '\n') {
        /* Enter: komutu isle */
        con_putchar('\n');
        shell_buf[shell_len] = '\0';
        if (shell_len > 0) {
            shell_exec(shell_buf);
        }
        shell_len = 0;
        shell_prompt();
    } else if (c == '\b') {
        /* Backspace */
        if (shell_len > 0) {
            shell_len--;
            con_putchar('\b');
        }
    } else {
        /* Normal karakter */
        if (shell_len < SHELL_BUF_SIZE - 1) {
            shell_buf[shell_len++] = c;
            con_putchar(c);
        }
    }
}

/* ------------------------------------------------
 * Cok-Gorevli Demo
 * sched.c / task.h / sched_asm.asm'in gercek anlamda
 * test edildigi bolum. Uc ayri task, kendi sayaclarini
 * sabit ekran satirlarina yazar ve task_yield() ile
 * gonullu (cooperative) olarak CPU'yu birakir.
 *
 * Cooperative secildi: mevcut sched_context_switch()
 * call/ret mantigiyla tasarlandi, IRQ-safe degil.
 * Eger IRQ0'dan (preemptive) tetiklenirse interrupt
 * frame'i bozulur. Bu yuzden task_yield() her zaman
 * normal fonksiyon cagrisi baglaminda calisir.
 * ------------------------------------------------ */
#define DEMO_TASK_ROW_BASE   56   /* Ekranin alt kismi (ROWS=60) */
#define DEMO_TASK_COUNT      3

static volatile int demo_tasks_running = 0;
static task_t *demo_task_handles[DEMO_TASK_COUNT];

static void demo_task_1(void) {
    uint64_t counter = 0;
    char buf[32];
    while (demo_tasks_running) {
        counter++;
        con_print_at(2, DEMO_TASK_ROW_BASE, "Task1: ", 12, 15); /* Kirmizi */
        utoa(counter, buf, 10);
        con_print_at(9, DEMO_TASK_ROW_BASE, "          ", 12, 15); /* eski sayiyi sil */
        con_print_at(9, DEMO_TASK_ROW_BASE, buf, 12, 15);
        task_yield();
    }
    task_kill(sched_get_current());
    for (;;) task_yield();
}

static void demo_task_2(void) {
    uint64_t counter = 0;
    char buf[32];
    while (demo_tasks_running) {
        counter++;
        con_print_at(2, DEMO_TASK_ROW_BASE + 1, "Task2: ", 10, 15); /* Yesil */
        utoa(counter, buf, 10);
        con_print_at(9, DEMO_TASK_ROW_BASE + 1, "          ", 10, 15);
        con_print_at(9, DEMO_TASK_ROW_BASE + 1, buf, 10, 15);
        task_yield();
    }
    task_kill(sched_get_current());
    for (;;) task_yield();
}

static void demo_task_3(void) {
    uint64_t counter = 0;
    char buf[32];
    while (demo_tasks_running) {
        counter++;
        con_print_at(2, DEMO_TASK_ROW_BASE + 2, "Task3: ", 9, 15); /* Mavi */
        utoa(counter, buf, 10);
        con_print_at(9, DEMO_TASK_ROW_BASE + 2, "          ", 9, 15);
        con_print_at(9, DEMO_TASK_ROW_BASE + 2, buf, 9, 15);
        task_yield();
    }
    task_kill(sched_get_current());
    for (;;) task_yield();
}

/*
 * multitask demo icin gecici klavye yakalayicisi.
 * Normal shell_key_handler'a hic karismadan calisir;
 * sadece 'q' veya Enter tusuna basilinca durma
 * sinyali verir. multitask_start() suresince aktif olur,
 * bitince shell_key_handler geri yuklenir.
 */
static volatile int demo_stop_requested = 0;

static void demo_stop_key_handler(char c) {
    (void)c;
    /* Herhangi bir tusa basinca demo dursun (basit ve guvenli) */
    demo_stop_requested = 1;
}

/*
 * multitask_start() - 3 demo task'i olusturur, baslatir ve
 * kullanici bir tusa basana kadar bloklayan bir dongude
 * surekli task_yield() cagirarak demo task'lara CPU verir.
 *
 * Bu fonksiyon donene kadar normal shell komut satiri
 * CALISMAZ (bilerek - "multistop'a kadar shell bloklanir"
 * secimi). Klavye callback'i gecici olarak degistirilip
 * fonksiyon sonunda eski haline dondurulur, boylece
 * shell_key_handler ile cakisma/iç ice interrupt riski olmaz.
 */
static void multitask_start(void) {
    if (demo_tasks_running) {
        con_set_color(14, 15);
        con_print("Multitask demo zaten calisiyor.\n");
        con_set_color(0, 15);
        return;
    }

    demo_tasks_running  = 1;
    demo_stop_requested = 0;

    demo_task_handles[0] = task_create("DemoTask1", demo_task_1, 0);
    demo_task_handles[1] = task_create("DemoTask2", demo_task_2, 0);
    demo_task_handles[2] = task_create("DemoTask3", demo_task_3, 0);

    int ok = 1;
    for (int i = 0; i < DEMO_TASK_COUNT; i++) {
        if (!demo_task_handles[i]) { ok = 0; continue; }
        sched_add(demo_task_handles[i]);
    }

    if (!ok) {
        con_set_color(12, 15);
        con_print("HATA: Bazi task'lar olusturulamadi (bellek yetersiz olabilir).\n");
        con_set_color(0, 15);
        demo_tasks_running = 0;
        return;
    }

    con_set_color(10, 15);
    con_print("Multitask demo baslatildi. Durdurmak icin herhangi bir tusa basin...\n");
    con_set_color(0, 15);

    /* Klavyeyi gecici olarak demo durdurucusuna bagla.
     * ONEMLI: bu satir daha once eksikti - demo_stop_key_handler
     * hicbir zaman klavye callback'i olarak ayarlanmadigi icin
     * tuslar hala shell_key_handler'a gidiyordu, demo_stop_requested
     * asla 1 olmuyordu ve multitask_pump() demoyu hic durdurmuyordu.
     * ONEMLI: burada ARTIK bloklayan bir dongu YOK. Bu fonksiyon
     * klavye IRQ handler'i (kbd_irq_handler) icinden cagrildigi
     * icin burada tikanip kalmak, ISR'in hic iretq yapamamasi ve
     * IF bayraginin bir daha ACILMAMASI demekti - yani ne yeni
     * klavye kesmesi ne de shell komutu islenebiliyordu. Simdi
     * fonksiyon hemen geri donuyor; task'lara CPU'yu asagidaki
     * multitask_pump() -kernel_main()'in idle dongusunden, yani
     * interrupt baglaminin DISINDAN- veriyor. */
    keyboard_set_callback(demo_stop_key_handler);
}

/*
 * multitask_pump() - kernel_main() idle dongusunden HER TURDA
 * cagrilir. Demo calisirken task_yield() ile CPU'yu demo task'lara
 * verir; durma istegi (herhangi bir tus) geldiginde task'lari
 * durdurup normal shell klavye yoneticisini geri yukler.
 */
static void multitask_pump(void) {
    if (!demo_tasks_running) return;

    if (demo_stop_requested) {
        /* Durma sinyali: task'lari sonlandir, birkac yield ile
         * temizlenmelerine (sched_next icindeki TASKf_KILL_TASK
         * temizligi) firsat ver */
        demo_tasks_running = 0;
        for (int i = 0; i < 8; i++) {
            task_yield();
        }

        /* Normal shell klavye yoneticisini geri yukle */
        keyboard_set_callback(shell_key_handler);

        con_set_color(10, 15);
        con_print("\nMultitask demo durduruldu.\n");
        con_set_color(0, 15);
        shell_prompt();
        return;
    }

    /* Demo task'lara CPU vermek icin yield et (idle dongude her
     * turda bir kez - hlt() yerine bunu cagiriyoruz, bkz. kernel_main) */
    task_yield();
}

/*
 * multistop komutu artik kullanilmiyor (multitask_start kendi
 * ici dongusunde herhangi bir tusla durduruluyor), ama shell_exec
 * icinde komut olarak yazilirsa kullanici dostu bir mesaj versin.
 */
static void multitask_stop(void) {
    con_set_color(14, 15);
    con_print("Multitask demo calisirken herhangi bir tusa basarak durdurabilirsiniz.\n");
    con_set_color(0, 15);
}


/* ------------------------------------------------
 * Test butonu - buton widget sistemini denemek icin.
 * ------------------------------------------------ */
static void on_hello_click(void) {
    con_print("\nButona tiklandi!\n");
}

/* ------------------------------------------------
 * Acilis banner'i
 * TempleOS "TempleOS V%5.3f\t%D %T\n\n" karsiligi.
 * ------------------------------------------------ */
static void print_banner(void) {
    /* Ust kisim: mavi arka plan (satirin tamami) - baslik yazisi
     * artik statik degil, marquee_create() ile kayan yazi olarak
     * kernel_main()'de bir kere olusturuluyor (bkz. asagida).
     * Sag taraf (58. sutundan itibaren) RTC saat icin ayrilmis. */
    con_print_at(0, 0,
        "                                                "
        "                                ",
        15, 1);

    /* Ana ekran: BEYAZ arka plan */
    con_set_color(0, 15);
    con_set_cursor(0, 2);

    con_set_color(14, 15);   /* Sari */
    con_print(" TKOS v2.07 x86_64\n");
    con_set_color(0, 15);    /* Acik gri */
    con_print(" Compile Time: 2026_07_01\n");
    con_set_color(0, 15);   /* siyah */
}

/* ------------------------------------------------
 * kernel_main()
 * TempleOS KMain() baslangic sirasi ile paralel.
 * kernel_entry.asm tarafindan cagrilir.
 * ------------------------------------------------ */
void kernel_main(void) {

    /* ---- 1. Grafik altyapisi --------------------------------
     * TempleOS: SysGrInit -> text.font, text.cols/rows
     * Bizim: fb_init() + set_palette() + con_init()
     * -------------------------------------------------------- */
    if (!fb_init()) {
        /* VBE basarisiz: duraklat (ekran yok) */
        for (;;) __asm__ volatile ("hlt");
    }

    /* 8bpp DAC palet yukle - fb_console renk indeksleriyle eslesir */
    set_palette();

    /* Konsolu baslat: fg=siyah(0), bg=beyaz(15) */
    con_init(0, 15);
    con_clear();

    /* ---- 2. Bellek yoneticisi --------------------------------
     * TempleOS: BlkPoolsInit
     * -------------------------------------------------------- */
    alloc_init();

    /* ---- 2b. Genel amacli (byte granulariteli) heap ----------
     * alloc.c uzerine kurulu kmalloc/kfree/kcalloc.
     * alloc_init()'ten HEMEN SONRA cagrilmali (alloc_pages_raw'a
     * bagimli). TempleOS Kernel/Mem/MAllocFree.HC karsiligi.
     * -------------------------------------------------------- */
    kheap_init();

    /* ---- 3. PIC - donanim kesme denetleyicisi ---------------
     * TempleOS: IntsInit
     * -------------------------------------------------------- */
    pic_init();

    /* ---- 4. IDT - kesme tanimlayici tablosu -----------------
     * TempleOS: IntInit1 + IntInit2
     * -------------------------------------------------------- */
    idt_init();

    /* ---- 5. Klavye IRQ handler'ini bagla --------------------
     * TempleOS: KbdInit
     * keyboard_handler() -> PS/2 scancode -> callback
     * -------------------------------------------------------- */
    idt_set_handler(I_KEYBOARD, kbd_irq_handler);
    pic_unmask_irq(IRQ_KEYBOARD);

    /* ---- 5b. Fare (PS/2) IRQ handler'ini bagla --------------
     * mouse_init(): aux aygiti ac, IRQ12'yi etkinlestir,
     * veri raporlamayi ac. Cursor en son, ekran ciziminden
     * sonra gosterilecek (bkz. asagida mouse_show_cursor()).
     * -------------------------------------------------------- */
    idt_set_handler(I_MOUSE, mouse_handler);
    mouse_init();
    pic_unmask_irq(IRQ_MOUSE);

    /* Fare olaylarini buton widget sistemine yonlendir - butonlar
     * mouse_handle_mouse() icinde basma/birakma/tiklama mantigini
     * kendisi isliyor. */
    mouse_set_callback(button_handle_mouse);

    /* ---- 6. PIT - zamanlayici ------------------------------
     * TempleOS: TimersInit
     * IRQ0 handler'i pit.c tarafindan kayit edilir.
     * -------------------------------------------------------- */
    pit_init();

    /* ---- 7. Zamanlayici -----------------------------------
     * TempleOS: Core0Init
     * -------------------------------------------------------- */
    sched_init();

    /* pit_irq_handler'in sched_tick() cagirmasini sagla */
    /* (pit.c'de pit_irq_handler icinde sched_tick() cagrisi var) */

    /* ---- 7b. FAT16 dosya sistemi ---------------------------
     * ata.c uzerinden diskteki FAT16 bolumunu (sektor
     * FAT16_PARTITION_LBA'den itibaren, host'ta 'mkfs.fat -F 16
     * --offset=2048' ile olusturulan bolum) baglar.
     *
     * IKI DISK: birincil disk (ATA_DRIVE_MASTER, tkos.img'in kendisi)
     * ve varsa ikincil disk (ATA_DRIVE_SLAVE, Limbo/QEMU'da ayrica
     * baglanmis 2. .img) BAGIMSIZ olarak mount edilir - bkz. g_vol0/
     * g_vol1 ve ls/cat (master) - ls2/cat2 (slave) komutlari.
     *
     * Ikincil disk bagli degilse ata_drive_present() hizlica -1
     * dondurur (uzun timeout'a takilmadan), sadece bir bilgi mesaji
     * yazip devam edilir - kernel boot etmeye devam eder. */
    if (fat16_init(ATA_DRIVE_MASTER, &g_vol0) != 0) {
        con_set_color(14, 15);
        con_print("[FAT16] Birincil disk baglama basarisiz - disk formatlanmamis olabilir.\n");
        con_set_color(0, 15);
    }

    if (fat16_init(ATA_DRIVE_SLAVE, &g_vol1) != 0) {
        con_set_color(8, 15);
        con_print("[FAT16] Ikincil disk bulunamadi/baglanamadi (opsiyonel).\n");
        con_set_color(0, 15);
    }

    /* ---- 8. Klavye callback'ini ayarla --------------------
     * TempleOS: KbdMsInit -> CFifoU8 aktif
     * -------------------------------------------------------- */
    keyboard_set_callback(shell_key_handler);

    /* ---- 9. Interrupt'lari ac ----------------------------
     * TempleOS: SetRFlags(RFLAGG_NORMAL)
     * -------------------------------------------------------- */
    sti();
    pcspk_play_melody(melody_boot);
    print_banner();  
    /* ---- 10. Acilis mesaji --------------------------------
     * TempleOS: "TempleOS V%5.3f\t%D %T\n\n"
     * -------------------------------------------------------- */
    print_banner();

    /* Baslangic istatistikleri */
    con_set_color(8, 15);    /* Koyu gri */
    con_print(" [MEM]  ");
    con_print_dec(alloc_get_free() * 4);
    con_print(" KB bos\n");
    con_print(" [FB]   ");
    con_print_dec(fb_width());
    con_print("x");
    con_print_dec(fb_height());
    con_print("x");
    con_print_dec(fb_bpp());
    con_print("bpp\n\n");
    con_set_color(0, 15);

    /* Ekran icerigi (banner + istatistikler) tamamen cizildikten
     * SONRA cursor'i goster - erken cagrilirsa metin cursor'in
     * uzerine yazar ve fb_get_pixel yanlis arka plani kaydeder. */
    mouse_show_cursor();

    con_set_color(0, 15);

    /* Ust bar baslik yazisi - kayan (marquee). Sadece bir kere olusturuluyor
     * (print_banner() birden fazla cagrilsa bile burada tekrar cagrilmaz).
     * Genislik 58*8=464px: RTC saatin bulundugu 58. sutuna kadar, sag
     * taraftaki saati oralim diye kaplamiyor. speed_div=8 -> ~saniyede
     * 125px, orta hizli akici bir kayma. */
    marquee_create(0, 0, 58 * 8, 8, "  TKOS v2.07  ", 15, 1, 8);

    /* Test butonu - buton widget sistemini denemek icin */
    button_create(200, 100, 80, 24, "Merhaba", on_hello_click);

    /* ---- 11. Shell ana dongusu ---------------------------
     * TempleOS: SrvTaskCont (hic donmez)
     * -------------------------------------------------------- */
    shell_prompt();

    /* Idle dongu: saat/tarih guncellemesi + (varsa) multitask demo
     * task'larina CPU verme burada, interrupt baglaminin DISINDA
     * yapiliyor - bkz. multitask_pump() ve maybe_update_clock()
     * yorumlari. Demo calismiyorken normal sekilde hlt() ile
     * bir sonraki kesmeyi bekliyoruz. */
    for (;;) {
        maybe_update_clock();

        if (demo_tasks_running) {
            multitask_pump();
        } else if (melody_pending) {
            melody_pump();
        } else {
            __asm__ volatile ("hlt");
        }
    }
}
