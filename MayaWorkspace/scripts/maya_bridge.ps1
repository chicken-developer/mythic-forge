param([Parameter(Mandatory=$true)][string]$Script,[int]$Port=5555)
$ErrorActionPreference='Stop'
$scriptPath=(Resolve-Path -LiteralPath $Script).Path.Replace('\','/')
$python="exec(compile(open('$scriptPath', encoding='utf-8').read(), '$scriptPath', 'exec'))"
$escaped=$python.Replace('\','\\').Replace('"','\"')
$command='python("'+$escaped+'");'+"`n"
$client=[System.Net.Sockets.TcpClient]::new();$client.Connect('127.0.0.1',$Port)
$stream=$client.GetStream();$stream.ReadTimeout=600000
$bytes=[Text.Encoding]::UTF8.GetBytes($command);$stream.Write($bytes,0,$bytes.Length)
$buffer=New-Object byte[] 65536
try {$count=$stream.Read($buffer,0,$buffer.Length);[Text.Encoding]::UTF8.GetString($buffer,0,$count).Trim([char]0)}finally{$client.Close()}
