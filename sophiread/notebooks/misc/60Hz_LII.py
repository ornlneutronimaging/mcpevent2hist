import os
from sys import argv
import time
import numpy as np
import matplotlib.pyplot as plt

import tdcsophiread as tdc

print(f"TDCSophiread version: {tdc.__version__}")
print(f"Numpy version: {np.__version__}")

# Setup plotting
plt.rcParams["figure.figsize"] = (10, 6)
plt.rcParams["font.size"] = 12
# Ni powder file path

data_file_with_path = argv[1]  # Get the file path from command line arguments
# Create neutron processing configuration with VENUS defaults
config = tdc.NeutronProcessingConfig.venus_defaults()
config.extraction.min_tot_threshold = 0
config.temporal.enable_deduplication = False
config.clustering.abs.min_cluster_size=25
config.clustering.abs.neutron_correlation_window=1400  # ns
config.extraction.super_resolution_factor=2

hit_view = tdc.process_tpx3(data_file_with_path, parallel=True)

# Convert to numpy array (zero-copy)
hits = np.array(hit_view, copy=False)

neutron_view = tdc.process_hits_to_neutrons(hits, config)
neutrons = np.array(neutron_view, copy=False)
# print("=== Creating Neutron Spatial Distribution (Filtered to Data Region) ===")

# VENUS detector has 514x514 pixels (256*2 + 2 pixel gap)
det_venus_config = tdc.DetectorConfig.venus_defaults()

det_venus_config.get_chip_size_x(), det_venus_config.get_chip_size_y()
DETECTOR_PIXELS = det_venus_config.get_chip_size_x() * 2 + 2

# use super-resolution factor from config
NEUTRON_SUPER_RES = config.extraction.super_resolution_factor

# Define region of interest based on actual data range
# Data is in x: 258-513, y: 0-255 (top-right quadrant)
X_MIN, X_MAX = 258, 514
Y_MIN, Y_MAX = 0, 256

# Filter neutrons to region of interest (in super-resolution coordinates)
neutrons_x_min = X_MIN * NEUTRON_SUPER_RES
neutrons_x_max = X_MAX * NEUTRON_SUPER_RES
neutrons_y_min = Y_MIN * NEUTRON_SUPER_RES
neutrons_y_max = Y_MAX * NEUTRON_SUPER_RES

neutrons_mask = (neutrons["x"] >= neutrons_x_min) & (neutrons["x"] < neutrons_x_max) & \
                (neutrons["y"] >= neutrons_y_min) & (neutrons["y"] < neutrons_y_max)
neutrons_filtered = neutrons[neutrons_mask]

print(f"Filtered to region: X=[{X_MIN}, {X_MAX}), Y=[{Y_MIN}, {Y_MAX})")
print(f"  Neutrons in region: {len(neutrons_filtered):,} / {len(neutrons):,} ({100*len(neutrons_filtered)/len(neutrons):.1f}%)")

# Create single figure for neutrons
fig, ax = plt.subplots(1, 1, figsize=(10, 8))

# Neutrons distribution
print("\nCreating neutrons 2D histogram...")
neutrons_x_filtered = neutrons_filtered["x"]
neutrons_y_filtered = neutrons_filtered["y"]

# Use super-resolution bins
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

# Rotate 90 degrees counterclockwise
hist_neutrons_rotated = np.rot90(hist_neutrons)
np.savetxt(
    'neutron_spatial_histogram.csv',
    hist_neutrons_rotated,
    delimiter=',',
    fmt='%d',
    header=f'Neutron spatial distribution histogram ({NEUTRON_X_BINS}x{NEUTRON_Y_BINS} bins)',
    comments=''
)

im = ax.imshow(
    hist_neutrons_rotated,
    origin="lower",
    aspect="equal",
    extent=[neutrons_y_min, neutrons_y_max, neutrons_x_min, neutrons_x_max],
    cmap="viridis",
    interpolation="gaussian",
)

# # Convert axis labels from super-resolution to pixel coordinates
# y_ticks = ax.get_xticks()
# y_tick_labels = [f'{int(y / NEUTRON_SUPER_RES)}' for y in y_ticks]
# ax.set_xticklabels(y_tick_labels)

# x_ticks = ax.get_yticks()
# x_tick_labels = [f'{int(x / NEUTRON_SUPER_RES)}' for x in x_ticks]
# ax.set_yticklabels(x_tick_labels)

ax.set_ylabel("Y (pixels)")
ax.set_xlabel("X (pixels)")
ax.set_title(f"Neutrons Spatial Distribution ({len(neutrons_filtered):,} neutrons)")
plt.colorbar(im, ax=ax, label="Counts")

plt.tight_layout()
plt.savefig('neutron_image.png', dpi=300, bbox_inches='tight')
plt.show()

# Print detailed statistics
print("\n=== Neutron Spatial Distribution Analysis ===")
print(f"Region of interest: X=[{X_MIN}, {X_MAX}), Y=[{Y_MIN}, {Y_MAX})")
print(f"Super-resolution factor: {NEUTRON_SUPER_RES}x")
print(f"  X range: {neutrons_x_filtered.min():.1f} - {neutrons_x_filtered.max():.1f} (super-res)")
print(f"  Y range: {neutrons_y_filtered.min():.1f} - {neutrons_y_filtered.max():.1f} (super-res)")
print(f"  Total neutrons in region: {len(neutrons_filtered):,}")
print(f"  Bins: {NEUTRON_X_BINS} × {NEUTRON_Y_BINS}")

fig, ax = plt.subplots(figsize=(12, 6))

# Neutrons TOF spectrum - bin the data first
tof_bins = np.linspace(0, 16.7, 1670)  # 0-16.7 ms in 1670 bins

hits_tof_ms = hits["tof"] * 25 / 1e6  # 25ns units to ms
neutrons_tof = neutrons["tof"]
neutrons_tof_ms = neutrons_tof * 25 / 1e6
hist_neutrons, edges_neutrons = np.histogram(neutrons_tof_ms, bins=tof_bins)
centers_neutrons = (edges_neutrons[:-1] + edges_neutrons[1:]) / 2
errors_neutrons = np.sqrt(hist_neutrons)  # Poisson error: sqrt(N)

# Remove first bin
centers_neutrons = centers_neutrons[1:]
hist_neutrons = hist_neutrons[1:]
errors_neutrons = errors_neutrons[1:]

ax.errorbar(
    centers_neutrons,
    hist_neutrons,
    yerr=errors_neutrons,
    fmt='o',
    markersize=3,
    capsize=2,
    alpha=0.6,
    color="red",
    label=f"Neutrons ({len(neutrons_tof_ms):,})",
)
ax.set_xlabel("TOF (ms)")
ax.set_ylabel("Counts")
ax.set_title("TOF Spectrum - Neutrons (first bin removed)")
ax.legend()
ax.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig('neutron_tof_spectrum.png', dpi=300, bbox_inches='tight')
plt.close()

# Save data to TXT file
data_array = np.column_stack((centers_neutrons, hist_neutrons))
np.savetxt(
    'neutron_tof_data.txt',
    data_array,
    header='TOF(ms)\tCounts',
    fmt=['%.6f', '%d'],
    delimiter='\t',
    comments=''
)

print("✅ Files saved successfully:")
print("   - neutron_tof_spectrum.png (TOF spectrum plot)")
print("   - neutron_tof_data.txt (TOF vs Counts data)")