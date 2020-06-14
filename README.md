# STD OTAU Plugin
The deCONZ STD OTAU plugin implements a ZigBee Over-the-Air-Upgrade (OTAU) server in deCONZ.

## Upgrading Firmware
See the [Phoscon help topics](https://phoscon.de/en/support#ota-update-osram-devices) for instructions on how to upgrade the firmware of your ZigBee devices using the STD OTAU plugin.

Note that after the firmware has upgraded, the deCONZ GUI (and the REST API) might still show the old values (notably the _SW Build ID_ and _Date Code_).
To refresh these, read the _Basic_ cluster attributes, in the _Cluster info_ panel in the GUI.
Likewise, if the device exposes new or different endpoints and/or clusters (e.g. when upgrading from ZLL to ZigBee 3.0), refresh these by reading the _Node Descriptor_ and _Simple Descriptor(s)_ from the left drop-down menu on the node.

ZigBee firmware is identified by the _Manufacturer Code_, _Image Code_ and _Version_.
On startup, the STD OTAU plugin will read all `*.ota`, `*.ota.signed`, `*.sbl-ota`, and `*.zigbee` files in `~/otau`, and copy them to _mmmm_`_`_iiii_`_`_vvvvvvvv_`.zigbee` files, matching these attributes.

To find the _Manufacturer Code_ for your device, see the _Node info_ panel in the deCONZ GUI.  To find the _Image Code_ and (current) _Version_, see the _STD OTAU Plugin_ panel in the deCONZ GUI.
Select your device from the list (match the IEEE address) and press _Query_ to populate the fields.
Make sure to wake battery-powered devices before pressing _Query_.

Note that not all ZigBee devices support over-the-air firmware upgrading.
Devices that do support this, expose a client (grey) _OTAU_ cluster (0x0019).
Only these devices are listed in the _STD OTAU Plugin_ panel.
However, not all devices that advertise this cluster have actually implemented it.

You need to obtain the firmware files from the device manufacturer.
Some manufacturers have officially published their firmware.
For some others, we found the location from where their native gateway downloads the firmware.

Manufacturer | Code | Firmware Files
-- | -- | --
IKEA | 117C | Available on the [IKEA website](http://fw.ota.homesmart.ikea.net/feed/version_info.json).  Use the [ikea-ota-download.py](https://github.com/dresden-elektronik/deconz-rest-plugin/blob/master/ikea-ota-download.py) script to download the current IKEA firmware files directly to `~/otou`.
Ledvance<br>OSRAM | 1189<br>110C | Published to the [Ledvance website](https://update.ledvance.com/firmware-overview?submit=all).
Philips (Signify) | 100B | See [Hue Firmware](#hue-firmware) below.
ubisys | 10F2 | Published to the [ubisys website](http://www.ubisys.de/en/support/firmware/).

#### Latest firmware files

The latest firmware files [can be found on the WiKi page](https://github.com/dresden-elektronik/deconz-rest-plugin/wiki/OTA-Image-Types---Firmware-versions).

## Installation

Basically, the deCONZ STD OTAU plugin uses the same setup as the [deCONZ REST API plugin](https://github.com/dresden-elektronik/deconz-rest-plugin).
To compile and install the STD OTAU plugin, follow the instructions to compile and install the REST API plugin, substituting the repository in step 1 with this one.

## Troubleshooting
Start deCONZ with `--dbg-ota=1` to make the STD OTAU plugin issue debug messages.
