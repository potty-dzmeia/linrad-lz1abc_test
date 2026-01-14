# Linrad - Fork

> This fork of Linrad is used to implement and test **Rig control** so that Linrad and a transceiver work in sync. When Linrad's frequency changes, the transceiver frequency will change to the same value, and vice versa.

For this purpose, [Hamlib](https://hamlib.github.io/) software is used, in particular `rigctld.exe`.

---

## How to Install (Windows)

1. **Download Linrad64.exe**
   - Copy it into your Linrad directory.
   - Latest version: [Linrad64.exe Releases](https://github.com/potty-dzmeia/linrad-lz1abc_test/releases)
2. **Download and install Hamlib**
   - [Hamlib Download Page](https://hamlib.github.io/)
   - This will install Hamlib, for example, to `C:\Program Files\hamlib-w64-4.6.5`.
3. **Copy Hamlib DLL files**
   - From: `C:\Program Files\hamlib-w64-4.6.5\bin`
   - To: Your Linrad directory
   - Required files:
     - `libhamlib-4.dll`
     - `libusb-1.0.dll`

---

## How to Use

1. **Run `rigctld.exe`**
   - Usually located at: `C:\Program Files\hamlib-w64-4.6.5\bin`
   - Run in the Windows command prompt:
     ```sh
     rigctld.exe -m <Rigmodel_number> -r <Com port>
     ```
     For example, for FTDX10 on COM6:
     ```sh
     rigctld -m 1042 -r COM6
     ```
   - To check that `rigctld.exe` is working properly, run:
     ```sh
     rigctl.exe -m 2 f
     ```
     This should return the current dial frequency on your radio.
2. **Run Linrad**
   - Start Linrad and check that the frequency moves accordingly as you tune around in Linrad and on your transceiver.
   - **Note:** If you don't start `rigctld.exe`, Linrad will continue working as usual without Rig control support.

---

## About Linrad - SDR Receiver

Linrad is an SDR receiver with advanced features developed by Leif Åsbrink, SM5BSZ.

- Linrad main page: [http://sm5bsz.com/linuxdsp/linrad.htm](http://sm5bsz.com/linuxdsp/linrad.htm)
- Linrad source code: [https://sourceforge.net/projects/linrad/](https://sourceforge.net/projects/linrad/)
- The [References](#References) section below has more information about Linrad.

The version of Linrad in this repository is based on [https://github.com/fventuri/linrad.git](https://github.com/fventuri/linrad.git)
