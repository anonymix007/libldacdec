### LDAC decoder

this is an early stage, jet functional LDAC audio stream decoder.

Shout-out to [@Thealexbarney](https://github.com/Thealexbarney) for the heavy lifting.
LDAC is basically a stripped down, streaming only ATRAC9.

#### Build
```sh
make libldacdec.so
sudo make install

```

#### Usage
see ldacdec.c for example usage

#### ldacdec
takes an LDAC stream and decodes it to WAV

#### ldacenc
uses Android LDAC encoder library to create LDAC streams from audio

#### Build BlueALSA with support for this library:
```sh
LDAC_INCLUDE_DIR=/usr/include/ldac
LDAC_LIB_DIR=/usr/lib

export LDAC_ABR_CFLAGS="-I$LDAC_INCLUDE_DIR"
export LDAC_ABR_LIBS="-L$LDAC_LIB_DIR -lldacBT_abr"
export LDAC_DEC_CFLAGS="-I$LDAC_INCLUDE_DIR"
export LDAC_DEC_LIBS="-L$LDAC_LIB_DIR -lldacdec"
export LDAC_ENC_CFLAGS="-I$LDAC_INCLUDE_DIR"
export LDAC_ENC_LIBS="-L$LDAC_LIB_DIR -lldacBT_enc"


autoreconf --install
mkdir build && cd build
../configure --enable-aac --enable-ofono --enable-aptx --enable-aptx-hd --with-libopenaptx --enable-ldac --enable-debug
make
sudo make install
```
