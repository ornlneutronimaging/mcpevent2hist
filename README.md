[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.8299343.svg)](https://doi.org/10.5281/zenodo.8299343)
[![OpenSSF Best Practices](https://bestpractices.coreinfrastructure.org/projects/7256/badge)](https://bestpractices.coreinfrastructure.org/projects/7256)

MCP2EventHist
=============

This repository hosts the application, `Sophiread`, which can be used to process
raw data from timepix3 chips and perform clustering and peak fitting to extract
neutron events.

- `Sophiread_display` is the original version of the application, which is a
  simple Qt application for test driving the initial commission of the MCPTPX3
  detector.

- `Sophiread` is the updated version of the application, which contains three
  main parts:
  - `libSophireadLib.a` is the static libary containing the core functions of
    the application.
  - `Sophiread` is the main command line application, which can be used to
    process raw data into clustered hits and neutron events.
  - `SophireadGUI` is the Qt GUI application, which can be used to process raw
    data into neutron events and display it as a 2D histogram via super-pixeling.

Developer Note
--------------

The development for `Sophiread_display` is ended and the code is kept for archiving
purposes.
Future feature implementation and bug fixes will be done on `Sophiread` only.
