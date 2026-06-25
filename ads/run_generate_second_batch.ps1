param(
  [string]$ApiKey = "",
  [string]$BaseUrl = "https://qweapi.com"
)

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$input = Join-Path $root "tmp\imagegen\second_batch_prompts.jsonl"
$outDir = Join-Path $root "output\imagegen\second-batch"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$argsList = @(
  (Join-Path $root "generate_image_proxy_batch.py"),
  "--input", $input,
  "--out-dir", $outDir,
  "--base-url", $BaseUrl
)

if ($ApiKey) {
  $argsList += @("--api-key", $ApiKey)
}

python @argsList

if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

Write-Host "Second batch generated to $outDir"
