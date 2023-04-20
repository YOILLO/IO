# Лабораторная работа 2

**Название:** "Разработка драйверов блочных устройств"

**Цель работы:** 

## Описание функциональности драйвера

...

## Инструкция по сборке

Сборка модуля:

```
make
```

Сборка с последующей установкой:

```
make install
```

Отключение модуля:

```
make uninstall
```

Пересборка и перезапуск модуля:

```
make reinstall
```

Очистка:

```
make clean
```

## Инструкция пользователя

sudo fdisk -l /dev/vramdisk
Disk /dev/vramdisk: 50 MiB, 52428800 bytes, 102400 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x36e5756d

Device         Boot Start    End Sectors Size Id Type
/dev/vramdisk1          1  40960   40960  20M 83 Linux
/dev/vramdisk2      40961 102400   61440  30M  5 Extended
/dev/vramdisk5      40961  61440   20480  10M 83 Linux
/dev/vramdisk6      61441  81920   20480  10M 83 Linux
/dev/vramdisk7      81921 102400   20480  10M 83 Linux

mkfs.vfat /dev/vramdisk1
mkfs.vfat /dev/vramdisk5 

mount -t vfat /dev/vramdisk1 /mnt/disk1
mount -t vfat /dev/vramdisk5 /mnt/disk5


## Примеры использования

fallocate -l 7M /mnt/disk1/file

pv /mnt/disk1/file > /mnt/disk5/file
7,00MiB 0:00:00 [84,2MiB/s] [================================>] 100% 

pv /mnt/disk1/file > ~/file
7,00MiB 0:00:00 [ 234MiB/s] [================================>] 100% 

