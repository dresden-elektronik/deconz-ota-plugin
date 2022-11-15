# STD OTAU Plugin
The deCONZ STD OTAU plugin implements a ZigBee Over-the-Air-Upgrade (OTAU) server in deCONZ.

## Upgrading Firmware
See the [Phoscon help topics](https://phoscon.de/en/support#ota-update-osram-devices) for instructions on how to upgrade the firmware of your ZigBee devices using the STD OTAU plugin.

_Make sure the source routing beta feature is disabled, before attempting a firmware upgrade.
When source routing is enabled, the upgrade of ubisys, Hue, and Develco devices might hang._

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
IKEA | 117C | Available on the [IKEA website](http://fw.ota.homesmart.ikea.net/feed/version_info.json).  Use the [ikea-ota-download.py](https://github.com/dresden-elektronik/deconz-rest-plugin/blob/master/ikea-ota-download.py) script to download the current IKEA firmware files directly to `~/otau`.
Ledvance<br>OSRAM | 1189<br>110C | Published to the [Ledvance website](https://update.ledvance.com/firmware-overview?submit=all).
Lutron | 1144 | The firmware for the Aurora Friends-of-Hue dimmer is available through [Hue Firmware](#hue-firmware).
Philips (Signify) | 100B | See [Hue Firmware](#hue-firmware) below.
ubisys | 10F2 | Published to the [ubisys website](http://www.ubisys.de/en/support/firmware/).
Danfoss | 1246 | Published to the [Danfoss website](https://www.danfoss.com/en/products/dhs/smart-heating/smart-heating/danfoss-ally/danfoss-ally-support/).

See the [Wiki](https://github.com/dresden-elektronik/deconz-rest-plugin/wiki/OTA-Image-Types---Firmware-versions) for a community-maintained list of firmware files, including reports which files have actually been tested.

#### Hue Firmware

Signify don't provide their firmware for use with other ZigBee gateways than the Hue bridge.
The communication between the bridge and the server hosting the firmware files is encrypted, so we cannot get an overview of the files available.
To find the firmware files, you need to sniff the traffic from the Hue bridge to the Internet, as it downloads the files.
Unfortunately, the bridge will only download firmware files for connected devices with outdated firmware.
For details, see [issue #10](https://github.com/dresden-elektronik/deconz-ota-plugin/issues/10).

Alternatively, you might use the Hue Bluetooth app to update the firmware of Bluetooth enabled devices.

Newer Hue light firmware versions, using the ZHA profile (for Zigbee 3.0) instead of ZHA, support attribute reporting.
This is indicated in the **AR** column below.
The latest firmwares versions of newer Hue white and color ambiance lights support dynamic scenes.
This is indicated in the **DS** column below.

Below is an overview of known fimrware files:

Image | Device(s) | Firmware | AR | DS
-- | -- | -- | -- | --
0100 | _unknown_ | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_0100/1124097291/ConnectedLamp-TI-Target_0012.sbl-ota)
0103 | Gamut-A _Color light_ | [67.93.11](http://fds.dc1.philips.com/firmware/ZGB_100B_0103/1124097291/LivingColors-Hue-Target_0012.sbl-ota) | n | n
0104 | Gamut-B _Extended color light_ | [67.88.1](http://fds.dc1.philips.com:80/firmware/ZGB_100B_0104/1124096001/Atmel_0104_ConnectedLamp-Target_0012_88.1.sbl-ota) | n | n
0105 | _Dimmable light_ | [5.130.1.30000](http://fds.dc1.philips.com/firmware/ZGB_100B_0105/1107326256/WhiteLamp-Atmel-Target_0105_5.130.1.30000_0012.sbl-ota) | n | n
0108 | _unknown_ | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_0108/1124097287/100B_0108_LivingColors-Target_0012_93.7.sbl-ota)
0109 | Hue dimmer switch | [6.1.1.28573](http://fds.dc1.philips.com/firmware/ZGB_100B_0109/1107324829/Switch-ATmega_6.1.1.28573_0012.sbl-ota) | - | -
010B | _unknown_ | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_010B/1124096001/LSP_010B_ModuLum-ATmega_0012_88.1.sbl-ota)
010C | Gamut-C _Extended color light_<br>_Color temperature light_ | [1.88.1](http://fds.dc1.philips.com/firmware/ZGB_100B_010C/16785664/100B-010C-01002100-ConfLight-Lamps_0012.zigbee) | n | n
010D | Hue motion sensor | [6.1.1.27575](http://fds.dc1.philips.com/firmware/ZGB_100B_010D/1107323831/Sensor-ATmega_6.1.1.27575_0012.sbl-ota) | - | -
010E | Aurelle Panels, Signes, Playbar, Being White Ambiance | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_010E/16785152/100B-010E-01001F00-ConfLight-ModuLum_0012.zigbee)
010F | Outdoor led strip (?) | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_010F/16781312/100B-010F-01001000-ConfLight-LedStrips_0012.zigbee)
0110 | Bluetooth light (?) | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_0110/16785410/100B-0110-01002002-ConfLight-Lamps-EFR32MG13.zigbee)
0111 | Hue Go (2nd Gen) | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_0111/16784640/100B-0111-01001D00-ConfLight-ModuLum-EFR32MG13.zigbee) 
0112 | Bluetooth E27 | [1.93.11](http://fds.dc1.philips.com/firmware/ZGB_100B_0112/16786178/100B-0112-01002302-ConfLightBLE-Lamps-EFR32MG13.zigbee) | Y | Y
0114 | Hue white and color ambiance bluetooth | [1.93.11](http://fds.dc1.philips.com:80/firmware/ZGB_100B_0114/16784402/100B-0114-01001C12-ConfLightBLE-Lamps-EFR32MG21.zigbee)| Y | Y
0115 | Hue smart plug | [1.93.6](http://fds.dc1.philips.com/firmware/ZGB_100B_0115/16781056/100B-0115-01000F00-SmartPlug-EFR32MG13.zigbee) | | -
0116 | Hue smart button | [2.47.8_h2F968624](http://fds.dc1.philips.com/firmware/ZGB_100B_0116/33566472/100B-0116-02002F08-Switch-EFR32MG13.zigbee) | - | -
0117 | Hue Lightstrip Plus v4 | [1.93.7](http://fds.dc1.philips.com/firmware/ZGB_100B_0117/16784640/100B-0117-01001D00-ConfLightBLE-ModuLum-EFR32MG21.zigbee) | Y | n
0118 | Hue gradient lightstrip | [1.97.3](http://fds.dc1.philips.com/firmware/ZGB_100B_0118/16781828/100B-0118-01001204-PixelLum-EFR32MG21.zigbee) | Y | Y
0119 | Hue dimmer switch (2021) | [2.45.2_hF4400CA](http://fds.dc1.philips.com/firmware/ZGB_100B_0119/33565954/100B-0119-02002D02-Switch-EFR32MG22.zigbee) | - | -
011A | Hue smart plug | [1.93.6](http://fds.dc1.philips.com/firmware/ZGB_100B_011A/16779776/100B-011A-01000A00-SmartPlug-EFR32MG21.zigbee) | Y | -
011B | Hue motion sensor (2022) | 2.53.6 | - | -
011C | Hue wall switch module | 1.0.3 | - | -
011D | _unknown_ | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_011D/16785154/100B-011D-01001F02-ConfLight-ModuLumV2-EFR32MG13.zigbee)
011E | Hue Go v2 (?) | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_011E/16785152/100B-011E-01001F00-ConfLight-PortableV2-EFR32MG13.zigbee)
011F | Hue Ensis | [1.93.11](http://fds.dc1.philips.com/firmware/ZGB_100B_011F/16784902/100B-011F-01001E06-ConfLightBLE-ModuLumV3-EFR32MG21.zigbee) | Y | Y
0120 | Hue Go v3 (?) | [_unknown_](http://fds.dc1.philips.com/firmware/ZGB_100B_0120/16784896/100B-0120-01001E00-ConfLightBLE-PortableV3-EFR32MG21.zigbee)
0121 | Hue tap dial switch | 2.59.19 | - | -
0123 | Hue gradient light string | 1.101.1 | Y | Y
0000 | Lutron Aurora<br>_Manufacturer Code_: 1144 | [3.4](http://fds.dc1.philips.com/firmware/ZGB_1144_0000/3040/Superman_v3_04_Release_3040.ota)<br>[3.8](http://fds.dc1.philips.com/firmware/ZGB_1144_0000/3080/Superman_v3_08_ProdKey_3080.ota)* | - | -

\* Note that the Lutron Aurora firmware v3.8 doesn't work with deCONZ.

## Installation

Basically, the deCONZ STD OTAU plugin uses the same setup as the [deCONZ REST API plugin](https://github.com/dresden-elektronik/deconz-rest-plugin).
To compile and install the STD OTAU plugin, follow the instructions to compile and install the REST API plugin, substituting the repository in step 1 with this one.

## Troubleshooting
Start deCONZ with `--dbg-ota=1` to make the STD OTAU plugin issue debug messages.
