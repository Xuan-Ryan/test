echo "Start patch customize file"
cd mctconfig/patch
subfolder=`ls`

for d in $subfolder
do
	if [ $d = "do-patch.sh" ]; then
		continue
	fi
	cd $d
	./patch.sh
	cd ..
done