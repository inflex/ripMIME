d=$(dirname "$0");
d=$(readlink -f "$d");

tmpdir='/tmp';
#make clean > /dev/null;
#make;
[ ! -f "$d/ripmime" ] && exit;
[ -d "$tmpdir/ripmime" ] && rm -r "$tmpdir/ripmime";
mkdir "$tmpdir/ripmime";
tmpdir="$tmpdir/ripmime";
err=0;

ls -1 "$d/test_data" | while read t; do
	dt="$tmpdir/$t";
	mkdir "$dt" "${dt}_res";
	cd "$dt";
	"$d/ripmime" -i "$d/test_data/$t" -d "$dt" -v > "${dt}_res/output0";
	echo ---- $t ----;
	cat "${dt}_res/output0";
	echo;
	ls -1 "$dt" | while read t1; do
		sha512sum "$dt/$t1" >> "${dt}_res/sha512sum";
	done;
	diff "$d/expected/${t}_res/output0" "${dt}_res/output0";
	od=$?;
	[ "$od" != "0" ] && err=$(($err + 1));
	diff "$d/expected/${t}_res/sha512sum" "${dt}_res/sha512sum";
	sd=$?;
	[ "$sd" != "0" ] && err=$(($err + 1));
done;

 [ "$err" != "0" ] && echo "no errors" || echo "THERE WAS $err ERRORS";
