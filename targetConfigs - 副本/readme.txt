本目录的 `MSPM0G3507.ccxml` 当前是 XDS110 调试目标配置，不是 CH340 UART BSL 配置。若要经 CH340 下载，应先确认设备管理器中当前 `USB-SERIAL CH340` 的 COM 号，并使用另行配置的 CCS UART Connection 或已批准的 BSL 下载工具；仅按厂商确认的 BOOT/RESET 时序进入 BSL。UART BSL 仅用于下载，不能提供 SWD 的断点、单步或实时内存查看。

The 'targetConfigs' folder contains target-configuration (.ccxml) files, automatically generated based
on the device and connection settings specified in your project on the Properties > General page.

Please note that in automatic target-configuration management, changes to the project's device and/or
connection settings will either modify an existing or generate a new target-configuration file. Thus,
if you manually edit these auto-generated files, you may need to re-apply your changes. Alternatively,
you may create your own target-configuration file for this project and manage it manually. You can
always switch back to automatic target-configuration management by checking the "Manage the project's
target-configuration automatically" checkbox on the project's Properties > General page.
