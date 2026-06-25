param(
  [string]$ApiKey = $env:QWEAPI_KEY,
  [string]$BaseUrl = "https://qweapi.com"
)

if (-not $ApiKey) {
  Write-Error "Missing API key. Pass -ApiKey or set QWEAPI_KEY."
  exit 1
}

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$input = Join-Path $root "tmp\imagegen\prompts.jsonl"
$outDir = Join-Path $root "output\imagegen"
$imagesDir = Join-Path $root "images"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
New-Item -ItemType Directory -Force -Path $imagesDir | Out-Null

python (Join-Path $root "generate_image_proxy_batch.py") `
  --input $input `
  --out-dir $outDir `
  --api-key $ApiKey `
  --base-url $BaseUrl

if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

$files = @(
  "hero.png",
  "pain-point.png",
  "auto-irrigation.png",
  "rainbird.png",
  "sprinkler-asset.png",
  "water-protection.png",
  "five-modes.png",
  "ota.png",
  "emotion.png",
  "pricing.png"
)

foreach ($file in $files) {
  $src = Join-Path $outDir $file
  if (Test-Path $src) {
    Copy-Item -Force $src (Join-Path $imagesDir $file)
  }
}

Write-Host "Assets generated to $outDir and copied into $imagesDir"
