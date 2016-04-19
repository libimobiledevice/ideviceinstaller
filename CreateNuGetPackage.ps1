Param(
  [string]$build
)

Write-Host Changing build number to $build

# Update the build number
(gc .\ideviceinstaller.autoconfig).replace('{build}', $build)|sc .\ideviceinstaller.out.autoconfig

# Create the NuGet package
Import-Module "C:\Program Files (x86)\Outercurve Foundation\modules\CoApp"
Write-NuGetPackage .\ideviceinstaller.out.autoconfig