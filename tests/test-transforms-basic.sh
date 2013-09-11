#!/bin/bash

mypath=`dirname $0`
r=$mypath/../sonic-annotator

infile=$mypath/audio/3clicks8.wav
testplug=vamp:vamp-example-plugins:percussiononsets
tmpfile1=$mypath/tmp_1_$$
tmpfile2=$mypath/tmp_2_$$

trap "rm -f $tmpfile1 $tmpfile2" 0

. test-include.sh

$r --skeleton $testplug > $tmpfile1 2>/dev/null || \
    fail "Fails to run with --skeleton $testplug"

$r -t $tmpfile1 -w csv --csv-stdout $infile > $tmpfile2 2>/dev/null || \
    fail "Fails to run with -t $tmpfile -w csv --csv-stdout $infile"

csvcompare $tmpfile2 $mypath/expected/transforms-basic-skeleton-1.csv || \
    fail "Output mismatch for transforms-basic-skeleton-1.csv"

for suffix in \
    -no-parameters-default-output \
    -no-parameters \
    "" \
    -set-parameters \
    -set-step-and-block-size \
    -set-sample-rate \
    -df-windowtype-default \
    -df-windowtype-hanning \
    -df-windowtype-hamming \
    -multiple-outputs \
    ; do

    for type in xml n3 ; do 

	transform=$mypath/transforms/transforms-basic-percussiononsets$suffix.$type
	expected=$mypath/expected/transforms-basic-percussiononsets$suffix.csv

	if [ ! -f $transform ]; then
	    if [ $type = "xml" ]; then
		continue # not everything can be expressed in the XML
			 # format, e.g. the multiple output test can't
	    fi
	fi

	test -f $transform || \
	    fail "Internal error: no transforms file for suffix $suffix (looking for $transform)"

	test -f $expected || \
	    fail "Internal error: no expected output file for suffix $suffix (looking for $expected)"

	$r -t $transform -w csv --csv-stdout $infile > $tmpfile2 2>/dev/null || \
	    fail "Fails to run transform $transform"

	csvcompare $tmpfile2 $expected || \
	    fail "Output mismatch for transform $transform"
    done
done

exit 0

