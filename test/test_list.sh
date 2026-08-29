#!/bin/bash

# phiola test: `list` commands

test_list() {
	if [[ ! -f list3.ogg ]]; then
		./phiola rec -o list1.wav -f -u 1
		./phiola co list1.wav -m artist=A2 -m title=T2 -f -o list2.ogg
		./phiola co list1.wav -m artist=A3 -m title=T3 -f -o list3.ogg
	fi

	./phiola list create . -include "./list*.ogg" -o test.m3u
	./phiola info test.m3u | grep '#1 "A2 - T2" "./list2.ogg"'

	./phiola list create list3.ogg list2.ogg -o test-sort.m3u
	./phiola list create list3.ogg list2.ogg -o test-sort2.m3u
	./phiola list sort test-sort.m3u test-sort2.m3u
	./phiola info test-sort.m3u | grep '#1 "A2 - T2" "list2.ogg"'
	./phiola info test-sort2.m3u | grep '#1 "A2 - T2" "list2.ogg"'

	cat >/tmp/phiola-test.pls <<EOF
[playlist]
File1=`pwd`/list1.wav
Title1=TITLE1
Length1=1
File2=`pwd`/list2.ogg
Title2=TITLE2
Length2=2
EOF
	./phiola i /tmp/phiola-test.pls
}

test_list_heal() {
	mkdir -p phi_test phi_test/listheal

	cat >phi_test/list.m3u <<EOF
#EXTM3U
#EXTINF:1,ARTIST - equal
listheal/file-eq.mp3
#EXTINF:1,ARTIST - abs-out-of-scope
/tmp/listheal/file-oos.mp3
#EXTINF:1,ARTIST - rel-out-of-scope
../listheal/file-oos.mp3
#EXTINF:1,ARTIST - abs-rel
$(pwd)/phi_test/listheal/file-abs.mp3
#EXTINF:1,ARTIST - norm
././/listheal//file-abs.mp3
#EXTINF:1,ARTIST - chg-ext
listheal/file.mp3
#EXTINF:1,ARTIST - chg-dir
listheal/dir1/dir2/file-cd.mp3
#EXTINF:1,ARTIST - chg-dir-ext
listheal/dir1/dir2/file-cde.mp3
EOF

	cat >phi_test/list2.m3u <<EOF
#EXTM3U
#EXTINF:1,ARTIST - equal
listheal/file-eq.mp3
#EXTINF:1,ARTIST - abs-out-of-scope
/tmp/listheal/file-oos.mp3
#EXTINF:1,ARTIST - rel-out-of-scope
../listheal/file-oos.mp3
#EXTINF:1,ARTIST - abs-rel
listheal/file-abs.mp3
#EXTINF:1,ARTIST - norm
listheal/file-abs.mp3
#EXTINF:1,ARTIST - chg-ext
listheal/file.ogg
#EXTINF:1,ARTIST - chg-dir
listheal/dir3/file-cd.mp3
#EXTINF:1,ARTIST - chg-dir-ext
listheal/dir3/file-cde.ogg
EOF

	touch phi_test/listheal/file-eq.mp3
	touch phi_test/listheal/file-abs.mp3
	touch phi_test/listheal/file.ogg
	mkdir -p phi_test/listheal/dir3
	touch phi_test/listheal/dir3/file-cd.mp3
	touch phi_test/listheal/dir3/file-cde.ogg

	./phiola list heal "phi_test/list.m3u"
	diff -Z phi_test/list.m3u phi_test/list2.m3u

	echo '#EXTM3U
#EXTINF:1,ARTIST - unchanged
listheal/file.ogg' >phi_test/list.m3u
	echo '#EXTM3U
#EXTINF:1,ARTIST - unchanged
listheal/file.ogg' >phi_test/list2.m3u
	./phiola list heal "phi_test/list.m3u"
	diff -Z phi_test/list.m3u phi_test/list2.m3u

	rm -rf phi_test/listheal phi_test/list*.m3u
	rmdir phi_test
}
