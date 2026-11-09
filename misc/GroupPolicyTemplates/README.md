# Administrative Template Policy for WinPX Proxy Server

- Run `InstallLocally.cmd` to install the templates for the Local Group Policy
- To edit the GPO locally, run `gpedit.msc`
- Machine settings can be found under `Computer Configuration\Administrative Templates\WinPX Proxy Server`
- User settings can be found under `Computer Configuration\Administrative Templates\WinPX Proxy Server`

## Registry Keys

- Machine group policy: HKEY_LOCAL_MACHINE\Software\Policies\Marius Greuel\WinPX
- User group policy: HKEY_CURRENT_USER\Software\Policies\Marius Greuel\WinPX
- User settings: HKEY_CURRENT_USER\Software\Marius Greuel\WinPX

Note: If WinPX is deployed as a packaged app (via MSIX package), the user registry values are stored in an app hive,
and not in the HKEY_CURRENT_USER registry hive. The HKEY_CURRENT_USER values can be used as a preset, however.

## Resources

For information on managing ADMX Files, see
Managing Group Policy ADMX Files Step-by-Step Guide
http://go.microsoft.com/fwlink/?LinkId=60363
