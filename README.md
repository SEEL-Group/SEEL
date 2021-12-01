# SEEL

The LoRa Synchronized Energy-Efficient LPWAN (SEEL) communication protocol is a novel Low Power Wide Area Network (LPWAN) utilizing LoRa-PHY technology. SEEL differs from LoRa-WAN and other existing work due to its implementation of an energy-efficient multi-hop tree topology. SEEL is recommended for low data rate applications that require an extensive area to be covered using battery-powered nodes; an example of this is agricultural data collection which typically requires monitoring of large areas without power distribution infrastructure.

While the SEEL protocol is hardware independent, the current software implementation of SEEL is designed for the Arduino platform.

Check the documentation file (provided in this repository) for more details.

*** Software Setup *********************************************************

1) Download the SEEL files (there should be an examples/ and src/ folder in the root directory)

2) Copy and Paste the SEEL/ (root) folder into your Arduino Library folder

3) Also add the external libraries needed by SEEL:
    https://github.com/sandeepmistry/arduino-LoRa
    https://github.com/rocketscream/Low-Power

4) For the arduino-LoRa library, apply the "lora_lib_crc_check.patch" file using "git apply <patch>" found in this repo's patches/ folder

5) Restart Arduino for the changes to take effect

6) See advanced install instructions (such as logging setup) in SEEL_documentation.pdf

*****************************************************************************

If you are interested in discussing commercial applications of SEEL, please contact:
zboyang@umich.edu or dickrp@umich.edu

Please contact the University of Michigan Office of Technology Transfer for a quote and licensing options: techtransfer@umich.edu
