本目录的 `MSPM0G3507.ccxml` 是板载 CH340 的 UART BSL 目标配置。CCS 下载时选择设备管理器中显示的 `USB-SERIAL CH340 (COM14)`；不要选择 JDY-31 蓝牙 COM 口，也不要选择 XDS110。

The 'targetConfigs' folder contains target-configuration (.ccxml) files, automatically generated based
on the device and connection settings specified in your project on the Properties > General page.

Please note that in automatic target-configuration management, changes to the project's device and/or
connection settings will either modify an existing or generate a new target-configuration file. Thus,
if you manually edit these auto-generated files, you may need to re-apply your changes. Alternatively,
you may create your own target-configuration file for this project and manage it manually. You can
always switch back to automatic target-configuration management by checking the "Manage the project's
target-configuration automatically" checkbox on the project's Properties > General page.
