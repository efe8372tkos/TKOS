----------TKOS---------- *Çalıştırma Adımları*

x86 Mimarisi 64 Bit OS Projesi.
vmware, QEMU, VirtualBox ve Limbo x86 PC Emulator ile calisabilir.
Limbo PC Emulator ile CALISTIRMA ADIMLARI:

HDA(Hard disk A) kismina tkos.img koyun.
HDB(Hard disk B) kismina tkos_disk2.img koyun(YOKSA SISTEM BASLAMAZ!!)
SES Kartı(Sound Card) Olarak --pcspk-- secmelisiniz.
ISLEMCI(CPU MODEL) Olarak --QEMU64-- secin.
EN AZ ----64 MB---- RAM VERMELISINIZ.
Boot kısmından "boot from device" bolmesinden "BOOT FROM HARD DISK"
seçin.

Cdrom, Floppy veya network card GEREKSIZDIR. Bos bırakabilirsiniz.

ONEMLI UYARILAR:

-Butona tiklandiktan sonra ENTER basarsanız, terminal yeniden yazmaniza
izin verir.

-henüz sistem %100 stabil sayılmaz, eğer bir bug bulursaniz,
bana bildirmenizden memnun olurum(YT: Efe8372=@EFE0404BY)

-Eger ikinci diski takmazsaniz, beyaz ekranda donar, açılmaz.

-Terminale help yazarak komut listesini görebilirsiniz.

-Açılış Melodisi yaklaşık 20 saniye, biraz fazla oldu, sorry :D

-Kodda değişiklik yapıp yeniden derlemek isterseniz, bootable kismina
eklediğim tkos_HowToCompile.txt dosyasindaki komutları birebir
alabilirsiniz, Clang veya GCC ile yapılabilir. 


