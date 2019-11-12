#!/usr/bin/env bash
counter= 0
#for filename in /home/kenny/data/seeds/*.seed; do
#  # strip pre and suffix
#  filename_nopath="${filename##*/}"
#  echo "$filename_nopath"
#  filename_noext="${filename_nopath%.*}"
#  echo "$filename_noext"

  # create folder for every RNA FAM with infernal--> dir( Genome, pos, msa)
  # cd /home/kenny/data/gen_pos && mkdir "$filename"
  # cd /home/kenny/infernal-1.1.2/rmark  && ./rmark-create /home/kenny/data/gen_pos/"$filename_noext"/"$filename_noext" ~/data/seeds/"$filename_nopath" rmark3-bg.hmm
  #echo "put ${filename}"
#   done
#   echo "$counter"
declare -a arr=("stats_0" "stats_0.02" "stats_0.04" "stats_0.06" "stats_0.08" "stats_0.1" "stats_0.12" "stats_0.14" "stats_0.16" "stats_0.18" "stats_0.2")
echo "move files..."

for full in ~/gen_pos/*; do
  echo "${full##*/} gets processed"

  filename="${full##*/}"
  # filename="${filename%.*}"
  # echo "$full"/"$filename"
  # create result dict
  # cd "$full" && mkdir results -> done

  # execute RNAMotif for all families
#  cd ~/server_results/likeMaster/ || exit
# ~/server_results/likeMaster/./RNAMotif

  cd ~/server_results/likeMaster/ || exit

  ~/server_results/likeMaster/./RNAMotif "$full"/"$filename".msa "$full"/"$filename".fa -r "$full"/"$filename".pos
  for filter in "${arr[@]}"; do
    #mv "$filter".txt "$full"/results
    # shellcheck disable=SC2046
    # RNAMotif creates for each Family for every stat profile one output file in the current dir
    # move them into their desired folder
    mv  ~/server_results/likeMaster/*"$filter".txt ~/server_results/likeMaster/"$filter"

  done

done
# calculuate spec and sens for each family and plot average over all for each stat profile-> safe fig

cd /server_results/scripts/ && python3 spec_spec.py
