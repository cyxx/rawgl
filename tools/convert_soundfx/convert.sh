set -x
for num in 7 137 138; do
	filename=soundfx$num.raw
	./convert_soundfx $num
	mv out.raw $filename
	sox -r 22050 -e signed -b 8 -c 2 $filename soundfx$num.wav
	oggenc soundfx$num.wav
	rm -f $filename soundfx$num.wav
done
