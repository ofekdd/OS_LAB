SRC=/usr/src/linux-2.4.18-14custom/

rsync -rc $1/ $SRC/ || exit 1
cd $SRC

make bzImage || exit 2
make modules  || exit 3
make modules_install  || exit 4

make install  || exit 5
cd /boot
mkinitrd -f 2.4.18-14custom.img 2.4.18-14custom || exit 6
