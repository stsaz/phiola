set -xe
DIR=$1
shift
MODS=("$@")

for M in "${MODS[@]}" ; do
	mv $DIR/$M $DIR/lib$M
done
