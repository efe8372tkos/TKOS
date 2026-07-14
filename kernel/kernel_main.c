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
static fat16_volume_t g_vol0;
static fat16_volume_t g_vol1;
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
    while (cmos_update_in_progress()) {  }
    t->sec   = cmos_read(0x00);
    t->min   = cmos_read(0x02);
    t->hour  = cmos_read(0x04);
    t->day   = cmos_read(0x07);
    t->month = cmos_read(0x08);
    t->year  = cmos_read(0x09);
}
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
    if (!(status_b & 0x04)) {   
        b.sec   = bcd2bin(b.sec);
        b.min   = bcd2bin(b.min);
        b.hour  = (uint8_t)(bcd2bin((uint8_t)(b.hour & 0x7F)) | (b.hour & 0x80));
        b.day   = bcd2bin(b.day);
        b.month = bcd2bin(b.month);
        b.year  = bcd2bin((uint8_t)b.year);
    }
    if (!(status_b & 0x02) && (b.hour & 0x80)) {  
        b.hour = (uint8_t)(((b.hour & 0x7F) + 12) % 24);
    }
    out->sec   = b.sec;
    out->min   = b.min;
    out->hour  = b.hour;
    out->day   = b.day;
    out->month = b.month;
    out->year  = (uint16_t)(2000 + b.year); 
}
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
    con_print_at(58, 0, buf, 15, 1);  
}
static uint64_t clock_last_sec = (uint64_t)-1;
static void maybe_update_clock(void) {
    uint64_t secs = pit_get_ticks() / 1000;
    if (secs != clock_last_sec) {
        clock_last_sec = secs;
        draw_clock();
    }
}
static const uint8_t palette16[16][3] = {
    {  0,  0,  0 },   
    {  0,  0, 42 },   
    {  0, 42,  0 },   
    {  0, 42, 42 },   
    { 42,  0,  0 },   
    { 42,  0, 42 },   
    { 42, 21,  0 },   
    { 42, 42, 42 },   
    { 21, 21, 21 },   
    { 21, 21, 63 },   
    {  0, 50,  8 },   
    {  0, 45, 45 },   
    { 63, 21, 21 },   
    { 63, 21, 63 },   
    { 51, 38,  0 },   
    { 63, 63, 63 },   
};
static void set_palette(void) {
    outb(0x3C8, 0);                 
    for (int i = 0; i < 16; i++) {
        outb(0x3C9, palette16[i][0]);   
        outb(0x3C9, palette16[i][1]);   
        outb(0x3C9, palette16[i][2]);   
    }
}
static void kbd_irq_handler(int_frame_t *frame) {
    (void)frame;
    keyboard_handler();     
}
#define SHELL_BUF_SIZE 256
static char    shell_buf[SHELL_BUF_SIZE];
static uint32_t shell_len = 0;
static uint8_t  shell_fg  = 0;   
static uint8_t  shell_bg  = 15;    
static void multitask_start(void);
static void multitask_stop(void);
static void shell_prompt(void);  
static const music_note_t *melody_pending  = 0;   
static volatile int         melody_playing = 0;    
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
#define SHELL_ALLOC_MAX  32
typedef struct {
    void    *ptr;
    uint64_t size;   
    int      used;
} shell_alloc_t;
static shell_alloc_t shell_allocs[SHELL_ALLOC_MAX];
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
static int shell_alloc_track_find(void *ptr) {
    for (int i = 0; i < SHELL_ALLOC_MAX; i++)
        if (shell_allocs[i].used && shell_allocs[i].ptr == ptr)
            return i;
    return -1;
}
static const char *shell_arg_after(const char *cmd, const char *word) {
    size_t wlen = strlen(word);
    if (strncmp(cmd, word, wlen) != 0) return NULL;
    if (cmd[wlen] == '\0') return cmd + wlen;
    if (cmd[wlen] != ' ')  return NULL;
    const char *p = cmd + wlen + 1;
    while (*p == ' ') p++;
    return p;
}
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
    con_set_color(11, 15);  
    con_print("Dosyalar (");
    con_print_dec(count);
    con_print("):\n");
    con_set_color(0, 15);
    for (int i = 0; i < count; i++) {
        if (entries[i].attr & FAT_ATTR_DIRECTORY) {
            con_set_color(9, 15);  
            con_print("  [DIR]  ");
        } else {
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
static void shell_exec(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return;
    if (strcmp(cmd, "clear") == 0) {
        con_clear();
        return;
    }
    if (strcmp(cmd, "help") == 0) {
        con_set_color(14, 15);  
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
        con_print("  reboot  - Restart system\n");
        return;
    }
    if (strcmp(cmd, "mem") == 0) {
        con_set_color(11, 15);  
        alloc_print_stats();
        con_set_color(0, 15);
        return;
    }
    if (strcmp(cmd, "heap") == 0) {
        con_set_color(11, 15);  
        heap_print_stats();
        con_set_color(0, 15);
        return;
    }
    {
        const char *arg = shell_arg_after(cmd, "alloc");
        if (arg) { shell_cmd_alloc(arg); return; }
    }
    {
        const char *arg = shell_arg_after(cmd, "free");
        if (arg) { shell_cmd_free(arg); return; }
    }
    if (strcmp(cmd, "allocs") == 0) {
        shell_cmd_allocs();
        return;
    }
    if (strcmp(cmd, "tasks") == 0) {
        con_set_color(11, 15);  
        sched_print_tasks();
        con_set_color(0, 15);
        return;
    }
    if (strcmp(cmd, "tskdemo") == 0) {
        multitask_start();
        return;
    }
    if (strcmp(cmd, "multistop") == 0) {
        multitask_stop();
        return;
    }
    if (strcmp(cmd, "play") == 0) {
        if (melody_playing) {
            con_set_color(14, 15);
            con_print("Zaten bir melodi calmakta.\n");
            con_set_color(0, 15);
            return;
        }
        con_set_color(11, 15);  
        con_print("Melodi calmiyor... (idle dongude calisir)\n");
        con_set_color(0, 15);
        melody_pending = melody_custom;
        return;
    }
    if (strcmp(cmd, "ls") == 0) {
        shell_cmd_ls(&g_vol0, "birincil disk");
        return;
    }
    if (strcmp(cmd, "ls2") == 0) {
        shell_cmd_ls(&g_vol1, "ikincil disk");
        return;
    }
    {
        const char *arg = shell_arg_after(cmd, "cat");
        if (arg) {
            shell_cmd_cat(&g_vol0, arg);
            return;
        }
    }
    {
        const char *arg = shell_arg_after(cmd, "cat2");
        if (arg) {
            shell_cmd_cat(&g_vol1, arg);
            return;
        }
    }
    {
        const char *arg = shell_arg_after(cmd, "exec");
        if (arg) { shell_cmd_exec(&g_vol0, arg); return; }
    }
    {
        const char *arg = shell_arg_after(cmd, "exec2");
        if (arg) { shell_cmd_exec(&g_vol1, arg); return; }
    } 
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
    if (strcmp(cmd, "reboot") == 0) {
        cli();
        outb(0x70, 0x8F);
        outb(0x71, 0x00);
        outb(0x92, inb(0x92) | 1);
        for (;;) __asm__ volatile ("hlt");
    }
    con_set_color(12, 15);  
    con_print("Bilinmeyen komut: ");
    con_print(cmd);
    con_print("\n");
    con_set_color(0, 15);
}
static void shell_prompt(void) {
    con_set_color(10, 15);   
    con_print("TK: ");
    con_set_color(0, 15);   
}
static void shell_key_handler(char c) {
    if (c == '\n') {
        con_putchar('\n');
        shell_buf[shell_len] = '\0';
        if (shell_len > 0) {
            shell_exec(shell_buf);
        }
        shell_len = 0;
        shell_prompt();
    } else if (c == '\b') {
        if (shell_len > 0) {
            shell_len--;
            con_putchar('\b');
        }
    } else {
        if (shell_len < SHELL_BUF_SIZE - 1) {
            shell_buf[shell_len++] = c;
            con_putchar(c);
        }
    }
}
#define DEMO_TASK_ROW_BASE   56   
#define DEMO_TASK_COUNT      3
static volatile int demo_tasks_running = 0;
static task_t *demo_task_handles[DEMO_TASK_COUNT];
static void demo_task_1(void) {
    uint64_t counter = 0;
    char buf[32];
    while (demo_tasks_running) {
        counter++;
        con_print_at(2, DEMO_TASK_ROW_BASE, "Task1: ", 12, 15); 
        utoa(counter, buf, 10);
        con_print_at(9, DEMO_TASK_ROW_BASE, "          ", 12, 15); 
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
        con_print_at(2, DEMO_TASK_ROW_BASE + 1, "Task2: ", 10, 15); 
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
        con_print_at(2, DEMO_TASK_ROW_BASE + 2, "Task3: ", 9, 15); 
        utoa(counter, buf, 10);
        con_print_at(9, DEMO_TASK_ROW_BASE + 2, "          ", 9, 15);
        con_print_at(9, DEMO_TASK_ROW_BASE + 2, buf, 9, 15);
        task_yield();
    }
    task_kill(sched_get_current());
    for (;;) task_yield();
}
static volatile int demo_stop_requested = 0;
static void demo_stop_key_handler(char c) {
    (void)c;
    demo_stop_requested = 1;
}
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
    keyboard_set_callback(demo_stop_key_handler);
}
static void multitask_pump(void) {
    if (!demo_tasks_running) return;
    if (demo_stop_requested) {
        demo_tasks_running = 0;
        for (int i = 0; i < 8; i++) {
            task_yield();
        }
        keyboard_set_callback(shell_key_handler);
        con_set_color(10, 15);
        con_print("\nMultitask demo durduruldu.\n");
        con_set_color(0, 15);
        shell_prompt();
        return;
    }
    task_yield();
}
static void multitask_stop(void) {
    con_set_color(14, 15);
    con_print("Multitask demo calisirken herhangi bir tusa basarak durdurabilirsiniz.\n");
    con_set_color(0, 15);
}
static void on_hello_click(void) {
    con_print("\nButona tiklandi!\n");
}
static void print_banner(void) {
    con_print_at(0, 0,
        "                                                "
        "                                ",
        15, 1);
    con_set_color(0, 15);
    con_set_cursor(0, 2);
    con_set_color(14, 15);   
    con_print(" TKOS v2.07 x86_64\n");
    con_set_color(0, 15);    
    con_print(" Compile Time: 2026_07_01\n");
    con_set_color(0, 15);   
}
void kernel_main(void) {
    if (!fb_init()) {
        for (;;) __asm__ volatile ("hlt");
    }
    set_palette();
    con_init(0, 15);
    con_clear();
    alloc_init();
    kheap_init();
    pic_init();
    idt_init();
    idt_set_handler(I_KEYBOARD, kbd_irq_handler);
    pic_unmask_irq(IRQ_KEYBOARD);
    idt_set_handler(I_MOUSE, mouse_handler);
    mouse_init();
    pic_unmask_irq(IRQ_MOUSE);
    mouse_set_callback(button_handle_mouse);
    pit_init();
    sched_init();
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
    keyboard_set_callback(shell_key_handler);
    sti();
    pcspk_play_melody(melody_boot);
    print_banner();  
    print_banner();
    con_set_color(8, 15);    
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
    mouse_show_cursor();
    con_set_color(0, 15);
    marquee_create(0, 0, 58 * 8, 8, "  TKOS v2.07  ", 15, 1, 8);
    button_create(200, 100, 80, 24, "Merhaba", on_hello_click);
    shell_prompt();
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
