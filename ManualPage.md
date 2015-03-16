# Introduction #

The installerget binary (cget.exe) is a native Windows GUI application.  For the vast majority of applications, you can use it by itself.  It also comes with three .dll files which are necessary for LDAP and SSL urls.

If these files are not found in the path, you will not be able to use these protocols.

installerget is not able to perform uploads.  This is entirely due to the lack of command-line options for handling, and not a limitation of libcurl.  This may be removed in the future.

## NOTE ##

Due to some issues with the libcurl binary library used, you may need to have the latest vc\_redist from Microsoft installed.  I intend to remove that dependency in the future.

# Details #

Command-line arguments: (The arguments below are intended functionality, and not all of them actually do something right now.)

| **Short option** | **Long Option** | **Description** |
|:-----------------|:----------------|:----------------|
| -u URL       | --url=URL     | **Required** The URL to retrieve.  |
| -o PATH	| --output=PATH | **Required** The name of the file to save the download to.|
| -a [[USERNAME](USERNAME.md)]:[[PASSWORD](PASSWORD.md)] | --auth=[[USERNAME](USERNAME.md)]:[[PASSWORD](PASSWORD.md)] | _Optional_ If you specify this, it will override any authorization you specified in the URL.  It is generally better  to specify authorization in the URL using the USERNAME:PASSWORD@HOST syntax if possible.|
| -V           | --version     | Write version information to stderr. |
| -h           | --help        | Displays help information. (Not working yet.) |
| -m SECONDS   | --max-time=SECONDS | _Optional_ Maximum time to wait for remote system.  This is basically a timeout. |
|              | --stderr=PATH | _Optional_ Writes information produce by verbosity or errors to the file at PATH.|
| -v LEVEL     | --verbose=LEVEL | _Optional_ Sets the verbosity.  Default is minimal.  Each instance of -v by itself will increase the verbosity, or you can set it directly by specifiying a LEVEL parameter.  Bigger value means more information.|
| -t TITLE     | --title=TITLE   | _Optional_ Sets the window title for the download. |
|              | --left=X | _Optional_ Sets the window's left coordinate.|
|              | --top=Y | _Optional_ Sets the window's top coordinate.|
|              | --width=W | _Optional_ Sets the window's width in pixels. |
|              | --height=H | _Optional_ Sets the window's height in pixels. |
|              | --over=HWND | _Optional_ HWND should be a string representation of the HWND of the window you want the download window placed over.  The progress window will be centered over the HWND you pass in.  If the HWND parameter is wrong, bad things will happen.|
|              | --truncate    | Not used currently. |

## URL and Output Details ##

The --url and --output options can be specified multiple times, and are dyadic.  They must be specified in pairs.  For example, the following command-line downloads two files in series:

```
cget --url=http://www.google.com --output=google_index.html --url=http://www.yahoo.com --output=yahoo_index.html
```

The first page will be downloaded from google and saved as google\_index.html.  When that is done, the second page will be downloaded from yahoo and saved as yahoo\_index.html

## Authorization Details ##

The same thing is truth with the --auth parameter.  It will bind to the last specified URL.  You may not specify multiple authorizations per URL, but you may specify one authorization per URL.  For example:

```
cget --url=http://www.google.com --output=google_index.html --auth=user:pass --url=http://www.yahoo.com --output=yahoo_index.html --auth=user2:pass
```

If you do not specify an authorization for each URL, a default empty authorization is used.

## Using the --over Parameter ##

This assumes that you are shelling out of some top-level installer to run cget.exe.  For example, if you are using Inno Setup, you might shell out to download some additional files in preparation for installation.

A Run section item in Inno Setup might look like this:

```
 Filename: {tmp}\cget.exe; 
  Parameters: "--url=http://download.microsoft.com/download/c/2/8/c28cc7df-c9d2-453b-9292-ae7d242dfeca/SQLEXPR_x86_ENU.exe --output=""{commonappdata}\MY_APP\SQLEXPR_SETUP.EXE"" --stderr=""{commonappdata}\MY_APP\download.log"" --over={wizardhwnd}"; 
  StatusMsg: "Downloading SQL Server 2008 x86..."; 
  Components: x86_Components; 
  WorkingDir: {tmp}
```

The {wizardhwnd} piece will be replaced by Inno with the HWND value of the setup wizard.  The downloader's progress window will then be centered over the wizard window automatically.