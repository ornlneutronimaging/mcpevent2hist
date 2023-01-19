# MCPEvent2Hist

This repository is used to track development of reduction code that converts events data (read from tpx3 binary) to 2D histogram (image).

- `manufacturer`: contains sample data processing source code and manual from detector manufacturer.

- `sophiread_display`: contains internal developed prototype for reducing and viewing tpx3 data (based on Qt).

## Build from source

Here is the minimum efforts route for compiling `sophiread_display`:

- Create a `conda` environment with `environment.yml` using the following command (we are using `micromamba` here for speed).

```bash
micromamba create -f environment.yml
```

- Activate the environment with `conda activate sophiread`.

- Go to `sophiread_display` folder.

- Type `qmake` to build the project.

- Type `make` to build the binary.

## Developer notes

1. On Ubuntu 22.04, the openGL library, `libGL.so.1` is often installed, but not symbolic linked to `libGL.so`. Since `ld` is looking for `libGL.so` only under `/usr/lib/x86_64-linux-gnu/`, we need to make the symbolic link manually.

2. On MacOSX (m1 pro), you might see some warning about SDK version issue (depending on how fast Qt plans to support the latest Apple SDK). This warning can be safely ignored as the application will be built without any issue.
