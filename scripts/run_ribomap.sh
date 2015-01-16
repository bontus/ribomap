#!/bin/bash
#=============================
# default parameters
#=============================
adapter=CTGTAGGCACCATCAAT
min_fplen=27
max_fplen=33
nproc=15 # threads
nmismatch=1
offset=12 # P-site offset
force=false
tabd_cutoff=0.01
#=============================
# pre-filled parameters
#=============================
src_dir=`dirname $0`
bin_dir=${src_dir}/../bin/
lib_dir=${src_dir}/../lib/
export PATH=${bin_dir}:$PATH
export LD_LIBRARY_PATH=${lib_dir}:$LD_LIBRARY_PATH
export DYLD_LIBRARY_PATH=${lib_dir}:$DYLD_LIBRARY_PATH
work_dir=${src_dir}/../
# preprocess
fasta_dir=${work_dir}data/fasta/
# star outputs
tmp_dir=${work_dir}alignment/
# salmon
sm_odir=${work_dir}sm_quant
# ribomap
output_dir=${work_dir}outputs
star_idx_dir=${work_dir}StarIndex/
# star index
rrna_idx=${star_idx_dir}contaminant/
transcript_idx=${star_idx_dir}transcript/
#=============================
# functions
#=============================
# print error message and usage message
# quit program if indicated
# error_msg $error_msg $quit
error_msg ()
{
    echo "$1"
    if [ "$2" = true ]; then
	echo "Usage: ./run_ribomap.sh --rnaseq_fq rnaseq.fq.gz --riboseq_fq riboseq.fq.gz --contaminant_fa contaminant.fa --transcript_fa transcript.fa --cds_range cds_range.txt"
	exit
    fi
}

# check whether file generated successfully at each single step
# quit program if file failed to be generated
# check_file $file_name $error_msg
check_file ()
{
    if [ ! -f "$1" ]; then
	echo "$2"
	exit
    fi
}

# filter out reads that can be aligned to contaminants
# filter_seq $input_reads $output_prefix $output_fa
filter_reads ()
{
    if [ "${force}" = true ] || [ ! -f "$3" ];  then
	if [ "${1##*.}" = gz ]; then
	    filter_params="--readFilesCommand zcat "${align_params}
	else
	    filter_params=${align_params}
	fi
	STAR --runThreadN $nproc --genomeDir ${rrna_idx} --readFilesIn $1 --outFileNamePrefix $2 --outStd SAM --outReadsUnmapped Fastx --outSAMmode NoQS ${filter_params} > /dev/null
	check_file "$3" "pipeline failed at filtering rrna!"
    else
	echo "filtering skipped."
    fi
}

# align reads to the transcriptome
# align_reads $input_reads $output_prefix $output_bam
align_reads ()
{
    if [ "${force}" = true ] || [ ! -f "$3" ]; then
	if [ "${1##*.}" = gz ]; then
	    filter_params="--readFilesCommand zcat "${align_params}
	else
	    filter_params=${align_params}
	fi
	STAR --runThreadN $nproc --genomeDir ${transcript_idx} --readFilesIn $1 --outFileNamePrefix $2 ${SAM_params} ${filter_params}
	check_file "$3" "pipeline failed at mapping reads to transcriptome!"
    else
	echo "alignment skipped."
    fi
}
#=============================
# read in command line args
# space separated
#=============================
while [[ $# > 1 ]]
do
    key="$1"
    shift
    case $key in
	--rnaseq_fq)
	    rnaseq_fq="$1"
	    shift
	    ;;
	--riboseq_fq)
	    riboseq_fq="$1"
	    shift
	    ;;
	--transcript_fa)
	    transcript_fa="$1"
	    shift
	    ;;
	--contaminant_fa)
	    contaminant_fa="$1"
	    shift
	    ;;
	--cds_range)
	    cds_range="$1"
	    shift
	    ;;
	--work_dir)
	    work_dir="$1"
	    # preprocess
	    fasta_dir=${work_dir}data/fasta/
	    # star outputs
	    tmp_dir=${work_dir}alignment/
	    # salmon
	    sm_odir=${work_dir}sm_quant
	    # ribomap
	    output_dir=${work_dir}outputs
	    shift
	    ;;
	--nproc)
	    nproc="$1"
	    shift
	    ;;
	--adapter)
	    adapter="$1"
	    shift
	    ;;
	--min_fplen)
	    min_fplen="$1"
	    shift
	    ;;
	--max_fplen)
	    max_fplen="$1"
	    shift
	    ;;
	--offset)
	    offset="$1"
	    shift
	    ;;
	--nmismatch)
	    nmismatch="$1"
	    shift
	    ;;
	--tabd_cutoff)
	    tabd_cutoff="$1"
	    shift
	    ;;
	--fasta_dir)
	    fasta_dir="$1"
	    shift
	    ;;
	--star_idx_dir)
	    star_idx_dir="$1"
	    rrna_idx=${star_idx_dir}contaminant/
	    transcript_idx=${star_idx_dir}transcript/
	    shift
	    ;;
	--alignment_dir)
	    tmp_dir="$1"
	    shift
	    ;;
	--sailfish_dir)
	    sm_odir="$1"
	    shift
	    ;;
	--output_dir)
	    output_dir="$1"
	    shift
	    ;;
	--force)
	    force="$1"
	    shift
	    ;;
	*)
            # unknown option
	    ;;
    esac
done

if [ -z "${riboseq_fq}" ]; then 
    error_msg "ribo-seq reads not provided!" true
elif [ ! -f ${riboseq_fq} ]; then
    error_msg "ribo-seq file not exist! ${riboseq_fq}" true
elif [ -z "${rnaseq_fq}" ]; then
    error_msg "RNA-seq reads not provided!" true
elif [ ! -f ${rnaseq_fq} ]; then
    error_msg "RNA-seq file not exist! ${rnaseq_fq}" true
elif [ -z "${contaminant_fa}" ]; then
    error_msg "contaminant fasta not provided! Filter step skipped." false
elif [ ! -f ${contaminant_fa} ]; then
    error_msg "contaminant fasta not exist! ${contaminant_fa}" false
elif [ -z "${cds_range}" ]; then
    error_msg "cds range not provided! assume transcript fasta only contain cds regions." false
elif [ ! -f ${cds_range} ]; then
    error_msg "cds range file not exist! ${cds_range}" false
fi

#============================================
# make directories
#============================================
# star params
align_params="--clip3pAdapterSeq ${adapter} --seedSearchLmax 10 --outFilterMultimapScoreRange 0 --outFilterMultimapNmax 255 --outFilterMismatchNmax ${nmismatch} --outFilterIntronMotifs RemoveNoncanonical"
SAM_params="--outSAMtype BAM Unsorted --outSAMmode NoQS --outSAMattributes NH NM" # --outSAMprimaryFlag AllBestScore"
mkdir -p ${fasta_dir}
mkdir -p ${tmp_dir}
mkdir -p ${output_dir}
rna_core=${rnaseq_fq##*/}
rna_core=${rna_core%%.*}
ribo_core=${riboseq_fq##*/}
ribo_core=${ribo_core%%.*}
#============================================
# step 1: filter rrna
#============================================
ornaprefix=${tmp_dir}${rna_core}_rrna_
oriboprefix=${tmp_dir}${ribo_core}_rrna_
rna_nrrna_fa=${ornaprefix}Unmapped.out.mate1
ribo_nrrna_fa=${oriboprefix}Unmapped.out.mate1
if [ ! -z "${contaminant_fa}" ] && [ -f ${contaminant_fa} ]; then
    echo "filtering contaminated reads"
    if [ "${force}" = true ] || [ ! -d ${rrna_idx} ];  then
	echo "building contaminant index..."
	mkdir -p ${rrna_idx}
	STAR --runThreadN $nproc --runMode genomeGenerate --genomeDir ${rrna_idx} --genomeFastaFiles ${contaminant_fa} --genomeSAindexNbases 5 --genomeChrBinNbits 11
    fi
    echo "filtering contaminants in RNA_seq..."
    filter_reads ${rnaseq_fq} ${ornaprefix} ${rna_nrrna_fa}
    echo "filtering contaminants in ribo_seq..."
    filter_reads ${riboseq_fq} ${oriboprefix} ${ribo_nrrna_fa}
else
    echo "skipped filter read step."
    rna_nrrna_fa=${rnaseq_fq}
    ribo_nrrna_fa=${riboseq_fq}
fi
#============================================
# step 2: map to transcriptome
#============================================
ornaprefix=${tmp_dir}${rna_core}_transcript_
oriboprefix=${tmp_dir}${ribo_core}_transcript_
rna_bam=${ornaprefix}Aligned.out.bam
ribo_bam=${oriboprefix}Aligned.out.bam
echo "aligning reads to transcriptome"
if [ "${force}" = true ] || [ ! -d ${transcript_idx} ];  then
    echo "building transcriptome index..."
    mkdir -p ${transcript_idx}
    STAR --runThreadN $nproc --runMode genomeGenerate --genomeDir ${transcript_idx} --genomeFastaFiles ${transcript_fa} --genomeSAindexNbases 11 --genomeChrBinNbits 12
fi
echo "aligning RNA_seq to transcriptome..."
align_reads ${rna_nrrna_fa} ${ornaprefix} ${rna_bam}
echo "aligning ribo_seq to transcriptome..."
align_reads ${ribo_nrrna_fa} ${oriboprefix} ${ribo_bam}
#============================================
# step 3: salmon expression quantification
#============================================
sm_out=${sm_odir}/quant_bias_corrected.sf
if [ "${force}" = true ] || [ ! -f ${sm_out} ]; then
    echo "running salmon quant..."
    salmon quant -t ${transcript_fa} -l U -a ${rna_bam} -o ${sm_odir} -p $nproc --bias_correct
    check_file ${sm_out} "pipeline failed at expression quantification!"
fi
#============================================
# step 4: run ribomap
#============================================
ribomap_out=${output_dir}/${ribo_core}
options="--fasta ${transcript_fa} --mrnabam ${rna_bam} --ribobam ${ribo_bam} --min_fplen ${min_fplen} --max_fplen ${max_fplen} --offset ${offset} --sf ${sm_out} --tabd_cutoff ${tabd_cutoff} --out ${ribomap_out} "
if [ ! -z "${cds_range}" ] && [ -f ${cds_range} ]; then
    options+=" --cds_range ${cds_range}"
fi
if [ "${force}" = true ] || [ ! -f ${ribomap_out}.base ]; then
    echo "running riboprof..."
    riboprof ${options}
    check_file ${ribomap_out}.base "pipeline failed at ribosome profile generation!"
fi
