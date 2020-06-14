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
Lutron | 1144 | The firmware for the Aurora Friends-of-Hue dimmer is available through [Hue Firmware](#hue-firmware).
Philips (Signify) | 100B | See [Hue Firmware](#hue-firmware) below.
ubisys | 10F2 | Published to the [ubisys website](http://www.ubisys.de/en/support/firmware/).

#### Hue Firmware

Signify don't provide their firmware for use with other ZigBee gateways than the Hue bridge.
The communication between the bridge and the server hosting the firmware files is encrypted, so we cannot get an overview of the files available.
To find the firmware files, you need to sniff the traffic from the Hue bridge to the Internet, as it downloads the files.
Unfortunately, the bridge will only download firmware files for connected devices with outdated firmware.
For details, see [issue #10](https://github.com/dresden-elektronik/deconz-ota-plugin/issues/10).

Below is an overview of the images found so far:

Image | Device(s) | Firmware
-- | -- | --
0100 | _unknown_ | [5.127.1.26581](http://fds.dc1.philips.com/firmware/ZGB_100B_0100/1107322837/TI_0100_5.127.1.26581_0012.sbl-ota)
0103 | Gamut-A _Color light_ | [5.127.1.26581](http://fds.dc1.philips.com/firmware/ZGB_100B_0103/1107322837/LivCol_0103_5.127.1.26581_0012.sbl-ota)
0104 | Gamut-B _Extended color light_ | [5.130.1.30000](http://fds.dc1.philips.com/firmware/ZGB_100B_0104/1107326256/ConnectedLamp-Atmel_0104_5.130.1.30000_0012.sbl-ota)
0105 | _Dimmable light_ | [5.130.1.30000](http://fds.dc1.philips.com/firmware/ZGB_100B_0105/1107326256/WhiteLamp-Atmel-Target_0105_5.130.1.30000_0012.sbl-ota)
0108 | _unknown_ | [5.130.1.30000](http://fds.dc1.philips.com/firmware/ZGB_100B_0108/1107326256/LivingColors-Target_0108_5.130.1.30000_0012.sbl-ota)
0109 | Hue dimmer switch | [6.1.1.28573](http://fds.dc1.philips.com/firmware/ZGB_100B_0109/1107324829/Switch-ATmega_6.1.1.28573_0012.sbl-ota)
010B | _unknown_ | [5.130.1.30000](http://fds.dc1.philips.com/firmware/ZGB_100B_010C/16783874/100B-010C-01001A02-ConfLight-Lamps_0012.zigbee)
010C | Gamut-C _Extended color light_<br>_Color temperature light_ | [1.50.2_r30933](http://fds.dc1.philips.com/firmware/ZGB_100B_010C/16783874/100B-010C-01001A02-ConfLight-Lamps_0012.zigbee)
010D | Hue motion sensor | [6.1.1.27575](http://fds.dc1.philips.com/firmware/ZGB_100B_010D/1107323831/Sensor-ATmega_6.1.1.27575_0012.sbl-ota)
010E | Aurelle Panels, Signes, Playbar, Being White Ambiance | [1.50.2_r30933](http://fds.dc1.philips.com/firmware/ZGB_100B_010E/16783620/100B-010E-01001904-ConfLight-ModuLum_0012.zigbee)
010F | Outdoor led strip (?) | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_010F/16779778/100B-010F-01000A02-ConfLight-LedStrips_0012.zigbee)
0110 | Bluetooth light (?) | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_0110/16782848/100B-0110-01001600-ConfLight-Lamps-EFR32MG13.zigbee)
0111 | Module (?) | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_0111/16782848/100B-0111-01001600-ConfLight-ModuLum-EFR32MG13.zigbee)
0112 | Bluetooth light (?) | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_0112/16782592/100B-0112-01001500-ConfLightBLE-Lamps-EFR32MG13.zigbee)
0114 | Bluetooth GU10 | [1.65.11_hB798F2BF](http://fds.dc1.philips.com/firmware/ZGB_100B_0114/16780804/100B-0114-01000E04-ConfLightBLE-Lamps-EFR32MG21.zigbee)
0115 | Hue smart plug | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_0115/16779264/100B-0115-01000800-SmartPlug-EFR32MG13.zigbee)
0116 | Hue smart button | [2.30.0_r30777](http://fds.dc1.philips.com/firmware/ZGB_100B_0116/33562112/100B-0116-02001E00-Switch-EFR32MG13.zigbee)
0000 | Lutron Aurora<br>_Manufacturer Code_: 1144 | [3.4](http://fds.dc1.philips.com/firmware/ZGB_1144_0000/3040/Superman_v3_04_Release_3040.ota)

## Installation

Basically, the deCONZ STD OTAU plugin uses the same setup as the [deCONZ REST API plugin](https://github.com/dresden-elektronik/deconz-rest-plugin).
To compile and install the STD OTAU plugin, follow the instructions to compile and install the REST API plugin, substituting the repository in step 1 with this one.

## Troubleshooting
Start deCONZ with `--dbg-ota=1` to make the STD OTAU plugin issue debug messages.
