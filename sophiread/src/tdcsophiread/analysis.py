"""
TDCSophiread Data Analysis Utilities

High-level analysis functions for processed TPX3 hit data including:
- TOF spectrum generation
- ROI (Region of Interest) selection
- Hit filtering and statistics
- Data visualization helpers
"""

import numpy as np
from typing import Dict, Tuple, Optional, Union, List
import matplotlib.pyplot as plt
import matplotlib.figure


def create_tof_spectrum(hits: Dict[str, np.ndarray],
                       tof_range_ms: Tuple[float, float] = (0, 20),
                       num_bins: int = 1000,
                       chip_filter: Optional[List[int]] = None) -> Tuple[np.ndarray, np.ndarray]:
    """
    Create time-of-flight spectrum from hit data

    Args:
        hits: Dictionary with hit data arrays (must contain 'tof' and 'chip_id')
        tof_range_ms: TOF range in milliseconds (min, max)
        num_bins: Number of histogram bins
        chip_filter: List of chip IDs to include (None = all chips)

    Returns:
        (bin_centers_ms, counts): TOF bin centers in ms and corresponding counts
    """
    if 'tof' not in hits:
        raise ValueError("Hit data must contain 'tof' field")

    # Convert TOF from 25ns units to milliseconds
    tof_ms = hits['tof'] * 25 / 1e6

    # Apply chip filter if specified
    if chip_filter is not None:
        if 'chip_id' not in hits:
            raise ValueError("Hit data must contain 'chip_id' field for chip filtering")

        mask = np.isin(hits['chip_id'], chip_filter)
        tof_ms = tof_ms[mask]

    # Create histogram
    counts, bin_edges = np.histogram(tof_ms, bins=num_bins, range=tof_range_ms)
    bin_centers = (bin_edges[:-1] + bin_edges[1:]) / 2

    return bin_centers, counts


def select_roi(hits: Dict[str, np.ndarray],
               x_range: Tuple[int, int],
               y_range: Tuple[int, int]) -> Dict[str, np.ndarray]:
    """
    Select hits within a rectangular region of interest

    Args:
        hits: Dictionary with hit data arrays (must contain 'x' and 'y')
        x_range: X coordinate range (min, max) inclusive
        y_range: Y coordinate range (min, max) inclusive

    Returns:
        Filtered hit data dictionary with same structure as input
    """
    if 'x' not in hits or 'y' not in hits:
        raise ValueError("Hit data must contain 'x' and 'y' fields")

    # Create mask for ROI
    x_mask = (hits['x'] >= x_range[0]) & (hits['x'] <= x_range[1])
    y_mask = (hits['y'] >= y_range[0]) & (hits['y'] <= y_range[1])
    roi_mask = x_mask & y_mask

    # Apply mask to all fields
    roi_hits = {}
    for key, values in hits.items():
        roi_hits[key] = values[roi_mask]

    return roi_hits


def filter_hits_by_tof(hits: Dict[str, np.ndarray],
                      tof_range_ms: Tuple[float, float]) -> Dict[str, np.ndarray]:
    """
    Filter hits by time-of-flight range

    Args:
        hits: Dictionary with hit data arrays (must contain 'tof')
        tof_range_ms: TOF range in milliseconds (min, max)

    Returns:
        Filtered hit data dictionary
    """
    if 'tof' not in hits:
        raise ValueError("Hit data must contain 'tof' field")

    # Convert TOF range to 25ns units
    tof_min_units = int(tof_range_ms[0] * 1e6 / 25)
    tof_max_units = int(tof_range_ms[1] * 1e6 / 25)

    # Create mask
    tof_mask = (hits['tof'] >= tof_min_units) & (hits['tof'] <= tof_max_units)

    # Apply mask to all fields
    filtered_hits = {}
    for key, values in hits.items():
        filtered_hits[key] = values[tof_mask]

    return filtered_hits


def calculate_hit_statistics(hits: Dict[str, np.ndarray]) -> Dict[str, Union[int, float, Dict]]:
    """
    Calculate comprehensive statistics for hit data

    Args:
        hits: Dictionary with hit data arrays

    Returns:
        Dictionary with statistics including totals, ranges, and per-chip breakdown
    """
    stats = {
        'total_hits': len(hits['x']) if 'x' in hits else 0,
        'coordinate_ranges': {},
        'timing_stats': {},
        'chip_breakdown': {}
    }

    if stats['total_hits'] == 0:
        return stats

    # Coordinate statistics
    if 'x' in hits and 'y' in hits:
        stats['coordinate_ranges'] = {
            'x_range': (int(hits['x'].min()), int(hits['x'].max())),
            'y_range': (int(hits['y'].min()), int(hits['y'].max())),
            'x_mean': float(hits['x'].mean()),
            'y_mean': float(hits['y'].mean())
        }

    # Timing statistics
    if 'tof' in hits:
        tof_ms = hits['tof'] * 25 / 1e6  # Convert to milliseconds
        stats['timing_stats'] = {
            'tof_range_ms': (float(tof_ms.min()), float(tof_ms.max())),
            'tof_mean_ms': float(tof_ms.mean()),
            'tof_std_ms': float(tof_ms.std())
        }

    if 'tot' in hits:
        stats['timing_stats']['tot_range'] = (int(hits['tot'].min()), int(hits['tot'].max()))
        stats['timing_stats']['tot_mean'] = float(hits['tot'].mean())

    # Per-chip breakdown
    if 'chip_id' in hits:
        unique_chips, chip_counts = np.unique(hits['chip_id'], return_counts=True)
        stats['chip_breakdown'] = {
            'active_chips': unique_chips.tolist(),
            'hits_per_chip': dict(zip(unique_chips.tolist(), chip_counts.tolist())),
            'chip_percentages': dict(zip(
                unique_chips.tolist(),
                (100 * chip_counts / len(hits['chip_id'])).tolist()
            ))
        }

    return stats


def plot_tof_spectrum(hits: Dict[str, np.ndarray],
                     tof_range_ms: Tuple[float, float] = (0, 20),
                     num_bins: int = 1000,
                     title: str = "TOF Spectrum",
                     figsize: Tuple[float, float] = (10, 6),
                     show_stats: bool = True) -> Optional[matplotlib.figure.Figure]:
    """
    Plot time-of-flight spectrum

    Args:
        hits: Dictionary with hit data arrays
        tof_range_ms: TOF range in milliseconds
        num_bins: Number of histogram bins
        title: Plot title
        figsize: Figure size (width, height)
        show_stats: Whether to display statistics on plot

    Returns:
        matplotlib Figure object if matplotlib available, None otherwise
    """

    # Create spectrum
    bin_centers, counts = create_tof_spectrum(hits, tof_range_ms, num_bins)

    # Create plot
    fig, ax = plt.subplots(figsize=figsize)

    ax.plot(bin_centers, counts, 'b-', linewidth=1.5, alpha=0.8)
    ax.fill_between(bin_centers, counts, alpha=0.3)

    ax.set_xlabel('Time of Flight (ms)')
    ax.set_ylabel('Counts')
    ax.set_title(title)
    ax.grid(True, alpha=0.3)

    # Add statistics text
    if show_stats:
        total_hits = len(hits['x']) if 'x' in hits else 0
        peak_tof = bin_centers[np.argmax(counts)]
        peak_count = np.max(counts)

        stats_text = f'Total hits: {total_hits:,}\nPeak: {peak_tof:.2f} ms ({peak_count:,} counts)'
        ax.text(0.02, 0.98, stats_text, transform=ax.transAxes,
                verticalalignment='top', bbox=dict(boxstyle='round', alpha=0.8))

    plt.tight_layout()
    return fig


def plot_hit_map(hits: Dict[str, np.ndarray],
                bins: Union[int, Tuple[int, int]] = 256,
                title: str = "Hit Position Map",
                figsize: Tuple[float, float] = (8, 8),
                cmap: str = 'viridis') -> Optional[matplotlib.figure.Figure]:
    """
    Plot 2D histogram of hit positions

    Args:
        hits: Dictionary with hit data arrays (must contain 'x' and 'y')
        bins: Number of bins for 2D histogram (int or (x_bins, y_bins))
        title: Plot title
        figsize: Figure size
        cmap: Colormap name

    Returns:
        matplotlib Figure object if matplotlib available, None otherwise
    """

    if 'x' not in hits or 'y' not in hits:
        raise ValueError("Hit data must contain 'x' and 'y' fields")

    # Create 2D histogram
    fig, ax = plt.subplots(figsize=figsize)

    h, xedges, yedges = np.histogram2d(hits['x'], hits['y'], bins=bins)

    # Plot
    im = ax.imshow(h.T, origin='lower', cmap=cmap, aspect='equal',
                   extent=[xedges[0], xedges[-1], yedges[0], yedges[-1]])

    ax.set_xlabel('X Coordinate')
    ax.set_ylabel('Y Coordinate')
    ax.set_title(title)

    # Add colorbar
    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label('Hit Count')

    plt.tight_layout()
    return fig


def combine_hit_chunks(chunk_list: List[Dict[str, np.ndarray]]) -> Dict[str, np.ndarray]:
    """
    Combine multiple hit data chunks into single dataset

    Args:
        chunk_list: List of hit data dictionaries from streaming processing

    Returns:
        Combined hit data dictionary
    """
    if not chunk_list:
        return {}

    # Get all field names
    all_fields = set()
    for chunk in chunk_list:
        all_fields.update(chunk.keys())

    # Combine each field
    combined = {}
    for field in all_fields:
        field_arrays = [chunk[field] for chunk in chunk_list if field in chunk]
        combined[field] = np.concatenate(field_arrays)

    return combined