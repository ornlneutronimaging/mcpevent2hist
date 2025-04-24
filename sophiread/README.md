# Sophiread

Sophiread is a simple, fast, and extensible toolkit for reading and processing raw data (`*.tpx3`) from the Timepix3 chip.
It provides both command line (CLI) and graphical user interface (GUI) interfaces.

> As of 2025-02-24, the GUI application is still undergoing development to use the new fast sophiread backend, only command line applications are available.

## How to build (with `pixi`)

As NDP is gradually shifting away from `anaconda` suite, we are offering an alternative to build `Sophiread` project with [pixi](https://pixi.sh/latest/).
The following steps have been tested with M-series Mac as well as Linux-64, and it is in theory possible to build it under Windows.

- Install `pixi` at the system level (assuming we are on linux):

    ```bash
    curl -sSL https://pixi.sh/install | bash
    ```

- [Optional] Set the cache directory in your shell profile.

    ```bash
    export PIXI_CACHE_DIR=$HOME/.cache
    ```

- Clone this repository and navigate to the `sophiread` folder.

- Build the project with the following `pixi` tasks:
  - `pixi run configure` to configure the project.
  - `pixi run build` to build the project.
  - `pixi run test` to invoke `ctest` for testing.
  - `pixi run package` to create a release package (optional).
  - `pixi run docs` to build the documentation (optional).
    - **IMPORTANT** MacOS by default does not have the Tex typesetting system installed, please install [MacTex](https://www.tug.org/mactex/) before building the documentation.

Once the above steps are complete, you can find all the binary apps under the `build` directory.

## How to build (with `conda`)

Sophiread is built using `cmake` and `make` under a sandboxed environment with `conda` (see [conda](https://conda.io/docs/)).
The following steps have been tested under MaxOS and Linux, and it is in theory possible to build it under Windows.

- Install `conda` or equivalent package manager such as `miniconda`, `mamba`, `micromamba`, etc.
- Create a new development environment with `conda` (assuming we are on linux):

    ```bash
    conda env create -n sophiread -c conda-forge -f environment_linux.yml
    ```

  > NOTE: When creating a conda environment without a file, use `conda create -n <env_name> <package_name>`; when creating a conda environment with a file, use `conda env create -n <env_name> -f <file_name>`.

- Activate the environment:

    ```bash
    conda activate sophiread
    ```

- Build the project:

    ```bash
    mkdir build; cd build
    cmake ..
    make -j4
    ```

- Build the documentation (when in `build` directory)

    ```bash
    make docs
    ```

- Make release package (when in `build` directory)

    ```bash
    make package
    ```

    This will create three files in the `build` directory:

  - `sophiread-<version>-Linux.tar.gz`: a tarball for Linux
  - `sophiread-<version>-Linux.tar.Z`: a tarball for Linux with TZ compression
  - `sophiread-<version>-Linux.sh`: an installer for Linux

- For Mac users with m-series chip, please make the following adjustment:
  - Install [MacTex](https://www.tug.org/mactex/) before building the documentation.

## Use the CLI

The CLI is a simple command line interface to read and process raw data from the Timepix3 chip.
It is a single executable file, `Sophiread`, which can be found in the `build` directory.
The current version of the CLI supports the following input arguments:

```bash
Sophiread -i <input_tpx3> -H <output_hits> -E <output_events> [-u <config_file>] [-T <tof_imaging_folder>] [-f <tof_filename_base>] [-m <tof_mode>] [-t <timing_mode>] [-d] [-v]
```

- `-i <input_tpx3>`: Input TPX3 file
- `-H <output_hits>`: Output hits HDF5 file
- `-E <output_events>`: Output events HDF5 file
- `-u <config_file>`: User configuration JSON file (optional)
- `-T <tof_imaging_folder>`: Output folder for TIFF TOF images (optional)
- `-f <tof_filename_base>`: Base name for TIFF files (default: tof_image)
- `-m <tof_mode>`: TOF mode: 'hit' or 'neutron' (default: neutron)
- `-t <timing_mode>`: Timing mode: 'tdc' or 'gdc' (default: tdc)
- `-d`: Enable debug logging
- `-v`: Enable verbose logging

One **important** thing to check before using this software is that you need to check your chip layout before using it.
By default, `Sophiread` is assuming the detector has a `2x2` layout with a 5 pixel gap between chips.
Each chip has `512x512` pixels.
If your chip has different spec, you will need to modify the source code to make it work for your detector. For the ToF calculation, using tdc mode `-t tdc` (default) for timing calculation is recommended because the GDC signal is unrelaiable with `2x2` layout.

A temporary auto reduction code binary is also available for the commission of [VENUS](https://neutrons.ornl.gov/venus), `venus_auto_reducer`:

```bash
venus_auto_reducer -i <input_dir> -o <output_dir> [-u <user_config_json>] [-f <tiff_file_name_base>] [-m <tof_mode>] [-c <check_interval>] [-v] [-d]
```

- `-i <input_dir>`:  Input directory with TPX3 files
- `-o <output_dir>`:  Output directory for TIFF files
- `-u <config_file>`:  User configuration JSON file (optional)
- `-f <tiff_base>`:  Base name for TIFF files (default: tof_image)
- `-m <tof_mode>`:  TOF mode: 'hit' or 'neutron' (default: neutron)
- `-c <interval>`:  Check interval in seconds (default: 5)
- `-d`:  Debug output
- `-v`:  Verbose output

## Important note

The raw data file is a binary file with a specific format, please **DO NOT** try to open it with a text editor as it can corrupt the bytes inside.
Additionally, super-pixeling (also known as super resolution) is used to increase the spatial resolution of the data, i.e. bumping the `512x512` native resolution to `4028x4028`.
This is done by splitting each pixel into 8x8 sub-pixels via peak fitting.
