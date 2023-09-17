# mlpack for macos

https://www.mlpack.org/getstarted.html

Using conda to get to mlpack only works for Linux installs.  For Mac, we must use source.

https://stackoverflow.com/questions/6241922/how-to-properly-set-cmake-install-prefix-from-the-command-line

So use `cmake -DCMAKE_INSTALL_PREFIX=${CONDA_PREFIX} ..`

## Recipe

```
conda activate sophiread
wget -c http://mlpack.org/files/mlpack-4.2.1.tar.gz -O - | tar -xz
mkdir mlpack-4.2.1/build
cd mlpack-4.2.1/build
cmake -DCMAKE_INSTALL_PREFIX=${CONDA_PREFIX} ..
make -j8
make install
```
