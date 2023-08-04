function Install-VC-Runtime {
    if (Get-Package "Microsoft Visual C++ 2015-2022 Redistributable*" -ErrorAction SilentlyContinue) {
        return;
    }

    # Download VC++ redistributable.
    # Turn off progress bar per https://stackoverflow.com/a/43477248
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -Uri "https://aka.ms/vs/17/release/vc_redist.x64.exe" -OutFile ".\vc_redist.x64.exe" -ErrorAction Stop
    $ProgressPreference = 'Continue'

    # Install VC++ redistributable.
    Start-Process -FilePath ".\vc_redist.x64.exe" -ArgumentList "/install /quiet /norestart" -Wait
    Remove-Item ".\vc_redist.x64.exe"
}

function Install-IIS {
    # Install IIS with CGI (for PHP) and management console.
    # This is a no-op if already installed.
    Install-WindowsFeature -Name Web-Server,Web-CGI,Web-Mgmt-Console -ErrorAction Stop

    # Install Url-Rewrite Module
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -Uri "https://download.microsoft.com/download/1/2/8/128E2E22-C1B9-44A4-BE2A-5859ED1D4592/rewrite_amd64_en-US.msi" -OutFile "C:\Windows\Temp\UrlRewrite.msi" -ErrorAction Stop
    $ProgressPreference = 'Continue'
    Start-Process -FilePath "C:\Windows\Temp\UrlRewrite.msi" -ArgumentList "/quiet" -Wait
    Remove-Item "C:\Windows\Temp\UrlRewrite.msi"
}

function Install-OpenSSL {
    if (Test-Path "C:\openssl-3") {
        return
    }

    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -Uri "https://download.firedaemon.com/FireDaemon-OpenSSL/openssl-3.1.1.zip" -OutFile ".\openssl-3.1.1.zip" -ErrorAction Stop
    $ProgressPreference = 'Continue'

    Expand-Archive -Path ".\openssl-3.1.1.zip" -DestinationPath "C:\" -ErrorAction Stop
    Remove-Item ".\openssl-3.1.1.zip"
}

function Install-PHP {
    if (Test-Path -Path "C:\PHP") {
        return
    }

    # Download non thread safe PHP.
    # Turn off progress bar per https://stackoverflow.com/a/43477248
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -Uri "https://windows.php.net/downloads/releases/archives/php-8.2.7-nts-Win32-vs16-x64.zip" -OutFile ".\php.zip" -ErrorAction Stop
    $ProgressPreference = 'Continue'

    # Uncompress PHP.
    Expand-Archive -Path ".\php.zip" -DestinationPath "C:\PHP" -ErrorAction Stop
    Remove-Item ".\php.zip"

    # Configure php.ini.
    $phpini = Get-Content -Path "C:\PHP\php.ini-production" -ErrorAction Stop
    $phpini = $phpini.Replace(";fastcgi.impersonate = 1", "fastcgi.impersonate = 1")
    $phpini = $phpini.Replace(";cgi.fix_pathinfo=1", "cgi.fix_pathinfo=0")
    $phpini = $phpini.Replace(";cgi.force_redirect = 1", "cgi.force_redirect = 0")
    #$phpini = $phpini.Replace(";open_basedir =", "open_basedir = `"C:\inetpub;C:\Windows\TEMP`"")
    $phpini = $phpini.Replace(';extension_dir = "ext"', 'extension_dir = "ext"')
    $phpini = $phpini.Replace(";error_log = syslog", "error_log = C:\inetpub\temp\php-errors.log")
    $phpini = $phpini.Replace(";fastcgi.logging = 0", "fastcgi.logging = 0")
    $phpini = $phpini.Replace(";extension=pdo_sqlite", "extension=pdo_sqlite")
    $phpini = $phpini.Replace(";extension=sqlite3", "extension=sqlite3")
    $phpini = $phpini.Replace(";extension=openssl", "extension=openssl")
    $phpini = $phpini.Replace(";extension=gmp", "extension=gmp")
    $phpini = $phpini.Replace(";extension=curl", "extension=curl")
    Set-Content -Path "C:\PHP\php.ini" -Value $phpini -ErrorAction Stop

    # Add IIS handler for PHP.
    Add-WebConfigurationProperty -Filter "system.webServer/fastCGI" -Name "." -Value @{
        fullPath='C:\PHP\php-cgi.exe';
    } -ErrorAction Stop
    Add-WebConfigurationProperty -Filter "system.webServer/handlers" -Name "." -Value @{
        name='PHP_via_FastCGI';
        path='*.php';
        verb='*';
        modules='FastCgiModule';
        scriptProcessor='C:\PHP\php-cgi.exe';
        resourceType='Unspecified';
    } -ErrorAction Stop

    # Add default PHP documents.
    Add-WebConfigurationProperty -Filter "system.webServer/defaultDocument/files" -Name "." -Value @{
        value='index.php';
    } -ErrorAction Stop
    Add-WebConfigurationProperty -Filter "system.webServer/defaultDocument/files" -Name "." -Value @{
        value='default.php';
    } -ErrorAction Stop

    # Add PHP to PATH.
    [Environment]::SetEnvironmentVariable(
        "Path",
        [Environment]::GetEnvironmentVariable("Path", [EnvironmentVariableTarget]::Machine) + ";C:\PHP",
        [EnvironmentVariableTarget]::Machine
    )
}

function Install-Native {
    # Copy native components to IIS directory.
    Copy-Item ".\native\iismodule.dll" "$Env:windir\System32\inetsrv\" -ErrorAction Stop
    Copy-Item ".\native\macro.exe" "$Env:windir\System32\inetsrv\" -ErrorAction Stop
    # Macro interpreter runs at low integrity level.
    icacls "$Env:windir\System32\inetsrv\macro.exe" /setintegritylevel Low
}

function Install-Web {
    # Remove default IIS content.
    Remove-Item "C:\inetpub\wwwroot\iisstart.htm" -ErrorAction SilentlyContinue
    Remove-Item "C:\inetpub\wwwroot\iisstart.png" -ErrorAction SilentlyContinue

    # Copy web contents to webroot.
    Copy-Item -Path ".\web\*" -Recurse -Force -Destination "C:\inetpub\wwwroot" -ErrorAction Stop

    # Initialize data directory.
    New-Item "C:\inetpub\data" -ItemType Directory -ErrorAction SilentlyContinue
    icacls "C:\inetpub\data" /grant Users:F /Q
    New-Item "C:\inetpub\data\documents\" -ItemType Directory -ErrorAction SilentlyContinue
    icacls "C:\inetpub\data\documents\" /grant Users:F /Q

    # Initialize SQLite.
    C:\PHP\php.exe "C:\inetpub\wwwroot\setup.php"
    icacls "C:\inetpub\data\database.sqlite" /grant Users:F /Q
    Remove-Item "C:\inetpub\wwwroot\setup.php" -ErrorAction SilentlyContinue

    if (Test-Path "C:\inetpub\keys") {
        return
    }

    # Generate signing key.
    New-Item "C:\inetpub\keys" -ItemType Directory -ErrorAction SilentlyContinue
    $CurrentLocation = Get-Location
    Set-Location "C:\inetpub\keys"
    C:\openssl-3\x64\bin\openssl.exe ecparam -name prime256v1 -genkey -noout -out private.pem
    C:\openssl-3\x64\bin\openssl.exe ec -in private.pem -pubout -out public.pem
    Copy-Item ".\public.pem" "C:\inetpub\wwwroot\static\keys" -ErrorAction Stop
    icacls "C:\inetpub\wwwroot\static\keys\public.pem" /grant Users:F /Q
    Set-Location "$CurrentLocation"
}

function Initialize-IIS {
    # Enable native IIS module.
    if ($null -eq (Get-WebGlobalModule -Name "CTF")) {
        New-WebGlobalModule -Name "CTF" -Image "%WINDIR%\System32\inetsrv\iismodule.dll" -ErrorAction Stop
        Enable-WebGlobalModule -Name "CTF" -ErrorAction Stop
    }

    # Configure URL rewrite.
    # No-op if the rule exist, it  just throws a duplicate error.
    $name = 'Rewrite everything but /api/macro to index.php'
    $site = "iis:\Sites\Default Web Site"
    $root = 'system.webServer/rewrite/rules'
    $filter = "{0}/rule[@name='{1}']" -f $root, $name
    Add-WebConfigurationProperty -PSPath $site -filter $root -name '.' -value @{name=$name; patterSyntax='Regular Expressions'; stopProcessing='False'}
    Set-WebConfigurationProperty -PSPath $site -filter "$filter/match" -name 'url' -value "api/macro"
    Set-WebConfigurationProperty -PSPath $site -filter "$filter/match" -name 'negate' -value "True"
    Set-WebConfigurationProperty -PSPath $site -filter "$filter/action" -name 'type' -value 'Rewrite'
    Set-WebConfigurationProperty -PSPath $site -filter "$filter/action" -name 'url' -value 'index.php'
}


Write-Host "Installing VC++ runtime..."
Install-VC-Runtime

Write-Host "Installing OpenSSL..."
Install-OpenSSL

Write-Host "Installing IIS..."
Install-IIS

# Stop IIS.
iisreset /stop

Write-Host "Installing PHP..."
Install-PHP

Write-Host "Installing native components..."
Install-Native

Write-Host "Installing web service..."
Install-Web

Write-Host "Configuring IIS..."
Initialize-IIS

# Start IIS.
iisreset /start
