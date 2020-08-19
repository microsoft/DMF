[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $TargetOS,
    [Parameter(Mandatory)]
    [string] $Configuration,
    [Parameter(Mandatory)]
    [string] $Platform,
    [string] $Solution = 'DMF.sln',
    [ValidateSet('Build', 'Pack', 'Release')]
    [string] $Action = 'Build'
)

# Global configuration
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = "tls12, tls11, tls"

$script:SourcesDirectory = (Get-Item "${PSScriptRoot}\..").FullName
$script:BinariesDirectory = if ([string]::IsNullOrEmpty($Env:BUILD_BINARIESDIRECTORY)) { "${SourcesDirectory}\.output\b" } else { $Env:BUILD_BINARIESDIRECTORY }
$script:ArtifactsDirectory = if ([string]::IsNullOrEmpty($Env:BUILD_ARTIFACTSTAGINGDIRECTORY)) { "${SourcesDirectory}\.output\a" } else { $Env:BUILD_ARTIFACTSTAGINGDIRECTORY }

$script:TARGETOS_URL = @{
    '20H1' = 'https://devicesoss.z5.web.core.windows.net/ewdk/EWDK_vb_release_19041_191206-1406.iso'
    '19H1' = 'https://devicesoss.z5.web.core.windows.net/ewdk/EWDK_19h1_release_svc_prod3_18362_190416-1111.iso'
    'RS5' = 'https://devicesoss.z5.web.core.windows.net/ewdk/EWDK_rs5_release_svc_prod2_17763_190129-1747.iso'
    'RS4' = 'https://devicesoss.z5.web.core.windows.net/ewdk/EWDK_rs4_release_svc_prod1_17134_180727-1807.iso'
    'RS3' = 'https://devicesoss.z5.web.core.windows.net/ewdk/EWDK_rs3_release_svc_16299_180320-1852.iso'
}
$script:CACHE_DIR = Join-Path $SourcesDirectory -ChildPath '.cache'


#################################################################
# Helper functions
#
#################################################################
<#
    Execute a command, and update environment modification into
    current powershell process.
#>
function Set-EnvironmentFromScript {
    param(
        [Parameter(Mandatory)]
        [string] $Command
    )
    $envFile = Join-Path $([System.IO.Path]::GetTempPath()) -ChildPath ("{0}.env.json" -f [guid]::NewGuid())
    try {
        # Constructing the command line to grab the environment after the execution of the script file
        $pwshCommand = "[System.Environment]::GetEnvironmentVariables() | ConvertTo-Json | Out-File '${envFile}' -Force"
        $encodedCommand = [Convert]::ToBase64String([System.Text.Encoding]::Unicode.GetBytes($pwshCommand))
        $cmdLine = "`"$Command`" $args & powershell -NoLogo -NoProfile -NonInteractive -encodedCommand ${encodedCommand}"
        $cmdDotExe = (Get-Command 'cmd.exe').Source
        Write-Verbose "Executing: ${cmdline}"
        &$cmdDotExe /c $cmdLine | Write-Host

        # Updating the environment back in current session
        if (Test-Path $envFile -PathType Leaf) {
            Write-Verbose "Loading ${envFile}"
            $object = Get-Content $envFile -Raw | ConvertFrom-Json
            foreach ($name in $object.PSObject.Properties.name) {
                $value = $object."${name}"
                Write-Verbose "Setting environment variable ${name}=${value}"
                [System.Environment]::SetEnvironmentVariable($name, $value)
            }
        }
    }
    finally {
        if (Test-Path $envFile -PathType Leaf) {
            Remove-Item $envFile -Force
        }
    }
}

function Get-Nuget {
    [CmdletBinding()]
    param()
    $url = 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe'
    $nugetExe = Join-Path $script:CACHE_DIR -ChildPath 'gitversion.exe'
    if (-not (Test-Path $nugetExe -PathType Leaf)) {
        Write-Host "> Downloading latest nuget.exe"
        $null = New-Item $script:CACHE_DIR -Force -ItemType Directory
        Invoke-WebRequest -Uri $url -UseBasicParsing -OutFile $nugetExe
    }
    $nugetExe
}

function Request-GitVersion {
    [CmdletBinding()]
    param(
        [Parameter(ValueFromRemainingArguments)]
        [string[]] $Arguments = @()
    )
    # Download GitVersion if needed
    $gitVersionExe = Join-Path $script:CACHE_DIR -ChildPath 'GitVersion.CommandLine/tools/gitversion.exe'
    if (-not (Test-Path $gitVersionExe -PathType Leaf)) {
        Write-Host "> Downloading latest gitversion.exe"
        $null = New-Item $script:CACHE_DIR -Force -ItemType Directory
        $nugetExe = Get-Nuget
        &$nugetExe install GitVersion.CommandLine -ExcludeVersion -OutputDirectory $script:CACHE_DIR
    }
    
    Write-Verbose "Executing: ${gitVersionExe} $($Arguments -join ' ')"
    $result = &$gitVersionExe @Arguments *>&1
    "GitVersion output: ${result}" | Write-Debug
    if ($LASTEXITCODE -ne 0) {
        $result | Write-Host
        throw "Unable to request the version! (ExitCode=${LASTEXITCODE})"
    }
    $result | ConvertFrom-Json
}

<#
    Download EWDK Iso from azure endpoint
#>
function Save-Ewdk {
    [CmdletBinding()]
    param(
        [string] $TargetOS
    )
    $backupProgressPreference = $ProgressPreference
    $isoFile = Join-Path $script:CACHE_DIR -ChildPath "${TargetOS}.iso"
    if (-not (Test-Path $isoFile -PathType Leaf)) {
        try {
            $ProgressPreference = 'SilentlyContinue'
            $url = $TARGETOS_URL[$TargetOS]
            Write-Host "Downloading ${TargetOS} from ${url} as ${isoFile}"
            Invoke-WebRequest -Uri $url -OutFile $isoFile -UseBasicParsing -ErrorAction Stop
            $isoFile
        }
        catch {
            if (Test-Path $isoFile -PathType Leaf) {
                Remove-Item $isoFile -Force
            }
        }
        finally {
            $ProgressPreference = $backupProgressPreference
        }
    }
    else {
        $isoFile
    }
}

function New-SourceIndexMetadata {
    [CmdletBinding()]
    param (
        [Parameter(Mandatory)]
        [string] $ManifestFile,
        [string] $CollectionUrl = $Env:SYSTEM_TEAMFOUNDATIONCOLLECTIONURI,
        [string] $TeamProjectId = ${Env:SYSTEM_TEAMPROJECTID},
        [string] $RepositoryId = ${Env:BUILD_REPOSITORY_ID},
        [string] $CommitId = ${Env:BUILD_SOURCEVERSION},
        [string] $RepositoryProvider = ${Env:BUILD_REPOSITORY_PROVIDER},
        [string] $Directory = ${Env:BUILD_SOURCESDIRECTORY}
            
    )

    # Check parameters
    if (-not [string]::IsNullOrEmpty($CollectionUrl)) {
        $CollectionUrl = $CollectionUrl.TrimEnd('/')
    }
    if ([string]::IsNullOrEmpty($Directory)) {
        $Directory = $script:SourcesDirectory
    }
    $Directory = (Get-Item $Directory).FullName.Trim('\')

    # Generate metadata
    $data = [PSCustomObject]@{
        collectionUrl = $CollectionUrl
        repositoryProvider = $RepositoryProvider
        teamProjectId = $TeamProjectId
        repositoryId = $RepositoryId
        commitId = $CommitId
        directory = $Directory
        files = @()
    }
    Get-ChildItem "${Directory}\*" -Recurse | Foreach-Object {
        $data.files += $_.FullName.Substring($data.directory.length + 1)
    }
    
    $data | ConvertTo-Json -Depth 100 | Out-File -FilePath $ManifestFile -Force
}

#################################################################
# Actions
#
#################################################################
function Invoke-Build {
    [CmdletBinding()]
    param()

    # Resolve the version
    $version = Request-GitVersion /nofetch /config "${PSScriptRoot}\GitVersion.yml"
    $version | Out-String | Write-Host
    Write-Host "##vso[build.updatebuildnumber]$($version.NuGetVersion)"


    # Download EWDK iso
    $isoFile = Save-Ewdk -TargetOS $TargetOS

    $mountedISO = $null
    try {
        Push-Location $SourcesDirectory
        Write-Host "Mounting ISO: ${isoFile}"
        $mountedISO = Mount-DiskImage -PassThru -ImagePath $isoFile
        Start-Sleep -Seconds 5 # TODO: Need to fix
        $diskVolume = ($mountedISO | Get-Volume).DriveLetter

        # Enable EWDK
        Write-Host "Enabling EWK from ${diskVolume}"
        Set-EnvironmentFromScript -Command "${diskVolume}:\BuildEnv\SetupBuildEnv.cmd"

        # Build the solution
        $splat = @(
            $Solution,
            "/p:Configuration=${Configuration}"
            "/p:Platform=${Platform}"
        )
        msbuild @splat
        $exitCode = $LASTEXITCODE
        $global:LASTEXITCODE = 0
        if ($exitCode -ne 0) {
            throw "Build failed (exitCode=${exitCode})"
        }
    } finally {
        Pop-Location
        if ($null -ne $mountedISO) {
            Write-Host "Unmounting $($mountedISO.ImagePath)"
            Dismount-DiskImage -ImagePath $mountedISO.ImagePath
        }
    }

    # Stage artifacts
    $dropPath = "${script:BinariesDirectory}\Bin_${TargetOS}_${Platform}_${Configuration}"
    Write-Host "Staging artifacts under ${dropPath}"
    $null = New-Item $dropPath -Force -ItemType Directory
    Copy-Item "${SourcesDirectory}\${Configuration}\${Platform}\*" -Destination $dropPath -Force -Recurse

    # Source indexing support
    Write-Host "Generating source indexing metadata."
    New-SourceIndexMetadata -ManifestFile "${dropPath}\sourceindexer-metadata.json"
}


function Invoke-Pack {
    [CmdletBinding()]
    param()
    # Calculate the version of the package
    $version = Request-GitVersion /nofetch /config "${PSScriptRoot}\GitVersion.yml"
    $packageVersion = $version.NuGetVersion

    # Package each target OS separately
    foreach ($tos in $TargetOS.Split(',').Trim()) {
        $packageName = 'Devices.Library.DMF.{0}' -f $tos
        $packageDir = Join-Path $script:BinariesDirectory -ChildPath $packageName
        $packageIncDir = Join-Path $packageDir -ChildPath 'include'
        $nuspecPath = Join-Path $packageDir -ChildPath "${packageName}.nuspec"
        $null = New-Item $packageDir -ItemType Directory -Force

        # Stage includes
        Write-Host "Staging package ${packageName}@${packageVersion} content"
        $null = New-Item "${packageIncDir}\DMF" -ItemType Directory -Force
        Get-ChildItem "${SourcesDirectory}\DMF\*" -Recurse -File |
            Where-Object { $_.Name -like '*.h' } |
            ForEach-Object {
            $dest = "${packageIncDir}\DMF\{0}" -f $_.FullName.Substring("${SourcesDirectory}\DMF\".Length)
            $null = New-Item (Split-Path $dest -Parent) -ItemType Directory -Force
            Copy-Item $_.FullName -Destination $dest -Force
        }

        # Stage bin content
        foreach ($c in $Configuration.Split(',').Trim()) {
            foreach ($p in $Platform.Split(',').Trim()) {
                $packageDropDir = Join-Path $packageDir -ChildPath "Drop_${tos}_${p}_${c}"
                $null = New-Item "${packageDropDir}\lib" -ItemType Directory -Force
                Copy-Item "${script:BinariesDirectory}\Bin_${tos}_${p}_${c}\lib\*" -Destination "${packageDropDir}\lib" -Force -Recurse
                Copy-Item "${script:BinariesDirectory}\Bin_${tos}_${p}_${c}\sourceindexer-metadata.json" -Destination "${packageDropDir}/sourceindexer-metadata.json" -Force 
            }
        }

        # Create a nuspec file
        $nuspecContent = @"
<?xml version=`"1.0`"?>
<package xmlns="http://schemas.microsoft.com/packaging/2011/08/nuspec.xsd">
    <metadata>
        <id>{0}</id>
        <version>{1}</version>
        <title>DMF library</title>
        <authors>Microsoft</authors>
        <licenseUrl>https://github.com/microsoft/DMF/blob/master/LICENSE</licenseUrl>
        <projectUrl>https://github.com/microsoft/DMF</projectUrl>
        <description>DMF library</description>
        <summary></summary>
        <copyright></copyright>
    </metadata>
</package>
"@
        $nuspecContent -f $packageName, $packageVersion | Out-File -FilePath $nuspecPath -Encoding utf8 -Force
        
        # Use nuget to package the content
        $null = New-Item $script:ArtifactsDirectory -ItemType Directory -Force
        $nugetExe = Get-Nuget
        $splat = @(
            'pack', $nuspecPath,
            '-OutputDirectory', $script:ArtifactsDirectory,
            '-NoPackageAnalysis',
            '-NonInteractive'
        )
        &$nugetExe @splat
        $exitCode = $LASTEXITCODE
        $global:LASTEXITCODE = 0
        if ($exitCode -ne 0) {
            throw "Failed to create the package"
        }
    }
}

function Invoke-Release {
    [CmdletBinding()]
    param()
    # Calculate the version of the package
    $version = Request-GitVersion /nofetch /config "${PSScriptRoot}\GitVersion.yml"
    $packageVersion = $version.NuGetVersion
    
    Write-Host "Creating tag v${packageVersion}"
    git tag "v${packageVersion}"
    git push origin "v${packageVersion}"
}

# Calling main action
&"Invoke-${Action}"