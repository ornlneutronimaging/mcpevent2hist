Sophiread
---------

Sophiread is a simple, fast, and extensible toolkit for reading and processing
raw data from the Timepix3 chip.
It provides both command line (CLI) and graphical user interface (GUI) interfaces.

Sophiread is written in C++ and Qt (with `qwt` extension).
The streaming data client, `SophireadStream`, is using a 3rd party library, [readerwriterqueue](https://github.com/cameron314/readerwriterqueue)
to read and process the raw data packet concurrently.

How to build
------------

Sophiread is built using `cmake` and `make` under a sandboxed environment with
`conda` (see [conda](https://conda.io/docs/)).
The following steps have been tested under MaxOS and Linux, and it is in theory possible to build it under Windows.

- Install `conda` or equivalent pacakge manager such as `miniconda`, `mamba`, `micromamba`, etc.
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
  - Create env with `CONDA_SUBDIR=osx-64 conda create -f environment_mac.yml`
    - Currently `mlpack` does not have a `arm64` package from conda, so we need to fallback to x86-64 using Rosetta 2 under the hood.
  - Install [MacTex](https://www.tug.org/mactex/) before building the documentation.
  - __DO NOT__ install `mlpack` from homebrew.
    - `mlpack` from homebrew can lead to linking error when building the DBSCAN object.

Use the CLI
-----------

The CLI is a simple command line interface to read and process raw data from the Timepix3 chip.
It is a single executable file, `Sophiread`, which can be found in the `build` directory.
The current version of the CLI supports the following input arguments:

```bash
Sophiread [-i input_tpx3] [-u user_defined_params_list] [-H output_hits_HDF5]  [-E output_event_HDF5]  [-v]
```

- `-i`: input raw Timepix3 data file.  (Mandatory)
- `-u`: parse user-defined parameter list for clustering algorithsm. Please refer to the `user_defined_params.txt` template for reference. (Optional)
- `-H`: output processed hits with corresponding cluster labels as HDF5 archive. (Optional)
- `-E`: output processed neutron events as HDF5 archive. (Optional)
- `-v`: verbose mode. (Optional)

Alternatively, you can use `SophireadStream` to process the raw data packet concurrently.
From the user perspective, it is the same as `Sophiread` but it is much faster for larger data set and has a better memory management.
Unlike `Sophiread`, `SophireadStream` has a much simpler interface:

```bash
SophireadStream <TIMEPIX3_FILE_NAME>
```

Use the GUI
-----------

The GUI is a graphical user interface to read and process raw data from the Timepix3 chip.
It is a single executable file, `SophireadGUI`, which can be found in the `build` directory.
Once the GUI is launched, you can open a raw data file by clicking the `Load Data` button on the top left corner.
The GUI will process the data and display the clustered neutron events in the 2D histogram.

Important note
--------------

The raw data file is a binary file with a specific format, so it is not recommended to open it with a text editor.
Additionally, super-pixeling (also known as super resolution) is used to increase the spatial resolution of the data, i.e. bumping the `512x512` native resolution to `4028x4028`.
This is done by splitting each pixel into 8x8 sub-pixels via peak fitting.
