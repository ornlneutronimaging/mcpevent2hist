#!/bin/bash
# Process all .tpx3 files in data/ (that have GDC timestamps...)
#
# usage:
#   $0 [output-file-extention [number-of-iterations] ]
#
# where:
#   output-file-extention is either { h5, csv, or bin } as supported by FastSophireadBenchmarksCLI.app (default h5)
#   number-of-iterations is how many times to repeat a clustering to obtain stable performance metrics (default 3)
#
# description:
#   The program processes .tpx3 files under a variety of parameters (w/ and w/o memory mapping, primarily)
# it produces data output files of clustered events for every iteration, collects information on the time used
# in each iteration, and compares the result of each iteration to check that the same answers are obtained.

EXT=${1:-h5}
# TODO: data/suann_socket_background_serval32.tpx3 produces poor performance statistics
LST="data/HV2700_1500_500_THLrel_274_sophy_chopper_60Hz_4.1mm_aperture_siemen_star_120s_000000.tpx3"
ITR=${2:-3}

# meaningful comparisons of hdf5 files requires h5diff
if [ "$EXT" == "h5" ] ; then
    DIFF="h5diff -v"
elif [ "$EXT" == "bin" ] ; then
    DIFF="diff -bq"
else
    DIFF="diff -s"
fi

for f in $LST ; do
    x=$(basename $f)
    y=${x%.*}
    result=result/$y
    mkdir -p $result
    # need a fixed output for comparisons
    echo "*** baseline output for $x"

    # TODO: the Sophiread program produces an .h5 file that slightly differs from app, so cannot be used at the moment
    #
    # Not comparable: </neutrons/nHits> is of class H5T_FLOAT and </neutrons/nHits> is of class H5T_INTEGER
    # Not comparable: </neutrons/tof> has rank 1, dimensions [190208], max dimensions [190208]
    #   and </neutrons/tof> has rank 1, dimensions [190207], max dimensions [190207]
    # ...
    # seems like some minor adjustments are needed

    ##if [ "$EXT" == "h5" ] ; then
    ##    ./Sophiread -i $f -E ${result}/${y}.verify.h5 ;
    ##else
        ./FastSophireadBenchmarksCLI.app $f ${result}/${y}.verify.${EXT} verify+tgdc
    ##fi
    echo ""
    for m in stream mmap ; do
        for c in tbb verify ; do
            CFG=${m}+${c}+tgdc
            echo "" > ${result}/${CFG}.result
            echo "*** ${result}/${CFG} ***"
            for i in $(seq 1 ${ITR}) ; do
                echo "${CFG} try ${i}:" >> ${result}/${CFG}.info
                (time ./FastSophireadBenchmarksCLI.app $f ${result}/${y}.${CFG}.${i}.${EXT} ${CFG}) 2>>${result}/${CFG}.result >> ${result}/${CFG}.info
            done
            echo "${CFG} real time summary:" >> ${result}/${CFG}.info
            grep real ${result}/${CFG}.result >> ${result}/${CFG}.info
            echo "" >> ${result}/${CFG}.info
        done
    done
done
echo ""

echo "" > result/verify.info
echo "*** inspecting results of .${EXT} files produced ***"
differences=0
total=0
for f in $(find result -name "*.${EXT}") ; do
    d=$(dirname $f)
    b=$(basename $d)
    if [ -e ${d}/${b}.verify.${EXT} ] ; then
        total=$(($total + 1))
	echo "*** Compare ${d}/${b}.verify.${EXT} $f ***" >> result/verify.info
        if ( $DIFF ${d}/${b}.verify.${EXT} $f >> result/verify.info ) ; then
            echo "*** 0 exit code (no differences) ***" >> result/verify.info
	else
            differences=$(($differences + 1))
            echo "*** DIFFERENCES IDENTIFIED ***" >> result/verify.info
	fi
	echo "" >> result/verify.info
    fi
done
if (( $total == 0 )) ; then
    echo "NO FILEs were inspected; cannot comment on results"
elif (( $differences == 0 )) ; then
    echo "ALL $total result files matched for every iteration"
else
    echo "$differences/$total mis-matched see result/verify.info for details"
fi
echo ""

echo "" > result/speed.info
echo "*** report statistics on recorded benchmarks ***"
# https://unix.stackexchange.com/questions/24140/return-only-the-portion-of-a-line-after-a-matching-pattern
# https://stackoverflow.com/questions/9789806/command-line-utility-to-print-statistics-of-numbers-in-linux
# https://unix.stackexchange.com/questions/13731/is-there-a-way-to-get-the-min-max-median-and-average-of-a-list-of-numbers-in
( find result -type f -exec grep "Single-Thread" {} /dev/null \; ) | sed -n -e 's/^.*Single-Thread speed: //p' | sort -u | cut -f1 -d' ' > result/single-thread.csv
( find result -type f -exec grep "TBB Parallel" {} /dev/null \; ) | sed -n -e 's/^.*TBB Parallel speed: //p' | sort -u | cut -f1 -d' ' > result/tbb.csv
for f in result/*.csv ; do
    if [ -z "$(which R 2>/dev/null)" ] ; then
        echo "$f: (hits/sec)" | tee -a result/speed.info
        cat $f | python3 -c "import fileinput,statistics; i = [float(l.strip()) for l in fileinput.input()]; print('min:', f'{int(min(i)):,}', 'median:', f'{int(statistics.median(i)):,}', 'mean:', f'{int(statistics.mean(i)):,}', 'max:', f'{int(max(i)):,}')" | tee -a result/speed.info
        echo "" | tee -a result/speed.info
    else
        # R replicates its code and produces a newline
        # https://stackoverflow.com/questions/1581232/add-commas-into-number-for-output
        cat $f | R -q -e "formatC(summary(as.numeric(read.table(\"${f}\")[,1])),format=\"d\",big.mark=\",\")" | tee -a result/speed.info
    fi
done
