echo "Compiling..."

for file in `ls`
do
	extension="${file##*.}"
    filename="${file%_test.c}"
    if [[ "$extension" = "c" ]]
    then
	    gcc $file -o $filename -lpthread 
	fi
done

echo "Running tests..."

for file in `ls`
do
	extension="${file##*.}"
    filename="${file%_test.c}"
    if [[ "$extension" = "c" ]]
    then
        ./$filename 
	fi
done

echo "Cleaning up..."

for file in `ls`
do
	extension="${file##*.}"
    filename="${file%_test.c}"
    if [[ "$extension" = "c" ]]
    then
	    rm $filename
	fi
done

