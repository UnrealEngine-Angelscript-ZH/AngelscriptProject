param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ForwardedArgs = @()
)

Write-Host ("arg-count:{0}" -f $ForwardedArgs.Count)
for ($index = 0; $index -lt $ForwardedArgs.Count; $index++) {
    Write-Host ("arg[{0}]:{1}" -f $index, $ForwardedArgs[$index])
}
