# DRS-LOG
Command line utility for logging data in a binary format from the [PSI](https://www.psi.ch/) [DRS4](https://www.psi.ch/drs/drs-chip) [evaluation board](https://www.psi.ch/drs/evaluation-board) module.

## Compile
Requires GCC, and the [libusb-1.0](http://www.libusb.org/wiki/libusb-1.0) package. On a debian machine, installation of these packageserquires a few lines.

```bash
sudo apt-get update && apt-get upgrade
sudo apt-get install build-essential
sudo apt-get install libusb-1.0.0-dev
```

Then just run make in this directory to get the binary.
```bash
make
```

## Execute
Currently it is set up to use arguments in a list. A use example is displayed.
```bash
./drsLog 5.0 0.45 60.0 R AND 01010 0.1 0.1 0.1 100 50 5 .
```
Description of the arguments and their order is shown below.
```
Usage: ./drsLog
      <sample speed (0.1-6)            (5.0)GSPS>
      <range center                    (0.450)V>
      <trigger delay                   (60.0)ns>
      <trigger type [R|F]              (R)ising edge>
      <trigger logic [AND|OR]          (AND) logic>
      <trigger [CH1,CH2,CH3,CH4,EXT]   (01010) CH2,CH4>
      <trigger voltage CH1             (0.050)V>
      <trigger voltage CH2             (0.060)V>
      <trigger voltage CH3             (0.800)V>
      <trigger voltage CH4             (0.020)V>
      <max events                      (10000) events>
      <max time                        (3600) seconds>
      <path                            ../data>
```