#!/bin/bash
### Analyze one file########
# Discription
# run this file in /Users/fumiaki/Desktop/Detector_test_sta/HFIR/NOWG/IPTS-31333/Analysis/
# how to use it
# ./auto_run_ver_2.sh
#  This program keep checking HFIR/NOWG/IPTS-31333/raw/ folder and prduce h5 and png file if it finds a new folder.

########

###### Bag ##########
## This program skips a files that is saved when sophiread is working...
#-> The Bag is fixed. This program keeps checking if all Run h5 files exit in H5 folder. If h5 files are missing, it produces them.
###


# RUM_NUM="000002"
Refreshtime=5  # The time that we wait for renew the data
DSCALE="2" # Number of pixels in one axis =256*DSCALE
####  Create initial configuration   #####
DATA_DIR="/gpfs/neutronsfs/instruments-hfir/NOWG/IPTS-31333/images/tpx3/"
IMG_DIR="/HFIR/CG4B/IPTS-31333/shared/Analyzed_DATA/IMG/"
IMAG_CSV_DIR="/HFIR/CG4B/IPTS-31333/shared/Analyzed_DATA/CSV/"
TOF_IMAG_DIR="/HFIR/CG4B/IPTS-31333/shared/Analyzed_DATA/TOF_IMG/"
TOF_CSV_DIR="/HFIR/CG4B/IPTS-31333/shared/Analyzed_DATA/TOF_CSV/"
Search_run=41676

# File to track processed runs
PROCESSED_RUNS_FILE="processed_runs.txt"

# Create the file if it doesn't exist
touch "$PROCESSED_RUNS_FILE"

##########################################

echo "Starting to monitor: ${DATA_DIR}"
echo "Will only process runs > ${Search_run}"
echo "Refresh interval: ${Refreshtime} seconds"
echo "Processed runs log: ${PROCESSED_RUNS_FILE}"
echo "=========================================="

while true
do
    # Get all Run directories
    DIR_array=($(ls ${DATA_DIR} 2>/dev/null | grep -E '^Run_[0-9]+'))
    
    echo "Checking for new folders... ($(date))"
    
    for folder in "${DIR_array[@]}"
    do
        # Extract run number from folder name (e.g., Run_41676 -> 41676)
        run_num=$(echo ${folder} | grep -Eo '[0-9]+')
        
        # Skip if run number is not greater than Search_run
        if [ "$run_num" -le "$Search_run" ]; then
            continue
        fi
        
        # Check if this run has already been processed
        if grep -q "^${run_num}$" "$PROCESSED_RUNS_FILE"; then
            # Already processed, skip
            continue
        fi
        
        # Look for .tpx3 files in this folder
        folder_path="${DATA_DIR}${folder}/"
        
        if [ -d "$folder_path" ]; then
            # Find .tpx3 files in the folder
            tpx3_files=($(find "$folder_path" -maxdepth 1 -name "*.tpx3" -type f))
            
            if [ ${#tpx3_files[@]} -gt 0 ]; then
                echo "=========================================="
                echo "Found new folder: ${folder}"
                echo "Run number: ${run_num}"
                
                # Process flag
                process_success=true
                
                for tpx3_file in "${tpx3_files[@]}"
                do
                    tpx3_filename=$(basename "$tpx3_file")
                    
                    echo "  TPX3 File: ${tpx3_filename}"
                    echo "  Full Path: ${tpx3_file}"
                    
                    # Call your analysis script
                    python 60Hz_LII_1.py "$tpx3_file" "$IMG_DIR" "$IMAG_CSV_DIR" "$TOF_IMAG_DIR" "$TOF_CSV_DIR" "$DSCALE"
                    
                    # Check if python script succeeded
                    if [ $? -ne 0 ]; then
                        echo "ERROR: Python script failed for ${tpx3_file}"
                        process_success=false
                    fi
                done
                
                # Mark this run as processed if successful
                if [ "$process_success" = true ]; then
                    echo "${run_num}" >> "$PROCESSED_RUNS_FILE"
                    echo "✅ Run ${run_num} marked as processed"
                fi
                
                echo "=========================================="
            fi
        fi
    done
    
    echo "Waiting ${Refreshtime} seconds..."
    sleep $Refreshtime
done