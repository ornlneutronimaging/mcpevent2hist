# MCPEvent2Hist

This repository is used to track development of reduction code that converts events data (read from tpx3 binary) to 2D histogram (image).

- `manufacturer`: contains sample data processing source code and manual from detector manufacturer.

- `sophiread_display`: contains internal developed prototype for reducing and viewing tpx3 data (based on Qt).

- `sophiread_v2`: contains the 2nd generation of refactored code for reducing and viewing tpx3 data (based on Qt).

## Build from source

### Building sophiread_display

Here is the minimum efforts route for compiling `sophiread_display`:

- Create a `conda` environment with `environment.yml` using the following command (we are using `micromamba` here for speed).

```bash
micromamba create -f environment.yml
```

- Activate the environment with `conda activate sophiread`.

- Go to `sophiread_display` folder.

- Type `qmake` to build the project.

- Type `make` to build the binary.


### Building sophiread_v2

Here is the minimum efforts route for compiling `sophiread_v2`:

- Create a `conda` environment with `environment.yml` using the following command (we are using `micromamba` here for speed).

```bash
micromamba create -f environment.yml
```

    - If the environment already exists, you can either updating the environment or simply delete the old one and create a new one.

- Activate the environment with `conda activate sophiread`.

- Go to `sophiread_v2` folder.

- Create build folder with `mkdir build`, going into the folder with `cd build`, and type `cmake ..` to build the project.

- Type `make` to build the binary.

- Run `ctest -V` to run the test.

- Run `Sophiread data/frames_pinhole_3mm_1s_RESOLUTION_000001.tpx3` to parse the sample data into a txt file.

## Developer notes

1. On Ubuntu 22.04, the openGL library, `libGL.so.1` is often installed, but not symbolic linked to `libGL.so`. Since `ld` is looking for `libGL.so` only under `/usr/lib/x86_64-linux-gnu/`, we need to make the symbolic link manually.

2. On MacOSX (m1 pro), you might see some warning about SDK version issue (depending on how fast Qt plans to support the latest Apple SDK). This warning can be safely ignored as the application will be built without any issue.

3. Currently, the testing data does not seem to have a `tdc` packet, so the `tof` and `spidertime` always match.  This might be a bug in how the `tof` is calculated, and future investigation is needed.

4. If seeing the following error, install `mesa-common-dev` and `libglu1-mesa-dev` to the host system (cannot provide them with conda).

```bash
CMake Error at /home/user/micromamba/envs/sophiread/lib/cmake/Qt5Gui/Qt5GuiConfigExtras.cmake:9 (message):
  Failed to find "GL/gl.h" in
  ...
```
