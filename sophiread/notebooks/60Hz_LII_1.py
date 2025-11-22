import os
import sys
import time
import numpy as np
import matplotlib.pyplot as plt
import tdcsophiread as tdc

# Parse command-line arguments
if len(sys.argv) != 7:
    print("Usage: python 60Hz_LII.py <tpx3_file> <IMG_DIR> <IMAG_CSV_DIR> <TOF_IMAG_DIR> <TOF_CSV_DIR> <DSCALE>")
    sys.exit(1)

# Get arguments
tpx3_file = sys.argv[1]
IMG_DIR = sys.argv[2]
IMAG_CSV_DIR = sys.argv[3]
TOF_IMAG_DIR = sys.argv[4]
TOF_CSV_DIR = sys.argv[5]
DSCALE = int(sys.argv[6])

print(f"Processing file: {tpx3_file}")
print(f"Image output directory: {IMG_DIR}")
print(f"Image CSV output directory: {IMAG_CSV_DIR}")
print(f"TOF Image output directory: {TOF_IMAG_DIR}")
print(f"TOF CSV output directory: {TOF_CSV_DIR}")
print(f"DSCALE: {DSCALE}")

# Create output directories if they don't exist
os.makedirs(IMG_DIR, exist_ok=True)
os.makedirs(IMAG_CSV_DIR, exist_ok=True)
os.makedirs(TOF_IMAG_DIR, exist_ok=True)
os.makedirs(TOF_CSV_DIR, exist_ok=True)

# Verify input file exists
if not os.path.exists(tpx3_file):
    print(f"Error: File not found: {tpx3_file}")
    sys.exit(1)

# Extract run number from filename (e.g., Run_41676_fBCFYv_000000.tpx3 -> 41676)
filename = os.path.basename(tpx3_file)
run_num = filename.split('_')[1] if 'Run_' in filename else 'unknown'

print(f"Run number: {run_num}")

# Setup plotting
plt.rcParams["figure.figsize"] = (10, 6)
plt.rcParams["font.size"] = 12

# ===== Process TPX3 file to hits =====
print("=== Processing TPX3 file to hits ===")
start_time = time.time()

hit_view = tdc.process_tpx3(tpx3_file, parallel=True)
hits = np.array(hit_view, copy=False)

processing_time = time.time() - start_time
print(f"✅ Processing completed in {processing_time:.2f} seconds")
print(f"✅ Total hits extracted: {len(hits):,}")

# ===== Clustering Configuration =====
print("=== Setting up clustering configuration ===")
config = tdc.NeutronProcessingConfig.venus_defaults()
config.extraction.min_tot_threshold = 0
config.temporal.enable_deduplication = False
config.clustering.abs.min_cluster_size = 25
config.clustering.abs.neutron_correlation_window = 1400
config.extraction.super_resolution_factor = DSCALE

# ===== Clustering =====
print("=== Clustering hits to neutrons ===")
start_time = time.time()
neutron_view = tdc.process_hits_to_neutrons(hits, config)
neutrons = np.array(neutron_view, copy=False)
clustering_time = time.time() - start_time

print(f"✅ Clustering completed in {clustering_time:.2f} seconds")
print(f"✅ Output neutrons: {len(neutrons):,}")

# ===== Detector Configuration =====
det_venus_config = tdc.DetectorConfig.venus_defaults()
NEUTRON_SUPER_RES = config.extraction.super_resolution_factor

# Define region of interest
X_MIN, X_MAX = 258, 514
Y_MIN, Y_MAX = 0, 256

# Filter neutrons to region of interest
neutrons_x_min = X_MIN * NEUTRON_SUPER_RES
neutrons_x_max = X_MAX * NEUTRON_SUPER_RES
neutrons_y_min = Y_MIN * NEUTRON_SUPER_RES
neutrons_y_max = Y_MAX * NEUTRON_SUPER_RES

neutrons_mask = (neutrons["x"] >= neutrons_x_min) & (neutrons["x"] < neutrons_x_max) & \
                (neutrons["y"] >= neutrons_y_min) & (neutrons["y"] < neutrons_y_max)
neutrons_filtered = neutrons[neutrons_mask]

print(f"Filtered to region: X=[{X_MIN}, {X_MAX}), Y=[{Y_MIN}, {Y_MAX})")
print(f"Neutrons in region: {len(neutrons_filtered):,}")

# ===== Create Neutron Spatial Distribution =====
print("=== Creating Neutron Spatial Distribution ===")

neutrons_x_filtered = neutrons_filtered["x"]
neutrons_y_filtered = neutrons_filtered["y"]

X_BINS = X_MAX - X_MIN
Y_BINS = Y_MAX - Y_MIN
NEUTRON_X_BINS = int(X_BINS * NEUTRON_SUPER_RES)
NEUTRON_Y_BINS = int(Y_BINS * NEUTRON_SUPER_RES)

hist_neutrons, x_edges_n, y_edges_n = np.histogram2d(
    neutrons_x_filtered,
    neutrons_y_filtered,
    bins=[NEUTRON_X_BINS, NEUTRON_Y_BINS],
    range=[[neutrons_x_min, neutrons_x_max], [neutrons_y_min, neutrons_y_max]],
)

hist_neutrons_rotated = np.rot90(hist_neutrons)

# Save histogram to CSV
csv_filename = os.path.join(IMAG_CSV_DIR, f'Run_{run_num}_neutron_spatial_histogram.csv')
np.savetxt(
    csv_filename,
    hist_neutrons_rotated,
    delimiter=',',
    fmt='%d',
    header=f'Neutron spatial distribution histogram ({NEUTRON_X_BINS}x{NEUTRON_Y_BINS} bins)',
    comments=''
)
print(f"✅ CSV saved: {csv_filename}")

# Create and save plot
fig, ax = plt.subplots(1, 1, figsize=(10, 8))

im = ax.imshow(
    hist_neutrons_rotated,
    origin="lower",
    aspect="equal",
    extent=[neutrons_y_min, neutrons_y_max, neutrons_x_min, neutrons_x_max],
    cmap="viridis",
    interpolation="gaussian",
)

# Convert axis labels to pixel coordinates
y_ticks = ax.get_xticks()
y_tick_labels = [f'{int(y / NEUTRON_SUPER_RES)}' for y in y_ticks]
ax.set_xticklabels(y_tick_labels)

x_ticks = ax.get_yticks()
x_tick_labels = [f'{int(x / NEUTRON_SUPER_RES)}' for x in x_ticks]
ax.set_yticklabels(x_tick_labels)

ax.set_xlabel("Y (pixels)")
ax.set_ylabel("X (pixels)")
ax.set_title(f"Run {run_num} - Neutrons Spatial Distribution ({len(neutrons_filtered):,} neutrons)")
plt.colorbar(im, ax=ax, label="Counts")

plt.tight_layout()
img_filename = os.path.join(IMG_DIR, f'Run_{run_num}_neutron_spatial_distribution.png')
plt.savefig(img_filename, dpi=300, bbox_inches='tight')
plt.close()
print(f"✅ Image saved: {img_filename}")

# ===== TOF Analysis =====
print("=== TOF Analysis ===")

neutrons_tof_ms = neutrons["tof"] * 25 / 1e6
######################### Number of Bins can be changed #######################################
######## Tof bins from 0 to 16.7 ms with 0.001 ms width #########
tof_bins = np.linspace(0, 16.7, 16700)
#################################################################
########################################################

hist_neutrons_tof, edges_neutrons = np.histogram(neutrons_tof_ms, bins=tof_bins)
centers_neutrons = (edges_neutrons[:-1] + edges_neutrons[1:]) / 2
errors_neutrons = np.sqrt(hist_neutrons_tof)

# Remove first bin
centers_neutrons = centers_neutrons[1:]
hist_neutrons_tof = hist_neutrons_tof[1:]
errors_neutrons = errors_neutrons[1:]

# Save TOF data to CSV
tof_csv_filename = os.path.join(TOF_CSV_DIR, f'Run_{run_num}_neutron_tof_data.txt')
data_array = np.column_stack((centers_neutrons, hist_neutrons_tof))
np.savetxt(
    tof_csv_filename,
    data_array,
    header='TOF(ms)\tCounts',
    fmt=['%.6f', '%d'],
    delimiter='\t',
    comments=''
)
print(f"✅ TOF CSV saved: {tof_csv_filename}")

# Create and save TOF plot
fig, ax = plt.subplots(figsize=(12, 6))

ax.errorbar(
    centers_neutrons,
    hist_neutrons_tof,
    yerr=errors_neutrons,
    fmt='o',
    markersize=3,
    capsize=2,
    alpha=0.6,
    color="red",
    label=f"Total counts: ({len(neutrons_tof_ms):,})",
)
ax.set_xlabel("TOF (ms)")
ax.set_ylabel("Counts")
ax.set_title(f"Run {run_num} - TOF Spectrum - Neutrons")
ax.legend()
ax.grid(True, alpha=0.3)

plt.tight_layout()
tof_img_filename = os.path.join(TOF_IMAG_DIR, f'Run_{run_num}_neutron_tof_spectrum.png')
plt.savefig(tof_img_filename, dpi=300, bbox_inches='tight')
plt.close()
print(f"✅ TOF Image saved: {tof_img_filename}")

print("========================================")
print("✅ Processing completed successfully!")
print("========================================")