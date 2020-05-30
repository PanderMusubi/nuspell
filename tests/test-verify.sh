cat v1cmdline/base.good v1cmdline/base.wrong | \
    ../build/tests/verify -d v1cmdline/base

paste v1cmdline/base.wrong v1cmdline/base.sug | \
    awk '{print $1"\t"$2}' | \
    grep -v , > corrections.tsv
../build/tests/verify -d v1cmdline/base -c corrections.tsv v1cmdline/base.good
